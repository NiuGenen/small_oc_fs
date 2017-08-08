#include "super_blk_area.h"
#include "./extent_meta/extent_blk_area.h"
#include "./file_meta/file_meta_blk_area.h"
#include "./filename_meta/file_name_blk_area.h"
#include "meta_blk_area.h"
#include "blk_addr.h"
#include "liblightnvm.h"
#include <stdio.h>
#include <string.h>
#include <liblightnvm.h>
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
    OCSSD_DBG_INFO( this, " gen ocssd_geo");

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

    if( ocssd_geo_.fn_obj_nr  < ocssd_geo_.fn_3LVL_obj_nr  )
        ocssd_geo_.fn_data_blk_nr  *= 3;
        ocssd_geo_.fm_data_blk_nr  *= 3;
    if( ocssd_geo_.ext_obj_nr < ocssd_geo_.ext_3LVL_obj_nr )
        ocssd_geo_.ext_data_blk_nr *= 3;

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
    OCSSD_DBG_INFO( this, " gen ocssd super block");
    sb_meta.magic_num = SUPER_BLK_MAGIC_NUM;

    sb_meta.fn_bitmap_blk_nr  = (uint32_t)ocssd_geo_.fn_bitmap_blk_nr ;
    sb_meta.fm_bitmap_blk_nr  = (uint32_t)ocssd_geo_.fm_bitmap_blk_nr ;
    sb_meta.ext_bitmap_blk_nr = (uint32_t)ocssd_geo_.ext_bitmap_blk_nr;

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

    uint32_t nchs = ocssd_geo_.nchannels;
    this->fn_st_blk_idx  = new uint32_t[ nchs ];
    this->fm_st_blk_idx  = new uint32_t[ nchs ];
    this->ext_st_blk_idx = new uint32_t[ nchs ];    // 0 means NOT SET yet
    for(size_t i = 0; i < nchs; ++i ){
        fn_st_blk_idx [ i ] = 0;
        fm_st_blk_idx [ i ] = 0;
        ext_st_blk_idx[ i ] = 0;
    }
    this->fn_blk_nr  = new size_t[ nchs ];
    this->fm_blk_nr  = new size_t[ nchs ];
    this->ext_blk_nr = new size_t[ nchs ];

    uint32_t ch = 0;

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
            if( ( i + 1 ) < ocssd_geo_.fn_blk_nr && sb_meta.fn_ch_nr < nchs )
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
            if( ( i + 1 ) < ocssd_geo_.fm_blk_nr && sb_meta.fm_ch_nr < nchs )
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
            if( ( i + 1 ) < ocssd_geo_.ext_blk_nr && sb_meta.ext_ch_nr < nchs )
                sb_meta.ext_ch_nr += 1;
            idx = ( idx + 1 ) % nchs;
            if( this->ext_st_blk_idx[ idx ] == 0 ) {
                this->ext_st_blk_idx[ idx ] = blk_idx[ ch ];
                this->ext_blk_nr[ idx ] = 0;
            }
        }
    }

    for(uint32_t i = 0; i < sb_meta.fn_ch_nr; ++i){
        this->fn_st_blk_idx[ i ] -= 1;
    }
    for(uint32_t i = 0; i < sb_meta.fm_ch_nr; ++i){
        this->fm_st_blk_idx[ i ] -= 1;
    }
    for(uint32_t i = 0; i < sb_meta.ext_ch_nr; ++i){
        this->ext_st_blk_idx[ i ] -= 1;
    }

    OCSSD_DBG_INFO( this, " gen over");
}

