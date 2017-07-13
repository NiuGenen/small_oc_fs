# small_oc_fs

- super_block : sb
- meta_block_area : mba
	- file_meta : fm
	- file_name : fn
	- extent    : ext
- NAT table : nat
- blk_addr_handle : bah
- liblightnvm.h

# OCSSD

- channel
- lun
- plane
- block
- page
- sector

# small_oc_fs Layout

- `struct ocssd_geo_`
	- sb_nbytes : size of sb_meta, much less than 1 block
	- nat_fn_blk_nr  : number of block to store file_name   NAT table
	- nat_fm_blk_nr  : number of block to store file_meta   NAT table
	- nat_ext_blk_nr : numbet of block to store extent tree NAT table
	- fn_blk_nr  : number of block to store file_name obj
	- fm_blk_nr  : number of block to store file_meta obj
	- ext_blk_nr : number of block to store extent    obj
- SSD Layout
	- all blocks as blks[ 0 , ssd_size - 1 ]
	- next time I will write this