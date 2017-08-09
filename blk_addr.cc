#include "blk_addr.h"
#include "liblightnvm.h"
#include "dbg_info.h"
#include <iostream>

blk_addr_handle::blk_addr_handle(struct nvm_dev* d, struct nvm_geo const * g, struct addr_meta const * tm)
	: dev_(d), geo_(g), tm_(tm), status_(0), format_()
{
	init();
}

blk_addr_handle::~blk_addr_handle()
{
//	if( tm_ != nullptr ){
//		free( tm_ );
//	} // it is freed bt BlkAddrHandle
}

// get binary mask of @x
template<typename XType>
int GetBinMaskBits(XType x)
{
	XType i = 1;
	int bits = 0;
	while(i < x){
		i = i << 1;
		bits++;
	}
	return bits;
}

/*
 * Block Address Format: >>>>>>>>default
 *
 *        |   ch   |  blk   |  pl   |   lun   |
 * length   _l[0]    _l[1]    _l[2]    _l[3]
 * idx      0        1        2        3
 */
void blk_addr_handle::init()
{
	int _l[4];                                                  //                  -- for example --
	_l[format_.ch] = GetBinMaskBits<size_t>(geo_->nchannels);   // format.ch  = 0   nch  = 16   10000
	_l[format_.lun] = GetBinMaskBits<size_t>(geo_->nluns);      // format.lun = 3   nlun = 8    01000
	_l[format_.pl] = GetBinMaskBits<size_t>(geo_->nplanes);     // format.pl  = 2   npl  = 16   10000
	_l[format_.blk] = GetBinMaskBits<size_t>(geo_->nblocks);    // format.blk = 1   nblk = 16   10000
    // _l = {  4,   4,   4,   3   }
    //         ch   blk  pl   lun
    //         [0]  [1]  [2]  [3]
    //printf("_l={%d,%d,%d,%d}\n", _l[0], _l[1],_l[2],_l[3]);

	usize_[format_.ch] = geo_->nchannels;
    usize_[format_.lun] = tm_->nluns;
    usize_[format_.pl] = geo_->nplanes;
	usize_[format_.blk] = geo_->nblocks;
    // usize_ = { 16,  16,  16,  8  }
    //            ch   blk  pl   lun
    //            [0]  [1]  [2]  [3]
    //printf("usize_={%d,%d,%d,%d}\n", usize_[0], usize_[1],usize_[2],usize_[3]);

	lmov_[0] = _l[1] + _l[2] + _l[3];
	lmov_[1] = _l[2] + _l[3];
	lmov_[2] = _l[3];
	lmov_[3] = 0;
    // lmov_ = { 11,  7,   3,   0 }
    //           [0]  [1]  [2]  [3]
    //printf("lmov_={%d,%d,%d,%d}\n",lmov_[0],lmov_[1],lmov_[2],lmov_[3]);

	for (int i = 0; i < 4; i++) {
		mask_[i] = 1;
		mask_[i] = (mask_[i] << _l[i]) - 1;
		mask_[i] = mask_[i] << lmov_[i];
	}
    //printf("mask_={%d,%d,%d,%d}\n",mask_[0],mask_[1],mask_[2],mask_[3]);
    //                                                                           | nch=16  | nblk=16 | npl=16  | nlun=8
    // addr_buf = 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000   0|0 0 0   0|0 0 0   0|0 0 0   0|0 0 0
    //                                                                           |    ch   |   blk   |   pl    | lun |
    // mask_[0] = 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000   0|1 1 1   1|0 0 0   0|0 0 0   0|0 0 0
    // mask_[1] = 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000   0|0 0 0   0|1 1 1   1|0 0 0   0|0 0 0
    // mask_[2] = 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000   0|0 0 0   0|0 0 0   0|1 1 1   1|0 0 0
    // mask_[3] = 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000   0|0 0 0   0|0 0 0   0|0 0 0   0|1 1 1

	MakeBlkAddr(tm_->ch, tm_->stlun, 0, 0, &lowest);
	MakeBlkAddr(tm_->ch, tm_->stlun + tm_->nluns - 1, geo_->nplanes - 1, geo_->nblocks - 1, &highest);
}

