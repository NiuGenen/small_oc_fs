#include <stdlib.h>
/*FS's logic*/
///global vars
static mba_mnmg_t mba_region;
static oc_page_pool *pgp;

meta_block_area_t *file_meta_mba;


void _mba_list_list_destroy();
void _mba_init_addr_tbl(meta_block_area_t *mbaptr);
void _mba_init(meta_block_area_t *mbaptr);
void _mba_release(meta_block_area_t *mbaptr);
void _mba_init_bitmaps(meta_block_area_t *mbaptr);
void _mba_get_acblks(meta_block_area_t *mbaptr, int idx);

void mba_mngm_init()
{
	mba_region.mba_list = new list_wrapper();
}

void mba_mngm_init_pgp(oc_page_pool *pgp_)
{
	mba_region.mba_list = new list_wrapper();
	pgp = pgp_;
}

struct list_wrapper_MBA_deleter {
	void operator ()(list_wrapper *lentry)
	{
		meta_block_area_t *mbaptr = list_entry_CTPtr<meta_block_area_t>(lentry);
		if (mbaptr) {
			_mba_release(mbaptr);
			free(mbaptr);
		}
		delete lentry;
	}
};
void _mba_list_list_destroy()
{
	list_wrapper_MBA_deleter del;
	list_traverse<list_wrapper_MBA_deleter>(mba_region.mba_list, &del);
}

void mba_mngm_dump(meta_block_area_t* mbaptr)
{
	//this will dump all the meta_data into SUPERBLOCK.
}

/* 
 *  WARNING: Before release, one should need to dump(mba_mngm_dump) it first.
 *  
 */
void mba_mngm_release()	
{
	_mba_list_list_destroy();
	if (mba_region.mba_list) {
		delete mba_region.mba_list;
	}
}
/* 
 *  
 *  
 */

#define mba_mngm_info_print_blk_addr_tbl
#define mba_mngm_bitmaps_info
void mba_mngm_info()
{
	list_wrapper *ptr;
	printf("----mba mngm----\n");
	for (ptr = mba_region.mba_list->next_; ptr != mba_region.mba_list; ptr = ptr->next_) {
		mba_info(list_entry_CTPtr<meta_block_area_t>(ptr));
#ifdef mba_mngm_info_print_blk_addr_tbl
		mba_pr_blk_addr_tbl(list_entry_CTPtr<meta_block_area_t>(ptr));
#endif
#ifdef mba_mngm_bitmaps_info
		mba_bitmaps_info(list_entry_CTPtr<meta_block_area_t>(ptr));
#endif
	}
}
#define __init_debug__


