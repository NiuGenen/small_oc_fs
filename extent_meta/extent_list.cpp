#include "extent_list.h"
//#include "extent_stub.h"
#include<iostream>
#include "../dbg_info.h"

extern BlkAddrHandle* occsd_bah;
extern MetaBlkArea *mba_extent;

ExtentList::ExtentList(
    Nat_Obj_ID_Type root_id,
    int ch,
    uint64_t blk_st, uint64_t blk_ed,
    int ext_size,
    blk_addr_handle* handler)
{
    OCSSD_DBG_INFO( this, "Constructor ExtentList for ch " << ch);

    this->ch = ch;
    this->blk_st = blk_st;
    this->blk_ed = blk_ed;
    this->ext_size = ext_size;
    this->handler = handler;

    init(root_id);
}

ExtentList::~ExtentList()
{

}

void ExtentList::init(Nat_Obj_ID_Type root_id)
{
    OCSSD_DBG_INFO( this, "Init Extent with root " << root_id );

    if( root_id == 0 ) {
        head_eobj_id = mba_extent->alloc_obj();
        struct ext_node *eobj = (struct ext_node *) mba_extent->read_by_obj_id(head_eobj_id);

        eobj->ecount = 1;

        eobj->exts[0].addr_st_buf = this->blk_st;
        eobj->exts[0].addr_ed_buf = this->blk_ed;
        eobj->exts[0].free_bitmap = 0;
        eobj->exts[0].junk_bitmap = 0;

        struct blk_addr blk_addr_;
        blk_addr_.__buf = this->blk_st;
        eobj->mobjs[0].free_vblk_num = handler->BlkAddrDiff(&blk_addr_, 1 ) / ext_size;

        eobj->prev = 0;
        eobj->next = 0;

        mba_extent->write_by_obj_id(head_eobj_id, eobj);
    }else{
        head_eobj_id = root_id;
    }
}

struct extent_descriptor* ExtentList::getNewExt()
{
    Nat_Obj_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) mba_extent->read_by_obj_id( eobj_id );
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
                struct blk_addr blk_addr_;
                blk_addr_.__buf = eobj->exts[i].addr_st_buf;
                handler->BlkAddrAdd( ext_size, &(blk_addr_) );
                eobj->exts[i].addr_st_buf = blk_addr_.__buf;
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
                    Nat_Obj_ID_Type n_eobj_id = mba_extent->alloc_obj();
                    struct ext_node* n_eobj = (struct ext_node*) mba_extent->read_by_obj_id( n_eobj_id );

                    for(int j = 0; j < Ext_Node_Half_Degree; ++j){
                        n_eobj->mobjs[j] = eobj->mobjs[ Ext_Node_Half_Degree + j];
                        n_eobj->exts[j] = eobj->exts[ Ext_Node_Half_Degree + j];
                    }
                    n_eobj->ecount = Ext_Node_Half_Degree;
                    eobj->ecount = Ext_Node_Half_Degree;

                    n_eobj->next = eobj->next;
                    eobj->next = n_eobj_id;
                    n_eobj->prev = eobj_id;

                    mba_extent->write_by_obj_id( n_eobj_id, n_eobj );
                    mba_extent->write_by_obj_id( eobj_id, eobj);
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
        eobj = (struct ext_node*) mba_extent->read_by_obj_id( eobj_id );
        goto _START_EOBJ_;
    }
    return nullptr;
}

void ExtentList::putExt(struct extent_descriptor* edes)
{
    Nat_Obj_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) mba_extent->read_by_obj_id( eobj_id );

    // eobj.st <= edes.st < edes.ed <= eobj.ed
    struct blk_addr eobj_st, eobj_ed, edes_st, edes_ed;
    eobj_st.__buf = eobj->exts[0].addr_st_buf;
    eobj_ed.__buf = eobj->exts[ eobj->ecount - 1].addr_ed_buf;
    edes_st.__buf = edes->addr_st_buf;
    edes_ed.__buf = edes->addr_ed_buf;
    while(!(
        handler->BlkAddrCmp( &(eobj_st), &(edes_st) )<=0 &&
        handler->BlkAddrCmp( &(edes_ed), &(eobj_ed) ) <=0
        ))
    {
        eobj_id = eobj->next;
        if( eobj_id == 0 ) break;
        eobj = (struct ext_node*) mba_extent->read_by_obj_id( eobj_id );
        eobj_st.__buf = eobj->exts[0].addr_st_buf;
        eobj_ed.__buf = eobj->exts[ eobj->ecount - 1].addr_ed_buf;
    }

    if( eobj_id == 0 ) return;

    int index = -1;
    for(int i = 0; i < eobj->ecount; ++i){
        eobj_st.__buf = eobj->exts[ i ].addr_st_buf;
        if( handler->BlkAddrCmp( &(eobj_st), &(edes_st) )==0){
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
    Nat_Obj_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) mba_extent->read_by_obj_id( eobj_id );
    Nat_Obj_ID_Type eobj_prev_id = 0;
    struct ext_node* eobj_prev = nullptr;
    Nat_Obj_ID_Type eobj_next_id = eobj->next;
    struct ext_node* eobj_next = (struct ext_node*) mba_extent->read_by_obj_id( eobj_next_id );

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
                    mba_extent->de_alloc_obj( eobj_id );      // delete eobj
                    eobj = eobj_next;                   // move next to eobj
                    eobj_id = eobj_next_id;
                    eobj_next_id = eobj->next;          // get new next
                    eobj_next = (struct ext_node*) mba_extent->read_by_obj_id( eobj->next );
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
                    mba_extent->de_alloc_obj( eobj_next_id ); // delete old next
                    eobj_next_id = eobj->next;     // get new next id
                    eobj_next = (struct ext_node*) mba_extent->read_by_obj_id( eobj_next_id ); // get new next
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
            mba_extent->write_by_obj_id( eobj_prev_id, eobj_prev );
        eobj_prev = eobj;
        eobj_prev_id = eobj_id;

        eobj = eobj_next;
        eobj_id = eobj_next_id;
        
        if( eobj != nullptr){
            eobj_next_id = eobj->next;
            eobj_next = (struct ext_node*) mba_extent->read_by_obj_id( eobj->next );
        }
    }
    mba_extent->write_by_obj_id( eobj_id , eobj );
}

void ExtentList::display()
{
    
    Nat_Obj_ID_Type eobj_id = head_eobj_id;
    struct ext_node* eobj = (struct ext_node*) mba_extent->read_by_obj_id( eobj_id );

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
        eobj = (struct ext_node*) mba_extent->read_by_obj_id( eobj_id );
    }
}

std::string ExtentList::txt()
{
    return "ExtentList";
}
