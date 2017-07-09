#ifndef _META_BLK_AREA_H_
#define _META_BLK_AREA_H_

#include "blk_addr.h"	// need one blk_addr_handle to handle meta data blk
//#include "lnvm/liblightnvm.h"

// NAT table for obj addressing

typedef uint64_t Nat_Obj_ID_Type
typedef uint32_t Nat_Obj_Addr_Type

struct nat_entry{
	Nat_Obj_ID_Type obj_id;
	//Nat_Obj_Addr_Type obj_address;	// blk_idx page obj
	uint16_t blk_idx;
	uint8_t page;
	uint8_t obj;
	uint16_t is_used;
	uint16_t is_dead;
};

struct nat_table{
	struct nat_entry* entry;
	uint64_t max_length;
};

// meta blk area
class MetaBlkArea{
public:
	MetaBlkArea(
		int obj_size,
		const char* mba_name,
		int st_ch, int ed_ch,
		struct blk_addr* st_addr, struct blk_addr* ed_addr,
		struct nar_table* nat
	);
	~MetaBlkArea();

	Nat_Obj_ID_Type alloc_obj();
	void de_alloc_obj(Nat_Obj_ID_Type obj_id);
	void  write_by_obj_id(Nat_Obj_Addr_Type obj_id, void* obj);
	void* read_by_obj_id(Nat_Obj_ID_Type obj_id);
	int if_nat_need_flush();
	void after_nat_flush();

private:
	int obj_size;	// 1KB or 4KB
	int obj_nr_per_page;
	const char* mba_name;

	struct blk_addr* meta_blk_addr;
	size_t meta_blk_addr_size;

	struct nar_table* nat;
	size_t nat_max_length;
	size_t nat_dead_nr;
	int nat_need_flush;

	void** obj_cache;
	char *buf;

	Nat_Obj_Addr_Type find_nat_addr_by_obj_id( Nat_Obj_ID_Type obj_id );
	struct nvm_addr* convert_nat_addr_to_nvm_addr(Nat_Obj_Addr_Type nat_addr);
};

#endif
