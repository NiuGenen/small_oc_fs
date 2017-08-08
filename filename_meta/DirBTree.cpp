#include <iostream>
#include "DirBTree.h"
//#include "DirBTree_stub.h"

extern MetaBlkArea *mba_file_name;

DirBTree::~DirBTree(){
}

DirBTree::DirBTree(Nat_Obj_ID_Type root_id)
{
    init( root_id );
}

// alloc root_dir_node_id & root->is_leaf = 1
void DirBTree::init(Nat_Obj_ID_Type root_id){
    if( root_id == 0 ){ // need to build up a new DirBTree
        root_dir_node_id = mba_file_name->alloc_obj();
        struct dir_meta_obj* root = new struct dir_meta_obj;
        root->obj_id = root_dir_node_id;
        root->is_leaf = 1;
        mba_file_name->write_by_obj_id(root_dir_node_id, root);
    }else{
        root_dir_node_id = root_id;
    }
}

// insert pair : fdes -> mobj_id
// add_new_file()
// insert_not_full()
// split_child()

bool DirBTree::add_new_file(struct file_descriptor fdes, Nat_Obj_ID_Type mobj_id)
{
    struct dir_meta_obj* dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( root_dir_node_id );
    if( dobj == nullptr ){
        //throw exception
        return false;
    }

    if( dobj-> fcount >= Dir_Node_Degree ){ // split node
        Nat_Obj_ID_Type father_id = mba_file_name->alloc_obj();
        struct dir_meta_obj* father_obj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( father_id );
        father_obj->fcount = 0;
        father_obj->is_leaf = 0;
        father_obj->cobj_id[0] = root_dir_node_id;
        split_child( father_obj, father_id , 0, dobj, root_dir_node_id);
        root_dir_node_id = father_id;
        dobj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( root_dir_node_id );
        insert_not_full( dobj, root_dir_node_id, fdes, mobj_id );
    }
    else{
        insert_not_full(dobj, root_dir_node_id, fdes, mobj_id );
    }

    return true;
}

// insert @fdes & @mobj_id into @dobj
// require @dobj is not full
void DirBTree::insert_not_full(struct dir_meta_obj* dobj,
        Nat_Obj_ID_Type dobj_id,
        struct file_descriptor fdes,
        Nat_Obj_ID_Type mobj_id)
{
    int i = dobj->fcount - 1; // last index of dobj->fdes[]

    if( dobj->is_leaf ){ // dobj is leaf node
        while(i >= 0 && fdes.fhash < dobj->fdes[i].fhash ){//move to empty [i]
            dobj->fdes[i+1] = dobj->fdes[i];
            dobj->mobj_id[i+1] = dobj->mobj_id[i];
            i--;
        }
        dobj->fdes[i+1] = fdes; // insert directly
        dobj->mobj_id[i+1] = mobj_id;
        dobj->fcount++;
        mba_file_name->write_by_obj_id(dobj_id, dobj); // dump by meta_block_area
    }
    else{ // not leaf node
        while( i >= 0 && fdes.fhash < dobj->fdes[i].fhash ) i--;
        i++;// find @i where fdes[i-1] < fdes < fdes[i]
        // fdes should be inserted into cobj[i]

        Nat_Obj_ID_Type cobj_id = dobj->cobj_id[i];
        struct dir_meta_obj* cobj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( cobj_id ); // get child obj
        if( cobj == nullptr ){
            //throw
        }

        //  fdes =   i-1,  i, i+1
        //           /    /   / \
        //  cobj = i-1, [i], i+1, i+2
        if( cobj->fcount >= Dir_Node_Degree ){ // if child obj is full
            split_child(dobj, dobj_id, i, cobj, cobj_id);
            //      fdes =   i-1, 'i' , i+1,  i+2
            // =>            /    /     /     /
            //      cobj = i-1, 'i',  'i+1', i+2
            if( fdes.fhash > dobj->fdes[i].fhash ){
                i++;
            }

            cobj_id = dobj->cobj_id[i];
            cobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( cobj_id ); // get new child obj
            if( cobj == nullptr ){
                //throw
            }
        }

        insert_not_full( cobj, cobj_id, fdes, mobj_id); // insert into child
    }
}

