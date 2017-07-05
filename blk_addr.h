#ifndef _BLK_ADDR_H_
#define _BLK_ADDR_H_

#include <stdint.h>
#include <stddef.h>
//#include "lnvm/liblightnvm.h"

/**
 * for test : copy from liblightnvm.h
 */
struct nvm_geo {
	size_t nchannels;	///< Number of channels on device
	size_t nluns;		///< Number of LUNs per channel
	size_t nplanes;		///< Number of planes per LUN
	size_t nblocks;		///< Number of blocks per plane
	size_t npages;		///< Number of pages per block
	size_t nsectors;	///< Number of sectors per page

	size_t page_nbytes;	///< Number of bytes per page
	size_t sector_nbytes;	///< Number of bytes per sector
	size_t meta_nbytes;	///< Number of bytes for out-of-bound / metadata

	size_t tbytes;		///< Total number of bytes in geometry
};

struct nvm_addr {
	union {
		/**
		 * Address packing and geometric accessors
		 */
		struct {
			uint64_t blk	: 16;	///< Block address
			uint64_t pg	: 16;	///< Page address
			uint64_t sec	: 8;	///< Sector address
			uint64_t pl	: 8;	///< Plane address
			uint64_t lun	: 8;	///< LUN address
			uint64_t ch	: 7;	///< Channel address
			uint64_t rsvd	: 1;	///< Reserved
		} g;

		struct {
			uint64_t line		: 63;	///< Address line
			uint64_t is_cached	: 1;	///< Cache hint?
		} c;

		uint64_t ppa;				///< Address as ppa
	};
};

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

//#include <stdio.h>
struct blk_addr{
	uint64_t __buf;
	//struct blk_addr& operator = (const struct blk_addr& rhs)
	//{
	//	this->__buf = rhs.__buf;
    //    printf("this=%u\nrhs=%u\n",this->__buf,rhs.__buf);
	//	return *this;
	//}
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
	void do_sub(size_t* aos, size_t* v, struct blk_addr *addr);
	void do_add(size_t* aos, size_t* v, struct blk_addr *addr);
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

void addr_init(const nvm_geo *g);
void addr_release();

#endif
