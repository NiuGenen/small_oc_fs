#include "super_blk_area.h"
#include "meta_blk_area.h"
#include "extent_meta/extent_blk_area.h"
#include "file_meta/file_meta_blk_area.h"
#include "filename_meta/file_name_blk_area.h"
#include <liblightnvm.h>

#define OC_SB_CH     0
#define OC_SB_LUN    0
#define OC_SB_PL     0
#define OC_SB_BLK    0

extern MetaBlkArea *mba_file_meta;
extern MetaBlkArea *mba_extent;
extern MetaBlkArea *mba_file_name;

#define OC_DEV_PATH "/dev/nvm0n1"

OcssdSuperBlock::OcssdSuperBlock(){
    dev = nvm_dev_open( OC_DEV_PATH );

    sb_addr.ppa = 0;
    sb.addr.g.ch = OC_SB_CH;
    sb.addr.g.lun = OC_SB_LUN;
    sb.addr.g.pl = OC_SB_PL;
    sb.addr.g.blk = OC_SB_BLK;

    struct nvm_vblk* sb_vblk = nvm_vblk_alloc( dev, &sb_addr, 1);

    nvm_vblk_pread( sb_vblk, &sb_meta, sizeof( struct ocssd_sb_meta), 0);
}