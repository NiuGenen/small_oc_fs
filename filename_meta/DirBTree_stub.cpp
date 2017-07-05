#include <iostream>
#include "DirBTree_stub.h"

// stub for test

#define uint16_size 65536

dir_meta_number_t dir_obj_id_table[ uint16_size ];
char dir_obj_table_use[ uint16_size ];

struct dir_meta_obj* dir_obj_table[ uint16_size ];

void dir_meta_init()
{
  for(dir_meta_number_t i = 0; i < uint16_size; ++i){
    dir_obj_id_table[i] = i;
    dir_obj_table_use[i] = '0';
    dir_obj_table[i] = nullptr;
  }
}

dir_meta_number_t dir_meta_alloc_obj_id()
{
  for(dir_meta_number_t i = 0; i < uint16_size; ++i){
    if( dir_obj_table_use[i] == '0' ){
      dir_obj_table_use[i] = '1';
      dir_obj_table[i] = new struct dir_meta_obj;
      return dir_obj_id_table[i];
    }
  }
  return 0;
}

void dir_meta_dealloc_obj_id(dir_meta_number_t dobj_id)
{
    dir_obj_table_use[ dobj_id ] = '0';
}

void dir_meta_write_by_obj_id(Dir_Nat_Entry_ID_Type dobj_id, struct dir_meta_obj* obj)
{
    // assert dir_obj_table_use[ dobj_id ] = '1'
    dir_obj_table[ dobj_id ] = obj;
}

struct dir_meta_obj* dir_meta_read_by_obj_id(Dir_Nat_Entry_ID_Type dobj_id)
{
    // assert dir_obj_table_use[ dobj_id ] = '1'
    return dir_obj_table[ dobj_id ];
}
