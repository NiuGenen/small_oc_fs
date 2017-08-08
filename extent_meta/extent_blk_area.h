#ifndef _EXTENT_BLK_AREA_H_
#define _EXTENT_BLK_AREA_H_

#include "../meta_blk_area.h"
#include "../blk_addr.h"
#include "../liblightnvm.h"

void extent_blk_area_init(
	struct nvm_dev* dev,
	const struct nvm_geo* geo,
	uint32_t st_ch, uint32_t ch_nr,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat, uint32_t bitmap_blk_nr
);

void extent_tree_init(
		size_t nchs,
		Nat_Obj_ID_Type* root_id,
		uint32_t * extent_st_blk_idx
);

#endif