void blk_addr_handle::convert_2_nvm_addr(struct blk_addr *blk_a, struct nvm_addr *nvm_a)
{
	uint64_t tmp[4];
	for (int idx = 0; idx < 4; idx++) {
		tmp[idx] = (blk_a->__buf & mask_[idx]) >> lmov_[idx];
	}
	nvm_a->ppa = 0;
	nvm_a->g.ch = tmp[format_.ch];
	nvm_a->g.lun = tmp[format_.lun];
	nvm_a->g.pl = tmp[format_.pl];
	nvm_a->g.blk = tmp[format_.blk];
}

int blk_addr_handle::MakeBlkAddr(size_t ch,
	size_t lun,
	size_t pl,
	size_t blk,
	struct blk_addr* addr)
{
    addr->__buf = 0;

	uint64_t tmp[4];
	int ret = BlkAddrValid(ch, lun, pl, blk); ///Warning - @ch must be same as ext_meta.ch
	if(ret){
		return ret;
	}

	tmp[format_.ch] = ch;
	tmp[format_.lun] = lun;
	tmp[format_.pl] = pl;
	tmp[format_.blk] = blk;

	addr->__buf = 0;

	for(int i = 0; i < 4; i++){
		tmp[i] = tmp[i] << lmov_[i];
		addr->__buf |= tmp[i];
	}

    return RetOK;
}

size_t blk_addr_handle::GetFieldFromBlkAddr(struct blk_addr const *addr, int field, bool isidx)
{
	int idx;
	size_t value;
	if (isidx) {
		idx = field;
	} else {
		switch (field) {
		case blk_addr_handle::FieldCh:
			idx = format_.ch; break;
		case blk_addr_handle::FieldLun:
			idx = format_.lun; break;
		case blk_addr_handle::FieldPl:
			idx = format_.pl; break;
		case blk_addr_handle::FieldBlk:
			idx = format_.blk; break;
		default:
			return -FieldOOR;
		}
	}

	value = (addr->__buf & mask_[idx]) >> lmov_[idx];
	return value;
}

size_t blk_addr_handle::SetFieldBlkAddr(size_t val, int field, struct blk_addr *addr, bool isidx)
{
	int idx;
	size_t org_value;

	if (isidx) {
		idx = field;
	} else {
		switch (field) {
		case blk_addr_handle::FieldCh:
			idx = format_.ch; break;
		case blk_addr_handle::FieldLun:
			idx = format_.lun; break;
		case blk_addr_handle::FieldPl:
			idx = format_.pl; break;
		case blk_addr_handle::FieldBlk:
			idx = format_.blk; break;
		default:
			return -FieldOOR;
		}
	}

	org_value = GetFieldFromBlkAddr(addr, idx, true);

	if (val > usize_[idx]) {
		return -AddrInvalid;
	}

	addr->__buf = addr->__buf & (~mask_[idx]);                              // clean
	addr->__buf = addr->__buf | (static_cast<uint64_t>(val) << lmov_[idx]); // set

	return org_value;                                               // return original value
}
/*
 * @blk_addr + @x
 * warning - @blk_addr will be change
 */
void blk_addr_handle::BlkAddrAdd(size_t x, struct blk_addr *addr)
{
	do_sub_or_add(x, addr, 1);
}

/*
 * @blk_addr - @x
 * warning - @blk_addr will be change
 */
void blk_addr_handle::BlkAddrSub(size_t x, struct blk_addr *addr)
{
	do_sub_or_add(x, addr, 0);
}

/*
 * return -1 if @lhs < @rhs;
 * 		  0  if @lhs == @rhs;
 * 		  1  if @lhs > @rhs;
 */