// split @dobj_parent->cobj[ index ]
//  fdes =   i-1,  i, i+1            fdes =   i-1, 'i' , i+1,  i+2
//           /    /   / \      ==>            /    /     /     /
//  cobj = i-1, [i], i+1, i+2        cobj = i-1, 'i',  'i+1', i+2
//  AND write them
void DirBTree::split_child(struct dir_meta_obj* dobj_parent,
    Nat_Obj_ID_Type dobj_parent_id,
    int index,
    struct dir_meta_obj* dobj_child,
    Nat_Obj_ID_Type dobj_child_id)
{
    //assert dobj_parent->cobj_id[index] == dobj_child_id

    // alloc new child dobj
    Nat_Obj_ID_Type n_dobj_child_id = mba_file_name->alloc_obj();
    struct dir_meta_obj* n_dobj_child = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( n_dobj_child_id );
    if( n_dobj_child == nullptr ){
        //throw
    }

    // child[0, half - 1] < child[ half ] < child[ half + 1, 2 * half ]
    // 
    // n_child [ 0, half - 1 ] = child [ half + 1, 2 * half ]
    // n_child size = half
    n_dobj_child->is_leaf = dobj_child->is_leaf;
    n_dobj_child->fcount = Dir_Node_Half_Degree;
    for(int i = 0; i < Dir_Node_Half_Degree; ++i){
        n_dobj_child->fdes[ i ] = dobj_child->fdes[ i + Dir_Node_Half_Degree + 1 ];
        n_dobj_child->mobj_id[ i ] = dobj_child->mobj_id[ i + Dir_Node_Half_Degree + 1 ];
    }
    if( !dobj_child->is_leaf ){
        for(int i = 0; i <= Dir_Node_Half_Degree; ++i){
            n_dobj_child->cobj_id[ i ] = dobj_child->cobj_id[ i + Dir_Node_Half_Degree + 1 ];
        }
    }
    // child[0, half - 1]
    dobj_child->fcount = Dir_Node_Half_Degree;

    // move parent->fdes [ index , fcount - 1 ] one step back
    for(int i = dobj_parent->fcount - 1; i >= index ; --i){
        dobj_parent->fdes[ i + 1 ] = dobj_parent->fdes[ i ];
        dobj_parent->mobj_id[ i + 1 ] = dobj_parent->mobj_id[ i ];
    }
    dobj_parent->fdes[index] = dobj_child->fdes[ Dir_Node_Half_Degree ];
    dobj_parent->mobj_id[index] = dobj_child->mobj_id[ Dir_Node_Half_Degree ];

    // move parent->cobj_id [ index + 1, fcount ] one step back
    for(int i = dobj_parent->fcount; i > index; i--){
        dobj_parent->cobj_id[ i + 1 ] = dobj_parent->cobj_id[ i ];
    }
    dobj_parent->cobj_id[ index + 1 ] = n_dobj_child_id;

    dobj_parent->fcount++;
}





// search by fdes
// return mobj_id

#define DirBTreeNotFoundReturn 0

Nat_Obj_ID_Type  DirBTree::get_file_meta_obj_id(
    struct file_descriptor fdes)
{
    Nat_Obj_ID_Type dobj_id = root_dir_node_id;
    struct dir_meta_obj* dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( root_dir_node_id );
    if( dobj == nullptr ){
        //throw
    }

DirBTree_get_restart:
    for( int i = 0; i < dobj->fcount; ++i){
        if( dobj->fdes[i].fhash == fdes.fhash ){
            return dobj->mobj_id[i];
        }
        if( dobj->fdes[i].fhash > fdes.fhash ){
            if( dobj->is_leaf ){
                // not found
                return DirBTreeNotFoundReturn;
            }
            else{
                dobj_id = dobj->cobj_id[i];
                dobj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( dobj_id );
                goto DirBTree_get_restart;
            }
        }
    }
    if( !dobj->is_leaf ){
        dobj_id = dobj->cobj_id[ dobj->fcount ];
        dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( dobj_id );
        goto DirBTree_get_restart;
    }