OcssdSuperBlock::OcssdSuperBlock()
    :sb_addr(nullptr),sb_vblk(nullptr),sb_meta_buf(nullptr),sb_meta_buf_size((size_t)0),
     fn_st_blk_idx(nullptr),fm_st_blk_idx(nullptr),ext_st_blk_idx(nullptr),
     fn_blk_nr(nullptr),fm_blk_nr(nullptr),ext_blk_nr(nullptr),
     fn_nat_vblk(nullptr),fm_nat_vblk(nullptr),ext_nat_vblk(nullptr)
{
    sb_need_flush = 0;
    nat_need_flush = 0;

    dev = nvm_dev_open( OC_DEV_PATH );
    geo = nvm_dev_get_geo( dev );

    test_geo = new struct nvm_geo;
    test_geo->nchannels = 16;
    test_geo->nluns = 8;
    test_geo->nplanes = 1;
    test_geo->nblocks = 2048;
    test_geo->npages = 2048;
    test_geo->nsectors = 1;
    test_geo->sector_nbytes = 4096;
    test_geo->page_nbytes = 4096;

    //geo = test_geo;

    OCSSD_DBG_INFO( this, "0. calculate SSD layout");
    gen_ocssd_geo( geo );

    OCSSD_DBG_INFO( this, "geo -> nchannels = " << ocssd_geo_.nchannels );
    OCSSD_DBG_INFO( this, "geo -> nluns     = " << ocssd_geo_.nluns     );
    OCSSD_DBG_INFO( this, "geo -> nplanes   = " << ocssd_geo_.nplanes   );
    OCSSD_DBG_INFO( this, "geo -> nblocks   = " << ocssd_geo_.nblocks   );
    OCSSD_DBG_INFO( this, "geo -> npages    = " << ocssd_geo_.npages    );
    OCSSD_DBG_INFO( this, "geo -> nsectors  = " << ocssd_geo_.nsectors  );

    OCSSD_DBG_INFO( this, "size -> ssd     = " << ocssd_geo_.ssd_nbytes );
    OCSSD_DBG_INFO( this, "size -> channel = " << ocssd_geo_.channel_nbytes );
    OCSSD_DBG_INFO( this, "size -> lun     = " << ocssd_geo_.lun_nbytes );
    OCSSD_DBG_INFO( this, "size -> plane   = " << ocssd_geo_.plane_nbytes );
    OCSSD_DBG_INFO( this, "size -> block   = " << ocssd_geo_.block_nbytes );
    OCSSD_DBG_INFO( this, "size -> page    = " << ocssd_geo_.page_nbytes );

    OCSSD_DBG_INFO( this, "===========================");

    OCSSD_DBG_INFO( this, "file_max_size = " << ocssd_geo_.file_max_nbytes );
    OCSSD_DBG_INFO( this, "file_min_size = " << ocssd_geo_.file_min_nbytes );
    OCSSD_DBG_INFO( this, "file_max_nr = " << ocssd_geo_.file_max_nr );
    OCSSD_DBG_INFO( this, "file_min_nr = " << ocssd_geo_.file_min_nr );
    OCSSD_DBG_INFO( this, "file_avg_nr = " << ocssd_geo_.file_avg_nr );

    OCSSD_DBG_INFO( this, "===========================");

    OCSSD_DBG_INFO( this, "fn_1LVL_obj_nr = " << ocssd_geo_.fn_1LVL_obj_nr );
    OCSSD_DBG_INFO( this, "fn_2LVL_obj_nr = " << ocssd_geo_.fn_2LVL_obj_nr );
    OCSSD_DBG_INFO( this, "fn_3LVL_obj_nr = " << ocssd_geo_.fn_3LVL_obj_nr );
    OCSSD_DBG_INFO( this, "ext_1LVL_obj_nr = " << ocssd_geo_.ext_1LVL_obj_nr );
    OCSSD_DBG_INFO( this, "ext_2LVL_obj_nr = " << ocssd_geo_.ext_2LVL_obj_nr );
    OCSSD_DBG_INFO( this, "ext_3LVL_obj_nr = " << ocssd_geo_.ext_3LVL_obj_nr );

    OCSSD_DBG_INFO( this, "===========================");

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

    OCSSD_DBG_INFO( this, "fn_st_ch  = " << sb_meta.fn_st_ch  );
    OCSSD_DBG_INFO( this, "fm_st_ch  = " << sb_meta.fm_st_ch  );
    OCSSD_DBG_INFO( this, "ext_st_ch = " << sb_meta.ext_st_ch );

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

    addr_init( dev, geo );   // init blk_handler

    int need_format = OCSSD_REFORMAT_SSD;
    // first block of SSD store super_block_meta
    size_t zero = 0;
    blk_addr_handle* bah_0_ = ocssd_bah->get_blk_addr_handle( zero );
    if( this->sb_addr == nullptr ){
        this->sb_addr = new struct blk_addr;
        bah_0_->MakeBlkAddr( zero, zero, zero, zero, sb_addr);
    }

    // 1. read super super_block_meta
    OCSSD_DBG_INFO( this, "1. READ super block meta from first block");
    if( !need_format ){
        struct nvm_addr sb_nvm_addr;
        sb_nvm_addr.ppa = 0;
        bah_0_->convert_2_nvm_addr( sb_addr, &sb_nvm_addr );
        bah_0_->PrBlkAddr( sb_addr, true, " first block of SSD to store sb_meta.");
        OCSSD_DBG_INFO( this, " - Go on to read super block");
        sb_vblk = nvm_vblk_alloc( dev, &sb_nvm_addr, 1);
        sb_meta_buf_size = ocssd_geo_.block_nbytes;
        sb_meta_buf = (char *) nvm_buf_alloc( geo, sb_meta_buf_size );
        nvm_vblk_read( sb_vblk, sb_meta_buf, sb_meta_buf_size );
    }
    else{
        OCSSD_DBG_INFO( this, " - Need to Format SSD");
        format_ssd();
        need_format = 0;
    }

    need_format |= (!sb_meta.magic_num == SUPER_BLK_MAGIC_NUM )?1:0;

    if( need_format ){
        OCSSD_DBG_INFO( this, "Not Fotmated DEV : " << OC_DEV_PATH );
        OCSSD_DBG_INFO( this, " - To format OCSSD ......");
        format_ssd();
//        exit(-1);
//        sb_need_flush = 1;
//        nat_need_flush = 1;
    }

    // 2. read nat table
    OCSSD_DBG_INFO( this, "2. Read NAT table");
    // struct nat_table* nat_fn;
    // struct nat_table* nat_fm;
    // struct nat_table* nat_ext;
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
    for(size_t i = 0; i < nat_fn->max_length; ++i ){
        nat_fn->entry[ i ].state = NAT_ENTRY_FREE;
    }
    for(size_t i = 0; i < nat_fm->max_length; ++i ){
        nat_fm->entry[ i ].state = NAT_ENTRY_FREE;
    }
    for(size_t i = 0; i < nat_ext->max_length; ++i ){
        nat_ext->entry[ i ].state = NAT_ENTRY_FREE;
    }

    // read nat table from  formated OCSSD
    if( !need_format ) {
        OCSSD_DBG_INFO( this, " - read fn nat table");
        struct blk_addr fn_nat_blk = *sb_addr;
        struct nvm_addr *fn_nat_nvm_addr = new struct nvm_addr[sb_meta.fn_nat_blk_nr];
        for (int i = 0; i < sb_meta.fn_nat_blk_nr; ++i) {
            bah_0_->BlkAddrAdd(1, &fn_nat_blk);
            fn_nat_nvm_addr[i].ppa = 0;
            bah_0_->convert_2_nvm_addr(&fn_nat_blk, &(fn_nat_nvm_addr[i]));
            OCSSD_DBG_INFO( this, " - nvm_addr idx === " << i );
            OCSSD_DBG_INFO( this, " - - nvm_addr ch    = " << fn_nat_nvm_addr[i].g.ch  );
            OCSSD_DBG_INFO( this, " - - nvm_addr lun   = " << fn_nat_nvm_addr[i].g.lun );
            OCSSD_DBG_INFO( this, " - - nvm_addr plane = " << fn_nat_nvm_addr[i].g.pl  );
            OCSSD_DBG_INFO( this, " - - nvm_addr block = " << fn_nat_nvm_addr[i].g.blk );
        }
        if( this->fn_nat_vblk == nullptr ) {
            this->fn_nat_vblk = nvm_vblk_alloc(dev, fn_nat_nvm_addr, sb_meta.fn_nat_blk_nr);    // get fn vblk
            free(fn_nat_nvm_addr);
            this->fn_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fn_nat_blk_nr;
            this->fn_nat_buf = (char *) nvm_buf_alloc(geo, fn_nat_buf_size); // read vblk into fn_nat_buf
        }
        nvm_vblk_read(fn_nat_vblk, fn_nat_buf, fn_nat_buf_size);
        size_t offset = 0;
        for (int i = 0; i < sb_meta.fn_nat_max_size; ++i) {
            memcpy(&(nat_fn->entry[i]), fn_nat_buf + offset, sizeof(struct nat_entry));
            offset += sizeof( struct nat_entry );
        }

        OCSSD_DBG_INFO( this, " - read FM nat table");
        struct blk_addr fm_nat_blk = fn_nat_blk;
        struct nvm_addr *fm_nat_nvm_addr = new struct nvm_addr[sb_meta.fm_nat_blk_nr];
        for (int i = 0; i < sb_meta.fm_nat_blk_nr; ++i) {
            bah_0_->BlkAddrAdd(1, &fm_nat_blk);
            fm_nat_nvm_addr[i].ppa = 0;
            bah_0_->convert_2_nvm_addr(&fm_nat_blk, &(fm_nat_nvm_addr[i]));
            OCSSD_DBG_INFO( this, " - nvm_addr idx === " << i );
            OCSSD_DBG_INFO( this, " - - nvm_addr ch    = " << fm_nat_nvm_addr[i].g.ch  );
            OCSSD_DBG_INFO( this, " - - nvm_addr lun   = " << fm_nat_nvm_addr[i].g.lun );
            OCSSD_DBG_INFO( this, " - - nvm_addr plane = " << fm_nat_nvm_addr[i].g.pl  );
            OCSSD_DBG_INFO( this, " - - nvm_addr block = " << fm_nat_nvm_addr[i].g.blk );
        }
        this->fm_nat_vblk = nvm_vblk_alloc(dev, fm_nat_nvm_addr, sb_meta.fm_nat_blk_nr);
        this->fm_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fm_nat_blk_nr;
        this->fm_nat_buf = (char *) nvm_buf_alloc(geo, fm_nat_buf_size);
        nvm_vblk_read(fm_nat_vblk, fm_nat_buf, fm_nat_buf_size);
        offset = 0;
        for (int i = 0; i < sb_meta.fm_nat_max_size; ++i) {
            memcpy(&(nat_fm->entry[i]), fm_nat_buf + offset, sizeof(struct nat_entry) );
            offset += sizeof( struct nat_entry );
        }

        OCSSD_DBG_INFO( this, " - read EXT nat table");
        struct blk_addr ext_nat_blk = fm_nat_blk;
        struct nvm_addr *ext_nat_nvm_addr = new struct nvm_addr[sb_meta.ext_nat_blk_nr];
        for (int i = 0; i < sb_meta.ext_nat_blk_nr; ++i) {
            bah_0_->BlkAddrAdd(1, &ext_nat_blk);
            ext_nat_nvm_addr[i].ppa = 0;
            bah_0_->convert_2_nvm_addr(&ext_nat_blk, &(ext_nat_nvm_addr[i]));
            OCSSD_DBG_INFO( this, " - nvm_addr idx === " << i );
            OCSSD_DBG_INFO( this, " - - nvm_addr ch    = " << ext_nat_nvm_addr[i].g.ch  );
            OCSSD_DBG_INFO( this, " - - nvm_addr lun   = " << ext_nat_nvm_addr[i].g.lun );
            OCSSD_DBG_INFO( this, " - - nvm_addr plane = " << ext_nat_nvm_addr[i].g.pl  );
            OCSSD_DBG_INFO( this, " - - nvm_addr block = " << ext_nat_nvm_addr[i].g.blk );
        }
        this->ext_nat_vblk = nvm_vblk_alloc(dev, ext_nat_nvm_addr, sb_meta.ext_nat_blk_nr);
        this->ext_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.ext_nat_blk_nr;
        this->ext_nat_buf = (char *) nvm_buf_alloc(geo, ext_nat_buf_size);
        nvm_vblk_read(ext_nat_vblk, ext_nat_buf, ext_nat_buf_size);
        offset = 0;
        for (int i = 0; i < sb_meta.ext_nat_max_size; ++i) {
            memcpy(&(nat_ext->entry[i]), ext_nat_buf + offset, sizeof(struct nat_entry) );
            offset += sizeof( struct nat_entry );
        }// */
    }

    // 3. init meta blk area
    OCSSD_DBG_INFO( this, "3. init meta blk area");
    // void xxx_blk_area_init(
	//     int st_ch, int ed_ch,
    //     struct blk_addr* st_addr, size_t* blk_nr,
	//     struct nar_table* nat );
    
    struct blk_addr* fn_st_blk  = new struct blk_addr[ sb_meta.fn_ch_nr ];
    for(size_t ch=sb_meta.fn_st_ch, count = 0; count < sb_meta.fn_ch_nr; ++ch, ++count){
        ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( ch,0,0,0, &(fn_st_blk[ count ]) );
        ocssd_bah->get_blk_addr_handle(ch)->BlkAddrAdd( fn_st_blk_idx[ count ], &(fn_st_blk[ count ]));
    }

    struct blk_addr* fm_st_blk  = new struct blk_addr[ sb_meta.fm_ch_nr ];
    for(size_t ch=sb_meta.fm_st_ch, count = 0; count<sb_meta.fm_ch_nr; ++ch, ++count){
        ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( ch,0,0,0, &(fm_st_blk[ count ]) );
        ocssd_bah->get_blk_addr_handle(ch)->BlkAddrAdd( fm_st_blk_idx[ count ], &(fm_st_blk[ count ]));
    }

    struct blk_addr* ext_st_blk = new struct blk_addr[ sb_meta.ext_ch_nr ];
    for(size_t ch=sb_meta.ext_st_ch, count = 0; count<sb_meta.ext_ch_nr; ++ch, ++count){
        ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( ch,0,0,0, &(ext_st_blk[ count ]) );
        ocssd_bah->get_blk_addr_handle(ch)->BlkAddrAdd( ext_st_blk_idx[ count ], &(ext_st_blk[ count ]));
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
        nat_fn, sb_meta.fn_bitmap_blk_nr
    );
    file_meta_blk_area_init(
        dev, geo,
        sb_meta.fm_st_ch, sb_meta.fm_ch_nr,
        fm_st_blk, fm_blk_nr,
        nat_fm, sb_meta.fm_bitmap_blk_nr
    );
    extent_blk_area_init(
        dev, geo,
        sb_meta.ext_st_ch, sb_meta.ext_ch_nr,
        ext_st_blk, ext_blk_nr,
        nat_ext, sb_meta.ext_bitmap_blk_nr
    );// */
}

