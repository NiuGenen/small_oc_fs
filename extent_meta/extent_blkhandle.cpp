#include"extent_blkhandle.h"

blk_addr_handle::blk_addr_handle(
    int ch, uint64_t blk_st, uint64_t blk_ed
)
{
    this->ch = ch;
    this->blk_st = blk_st;
    this->blk_ed = blk_ed;
}

int blk_addr_handle::GetBlkNr(){
    return blk_st - blk_ed + 1 ;
}

void blk_addr_handle::BlkAddrAdd(int x, uint64_t* addr)
{
    *addr += x;
}

void blk_addr_handle::BlkAddrSub(int x, uint64_t* addr)
{
    *addr -= x;
    if(*addr < 0) *addr = 0;
}

int blk_addr_handle::BlkAddrCmp(
    uint64_t* laddr,
    uint64_t* raddr)
{
    if( *laddr == *raddr ){
        return 0;
    }
    if( *laddr < *raddr ){
        return -1;
    }
    return 1;
}
