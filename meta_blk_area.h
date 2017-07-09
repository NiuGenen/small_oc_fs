#ifndef _META_BLK_AREA_H_
#define _META_BLK_AREA_H_

#include "blk_addr.h"	// need one blk_addr_handle to handle meta data blk
//#include "lnvm/liblightnvm.h"

// NAT table for obj addressing

typedef uint64_t Nat_Obj_ID_Type
typedef uint32_t Nat_Obj_Addr_Type

struct nat_entry{
	Nat_Obj_ID_Type obj_id;
	Nat_Obj_Addr_Type obj_address;	// blk_idx page obj
	uint16_t is_used;
};

struct nat_table{
	struct nat_entry* table;
	uint64_t max_length;
};

// meta blk area
class MetaBlkArea{
public:
	MetaBlkArea(
		int obj_size;
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
private:
	int obj_size_;	// 1KB or 4KB
	const char* mba_name_;
	int st_ch_;
	int ed_ch_;
	int ch_nr_;
	struct blk_addr* st_addr_;
	struct blk_addr* ed_addr_;
	blk_addr_handle* blk_addr_handler;
	struct nar_table* nat_;

	Nat_Obj_Addr_Type find_nat_addr_by_obj_id( Nat_Obj_ID_Type obj_id );
	struct nvm_addr* convert_nat_addr_to_nvm_addr(Nat_Obj_Addr_Type nat_addr);
};

#endif