    //not found
    return DirBTreeNotFoundReturn;
}





// delete pair : fdes -> mobj_id
// according to fdes
// delete_not_half()
// union_child()

bool DirBTree::del_file(struct file_descriptor fdes)
{
    struct dir_meta_obj* dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( root_dir_node_id );
    if( dobj == nullptr ){
        return false;
    }
    Nat_Obj_ID_Type dobj_id = root_dir_node_id;

    delete_not_half( dobj, dobj_id, fdes);
    return true;
}

void DirBTree::delete_not_half(struct dir_meta_obj* dobj,
        Nat_Obj_ID_Type dobj_id,
        struct file_descriptor fdes)
{
    int index = 0;
    while(index < dobj->fcount && fdes.fhash > dobj->fdes[index].fhash){
        index++;
    }// index : fdes.fhash <= dobj->fdes[index].fhash

    // leaf node
    // delete it directly
    if( dobj->is_leaf ){
        if( index < dobj->fcount && fdes.fhash == dobj->fdes[index].fhash ){
            for(int i = index; i < dobj->fcount - 1; ++i){
                dobj->fdes[i] = dobj->fdes[i+1];
                dobj->mobj_id[i] = dobj->mobj_id[i+1];
            }
            dobj->fcount--;
            mba_file_name->write_by_obj_id( dobj_id, dobj );
            return ;//directly delete
        }
        else{
            // not found this fdes
            return ;
        }
    }
    // dobj has child
    // fdes.fhash == dobj->fdes[index].fhash
    // maybe : union_child
    else if( index < dobj->fcount && fdes.fhash == dobj->fdes[index].fhash ){
        // move left_child[max] to parent[index]
        // delete left_child[max] // NOT left_child[max] BUT MAX() of left_child
        Nat_Obj_ID_Type left_child_obj_id = dobj->cobj_id[ index ];
        struct dir_meta_obj* left_child_obj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( left_child_obj_id );
        if( left_child_obj->fcount > Dir_Node_Half_Degree ){
            //dobj->fdes[index] = left_child_obj->fdes[ left_child_obj->fcount - 1 ];
            //dobj->mobj_id[index] = left_child_obj->mobj_id[ left_child_obj->fcount - 1];
            max_of_dobj( left_child_obj, left_child_obj_id,
                &(dobj->fdes[index]), &(dobj->mobj_id[index]) );
            mba_file_name->write_by_obj_id( dobj_id, dobj );  // write parent
            delete_not_half( left_child_obj, left_child_obj_id, dobj->fdes[ index ] );
            return;
        }
        // move right_child[min] to parent[index]
        // delete right_child[min] // NOT [min] BUT MIN() of right_child
        Nat_Obj_ID_Type right_child_obj_id = dobj->cobj_id[ index + 1 ];
        struct dir_meta_obj* right_child_obj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( right_child_obj_id );
        if( right_child_obj->fcount > Dir_Node_Half_Degree ){
            //dobj->fdes[index] = right_child_obj->fdes[ 0 ];
            //dobj->mobj_id[index] = right_child_obj->mobj_id[ 0 ];
            min_of_dobj( right_child_obj, right_child_obj_id,
                &(dobj->fdes[index]), &(dobj->mobj_id[index]) );
            mba_file_name->write_by_obj_id( dobj_id, dobj );
            delete_not_half( right_child_obj, right_child_obj_id, dobj->fdes[ index ] );
            return;
        }
        //
        // both child->fcount == Dir_Node_Half_Degree
        // need to union child & delete parent[index]
        //                                ┌---------------------------┐
        // { dobj }                       ↑                           ↓
        //   fdes =  index-1   index   index+1          index-1    'index'
        //                \    /   \   /    \      ==>        \   /       \
        //   cobj =       index  index+1  index+2            index        'index+1'
        //              { left } { right }   ↓          { left + right }      ↑
        //                                   |        { left } += { right }   |
        //                                   |                                |
        //                                   └--------------------------------┘
        union_child(dobj, dobj_id,
            left_child_obj, left_child_obj_id,   // left id  == dobj->cobj_id[ index ]
            right_child_obj, right_child_obj_id, // right id == dobj->cobj_id[ index + 1 ]
            index);
        // after union_child()
        // the dobj->fdes[index] will be added to dobj->cobj_id[ index ]
        // so we need to delete from cobj[ index ]
        delete_not_half( left_child_obj, left_child_obj_id, fdes );
    }
    // situation 1. index == dobj->fcount
    //              which means fdes > all fdes in dobj
    // situation 2. dobj->fdes[ index - 1 ] < fdes < dobj->fdes[ index ]
    // in both situation, we need to delete it from dobj->cobj_id[ index ]
    else{
        Nat_Obj_ID_Type cobj_id = dobj->cobj_id[ index ];
        struct dir_meta_obj * cobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( cobj_id );

        if( cobj->fcount > Dir_Node_Half_Degree ){// delete from cobj 
            delete_not_half( cobj, cobj_id, fdes );
        }
        // cobj->fcount == Dir_Node_Half_Degree
        // index ∈ [ 0, dobj->fcount ]
        else{
            Nat_Obj_ID_Type left_brother_cobj_id = 0;
            struct dir_meta_obj* left_brother_cobj = nullptr;
            Nat_Obj_ID_Type right_brother_cobj_id = 0;
            struct dir_meta_obj* right_brother_cobj = nullptr;
            // borrow one from its left brother
            if( index > 0 ){
                left_brother_cobj_id = dobj->cobj_id[ index - 1 ];
                left_brother_cobj =(struct dir_meta_obj*) mba_file_name->read_by_obj_id( left_brother_cobj_id );
                if( left_brother_cobj->fcount > Dir_Node_Half_Degree ){// OK to brorrow
                    // brorrow from left brother and delete from child
                    for(int i = cobj->fcount - 1; i >= 0; --i){
                        cobj->fdes[ i + 1 ]    = cobj->fdes[ i ];
                        cobj->mobj_id[ i + 1 ] = cobj->mobj_id[ i ];
                    }
                    for(int i = cobj->fcount; i >= 0; --i){
                        cobj->cobj_id[ i + 1 ] = cobj->cobj_id[ i ];
                    }

                    struct file_descriptor dobj_saved_fdes   = dobj->fdes[ index - 1 ];
                    Nat_Obj_ID_Type dobj_saved_mobj_id = dobj->mobj_id[ index - 1 ];

                    dobj->fdes[ index - 1 ]    = left_brother_cobj->fdes[ left_brother_cobj->fcount - 1];
                    dobj->mobj_id[ index - 1 ] = left_brother_cobj->mobj_id[ left_brother_cobj->fcount - 1];

                    cobj->fdes[ 0 ]    = dobj_saved_fdes;
                    cobj->mobj_id[ 0 ] = dobj_saved_mobj_id;
                    cobj->cobj_id[ 0 ] = left_brother_cobj->cobj_id[ left_brother_cobj->fcount];
                    cobj->fcount += 1;

                    left_brother_cobj->fcount -= 1;

                    mba_file_name->write_by_obj_id( left_brother_cobj_id, left_brother_cobj );
                    mba_file_name->write_by_obj_id( dobj_id, dobj );
                    delete_not_half( cobj, cobj_id, fdes );
                    return;
                }
            }
            // borrow one from its right brother
            if( index < dobj->fcount ){
                right_brother_cobj_id = dobj->cobj_id[ index + 1 ];
                right_brother_cobj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( right_brother_cobj_id );
                if( right_brother_cobj->fcount > Dir_Node_Half_Degree ){// OK to brorrow
                    // brorrow from left brother and delete
                    struct file_descriptor dobj_saved_fdes   = dobj->fdes[ index ];
                    Nat_Obj_ID_Type dobj_saved_mobj_id = dobj->mobj_id[ index ];

                    dobj->fdes[ index ]    = right_brother_cobj->fdes[ 0 ];
                    dobj->mobj_id[ index ] = right_brother_cobj->mobj_id[ 0 ];

                    cobj->fdes[ cobj->fcount ]    = dobj_saved_fdes;
                    cobj->mobj_id[ cobj->fcount ] = dobj_saved_mobj_id;
                    cobj->cobj_id[ cobj->fcount + 1 ] = right_brother_cobj->cobj_id[ 0 ];
                    cobj->fcount += 1;

                    for(int i = 0; i < right_brother_cobj->fcount - 1; ++i){
                        right_brother_cobj->fdes[ i ]    = right_brother_cobj->fdes[ i + 1 ];
                        right_brother_cobj->mobj_id[ i ] = right_brother_cobj->mobj_id[ i + 1 ];
                    }
                    for(int i = 0; i < right_brother_cobj->fcount; ++i){
                        right_brother_cobj->cobj_id[ i ] = right_brother_cobj->cobj_id[ i + 1 ];
                    }
                    right_brother_cobj->fcount -= 1;

                    mba_file_name->write_by_obj_id( right_brother_cobj_id, right_brother_cobj );
                    mba_file_name->write_by_obj_id( dobj_id, dobj );
                    delete_not_half( cobj, cobj_id, fdes );
                    return;
                }
            }
            // both left and right brother cannot brorrow
            // then union with one brother and delete
            // limited by union_child()
            //     1. if index ∈ [ 0 , dobj->fcount - 1 )
            //        then 1) union cobj with right
            //             2) write parent
            //             3) dealloc right 
            //             4) delete fdes from cobj
            //     2. if index == dobj->fcount
            //        then 1) union left with cobj 
            //             2) write parent
            //             3) dealloc cobj
            //             4) delete from left
            if( index == dobj->fcount ){
                // assert left_brother_cobj != nullptr
                union_child(dobj, dobj_id,
                    left_brother_cobj, left_brother_cobj_id,
                    cobj, cobj_id,
                    index - 1);
                delete_not_half( left_brother_cobj, left_brother_cobj_id, fdes );
            }
            else{ // index ∈ [ 0 , dobj->fcount - 1 )
                // assert right_brother_cobj != nullptr
                union_child(dobj, dobj_id,
                    cobj, cobj_id,
                    right_brother_cobj, right_brother_cobj_id,
                    index);
                delete_not_half( cobj, cobj_id, fdes );
            }
        }
    }
}

