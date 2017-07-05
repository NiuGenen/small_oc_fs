#ifndef _EXTENT_TREE_STUB_H_
#define _EXTENT_TREE_STUB_H_

#include "extent_type.h"

void ext_meta_mba_init();

Ext_Nat_Entry_ID_Type ext_alloc_obj_id( );

void ext_dealloc_obj_id(
    Ext_Nat_Entry_ID_Type eobj_id );

void ext_write_by_obj_id(
    Ext_Nat_Entry_ID_Type eobj_id,
    void *eobj);

void* ext_read_by_obj_id(
    Ext_Nat_Entry_ID_Type eobj_id );

#endif
