#include "blk_addr.h"
#include "liblightnvm.h"
#include <stdio.h>

#define TEST_SIZE 33

void binary_print( struct blk_addr* addr ){
    uint64_t buf = addr->__buf;
    char out[65] = {'0'};
    for(int i = 1; i <= 64; ++i){
        out[64-i] = (char)( ( (char)buf & 1 ) + '0' );
        buf = buf >> 1;
    }
    out[64]='\0';
    printf("%s",out);
}

extern blk_addr_handle** blk_addr_handlers_of_ch;
extern size_t nchs;

struct blk_addr blk_low;
struct blk_addr blk_hig;

int main()
{
	struct nvm_geo g;

	g.nchannels = 16;
	g.nluns = 8;
	g.nblocks = 16;
	g.nplanes = 16;
	g.npages = 256;

    nchs = g.nchannels;
    addr_init( &g );

    blk_addr_handle* bah_ch_0 = blk_addr_handlers_of_ch[0] ;

    blk_low = bah_ch_0->get_lowest();
    blk_hig = bah_ch_0->get_highest();
    size_t blk_nr_ch_0 = bah_ch_0->get_blk_nr();
    printf("ch_0_blk_nr = %u\n", blk_nr_ch_0 );
    printf("ch_0_blk_nr = %u\n", 8 * 16 * 16 );
    printf("blk_low = %llu\n", blk_low.__buf );
    printf("blk_hig = %llu\n", blk_hig.__buf );

    printf("blk_ch_0_low = ");
    binary_print( &blk_low );
    printf("\n");
    bah_ch_0->PrBlkAddr( &blk_low, true, "blk_ch_0_low = " );

    for(int i=0; i<TEST_SIZE; ++i){
        bah_ch_0->BlkAddrAdd( 1, &blk_low );
        printf("blk_ch_0_low = ");
        binary_print(&blk_low);
        printf("\n");
        bah_ch_0->PrBlkAddr( &blk_low, true, "blk_low = " );
    }

    bah_ch_0->PrBlkAddr( &blk_hig, true, "blk_hig = " );

    addr_release();
}
