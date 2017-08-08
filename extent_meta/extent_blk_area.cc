#include "extent_blk_area.h"
#include "extent_list.h"

#define MBA_EXTENT_NAME        "MBA_EXTENT"
#define MBA_EXTENT_OBJ_SIZE    4096

extern BlkAddrHandle* ocssd_bah;
//global var
MetaBlkArea *mba_extent;
ExtentList** extentList;

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

void extent_tree_init(
		size_t nchs,
		Nat_Obj_ID_Type* root_id,
		uint32_t * extent_st_blk_idx)
{
	extentList = new ExtentList*[ nchs ];
    struct blk_addr blk_addr_;
	for(int ch = 0; ch < nchs; ++ch){
		blk_addr_handle* bah = ocssd_bah->get_blk_addr_handle( ch );
		bah->MakeBlkAddr( ch, 0, 0 ,0, &blk_addr_ );
		bah->BlkAddrAdd(extent_st_blk_idx[ch], &blk_addr_);
		printf(" - - - - - - - - build extenr tree for ch %d , st_blk = %u\n", ch, extent_st_blk_idx[ch]);
		extentList[ch] = new ExtentList(
				root_id[ch],
				ch,
				blk_addr_.__buf, bah->get_highest().__buf,
				sizeof(struct ext_node),
				bah
		);
	}
}
