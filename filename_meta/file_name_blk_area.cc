#include "file_name_blk_area.h"
#include "DirBTree.h"

//global var
MetaBlkArea *mba_file_name;
DirBTree* dirBTree;

#define MBA_FILE_NAME_OBJ_SIZE  4096
#define MBA_FILE_NAME_NAME      "MBA_FILE_NAME"

void file_name_blk_area_init(
	struct nvm_dev* dev,
    const struct nvm_geo* geo,
	uint32_t st_ch, uint32_t ch_nr,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat, uint32_t bitmap_blk_nr)
{
    mba_file_name = new MetaBlkArea(
		dev, geo,
		MBA_FILE_NAME_OBJ_SIZE,
		MBA_FILE_NAME_NAME,
		st_ch, ch_nr,
		st_addr, addr_nr,
		nat,bitmap_blk_nr
    );
}

void file_name_btree_init( Nat_Obj_ID_Type root_id)
{
	dirBTree = new DirBTree( root_id );
}
