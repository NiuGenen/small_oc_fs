#ifndef _EXTENT_TREE_H_
#define _EXTENT_TREE_H_

#include "extent_stub.h"
#include "extent_blkhandle.h"
#include "extent_type.h"

// 24 B
// extent stored in eobj( extent_tree_node )
struct extent{
    uint64_t addr_st_buf;
    uint64_t addr_ed_buf;
    uint32_t free_bitmap;
    uint32_t junk_bitmap;
};

#define Ext_Nat_Entry_ID_Type   uint16_t
#define Ext_Node_ID_Type        Ext_Nat_Entry_ID_Type

// 8B
// sorting the extent in @free_vblk_num
struct ext_meta_obj{
    uint32_t free_vblk_num;
    uint16_t index;   // uint16_t
    uint16_t reserve;
};

#define Ext_Leaf_Node_Degree            127
#define Ext_Non_Leaf_Node_Degree        145
#define Ext_Non_Leaf_Node_Degree_Meta   ( 3 * Ext_Non_Leaf_Node_Degree )
//MUST be { 3 * Ext_Non_Leaf_Node_Degree }

// extent_tree_leaf_node
// node_type == 0
struct ext_leaf_node{
	uint32_t node_type;             // 4 B

	uint16_t el_2_mobj_num;
	uint16_t el_8_mobj_num;
	uint16_t el_4_mobj_num;
	uint16_t el_none_mobj_num;      // 12 B

	uint32_t free_vblk_sum_2;
	uint32_t free_vblk_sum_4;
	uint32_t free_vblk_sum_8;
	uint32_t free_vblk_sum_none;    // 16 B                                       +   12 B =   28 B

	struct ext_meta_obj mobjs[Ext_Leaf_Node_Degree];    // 127 * 8 B  = 1016 B    +   28 B = 1044 B
	struct extent exts[Ext_Leaf_Node_Degree];           // 127 * 24 B = 3048 B    + 1044 B = 4092 B
	
	Ext_Node_ID_Type father_id;     // uint16_t
    uint16_t reserve;               // 4 B                                        + 4092 B = 4096 B
};

// extent_tree_none_leaf_node
// node_type == 1
struct ext_non_leaf_node{
	uint32_t node_type;             // 4 B

	uint16_t el_2_mobj_num;
	uint16_t el_8_mobj_num;
	uint16_t el_4_mobj_num;
	uint16_t el_none_mobj_num;      // 8 B

	uint32_t free_vblk_sum_2;
	uint32_t free_vblk_sum_4;
	uint32_t free_vblk_sum_8;
	uint32_t free_vblk_sum_none;    // 16 B                                               +    8 B =   24 B

	struct ext_meta_obj mobjs[Ext_Non_Leaf_Node_Degree_Meta];   // 435 * 8 B = 3480 B     +   24 B = 3504 B
	Ext_Node_ID_Type node_id_array[Ext_Non_Leaf_Node_Degree];   // 145 * 2 B =  290 B     + 3504 B = 3794 B
	
	uint32_t father_id;     // 
	uint32_t reserve;       // 
};

// extent_descriptor
// 20 B
// stored in file_meta_obj ( 1 KB / 20 B = 51 ... 4 )
struct ext_descriptor{
	uint64_t addr_st_buf;
	uint64_t addr_ed_buf;
	uint32_t use_bitmap;
};

#define SIZE_EXT_DES (sizeof( struct ext_descriptor ))

class ExtentTree{
public:
	ExtentTree(
		int ch,
		uint64_t blk_st, uint64_t blk_ed,
		int ext_size,
		blk_addr_handle handler);
	~ExtentTree();

	void init();

	int getChannel(){ return ch; }
	int getExtSize(){ return ext_size; }

	struct extent_descriptor* getExt();
	void putExt(struct extent_descriptor* edes);
private:
	int ch;
	int ext_size;		// 2,4,8
	uint64_t blk_st;
	uint64_t blk_ed;
	uint64_t blk_nr;
	blk_addr_handle handler;	// to handle blk_addr
	Ext_Node_ID_Type root_ext_node_id;
}

#endif
