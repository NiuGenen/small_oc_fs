#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <liblightnvm.h>
#include "meta_blk_area.h"
#include "dbg_info.h"
#include "blk_addr.h"
#include "liblightnvm.h"

extern BlkAddrHandle* ocssd_bah;

MetaBlkArea::MetaBlkArea(   // first block to store bitmap
    struct nvm_dev * dev,
    const struct nvm_geo* geo,
	size_t obj_size,
	const char* mba_name,
	uint32_t st_ch, uint32_t ch_nr,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat, uint32_t bitmap_blk_nr)
{
    OCSSD_DBG_INFO( this, "build MetaBlkArea : " << mba_name );

    size_t nat_max_length   =   nat->max_length;
    OCSSD_DBG_INFO( this, " - nat max length = " << nat->max_length);

    this->dev               =   dev;
    this->geo               =   geo;

    this->obj_size          =   obj_size;
    this->obj_nr_per_page   =   geo->page_nbytes / obj_size ;
    this->obj_nr_per_blk    =   obj_nr_per_page * geo->npages;
    this->mba_name          =   mba_name;
    OCSSD_DBG_INFO( this, " - obj size = " << this->obj_size );
    OCSSD_DBG_INFO( this, " - obj per page = " << this->obj_nr_per_page );
    
    OCSSD_DBG_INFO( this, " - build meta block address " );
    this->meta_blk_nr = 0;
    for(uint32_t ch = st_ch, count = 0; count < ch_nr; ++count){
        this->meta_blk_nr += addr_nr[ ch - st_ch ];
    }
    this->meta_blk_addr = new struct blk_addr[ meta_blk_nr ];  // all blk_addr of this meta area
    OCSSD_DBG_INFO( this, " - - meta blk nr = " << meta_blk_nr );
    size_t idx = 0;
    for(uint32_t ch = st_ch, count = 0; count < ch_nr; ++count){
        blk_addr_handle* bah_ch_ = ocssd_bah->get_blk_addr_handle( ch );
        for(size_t i = 0; i < addr_nr[ count ]; ++i){
            meta_blk_addr[ idx ] = st_addr[ ch-st_ch ];
            bah_ch_->BlkAddrAdd( i, &(meta_blk_addr[ idx ]) );
            idx += 1;
        }
    }

    // read bitmap from SSD & build blk_map[],blk_page_map[],blk_page_obj_map[]
    this->bitmap_blk_nr = bitmap_blk_nr;
    this->data_blk_nr   = meta_blk_nr - bitmap_blk_nr;
    OCSSD_DBG_INFO( this, " - bitmap blk nr = " << this->bitmap_blk_nr );
    OCSSD_DBG_INFO( this, " - data   blk nr = " << this->data_blk_nr );

    struct nvm_addr* bitmap_blk_nvm_addr = new struct nvm_addr[ this->bitmap_blk_nr ];
    for(size_t i = 0; i < this->bitmap_blk_nr; ++i)
       bitmap_blk_nvm_addr[i].ppa = 0;
    blk_addr_handle* bah_ch_ = ocssd_bah->get_blk_addr_handle( st_ch );
    for(size_t i = 0; i < this->bitmap_blk_nr; ++i){
        bah_ch_->convert_2_nvm_addr( &(meta_blk_addr[i]), &(bitmap_blk_nvm_addr[i]) );
        OCSSD_DBG_INFO( this, " - - bitmap blk nvm_addr : idx === " << i  );
        OCSSD_DBG_INFO( this, " - - bitmap blk nvm_addr : ch    = " << bitmap_blk_nvm_addr[i].g.ch  );
        OCSSD_DBG_INFO( this, " - - bitmap blk nvm_addr : lun   = " << bitmap_blk_nvm_addr[i].g.lun );
        OCSSD_DBG_INFO( this, " - - bitmap blk nvm_addr : plane = " << bitmap_blk_nvm_addr[i].g.pl  );
        OCSSD_DBG_INFO( this, " - - bitmap blk nvm_addr : block = " << bitmap_blk_nvm_addr[i].g.blk );
    }
    this->bitmap_vblk = nvm_vblk_alloc( dev, bitmap_blk_nvm_addr, (int) this->bitmap_blk_nr );
    
    this->map_buf_size = geo->npages * geo->page_nbytes; // one block size
    this->map_buf = (uint8_t* )malloc( map_buf_size );
    nvm_vblk_read( this->bitmap_vblk, map_buf, map_buf_size );    // read bitmap
    OCSSD_DBG_INFO( this, " - bitmap buf size = " << this->map_buf_size );
    free( bitmap_blk_nvm_addr );

    size_t map_buf_idx = 0;

    OCSSD_DBG_INFO( this, " - build blk map");
    this->blk_map = new uint8_t[ data_blk_nr ];    // alloc blk_map[ 1, size ]
    for(size_t i = 0; i < data_blk_nr; ++i){
        this->blk_map[i] = map_buf[ map_buf_idx++ ];
    }

    OCSSD_DBG_INFO( this, " - build blk page map");
    this->blk_page_map = new uint8_t*[ data_blk_nr ]; // alloc blk_page_map
    for(size_t i = 0; i < data_blk_nr; ++i){
        this->blk_page_map[i] = new uint8_t[ geo->npages ];
        for(size_t j = 0; j < geo->npages; ++j){
            this->blk_page_map[i][j] = map_buf[ map_buf_idx++ ];
        }
    }

    OCSSD_DBG_INFO( this, " - build blk page obj map");
    this->blk_page_obj_map = new uint8_t**[ data_blk_nr ]; // alloc blk_page_obj_map
    for(size_t i = 0; i < data_blk_nr; ++i){
        this->blk_page_obj_map[i] = new uint8_t*[ geo->npages ];
        for(size_t j = 0; j < geo->npages; ++j){
            this->blk_page_obj_map[i][j] = new uint8_t[ obj_nr_per_page ];
            for(size_t k = 0; k < obj_nr_per_page; ++k){
                this->blk_page_obj_map[i][j][k] = map_buf[ map_buf_idx++ ];
            }
        }
    }

    // init dead obj nr
    this->blk_dead_obj_nr = new size_t[ this->data_blk_nr ];
    for(size_t i = 0; i < this->data_blk_nr; ++i )
        this->blk_dead_obj_nr[ i ] = 0;
    this->all_dead_obj_nr = 0;
    this->gc_threshold = data_blk_nr * obj_nr_per_blk / 2;

    // active blk & page & obj
    this->blk_act  = 0;
    this->page_act = 0;
    this->obj_act  = 0;
    find_next_act_blk( blk_act );
    OCSSD_DBG_INFO( this, " - next active blk = " << this->blk_act );
    OCSSD_DBG_INFO( this, " - - page = " << (uint32_t)this->page_act );
    OCSSD_DBG_INFO( this, " - - obj  = " << (uint32_t)this->obj_act );

    // nat table
    this->nat               =   nat;
    this->nat_max_length    =   nat_max_length;

    this->w_buf = malloc( geo->page_nbytes );
    this->r_buf = malloc( geo->page_nbytes );
}