void OcssdSuperBlock::format_ssd() {
    OCSSD_DBG_INFO( this, "To Format SSD");

    // erease all block
    OCSSD_DBG_INFO( this, " - Erease All Block");
    ocssd_bah->erase_all_block();

    // write super block
    OCSSD_DBG_INFO( this, " - Write super block");
    this->sb_need_flush  = 1;
    flush_sb(0);

    // write nat
    OCSSD_DBG_INFO( this, " - Write empty NAT table");
    nat_fn  = new struct nat_table;
    nat_fm  = new struct nat_table; // struct nat_entry * entry     // obj_id blk_idx page obj state
    nat_ext = new struct nat_table; // uint6_t max_length
    nat_fn->max_length  = sb_meta.fn_nat_max_size;
    nat_fm->max_length  = sb_meta.fm_nat_max_size;
    nat_ext->max_length = sb_meta.ext_nat_max_size;
    nat_fn->entry  = new struct nat_entry[ nat_fn->max_length  ];
    nat_fm->entry  = new struct nat_entry[ nat_fm->max_length  ];
    nat_ext->entry = new struct nat_entry[ nat_ext->max_length ];
    for(size_t i = 0; i < nat_fn->max_length; ++i ){
        nat_fn->entry[ i ].state = NAT_ENTRY_FREE;
    }
    for(size_t i = 0; i < nat_fm->max_length; ++i ){
        nat_fm->entry[ i ].state = NAT_ENTRY_FREE;
    }
    for(size_t i = 0; i < nat_ext->max_length; ++i ){
        nat_ext->entry[ i ].state = NAT_ENTRY_FREE;
    }
    nat_need_flush = 1;
    this->flush_nat(0);

    // write meta data area
    OCSSD_DBG_INFO( this, " - Write Meta Data Obj");
    struct blk_addr bitmap_addr;
    bitmap_addr.__buf = 0;
        // only need to write all 0 bitmap
    OCSSD_DBG_INFO( this, " - - write FN  bitmap");
    struct nvm_addr* fn_bitmap_nvm_addr = new struct nvm_addr[ sb_meta.fn_bitmap_blk_nr ];
    for(uint32_t i = 0; i < sb_meta.fn_bitmap_blk_nr; ++i ){
        fn_bitmap_nvm_addr[ i ].ppa = 0;
    }
    size_t count = 0;
    size_t ch = sb_meta.fn_st_ch;
    size_t nchs = ocssd_geo_.nchannels;
    size_t idx = 0;
    size_t blk_count = 0;
    ocssd_bah->get_blk_addr_handle( ch )->MakeBlkAddr( ch, 0 , 0 , 0 , &bitmap_addr );
    ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( this->fn_st_blk_idx[ idx ], &bitmap_addr );
    while( count < sb_meta.fn_bitmap_blk_nr ){
        ocssd_bah->get_blk_addr_handle( ch )->convert_2_nvm_addr( &bitmap_addr, &(fn_bitmap_nvm_addr[count]) );
        ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( 1 , &bitmap_addr );
        blk_count += 1;
        count += 1;
        if( count < sb_meta.fn_bitmap_blk_nr && blk_count >= this->fn_blk_nr[ idx ] ){
            blk_count = 0;
            idx += 1; ch = ( ch + 1 ) % nchs;
            ocssd_bah->get_blk_addr_handle( ch )->MakeBlkAddr( ch, 0 , 0 , 0 , &bitmap_addr );
            ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( this->fn_st_blk_idx[ idx ], &bitmap_addr );
        }
    }
    struct nvm_vblk* fn_bitmap_vblk = nvm_vblk_alloc( dev, fn_bitmap_nvm_addr, sb_meta.fn_bitmap_blk_nr );
    size_t fn_bitmap_buf_size = ocssd_geo_.block_nbytes * sb_meta.fn_bitmap_blk_nr;
    void* fn_bitmap_buf = (void *) malloc( fn_bitmap_buf_size );
    memset( fn_bitmap_buf, 0, fn_bitmap_buf_size );    // set all 0
    nvm_vblk_write( fn_bitmap_vblk, fn_bitmap_buf, fn_bitmap_buf_size );
    free( fn_bitmap_nvm_addr );
    free( fn_bitmap_buf );

    OCSSD_DBG_INFO( this, " - - write FM  bitmap");
    struct nvm_addr* fm_bitmap_nvm_addr = new struct nvm_addr[ sb_meta.fm_bitmap_blk_nr ];
    for(uint32_t i = 0; i < sb_meta.fm_bitmap_blk_nr; ++i ){
        fm_bitmap_nvm_addr[ i ].ppa = 0;
    }
    count = 0; ch = sb_meta.fm_st_ch; idx = 0; blk_count = 0;
    ocssd_bah->get_blk_addr_handle( ch )->MakeBlkAddr( ch, 0 , 0 , 0 , &bitmap_addr );
    ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( this->fm_st_blk_idx[ idx ], &bitmap_addr );
    while( count < sb_meta.fm_bitmap_blk_nr ){
        ocssd_bah->get_blk_addr_handle( ch )->convert_2_nvm_addr( &bitmap_addr, &(fm_bitmap_nvm_addr[count]) );
        ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( 1 , &bitmap_addr );
        blk_count += 1; count += 1;
        if( count < sb_meta.fm_bitmap_blk_nr && blk_count >= this->fm_blk_nr[ idx ] ){
            blk_count = 0; idx += 1; ch = ( ch + 1 ) % nchs;
            ocssd_bah->get_blk_addr_handle( ch )->MakeBlkAddr( ch, 0 , 0 , 0 , &bitmap_addr );
            ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( this->fm_st_blk_idx[ idx ], &bitmap_addr );
        }
    }
    struct nvm_vblk* fm_bitmap_vblk = nvm_vblk_alloc( dev, fm_bitmap_nvm_addr, sb_meta.fm_bitmap_blk_nr );
    size_t fm_bitmap_buf_size = ocssd_geo_.block_nbytes * sb_meta.fm_bitmap_blk_nr;
    void* fm_bitmap_buf = (void *) malloc( fm_bitmap_buf_size );
    memset( fm_bitmap_buf, 0, fm_bitmap_buf_size );    // set all 0
    nvm_vblk_write( fm_bitmap_vblk, fm_bitmap_buf, fm_bitmap_buf_size );
    free( fm_bitmap_nvm_addr );
    free( fm_bitmap_buf );

    OCSSD_DBG_INFO( this, " - - write EXT bitmap");
    struct nvm_addr* ext_bitmap_nvm_addr = new struct nvm_addr[ sb_meta.ext_bitmap_blk_nr ];
    for(uint32_t i = 0; i < sb_meta.ext_bitmap_blk_nr; ++i ){
        ext_bitmap_nvm_addr[ i ].ppa = 0;
    }
    count = 0; ch = sb_meta.ext_st_ch; idx = 0; blk_count = 0;
    ocssd_bah->get_blk_addr_handle( ch )->MakeBlkAddr( ch, 0 , 0 , 0 , &bitmap_addr );
    ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( this->ext_st_blk_idx[ idx ], &bitmap_addr );
    while( count < sb_meta.ext_bitmap_blk_nr ){
        ocssd_bah->get_blk_addr_handle( ch )->convert_2_nvm_addr( &bitmap_addr, &(ext_bitmap_nvm_addr[count]) );
        ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( 1 , &bitmap_addr );
        blk_count += 1; count += 1;
        if( count < sb_meta.ext_bitmap_blk_nr && blk_count >= this->ext_blk_nr[ idx ] ){
            blk_count = 0; idx += 1; ch = ( ch + 1 ) % nchs;
            ocssd_bah->get_blk_addr_handle( ch )->MakeBlkAddr( ch, 0 , 0 , 0 , &bitmap_addr );
            ocssd_bah->get_blk_addr_handle( ch )->BlkAddrAdd( this->ext_st_blk_idx[ idx ], &bitmap_addr );
        }
    }
    struct nvm_vblk* ext_bitmap_vblk = nvm_vblk_alloc( dev, ext_bitmap_nvm_addr, sb_meta.ext_bitmap_blk_nr );
    size_t ext_bitmap_buf_size = ocssd_geo_.block_nbytes * sb_meta.ext_bitmap_blk_nr;
    void* ext_bitmap_buf = (void *) malloc( ext_bitmap_buf_size );
    memset( ext_bitmap_buf, 0, ext_bitmap_buf_size );    // set all 0
    nvm_vblk_write( ext_bitmap_vblk, ext_bitmap_buf, ext_bitmap_buf_size );
    free( ext_bitmap_nvm_addr );
    free( ext_bitmap_buf );
}

