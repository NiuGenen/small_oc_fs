#include "extent_list.h"
#include "extent_stub.h"
#include<iostream>

ExtentList::ExtentList(
    int ch,
    uint64_t blk_st, uint64_t blk_ed,
    int ext_size,
    blk_addr_handle* handler)
{
    std::cout << "ExtentList()" << std::endl;
    this->ch = ch;
    this->blk_st = blk_st;
    this->blk_ed = blk_ed;
    this->ext_size = ext_size;
    this->handler = handler;
    this->blk_nr = handler->GetBlkNr();

    init();
}

ExtentList::~ExtentList()
{

}

void ExtentList::init()
{
    std::cout << "init()" << std::endl;
    head_eobj_id = ext_alloc_obj_id();
    struct ext_node* eobj = (struct ext_node*) ext_read_by_obj_id( head_eobj_id );

    eobj->ecount = 1;

    eobj->mobjs[0].free_vblk_num = blk_nr / ext_size;
   
    eobj->exts[0].addr_st_buf = this->blk_st;
    eobj->exts[0].addr_ed_buf = this->blk_ed;
    eobj->exts[0].free_bitmap = 0;
    eobj->exts[0].junk_bitmap = 0;

    eobj->prev = 0;
    eobj->next = 0;

    ext_write_by_obj_id( head_eobj_id, eobj);
}

struct extent_descriptor* ExtentList::getNewExt()
{
    Ext_Node_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) ext_read_by_obj_id( eobj_id );
    struct extent_descriptor* edes = nullptr;

_START_EOBJ_:
    for(int i = 0; i < eobj->ecount; ++i){
        if( eobj->mobjs[i].free_vblk_num > 0){// ok to getNewExt
            edes = (struct extent_descriptor*)malloc( EXT_DES_SIZE );
            if( eobj->mobjs[i].free_vblk_num == 1){// no split extent
                eobj->mobjs[i].free_vblk_num = 0;

                edes->addr_st_buf = eobj->exts[i].addr_st_buf;
                edes->addr_ed_buf = eobj->exts[i].addr_ed_buf;
                edes->use_bitmap = 0;
                break;
            }
            else{// split extent
                eobj->mobjs[i].free_vblk_num -= 1;
                
                edes->addr_st_buf = eobj->exts[i].addr_st_buf;
                handler->BlkAddrAdd( ext_size, &(eobj->exts[i].addr_st_buf) );
                edes->addr_ed_buf = eobj->exts[i].addr_st_buf;
                edes->use_bitmap = 0;

                for(int j = eobj->ecount - 1; j>=i; --j){
                    eobj->mobjs[j+1] = eobj->mobjs[j];
                    eobj->exts[j+1] = eobj->exts[j];
                }
                eobj->mobjs[i].free_vblk_num = 0;
                eobj->exts[i].addr_st_buf = edes->addr_st_buf;
                eobj->exts[i].addr_ed_buf = edes->addr_ed_buf;
                eobj->exts[i].free_bitmap = 0;
                eobj->exts[i].junk_bitmap = 0;
                eobj->ecount += 1;

                // [ 0 , 2 * H - 1 ]
                // => [ 0 , H - 1 ] + [ H , 2 * H - 1 ]
                //        { eobj }       { n_eobj }
                if( eobj->ecount >= Ext_Node_Degree ){
                    Ext_Node_ID_Type n_eobj_id = ext_alloc_obj_id();
                    struct ext_node* n_eobj = (struct ext_node*) ext_read_by_obj_id( n_eobj_id );

                    for(int j = 0; j < Ext_Node_Half_Degree; ++j){
                        n_eobj->mobjs[j] = eobj->mobjs[ Ext_Node_Half_Degree + j];
                        n_eobj->exts[j] = eobj->exts[ Ext_Node_Half_Degree + j];
                    }
                    n_eobj->ecount = Ext_Node_Half_Degree;
                    eobj->ecount = Ext_Node_Half_Degree;

                    n_eobj->next = eobj->next;
                    eobj->next = n_eobj_id;
                    n_eobj->prev = eobj_id;

                    ext_write_by_obj_id( n_eobj_id, n_eobj );
                    ext_write_by_obj_id( eobj_id, eobj);
                }
                break;
            }
        }
    }

    if( edes != nullptr ){
        return edes;
    }
    if( eobj->next != 0 ){
        eobj_id = eobj->next;
        eobj = (struct ext_node*) ext_read_by_obj_id( eobj_id );
        goto _START_EOBJ_;
    }
    return nullptr;
}