int blk_addr_handle::BlkAddrCmp(const struct blk_addr *lhs, const struct blk_addr *rhs)
{
	if (lhs->__buf == rhs->__buf) {
		return 0;
	}else if(lhs->__buf < rhs->__buf){
		return -1;
	}else{
		return 1;
	}
}

bool blk_addr_handle::CalcOK()
{
	return status_ == 0;
}

/*
 * return @addr - @lowest, if mode == 0;
 * return @highest - @addr, if mode == 1;
 */
size_t blk_addr_handle::BlkAddrDiff(const struct blk_addr *addr, int mode)
{
	const struct blk_addr *lhs = mode == 0 ? addr : &highest;
	const struct blk_addr *rhs = mode == 0 ? &lowest : addr;
	size_t diff = 0;
	size_t unit = 1;
	size_t v1[4], v2[4];

	// 0 -- 1 -- 2 -- 3
	// high --------- low
	for (int i = 3; i >= 0; i--) {
		v1[i] = static_cast<size_t>((lhs->__buf & mask_[i]) >> lmov_[i]);
		v2[i] = static_cast<size_t>((rhs->__buf & mask_[i]) >> lmov_[i]);
		diff += (v1[i] - v2[i]) * unit;
		unit = unit * usize_[i];
	}
	return diff;
}

/*
 * valid value range: [0, field_limit_val).
 * @ret - 0 - OK.
 *      - otherwise return value means corresponding field not valid.
 */
int blk_addr_handle::BlkAddrValid(size_t ch, size_t lun, size_t pl, size_t blk)
{
	//if (ch != tm_->ch)
	//	return -FieldCh;
	//if (!ocssd::InRange<size_t>(lun, tm_->stlun, tm_->stlun + tm_->nluns - 1))
    //	return -FieldLun;
	//if (!ocssd::InRange<size_t>(pl, 0, geo_->nplanes - 1))
	//	return -FieldPl;
	//if (!ocssd::InRange<size_t>(blk, 0, geo_->nblocks - 1))
	//	return -FieldBlk;

	return RetOK;
}
/*
 *  do sub, if @mode == 0
 *  do add, if @mode == 1
 */
void blk_addr_handle::do_sub_or_add(size_t x, struct blk_addr *addr, int mode)
{
	ssize_t aos[4]; //add or sub
	ssize_t v[4];
	int i;
	if (x > BlkAddrDiff(addr, mode)) { // out of range [ lowest, highest ]
		status_ = -CalcOF;
		return ;
	}

    // usize_ = { 16,  16,  16,  8  }
    //            ch   blk  pl   lun
    //            [0]  [1]  [2]  [3]
	for (i = 3; i >= 0; i--) {
		aos[i] = static_cast<ssize_t>( x % usize_[i] );
		x = x / usize_[i];
		v[i] = static_cast<ssize_t>((addr->__buf & mask_[i]) >> lmov_[i]);
	}

	if (mode == 0) {
		do_sub(aos, v, addr);
	} else {
		do_add(aos, v, addr);
	}

	addr->__buf = 0;
	for(i = 0; i < 4; i++){
		addr->__buf |= static_cast<uint64_t>(v[i]) << lmov_[i];
	}
}
void blk_addr_handle::do_sub(ssize_t *aos, ssize_t *v, struct blk_addr *addr)
{
	for (int i = 3; i >= 0; i--) {
		if (v[i] < aos[i]) {
			v[i - 1]--;
			v[i] = static_cast<ssize_t>( usize_[i] + v[i] - aos[i] );
		} else {
			v[i] -= aos[i];
		}
	}
	v[format_.lun] += tm_->stlun;
}
void blk_addr_handle::do_add(ssize_t *aos, ssize_t *v, struct blk_addr *addr)
{
	for (int i = 3; i >= 0; i--) {
		v[i] += aos[i];
		if (v[i] >= static_cast<ssize_t>( usize_[i]) ) {
			v[i - 1]++;
			v[i] -= static_cast<ssize_t>( usize_[i] );
		}
	}
	v[format_.lun] += tm_->stlun;
}