OcssdSuperBlock::~OcssdSuperBlock()
{
    OCSSD_DBG_INFO( this, "DELETE OcssdSuperBlock");
    this->sb_need_flush  = 0;
    this->nat_need_flush = 1;
    flush();
    // release
    if( fn_st_blk_idx  != nullptr) free(fn_st_blk_idx );
    if( fm_st_blk_idx  != nullptr) free(fm_st_blk_idx );
    if( ext_st_blk_idx != nullptr) free(ext_st_blk_idx);
    if( fn_blk_nr  != nullptr ) free( fn_blk_nr  );
    if( fm_blk_nr  != nullptr ) free( fm_blk_nr  );
    if( ext_blk_nr != nullptr ) free( ext_blk_nr );
    if( sb_addr != nullptr ) free(sb_addr);
    if( sb_vblk != nullptr) nvm_vblk_free( sb_vblk );
    if( sb_meta_buf != nullptr ) free( sb_meta_buf );
    if( fn_nat_vblk != nullptr  ) nvm_vblk_free( fn_nat_vblk  );
    if( fm_nat_vblk != nullptr  ) nvm_vblk_free( fm_nat_vblk  );
    if( ext_nat_vblk != nullptr ) nvm_vblk_free( ext_nat_vblk );
    if( fn_nat_buf != nullptr  ) free( fn_nat_buf  );
    if( fm_nat_buf != nullptr  ) free( fm_nat_buf  );
    if( ext_nat_buf != nullptr ) free( ext_nat_buf );
    addr_free();
}

