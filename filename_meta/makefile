all:test_btree test_dirbtree
	
test_btree:BTree.h BTree.cpp BTree_main.cpp
	g++ -o test_btree BTree.h BTree.cpp BTree_main.cpp -std=c++0x

test_dirbtree:DirBTree.h DirBTree.cpp DirBTree_main.cpp DirBTree_stub.h DirBTree_stub.cpp
	g++ -o test_dirbtree DirBTree.h DirBTree.cpp DirBTree_main.cpp DirBTree_stub.h DirBTree_stub.cpp -std=c++0x
