#include "super_blk_area.h"
#include "meta_blk_area.h"
#include "filename_meta/DirBTree.h"

extern MetaBlkArea *mba_file_name;

int main()
{
    OcssdSuperBlock oc_sb;

    printf("\n");

    int test_size = 600;    // 256 + 256 + 88
    Nat_Obj_ID_Type obj_id[ test_size ] = { (Nat_Obj_ID_Type )0 };  // obj id start from 1

    for(int i = 0; i < test_size; ++i ){
        obj_id[ i ] = mba_file_name->alloc_obj();
    }

    for( int i = 0; i < 512; i += 2 ){
        mba_file_name->de_alloc_obj( obj_id[ i ] );
        obj_id[ i ] = 0;
    }


//    for(size_t i = 0 ; i < 1024; ++i){
//        mba_file_name->alloc_obj();
//    }
//
//    Nat_Obj_ID_Type obj_id[10];
//    for(int i = 0 ; i < 10; ++i ) {
//        obj_id[i] = mba_file_name->alloc_obj();
//        printf(" obj_id[%d] = %lu\n", i , obj_id[i] );
//    }
//
//    printf("\n");
//
//    for(size_t i = 0 ; i < 10; ++i ){
//        void* obj_ = mba_file_name->read_by_obj_id( obj_id[i] );
//        struct dir_meta_obj* obj = (struct dir_meta_obj*)obj_;
//        obj->fcount = (dir_meta_number_t) i;
//        mba_file_name->write_by_obj_id(obj_id[i], obj);
//        free(obj);
//        obj = (struct dir_meta_obj*) mba_file_name->read_by_obj_id( obj_id[i] );
//        printf(" obj_id == %lu\n",obj_id[i] );
//        printf(" dir_obj.fcount == %d\n", obj->fcount );
//    }
//
    return 0;
}
