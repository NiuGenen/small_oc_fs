//
// Created by ng on 8/9/17.
//

#ifndef SMALL_OC_FS_FILE_META_H_H
#define SMALL_OC_FS_FILE_META_H_H

#include "file_meta_blk_area.h"
#include "../filename_meta/DirBTree.h"
#include "../extent_meta/extent_list.h"

#define FILE_META_NAME_MAX_LENGTH 240
#define FILE_META_EDES_MAX_NR     32

struct file_meta_obj{   // 1024
    Nat_Obj_ID_Type obj_id;                             //   0 +   8  =    8
    struct file_descriptor fdes;                        //   8 +   4  =   12
    uint32_t edes_nr;                                   //  12 +   4  =   16
    char filename[ FILE_META_NAME_MAX_LENGTH ];                               //  16 + 240  =  256
    struct extent_descriptor edes[FILE_META_EDES_MAX_NR];  // 8+8+4+4=24   // 256 + 24 * 32 = 1024
};

#endif //SMALL_OC_FS_FILE_META_H_H
