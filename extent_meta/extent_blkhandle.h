#ifndef _EXTENT_BLK_HANDLE_H_
#define _EXTENT_BLK_HANDLE_H_

#include "extent_type.h"

class blk_addr_handle{
public:
    blk_addr_handle(
        int ch,
        uint64_t blk_st, uint64_t blk_ed);
    int GetBlkNr();
    void BlkAddrAdd(int x, uint64_t* addr);
    void BlkAddrSub(int x, uint64_t* addr);
    int BlkAddrCmp(
        uint64_t* laddr,
        uint64_t* raddr);
private:
    int ch;
    uint64_t blk_st;
    uint64_t blk_ed;
};

#endif
