#include "super_blk_area.h"
#include "./extent_meta/extent_blk_area.h"
#include "./file_meta/file_meta_blk_area.h"
#include "./filename_meta/file_name_blk_area.h"
#include "meta_blk_area.h"
#include "blk_addr.h"
#include "liblightnvm.h"
#include <stdio.h>
#include <string.h>
#include "dbg_info.h"

#define OC_DEV_PATH "/dev/nvme0n1"
#define SUPER_BLK_MAGIC_NUM 0x1234567812345678

#define OCSSD_REFORMAT_SSD 0

extern BlkAddrHandle* ocssd_bah;

// channel[0]
//              blk[0]  super_block_meta
//              blk[1]  - fn_nat
//              blk[2]  ┐
//              blk[3]  | fm_nat       NAT: [ obj_id ] --  |blk|page|obj|
//              blk[4]  ┘
//              blk[5]  - ext_nat
// all channel
//              blks    fn_obj      [0,map_size-1]:bitmap [map_size,size-1]:obj
//              blks    fm_obj      [0,map_size-1]:bitmap [map_size,size-1]:obj
//              blks    ext_obj     [0,map_size-1]:bitmap [map_size,size-1]:obj
//                       |  bitmap name     |  size                |  type                                    |
//                       |------------------|----------------------|------------------------------------------|
//                       | blk_map          |  blk_nr              |  uint8_t[ blk_nr ]                       |
//                       | blk_page_map     |  blk_nr * npages     |  uint8_t[ blk_nr ][ npages ]             |
//                       | blk_page_obj_map |  blk_nr * blk_obj_nr |  uint8_t[ blk_nr ][ napges ][ obj_nr ]   |
//
void OcssdSuperBlock::gen_ocssd_geo(const nvm_geo* geo) {
    ocssd_geo_.nchannels = geo->nchannels;
    ocssd_geo_.nluns     = geo->nluns;
    ocssd_geo_.nplanes   = geo->nplanes;
    ocssd_geo_.nblocks   = geo->nblocks;
    ocssd_geo_.npages    = geo->npages;
    ocssd_geo_.nsectors  = geo->nsectors;

    ocssd_geo_.sector_nbytes  = geo->sector_nbytes;
    ocssd_geo_.page_nbytes    = geo->page_nbytes;
    ocssd_geo_.block_nbytes   = geo->npages * geo->page_nbytes;
    ocssd_geo_.plane_nbytes   = ocssd_geo_.block_nbytes   * geo->nblocks;
    ocssd_geo_.lun_nbytes     = ocssd_geo_.plane_nbytes   * geo->nplanes;
    ocssd_geo_.channel_nbytes = ocssd_geo_.lun_nbytes     * geo->nluns;
    ocssd_geo_.ssd_nbytes     = ocssd_geo_.channel_nbytes * geo->nchannels;

    ocssd_geo_.extent_nbytes     = 24;
    ocssd_geo_.extent_des_nbytes = 24;
    ocssd_geo_.max_ext_addr_nr   = 8;
    ocssd_geo_.min_ext_addr_nr   = 2;

    ocssd_geo_.fn_obj_nbytes  = 4096;   // fn_btree_degree  = 336
    ocssd_geo_.fm_obj_nbytes  = 1024;   // filename + meta_obj_id + extent[]
    ocssd_geo_.ext_obj_nbytes = 4096;   // ext_btree_degree = 128

    ocssd_geo_.fn_blk_obj_nr  = ocssd_geo_.block_nbytes / ocssd_geo_.fn_obj_nbytes;
    ocssd_geo_.fm_blk_obj_nr  = ocssd_geo_.block_nbytes / ocssd_geo_.fm_obj_nbytes;
    ocssd_geo_.ext_blk_obj_nr = ocssd_geo_.block_nbytes / ocssd_geo_.ext_obj_nbytes;

    ocssd_geo_.fn_btree_degree  = 336;
    ocssd_geo_.ext_btree_degree = 128;

    ocssd_geo_.fn_1LVL_obj_nr  = 1;
    ocssd_geo_.fn_2LVL_obj_nr  = ocssd_geo_.fn_btree_degree + ocssd_geo_.fn_1LVL_obj_nr;
    ocssd_geo_.fn_3LVL_obj_nr  = ocssd_geo_.fn_btree_degree * ocssd_geo_.fn_btree_degree
                                + ocssd_geo_.fn_2LVL_obj_nr;
    ocssd_geo_.fn_1LVL_cnt = ocssd_geo_.fn_1LVL_obj_nr * ocssd_geo_.fn_btree_degree;
    ocssd_geo_.fn_2LVL_cnt = ocssd_geo_.fn_2LVL_obj_nr * ocssd_geo_.fn_btree_degree;
    ocssd_geo_.fn_3LVL_cnt = ocssd_geo_.fn_3LVL_obj_nr * ocssd_geo_.fn_btree_degree;

    ocssd_geo_.ext_1LVL_obj_nr = 1;
    ocssd_geo_.ext_2LVL_obj_nr = ocssd_geo_.ext_btree_degree + ocssd_geo_.ext_1LVL_obj_nr;
    ocssd_geo_.ext_3LVL_obj_nr = ocssd_geo_.ext_btree_degree * ocssd_geo_.ext_btree_degree
                                + ocssd_geo_.ext_2LVL_obj_nr;
    ocssd_geo_.ext_1LVL_cnt = ocssd_geo_.ext_1LVL_obj_nr * ocssd_geo_.ext_btree_degree;
    ocssd_geo_.ext_2LVL_cnt = ocssd_geo_.ext_2LVL_obj_nr * ocssd_geo_.ext_btree_degree;
    ocssd_geo_.ext_3LVL_cnt = ocssd_geo_.ext_3LVL_obj_nr * ocssd_geo_.ext_btree_degree;

    ocssd_geo_.file_ext_nr     = ( ocssd_geo_.fm_obj_nbytes - 248 - 8 ) / ocssd_geo_.extent_des_nbytes;
    ocssd_geo_.file_min_nbytes = ocssd_geo_.min_ext_addr_nr * ocssd_geo_.block_nbytes;
    ocssd_geo_.file_max_nbytes = ocssd_geo_.file_ext_nr  * ocssd_geo_.max_ext_addr_nr * ocssd_geo_.block_nbytes;
    ocssd_geo_.file_min_nr = ocssd_geo_.ssd_nbytes / ocssd_geo_.file_max_nbytes;
    ocssd_geo_.file_max_nr = ocssd_geo_.ssd_nbytes / ocssd_geo_.file_min_nbytes;
    ocssd_geo_.file_avg_nr = ( ocssd_geo_.file_min_nr + ocssd_geo_.file_max_nr ) / 2;

//    ocssd_geo_.fn_obj_nr  = ocssd_geo_.fn_3LVL_obj_nr;
    if( ocssd_geo_.file_max_nr <= ocssd_geo_.fn_1LVL_cnt ){
        ocssd_geo_.fn_obj_nr = ocssd_geo_.fn_1LVL_obj_nr;
    }else if( ocssd_geo_.file_max_nr <= ocssd_geo_.fn_2LVL_cnt ) {
        ocssd_geo_.fn_obj_nr = ocssd_geo_.fn_2LVL_obj_nr;
    }else{ // suppose that file_max_nr <= fn_3LVL_cnt
        ocssd_geo_.fn_obj_nr = ocssd_geo_.fn_3LVL_obj_nr;
    }
    ocssd_geo_.fm_obj_nr  = ocssd_geo_.file_max_nr;
//    ocssd_geo_.ext_obj_nr = ocssd_geo_.ext_3LVL_obj_nr;
    if( ocssd_geo_.file_max_nr <= ocssd_geo_.ext_1LVL_cnt ) {
        ocssd_geo_.ext_obj_nr = ocssd_geo_.ext_1LVL_obj_nr;
    }else if( ocssd_geo_.file_max_nr <= ocssd_geo_.ext_2LVL_cnt ){
        ocssd_geo_.ext_obj_nr = ocssd_geo_.ext_2LVL_obj_nr;
    }else{ // suppose that file_max_nr <= ext_3LVL_cnt
        ocssd_geo_.ext_obj_nr = ocssd_geo_.ext_3LVL_obj_nr;
    }

        ocssd_geo_.fn_data_blk_nr  = ocssd_geo_.fn_obj_nr  / ocssd_geo_.fn_blk_obj_nr  + 1;
        ocssd_geo_.fm_data_blk_nr  = ocssd_geo_.fm_obj_nr  / ocssd_geo_.fm_blk_obj_nr  + 1;
        ocssd_geo_.ext_data_blk_nr = ocssd_geo_.ext_obj_nr / ocssd_geo_.ext_blk_obj_nr + 1;

    ocssd_geo_.nat_entry_nbytes = 16;   // 16 = 8(ID) + 4(blk) + 2(page) + 1(obj) + 1(state)
    ocssd_geo_.nat_fn_entry_nr  = ocssd_geo_.fn_3LVL_obj_nr;
    ocssd_geo_.nat_fm_entry_nr  = ocssd_geo_.file_max_nr;
    ocssd_geo_.nat_ext_entry_nr = ocssd_geo_.ext_3LVL_obj_nr;

        ocssd_geo_.nat_fn_blk_nr  = ocssd_geo_.nat_fn_entry_nr  * ocssd_geo_.nat_entry_nbytes / ocssd_geo_.block_nbytes + 1;
        ocssd_geo_.nat_fm_blk_nr  = ocssd_geo_.nat_fm_entry_nr  * ocssd_geo_.nat_entry_nbytes / ocssd_geo_.block_nbytes + 1;
        ocssd_geo_.nat_ext_blk_nr = ocssd_geo_.nat_ext_entry_nr * ocssd_geo_.nat_entry_nbytes / ocssd_geo_.block_nbytes + 1;

    ocssd_geo_.fn_bitmap_nr = ocssd_geo_.fn_data_blk_nr  // blk_map[ blk_nr ]
                                + ocssd_geo_.fn_data_blk_nr * ocssd_geo_.npages  // blk_page_map[ blk_nr ][ napges ]
                                + ocssd_geo_.fn_data_blk_nr * ocssd_geo_.fn_blk_obj_nr;  // blk_page_obj_map[ blk_nr ][ napges ][ obj_nr ]
    ocssd_geo_.fn_bitmap_blk_nr = ocssd_geo_.fn_bitmap_nr / ocssd_geo_.block_nbytes + 1;

    ocssd_geo_.fm_bitmap_nr = ocssd_geo_.fm_data_blk_nr
                                + ocssd_geo_.fm_data_blk_nr * ocssd_geo_.npages
                                + ocssd_geo_.fm_data_blk_nr * ocssd_geo_.fm_blk_obj_nr;
    ocssd_geo_.fm_bitmap_blk_nr = ocssd_geo_.fm_bitmap_nr / ocssd_geo_.block_nbytes + 1;

    ocssd_geo_.ext_bitmap_nr = ocssd_geo_.ext_data_blk_nr
                                + ocssd_geo_.ext_data_blk_nr * ocssd_geo_.npages
                                + ocssd_geo_.ext_data_blk_nr * ocssd_geo_.fm_blk_obj_nr;
    ocssd_geo_.ext_bitmap_blk_nr = ocssd_geo_.ext_bitmap_nr / ocssd_geo_.block_nbytes + 1;

    ocssd_geo_.fn_blk_nr  = ocssd_geo_.fn_data_blk_nr + ocssd_geo_.fn_bitmap_blk_nr;
    ocssd_geo_.fm_blk_nr  = ocssd_geo_.fm_data_blk_nr + ocssd_geo_.fm_bitmap_blk_nr;
    ocssd_geo_.ext_blk_nr = ocssd_geo_.ext_data_blk_nr + ocssd_geo_.ext_bitmap_blk_nr;

    // super block meta
    sb_meta.magic_num = SUPER_BLK_MAGIC_NUM;

    sb_meta.fn_obj_size  = ocssd_geo_.fn_obj_nbytes ;
    sb_meta.fm_obj_size  = ocssd_geo_.fm_obj_nbytes ;
    sb_meta.ext_obj_size = ocssd_geo_.ext_obj_nbytes;

    sb_meta.fn_nat_max_size  = ocssd_geo_.nat_fn_entry_nr ;
    sb_meta.fm_nat_max_size  = ocssd_geo_.nat_fm_entry_nr ;
    sb_meta.ext_nat_max_size = ocssd_geo_.nat_ext_entry_nr;

    sb_meta.fn_nat_blk_nr  = ocssd_geo_.nat_fn_blk_nr ;
    sb_meta.fm_nat_blk_nr  = ocssd_geo_.nat_fm_blk_nr ;
    sb_meta.ext_nat_blk_nr = ocssd_geo_.nat_ext_blk_nr; // super block & nat all in channel 0

//    size_t p_nr_of_ch = ocssd_geo_.nluns;
    uint32_t* blk_idx = new uint32_t[ ocssd_geo_.nchannels ];
    blk_idx[0] = 1; // index from 1
    blk_idx[0] += 1 + ocssd_geo_.nat_fn_blk_nr + ocssd_geo_.nat_fm_blk_nr + ocssd_geo_.nat_ext_blk_nr;
    for(size_t i = 1; i < ocssd_geo_.nchannels; ++i){
        blk_idx[i] = 1;
    }

    size_t nchs = ocssd_geo_.nchannels;
    this->fn_st_blk_idx  = new uint32_t[ nchs ];
    this->fm_st_blk_idx  = new uint32_t[ nchs ];
    this->ext_st_blk_idx = new uint32_t[ nchs ];    // 0 means NOT SET yet
    for(size_t i = 0; i < nchs; ++i ){
        fn_st_blk_idx [ i ] = 0;
        fm_st_blk_idx [ i ] = 0;
        ext_st_blk_idx[ i ] = 0;
    }
    this->fn_blk_nr  = new uint32_t[ nchs ];
    this->fm_blk_nr  = new uint32_t[ nchs ];
    this->ext_blk_nr = new uint32_t[ nchs ];

    size_t ch = 0;

    size_t idx = 0;
    sb_meta.fn_st_ch = ch;
    sb_meta.fn_ch_nr = 1;
    this->fn_st_blk_idx[ idx ] = blk_idx[ ch ];
    this->fn_blk_nr[ idx ] = 0;
    for(size_t i = 0; i < ocssd_geo_.fn_blk_nr; ++i){
        blk_idx[ ch ] += 1;
        this->fn_blk_nr[ idx ] += 1;
        if( ( blk_idx[ ch ] - 1 ) % ocssd_geo_.nluns == 0 ){
            ch = ( ch + 1 ) % nchs;
            if( ( i + 1 ) < ocssd_geo_.fn_blk_nr )
                sb_meta.fn_ch_nr += 1;
            idx = ( idx + 1 ) % nchs;
            if( this->fn_st_blk_idx[ idx ] == 0 ) {
                this->fn_st_blk_idx[ idx ] = blk_idx[ ch ];
                this->fn_blk_nr[ idx ] = 0;
            }
        }
    }

    idx = 0;
    sb_meta.fm_st_ch = ch;
    sb_meta.fm_ch_nr = 1;
    this->fm_st_blk_idx[ idx ] = blk_idx[ ch ];
    this->fm_blk_nr[ idx ] = 0;
    for(size_t i = 0; i < ocssd_geo_.fm_blk_nr; ++i){
        blk_idx[ ch ] += 1;
        this->fm_blk_nr[ idx ] += 1;
        if( ( blk_idx[ ch ] - 1 ) % ocssd_geo_.nluns == 0 ){
            ch = ( ch + 1 ) % nchs;
            if( ( i + 1 ) < ocssd_geo_.fm_blk_nr )
                sb_meta.fm_ch_nr += 1;
            idx = ( idx + 1) % nchs;
            if( this->fm_st_blk_idx[ idx ] == 0 ) {
                this->fm_st_blk_idx[ idx ] = blk_idx[ ch ];
                this->fm_blk_nr[ idx ] = 0;
            }
        }
    }

    idx = 0;
    sb_meta.ext_st_ch = ch;
    sb_meta.ext_ch_nr = 1;
    this->ext_st_blk_idx[ idx ] = blk_idx[ ch ];
    this->ext_blk_nr[ idx ] = 0;
    for(size_t i = 0; i < ocssd_geo_.ext_blk_nr; ++i){
        blk_idx[ ch ] += 1;
        this->ext_blk_nr[ idx ] += 1;
        if( ( blk_idx[ ch ] - 1 ) % ocssd_geo_.nluns == 0 ){
            ch = ( ch + 1 ) % nchs;
            if( ( i + 1 ) < ocssd_geo_.ext_blk_nr )
                sb_meta.ext_ch_nr += 1;
            idx = ( idx + 1 ) % nchs;
            if( this->ext_st_blk_idx[ idx ] == 0 ) {
                this->ext_st_blk_idx[ idx ] = blk_idx[ ch ];
                this->ext_blk_nr[ idx ] = 0;
            }
        }
    }

}