MetaBlkArea::~MetaBlkArea()
{
    if( meta_blk_addr != nullptr ) free( meta_blk_addr );
    nvm_vblk_free( bitmap_vblk );
    if( map_buf != nullptr ) free( map_buf );
    if( blk_map != nullptr ) free( blk_map );
    if( blk_page_map != nullptr ) free( blk_page_map );
    if( blk_page_obj_map != nullptr ) free( blk_page_obj_map );
    if( blk_dead_obj_nr != nullptr ) free( blk_dead_obj_nr );
    if( w_buf != nullptr ) free( w_buf );
    if( r_buf != nullptr ) free( r_buf );
}

void MetaBlkArea::find_next_act_blk( uint32_t last_blk_idx )
{
    uint32_t blk_idx = ( last_blk_idx + 1 ) % (uint32_t ) data_blk_nr;

    size_t count = 0;
    while( count < data_blk_nr ){
        if( blk_map[blk_idx] != MBA_MAP_STATE_FULL ){
            blk_act = blk_idx;
            uint16_t page_idx = 0;
            for(page_idx = 0; page_idx < geo->npages; ++page_idx ){
                if( blk_page_map[ blk_act ][ page_idx ] != MBA_MAP_STATE_EMPT ) {
                    page_act = page_idx;
                    obj_act = 0;
                }
//                if( blk_page_map[ blk_act ][ page_idx ] != MBA_MAP_STATE_FULL ){
//                    page_act = page_idx;
//                    uint8_t obj_idx = 0;
//                    for( obj_idx = 0; obj_idx < obj_nr_per_page; ++obj_idx ){
//                        if( blk_page_obj_map[ blk_act ][ page_act ][ obj_idx ] == MBA_MAP_OBJ_STATE_FREE )
//                        {
//                            obj_act = obj_idx;
//                            return;
//                        }
//                    }
//                }
            }
        }
        blk_idx = ( blk_idx + 1 ) % (uint32_t ) data_blk_nr;
        count += 1;
    }
}