void _mba_init_addr_tbl(meta_block_area_t *mbaptr)
{
	size_t nluns = addr::bah[0]->geo_->nluns;
	addr::blk_addr *blkas, blkline;
	int dis_ch = mbaptr->ed_ch - mbaptr->st_ch + 1; // mbaptr->blk_count;
	int i, tblidx = 0;
	size_t *idx;
	int mbaidx;
	blkas = new addr::blk_addr [dis_ch];
	idx = new size_t[dis_ch];
#ifdef __init_debug__
	printf("----mba init blks----\n");
#endif

	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch && tblidx < mbaptr->blk_count; i++) 
	{
		mbaidx = i - mbaptr->st_ch;
		idx[mbaidx] = mbaptr->occ_ext[mbaidx].stblk; 
	}

	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch && tblidx < mbaptr->blk_count; i++) 
	{
		mbaidx = i - mbaptr->st_ch;
		if (idx[mbaidx] == mbaptr->occ_ext[mbaidx].stblk) {
			blkas[mbaidx] = addr::bah[i]->get_lowest();
			addr::bah[i]->BlkAddrAdd(idx[mbaidx], &blkas[mbaidx]);
		}
		blkline = addr::bah[i]->get_right_edge_addr(&blkas[mbaidx]);

		while (addr::bah[i]->BlkAddrCmp(&blkas[mbaidx], &blkline) < 0 
			   && idx[mbaidx] <= mbaptr->occ_ext[mbaidx].edblk) {
#ifdef __init_debug__
			if (tblidx == 0) {
				addr::bah[i]->PrBlkAddr(&blkas[mbaidx], true, "");
			} else {
				addr::bah[i]->PrBlkAddr(&blkas[mbaidx], false, ""); 
			}
#endif
			addr::bah[i]->convert_2_nvm_addr(&blkas[mbaidx], mbaptr->blk_addr_tbl + tblidx);

			addr::bah[i]->BlkAddrAdd(1, &blkas[mbaidx]);
			idx[mbaidx]++;
			tblidx++;
		}// while
	}// for

	delete[] idx;
	delete[] blkas;
}
void _mba_init_bitmaps(meta_block_area_t *mbaptr)
{
	int i;
	size_t pg_per_blk = addr::bah[0]->geo_->npages;
	int ac_pg_sum = mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk;
	size_t objs_per_pg = addr::bah[0]->geo_->page_nbytes / mbaptr->obj_size;
	size_t objs_per_blk = objs_per_pg * pg_per_blk;

	//calloc things...
	mbaptr->bitmaps[0] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), 1);	                    //lvl 0 - 1 bit/blk  for all blocks.
	mbaptr->bitmaps[1] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), mbaptr->acblk_counts);	//lvl 1 - 1 bit/page for active blocks.
	mbaptr->bitmaps[2] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), ac_pg_sum);  			//lvl 2 - 1 bit/obj  for active pages.
																							
	mbaptr->bitmaps[3] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), mbaptr->blk_count);      //lvl 4 - 1


	mbaptr->bitmaps[0][0] = new oc_bitmap(mbaptr->blk_count);//lvl 0 - 1 bit/blk  for all blocks.

	for (i = 0; i < mbaptr->acblk_counts; i++) {
		mbaptr->bitmaps[1][i] = new oc_bitmap(pg_per_blk);  //lvl 1 - 1 bit/page for active blocks.
	}
	for (i = 0; i < ac_pg_sum; i++) {
		mbaptr->bitmaps[2][i] = new oc_bitmap(objs_per_pg); //lvl 2 - 1 bit/obj  for active pages.
	}
	for (i = 0; i < mbaptr->blk_count; i++) {
		mbaptr->bitmaps[3][i] = new oc_bitmap(objs_per_blk);	//lvl 4 - 1 bit/obj  for junk bits for all objects.
	}
}

void _mba_release_bitmaps(meta_block_area_t *mbaptr)
{
	int i;
	size_t pg_per_blk = addr::bah[0]->geo_->npages;
	int ac_pg_sum = mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk;
	size_t objs_per_pg = addr::bah[0]->geo_->page_nbytes / mbaptr->obj_size;
	size_t objs_per_blk = objs_per_pg * pg_per_blk;

	delete mbaptr->bitmaps[0][0];
	for (i = 0; i < mbaptr->acblk_counts; i++) {
		delete mbaptr->bitmaps[1][i];  //lvl 1 - 1 bit/page for active blocks.
	}
	for (i = 0; i < ac_pg_sum; i++) {
		delete mbaptr->bitmaps[2][i]; //lvl 2 - 1 bit/obj  for active pages.
	}
	for (i = 0; i < mbaptr->blk_count; i++) {
		delete mbaptr->bitmaps[3][i];	//lvl 4 - 1 bit/obj  for junk bits for all objects.
	}
	for (int i = 0 ; i < MBA_BM_LVLs; i++) {
		free(mbaptr->bitmaps[i]);
	}
}

void _mba_init_buffer_pgp(meta_block_area_t *mbaptr)
{
	mbaptr->buffer = (oc_page **)calloc(sizeof(oc_page*), mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk);
	for (int i = 0; i < mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk; i++) {
		pgp->page_alloc(&mbaptr->buffer[i]);
	}
}
void _mba_release_buffer_pgp(meta_block_area_t *mbaptr)
{
	for (int i = 0; i < mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk; i++) {
		pgp->page_dealloc(mbaptr->buffer[i]); 
	}
	free(mbaptr->buffer);
}


/*
 * this will setup the blocks layout & other stuff.
 */
