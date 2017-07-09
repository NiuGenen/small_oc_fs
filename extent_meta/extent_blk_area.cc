#include "extent_blk_area.h"
#include "../meta_blk_area.h"
#include <liblightnvm.h>

#define MBA_EXTENT_NAME        "MBA_EXTENT"
#define MBA_EXTENT_OBJ_SIZE    4096

//global var
MetaBlkArea *mba_extent;

void extent_blk_area_init(
	struct nvm_dev* dev,
    const struct nvm_geo* geo,
	int st_ch, int ed_ch,
	struct blk_addr* st_addr, size_t addr_nr,
	struct nar_table* nat, size_t nat_max_length )
{
    mba_extent = new MetaBlkArea(
        dev, geo,
        MBA_EXTENT_OBJ_SIZE,
        MBA_EXTENT_NAME,
        st_ch, ed_ch,
        st_addr, addr_nr,
        nat, nat_max_length
    );
}