Nat_Obj_ID_Type MetaBlkArea::alloc_obj()
{
    for(size_t i = 1; i < nat_max_length; ++i ){
        if( nat->entry[i].state == NAT_ENTRY_FREE ){
            nat->entry[i].state    = NAT_ENTRY_USED;
            return i;
        }
    }
}

void MetaBlkArea::de_alloc_obj(Nat_Obj_ID_Type obj_id) {
    if (obj_id <= 0 || obj_id >= nat_max_length) return;
    if (nat->entry[obj_id].state == NAT_ENTRY_WRIT){
        set_obj_state(
                nat->entry[obj_id].blk_idx,
                nat->entry[obj_id].page,
                nat->entry[obj_id].obj,
                MBA_MAP_OBJ_STATE_DEAD);
        // GC counter
        this->all_dead_obj_nr += 1;
        this->blk_dead_obj_nr[ nat->entry[ obj_id ].blk_idx ] += 1;
    }
    nat->entry[ obj_id ].state = NAT_ENTRY_FREE;
}

void MetaBlkArea::set_obj_state(uint32_t blk, uint16_t pg, uint8_t obj, uint8_t state){
    blk_page_obj_map[ blk ][ pg ][ obj ] = state; // update obj

//#define MBA_MAP_STATE_EMPT 0
//#define MBA_MAP_STATE_USED 1
//#define MBA_MAP_STATE_FULL 3

    uint8_t s_and = blk_page_obj_map[ blk ][ pg ][ 0 ];
    uint8_t s_or  = blk_page_obj_map[ blk ][ pg ][ 0 ];
    for(size_t i = 0; i < obj_nr_per_page; ++i){    // obj = 00(FREE), 01(USED), 11(DEAD)
        // all 0     :  s_and = 00 , s_or = 00     EMPT
        // all 1     :  s_and = 01 , s_or = 01         FULL
        // all 3     :  s_and = 11 , s_or = 11         FULL
        // 1 & 0     :  s_and = 00 , s_or = 01       PART
        // 3 & 0     :  s_and = 00 , s_or = 11       PART
        // 1 & 3     :  s_and = 01 , s_or = 11         FULL
        // 1 & 3 & 0 :  s_and = 00 , s_or = 11       PART
        s_and = s_and & blk_page_obj_map[ blk ][ pg ][ i ];
        s_or  = s_or  | blk_page_obj_map[ blk ][ pg ][ i ];
    }
    if( s_and & 1 ){
        blk_page_map[ blk ][ pg ] = MBA_MAP_STATE_FULL;   // update page
    }
    else if( s_or & 1 ){
        blk_page_map[ blk ][ pg ] = MBA_MAP_STATE_PART;
    }

    s_and = blk_page_map[ blk ][ 0 ];
    s_or  = blk_page_map[ blk ][ 0 ];
    for(size_t i = 0; i < geo->npages; ++i){ // page = 0(EMPR), 1(PART), 3(FULL)
        // all 0     :  s_and = 00 , s_or = 00     EMPT
        // all 1     :  s_and = 01 , s_or = 01       PART
        // all 3     :  s_and = 11 , s_or = 11         FULL
        // 1 & 0     :  s_and = 00 , s_or = 01       PART
        // 3 & 0     :  s_and = 00 , s_or = 11       PART
        // 1 & 3     :  s_and = 01 , s_or = 11       PART
        // 1 & 3 & 0 :  s_and = 00 , s_or = 11       PART
        s_and = s_and & blk_page_map[ blk ][ i ];
        s_or  = s_or  | blk_page_map[ blk ][ i ];
    }
    if( s_and & s_or & MBA_MAP_STATE_FULL ){
        blk_map[ blk ] = MBA_MAP_STATE_FULL;        // update blk
    }
    else if( s_and | s_or ) {
        blk_map[ blk ] = MBA_MAP_STATE_PART;        // update blk
    }
}

