#ifndef _DIRBTREE_STUB_H_
#define _DIRBTREE_STUB_H_

#include "DirBTree.h"

// meta_block_area_t* dir_meta_mba; // global var
// 'dobj' means dir_meta_obj
// 'mobj' means file_meta_obj
//      struct dir_meta_obj;
  void dir_meta_init();
  Dir_Nat_Entry_ID_Type dir_meta_alloc_obj_id();
  void dir_meta_dealloc_obj_id( Dir_Nat_Entry_ID_Type dobj_id );
  void dir_meta_write_by_obj_id( Dir_Nat_Entry_ID_Type dobj_id,
                                 struct dir_meta_obj * obj );
  struct dir_meta_obj* dir_meta_read_by_obj_id( Dir_Nat_Entry_ID_Type dobj_id );

#endif
