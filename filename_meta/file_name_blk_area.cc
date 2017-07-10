#include "file_name_blk_area.h"
#include "../meta_blk_area.h"
#include <liblightnvm.h>

//global var
MetaBlkArea *mba_file_name;

#define MBA_FILE_NAME_OBJ_SIZE  4096
#define MBA_FILE_NAME_NAME      "MBA_FILE_NAME"

void file_name_blk_area_init(
	struct nvm_dev* dev,
    const struct nvm_geo* geo,
	uint32_t st_ch, uint32_t ed_ch,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat)
{
    mba_file_name = new MetaBlkArea(
		dev, geo,
		MBA_FILE_NAME_OBJ_SIZE,
		MBA_FILE_NAME_NAME,
		st_ch, ed_ch,
		st_addr, addr_nr,
		nat
    );
}
