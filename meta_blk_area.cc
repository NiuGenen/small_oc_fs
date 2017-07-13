#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "meta_blk_area.h"
#include "dbg_info.h"
#include "blk_addr.h"
#include "liblightnvm.h"

extern blk_addr_handle **blk_addr_handlers_of_ch;

MetaBlkArea::MetaBlkArea(   // first block to store bitmap
    struct nvm_dev * dev,
    const struct nvm_geo* geo,
	size_t obj_size,
	const char* mba_name,
	uint32_t st_ch, uint32_t ed_ch,
	struct blk_addr* st_addr, size_t* addr_nr,
	struct nat_table* nat )
{
    size_t nat_max_length   =   nat->max_length;

    this->dev               =   dev;
    this->geo               =   geo;

    this->obj_size          =   obj_size;
    this->obj_nr_per_page   =   geo->page_nbytes / obj_size ;
    this->obj_nr_per_blk    =   obj_nr_per_page * geo->npages;
    this->mba_name          =   mba_name;
    
    // build meta_blk_addr[ meta_blk_addr_size ]
    // meta_blk_addr[0] contains bitmap
    // meta_blk_addr[1, size-1] contains obj
    meta_blk_addr_size = 0;
    for(int ch=st_ch; ch<=ed_ch; ++ch){
        meta_blk_addr_size += addr_nr[ ch - st_ch ];
    }
    meta_blk_addr = new struct blk_addr[ meta_blk_addr_size ];  // all blk_addr of this meta area
    size_t idx = 0;
    for(int ch=st_ch; ch<=ed_ch; ++ch){
        for(size_t i=0; i<addr_nr[ch-st_ch]; ++i){
            meta_blk_addr[ idx ] = st_addr[ ch-st_ch ];
            blk_addr_handlers_of_ch[ ch ]->BlkAddrAdd( i, &(meta_blk_addr[ idx ]) );
            idx += 1;
        }
    }

    // read bitmap from SSD & build blk_map[],blk_page_map[],blk_page_obj_map[]
    struct nvm_addr first_blk_nvm_addr;
    blk_addr_handlers_of_ch[ st_ch ]->convert_2_nvm_addr( &(meta_blk_addr[0]), &first_blk_nvm_addr );
    struct nvm_vblk * first_blk_vblk = nvm_vblk_alloc( dev, &first_blk_nvm_addr, 1 ); 
    
    map_buf_size = geo->npages * geo->page_nbytes; // one block size
    map_buf =(uint8_t*)malloc( map_buf_size );
    size_t map_buf_idx = 0;
    nvm_vblk_read( first_blk_vblk, map_buf, map_buf_size );    // read bitmap
    nvm_vblk_free( first_blk_vblk );

    blk_map = new uint8_t[ meta_blk_addr_size ];    // alloc blk_map[ 1, size ]
    for(int i=1; i<meta_blk_addr_size; ++i){
        blk_map[i] = map_buf[ map_buf_idx++ ];
    }

    blk_page_map = new uint8_t*[ meta_blk_addr_size ]; // alloc blk_page_map
    for(int i=1; i<meta_blk_addr_size; ++i){
        blk_page_map[i] = new uint8_t[ geo->npages ];
        for(int j=0; j<geo->npages; ++j){
            blk_page_map[i][j] = map_buf[ map_buf_idx++ ];
        }
    }

    blk_page_obj_map = new uint8_t**[ meta_blk_addr_size ]; // alloc blk_page_obj_map
    for(int i=1; i<meta_blk_addr_size; ++i){
        blk_page_obj_map[i] = new uint8_t*[ geo->npages ];
        for(int j=0; j<geo->npages; ++j){
            blk_page_obj_map[i][j] = new uint8_t[ obj_nr_per_page ];
            for(int k=0; k<obj_nr_per_page; ++k){
                blk_page_obj_map[i][j][k] = map_buf[ map_buf_idx++ ];
            }
        }
    }

    // active blk & page & obj
    blk_act = 0;
    page_act = 0;
    obj_act = 0;
    find_next_act_blk( blk_act );

    // nat table
    this->nat               =   nat;
    this->nat_max_length    =   nat_max_length;
    this->nat_dead_nr       =   0;

    // buf to read one page ( the number of obj in one page is @obj_nr_per_page )
    // obj_size * obj_nr_per_page = geo->page_nbytes
    this->buf = new char[ geo->page_nbytes ];

    nat_need_flush = 0;

    // obj cache
    obj_cache = (void**) malloc( sizeof(void*) * nat_max_length );
}

MetaBlkArea::~MetaBlkArea()
{

}