// only used when deleting by fdes
// only used when left & right 's fcount == Dir_Node_Half_Degree
//
//                                  ┌---------------------------┐
// { parent }                       ↑                           ↓
//   fdes =  index-1   index   index+1            index-1    'index'
//                \    /   \   /    \      ==>          \   /       \
//   cobj =       index  index+1  index+2              index        'index+1'
//              { left } { right }   ↓            { left + right }      ↑
//                                   |  1.{ left } += { parent }[index] |
//                                   |  2.{ left } += { right }         |
//                                   └----------------------------------┘
//
// step 1. move parent->fdes[ index ] into left_child & consist parent
// step 2. move right->fdes[ all ]    into left_child
// after 1 & 2 : assert left_child->fcount == Dir_Node_Degree + 1
// step 3. write parent
// step 4. dealloc right
// step 5. if parent->fcount == 0 : change root
// step 6. return left_child ===> for delete by fdes
// NO need setp 6 : useless without its id
void DirBTree::union_child(
    struct dir_meta_obj* dobj_parent, Nat_Obj_ID_Type dobj_parent_id,
    struct dir_meta_obj* dobj_left,  Nat_Obj_ID_Type dobj_left_id,    // left_id  == parent->cobj_id[ index ]
    struct dir_meta_obj* dobj_right, Nat_Obj_ID_Type dobj_right_id,   // right_id == parent-<cobj_id[ index+1 ]
    int index)
{
    // assert dobj_left_id  == dobj_parent->cobj_id[ index ]
    // assert dobj_right_id == dobj_parent->cobj_id[ index + 1 ]
    // assert dobj_left->fcount  == Dir_Node_Half_Degree
    // assert dobj_right->fcount == Dir_Node_Half_Degree

    // step 1. move parent->fdes[ index ] into left_child & consist parent
    // { parent }
    //   fdes =    index-1 index index+1
    //                      ↓
    //                      └------------------------------┐
    // { left }                                            ↓
    //   fdes =     fcount-1                 fcount-1   'fcount'
    //              /      \        ==>      /      \    /    \
    //   cobj = fcount-1  fcount         fcount-1  fcount    nullptr 
    //
    dobj_left->fdes[ dobj_left->fcount ]    = dobj_parent->fdes[ index ];
    dobj_left->mobj_id[ dobj_left->fcount ] = dobj_parent->mobj_id[ index ];
    for(int i = index; i < dobj_parent->fcount - 1; ++i){
        dobj_parent->fdes[ i ]    = dobj_parent->fdes[ i + 1];
        dobj_parent->mobj_id[ i ] = dobj_parent->mobj_id[ i + 1];
    }
    for(int i = index + 1; i < dobj_parent->fcount; ++i){
        dobj_parent->cobj_id[ i ] = dobj_parent->cobj_id[ i + 1];
    }
    dobj_parent->fcount -= 1;

    // step 2. move right->fdes[ all ]    into left_child
    //                                    ┌---------------------------┐
    //  { left }                          ↓             { right }     ↑
    //   fdes =   fcount-1   fcount   'fcount+1'          fdes =     [0]  ... fcount-1
    //            /      \   /    \    /     \                      /   \         \
    //   cobj = fcount-1 fcount 'fcount+1'  'fcount+2'    cobj =  [0]    [1] ...  fcount
    //                              ↑           ↑                  ↓      ↓
    //                              └-----------┼------------------┘      |
    //                                          └-------------------------┘
    for(int i = 0; i < dobj_right->fcount; ++i){
        dobj_left->fdes[ dobj_left->fcount + 1 + i ]    = dobj_right->fdes[ i ];
        dobj_left->mobj_id[ dobj_left->fcount + 1 + i ] = dobj_right->mobj_id[ i ];
    }
    for(int i = 0; i <= dobj_right->fcount; ++i){
        dobj_left->cobj_id[ dobj_left->fcount + 1 + i ] = dobj_right->cobj_id[ i ]; 
    }

    // after 1 & 2 : assert left_child->fcount == Dir_Node_Degree + 1
    dobj_left->fcount += dobj_right->fcount + 1;

    // step 3. write parent
    mba_file_name->write_by_obj_id( dobj_parent_id, dobj_parent );

    // step 4. dealloc right
    mba_file_name->de_alloc_obj( dobj_right_id );

    // step 5. if parent->fcount == 0 : change root
    // could conbine with step 3. to avoid write if parent is empty
    if( dobj_parent->fcount == 0){
        mba_file_name->de_alloc_obj( dobj_parent_id );
        root_dir_node_id = dobj_left_id;
    }
}

