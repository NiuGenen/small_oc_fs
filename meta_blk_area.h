#ifndef YWJ_OCSSD_OC_TREE_H
#define YWJ_OCSSD_OC_TREE_H

inline struct blk_addr blk_addr_handle::get_lowest()
{
	return lowest;
}

extern addr_meta *am;
extern blk_addr_handle **bah;
extern size_t *next_start;
void addr_init(const nvm_geo *g) throw(ocssd::oc_excpetion);
void addr_release();
void addr_info();
void addr_next_addr_info();
void addr_nvm_addr_print(nvm_addr *naddr, bool pr_title, const char *prefix = "");
} // namespace addr




/* fs-logic */
//mba mngm
typedef struct mba_extent {
	size_t stblk;
	size_t edblk;
}mba_extent_t;

#define MBA_BM_LVLs 4
#define MBA_ACTIVE_BLK 4
#define MBA_ACTIVE_PAGE 1
typedef struct meta_block_area {
	const char *mba_name;
	int blk_count;
	int obj_size;
	int st_ch;                  //
	int ed_ch;

	mba_extent_t *occ_ext;      //each channel's blocks, length = [st_ch, ed_ch]
	nvm_addr *blk_addr_tbl;     //for real-time I/O's quick reference
	int *st_pg_id;				//for gc use

	int acblk_counts;           //active block
	int *acblk_id;
	rocksdb::port::Mutex acblk_id_lock;

	int pg_counts_p_acblk;      //active page per active block
	int *pg_id;

	oc_page **buffer; 			//

	rocksdb::port::Mutex pg_id_lock;

	oc_bitmap **bitmaps[MBA_BM_LVLs];   //one of <@counts>-bits-bitmap
										//<@acblk_counts> of <@geo->npages>-bits-bitmap
										//<acpg_counts_per_acblk * acblk_counts> of <pg_size/obj_size>-bitmap

										//<counts> of <blk_size/obj_size>-bits-bitmap

	
	void *obj_addr_tbl;                 //object address table or "oat"	(NAT)
	int obj_num_max;
}meta_block_area_t;

typedef struct mba_mnmg {
	list_wrapper *mba_list;
}mba_mnmg_t; 

void mba_mngm_init();
void mba_mngm_init_pgp(oc_page_pool *pgp_);
void mba_mngm_dump(meta_block_area_t *mbaptr);
void mba_mngm_release();
void mba_mngm_info();
meta_block_area_t*  mba_alloc(const char *name, int blkcts, int obj_size, int st_ch, int ed_ch, int obj_num_max);
void mba_info(meta_block_area_t *mbaptr); 

void mba_pr_blk_addr_tbl(meta_block_area_t *mbaptr);

void mba_bitmaps_info(meta_block_area_t *mbaptr);
void mba_pr_bitmaps(meta_block_area_t *mbaptr);

inline void mba_set_oat(meta_block_area_t *mbaptr, void *oat)
{
	mbaptr->obj_addr_tbl = oat;
}

//file_meta implementation
typedef uint32_t file_meta_number_t;
#define FILE_META_NAME "FileMeta"
#define FILE_META_BLK_CTS 300
#define FILE_META_OBJ_SIZE 1024
#define FILE_META_OBJ_CTS 2000000
extern meta_block_area_t *file_meta_mba;

inline file_meta_number_t* _file_meta_mba_get_oat()
{
	return reinterpret_cast<file_meta_number_t *>(file_meta_mba->obj_addr_tbl);
}

struct nat_entry{
	Node_ID_Type id;
	uint8_t dead;
	uint8_t reserve[3];
}__attribute__((aligned(8)));

#define Nat_Node_Degree 511
struct nat_node{
	struct nat_entry entries[Nat_Node_Degree];
	Nat_Node_ID_Type next_nat_node_id;
}__attribute__((aligned(8)));;

#define Nat_Entry_ID_Type uint32_t

#endif
