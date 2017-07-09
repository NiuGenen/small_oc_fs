#include "file_meta_blk_area.h"
#include "../meta_blk_area.h"
#include <liblightnvm.h>

#define MBA_FILE_META_NAME "FileMeta"
#define MBA_FILE_META_OBJ_SIZE 1024

//global var
extern MetaBlkArea *mba_file_meta;

void file_meta_blk_area_init(
    const struct nvm_geo* geo,
	int st_ch, int ed_ch,
	struct blk_addr* st_addr, size_t addr_nr,
	struct nar_table* nat, size_t nat_max_length )
{
    mba_file_meta = new MetaBlkArea(
        MBA_FILE_META_OBJ_SIZE,
        geo->page_nbytes / MBA_FILE_META_OBJ_SIZE,
        MBA_FILE_META_NAME,
        st_ch, ed_ch,
        st_addr, addr_nr,
        nat, nat_max_length
    );
}