// get the max/min fdes of dobj
// go through all the child dobj
void DirBTree::max_of_dobj(
        struct dir_meta_obj* dobj,
        Nat_Obj_ID_Type dobj_id,
        struct file_descriptor* max_fdes,
        Nat_Obj_ID_Type * max_mobj_id)
{
  while( !dobj->is_leaf ){
    dobj_id = dobj->cobj_id[ dobj->fcount ];
    dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( dobj_id );
  }
  *max_fdes    = dobj->fdes[ dobj->fcount - 1 ];
  *max_mobj_id = dobj->mobj_id[ dobj->fcount - 1];
  return;
}

void DirBTree::min_of_dobj(
        struct dir_meta_obj* dobj,
        Nat_Obj_ID_Type dobj_id,
        struct file_descriptor* min_fdes,
        Nat_Obj_ID_Type * min_mobj_id)
{
  while( !dobj->is_leaf ){
    dobj_id = dobj->cobj_id[ 0 ];
    dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( dobj_id );
  }
  *min_fdes    = dobj->fdes[ 0 ];
  *min_mobj_id = dobj->mobj_id[ 0 ];
  return;
}



// display the whole dir_meta_tree
// display()
// print_dir_node()

void DirBTree::print_dir_node(
        struct dir_meta_obj* dobj,
        Nat_Obj_ID_Type dobj_id, int print_child)
{
  std::cout << "dobj{ " << dobj_id << " } = ";

  if( dobj->is_leaf ){
    std::cout << "leaf_node [ " << dobj->fcount << " ] : ";
  }
  else{
    std::cout << "iner_node [ " << dobj->fcount << " ] : "; 
  }
  for(int i = 0; i < dobj->fcount; ++i){
    std::cout << " (" << dobj->fdes[i].fhash << "," << dobj->mobj_id[i] << ")";
  }
  if( !dobj->is_leaf ){
    std::cout << std::endl << "              child: ";
    for(int i = 0; i <= dobj->fcount; ++i ){
      std::cout << " {" << dobj->cobj_id[i] << "} ";
    }
  }
  std::cout << std::endl;
  if( print_child ){    // print all child node
    if( !dobj->is_leaf ){
        for(int i = 0; i <= dobj->fcount; ++i){
        Nat_Obj_ID_Type id = dobj->cobj_id[i];
        struct dir_meta_obj* obj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( id );
        print_dir_node( obj, id , 1);
        }
    }
  }
}

