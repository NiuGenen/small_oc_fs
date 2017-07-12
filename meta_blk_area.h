#ifndef _META_BLK_AREA_H_
#define _META_BLK_AREA_H_

#include <stddef.h>
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

#define NAT_ENTRY_FREE	0
#define NAT_ENTRY_USED	1
#define NAT_ENTRY_DEAD	2
#define NAT_ENTRY_MOVE	3

struct nat_table{
	struct nat_entry* entry;
	uint64_t max_length;
};

#define MBA_MAP_STATE_FREE 0
#define MBA_MAP_STATE_USED 1 
#define MBA_MAP_STATE_FULL 2 // for bit map to alloc new obj

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
		struct nat_table* nat
	);
	~MetaBlkArea();

	Nat_Obj_ID_Type alloc_obj();
	void de_alloc_obj(Nat_Obj_ID_Type obj_id);
	void write_by_obj_id(Nat_Obj_ID_Type obj_id, void* obj);
	void* read_by_obj_id(Nat_Obj_ID_Type obj_id);
	int if_nat_need_flush();
	void after_nat_flush();
	int need_GC();
	void GC();

private:
	struct nvm_dev* dev;
   	const struct nvm_geo* geo;

	size_t obj_size;	// 1KB or 4KB
	size_t obj_nr_per_page;
	size_t obj_nr_per_blk;
	const char* mba_name;

	struct blk_addr* meta_blk_addr;
	size_t meta_blk_addr_size;

	char* map_buf;
	size_t map_buf_size;
	uint8_t* blk_map;
	uint8_t** blk_page_map;
	uint8_t*** blk_page_obj_map;	// bit map

	size_t blk_act;
	size_t page_act;
	size_t obj_act;
	void act_addr_set_state( uint8_t state );	// update bitmap with current act
	void find_next_act_blk( size_t start_blk_idx );
	void act_addr_add(size_t n);

	struct nat_table* nat;
	size_t nat_max_length;
	size_t nat_dead_nr;
	int nat_need_flush;

	void** obj_cache;
	char *buf;

	Nat_Obj_Addr_Type find_nat_addr_by_obj_id( Nat_Obj_ID_Type obj_id );
	struct nvm_addr* convert_nat_addr_to_nvm_addr(Nat_Obj_Addr_Type nat_addr);
};

#endif