void MetaBlkArea::act_obj_set_state( uint8_t state )	// update bitmap with current act
{
    this->set_obj_state(blk_act, page_act, obj_act, state);
}

void MetaBlkArea::act_addr_add(size_t n){
    while( n > 0 ){
        obj_act += 1;   // obj ++
        if( obj_act >= obj_nr_per_page ){
            obj_act = 0;
            page_act += 1;  // page ++
            if( page_act >= geo->npages ){
                page_act = 0;
                find_next_act_blk( blk_act );   // blk ++
            }
        }
        n -= 1;
    }
}

int MetaBlkArea::need_GC()
{
    if( this->all_dead_obj_nr > gc_threshold ){
        return 1;
    }
    return 0;
}

void MetaBlkArea::GC()
{
    OCSSD_DBG_INFO( this, " GC start!");
    OCSSD_DBG_INFO( this, " - all dead obj nr = " << this->all_dead_obj_nr );
    for(size_t i = 0; i < this->data_blk_nr; ++i){
        OCSSD_DBG_INFO( this, " - blk [ " << i << "] dead obj nr = " << this->blk_dead_obj_nr[i] );
    }
    // GC will ignore blk_act
    uint32_t blk_surce = 0;
    uint32_t blk_destn = 0;

    struct nvm_addr nvm_addr_surce, nvm_addr_destn;
    struct nvm_ret ret;

    char* gc_buf_surce_page = (char* )malloc( geo->page_nbytes );
    char* obj_surce = nullptr ;

    char* gc_buf_destn_page = (char* )malloc( geo->page_nbytes );
    uint16_t destn_pg_idx = 0;
    uint8_t  destn_page_obj_count = 0;

    // find destination block
    for(uint32_t i = 0; i < this->data_blk_nr; ++i ){
        if( blk_map[ i ] == MBA_MAP_STATE_EMPT && i != blk_act ){
            blk_destn = i;
            OCSSD_DBG_INFO( this, " - blk destn = " << blk_destn );
            break;
        }
    }
    for(uint32_t blk_idx = 0; blk_idx < this->data_blk_nr; ++blk_idx){
        // find source block
        if( blk_map[ blk_idx ] == MBA_MAP_STATE_FULL &&
            blk_idx != blk_act &&
            this->blk_dead_obj_nr[ blk_idx ] > 0 ){
            blk_surce = blk_idx;
            OCSSD_DBG_INFO( this, " - blk source = " << blk_surce );
        }
        else{
            continue;
        }
        if( blk_surce == blk_destn ){
            OCSSD_DBG_INFO( this , " - Cannot find free block. GC over.");
            break;
        }

        OCSSD_DBG_INFO( this, " - GC from surce to destn.");
        struct blk_addr* blk_addr_surce = &( this->meta_blk_addr[ this->bitmap_blk_nr + blk_surce ] );
        struct blk_addr* blk_addr_destn = &( this->meta_blk_addr[ this->bitmap_blk_nr + blk_destn ] );
        nvm_addr_surce.ppa = 0;
        nvm_addr_destn.ppa = 0;
        ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(blk_addr_surce, & nvm_addr_surce );
        ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(blk_addr_destn, & nvm_addr_destn );

        for(uint16_t page_idx = 0; page_idx < geo->npages; ++page_idx ){
            nvm_addr_surce.g.pg = page_idx;
            nvm_addr_read( dev, &nvm_addr_surce, 1, gc_buf_surce_page, nullptr, NVM_FLAG_PMODE_SNGL, &ret);
//            OCSSD_DBG_INFO( this, " - - read page " << page_idx);
            if( ret.status > 0 ) {
                // read fail
            }
            for(size_t obj_nr = 0; obj_nr < this->obj_nr_per_page; ++obj_nr){
                obj_surce = gc_buf_surce_page + obj_nr * obj_size;
                Nat_Obj_ID_Type obj_id = *( (Nat_Obj_ID_Type*)obj_surce );// every obj struct start with its obj_id
                uint8_t nat_state = nat->entry[ obj_id ].state;
                uint32_t _blk_idx = nat->entry[ obj_id ].blk_idx;
                uint16_t _pg_idx  = nat->entry[ obj_id ].page;
                uint8_t  _obj_idx = nat->entry[ obj_id ].obj;
                uint8_t obj_state = blk_page_obj_map[_blk_idx][_pg_idx][_obj_idx];
                if( obj_state == MBA_MAP_OBJ_STATE_USED ){// find one useful obj in source block
                    memcpy( gc_buf_destn_page + destn_page_obj_count * obj_size, obj_surce, obj_size );
                    this->nat->entry[ obj_id ].blk_idx = blk_destn;
                    this->nat->entry[ obj_id ].page = destn_pg_idx;
                    this->nat->entry[ obj_id ].obj = destn_page_obj_count;
                    set_obj_state(blk_destn, destn_pg_idx, destn_page_obj_count, MBA_MAP_OBJ_STATE_USED );
                    destn_page_obj_count += 1;
                    this->nat->entry[ obj_id ].state = NAT_ENTRY_WRIT;
                    if( destn_page_obj_count >= this->obj_nr_per_page ){// this page is full. need to write into ssd
                        nvm_addr_destn.g.pg = destn_pg_idx;
                        nvm_addr_write( dev, &nvm_addr_destn, 1, gc_buf_destn_page, nullptr, NVM_FLAG_PMODE_SNGL, &ret);
                        if( ret.status > 0 ){
                            // write fail
                        }
                        destn_page_obj_count = 0;
                        destn_pg_idx += 1;
                    }
                    if( destn_pg_idx == geo->npages ){// need to find another empty block as destination
                        for(uint32_t i = 0; i < this->data_blk_nr; ++i ){
                            if( blk_map[ i ] == MBA_MAP_STATE_EMPT && i != blk_act ){
                                blk_destn = i;
                                destn_page_obj_count = 0;
                                destn_pg_idx = 0;
                                OCSSD_DBG_INFO( this, " - blk destn = " << blk_destn );
                                break;
                            }
                        }
                    }
                }
            }
        }// end loop for all page in source block
        OCSSD_DBG_INFO( this, " - erease source block");
        nvm_addr_surce.g.pg = 0;
        nvm_addr_erase( dev, &nvm_addr_surce, 1 , NVM_FLAG_PMODE_SNGL, &ret);

        OCSSD_DBG_INFO( this, " - update bitmap");
        blk_map[ blk_surce ] = MBA_MAP_STATE_EMPT;
        for(size_t pg = 0; pg < geo->npages; ++pg ){
            blk_page_map[ blk_surce ][ pg ] = MBA_MAP_STATE_EMPT;
        }
        for(size_t obj = 0; obj < this->obj_nr_per_page; ++obj){
            for(size_t pg = 0; pg < geo->npages; ++pg ){
                blk_page_obj_map[ blk_surce ][ pg ][ obj ] = MBA_MAP_OBJ_STATE_FREE;
            }
        }
        OCSSD_DBG_INFO( this, " - update dead block counter");
        this->all_dead_obj_nr -= this->blk_dead_obj_nr[ blk_surce ];
        this->blk_dead_obj_nr[ blk_surce ] = 0;
    }// end loop for all source block
    if( destn_page_obj_count > 0 ){ // last page is not full
        OCSSD_DBG_INFO( this, " - write last page for destn");
        for(uint8_t obj = destn_page_obj_count; obj < obj_nr_per_page; ++obj){
            set_obj_state( blk_destn, destn_page_obj_count, obj, MBA_MAP_OBJ_STATE_DEAD );
        }
        memset( gc_buf_destn_page + destn_page_obj_count * obj_size, 0, geo->page_nbytes - destn_page_obj_count * obj_size);
        struct blk_addr* blk_addr_destn = &( this->meta_blk_addr[ this->bitmap_blk_nr + blk_destn ] );
        nvm_addr_destn.ppa = 0;
        ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(blk_addr_destn, & nvm_addr_destn );
        nvm_addr_destn.g.pg = destn_pg_idx;
        nvm_addr_write( dev, &nvm_addr_destn, 1, gc_buf_destn_page, nullptr, NVM_FLAG_PMODE_SNGL, &ret);
        if( ret.status > 0 ){
            // write fail
        }
    }
}

