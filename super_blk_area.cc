#include "super_blk_area.h"
#include "extent_meta/extent_blk_area.h"
#include "file_meta/file_meta_blk_area.h"
#include "filename_meta/file_name_blk_area.h"
#include <liblightnvm.h>
#include <stdio.h>
#include "blk_addr.h"

#define OC_DEV_PATH "/dev/nvm0n1"
#define SUPER_BLK_MAGIC_NUM 0x1234567812345678

#define OCSSD_REFORMAT_SSD 0

extern blk_addr_handle **blk_addr_handlers_of_ch;

// channel[0]
//              blk[0]  super_block_meta
//              blk[1]  - fn_nat
//              blk[2]  ┐
//              blk[3]  | fm_nat
//              blk[4]  ┘
//              blk[5]  - ext_nat

OcssdSuperBlock::OcssdSuperBlock(){
    need_flush = 0;

    dev = nvm_dev_open( OC_DEV_PATH );
    geo = nvm_dev_get_geo( dev );

    addr_init( geo );   // init blk_handler

    // first block of SSD store super_block_meta
    blk_addr_handlers_of_ch[0].MakeBlkAddr(0,0,0,0,sb_addr);
    struct nvm_addr sb_nvm_addr;
    blk_addr_handlers_of_ch[0].convert_2_nvm_addr( sb_addr, &sb_nvm_addr );

    // 1. read super super_block_meta
    sb_vblk = nvm_vblk_alloc( dev, &sb_nvm_addr, 1);
    sb_meta_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes;
    sb_meta_buf = nvm_buf_alloc( geo, sb_meta_buf_size );
    nvm_vblk_read( sb_vblk, sb_meta_buf, sb_meta_buf_size );
    // struct ocssd_super_block_meta sb_meta

    int need_format =     // first to run on disk or OCSSD_REFORMAT_SSD
        ( OCSSD_REFORMAT_SSD || 
          !sb_meta.magic_num == SUPER_BLK_MAGIC_NUM )?1:0;

    if( need_format ){
        printf("Not Formated dev : %s\n", OC_DEV_PATH );
        exit(-1);
        
        need_flush = 1;

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
    printf("[OcssdSuperBlock] fn_nat_max_size  = %ul\n", sb_meta.fn_nat_max_size );
    printf("[OcssdSuperBlock] fm_nat_max_size  = %ul\n", sb_meta.fm_nat_max_size );
    printf("[OcssdSuperBlock] ext_nat_max_size = %ul\n", sb_meta.ext_nat_max_size);
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
    struct blk_addr fn_nat_blk = sb_addr;
    struct nvm_addr* fn_nat_nvm_addr = new struct nvm_addr[ sb_meta.fn_nat_blk_nr ];
    for(int i=0; i<sb_meta.fn_nat_blk_nr; ++i){
        blk_addr_handlers_of_ch[0].BlkAddrAdd( 1, &fn_nat_blk );
        blk_addr_handlers_of_ch[0].convert_2_nvm_addr( &fn_nat_blk, &(fn_nat_nvm_addr[i]) );
    }
    fn_nat_vblk = nvm_vblk_alloc( dev, fn_nat_nvm_addr, sb_meta.fn_nat_blk_nr );    // get fn vblk
    // free( fn_nat_nvm_addr )
    fn_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fn_nat_blk_nr ;
    fn_nat_buf = nvm_buf_alloc( geo, fn_nat_buf_size ); // read vblk into fn_nat_buf
    nvm_vblk_read( fn_nat_vblk, fn_nat_buf, fn_nat_buf_size );
    for(int i=0; i<sb_meta.fn_nat_max_size; ++i){
        memcpy( &(nat_fn->entry[i]),  // nat_fn->entry[i]
            fn_nat_buf + sizeof(struct nat_entry) * i,
            sizeof(struct nat_entry) );
    }

    struct blk_addr fm_nat_blk = fn_nat_blk;
    struct nvm_addr* fm_nat_nvm_addr = new struct nvm_addr[ sb_meta.fm_nat_blk_nr ];
    for(int i=0; i<sb_meta.fm_nat_blk_nr; ++i){
        blk_addr_handlers_of_ch[0].BlkAddrAdd( 1, &fm_nat_blk );
        blk_addr_handlers_of_ch[0].convert_2_nvm_addr( &fm_nat_blk, &(fm_nat_nvm_addr[i]) );
    }
    fm_nat_vblk = nvm_vblk_alloc( dev, fm_nat_nvm_addr, sb_meta.fm_nat_blk_nr );
    fm_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.fm_nat_blk_nr ;
    fm_nat_buf = nvm_buf_alloc( geo, fm_nat_buf_size );
    nvm_vblk_read( fm_nat_vblk, fm_nat_buf, fm_nat_buf_size );
    for(int i=0; i<sb_meta.fm_nat_max_size; ++i){
        // nat_fm->entry[i]
    }

    struct blk_addr ext_nat_blk = fm_nat_blk;
    struct nvm_addr* ext_nat_nvm_addr = new struct nvm_addr[ sb_meta.ext_nat_blk_nr ];
    for(int i=0; i<sb_meta.ext_nat_blk_nr; ++i){
        blk_addr_handlers_of_ch[0].BlkAddrAdd( 1, &ext_nat_blk );
        blk_addr_handlers_of_ch[0].convert_2_nvm_addr( &ext_nat_blk, &(ext_nat_nvm_addr[i]) );
    }
    ext_nat_vblk = nvm_vblk_alloc( dev, ext_nat_nvm_addr, sb_meta.ext_nat_blk_nr );
    ext_nat_buf_size = geo->npages * geo->nsectors * geo->sector_nbytes * sb_meta.ext_nat_blk_nr ;
    ext_nat_buf = nvm_buf_alloc( geo, ext_nat_buf_size );
    nvm_vblk_read( ext_nat_vblk, ext_nat_buf, ext_nat_buf_size );
    for(int i=0; i<sb_meta.ext_nat_max_size; ++i){
        // nat_ext->entry[i]
    }

    // 3. init meta blk area
    //
    // void xxx_blk_area_init(
	//     int st_ch, int ed_ch,
    //     struct blk_addr* st_addr, struct blk_addr* ed_addr,
	//     struct nar_table* nat );
    //
    struct bblk_addr* fn_st_blk  = new struct blk_addr[ sb_meta.fn_ch_nr ];
    size_t* fn_blk_nr = new size_t[ sb_meta.fn_ch_nr ];
    for(int ch=sb_meta.fn_st_ch; ch<sb_meta.fn_ed_ch; ++ch){
        blk_addr_handlers_of_ch[ ch ].MakeBlkAddr( , , , , fn_st_blk[ ch - sb_meta.fn_st_ch ]);
        fn_blk_nr[ ch - sb_meta.fn_st_ch ] = ;
    }

    struct bblk_addr* fm_st_blk  = new struct blk_addr[ sb_meta.fm_ch_nr ];
    size_t* fm_blk_nr = new size_t[ sb_meta.fm_ch_nr ];
    for(int ch=sb_meta.fm_st_ch; ch<sb_meta.fm_ed_ch; ++ch){
        blk_addr_handlers_of_ch[ ch ].MakeBlkAddr( , , , , fm_st_blk[ ch - sb_meta.fm_st_ch ]);
        fm_blk_nr[ ch - sb_meta.fm_st_ch ] = ;
    }

    struct bblk_addr* ext_st_blk = new struct blk_addr[ sb_meta.ext_ch_nr ];
    size_t* ext_blk_nr = new size_t[ sb_meta.ext_ch_nr ];
    for(int ch=sb_meta.ext_st_ch; ch<sb_meta.ext_ed_ch; ++ch){
        blk_addr_handlers_of_ch[ ch ].MakeBlkAddr( , , , , ext_st_blk[ ch - sb_meta.ext_st_ch ]);
        ext_blk_nr[ ch - sb_meta.ext_st_ch ] = ;
    }

    file_name_blk_area_init(
        geo,
        sb_meta.fn_st_ch, sb_meta.fn_ed_ch,
        fn_st_blk, fn_blk_nr,
        nat_fn, sb_meta.fn_nat_max_size
    );
    file_meta_blk_area_init(
        geo,
        sb_meta.fm_st_ch, sb_meta.fm_ed_ch,
        fm_st_blk, fm_blk_nr,
        nat_fm, sb_meta.fm_nat_max_size
    );
    extent_blk_area_init(
        geo,
        sb_meta.ext_st_ch, sb_meta.ext_ed_ch,
        ext_st_blk, ext_blk_nr,
        nat_ext, sb_mate.ext_nat_max_size
    );
}

OcssdSuperBlock::~OcssdSuperBlock()
{
    flush();
    // release pointer
}

OcssdSuperBlock::flush()
{
    flush_sb();
    flush_nat();
}

OcssdSuperBlock::flush_sb()
{
    if( sb_need_flush ){
        // write sb_meta into SSD
    }
}

OcssdSuperBlock::flush_nat()
{
    if( nat_need_flush ){
        // write nat table into SSD
    }
}