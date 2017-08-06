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
    
    // build meta_blk_addr[ meta_blk_addr_size ]
    // meta_blk_addr[0] contains bitmap
    // meta_blk_addr[1, size-1] contains obj
    this->meta_blk_nr = 0;
    for(uint32_t ch=st_ch, count = 0; count < ch_nr; ++count){
        this->meta_blk_nr += addr_nr[ ch - st_ch ];
    }
    this->meta_blk_addr = new struct blk_addr[ meta_blk_nr ];  // all blk_addr of this meta area
    OCSSD_DBG_INFO( this, " - meta blk nr = " << meta_blk_nr );

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
    this->nat_dead_nr       =   0;

    this->buf = nullptr;

    nat_need_flush = 0;

    // obj cache
    obj_cache = (void**) malloc( sizeof(void*) * nat_max_length );
    OCSSD_DBG_INFO( this, " - build obj cache table");
}

MetaBlkArea::~MetaBlkArea()
{
    if( meta_blk_addr != nullptr ) free( meta_blk_addr );
    nvm_vblk_free( bitmap_vblk );
    if( map_buf != nullptr ) free( map_buf );
    if( blk_map != nullptr ) free( blk_map );
    if( blk_page_map != nullptr ) free( blk_page_map );
    if( blk_page_obj_map != nullptr ) free( blk_page_obj_map );
    if( obj_cache != nullptr) {
        for(size_t i = 0 ; i < nat->max_length; ++i){
            if( obj_cache[i] != nullptr ) free( obj_cache[ i ] ) ;
        }
        free(obj_cache);
    }
    if( buf != nullptr ) free( buf );
}

void MetaBlkArea::find_next_act_blk( uint32_t last_blk_idx )
{
    uint32_t blk_idx = ( last_blk_idx + 1 ) % (uint32_t ) data_blk_nr;
//    if( !blk_idx ) blk_idx = 1;

    size_t count = 0;
    while( count < data_blk_nr ){
        if( blk_map[blk_idx] != MBA_MAP_STATE_FULL ){
            blk_act = blk_idx;
            uint16_t page_idx = 0;
            for(page_idx = 0; page_idx < geo->npages; ++page_idx ){
                if( blk_page_map[ blk_act ][ page_idx ] != MBA_MAP_STATE_FULL ){
                    page_act = page_idx;
                    uint8_t obj_idx = 0;
                    for( obj_idx = 0; obj_idx < obj_nr_per_page; ++obj_idx ){
                        if( blk_page_obj_map[ blk_act ][ page_act ][ obj_idx ] == MBA_MAP_STATE_FREE )
                        {
                            obj_act = obj_idx;
                            return;
                        }
                    }
                }
            }
        }
        blk_idx = ( blk_idx + 1 ) % (uint32_t ) data_blk_nr;
//        if( !blk_idx ) blk_idx = 1;
        count += 1;
    }
}

Nat_Obj_ID_Type MetaBlkArea::alloc_obj()
{
    for(size_t i = 1; i < nat_max_length; ++i ){
        if( nat->entry[i].state == NAT_ENTRY_FREE ){
            nat->entry[i].blk_idx  = blk_act;
            nat->entry[i].page     = page_act;
            nat->entry[i].obj      = obj_act;
            nat->entry[i].state    = NAT_ENTRY_USED;
            act_addr_set_state( MBA_MAP_STATE_FULL );   // obj is full ( of course )
            act_addr_add( 1 );
            if( obj_cache[i] != nullptr ){
//                free( obj_cache[i] );
                OCSSD_DBG_INFO( this, "obj_cache[ " << i << "] not null. will be COVERED");
//                obj_cache[i] = nullptr;
            }
            else{
                obj_cache[ i ] = malloc( obj_size );
                *((Nat_Obj_ID_Type*)(obj_cache[i])) = i;
            }
            return i;
        }
    }
}