void* MetaBlkArea::read_by_obj_id(Nat_Obj_ID_Type obj_id)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return nullptr;
    if( /*nat->entry[ obj_id ].state == NAT_ENTRY_USED || */
        nat->entry[ obj_id ].state == NAT_ENTRY_FREE ) return nullptr;
    //if( nat->entry[obj_id].obj_id != obj_id ) return;

    void* ret = malloc( obj_size );

    if( nat->entry[ obj_id ].state == NAT_ENTRY_USED ){
        *((Nat_Obj_ID_Type*)ret) = obj_id;
        memset( (char* )ret + sizeof(Nat_Obj_ID_Type), 0, obj_size - sizeof(Nat_Obj_ID_Type) );
        return ret;
    }

    uint32_t blk_idx = this->nat->entry[ obj_id ].blk_idx;
    uint16_t pg_idx  = this->nat->entry[ obj_id ].page;
    uint8_t  obj_idx = this->nat->entry[ obj_id ].obj;

    if( blk_idx == blk_act && pg_idx == page_act ){ // still in buf
        memcpy(ret, (uint8_t * ) w_buf + obj_idx * obj_size, obj_size );
        return ret;
    }

    struct nvm_addr nvm_addr_;
    nvm_addr_.ppa = 0;
    blk_addr_handle* bah_ = ocssd_bah->get_blk_addr_handle( 0 );
    bah_->convert_2_nvm_addr(
        &( meta_blk_addr[ bitmap_blk_nr + blk_idx ] ),
        &nvm_addr_
    );
    nvm_addr_.g.pg = pg_idx;

    struct nvm_ret nvm_ret_;
    nvm_addr_read( dev, &nvm_addr_, 1, r_buf, nullptr, 0, &nvm_ret_ );
    // copy to ret
    memcpy( ret, (char*)r_buf + obj_idx * obj_size, obj_size );
    return ret;
}

