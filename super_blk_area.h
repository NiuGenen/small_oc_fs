#ifndef _SUPER_BLK_AREA_H_
#define _SUPER_BLK_AREA_H_

#include "blk_addr.h"

class OcssdSuperBlock{
public:
    OcssdSuperBlock();
    ~OcssdSuperBlock();

private:
    struct nvm_dev* dev;
    const struct nvm_geo* geo;

    struct ocssd_super_block_meta{  
        uint64_t magic_num;         //  8 B

        uint32_t fn_st_ch;
        uint32_t fn_ed_ch;
        uint32_t fm_st_ch;
        uint32_t fm_ed_ch;
        uint32_t ext_st_ch;
        uint32_t ext_ed_ch;         // 4 B * 6 = 24 B

        uint64_t fn_nat_max_size;
        uint64_t fm_nat_max_size;
        uint64_t ext_nat_max_size;  // 8 B * 3 = 24 B

        uint32_t fn_nat_blk_nr;
        uint32_t fm_nat_blk_nr;
        uint32_t ext_nat_blk_nr;    // 4 B * 3 = 12 B

        uint32_t fn_obj_size;
        uint32_t fm_obj_size;
        uint32_t ext_obj_size;      // 4 B * 3 = 12 B

        uint32_t fn_ch_nr;
        uint32_t fm_ch_nr;
        uint32_t ext_ch_nr;         // 4 B * 3 = 12 B
    };

    struct blk_addr* sb_addr;
    struct nvm_vblk* sb_vblk;
    size_t sb_meta_buf_size;
    char* sb_meta_buf;
    struct ocssd_super_block_meta sb_meta;
    
    struct nar_table* nat_fn;
    struct nar_table* nat_fm;
    struct nar_table* nat_ext;

    struct nvm_vblk* fn_nat_vblk;
    struct nvm_vblk* fm_nat_vblk;
    struct nvm_vblk* ext_nat_vblk;
    size_t fn_nat_buf_size;
    char* fn_nat_buf;
    size_t fm_nat_buf_size;
    char* fm_nat_buf;
    size_t ext_nat_buf_size;
    char* ext_nat_buf;

    int sb_need_flush;
    int nat_need_flush;
    void flush();
    void flush_sb();
    void flush_nat();
}

#endif