#ifndef _EXTENT_BLK_AREA_H_
#define _EXTENT_BLK_AREA_H_

#include "../meta_blk_area.h"
#include "../blk_addr.h"
#include <liblightnvm.h>

void extent_blk_area_init(
	struct nvm_dev* dev,
	const struct nvm_geo* geo,
	int st_ch, int ed_ch,
	struct blk_addr* st_addr, size_t addr_nr,
	struct nat_table* nat, size_t nat_max_length
);

#endif