all:test_addr test_sb

_blk_addr =  blk_addr.h blk_addr.cc
_sb_area  =  super_blk_area.h super_blk_area.cc
_mba	  =  meta_blk_area.h meta_blk_area.cc
_ext	  =  extent_meta/extent_blk_area.h extent_meta/extent_blk_area.cc
_fn		  =  filename_meta/file_name_blk_area.h filename_meta/file_name_blk_area.cc
_fm       =  file_meta/file_meta_blk_area.h file_meta/file_meta_blk_area.cc

test_addr: blk_addr_main.cc ${_blk_addr}
	g++ -o test_addr blk_addr_main.cc ${_blk_addr} -std=c++0x

test_sb: super_blk_area_main.cc ${_sb_area} ${_blk_addr} ${_mba} ${_ext} ${_fn} ${_fm}
	g++ -o test_sb \
		 super_blk_area_main.cc ${_sb_area} ${_blk_addr} ${_mba} ${_ext} ${_fn} ${_fm} \
		 -std=c++0x -llightnvm

