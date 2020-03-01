#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <zlib.h>
#include "alloc.h"
#include "defs.h"
#include "RNG.h"
#define NBLOCKS 8
#define NBYTES 128
#define NBYTES_REALLOC 138
struct block_alloc_t {
	void*addr;
	bool valid;
	size_t size;
	uLong chksum;
};
struct block_alloc_t blks[NBLOCKS];

void specific_test(void);
void rand_test(void);

void alloc_blks(void)
{
	for(int i=0; i<NBLOCKS; i++) {
		// allocate only not allocated before
		if(blks[i].valid == false) {
			blks[i].addr = mem_alloc(NBYTES);
			blks[i].size = NBYTES;
			// set validity
			if(blks[i].addr != NULL)
				blks[i].valid = true;
			else
				blks[i].valid = false;
		}
	}
}

void free_blk(int num)
{
	if(blks[num].valid == true)
		mem_free(blks[num].addr);
	blks[num].valid = false;
}

void realloc_blk(int num)
{
	if(blks[num].valid == false)
		return;
	int size_before = blks[num].size;
	void*addr = mem_realloc(blks[num].addr, NBYTES_REALLOC);
	if(addr != NULL) {
		blks[num].addr = addr;
		blks[num].size = NBYTES_REALLOC;
		if(crc32(0, blks[num].addr, size_before) != blks[num].chksum) {
			pr_err("CRC err, blk=%d", num);
		}
		blks[num].chksum = crc32(0, blks[num].addr, blks[num].size);
	} else {
		pr_info("not enough memory to reallocate blk=%d", num);
	}
}
/*
uint64_t blk_chksum(void*addr, size_t sz)
{
	uint64_t chksum;
	chksum = 0;
	for(int i=0; i<sz; i++) {
		chksum += ((uint8_t*)addr)[i]*(1+i);
	}
	return chksum;
}*/

void calc_allchksums(void)
{
	for(int i=0; i<NBLOCKS; i++) {
		blks[i].chksum = (blks[i].valid)?crc32(0, blks[i].addr, blks[i].size):0;
	}
}

void pr_blks(void)
{
	pr_info("blk output:");
	for(int i=0; i<NBLOCKS; i++) {
		pr_info("blk=%i, addr=%016x, sz=%d, chksm=%08x, %s", i, blks[i].addr, blks[i].size, blks[i].chksum, blks[i].valid?"valid":"invalid");
	}
}

void free_all_blks(void)
{
	for(int i=0; i<NBLOCKS; i++) {
		free_blk(i);
	}
}

void set_rand_values(int blk)
{
	if(!blks[blk].valid)
		return;
	for(int i=0; i<blks[blk].size; i++)
		((uint8_t*)blks[blk].addr)[i] = RNG.get_int(0, 255);
}

void set_all_rand_values(void)
{
	for(int i=0; i<NBLOCKS; i++)
		set_rand_values(i);
}

int get_valid_block_amount(void)
{
	int num = 0;
	for(int i=0; i<NBLOCKS; i++) {
		if(blks[i].valid)
			num++;
	}
	return num;
}

int main(int argc, char **argv)
{
	specific_test();
	//rand_test();
}
void specific_test(void)
{
	RNG_init();
	pr_info("main process started");
	memset(blks, 0, sizeof blks);
	// try allocating all blocks
	alloc_blks();
	pr_blks();
	// fill with rand
	//set_all_rand_values();
	// calc checksums for fulfilled blks
	calc_allchksums();
	if(get_valid_block_amount() == 0)
		pr_err("0 valid blocks, no blocks allocated!");
	mem_dump();
	// free some rand blks:
	pr_info("freeing rand blocks");
	free_blk(3);
	free_blk(4);
	free_blk(5);
	pr_blks();
	mem_dump();
	// realloc some rand blks:
	pr_info("Reallocating some blks");

	realloc_blk(1);
	realloc_blk(2);
	realloc_blk(0);
	mem_dump();
	pr_blks();
	free_all_blks();
}

void rand_test(void)
{
	RNG_init();
	pr_info("main process started");
	memset(blks, 0, sizeof blks);
	// try allocating all blocks
	alloc_blks();
	pr_blks();
	// fill with rand
	set_all_rand_values();
	// calc checksums for fulfilled blks
	calc_allchksums();
	if(get_valid_block_amount() == 0)
		pr_err("0 valid blocks, no blocks allocated!");
	mem_dump();
	// free some rand blks:
	pr_info("freeing rand blocks");
	for(int i=0; i<RNG.get_int(2,get_valid_block_amount()); ) {
		int new_fr = RNG.get_int(0, NBLOCKS-1);
		if(blks[new_fr].valid) {
			i++;
			free_blk(new_fr);
		}
	}
	pr_blks();
	mem_dump();
	// realloc some rand blks:
	pr_info("Reallocating some blks");
	for(int i=0; i<get_valid_block_amount(); ) {
		int blk = RNG.get_int(0, NBLOCKS-1);
		if(blks[blk].valid) {
			pr_info("blk=%d", blk);
			i++;
			int size_before = blks[blk].size;
			void*addr = mem_realloc(blks[blk].addr, NBYTES_REALLOC);
			if(addr != NULL) {
				blks[blk].addr = addr;
				blks[blk].size = NBYTES_REALLOC;

				if(crc32(0, blks[blk].addr, size_before) != blks[blk].chksum) {
					pr_err("CRC err, blk=%d", blk);
				}
				blks[blk].chksum = crc32(0, blks[blk].addr, blks[blk].size);
			}
		}
	}
	mem_dump();
	pr_blks();
	free_all_blks();
}