void MetaBlkArea::write_by_obj_id(Nat_Obj_ID_Type obj_id, void* obj)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return;
    if( nat->entry[ obj_id ].state == NAT_ENTRY_FREE ) return;

    if( nat->entry[ obj_id ].state == NAT_ENTRY_WRIT ){
        set_obj_state(
          nat->entry[ obj_id ].blk_idx,
          nat->entry[ obj_id ].page,
          nat->entry[ obj_id ].obj,
          MBA_MAP_OBJ_STATE_DEAD
        );
        this->all_dead_obj_nr += 1;
        this->blk_dead_obj_nr[ nat->entry[obj_id].blk_idx ] += 1;
    }

    nat->entry[ obj_id ].blk_idx  = blk_act;
    nat->entry[ obj_id ].page     = page_act;
    nat->entry[ obj_id ].obj      = obj_act;
    nat->entry[ obj_id ].state    = NAT_ENTRY_WRIT;

    uint32_t blk_idx = this->nat->entry[ obj_id ].blk_idx;
    uint16_t pg_idx  = this->nat->entry[ obj_id ].page;
    uint8_t  obj_idx = this->nat->entry[ obj_id ].obj;
    memcpy( (char*)w_buf + obj_idx * obj_size, obj, obj_size);
    act_obj_set_state( MBA_MAP_OBJ_STATE_USED );
    act_addr_add( 1 );

    if( obj_act == 0 ){
        struct nvm_addr nvm_addr_w;
        nvm_addr_w.ppa = 0;
        ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(&(meta_blk_addr[ bitmap_blk_nr + blk_idx]), &nvm_addr_w);
        nvm_addr_w.g.pg = pg_idx;
        struct nvm_ret ret;
        nvm_addr_write(dev, &nvm_addr_w, 1 , w_buf, nullptr, NVM_FLAG_PMODE_SNGL, &ret);
    }

}

