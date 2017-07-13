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
//              blk[3]  | fm_nat
//              blk[4]  ┘
//              blk[5]  - ext_nat
// all channel
//              blks    fn_obj      [0,map_size-1]:bitmap [map_size,size-1]:obj
//              blks    fm_obj      [0,map_size-1]:bitmap [map_size,size-1]:obj
//              blks    ext_obj     [0,map_size-1]:bitmap [map_size,size-1]:obj
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

        ocssd_geo_.fn_blk_nr  = ocssd_geo_.fn_obj_nr  / ocssd_geo_.fn_blk_obj_nr  + 1;
        ocssd_geo_.fm_blk_nr  = ocssd_geo_.fm_blk_nr  / ocssd_geo_.fm_blk_obj_nr  + 1;
        ocssd_geo_.ext_blk_nr = ocssd_geo_.ext_obj_nr / ocssd_geo_.ext_blk_obj_nr + 1;

    ocssd_geo_.nat_entry_nbytes = 16;   // 16 = 8(ID) + 4(blk) + 2(page) + 1(obj) + 1(state)
    ocssd_geo_.nat_fn_entry_nr  = ocssd_geo_.fn_3LVL_obj_nr;
    ocssd_geo_.nat_fm_entry_nr  = ocssd_geo_.file_max_nr;
    ocssd_geo_.nat_ext_entry_nr = ocssd_geo_.ext_3LVL_obj_nr;

        ocssd_geo_.nat_fn_blk_nr  = ocssd_geo_.nat_fn_entry_nr  * ocssd_geo_.nat_entry_nbytes / ocssd_geo_.block_nbytes + 1;
        ocssd_geo_.nat_fm_blk_nr  = ocssd_geo_.nat_fm_entry_nr  * ocssd_geo_.nat_entry_nbytes / ocssd_geo_.block_nbytes + 1;
        ocssd_geo_.nat_ext_blk_nr = ocssd_geo_.nat_ext_entry_nr * ocssd_geo_.nat_entry_nbytes / ocssd_geo_.block_nbytes + 1;
}

