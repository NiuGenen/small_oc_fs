#include "file_meta_blk_area.h"

#define MBA_FILE_META_NAME "FileMeta"
#define MBA_FILE_META_OBJ_SIZE 1024

//global var
MetaBlkArea *mba_file_meta;

void file_meta_blk_area_init(
	struct nvm_dev* dev,
    const struct nvm_geo* geo,
	uint32_t st_ch, uint32_t ch_nr,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat , uint32_t bitmap_blk_nr)
{
    mba_file_meta = new MetaBlkArea(
        dev, geo,
        MBA_FILE_META_OBJ_SIZE,
        MBA_FILE_META_NAME,
        st_ch, ch_nr,
        st_addr, addr_nr,
        nat,bitmap_blk_nr
    );
}
