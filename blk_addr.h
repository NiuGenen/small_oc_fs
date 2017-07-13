#ifndef _BLK_ADDR_H_
#define _BLK_ADDR_H_

#include <stdint.h>
#include <stddef.h>
#include <liblightnvm.h>
#include <sys/types.h>
#include <iostream>

//1 channel 1 ext_tree
//for qemu:
// @ch is fixed to 0
// @stlun & @nluns is changable
// a channel contains multi ext_tree
//
//for real-device:
// @ch is changable
// @stlun & @nluns is fixed
// this ext_tree's lun range: [stlun, stlun + nluns - 1]
struct addr_meta { //fields for address-format 
    size_t ch;
    size_t stlun;
    size_t nluns;
};

/*
 * Block Address Format: >>>>>>>>default
 *
 *        |   ch   |  blk   |  pl   |   lun   |
 * length   _l[0]    _l[1]    _l[2]    _l[3]
 * idx      0        1        2        3
 */

/* 
 * Block Address Layout:(format 3)
 * 				 
 *  			 ext_tree_0						ext_tree_1					....
 * 			  |-----ch_0---------|			|-----ch_1---------|			....
 *  		  L0    L2    L3    L4			L0    L2    L3    L4	  
 * B0  p0     =     =     =     =		    =     =     =     =				Low ----> High
 *     p1     =     =     =     =           =     =     =     =				|   ---->
 * B1  p0     =     =     =     =		    =     =     =     =				|
 *     p1     =     =     =     =           =     =     =     =				|
 * B2  p0     =     =     =     =		    =     =     =     =				High
 *     p1     =     =     =     =           =     =     =     = 
 * B3  p0     =     =     =     =		    =     =     =     =
 *     p1     =     =     =     =           =     =     =     =
 * Bn  p0     =     =     =     =		    =     =     =     =
 *     p1     =     =     =     =           =     =     =     =
 * 			  BBT0  BBT1  BBT2  BBT3 		BBT0  BBT1  BBT2  BBT3
 */

struct blk_addr{
	uint64_t __buf;
};

class blk_addr_handle{ // a handle should attach to a tree.
public:
	blk_addr_handle(struct nvm_geo const *g, struct addr_meta const *tm);
	~blk_addr_handle();

	enum{
		RetOK	 = 0,
		FieldCh  = 1,
		FieldLun = 2,
		FieldPl  = 3,
		FieldBlk = 4,
		FieldOOR = 5,		//Out of range

		AddrInvalid = 6,
		CalcOF = 7			//calculation: overflow/underflow occur
	};
	
	struct blk_addr get_lowest()  { return this->lowest; }
	struct blk_addr get_highest() { return this->highest; }
    size_t get_blk_nr() { return BlkAddrDiff( &(this->highest), 0) + 1; }
	void convert_2_nvm_addr(struct blk_addr *blk_a, struct nvm_addr *nvm_a);

	int MakeBlkAddr(size_t ch, size_t lun, size_t pl, size_t blk, struct blk_addr* addr);
	size_t GetFieldFromBlkAddr(struct blk_addr const * addr, int field, bool isidx);
    size_t SetFieldBlkAddr(size_t val, int field, struct blk_addr * addr, bool isidx);

	/*
	 * @blk_addr + @x 
	 * warning - @blk_addr will be change 
	 */
	void BlkAddrAdd(size_t x, struct blk_addr* addr);
	
	/*
	 * @blk_addr - @x 
	 * warning - @blk_addr will be change  
	 */
	void BlkAddrSub(size_t x, struct blk_addr* addr);
	
	/*
	 * return -1 if @lhs < @rhs; 
	 * 		  0  if @lhs == @rhs;
	 * 		  1  if @lhs > @rhs; 
	 */
	int BlkAddrCmp(const struct blk_addr* lhs, const struct blk_addr* rhs);

	bool CalcOK();
	
    /*
     * return @addr - @_lowest, if mode == 0; 
     * return @_highest - @addr, if mode == 1;
     */
    size_t BlkAddrDiff(const struct blk_addr* addr, int mode);

	/*
	 * valid value range: [0, field_limit_val).
	 * @ret - 0 - OK. 
	 *      - otherwise return value means corresponding field not valid.
	 */
	int BlkAddrValid(size_t ch, size_t lun, size_t pl, size_t blk);

    void PrBlkAddr(struct blk_addr *addr, bool pr_title, const char *prefix);
private:

	struct AddrFormat {
		int ch;
		int lun;
		int pl;
		int blk;
		AddrFormat()
			: ch(0), lun(3), pl(2), blk(1)
		{
		}
	};

	void init();
	void do_sub_or_add(size_t x, struct blk_addr* addr, int mode);
	void do_sub(ssize_t* aos, ssize_t* v, struct blk_addr *addr);
	void do_add(ssize_t* aos, ssize_t* v, struct blk_addr *addr);
public:
	struct nvm_geo const * geo_;
private:
	struct addr_meta const *tm_; 
	int status_;						//status of last operation. 
	AddrFormat format_;

	struct blk_addr lowest;
	struct blk_addr highest;

	int lmov_[4];
	uint64_t mask_[4];
	size_t usize_[4];
};

class BlkAddrHandle{
public:
	BlkAddrHandle(const struct nvm_geo* geo);
	~BlkAddrHandle();

	blk_addr_handle* get_blk_addr_handle( size_t nch );

	std::string txt();
private:
    const struct nvm_geo* geo_;
    BlkAddrHandle* bah;

	struct addr_meta *am;
	blk_addr_handle **blk_addr_handlers_of_ch;
	size_t nchs;
};

void addr_init(const struct nvm_geo *g);

#endif