std::string MetaBlkArea::txt()
{
    return "MetaBlockArea";
}

void MetaBlkArea::flush()
{
    flush_data();   // flush data may change bitmap
    flush_bitmap();
}

void MetaBlkArea::flush_bitmap()
{
    OCSSD_DBG_INFO( this, "Flush bitmap");

    nvm_vblk_erase( this->bitmap_vblk );
    OCSSD_DBG_INFO( this, " - erease vblk");

    size_t size_ = 0;
    size_t offset = 0;

    if( map_buf == nullptr ){
        map_buf = (uint8_t * )malloc( map_buf_size );
    }

    size_ = sizeof( uint8_t ) * this->data_blk_nr;
    memcpy(map_buf + offset, blk_map, size_);
    offset += size_;

    size_ = sizeof( uint8_t ) * geo->npages;
    for(size_t blk = 0; blk < this->data_blk_nr; ++blk){
        memcpy( map_buf + offset, blk_page_map[blk], size_);
        offset += size_;
    }

    size_ = sizeof( uint8_t ) * this->obj_nr_per_page;
    for(size_t blk = 0; blk < this->data_blk_nr; ++blk){
        for(size_t pg = 0; pg < geo->npages; ++pg){
            memcpy(map_buf + offset, blk_page_obj_map[blk][pg], size_);
            offset += size_;
        }
    }

    memset(map_buf + offset, 0, map_buf_size - offset);

    nvm_vblk_write( bitmap_vblk, map_buf, map_buf_size );
    OCSSD_DBG_INFO( this, " - write vblk");
}

void MetaBlkArea::flush_data()
{
    OCSSD_DBG_INFO( this, "Flush meta data");

    if( obj_act == 0 ){
        OCSSD_DBG_INFO(this, " - all data is flushed");
        return ;
    }

    OCSSD_DBG_INFO( this, " - obj per page = " << this->obj_nr_per_page );
    OCSSD_DBG_INFO( this, " - active obj   = " << obj_act );

    memset( (char* )w_buf + obj_act * obj_size, 0, (obj_nr_per_page - obj_act) * obj_size);
    for(uint8_t obj = obj_act; obj < obj_nr_per_page; ++obj){
        set_obj_state(blk_act, page_act, obj, MBA_MAP_OBJ_STATE_DEAD);
//        act_addr_add( 1 );
//        this->all_dead_obj_nr += 1;
//        this->blk_dead_obj_nr[ blk_act ] += 1;
    }
    OCSSD_DBG_INFO( this, " - set remaining obj DEAD");

    struct nvm_addr nvm_addr_w;
    nvm_addr_w.ppa = 0;
    ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(&(meta_blk_addr[ bitmap_blk_nr + blk_act]), &nvm_addr_w);
    nvm_addr_w.g.pg = page_act;
    struct nvm_ret ret;
    nvm_addr_write(dev, &nvm_addr_w, 1 , w_buf, nullptr, NVM_FLAG_PMODE_SNGL, &ret);
    OCSSD_DBG_INFO( this, " - write pg_act " << page_act );

    act_addr_add( obj_nr_per_page - obj_act );
}

