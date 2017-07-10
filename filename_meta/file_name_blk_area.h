#ifndef _FILE_NAME_BLK_AREA_H_
#define _FILE_NAME_BLK_AREA_H_

#include "../meta_blk_area.h"
#include "../blk_addr.h"
#include <liblightnvm.h>
#include <stdint.h>
#include <stddef.h>

void file_name_blk_area_init(
	struct nvm_dev* dev,
	const struct nvm_geo* geo,
	uint32_t st_ch, uint32_t ed_ch,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat
);

#endif
