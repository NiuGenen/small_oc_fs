#include "extent_blk_area.h"

#define MBA_EXTENT_NAME        "MBA_EXTENT"
#define MBA_EXTENT_OBJ_SIZE    4096

//global var
MetaBlkArea *mba_extent;

void extent_blk_area_init(
	struct nvm_dev* dev,
    const struct nvm_geo* geo,
	uint32_t st_ch, uint32_t ch_nr,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat, uint32_t bitmap_blk_nr )
{
    mba_extent = new MetaBlkArea(
        dev, geo,
        MBA_EXTENT_OBJ_SIZE,
        MBA_EXTENT_NAME,
        st_ch, ch_nr,
        st_addr, addr_nr,
        nat,bitmap_blk_nr
    );
}
