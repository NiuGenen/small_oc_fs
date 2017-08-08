#ifndef _FILE_NAME_BLK_AREA_H_
#define _FILE_NAME_BLK_AREA_H_

#include "../meta_blk_area.h"
#include "../blk_addr.h"
#include "../liblightnvm.h"
#include <stdint.h>
#include <stddef.h>

void file_name_blk_area_init(
	struct nvm_dev* dev,
	const struct nvm_geo* geo,
	uint32_t st_ch, uint32_t ch_nr,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat, uint32_t bitmap_blk_nr
);

void file_name_btree_init(
		Nat_Obj_ID_Type root_id
);

#endif
