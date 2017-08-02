#ifndef _SUPER_BLK_AREA_H_
#define _SUPER_BLK_AREA_H_

#include <iostream>
#include <stddef.h>
#include "blk_addr.h"

class OcssdSuperBlock{
public:
    OcssdSuperBlock();
    ~OcssdSuperBlock();

private:
    struct nvm_dev* dev;
    const struct nvm_geo* geo;

    void gen_ocssd_geo(const nvm_geo* geo);

    struct ocssd_geo{
        size_t nchannels;
        size_t nluns;
        size_t nplanes;
        size_t nblocks;
        size_t npages;
        size_t nsectors;

        size_t ssd_nbytes;
        size_t channel_nbytes;
        size_t lun_nbytes;
        size_t plane_nbytes;
        size_t block_nbytes;
        size_t page_nbytes;
        size_t sector_nbytes;

        size_t extent_nbytes;       //  24 = 8(st) + 8(ed) + 8(junk)
        size_t extent_des_nbytes;   //  24 = 8(st) + 8(ed) + 8(ch)
        size_t max_ext_addr_nr;     //  2
        size_t min_ext_addr_nr;     //  8

        size_t file_ext_nr; //  fm_obj_nbytes - 248 -

        size_t file_min_nbytes;
        size_t file_max_nbytes;
        size_t file_min_nr;
        size_t file_max_nr;
        size_t file_avg_nr;

        size_t fn_btree_degree;  //  336
        size_t fn_obj_nbytes;    //  4096
        size_t fn_blk_obj_nr;    //  block_nbytes / fn_obj_nbytes
        size_t fn_1LVL_obj_nr;   //  fn_btree_degree * fn_btree_degree + fn_btree_degree + 1
        size_t fn_2LVL_obj_nr;   //  fn_btree_degree * fn_btree_degree + fn_btree_degree + 1
        size_t fn_3LVL_obj_nr;   //  fn_btree_degree * fn_btree_degree + fn_btree_degree + 1
        size_t fn_1LVL_cnt;
        size_t fn_2LVL_cnt;
        size_t fn_3LVL_cnt;
        size_t fn_obj_nr;
        size_t fn_data_blk_nr;        //  fn_3LVL_obj_nr / fn_blk_obj_nr + 1
        size_t fn_bitmap_nr;
        size_t fn_bitmap_blk_nr;
        size_t fn_blk_nr;

        size_t fm_obj_nbytes;   //  1024
        size_t fm_blk_obj_nr;
        size_t fm_obj_nr;       //  file_max_nr
        size_t fm_data_blk_nr;       //  fm_obj_nr * fm_obj_nbytes / block_nbytes + 1
        size_t fm_bitmap_nr;
        size_t fm_bitmap_blk_nr;
        size_t fm_blk_nr;

        size_t ext_btree_degree;  //  128
        size_t ext_obj_nbytes;    //  4096
        size_t ext_blk_obj_nr;    //  block_nbytes / ext_obj_nbytes
        size_t ext_1LVL_obj_nr;   //  ext_btree_degree * ext_btree_degree + ext_btree_degree + 1
        size_t ext_2LVL_obj_nr;   //  ext_btree_degree * ext_btree_degree + ext_btree_degree + 1
        size_t ext_3LVL_obj_nr;   //  ext_btree_degree * ext_btree_degree + ext_btree_degree + 1
        size_t ext_1LVL_cnt;
        size_t ext_2LVL_cnt;
        size_t ext_3LVL_cnt;
        size_t ext_obj_nr;
        size_t ext_data_blk_nr;        //  ext_3LVL_obj_nr / ext_blk_obj_nr + 1
        size_t ext_bitmap_nr;
        size_t ext_bitmap_blk_nr;        //  ext_3LVL_obj_nr / ext_blk_obj_nr + 1
        size_t ext_blk_nr;

        size_t nat_entry_nbytes;    //  16 = 8(ID) + 4(blk) + 2(page) + 1(obj) + 1(state)
        size_t nat_fn_entry_nr;     //  fn_3LVL_obj_nr
        size_t nat_fm_entry_nr;     //  file_max_nr
        size_t nat_ext_entry_nr;    //  ext_3LVL_obj_nr

        size_t nat_fn_blk_nr;   //  nat_fn_entry_nr  * nat_entry_nbytes / block_nbytes + 1
        size_t nat_fm_blk_nr;   //  nat_fm_entry_nr  * nat_entry_nbytes / block_nbytes + 1
        size_t nat_ext_blk_nr;  //  nat_ext_entry_nr * nat_entry_nbytes / block_nbytes + 1

        size_t sb_nbytes;   // sizeof(ocssd_super_block_meta)

    } ocssd_geo_;

    struct ocssd_super_block_meta{
        uint64_t magic_num;

        uint32_t fn_nat_blk_nr;
        uint32_t fm_nat_blk_nr;
        uint32_t ext_nat_blk_nr;

        uint64_t fn_nat_max_size;
        uint64_t fm_nat_max_size;
        uint64_t ext_nat_max_size;

        uint32_t fn_st_ch;
        uint32_t fm_st_ch;
        uint32_t ext_st_ch;

        uint32_t fn_ch_nr;   // uint32_t fn_st_blk_idx  [ fn_ch_nr  ]      uint32_t fn_blk_nr  [ fn_ch_nr  ]
        uint32_t fm_ch_nr;   // uint32_t fm_st_blk_idx  [ fm_ch_nr  ]      uint32_t fm_blk_nr  [ fm_ch_nr  ]
        uint32_t ext_ch_nr;  // uint32_t ext_st_blk_idx [ ext_ch_nr ]      uint32_t ext_blk_nr [ ext_ch_nr ]

        uint32_t fn_obj_size;
        uint32_t fm_obj_size;
        uint32_t ext_obj_size;
    };
    uint32_t* fn_st_blk_idx;
    uint32_t* fm_st_blk_idx;
    uint32_t* ext_st_blk_idx;

    uint32_t* fn_blk_nr;
    uint32_t* fm_blk_nr;
    uint32_t* ext_blk_nr;

    struct blk_addr* sb_addr;
    struct nvm_vblk* sb_vblk;
    size_t sb_meta_buf_size;
    char* sb_meta_buf;    //→-----------------------┐
    struct ocssd_super_block_meta sb_meta;   //←---┘

    struct nvm_vblk* fn_nat_vblk;
    struct nvm_vblk* fm_nat_vblk;
    struct nvm_vblk* ext_nat_vblk;
    size_t fn_nat_buf_size;
    char* fn_nat_buf;
    struct nat_table* nat_fn;
    size_t fm_nat_buf_size;
    char* fm_nat_buf;
    struct nat_table* nat_fm;
    size_t ext_nat_buf_size;
    char* ext_nat_buf;
    struct nat_table* nat_ext;

    int sb_need_flush;
    int nat_need_flush;
    void flush();
    void flush_sb();
    void flush_nat();

    void format_ssd();
    std::string txt();
};

#endif
