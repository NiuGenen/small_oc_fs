#include"extent_list.h"
#include"extent_stub.h"
#include<iostream>

using namespace std;

int ch = 0;
uint64_t blk_st = 0;
uint64_t blk_ed = 1023;
blk_addr_handle handler(ch,blk_st,blk_ed);

#define TEST_SIZE 16

int main()
{
    ext_meta_mba_init();

    std::cout <<" build extent_list " << std::endl;
    ExtentList el(ch,blk_st,blk_ed,4,&handler);

    std::cout <<" test getNexExt " << std::endl;
    struct extent_descriptor** edes;
    edes = (struct extent_descriptor**)malloc(sizeof(struct extent_descriptor*) * TEST_SIZE);
    for(int i=0; i<TEST_SIZE; ++i){
        edes[i] = el.getNewExt();
        el.display();
    }
    
    std::cout <<" test put " << std::endl;
    edes[0]->junk_bitmap=1;
    edes[1]->junk_bitmap=1;
    edes[4]->junk_bitmap=1;
    edes[5]->junk_bitmap=1;
    edes[6]->junk_bitmap=1;
    edes[13]->junk_bitmap=1;
    edes[14]->junk_bitmap=1;
    el.putExt( edes[0] );
    el.putExt( edes[1] );
    el.putExt( edes[4] );
    el.putExt( edes[5] );
    el.putExt( edes[6] );
    el.putExt( edes[13] );
    el.putExt( edes[14] );
    el.display();

    std::cout <<" text GC() " << std::endl;
    el.GC();
    el.display();

    std::cout <<" test over " << std::endl;
}