OcssdSuperBlock::OcssdSuperBlock(){
    sb_need_flush = 0;
    nat_need_flush = 0;

    dev = nvm_dev_open( OC_DEV_PATH );
    geo = nvm_dev_get_geo( dev );

    gen_ocssd_geo( geo );

    OCSSD_DBG_INFO( this, "OCSSD sector  nr = " << ocssd_geo_.nsectors );
    OCSSD_DBG_INFO( this, "OCSSD page    nr = " << ocssd_geo_.npages );
    OCSSD_DBG_INFO( this, "OCSSD block   nr = " << ocssd_geo_.nblocks );
    OCSSD_DBG_INFO( this, "OCSSD plane   nr = " << ocssd_geo_.nplanes );
    OCSSD_DBG_INFO( this, "OCSSD lun     nr = " << ocssd_geo_.nluns );
    OCSSD_DBG_INFO( this, "OCSSD channel nr = " << ocssd_geo_.nchannels );
    OCSSD_DBG_INFO( this, "OCSSD disk    nr = 1" );

    OCSSD_DBG_INFO( this, "OCSSD sector  size = " << ocssd_geo_.sector_nbytes );
    OCSSD_DBG_INFO( this, "OCSSD page    size = " << ocssd_geo_.page_nbytes );
    OCSSD_DBG_INFO( this, "OCSSD block   size = " << ocssd_geo_.block_nbytes );
    OCSSD_DBG_INFO( this, "OCSSD plane   size = " << ocssd_geo_.plane_nbytes );
    OCSSD_DBG_INFO( this, "OCSSD lun     size = " << ocssd_geo_.lun_nbytes );
    OCSSD_DBG_INFO( this, "OCSSD channel size = " << ocssd_geo_.channel_nbytes );
    OCSSD_DBG_INFO( this, "OCSSD disk    size = " << ocssd_geo_.ssd_nbytes );

    OCSSD_DBG_INFO( this, "extent struct     size = " << ocssd_geo_.extent_nbytes );
    OCSSD_DBG_INFO( this, "extent descriptor size = " << ocssd_geo_.extent_des_nbytes );

    OCSSD_DBG_INFO( this, "file_min_size = " << ocssd_geo_.file_min_nbytes );
    OCSSD_DBG_INFO( this, "file_max_size = " << ocssd_geo_.file_max_nbytes );
    OCSSD_DBG_INFO( this, "file_min_nr   = " << ocssd_geo_.file_min_nr );
    OCSSD_DBG_INFO( this, "file_max_nr   = " << ocssd_geo_.file_max_nr );

    OCSSD_DBG_INFO( this, "fn_btree_degree   = " << ocssd_geo_.fn_btree_degree );
    OCSSD_DBG_INFO( this, "fn_1LVL_obj_nr    = " << ocssd_geo_.fn_1LVL_obj_nr );
    OCSSD_DBG_INFO( this, "fn_2LVL_obj_nr    = " << ocssd_geo_.fn_2LVL_obj_nr );
    OCSSD_DBG_INFO( this, "fn_3LVL_obj_nr    = " << ocssd_geo_.fn_3LVL_obj_nr );

    OCSSD_DBG_INFO( this, "file_max_nr       = " << ocssd_geo_.file_max_nr );

    OCSSD_DBG_INFO( this, "ext_btree_degree  = " << ocssd_geo_.ext_btree_degree );
    OCSSD_DBG_INFO( this, "ext_1LVL_obj_nr   = " << ocssd_geo_.ext_1LVL_obj_nr );
    OCSSD_DBG_INFO( this, "ext_2LVL_obj_nr   = " << ocssd_geo_.ext_2LVL_obj_nr );
    OCSSD_DBG_INFO( this, "ext_3LVL_obj_nr   = " << ocssd_geo_.ext_3LVL_obj_nr );

    OCSSD_DBG_INFO( this, "fn_obj_nr  = " << ocssd_geo_.fn_obj_nr );
    OCSSD_DBG_INFO( this, "fm_obj_nr  = " << ocssd_geo_.fm_obj_nr );
    OCSSD_DBG_INFO( this, "ext_obj_nr = " << ocssd_geo_.ext_obj_nr );

    OCSSD_DBG_INFO( this, "fn_blk_nr  = " << ocssd_geo_.fn_blk_nr  );
    OCSSD_DBG_INFO( this, "fm_blk_nr  = " << ocssd_geo_.fm_blk_nr  );
    OCSSD_DBG_INFO( this, "ext_blk_nr = " << ocssd_geo_.ext_blk_nr );

    OCSSD_DBG_INFO( this, "nat_fn_blk_nr  = " << ocssd_geo_.nat_fn_blk_nr  );
    OCSSD_DBG_INFO( this, "nat_fm_blk_nr  = " << ocssd_geo_.nat_fm_blk_nr  );
    OCSSD_DBG_INFO( this, "nat_ext_blk_nr = " << ocssd_geo_.nat_ext_blk_nr );

    addr_init( geo );   // init blk_handler

    // first block of SSD store super_block_meta
    blk_addr_handle* bah_0_ = ocssd_bah->get_blk_addr_handle( 0 );
    bah_0_->MakeBlkAddr(0,0,0,0,sb_addr);
    struct nvm_addr sb_nvm_addr;
    bah_0_->convert_2_nvm_addr( sb_addr, &sb_nvm_addr );

bah_0_->PrBlkAddr( sb_addr, true, " first block of SSD to store sb_meta.");

    // 1. read super super_block_meta
    sb_vblk = nvm_vblk_alloc( dev, &sb_nvm_addr, 1);
    sb_meta_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes;
    sb_meta_buf = (char *) nvm_buf_alloc( geo, sb_meta_buf_size );
    nvm_vblk_read( sb_vblk, sb_meta_buf, sb_meta_buf_size );
    // struct ocssd_super_block_meta sb_meta

    int need_format =     // first to run on disk or OCSSD_REFORMAT_SSD
        ( OCSSD_REFORMAT_SSD || 
          !sb_meta.magic_num == SUPER_BLK_MAGIC_NUM )?1:0;

    if( need_format ){
        printf("Not Formated dev : %s\n", OC_DEV_PATH );
//      exit(-1);

        sb_need_flush = 1;
        nat_need_flush = 1;

        sb_meta.magic_num = SUPER_BLK_MAGIC_NUM;

        sb_meta.fn_st_ch = 1;
        sb_meta.fn_ed_ch = geo->nchannels;
        sb_meta.fn_ch_nr = sb_meta.fn_ed_ch - sb_meta.fn_st_ch + 1;

        sb_meta.fm_st_ch = 0;
        sb_meta.fm_ed_ch = geo->nchannels;
        sb_meta.fm_ch_nr = sb_meta.fm_ed_ch - sb_meta.fm_st_ch + 1;

        sb_meta.ext_st_ch = 0;
        sb_meta.ext_ed_ch = geo->nchannels;
        sb_meta.ext_ch_nr = sb_meta.ext_ed_ch - sb_meta.ext_st_ch + 1;

        sb_meta.fn_nat_max_size  = 336 * 336;       // 8M/4K = 2048  degree = 336
        sb_meta.fm_nat_max_size  = 1000000;         // 8M/1K = 8192
        sb_meta.ext_nat_max_size = 128 * 128;       // 8M/4K = 2048  degree = 128

        // sizeof ( struct nat_entry ) = 12 B
        // 8M/12B = 699050.xxx
        sb_meta.fn_nat_blk_nr  = 1;     // 699050                   >   112896
        sb_meta.fm_nat_blk_nr  = 3;     // 699050 * 3 = 2097150     >   1000000
        sb_meta.ext_nat_blk_nr = 1;     // 699050                   >   16384
    }

    // 2. read nat table
    // struct nar_table* nat_fn;
    // struct nar_table* nat_fm;
    // struct nar_table* nat_ext;
    printf("[OcssdSuperBlock] fn_nat_max_size  = %lu\n", sb_meta.fn_nat_max_size );
    printf("[OcssdSuperBlock] fm_nat_max_size  = %lu\n", sb_meta.fm_nat_max_size );
    printf("[OcssdSuperBlock] ext_nat_max_size = %lu\n", sb_meta.ext_nat_max_size);
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
    //     struct blk_addr* st_addr, struct blk_addr* ed_addr,
	//     struct nar_table* nat );
    
    struct blk_addr* fn_st_blk  = new struct blk_addr[ sb_meta.fn_ch_nr ];
    size_t* fn_blk_nr = new size_t[ sb_meta.fn_ch_nr ];
    for(int ch=sb_meta.fn_st_ch; ch<sb_meta.fn_ed_ch; ++ch){
        //ocssd_bah->get_blk_addr_handle(ch)->.MakeBlkAddr( , , , , fn_st_blk[ ch - sb_meta.fn_st_ch ]);
        //fn_blk_nr[ ch - sb_meta.fn_st_ch ] = ;
    }

    struct blk_addr* fm_st_blk  = new struct blk_addr[ sb_meta.fm_ch_nr ];
    size_t* fm_blk_nr = new size_t[ sb_meta.fm_ch_nr ];
    for(int ch=sb_meta.fm_st_ch; ch<sb_meta.fm_ed_ch; ++ch){
        //ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( , , , , fm_st_blk[ ch - sb_meta.fm_st_ch ]);
        //fm_blk_nr[ ch - sb_meta.fm_st_ch ] = ;
    }

    struct blk_addr* ext_st_blk = new struct blk_addr[ sb_meta.ext_ch_nr ];
    size_t* ext_blk_nr = new size_t[ sb_meta.ext_ch_nr ];
    for(int ch=sb_meta.ext_st_ch; ch<sb_meta.ext_ed_ch; ++ch){
        //ocssd_bah->get_blk_addr_handle(ch)->MakeBlkAddr( , , , , ext_st_blk[ ch - sb_meta.ext_st_ch ]);
        //ext_blk_nr[ ch - sb_meta.ext_st_ch ] = ;
    }

	// struct nvm_dev* dev,
	// const struct nvm_geo* geo,
	// uint32_t st_ch, uint32_t ed_ch,
	// struct blk_addr* st_addr, size_t* addr_nr,
	// struct nat_table* nat
    //
    file_name_blk_area_init(
        dev, geo,
        sb_meta.fn_st_ch, sb_meta.fn_ed_ch, // uint32_t
        fn_st_blk, fn_blk_nr,
        nat_fn
    );
    file_meta_blk_area_init(
        dev, geo,
        sb_meta.fm_st_ch, sb_meta.fm_ed_ch,
        fm_st_blk, fm_blk_nr,
        nat_fm
    );
    extent_blk_area_init(
        dev, geo,
        sb_meta.ext_st_ch, sb_meta.ext_ed_ch,
        ext_st_blk, ext_blk_nr,
        nat_ext
    );// */

    flush();
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