void blk_addr_handle::PrBlkAddr(struct blk_addr *addr, bool pr_title, const char *prefix)
{
	size_t vals;
	const char *optitles[4];
	int i;

	optitles[format_.ch] = "ch";
	optitles[format_.lun] = "lun";
	optitles[format_.pl] = "pl";
	optitles[format_.blk] = "blk";

	if (pr_title) {
		printf("%s", prefix);
		for(i = 0; i < 4; i++){
			printf("%8s ", optitles[i]);
		}
		printf("\n");
	}
	printf("%s", prefix);
	for(i = 0; i < 4; i++){
		vals = GetFieldFromBlkAddr(addr, i, true);
		printf("%8zu ", vals);
	}
	printf("\n");
}

void blk_addr_handle::erase_all_block()
{
	OCSSD_DBG_INFO( this, "   - erase ch " << tm_->ch );
	struct nvm_addr* nvm_address = new struct nvm_addr;
    nvm_address->ppa = 0;
	struct nvm_vblk* nvm_addr_vblk = nullptr;
	struct blk_addr blk_address = this->lowest;
	size_t blk_nr = this->get_blk_nr();
//	struct nvm_ret ret;
	for(size_t c = 0; c < blk_nr; ++c){
		this->convert_2_nvm_addr( &blk_address, nvm_address );
		this->BlkAddrAdd( (size_t)1, &blk_address );
//		nvm_addr_erase( dev_, nvm_address, 1 , NVM_FLAG_PMODE_SNGL ,&ret );
		nvm_addr_vblk = nvm_vblk_alloc( dev_, nvm_address, 1 );
		nvm_vblk_erase( nvm_addr_vblk );
//		nvm_ret_pr( &ret );
	}
}

std::string blk_addr_handle::txt() {
	return "blk_addr_handle";
}

// global vars
BlkAddrHandle* ocssd_bah;

BlkAddrHandle::BlkAddrHandle(struct nvm_dev* dev, const struct nvm_geo* geo)
{
	OCSSD_DBG_INFO( this, "BlkAddrHandle(const struct nvm_geo* geo)");

	this->dev_ = dev;
	this->geo_ = geo;

	size_t i;
	this->nchs = geo->nchannels;

	am = (addr_meta *)malloc(sizeof(addr_meta) * nchs);
	if (!am) {
	}

	blk_addr_handlers_of_ch = (blk_addr_handle **)malloc(sizeof(blk_addr_handle *) * nchs);
	if (!blk_addr_handlers_of_ch) {
	}

	for (i = 0; i < nchs; i++) {
		am[i].ch = i;
		am[i].nluns = geo->nluns;
		am[i].stlun = 0;

		blk_addr_handlers_of_ch[i] = new blk_addr_handle(dev, geo, am + i);
	}
}

BlkAddrHandle::~BlkAddrHandle()
{
	if (am) {
		free(am);
	}
	for (size_t i = 0; i < nchs; i++) {
		if (blk_addr_handlers_of_ch[i]) {
			delete blk_addr_handlers_of_ch[i];
		}
	}
	if (blk_addr_handlers_of_ch) {
		free(blk_addr_handlers_of_ch);
	}
}

blk_addr_handle* BlkAddrHandle::get_blk_addr_handle(size_t nch) {
	if( nch >= nchs ) return nullptr;
	return blk_addr_handlers_of_ch[nch];
}

void BlkAddrHandle::erase_all_block() {
	for(size_t ch = 0; ch < this->nchs; ++ch ){
		blk_addr_handlers_of_ch[ ch ]->erase_all_block();
	}
}

std::string BlkAddrHandle::txt()
{
	return "BlkAddrHandle";
}

void addr_init(struct nvm_dev* dev, const struct nvm_geo *geo)
{
    if( ocssd_bah == nullptr ) {
		ocssd_bah = new BlkAddrHandle(dev, geo);
	}
}

void addr_free(){
	delete ocssd_bah;
}