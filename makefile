all:test_addr

test_addr: blk_addr.h blk_addr.cpp blk_addr_main.cpp
	g++ -o test_addr blk_addr.h blk_addr.cpp blk_addr_main.cpp -std=c++0x