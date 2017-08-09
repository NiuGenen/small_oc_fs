//
// Created by ng on 8/8/17.
//

#include<stdio.h>
#include <cstring>
#include "filename_meta/DirBTree.h"
#include "file_meta/file_meta.h"
#include "super_blk_area.h"

extern DirBTree* dirBTree;
extern ExtentList** extentList;
extern MetaBlkArea *mba_file_meta;

int main()
{
    OcssdSuperBlock ocsb;

    printf("sizeof(dir_meta_obj)    = %lu\n",sizeof(dir_meta_obj) );
    printf("sizeof(extent_meta_obj) = %lu\n",sizeof(ext_node) );
    printf("sizeof(file_meta_obj)   = %lu\n",sizeof(file_meta_obj));

    // generate fhash
    struct file_descriptor fdes;
    fdes.fhash = 1;
    // gei file meta obj id
    Nat_Obj_ID_Type mobj_id = mba_file_meta->alloc_obj();
    // get extent descriptor
    int ch = 0;
    struct extent_descriptor* edes = extentList[ ch ]->getNewExt();

    struct file_meta_obj file_meta;
    file_meta.obj_id = mobj_id;
    file_meta.fdes.fhash = fdes.fhash;
    file_meta.edes_nr = 1;
    memcpy(file_meta.filename,"file1",5);
    file_meta.edes[0] = *edes;

    mba_file_meta->write_by_obj_id(mobj_id, &file_meta);
    dirBTree->add_new_file( fdes, mobj_id );

    Nat_Obj_ID_Type mobj_id_ = dirBTree->get_file_meta_obj_id( fdes );
    struct file_meta_obj* mobj = (struct file_meta_obj*) mba_file_meta->read_by_obj_id( mobj_id );
    printf("filename = %s\n",mobj->filename);

    return 0;
}
