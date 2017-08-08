#ifndef _META_BLK_AREA_H_
#define _META_BLK_AREA_H_

#include <stddef.h>
#include <iostream>
#include "blk_addr.h"	// need one blk_addr_handle to handle meta data blk
#include "liblightnvm.h"

// NAT table for obj addressing

typedef uint64_t Nat_Obj_ID_Type;
typedef uint32_t Nat_Obj_Addr_Type;

struct nat_entry{
	Nat_Obj_ID_Type obj_id;
	//Nat_Obj_Addr_Type obj_address;	// blk_idx page obj
	uint32_t blk_idx;	// > 0 ; [0] for bitmap
	uint16_t page;
	uint8_t obj;
	uint8_t state;
};//	8 + 4 + 2 + 1 + 1 = 16

#define NAT_ENTRY_FREE 0	// free to alloc
#define NAT_ENTRY_USED 1	// after alloc, NO obj address
#define NAT_ENTRY_WRIT 2	// after write, HAVE obj address

struct nat_table{
	struct nat_entry* entry;
	uint64_t max_length;
};

#define MBA_MAP_STATE_EMPT 0 // all is free to use
#define MBA_MAP_STATE_PART 1 // part is used & part is free
#define MBA_MAP_STATE_FULL 3 // all is in use
#define MBA_MAP_OBJ_STATE_FREE 0
#define MBA_MAP_OBJ_STATE_USED 1
#define MBA_MAP_OBJ_STATE_DEAD 3

// meta blk area
class MetaBlkArea{
public:
	MetaBlkArea(
		struct nvm_dev* dev,
    	const struct nvm_geo* geo,
		size_t obj_size,
		const char* mba_name,
		uint32_t st_ch, uint32_t ed_ch,
		struct blk_addr* st_addr, size_t* addr_nr,
		struct nat_table* nat, uint32_t bitmap_blk_nr
	);
	~MetaBlkArea();

	Nat_Obj_ID_Type alloc_obj();
	void de_alloc_obj(Nat_Obj_ID_Type obj_id);
	void write_by_obj_id(Nat_Obj_ID_Type obj_id, void* obj);
	void* read_by_obj_id(Nat_Obj_ID_Type obj_id);

	int need_GC();
	void GC();

	void print_bitmap();
	void print_nat();

	void flush();
private:
	struct nvm_dev* dev;
   	const struct nvm_geo* geo;

	size_t obj_size;	// 1KB or 4KB
	size_t obj_nr_per_page;
	size_t obj_nr_per_blk;
	const char* mba_name;

	struct blk_addr* meta_blk_addr;
	size_t meta_blk_nr;
	size_t bitmap_blk_nr;
	size_t data_blk_nr;

	struct nvm_vblk* bitmap_vblk;
	uint8_t* map_buf;
	size_t map_buf_size;
	uint8_t* blk_map;
	uint8_t** blk_page_map;
	uint8_t*** blk_page_obj_map;	// bit map
	void set_obj_state(uint32_t blk, uint16_t pg, uint8_t obj, uint8_t state);
    size_t* blk_dead_obj_nr;	// per block
	size_t all_dead_obj_nr;		// total dead obj
	size_t gc_threshold;

	uint32_t blk_act;
	uint16_t page_act;
	uint8_t obj_act;
	void act_obj_set_state( uint8_t state );	// update bitmap with current act
	void find_next_act_blk( uint32_t start_blk_idx );
	void act_addr_add(size_t n);

	struct nat_table* nat;
	size_t nat_max_length;

	void *w_buf;
	void *r_buf;

	void flush_bitmap();
	void flush_data();

	std::string txt();
};

#endif