void ExtentList::putExt(struct extent_descriptor* edes)
{
    Ext_Node_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) ext_read_by_obj_id( eobj_id );

    // eobj.st <= edes.st < edes.ed <= eobj.ed
    while(!(
        handler->BlkAddrCmp( &(eobj->exts[0].addr_st_buf), &(edes->addr_st_buf) )<=0 &&
        handler->BlkAddrCmp( &(edes->addr_ed_buf), &(eobj->exts[ eobj->ecount - 1].addr_ed_buf) ) <=0
        ))
    {
        eobj_id = eobj->next;
        if( eobj_id == 0 ) break;
        eobj = (struct ext_node*) ext_read_by_obj_id( eobj_id );
    }

    if( eobj_id == 0 ) return;

    int index = -1;
    for(int i = 0; i < eobj->ecount; ++i){
        if( handler->BlkAddrCmp( &(eobj->exts[i].addr_st_buf), &(edes->addr_st_buf) )==0){
            index = i;
            break;
        }
    }

    if( index >= 0 ){
        eobj->exts[ index ].free_bitmap = edes->use_bitmap;
        eobj->exts[ index ].junk_bitmap = edes->junk_bitmap; // mark to erase
    }
}

void ExtentList::GC()
{
    Ext_Node_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) ext_read_by_obj_id( eobj_id );
    Ext_Node_ID_Type eobj_prev_id = 0;
    struct ext_node* eobj_prev = nullptr;
    Ext_Node_ID_Type eobj_next_id = eobj->next;
    struct ext_node* eobj_next = (struct ext_node*) ext_read_by_obj_id( eobj_next_id );

    while( eobj != nullptr ){
        std::cout<<"GC() erase eobj{"<<eobj_id<<"}"<<std::endl;
        // find extent with junk block
        for(int i=0; i<eobj->ecount; ++i){
            if( eobj->exts[i].junk_bitmap != 0 ){// need to erase
                // erase junk block
                eobj->mobjs[i].free_vblk_num = 1;
                eobj->exts[i].free_bitmap = 0;
                eobj->exts[i].junk_bitmap = 0;
                // merge extent
                if( i>0 && eobj->mobjs[i-1].free_vblk_num > 0 ){// merge [i-1] & [i]
                    eobj->mobjs[i-1].free_vblk_num += 1;
                    eobj->exts[i-1].addr_ed_buf = eobj->exts[i].addr_ed_buf;
                    for(int j=i; j<eobj->ecount-1; ++j){
                        eobj->mobjs[j] = eobj->mobjs[j+1];
                        eobj->exts[j] = eobj->exts[j+1];
                    }
                    eobj->ecount -= 1;
                    i -= 1;
                }
                else if( i<eobj->ecount-1 && eobj->mobjs[i+1].free_vblk_num > 0){// merge [i] & [i+1]
                    eobj->mobjs[i].free_vblk_num += eobj->mobjs[i+1].free_vblk_num;
                    eobj->exts[i].addr_ed_buf = eobj->exts[i+1].addr_ed_buf;
                    for(int j=i+1; j<eobj->ecount; ++j){
                        eobj->mobjs[j] = eobj->mobjs[j+1];
                        eobj->exts[j] = eobj->exts[j+1];
                    }
                    eobj->ecount -= 1;
                }
            }
        }
        // if this eobj is less than Ext_Node_Half_Degree
        // borrow or merge with adjacent node
        int if_merge_with_next = 0;
        if( eobj->ecount < Ext_Node_Half_Degree ){
            std::cout<<"GC() merge or borrow"<<std::endl;
            do{
                // borrow from prev
                if( eobj_prev != nullptr && eobj_prev->ecount > Ext_Node_Half_Degree ){
                    for(int i=eobj->ecount-1; i>=0; --i){
                        eobj->mobjs[i+1] = eobj->mobjs[i];
                        eobj->exts[i+1] = eobj->exts[i];
                    }
                    eobj->mobjs[0] = eobj_prev->mobjs[ eobj_prev->ecount-1 ];
                    eobj->mobjs[0] = eobj_prev->mobjs[ eobj_prev->ecount-1 ];
                    eobj->ecount += 1;
                    eobj_prev->ecount -= 1;
                    break;
                }
                // merge with prev
                if( eobj_prev != nullptr && eobj_prev->ecount == Ext_Node_Half_Degree ){
                    for(int i=0; i<eobj->ecount; ++i){
                        eobj_prev->mobjs[ eobj_prev->ecount + i ] = eobj->mobjs[i];
                        eobj_prev->exts[ eobj_prev->ecount + i ] = eobj->exts[i];
                    }
                    eobj_prev->ecount += eobj->ecount;  // modify prev
                    eobj_prev->next = eobj_next_id;
                    eobj_next->prev = eobj_prev_id;     // modify next
                    ext_dealloc_obj_id( eobj_id );      // delete eobj
                    eobj = eobj_next;                   // move next to eobj
                    eobj_id = eobj_next_id;
                    eobj_next_id = eobj->next;          // get new next
                    eobj_next = (struct ext_node*) ext_read_by_obj_id( eobj->next );
                    break;
                }
                // borrow from next
                if( eobj_next != nullptr && eobj_next->ecount > Ext_Node_Half_Degree ){
                    eobj->mobjs[ eobj->ecount ] = eobj_next->mobjs[0];
                    eobj->exts[ eobj->ecount ] = eobj_next->exts[0];
                    eobj->ecount += 1;
                    for(int j=0; j<eobj_next->ecount-1; ++j){
                        eobj_next->mobjs[j] = eobj_next->mobjs[j+1];
                        eobj_next->exts[j] = eobj_next->exts[j+1];
                    }
                    eobj_next->ecount -= 1;
                    break;
                }
                // merge with next
                if( eobj_next != nullptr && eobj_next->ecount == Ext_Node_Half_Degree ){
                    for(int i=0; i<eobj_next->ecount; ++i){
                        eobj->mobjs[ eobj->ecount + i ] = eobj_next->mobjs[i];
                        eobj->exts[ eobj->ecount + i ] = eobj_next->exts[i];
                    }
                    eobj->ecount += eobj_next->ecount;  // modify eobj
                    eobj->next = eobj_next->next;
                    ext_dealloc_obj_id( eobj_next_id ); // delete old next
                    eobj_next_id = eobj->next;     // get new next id
                    eobj_next = (struct ext_node*) ext_read_by_obj_id( eobj_next_id ); // get new next
                    eobj_next->prev = eobj_id;          // modify next
                    if_merge_with_next = 1;
                    break;
                }
            }while(0);
        }

        if( if_merge_with_next ){
            if_merge_with_next = 0;
            continue;
        }

        if( eobj_prev != nullptr )
            ext_write_by_obj_id( eobj_prev_id, eobj_prev );
        eobj_prev = eobj;
        eobj_prev_id = eobj_id;

        eobj = eobj_next;
        eobj_id = eobj_next_id;
        
        if( eobj != nullptr){
            eobj_next_id = eobj->next;
            eobj_next = (struct ext_node*) ext_read_by_obj_id( eobj->next );
        }
    }
    ext_write_by_obj_id( eobj_id , eobj );
}

void ExtentList::display()
{
    
    Ext_Node_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) ext_read_by_obj_id( eobj_id );

    std::cout << "**********" << std::endl;
    while( eobj != nullptr ){
        std::cout 
            << "==========" << std::endl
            << "eobj{ " << eobj_id << " }.next = " << eobj->next << std::endl;
        for(int i = 0; i < eobj->ecount; ++i){
            std::cout
                << "----------" << std::endl
                << "     free = " << eobj->mobjs[i].free_vblk_num // << std::endl
                << "     st   = " << eobj->exts[i].addr_st_buf <<" ; ed   = " << eobj->exts[i].addr_ed_buf// << std::endl
                << "     use  = " << eobj->exts[i].free_bitmap <<" ; junk = " << eobj->exts[i].junk_bitmap << std::endl;
        }

        eobj_id = eobj->next;
        eobj = (struct ext_node*) ext_read_by_obj_id( eobj_id );
    }
}
