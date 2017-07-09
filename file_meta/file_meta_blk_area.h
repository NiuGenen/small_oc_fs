#ifndef _FILE_META_BLK_AREA_H_
#define _FILE_META_BLK_AREA_H_

#include "../meta_blk_area.h"

#define FILE_META_NAME "FileMeta"
#define FILE_META_BLK_CTS 300
#define FILE_META_OBJ_SIZE 1024
#define FILE_META_OBJ_CTS 2000000

//global var
extern MetaBlkArea *mba_file_meta;

#endif