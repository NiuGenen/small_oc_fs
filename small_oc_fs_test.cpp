//
// Created by ng on 8/8/17.
//

#include<stdio.h>
#include "filename_meta/DirBTree.h"
#include "file_meta/file_meta_blk_area.h"
#include "extent_meta/extent_list.h"

extern DirBTree* dirBTree;
extern ExtentList** extentList;
extern MetaBlkArea *mba_file_meta;

int main()
{
    struct file_descriptor fdes;
    fdes.fhash = 1;
    Nat_Obj_ID_Type mobj_id = mba_file_meta->alloc_obj();
    // need to define file meta
    int ch = 0;
    struct extent_descriptor* edes = extentList[ ch ]->getNewExt();

    dirBTree->add_new_file( fdes, mobj_id );

    return 0;
}