void _mba_init(meta_block_area_t *mbaptr)
{
	int dis_ch = mbaptr->ed_ch - mbaptr->st_ch + 1; //range: [mbaptr->ed_ch, mbaptr->st_ch]
	int blks = mbaptr->blk_count;
	int each_ch_blk = blks / dis_ch;
	int i, iblk;

	/* "extent" for mba-occupied-blocks per channel. */
	mbaptr->occ_ext = (mba_extent_t *)calloc(sizeof(mba_extent_t), dis_ch);

	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		iblk = i == mbaptr->ed_ch ? blks : each_ch_blk;

		mbaptr->occ_ext[i - mbaptr->st_ch].stblk = addr::next_start[i];
		mbaptr->occ_ext[i - mbaptr->st_ch].edblk = addr::next_start[i] + iblk - 1;

		addr::next_start[i] = addr::next_start[i] + iblk; //need lock ?
		blks -= iblk;
	}

	/* nvm_addr blk_addr_tbl for blocks's quick referencing */
	mbaptr->blk_addr_tbl = (nvm_addr *)calloc(sizeof(nvm_addr), mbaptr->blk_count);
	_mba_init_addr_tbl(mbaptr);


	/* active id array for blks & pgs */
	mbaptr->acblk_id = (int *)calloc(sizeof(int), mbaptr->acblk_counts); 
	mbaptr->pg_id = (int *)calloc(sizeof(int), mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk);// [0, 1 * mbaptr->pg_counts_p_acblk),[,),...,[,)

	/* buffer */
	_mba_init_buffer_pgp(mbaptr);


	/* bitmaps */
	_mba_init_bitmaps(mbaptr);

	//obj addr tbl or oat
	mbaptr->obj_addr_tbl = NULL; //init by user.
}

void _mba_release(meta_block_area_t* mbaptr)
{
	int i,j;
	if (mbaptr->occ_ext) {
		free(mbaptr->occ_ext);
	}
	if (mbaptr->blk_addr_tbl) {
		free(mbaptr->blk_addr_tbl);
	}
	if (mbaptr->acblk_id) {
		free(mbaptr->acblk_id);
	}
	if (mbaptr->pg_id) {
		free(mbaptr->pg_id);
	}
	_mba_release_buffer_pgp(mbaptr);
	_mba_release_bitmaps(mbaptr);
}	

meta_block_area_t*  mba_alloc(const char *name,
	int blkcts, int obj_size, int st_ch, int ed_ch,int obj_num_max)
{
	meta_block_area_t* mba = (meta_block_area_t*)calloc(sizeof(meta_block_area_t), 1);
	mba->mba_name = name;
	mba->blk_count = blkcts;
	mba->obj_size = obj_size;
	mba->st_ch = st_ch;
	mba->ed_ch = ed_ch;
	mba->acblk_counts = MBA_ACTIVE_BLK;
	mba->pg_counts_p_acblk = MBA_ACTIVE_PAGE;
	mba->obj_num_max = obj_num_max;

	list_wrapper *mba_list_node = new list_wrapper(mba);
	list_push_back(mba_region.mba_list, mba_list_node);
	_mba_init(mba);
	return mba;
}

/* 
 *  
 *  
 */
void mba_info(meta_block_area_t* mbaptr)
{
	int i, mbaidx;
	printf("mba: %s\n", mbaptr->mba_name);
	printf("blocks: %d\n", mbaptr->blk_count);
	printf("blks layout:\n");

	printf("       "); 
	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		printf("%4d ", i);
	}
	printf("\n");

	printf("start  ");
	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		mbaidx = i - mbaptr->st_ch;
		printf("%4zu ", mbaptr->occ_ext[mbaidx].stblk); 
	}
	printf("\n");

	printf("ed     "); 
	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		mbaidx = i - mbaptr->st_ch;
		printf("%4zu ", mbaptr->occ_ext[mbaidx].edblk);
	}
	printf("\n");
}

void mba_pr_blk_addr_tbl(meta_block_area_t *mbaptr)
{
	printf("----blk_addr_tbl----\n");
	for (int i = 0; i < mbaptr->blk_count; i++) {
		if (i == 0) {
			addr::addr_nvm_addr_print(mbaptr->blk_addr_tbl + i, true, ""); 
		} else {
			addr::addr_nvm_addr_print(mbaptr->blk_addr_tbl + i, false, ""); 
		}
	}
}

void mba_bitmaps_info(meta_block_area_t *mbaptr)
{
	int bm_counts[4] = {
		1, 
		mbaptr->acblk_counts, 
		mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk,
		mbaptr->blk_count
	};

	for (int i = 0; i < 4; i++) {
		printf("LVL:%d (%d)\n", i, bm_counts[i]);
		for (int j = 0; j < bm_counts[i]; j++) {
			printf("--%d--\n", j);
			mbaptr->bitmaps[i][j]->info();
		}
	}
}

void mba_pr_bitmaps(meta_block_area_t *mbaptr)
{
}

