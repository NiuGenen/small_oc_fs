#ifndef _DIRBTREE_H_
#define _DIRBTREE_H_

#include <iostream>

//#define DEBUG

typedef int File_Nat_Entry_ID_Type;
typedef int dir_meta_number_t;
typedef dir_meta_number_t Dir_Nat_Entry_ID_Type;
typedef unsigned int uint32_t;

struct file_descriptor{
    uint32_t fhash;
};

#define Dir_Node_Half_Degree   2
#define Dir_Node_Degree        ( Dir_Node_Half_Degree * 2 + 1 )

// 4KB
// fdes[]    =    { 0, 1, 2 ... n }
//                 /  /  / ... / \
// cobj_id[] =  { 0, 1, 2 ... n, n+1 }
struct dir_meta_obj{    // node
    struct file_descriptor  fdes       [ Dir_Node_Degree ];        // 401 * 4 B = 1604 B 
    File_Nat_Entry_ID_Type  mobj_id    [ Dir_Node_Degree ];        // 401 * 4 B = 1604 B
    Dir_Nat_Entry_ID_Type   cobj_id    [ Dir_Node_Degree + 1 ];   // 401 * 2 B =  802 B
    //Dir_Nat_Entry_ID_Type   father_id;                                  // 2 B
    uint32_t                is_leaf;                                    // 4 B
    dir_meta_number_t       fcount;                                     // 4 B
};// 1604 + 1604 + 802 + 4 = 4014 < 4096

class DirBTree{
    public:
    DirBTree();
    virtual ~DirBTree();

    void init();
    bool add_new_file(struct file_descriptor fdes, File_Nat_Entry_ID_Type mobj_id);
    bool del_file(struct file_descriptor fdes);
    // bool modify_file(struct file_descriptor fdes, File_Nat_Entry_ID_Type n_mobj_id);
    File_Nat_Entry_ID_Type get_file_meta_obj_id(struct file_descriptor fdes);
    void display();
    bool verify();

    private:
    int root_dir_node_id;
    //int node_degree;
    void insert_not_full(
        struct dir_meta_obj* dobj, 
        Dir_Nat_Entry_ID_Type dobj_id,
        struct file_descriptor fdes, 
        File_Nat_Entry_ID_Type mobj_id);

    void split_child(
        struct dir_meta_obj* dobj_parent,
        Dir_Nat_Entry_ID_Type dobj_parent_id,
        int index,
        struct dir_meta_obj* dobj_child,
        Dir_Nat_Entry_ID_Type dobj_child_id);

    void print_dir_node(
        struct dir_meta_obj* dobj,
        Dir_Nat_Entry_ID_Type dobj_id,
        int print_child);

    void delete_not_half(
        struct dir_meta_obj* dobj,
        Dir_Nat_Entry_ID_Type dobj_id,
        struct file_descriptor fdes);

    void union_child(
        struct dir_meta_obj* dobj_parent, Dir_Nat_Entry_ID_Type dobj_parent_id,
        struct dir_meta_obj* dobj_left,  Dir_Nat_Entry_ID_Type dobj_left_id,
        struct dir_meta_obj* dobj_right, Dir_Nat_Entry_ID_Type dobj_right_id,
        int index);

    void max_of_dobj(
        struct dir_meta_obj* dobj,
        Dir_Nat_Entry_ID_Type dobj_id,
        struct file_descriptor * max_fdes,
        File_Nat_Entry_ID_Type * max_mobj_id);

    void min_of_dobj(
        struct dir_meta_obj* dobj,
        Dir_Nat_Entry_ID_Type dobj_id,
        struct file_descriptor * min_fdes,
        File_Nat_Entry_ID_Type * min_mobj_id);

    bool verify_node(
        struct dir_meta_obj* dobj,
        Dir_Nat_Entry_ID_Type dobj_id,
        struct dir_meta_obj* its_parent,
        int index,
        int verify_child);
};

#endif