void MetaBlkArea::set_obj_state(uint32_t blk, uint16_t pg, uint8_t obj, uint8_t state){
    blk_page_obj_map[ blk ][ pg ][ obj ] = state; // update obj

//#define MBA_MAP_STATE_FREE 0
//#define MBA_MAP_STATE_USED 1
//#define MBA_MAP_STATE_FULL 2

    uint8_t s_full = MBA_MAP_STATE_FULL;
    uint8_t s_used = 0;
    for(size_t i = 0; i < obj_nr_per_page; ++i){    // obj = 0, 2
        // all 0 : s_full == 0 , s_used == 0
        // all 2 : s_full == 2 , s_used == 2
        // 0 & 2 : s_full == 0 , s_used == 2
        s_full = s_full & blk_page_obj_map[ blk ][ pg ][ i ];
        s_used = s_used | blk_page_obj_map[ blk ][ pg ][ i ];
    }
    if( s_full & MBA_MAP_STATE_FULL ){
        blk_page_map[ blk ][ pg ] = MBA_MAP_STATE_FULL;   // update page
    }
    else if( s_used ){
        blk_page_map[ blk ][ pg ] = MBA_MAP_STATE_USED;
    }

    s_full = MBA_MAP_STATE_FULL;
    s_used = 0;
    for(size_t i = 0; i < geo->npages; ++i){ // page = 0, 1, 2
        // all 0     :  s_full = 0 , s_used = 0
        // all 1     :  s_full = 0 , s_used = 1
        // all 2     :  s_full = 2 , s_used = 2
        // 1 & 0     :  s_full = 0 , s_used = 1
        // 2 & 0     :  s_full = 0 , s_used = 2
        // 1 & 2     :  s_full = 0 , s_used = 3
        // 1 & 2 & 0 :  s_full = 0 , s_used = 3
        s_full = s_full & blk_page_map[ blk ][ i ];
        s_used = s_used | blk_page_map[ blk ][ i ];
    }
    if( s_full & MBA_MAP_STATE_FULL ){
        blk_map[ blk ] = MBA_MAP_STATE_FULL;        // update blk
    }
    else if( s_used ) {
        blk_map[ blk ] = MBA_MAP_STATE_USED;        // update blk
    }
}

void MetaBlkArea::act_addr_set_state( uint8_t state )	// update bitmap with current act
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

void MetaBlkArea::de_alloc_obj(Nat_Obj_ID_Type obj_id)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return;
    nat->entry[ obj_id ].state = NAT_ENTRY_DEAD;
    nat_dead_nr += 1;                   // for GC
}

int MetaBlkArea::need_GC()
{
    if( nat_dead_nr > nat_max_length / 2 ){
        return 1;
    }
    return 0;
}

void MetaBlkArea::GC()
{
    OCSSD_DBG_INFO( this, " GC start!");
    // GC will ignore blk_act
    for(uint32_t blk_idx = 0; blk_idx < this->data_blk_nr; ++blk_idx){
        uint32_t blk_surce = blk_idx;
        uint32_t blk_destn = blk_idx;
        if( blk_map[ blk_idx ] == MBA_MAP_STATE_FULL ){
            blk_surce = blk_idx;
            OCSSD_DBG_INFO( this, " - blk source = " << blk_surce );
        }
        for(uint32_t i = 0; i < this->data_blk_nr; ++i ){
            if( blk_map[ i ] == MBA_MAP_STATE_FREE ){
                blk_destn = i;
                OCSSD_DBG_INFO( this, " - blk destn = " << blk_destn );
                break;
            }
        }
        if( blk_surce == blk_destn ){
            OCSSD_DBG_INFO( this , " - Cannot find free block. GC over.");
            break;
        }
        OCSSD_DBG_INFO( this, " - GC from surce to destn.");
        struct blk_addr* blk_addr_surce = &( this->meta_blk_addr[ this->bitmap_blk_nr + blk_surce ] );
        struct blk_addr* blk_addr_destn = &( this->meta_blk_addr[ this->bitmap_blk_nr + blk_destn ] );
        struct nvm_addr nvm_addr_surce, nvm_addr_destn;
        nvm_addr_surce.ppa = 0;
        nvm_addr_destn.ppa = 0;
        ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(blk_addr_surce, & nvm_addr_surce );
        ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(blk_addr_destn, & nvm_addr_destn );

        char* gc_buf_surce_page = (char* )malloc( geo->page_nbytes );
        char* obj_surce = (char* )malloc( this->obj_size );

        char* gc_buf_destn_page = (char* )malloc( geo->page_nbytes );
        uint16_t destn_pg_idx = 0;
        uint8_t  destn_page_obj_count = 0;

        struct nvm_ret ret;
        for(uint16_t page_idx = 0; page_idx < geo->npages; ++page_idx ){
            nvm_addr_surce.g.pg = page_idx;
            nvm_addr_read( dev, &nvm_addr_surce, 1, gc_buf_surce_page, nullptr, NVM_FLAG_PMODE_SNGL, &ret);
            if( ret.status > 0 ) {
                // read fail
            }
            for(size_t obj_nr = 0; obj_nr < this->obj_nr_per_page; ++obj_nr){
                obj_surce = gc_buf_surce_page + obj_nr * obj_size;
                Nat_Obj_ID_Type obj_id = *( (Nat_Obj_ID_Type*)obj_surce );// every obj start with its obj_id
                if( this->nat->entry[ obj_id ].state == NAT_ENTRY_USED ||
                    this->nat->entry[ obj_id ].state == NAT_ENTRY_MOVE){
                    memcpy( gc_buf_destn_page + destn_page_obj_count * obj_size, obj_surce, obj_size );
                    this->nat->entry[ obj_id ].blk_idx = blk_destn;
                    this->nat->entry[ obj_id ].page = destn_pg_idx;
                    this->nat->entry[ obj_id ].obj = destn_page_obj_count;
                    set_obj_state(blk_destn, destn_pg_idx, destn_page_obj_count, MBA_MAP_STATE_USED );
                    destn_page_obj_count += 1;
                    if( this->nat->entry[ obj_id ].state == NAT_ENTRY_MOVE )
                        this->nat->entry[ obj_id ].state = NAT_ENTRY_USED;
                    if( destn_page_obj_count >= this->obj_nr_per_page ){
                        nvm_addr_destn.g.pg = destn_pg_idx;
                        nvm_addr_write( dev, &nvm_addr_destn, 1, gc_buf_destn_page, nullptr, NVM_FLAG_PMODE_SNGL, &ret);
                        if( ret.status > 0 ){
                            // write fail
                        }
                        destn_page_obj_count = 0;
                        destn_pg_idx += 1;
                    }
                }
                else if( this->nat->entry[ obj_id ].state == NAT_ENTRY_DEAD ){
                    this->nat->entry[ obj_id ].state = NAT_ENTRY_FREE;
                    nat_dead_nr -= 1;
                }
            }
        }// end loop for GC
        nvm_addr_surce.g.pg = 0;
        nvm_addr_erase( dev, &nvm_addr_surce, 1 , NVM_FLAG_PMODE_SNGL, &ret);
        blk_map[ blk_surce ] = MBA_MAP_STATE_FREE;
        for(size_t pg = 0; pg < geo->npages; ++pg ){
            blk_page_map[ blk_surce ][ pg ] = MBA_MAP_STATE_FREE;
        }
        for(size_t obj = 0; obj < this->obj_nr_per_page; ++obj){
            for(size_t pg = 0; pg < geo->npages; ++pg ){
                blk_page_obj_map[ blk_surce ][ pg ][ obj ] = MBA_MAP_STATE_FREE;
            }
        }
    }
}

