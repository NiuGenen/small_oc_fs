#include "extent_tree.h"

#define PtrExtLeafNode(x) ((struct ext_leaf_node*)x)
#define PtrExtNoLeafNode(x) ((struct ext_non_leaf_node*)x)

#define PtrNodeType(x) ((uint32_t*)x)

#define IfNodeIsLeaf(x)   (*(PtrNodeType(x))==0) // 0 is leaf
#define IfNodeNotLeaf(x)  (*(PtrNodeType(x))==1) // 1 is iner

ExtentTree::ExtentTree(
    int ch,
    uint64_t blk_st, uint64_t blk_ed,
    int ext_size)
{
    this.ch = ch;
    this.blk_st = blk_st;
    this.blk_ed = blk_ed;
    this.ext_size = ext_size;
    this.handler = handler;
    this.blk_nr = handler.GetBlkNr();
}

ExtentTree::~ExtentTree(){
}

void ExtentTree::init(){
    root_ext_node_id = ext_alloc_obj_id();
    void* root_eobj = ext_read_by_obj_id( root_ext_node_id );
    //*(PtrNodeType( root_eobj )) = 0;
    struct ext_non_leaf_node* eobj = PtrExtNoLeafNode( root_eobj );
    eobj->node_type = 0;
    eobj->el_2_mobj_num = 0;
    eobj->el_8_mobj_num = 0;
    eobj->el_4_mobj_num = 0;
    eobj->el_none_mobj_num = 1;
    eobj->mobjs[0].free_vblk_num = blk_nr / ext_size;
    eobj->mobjs[0].index = 0;
    eobj->exts[0].addr_st_buf = blk_st;
    eobj->exts[0].addr_ed_buf = blk_ed;
    eobj->exts[0].free_bitmap = 0;
    eobj->exts[0].junk_bitmap = 0;
    ext_write_by_obj_id( root_ext_node_id, root_eobj);
}


struct extent_descriptor* ExtentTree::getExt()
{
    struct extent_descriptor* edes = 
        (struct extent_descriptor*)malloc( SIZE_EXT_DES );
    Ext_Node_ID_Type eobj_id = root_ext_node_id;
    void* _eobj = ext_read_by_obj_id( eobj_id );

    if( IfNodeIsLeaf(_eobj) ){ // leaf node
        struct ext_leaf_node* eobj = PtrExtLeafNode( _eobj );
        if( eobj->mobjs[0].free_vblk_num > 0){ // ok to get ext
            struct extent* _ext = eobj->exts + eobj->mobjs[0].index;
            
            eobj->mobjs[0].free_vblk_num -= 1;  // modify meta & sort
            uint32_t exts_nr = 
                eobj->el_2_mobj_num +
                eobj->el_4_mobj_num + 
                eobj->el_8_mobj_num + 
                eobj->el_none_mobj_num;
            for(uint32_t i = 0; i < exts_nr - 1; ++i){
                if(eobj->mobjs[i].free_vblk_num < eobj->mobjs[i+1].free_vblk_num){
                    struct ext_meta_obj tmp = eobj->mobjs[i];
                    eobj->mobjs[i] = eobj->mobjs[i+1];
                    eobj->mobjs[i+1] = tmp;
                }
            }

            edes->addr_st_buf = _ext->addr_st_buf;
            edes->addr_ed_buf = edes->addr_st_buf;
            handler.BlkAddrAdd( ext_size, &(edes->addr_ed_buf) );
            handler.BlkAddrAdd( ext_size, &(_ext->addr_st_buf) );
            edes->use_bitmap = 0;

            eobj->exts[ exts_nr ].addr_st_buf = edes->addr_st_buf;
            eobj->exts[ exts_nr ].addr_ed_buf = edes->addr_ed_buf;
            eobj->mobjs[ exts_nr ].free_vblk_num = 0;
            eobj->mobjs[ exts_nr ].index = exts_nr;
            switch(ext_size){
                case 2: eobj->el_2_mobj_num += 1; break;
                case 4: eobj->el_4_mobj_num += 1; break;
                case 8: eobj->el_8_mobj_num += 1; break;
            }
            
            return &edes;
        }
        else{ // no more blk
            return nullptr;
        }
    }
    else if( IfNodeNotLeaf(_eobj) ){ // not leaf
        struct ext_non_leaf_node* eobj = PtrExtNoLeafNode( _eobj );
    }

}