std::string OcssdSuperBlock::txt()
{
    return "OcssdSuperBlock";
}

void OcssdSuperBlock::flush()
{
    flush_sb(0);
    flush_nat(1);
}

void OcssdSuperBlock::flush_sb(int if_erease)
{
    if( sb_need_flush ){
        sb_need_flush  = 0;
        // write sb_meta into SSD
        OCSSD_DBG_INFO( this, "Flush super block");
        size_t zero = 0;
        blk_addr_handle* bah_0_ = ocssd_bah->get_blk_addr_handle( zero );
        if( this->sb_addr == nullptr ){
            this->sb_addr = new struct blk_addr;
            bah_0_->MakeBlkAddr( zero, zero, zero, zero, sb_addr);
        }
        struct nvm_addr sb_nvm_addr;
        sb_nvm_addr.ppa = 0;
        bah_0_->convert_2_nvm_addr( sb_addr, &sb_nvm_addr );
        if( this->sb_vblk == nullptr ){
            this->sb_vblk = nvm_vblk_alloc( dev, &sb_nvm_addr, 1);
            sb_meta_buf_size = ocssd_geo_.block_nbytes;
            sb_meta_buf = (char *) nvm_buf_alloc( geo, sb_meta_buf_size );
        }
        size_t offset = 0;

        size_t size_ = sizeof( struct ocssd_super_block_meta );
        memcpy( sb_meta_buf + offset, &sb_meta, size_ );
        offset += size_;
        // xx_st_blk_idx - uin32_t
        size_ = sizeof(uint32_t) * sb_meta.fn_ch_nr;
        memcpy( sb_meta_buf + offset, fn_st_blk_idx, size_ );
        offset += size_;
        size_ = sizeof(uint32_t) * sb_meta.fm_ch_nr;
        memcpy( sb_meta_buf + offset, fm_st_blk_idx, size_ );
        offset += size_;
        size_ = sizeof(uint32_t) * sb_meta.ext_ch_nr;
        memcpy( sb_meta_buf + offset, ext_st_blk_idx, size_ );
        offset += size_;
        // xx_blk_nr size_t
        size_ = sizeof( size_t ) * sb_meta.fn_ch_nr;
        memcpy( sb_meta_buf + offset, fn_blk_nr, size_ );
        offset += size_;
        size_ = sizeof( size_t ) * sb_meta.fm_ch_nr;
        memcpy( sb_meta_buf + offset, fm_blk_nr, size_ );
        offset += size_;
        size_ = sizeof( size_t ) * sb_meta.ext_ch_nr;
        memcpy( sb_meta_buf + offset, ext_blk_nr, size_ );
        offset += size_;
        // pad
        memset( sb_meta_buf + offset, 1, sb_meta_buf_size - offset);
        // vblk write
        nvm_vblk_write( sb_vblk, sb_meta_buf, sb_meta_buf_size );
//    uint64_t *magic = ( uint64_t* )sb_meta_buf;
//    uint64_t a = SUPER_BLK_MAGIC_NUM;
    }
}