typedef struct obj_idx{
	int acblk_id_idx;
	int pg_id_idx;
	int obj_idx;
} obj_idx_t;
///blk - pg - obj
void _mba_get_acblks_init(meta_block_area_t *mbaptr)
{
	int blkid;
	mbaptr->acblk_id_lock.Lock();
	for (int i = 0; i < mbaptr->acblk_counts; i++) {
		blkid = mbaptr->bitmaps[0][0]->get_slot();		
		mbaptr->acblk_id[i] = blkid;
		mbaptr->bitmaps[0][0]->set_slot(blkid);	//LVL 0

		//mbaptr->bitmaps[1][i]->unset_all();		//LVL 1
		_mba_get_pg_id_from_blk_id_init(mbaptr, i);
	}
	mbaptr->acblk_id_lock.Unlock();
}

void _mba_get_acblks_idx(meta_block_area_t *mbaptr, int idx)
{
	int i, id;
	mbaptr->acblk_id_lock.Lock();
	id = mbaptr->bitmaps[0][0]->get_slot();	//this will always ok.
	mbaptr->acblk_id[idx] = id;
	mbaptr->bitmaps[0][0]->set_slot(id);    //LVL 0
											
	mbaptr->bitmaps[1][idx]->unset_all();   //LVL 1
	_mba_get_pg_id_from_blk_id_init(mbaptr, idx);

	mbaptr->acblk_id_lock.Unlock();
}

void _mba_get_pg_id_from_blk_id_init(meta_block_area_t *mbaptr, int acblkidx)
{
	int i, pg_id;
	mbaptr->pg_id_lock.Lock();
	for (i = acblkidx * mbaptr->pg_counts_p_acblk; 
		  i < (acblkidx + 1) * mbaptr->pg_counts_p_acblk; 
		  i++) {
		pg_id = mbaptr->bitmaps[1][acblkidx]->get_slot();
		mbaptr->pg_id[i] = pg_id;
		mbaptr->bitmaps[1][acblkidx]->set_slot(pg_id);
		mbaptr->bitmaps[2][i]->unset_all(); //LVL 2
	}
	mbaptr->pg_id_lock.Unlock();
}

void _mba_get_pg_id_from_blk_id_idx(meta_block_area_t *mbaptr, int pgidx)
{
	int pg_id;
	int blk_idx = pgidx / mbaptr->pg_counts_p_acblk;
	mbaptr->pg_id_lock.Lock();
	pg_id = mbaptr->bitmaps[1][blk_idx]->get_slot();
	if (pg_id < 0) {
		mbaptr->pg_id_lock.Unlock();
		_mba_get_acblks_idx(mbaptr, blk_idx);
		return ;
	}
	mbaptr->pg_id[pgidx] = pg_id;
	mbaptr->bitmaps[1][blk_idx]->set_slot(pg_id);
	mbaptr->bitmaps[2][pgidx]->unset_all();
	mbaptr->pg_id_lock.Unlock();
}


void _mba_get_obj_id(meta_block_area_t *mbaptr, obj_idx_t *obj_id)
{
	int pg_idx, obj_idx;
	int ac_pg_sum = mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk;
ALLOC_OBJ:
	for (pg_idx = 0 ; pg_idx < ac_pg_sum; pg_idx++) {
		obj_idx = mbaptr->bitmaps[2][pg_idx]->get_slot();
		if (obj_idx >= 0) {
			obj_id->acblk_id_idx = pg_idx / mbaptr->pg_counts_p_acblk; 
			obj_id->pg_id_idx = pg_idx;
			obj_id->obj_idx = obj_idx;
			return;
		}
	}
	//issue an I/O to dump all the active pages

	//get new active pages
	for (pg_idx = 0 ; pg_idx < ac_pg_sum; pg_idx++) {
		_mba_get_pg_id_from_blk_id_idx(mbaptr, pg_idx);
	}
	goto ALLOC_OBJ;
}




//file_meta implementation

void* _file_meta_mba_alloc_oat() throw(oc_excpetion)
{
	file_meta_number_t *oat = (file_meta_number_t *)calloc(sizeof(file_meta_number_t), FILE_META_OBJ_CTS); //<8MB
	if (!(*oat)) {
		throw (oc_excpetion("not enough memory", false));
	}
	return reinterpret_cast<void *>(oat);
}

void file_meta_init()
{	
	file_meta_mba = mba_alloc(FILE_META_NAME, FILE_META_BLK_CTS, FILE_META_OBJ_SIZE,
		0, addr::bah[0]->geo_->nchannels - 1, FILE_META_OBJ_CTS);
	mba_set_oat(file_meta_mba, _file_meta_mba_alloc_oat());
}

file_meta_number_t file_meta_alloc_obj_id()
{
}

void file_meta_write_by_obj_id()
{
}

void file_meta_read_by_obj_id()
{
}

void file_meta_info()
{
}
