#include <stdlib.h>
#include "meta_blk_area.h"
#include "blk_addr.h"
#include <liblightnvm.h>

extern blk_addr_handle **blk_addr_handlers_of_ch;

MetaBlkArea::MetaBlkArea(
		int obj_size,
        int obj_nr_per_page,
		const char* mba_name,
		int st_ch, int ed_ch,
		struct blk_addr* st_addr, size_t* addr_nr,
		struct nar_table* nat, size_t nat_max_length )
{
    this->obj_size          =   obj_size;
    this->obj_nr_per_page   =   obj_nr_per_page;
    this->mba_name          =   mba_name;
    
    meta_blk_addr_size = 0;
    for(int ch=st_ch; cd<=ed_ch; ++ch){
        meta_blk_addr_size += addr_nr[ ch - st_ch ];
    }
    meta_blk_addr = new struct blk_addr[ meta_blk_addr_size ];  // all blk_addr of this meta area
    size_t idx = 0;
    for(int ch=st_ch; cd<=ed_ch; ++ch){
        for(int i=0; i<addr_nr[ch-st_ch]; ++i){
            meta_blk_addr[ idx ] = st_addr[ ch-st_ch ];
            blk_addr_handlers_of_ch[ ch ]->BlkAddrAdd( i, &(meta_blk_addr[ idx ]) );
            idx += 1;
        }
    }

    this->nat               =   nat;
    this->nat_max_length    =   nat_max_length;
    this->nat_dead_nr       =   0;

    this->buf = new char[ obj_size * obj_nr_per_page ];

    nat_need_flush = 0;

    obj_cache = malloc( sizeof(void*) * nat_max_length );
}


Nat_Obj_ID_Type MetaBlkArea::alloc_obj()
{
    for(int i=0; i<nat_max_length; ++i){
        if( nat->entry[i].is_used == 0 ){
            nat->entry[i].is_used = 1;
            return i;
        }
    }
}

void MetaBlkArea::de_alloc_obj(Nat_Obj_ID_Type obj_id)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return;
    nat->entry[ obj_id ].is_used = 1;
    nat->entry[ obj_id ].is_dead = 1;
    nat_dead_nr += 1;                   // for GC
}

void  MetaBlkArea::write_by_obj_id(Nat_Obj_Addr_Type obj_id, void* obj)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return;
    if( nat->entrt[i].is_used == 0 ) return;
    if( obj_cache[i] != obj ){
        //free( obj_cache[i] );
        obj_cache[i] = obj;
        // memcpy better
    }
}

void* MetaBlkArea::read_by_obj_id(Nat_Obj_ID_Type obj_id)
{
    if( obj_id <=0 || obj_id >= nat_max_length ) return;
    if( nat->entrt[obj_id].is_used == 0 ) return;
    if( nat->entry[obj_id].obj_id != obj_id ) return; 
    
    if( obj_cache[obj_id] != nullptr ){
        return obj_cache[i];
    }
    else{
        struct nvm_addr nvm_addr_;
        blk_addr_handlers_of_ch[0]->convert_2_nvm_addr(
            &( meta_blk_addr[ nat->entry[obj_id].blk_idx ] ),
            &nvm_addr_
        );
        nvm_addr_.g.pg = nat->entry[obj_id].page;

        struct nvm_ret nvm_ret_;
        nvm_addr_read( dev, &nvm_addr_, 1, buf, nullptr, 0, &nvm_ret_ );

        void *obj = malloc( obj_size );
        memcpy( obj, buf + obj_size * nat->entry[i].obj , obj_size );
        obj_cache[obj_id] = obj;
        
        return obj_cache[obj_id];
    }
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