void OcssdSuperBlock::flush_nat(int if_erease)
{
    if( nat_need_flush ){
        nat_need_flush  = 0;

        if( if_erease ){
            nvm_vblk_erase( fn_nat_vblk  );
            nvm_vblk_erase( fm_nat_vblk  );
            nvm_vblk_erase( ext_nat_vblk );
        }

        size_t offset = 0;
        size_t size_  = 0;
        blk_addr_handle* bah_0_ = ocssd_bah->get_blk_addr_handle( 0 );
        // write nat table into SSD
        OCSSD_DBG_INFO( this, "flush FN  NAT table");
        if( fn_nat_vblk == nullptr ) {
            struct blk_addr fn_nat_blk = *sb_addr;
            struct nvm_addr *fn_nat_nvm_addr = new struct nvm_addr[sb_meta.fn_nat_blk_nr];
            for (int i = 0; i < sb_meta.fn_nat_blk_nr; ++i) {
                bah_0_->BlkAddrAdd(1, &fn_nat_blk);
                fn_nat_nvm_addr[i].ppa = 0;
                bah_0_->convert_2_nvm_addr( &fn_nat_blk, &( fn_nat_nvm_addr[i] ) );
            }
            fn_nat_vblk = nvm_vblk_alloc(dev, fn_nat_nvm_addr, sb_meta.fn_nat_blk_nr);    // get fn vblk
            free(fn_nat_nvm_addr);
            fn_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fn_nat_blk_nr;
            fn_nat_buf = (char *) nvm_buf_alloc(geo, fn_nat_buf_size);
        }
        offset = 0;
        size_ = sizeof(struct nat_entry) * this->nat_fn->max_length;
        memcpy( fn_nat_buf + offset, this->nat_fn->entry, size_ );
        offset += size_;
        memset( fn_nat_buf + offset, 1 , fn_nat_buf_size - offset );
        nvm_vblk_write( fn_nat_vblk, fn_nat_buf, fn_nat_buf_size );

        OCSSD_DBG_INFO( this, "flush FM  NAT table");
        if( fm_nat_vblk == nullptr ) {
            struct blk_addr fm_nat_blk = *sb_addr;
            struct nvm_addr *fm_nat_nvm_addr = new struct nvm_addr[sb_meta.fn_nat_blk_nr];
            for (int i = 0; i < sb_meta.fm_nat_blk_nr; ++i) {
                bah_0_->BlkAddrAdd(1, &fm_nat_blk);
                fm_nat_nvm_addr[i].ppa = 0;
                bah_0_->convert_2_nvm_addr( &fm_nat_blk, &( fm_nat_nvm_addr[i] ) );
            }
            fm_nat_vblk = nvm_vblk_alloc(dev, fm_nat_nvm_addr, sb_meta.fm_nat_blk_nr);    // get fm vblk
            free(fm_nat_nvm_addr);
            fm_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fm_nat_blk_nr;
            fm_nat_buf = (char *) nvm_buf_alloc(geo, fm_nat_buf_size);
        }
        offset = 0;
        size_ = sizeof(struct nat_entry) * this->nat_fm->max_length;
        memcpy( fm_nat_buf + offset, this->nat_fm->entry, size_ );
        offset += size_;
        memset( fm_nat_buf + offset, 1 , fm_nat_buf_size - offset );
        nvm_vblk_write( fm_nat_vblk, fm_nat_buf, fm_nat_buf_size );

        OCSSD_DBG_INFO( this, "flush EXT NAT table " );
        if( ext_nat_vblk == nullptr ) {
            struct blk_addr ext_nat_blk = *sb_addr;
            struct nvm_addr *ext_nat_nvm_addr = new struct nvm_addr[sb_meta.ext_nat_blk_nr];
            for (int i = 0; i < sb_meta.ext_nat_blk_nr; ++i) {
                bah_0_->BlkAddrAdd(1, &ext_nat_blk);
                ext_nat_nvm_addr[i].ppa = 0;
                bah_0_->convert_2_nvm_addr( &ext_nat_blk, &( ext_nat_nvm_addr[i] ) );
            }
            ext_nat_vblk = nvm_vblk_alloc(dev, ext_nat_nvm_addr, sb_meta.ext_nat_blk_nr);    // get fm vblk
            free(ext_nat_nvm_addr);
            ext_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.ext_nat_blk_nr;
            ext_nat_buf = (char *) nvm_buf_alloc(geo, ext_nat_buf_size);
        }
        offset = 0;
        size_ = sizeof(struct nat_entry) * this->nat_ext->max_length;
        memcpy( ext_nat_buf + offset, this->nat_ext->entry, size_ );
        offset += size_;
        memset( ext_nat_buf + offset, 1 , ext_nat_buf_size - offset );
        nvm_vblk_write( ext_nat_vblk, ext_nat_buf, ext_nat_buf_size );
    }
}
