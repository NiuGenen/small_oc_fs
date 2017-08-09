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

    srand((unsigned int)time(0));
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

    int test_nr = 100;

    printf("---- ADD\n");
    struct file_descriptor _fdes[ test_nr ];
    for(int h = 0; h < test_nr; ++h){
        _fdes[ h ].fhash = h + 100;
        Nat_Obj_ID_Type mobj_id = mba_file_meta->alloc_obj();
        struct extent_descriptor* edes = extentList[ ch ]->getNewExt();
        struct file_meta_obj file_meta;
        file_meta.obj_id = mobj_id;
        file_meta.fdes.fhash = _fdes[h].fhash;
        file_meta.edes_nr = 1;
        sprintf(file_meta.filename, "file%d", _fdes[h].fhash);
        file_meta.edes[0] = *edes;
        mba_file_meta->write_by_obj_id(mobj_id, &file_meta);
        dirBTree->add_new_file( _fdes[h], mobj_id );
        printf("Add %s\n", file_meta.filename);
    }

    printf("---- DELETE\n");
    for(int _i = 0; _i < 20; ++_i){
        int i = random() % test_nr;
        if( _fdes[i].fhash == 0 ) continue;
        Nat_Obj_ID_Type mobj_id = dirBTree->get_file_meta_obj_id(_fdes[i]);
        dirBTree->del_file( _fdes[i] );
        struct file_meta_obj* mobj = (struct file_meta_obj*) mba_file_meta->read_by_obj_id( mobj_id );
        printf("Delete %s\n", mobj->filename);
        for(int i = 0; i < mobj->edes_nr; ++i){
            extentList[ch]->putExt( &(mobj->edes[i]) );
        }
        mba_file_meta->de_alloc_obj( mobj_id );
        _fdes[i].fhash = 0;
    }

    printf("---- FIND\n");
    for(int _i = 0; _i < 20; ++_i){
        int i = random() % test_nr;
        if(_fdes[i].fhash == 0 ) continue;
        Nat_Obj_ID_Type mobj_id = dirBTree->get_file_meta_obj_id(_fdes[i]);
        struct file_meta_obj* mobj = (struct file_meta_obj*) mba_file_meta->read_by_obj_id( mobj_id );
        printf("Find %s   ", mobj->filename);
        printf(" - fhash = %u\n", mobj->fdes.fhash);
    }

    return 0;
}