OcssdSuperBlock::OcssdSuperBlock(){
    sb_need_flush = 0;
    nat_need_flush = 0;

    dev = nvm_dev_open( OC_DEV_PATH );
    geo = nvm_dev_get_geo( dev );

    gen_ocssd_geo( geo );

    OCSSD_DBG_INFO( this, "nat_fn_blk_nr  = " << ocssd_geo_.nat_fn_blk_nr  );
    OCSSD_DBG_INFO( this, "nat_fm_blk_nr  = " << ocssd_geo_.nat_fm_blk_nr  );
    OCSSD_DBG_INFO( this, "nat_ext_blk_nr = " << ocssd_geo_.nat_ext_blk_nr );

    OCSSD_DBG_INFO( this, "---------------------------");

    OCSSD_DBG_INFO( this, "fn_bitmap_blk_nr  = " << ocssd_geo_.fn_bitmap_blk_nr  );
    OCSSD_DBG_INFO( this, "fm_bitmap_blk_nr  = " << ocssd_geo_.fm_bitmap_blk_nr  );
    OCSSD_DBG_INFO( this, "ext_bitmap_blk_nr = " << ocssd_geo_.ext_bitmap_blk_nr );

    OCSSD_DBG_INFO( this, "fn_data_blk_nr  = " << ocssd_geo_.fn_data_blk_nr  );
    OCSSD_DBG_INFO( this, "fm_data_blk_nr  = " << ocssd_geo_.fm_data_blk_nr  );
    OCSSD_DBG_INFO( this, "ext_data_blk_nr = " << ocssd_geo_.ext_data_blk_nr );

    OCSSD_DBG_INFO( this, "---------------------------");

    OCSSD_DBG_INFO( this, "fn_blk_nr  = " << ocssd_geo_.fn_blk_nr  );
    OCSSD_DBG_INFO( this, "fm_blk_nr  = " << ocssd_geo_.fm_blk_nr  );
    OCSSD_DBG_INFO( this, "ext_blk_nr = " << ocssd_geo_.ext_blk_nr );

    OCSSD_DBG_INFO( this, "==========================");

    OCSSD_DBG_INFO( this, "fn_ch_nr  = " << sb_meta.fn_ch_nr  );
    OCSSD_DBG_INFO( this, "fm_ch_nr  = " << sb_meta.fm_ch_nr  );
    OCSSD_DBG_INFO( this, "ext_ch_nr = " << sb_meta.ext_ch_nr );

    for(size_t i = 0; i < sb_meta.fn_ch_nr; ++i){
        OCSSD_DBG_INFO( this, "fn_st_blk_idx[ " << i << " ] = " << this->fn_st_blk_idx[i]);
        OCSSD_DBG_INFO( this, "fn_blk_nr    [ " << i << " ] = " << this->fn_blk_nr[i]);
    }

    for(size_t i = 0; i < sb_meta.fm_ch_nr; ++i){
        OCSSD_DBG_INFO( this, "fm_st_blk_idx[ " << i << " ] = " << this->fm_st_blk_idx[i]);
        OCSSD_DBG_INFO( this, "fm_blk_nr    [ " << i << " ] = " << this->fm_blk_nr[i]);
    }

    for(size_t i = 0; i < sb_meta.ext_ch_nr; ++i){
        OCSSD_DBG_INFO( this, "ext_st_blk_idx[ " << i << " ] = " << this->ext_st_blk_idx[i]);
        OCSSD_DBG_INFO( this, "ext_blk_nr    [ " << i << " ] = " << this->ext_blk_nr[i]);
    }

    addr_init( geo );   // init blk_handler

    int need_format = OCSSD_REFORMAT_SSD;
    // first block of SSD store super_block_meta
    blk_addr_handle* bah_0_ = ocssd_bah->get_blk_addr_handle( 0 );
    bah_0_->MakeBlkAddr(0,0,0,0,sb_addr);
    struct nvm_addr sb_nvm_addr;
    bah_0_->convert_2_nvm_addr( sb_addr, &sb_nvm_addr );
    bah_0_->PrBlkAddr( sb_addr, true, " first block of SSD to store sb_meta.");

    // 1. read super super_block_meta
    OCSSD_DBG_INFO( this, "READ super block meta from first block");
    if( !need_format ){
        OCSSD_DBG_INFO( this, " - Go on to read super block");
        sb_vblk = nvm_vblk_alloc( dev, &sb_nvm_addr, 1);
        sb_meta_buf_size = ocssd_geo_.block_nbytes;
        sb_meta_buf = (char *) nvm_buf_alloc( geo, sb_meta_buf_size );
        nvm_vblk_read( sb_vblk, sb_meta_buf, sb_meta_buf_size );
    }
    else{
        OCSSD_DBG_INFO( this, " - Need to Format SSD");
        format_ssd();
    }
    // struct ocssd_super_block_meta sb_meta

    need_format |= (!sb_meta.magic_num == SUPER_BLK_MAGIC_NUM )?1:0;

    if( need_format ){
        OCSSD_DBG_INFO( this, "Not Fotmated DEV" << OC_DEV_PATH );
//      exit(-1);
        sb_need_flush = 1;
        nat_need_flush = 1;
    }

    // 2. read nat table
    // struct nat_table* nat_fn;
    // struct nat_table* nat_fm;
    // struct nat_table* nat_ext;
    OCSSD_DBG_INFO(this, "READ nat table");
    OCSSD_DBG_INFO(this, "fn_nat_max_size  = " << sb_meta.fn_nat_max_size  );
    OCSSD_DBG_INFO(this, "fm_nat_max_size  = " << sb_meta.fm_nat_max_size  );
    OCSSD_DBG_INFO(this, "ext_nat_max_size = " << sb_meta.ext_nat_max_size );
    nat_fn  = new struct nat_table;
    nat_fm  = new struct nat_table;
    nat_ext = new struct nat_table;
    nat_fn->max_length  = sb_meta.fn_nat_max_size;
    nat_fm->max_length  = sb_meta.fm_nat_max_size;
    nat_ext->max_length = sb_meta.ext_nat_max_size;
    nat_fn->entry  = new struct nat_entry[ nat_fn->max_length  ];
    nat_fm->entry  = new struct nat_entry[ nat_fm->max_length  ];
    nat_ext->entry = new struct nat_entry[ nat_ext->max_length ];

    // read nat table from SSD
    if( !need_format ) {
        struct blk_addr fn_nat_blk = *sb_addr;
        struct nvm_addr *fn_nat_nvm_addr = new struct nvm_addr[sb_meta.fn_nat_blk_nr];
        for (int i = 0; i < sb_meta.fn_nat_blk_nr; ++i) {
            bah_0_->BlkAddrAdd(1, &fn_nat_blk);
            bah_0_->convert_2_nvm_addr(&fn_nat_blk, &(fn_nat_nvm_addr[i]));
        }
        fn_nat_vblk = nvm_vblk_alloc(dev, fn_nat_nvm_addr, sb_meta.fn_nat_blk_nr);    // get fn vblk
        // free( fn_nat_nvm_addr )
        fn_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fn_nat_blk_nr;
        fn_nat_buf = (char *) nvm_buf_alloc(geo, fn_nat_buf_size); // read vblk into fn_nat_buf
        nvm_vblk_read(fn_nat_vblk, fn_nat_buf, fn_nat_buf_size);
        for (int i = 0; i < sb_meta.fn_nat_max_size; ++i) {
            memcpy(&(nat_fn->entry[i]),  // nat_fn->entry[i]
                   fn_nat_buf + sizeof(struct nat_entry) * i,
                   sizeof(struct nat_entry));
        }

        struct blk_addr fm_nat_blk = fn_nat_blk;
        struct nvm_addr *fm_nat_nvm_addr = new struct nvm_addr[sb_meta.fm_nat_blk_nr];
        for (int i = 0; i < sb_meta.fm_nat_blk_nr; ++i) {
            bah_0_->BlkAddrAdd(1, &fm_nat_blk);
            bah_0_->convert_2_nvm_addr(&fm_nat_blk, &(fm_nat_nvm_addr[i]));
        }
        fm_nat_vblk = nvm_vblk_alloc(dev, fm_nat_nvm_addr, sb_meta.fm_nat_blk_nr);
        fm_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fm_nat_blk_nr;
        fm_nat_buf = (char *) nvm_buf_alloc(geo, fm_nat_buf_size);
        nvm_vblk_read(fm_nat_vblk, fm_nat_buf, fm_nat_buf_size);
        for (int i = 0; i < sb_meta.fm_nat_max_size; ++i) {
            // nat_fm->entry[i]
        }

        struct blk_addr ext_nat_blk = fm_nat_blk;
        struct nvm_addr *ext_nat_nvm_addr = new struct nvm_addr[sb_meta.ext_nat_blk_nr];
        for (int i = 0; i < sb_meta.ext_nat_blk_nr; ++i) {
            bah_0_->BlkAddrAdd(1, &ext_nat_blk);
            bah_0_->convert_2_nvm_addr(&ext_nat_blk, &(ext_nat_nvm_addr[i]));
        }
        ext_nat_vblk = nvm_vblk_alloc(dev, ext_nat_nvm_addr, sb_meta.ext_nat_blk_nr);
        ext_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.ext_nat_blk_nr;
        ext_nat_buf = (char *) nvm_buf_alloc(geo, ext_nat_buf_size);
        nvm_vblk_read(ext_nat_vblk, ext_nat_buf, ext_nat_buf_size);
        for (int i = 0; i < sb_meta.ext_nat_max_size; ++i) {
            // nat_ext->entry[i]
        }// */
    }
    else{
        for(size_t i = 0; i < sb_meta.fn_nat_max_size ; ++i){
            nat_fn->entry[i].state= NAT_ENTRY_FREE;
        }
        for(size_t i = 0; i < sb_meta.fm_nat_max_size; ++i){
            nat_fm->entry[i].state = NAT_ENTRY_FREE;
        }
        for(size_t i = 0; i < sb_meta.ext_nat_max_size; ++i){
            nat_ext->entry[i].state = NAT_ENTRY_FREE;
        }
    }

    // 3. init meta blk area
    //
    // void xxx_blk_area_init(
	//     int st_ch, int ed_ch,
    //     struct blk_addr* st_addr, size_t* blk_nr,
	//     struct nar_table* nat );
    
    struct blk_addr* fn_st_blk  = new struct blk_addr[ sb_meta.fn_ch_nr ];
    for(size_t ch=sb_meta.fn_st_ch, count = 0; count < sb_meta.fn_ch_nr; ++ch, ++count){
        ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( ch,0,0,0, &(fn_st_blk[ ch - sb_meta.fn_st_ch ]) );
        ocssd_bah->get_blk_addr_handle(ch)->BlkAddrAdd( fn_st_blk_idx[ ch - sb_meta.fn_st_ch ], &(fn_st_blk[ ch - sb_meta.fn_st_ch ]));
    }

    struct blk_addr* fm_st_blk  = new struct blk_addr[ sb_meta.fm_ch_nr ];
    for(size_t ch=sb_meta.fm_st_ch, count = 0; count<sb_meta.fm_ch_nr; ++ch, ++count){
        ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( ch,0,0,0, &(fm_st_blk[ ch - sb_meta.fm_st_ch ]) );
        ocssd_bah->get_blk_addr_handle(ch)->BlkAddrAdd( fm_st_blk_idx[ ch - sb_meta.fm_st_ch ], &(fm_st_blk[ ch - sb_meta.fm_st_ch ]));
    }

    struct blk_addr* ext_st_blk = new struct blk_addr[ sb_meta.ext_ch_nr ];
    for(size_t ch=sb_meta.ext_st_ch, count = 0; count<sb_meta.ext_ch_nr; ++ch, ++count){
        ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( ch,0,0,0, &(ext_st_blk[ ch - sb_meta.ext_st_ch ]) );
        ocssd_bah->get_blk_addr_handle(ch)->BlkAddrAdd( ext_st_blk_idx[ ch - sb_meta.ext_st_ch ], &(ext_st_blk[ ch - sb_meta.ext_st_ch ]));
    }

	// struct nvm_dev* dev,
	// const struct nvm_geo* geo,
	// uint32_t st_ch, uint32_t ed_ch,
	// struct blk_addr* st_addr, size_t* addr_nr,
	// struct nat_table* nat
    //
    file_name_blk_area_init(
        dev, geo,
        sb_meta.fn_st_ch, sb_meta.fn_ch_nr, // uint32_t
        fn_st_blk, fn_blk_nr,
        nat_fn
    );
    file_meta_blk_area_init(
        dev, geo,
        sb_meta.fm_st_ch, sb_meta.fm_ch_nr,
        fm_st_blk, fm_blk_nr,
        nat_fm
    );
    extent_blk_area_init(
        dev, geo,
        sb_meta.ext_st_ch, sb_meta.ext_ch_nr,
        ext_st_blk, ext_blk_nr,
        nat_ext
    );// */

    flush();
}

void OcssdSuperBlock::format_ssd() {
    OCSSD_DBG_INFO( this, "To Format SSD");
    // erease all block
    // write super block
    // write nat
    // write meta data area
}

OcssdSuperBlock::~OcssdSuperBlock()
{
    flush();
    // release
}

std::string OcssdSuperBlock::txt() {
    return "OcssdSuperBlock";
}

void OcssdSuperBlock::flush()
{
    flush_sb();
    flush_nat();
}

void OcssdSuperBlock::flush_sb()
{
    if( sb_need_flush ){
        // write sb_meta into SSD
    }
}

void OcssdSuperBlock::flush_nat()
{
    if( nat_need_flush ){
        // write nat table into SSD
    }
}