void MetaBlkArea::write_by_obj_id(Nat_Obj_ID_Type obj_id, void* obj)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return;
    if( nat->entry[ obj_id ].state == NAT_ENTRY_FREE ||
        nat->entry[ obj_id ].state == NAT_ENTRY_DEAD ) return;

    if( obj_cache[ obj_id ] != nullptr ){
        memcpy( obj_cache[ obj_id ], obj, obj_size );
        nat->entry[ obj_id ].state = NAT_ENTRY_MOVE;
    }
}

void* MetaBlkArea::read_by_obj_id(Nat_Obj_ID_Type obj_id)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return nullptr;
    if( nat->entry[ obj_id ].state == NAT_ENTRY_DEAD ||
        nat->entry[ obj_id ].state == NAT_ENTRY_FREE ) return nullptr;
    //if( nat->entry[obj_id].obj_id != obj_id ) return; 

    void* ret = malloc( obj_size );
    
    if( obj_cache[ obj_id ] == nullptr ){
        struct nvm_addr nvm_addr_;
        nvm_addr_.ppa = 0;
        blk_addr_handle* bah_ = ocssd_bah->get_blk_addr_handle( 0 );
        bah_->convert_2_nvm_addr(
            &( meta_blk_addr[ nat->entry[ obj_id ].blk_idx ] ),
            &nvm_addr_
        );
        nvm_addr_.g.pg = nat->entry[ obj_id ].page;

        struct nvm_ret nvm_ret_;
        if( buf == nullptr ) buf = (char* )malloc( geo->page_nbytes );
        nvm_addr_read( dev, &nvm_addr_, 1, buf, nullptr, 0, &nvm_ret_ );

        void *obj = malloc( obj_size );
        memcpy( obj, buf + obj_size * nat->entry[ obj_id ].obj , obj_size );
        obj_cache[obj_id] = obj;
    }
    memcpy( ret, obj_cache[ obj_id ], obj_size );
    return ret;
}

void MetaBlkArea::flush_obj_cache()
{
    for(size_t i = 0; i < this->nat_max_length; ++i){
        if( this->obj_cache[i] != nullptr ){
            Nat_Obj_ID_Type obj_id = *((Nat_Obj_ID_Type*)obj_cache[i]);
            struct nvm_addr nvm_addr_obj;
            nvm_addr_obj.ppa = 0;
            ocssd_bah->get_blk_addr_handle( 0 )->convert_2_nvm_addr(&(meta_blk_addr[this->nat->entry[i].blk_idx]), &nvm_addr_obj);
            // NEED TO CHANGE CACHE
            // cache granularity should be PAGE
        }
    }
}

std::string MetaBlkArea::txt()
{
    return "MetaBlockArea";
}

// let OcssdSuperBlock to flush NAT

int MetaBlkArea::if_nat_need_flush()
{
    return nat_need_flush;
}

void MetaBlkArea::after_nat_flush()
{
    nat_need_flush = 0;
}