void DirBTree::display()
{
  struct dir_meta_obj* dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( root_dir_node_id );
  if( dobj == nullptr ){
    return;
  }
  
  Nat_Obj_ID_Type dobj_id = root_dir_node_id;
  print_dir_node( dobj, dobj_id, 1 );
}



// verify my self
bool DirBTree::verify()
{
  Nat_Obj_ID_Type dobj_id = root_dir_node_id;
  struct dir_meta_obj* dobj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( dobj_id );

  if( !verify_node( dobj, dobj_id, nullptr, 0, 1 ) ) return false;
  
  return true;
}

bool DirBTree::verify_node(struct dir_meta_obj* dobj,
    Nat_Obj_ID_Type dobj_id,
    struct dir_meta_obj* its_father,
    int index, int verify_child)
{
  if( dobj == nullptr ){
    std::cout << " dobj == nullptr {" << dobj_id << "}" << std::endl;
    return false;
  }

  if( its_father != nullptr && dobj->fcount < Dir_Node_Half_Degree ){
    std::cout << " size less than half {" << dobj_id << "}" << std::endl;
    return false;
  }
  if( its_father != nullptr && dobj->fcount > Dir_Node_Degree ){
    std::cout << " size more than degree {" << dobj_id << "}" << std::endl;
    return false;
  }

  for(int i=0; i<dobj->fcount-1; ++i){
    if(dobj->fdes[i].fhash > dobj->fdes[i+1].fhash){
      std::cout << " values not sorted inner node {" << dobj_id << "}" << std::endl;
      return false;
    }
  }

  if( its_father != nullptr ){
    if( index > 0 &&
      dobj->fdes[0].fhash < its_father->fdes[ index - 1 ].fhash){
      std::cout << " values[0] less than left {" << dobj_id << "}" << std::endl;
      return false;
    }
    if( index < its_father->fcount &&
      dobj->fdes[ dobj->fcount - 1 ].fhash > its_father->fdes[ index ].fhash){
      std::cout << " values[fcount-1] more than right {" << dobj_id << "}" << std::endl;
      return false;
    }
  }

  if( verify_child && !dobj->is_leaf ){
    struct dir_meta_obj* obj = nullptr;
    for(int i = 0; i <= dobj->fcount; ++i){
      obj = (struct dir_meta_obj*)mba_file_name->read_by_obj_id( dobj->cobj_id[ i ] );
      if( !verify_node( obj, dobj->cobj_id[i], dobj, i, 1 ) ) return false;
    }
  }

  return true; 
}
