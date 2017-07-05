#include "blk_addr.h"
#include <stdio.h>

#define TEST_SIZE 33

void binary_print( struct blk_addr* addr ){
    uint64_t buf = addr->_buf;
    char out[65];
    for(int i = 1; i <= 64; ++i){
        out[64-i] = bug & 1 + '0' ;
        buf = buf >> 1;
    }
    out[64]='\0';
    printf("%s",out);
}

int main()
{
	nvm_geo g;

	g.nchannels = 16;
	g.nluns = 8;
	g.nblocks = 16;
	g.nplanes = 16;
	g.npages = 256;

    nchs = g.nchannels;
    addr_init(&g);

    blk_addr_handle* bah_ch_0 = blk_addr_handlers_of_ch[0];

    struct blk_addr blk_ch_0_low = bah_ch_0.get_lowest();
    struct blk_addr blk_ch_0_hig = bah_ch_0.get_highest();
    size_t blk_nr_ch_0 = bah_ch_0.get_blk_nr();
    printf("blk_ch_0_low = ");
    binary_print(&blk_ch_0_low);
    printf("\n");
    bah_ch_0.PrBlkAddr( &blk_ch_0_low, true, "blk_ch_0_low = " );

    for(int i=0; i<TEST_SIZE; ++i){
        bah_ch_0.BlkAddrAdd( 1, &blk_ch_0_low );
        printf("blk_ch_0_low = ");
        binary_print(&blk_ch_0_low);
        printf("\n");
        bah_ch_0.PrBlkAddr( &blk_ch_0_low, true, "blk_ch_0_low = " );
    }

    bah_ch_0.PrBlkAddr( &blk_ch_0_hig, true, "blk_ch_0_hig = " );

    addr_release();
}