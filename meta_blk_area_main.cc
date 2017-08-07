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

    void* obj = malloc( 4096 );

    for(int i = 0; i < test_size; ++i ){
        obj_id[ i ] = mba_file_name->alloc_obj();
        *((uint64_t*)obj) = obj_id[i];
        mba_file_name->write_by_obj_id( obj_id[i], obj );
    }

    printf("=========================================\n");
//    mba_file_name->print_nat();
    mba_file_name->print_bitmap();
    printf("=========================================\n");

    for( int i = 0; i < 512; i += 2 ){
        mba_file_name->de_alloc_obj( obj_id[ i ] );
        obj_id[ i ] = 0;
    }

    printf("=========================================\n");
//    mba_file_name->print_nat();
    mba_file_name->print_bitmap();
    printf("=========================================\n");

    mba_file_name->GC();

    printf("=========================================\n");
//    mba_file_name->print_nat();
    mba_file_name->print_bitmap();
    printf("=========================================\n");

    return 0;
}
