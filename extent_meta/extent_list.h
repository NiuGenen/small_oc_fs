#ifndef _EXTENT_LIST_H_
#define _EXTENT_LIST_H_

//#include "extent_blkhandle.h"
//#include "extent_type.h"
//#include "../meta_blk_area.h"

#include "extent_blk_area.h"

// 24 B
// extent stored in eobj
struct extent{
    uint64_t addr_st_buf;
    uint64_t addr_ed_buf;
    uint32_t free_bitmap;
    uint32_t junk_bitmap;
};

// 8B
struct ext_meta_obj{
    uint32_t free_vblk_num;
};

#define Ext_Node_Half_Degree 64
#define Ext_Node_Degree ( Ext_Node_Half_Degree * 2 )

// 4 KB
struct ext_node{
	Nat_Obj_ID_Type obj_id;
	uint16_t ecount;
	//uint16_t first_avail_index;

	struct ext_meta_obj mobjs[ Ext_Node_Degree ];
	struct extent exts[ Ext_Node_Degree ];

    Nat_Obj_ID_Type prev;
    Nat_Obj_ID_Type next;
};

// extent_descriptor
// 24 B
// stored in file_meta_obj ( 1 KB / 24 B = 42 ... 4 )
struct extent_descriptor{
	uint64_t addr_st_buf;
	uint64_t addr_ed_buf;
	uint32_t use_bitmap;
	uint32_t junk_bitmap;
};

#define EXT_DES_SIZE (sizeof( struct extent_descriptor ))

class ExtentList{
public:
    ExtentList(Nat_Obj_ID_Type root_id,
        int ch,
        uint64_t blk_st, uint64_t blk_ed,
        int ext_size,
        blk_addr_handle* handler);
    ~ExtentList();

	void init(Nat_Obj_ID_Type root_id);

	int getChannel(){ return ch; }
	int getExtSize(){ return ext_size; }

    struct extent_descriptor* getNewExt();
    void putExt(struct extent_descriptor* edes);
    void GC();
    void display();
private:
	int ch;
	int ext_size;		// 2,4,8
	uint64_t blk_st;
	uint64_t blk_ed;
	uint64_t blk_nr;
    Nat_Obj_ID_Type  head_eobj_id;
    blk_addr_handle* handler;
};

#endif
