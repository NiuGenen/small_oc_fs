#include "DirBTree.h"
#include "DirBTree_stub.h"
#include <stdlib.h>
#include <time.h>

#define AUTO_TEST_SIZE 1024

bool in_set(
    struct file_descriptor* fdeses,
    unsigned int length,
    struct file_descriptor fdes )
{
  for(int i=0; i<length; ++i)
    if( fdeses[i].fhash == fdes.fhash ) return true;

  return false;
}

void auto_test()
{
  DirBTree bt;
  bt.init();
 
  struct file_descriptor fdes[ AUTO_TEST_SIZE ];
  File_Nat_Entry_ID_Type mobj_id[ AUTO_TEST_SIZE ];

  srand(time(0));
  for(int i = 0; i<AUTO_TEST_SIZE; ++i){
    fdes[i].fhash  = rand() % ( AUTO_TEST_SIZE * 1000 ) + 1;
    while( in_set( fdes, i, fdes[i] ) )
        fdes[i].fhash = rand() % ( AUTO_TEST_SIZE * 1000 ) + 1;
    mobj_id[i]     = rand() % ( AUTO_TEST_SIZE * 1000 ) + 1;
    bt.add_new_file( fdes[i], mobj_id[i] );
    if( !bt.verify() ) { bt.display(); return; }
  }

  bool verify = true;

  std::cout << "verify result = " << ( (verify=bt.verify()) ?"True":"False") << std::endl;
  if( !verify ) bt.display();

  for(int i = 0; i< ( AUTO_TEST_SIZE / 50 ); ++i){
    int index = rand() % AUTO_TEST_SIZE ;
    std::cout << " === delete fdes[" << index << "] = " << fdes[index].fhash << std::endl; 
    bt.del_file( fdes[ index ] ) ;
    //bt.display();
    if( !bt.verify() ) exit(1);
  }
  
  std::cout << "verify result = " << ( (verify=bt.verify()) ?"True":"False") << std::endl;
  if( !verify ) bt.display();
}

#define RANDOM_TEST_SIZE 10240

void random_test()
{
  DirBTree bt;
  bt.init();

  struct file_descriptor fdes[ RANDOM_TEST_SIZE ];
  File_Nat_Entry_ID_Type mobj_id[ RANDOM_TEST_SIZE ];

  std::cout << "insert random " << RANDOM_TEST_SIZE <<std::endl;
  srand(time(0));
  for(int i=0; i<RANDOM_TEST_SIZE; ++i){
    fdes[i].fhash  = rand() % ( RANDOM_TEST_SIZE * 1000 ) + 1;
    mobj_id[i]     = rand() % ( RANDOM_TEST_SIZE * 1000 ) + 1;
    bt.add_new_file( fdes[i], mobj_id[i] );
    //std::cout << "insert fdes=" << fdes[i].fhash << " - " << mobj_id[i]  << std::endl;
    //bt.display();
  }

  std::cout << "search : " << fdes[10].fhash << " - " << mobj_id[10] << std::endl;
  File_Nat_Entry_ID_Type s_id = bt.get_file_meta_obj_id( fdes[10] );
  std::cout << "search result = " << s_id << std::endl;

  for(int i=0; i<5; ++i){
    bt.del_file( fdes[i] );
    std::cout << "delete fdes=" << fdes[i].fhash << std::endl;
  }
  bt.add_new_file( fdes[2] , mobj_id[2] );
  std::cout << "insert fdes=" << fdes[2].fhash << " - " << mobj_id[2]  << std::endl;

  bt.display();
}

void manual_test()
{
  DirBTree b;
  b.init();

  char asd = 'a';
  int i = 0;

  std::cout << "====== insert test begin ======" << std::endl;
  struct file_descriptor fdes[32];
  File_Nat_Entry_ID_Type mobj_id[32];
  for(i=0; i<32; ++i){
    std::cout << "---------" << i << "---------" << std::endl;

    fdes[i].fhash = i;
    mobj_id[i] = i+1000;
    b.add_new_file(fdes[i], mobj_id[i]);
    b.display();

    std::cin >> asd;
    if( asd == 'q' ){
      std::cout << "====== insert test finish ======" << std::endl;
      break;
    }
  }
  for(i++;i<32;++i){
    fdes[i].fhash = i;
    mobj_id[i] = i+1000;
    b.add_new_file( fdes[i], mobj_id[i] );
  }
  b.display();

  i = 0;
  asd = 'a';
  std::cout << std::endl << "====== search test begin ======" << std::endl;
  for(i=0; i<32; ++i){
    std::cout<<"---- search fdes="<<fdes[i].fhash<<" ----"<<std::endl;
    File_Nat_Entry_ID_Type mobj_id = b.get_file_meta_obj_id( fdes[i] );
    std::cout<<"result = "<<mobj_id<<std::endl;

    std::cin >> asd;
    if( asd == 'q' ){
      std::cout << "====== search test finish ======"<<std::endl;
      break;
    }
  }

  i = 0;
  asd = 'a';
  std::cout << std::endl << "====== delete test begin ======" << std::endl;
  for(i=0; i<32; ++i){
    std::cout<<"---- delete fdes="<<fdes[i].fhash<<" ----"<<std::endl;
    b.del_file( fdes[i] );
    b.display();

    std::cin >> asd;
    if( asd == 'q' ){
      std::cout << "====== delete test finish ======"<<std::endl;
      break;
    }
  }
}

int main(int argc, char*argv[])
{
  dir_meta_init();

  //random_test();
  //manual_test();
  auto_test();
}