void MetaBlkArea::print_nat()
{
    OCSSD_DBG_INFO( this, " - print NAT table");
    for(size_t i = 1; i < nat->max_length; ++i){
        if(nat->entry[ i ].state == NAT_ENTRY_FREE){
            printf("NAT[%8lu]{%s}\n", i, "\"FREE\"");
        }else if(nat->entry[ i ] .state == NAT_ENTRY_USED){
            printf("NAT[%8lu]{%s}\n", i, "\"USED\"");
        }else {
            printf("NAT[%8lu]{%s} ", i, "\"WRIT\"");
            printf("blk = %3u, pg = %3u, obj = %1u\n",
                nat->entry[i].blk_idx,
                nat->entry[i].page,
                nat->entry[i].obj);
        }
    }
}

void MetaBlkArea::print_bitmap()
{
    OCSSD_DBG_INFO( this, " - print bitmap");
    printf(" - blk map - \n");
    for(size_t blk = 0; blk < data_blk_nr; ++blk){
        printf("[%2lu]=", blk);
        switch (blk_map[blk]){
            case MBA_MAP_STATE_EMPT: printf("\"EMPT\" ");break;
            case MBA_MAP_STATE_PART: printf("\"PART\" ");break;
            case MBA_MAP_STATE_FULL: printf("\"FULL\" ");break;
            default:break;
        }
        if( (blk+1) % 8 == 0 ){
            printf("\n");
        }
    }
    printf("\n");

    printf(" - blk page map - \n");
    for(size_t blk = 0; blk < data_blk_nr; ++blk){
        printf(" block = %2lu\n", blk);
        for(size_t pg = 0; pg < geo->npages; ++pg){
            printf("[%3lu]=",pg);
            switch (blk_page_map[blk][pg]) {
                case MBA_MAP_STATE_EMPT:
                    printf("\"EMPT\" ");
                    break;
                case MBA_MAP_STATE_PART:
                    printf("\"PART\" ");
                    break;
                case MBA_MAP_STATE_FULL:
                    printf("\"FULL\" ");
                    break;
                default:
                    break;
            }
            if( (pg+1) % 8 == 0 ){
                printf("\n");
            }
        }
    }
    printf("\n");

    printf(" - blk page obj map - ");
    for(size_t blk = 0; blk < data_blk_nr; ++blk){
        printf(" block = %2lu\n", blk);
        for(size_t pg = 0; pg < geo->npages; ++pg){
            printf(" page = %2lu : ", pg);
            for(size_t obj = 0; obj < obj_nr_per_page; ++obj){
                printf("[%2lu]=", obj);
                switch (blk_page_obj_map[blk][pg][obj]){
                    case MBA_MAP_OBJ_STATE_FREE: printf("\"FREE\" ");break;
                    case MBA_MAP_OBJ_STATE_DEAD: printf("\"DEAD\" ");break;
                    case MBA_MAP_OBJ_STATE_USED: printf("\"USED\" ");break;
                    default: printf("WHAT???%d",blk_page_obj_map[blk][pg][obj]);break;
                }
            }
            if( (pg+1)%4 == 0 )
               printf("\n");
        }
    }
    printf("\n");
}