void MetaBlkArea::find_next_act_blk( size_t last_blk_idx )
{
    size_t blk_idx = ( last_blk_idx + 1 ) % meta_blk_addr_size;
    if( !blk_idx ) blk_idx = 1;

    size_t count = 0;
    // blk_map[ 1, size-1 ]
    while( count < meta_blk_addr_size - 1 ){
        if( blk_map[blk_idx] != MBA_MAP_STATE_FULL ){
            
            blk_act = blk_idx;
            size_t page_idx = 0;
            for(page_idx = 0; page_idx < geo->npages; ++page_idx ){
                if( blk_page_map[ blk_act ][ page_idx ] != MBA_MAP_STATE_FULL ){
                    page_act = page_idx;
                    size_t obj_idx = 0;
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
        blk_idx = ( blk_idx + 1 ) % meta_blk_addr_size;
        if( !blk_idx ) blk_idx = 1;
        count += 1;
    }
}

Nat_Obj_ID_Type MetaBlkArea::alloc_obj()
{
    for(size_t i = 0; i < nat_max_length; ++i ){
        if( nat->entry[i].state == NAT_ENTRY_FREE ){
            nat->entry[i].blk_idx  = blk_act;
            nat->entry[i].page     = page_act;
            nat->entry[i].obj      = obj_act;
            act_addr_set_state( MBA_MAP_STATE_FULL );   // obj is full ( of course )
            act_addr_add( 1 );
            if( obj_cache[i] != nullptr ){
                //free( obj_cache[i] );
                OCSSD_DBG_INFO( this, "obj_cache[ " << i << "] not null.");
                obj_cache[i] = nullptr;
            }
            return i;
        }
    }
}

void MetaBlkArea::act_addr_set_state( uint8_t state )	// update bitmap with current act
{
    blk_page_obj_map[ blk_act ][ page_act ][ obj_act ] = state; // update obj

//#define MBA_MAP_STATE_FREE 0
//#define MBA_MAP_STATE_USED 1 
//#define MBA_MAP_STATE_FULL 2

    uint8_t s_full = MBA_MAP_STATE_FULL;
    uint8_t s_used = 0;
    for(size_t i = 0; i < obj_nr_per_page; ++i){    // obj = 0, 2
        // all 0 : s_full == 0 , s_used == 0
        // all 2 : s_full == 2 , s_used == 2
        // 0 & 2 : s_full == 0 , s_used == 2
        s_full = s_full & blk_page_obj_map[ blk_act ][ page_act ][ i ];
        s_used = s_used | blk_page_obj_map[ blk_act ][ page_act ][ i ];
    }
    if( s_full & MBA_MAP_STATE_FULL ){
        blk_page_map[ blk_act ][ page_act ] = MBA_MAP_STATE_FULL;   // update page
    }
    else if( s_used ){
        blk_page_map[ blk_act ][ page_act ] = MBA_MAP_STATE_USED; 
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
        s_full = s_full & blk_page_map[ blk_act ][ i ];
        s_used = s_used | blk_page_map[ blk_act ][ i ];
    }
    if( s_full & MBA_MAP_STATE_FULL ){
        blk_map[ blk_act ] = MBA_MAP_STATE_FULL;        // update blk
    }
    else if( s_used ) {
        blk_map[ blk_act ] = MBA_MAP_STATE_USED;        // update blk
    }
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
    // GC will ignore blk_act
}

void MetaBlkArea::write_by_obj_id(Nat_Obj_ID_Type obj_id, void* obj)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return;
    if( nat->entry[ obj_id ].state == NAT_ENTRY_FREE ||
            nat->entry[ obj_id ].state == NAT_ENTRY_DEAD ) return;
    if( obj_cache[ obj_id ] != nullptr ){
        //free( obj_cache[i] );
        //obj_cache[i] = obj;
        // memcpy better
        memcpy( obj_cache[ obj_id ], obj, obj_size );
        nat->entry[ obj_id ].state = NAT_ENTRY_MOVE;
    }
}

void* MetaBlkArea::read_by_obj_id(Nat_Obj_ID_Type obj_id)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return nullptr;
    if( nat->entry[obj_id].state == NAT_ENTRY_DEAD ||
            nat->entry[ obj_id ].state == NAT_ENTRY_FREE) return nullptr;
    //if( nat->entry[obj_id].obj_id != obj_id ) return; 

    void* ret = malloc( obj_size );
    
    if( obj_cache[ obj_id ] != nullptr ){
        memcpy( ret, obj_cache[ obj_id ], obj_size );
        return ret;
    }
    else{
        struct nvm_addr nvm_addr_;
        blk_addr_handlers_of_ch[0]->convert_2_nvm_addr(
            &( meta_blk_addr[ nat->entry[ obj_id ].blk_idx ] ),
            &nvm_addr_
        );
        nvm_addr_.g.pg = nat->entry[ obj_id ].page;

        struct nvm_ret nvm_ret_;
        nvm_addr_read( dev, &nvm_addr_, 1, buf, nullptr, 0, &nvm_ret_ );

        void *obj = malloc( obj_size );
        memcpy( obj, buf + obj_size * nat->entry[ obj_id ].obj , obj_size );
        obj_cache[obj_id] = obj;

        memcpy( ret, obj_cache[ obj_id ], obj_size );
        return ret;
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
