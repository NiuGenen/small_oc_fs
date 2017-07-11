cmake_minimum_required(VERSION 3.7)
project(small_oc_fs)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        liblightnvm.h
        extent_meta/extent_blk_area.cc
        extent_meta/extent_blk_area.h
        extent_meta/extent_blkhandle.cpp
        extent_meta/extent_blkhandle.h
        extent_meta/extent_list.cpp
        extent_meta/extent_list.h
        extent_meta/extent_list_main.cpp
        extent_meta/extent_stub.cpp
        extent_meta/extent_stub.h
        extent_meta/extent_tree.cpp
        extent_meta/extent_tree.h
        extent_meta/extent_type.h
        file_meta/file_meta_blk_area.cc
        file_meta/file_meta_blk_area.h
        filename_meta/DirBTree.cpp
        filename_meta/DirBTree.h
        filename_meta/DirBTree_main.cpp
        filename_meta/DirBTree_stub.cpp
        filename_meta/DirBTree_stub.h
        filename_meta/file_name_blk_area.cc
        filename_meta/file_name_blk_area.h
        blk_addr.cc
        blk_addr.h
        blk_addr_main.cc
        meta_blk_area.cc
        meta_blk_area.h
        meta_blk_area_main.cc
        super_blk_area.cc
        super_blk_area.h
        super_blk_area_main.cc)

add_executable(small_oc_fs ${SOURCE_FILES})