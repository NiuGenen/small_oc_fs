#ifndef _SUPER_BLK_AREA_H_
#define _SUPER_BLK_AREA_H_

#define SUPER_BLK_MAGIC_NUM 0x1234567812345678

/*

1. read super OcssdSuperBlock

2. read nat table

3. extern MetaBlkArea *mba_file_meta;
   extern MetaBlkArea *mba_extent;
   extern MetaBlkArea *mba_file_name;

*/

struct ocssd_sb_meta{  
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

    uint32_t fn_obj_size;
    uint32_t fm_obj_size;
    uint32_t ext_obj_size;      // 4 B * 3 = 12 B

    uint32_t fn_ch_nr;
    uint32_t fm_ch_nr;
    uint32_t ext_ch_nr;         // 4 B * 3 = 12 B
};

class OcssdSuperBlock{
public:
    OcssdSuperBlock();

private:
    struct nvm_dev * dev;
    struct nvm_addr sb_addr;
    
    struct ocssd_sb_1 sb_meta;

    uint64_t* fn_st_blk;
    uint64_t* fn_ed_blk;        // 16 B * fn_ch_nr

    uint64_t* fm_st_blk;
    uint64_t* fm_ed_blk;        // 16 B * fm_ch_nr

    uint64_t* ext_st_blk;
    uint64_t* ext_ed_blk;       // 16 B * ext_ch_nr

    struct nar_table* nat_fn;
    struct nar_table* nat_fm;
    struct nar_table* nat_ext;
}

#endif