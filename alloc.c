/*
 * alloc.c
 *
 *  Created on: Feb 9, 2020
 *      Author: igor
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "alloc.h"
#include "defs.h"
//local vars
static pg_t pgs[PG_AMOUNT] __attribute__ ((aligned (8)));
static uint8_t array[PG_AMOUNT*PG_SIZE];
/*****************local functions*******************/
/*
 * calculate page(s) for requested blk size
 */
static pg_t calc_pg_blk(size_t sz);
static size_t calc_blk_class(size_t sz);
/*
 * return num of page with class specified,
 * if not found return -1
 */
static int get_pg_class(size_t size);

/*
 * return num of first free pages(amount), if not found return -1
 */
static int get_pgs_free(int amount);
/*
 * init pg with small blks
 */
bool init_pg_multiblk(int pgnum, size_t size);
/*
 * init pgs with huge blk
 */
bool init_pgs_singleblk(int pgnum, size_t size);
/************end  local functions*******************/

/*
 * calculate page(s) for requested blk size
 */
static pg_t calc_pg_blk(size_t sz)
{
	pg_t pg;
	// if we can put this block to half 1 pg?
	if(PG_HALF_SIZE > sz) {
		// if so we can put > 1 blk to 1 pg
		pg.st = pg_multiblk;
		// save blk size
		pg.blkinfo.size = calc_blk_class(sz);
	} else {
		// if not so determine amount of pgs for such blk
		pg.st = pg_singleblk;
		// determine amount of pages
		pg.blkinfo.pgsamount = sz / PG_SIZE;
		// add remainder(1 page) if needed
		pg.blkinfo.pgsamount += ((sz % PG_SIZE) > 0) ? 1 : 0;
	}
	return pg;
}

static size_t calc_blk_class(size_t sz)
{
	// first class = (2^ALIGNMENT)
	size_t c=1<<ALIGNMENT;
	while(sz > c) {
		c<<=1;
	}
	return c;
}
/*
 * return num of page with class specified,
 * if not found return -1
 */
static int get_pg_class(size_t size)
{
	for(int i=0; i<PG_AMOUNT; i++) {
		if(pgs[i].st == pg_multiblk && pgs[i].blkinfo.size == size) {
			return i;
		}
	}
	return -1;
}
/*
 * return num of first free pages(amount), if not found return -1
 */
static int get_pgs_free(int amount)
{
	for(int i=0; i<PG_AMOUNT; i++) {
		if(pgs[i].st == pg_free) {
			int num = 0;
			for(int j=i; j<PG_AMOUNT; j++) {
				if(pgs[j].st == pg_free && num < amount)
					num++;
				else
					break;
			}
			if(num >= amount)
				return i;
		}
	}
	return -1;
}

bool init_pg_multiblk(int pgnum, size_t size)
{
	if(pgs[pgnum].st != pg_free ||
			size > PG_HALF_SIZE || pgnum > PG_AMOUNT) {
		return false;
	}
	pgs[pgnum].st = pg_multiblk;
	pgs[pgnum].blkinfo.size = size;
	pgs[pgnum].firstfreeblk = (blk_t*)&array[pgnum*PG_SIZE];
	uint8_t* iblk = &array[pgnum*PG_SIZE];
	// fill blk info
	for(; iblk < &array[(pgnum+1)*PG_SIZE];) {
		((blk_t*)iblk)->busy = false;
		((blk_t*)iblk)->nxtblk = size;
		// goto next
		iblk = iblk + ((blk_t*)iblk)->nxtblk + ALIGN(sizeof(blk_t));
	}
	return true;
}
/* visible functions */
void *mem_alloc(size_t size)
{
	pg_t likelypg = calc_pg_blk(size);
	int pgnum;
	if(likelypg.st == pg_multiblk) {
		// find 1st page with the same size class
		pgnum = get_pg_class(size);
		if(pgnum < 0) {
			// not found such page, getting new one
			pgnum = get_pgs_free(1);
			if(pgnum < 0) {
				// this means no such pgs available
				return NULL;
			}
			// initialize new page
			assert(init_pg_multiblk(pgnum, size));
		}
		// get first free blk from this page
		blk_t* blk = pgs[pgnum].firstfreeblk;
		assert(!blk->busy);
		blk->busy = true;
		pgs[pgnum].firstfreeblk = (blk_t*)((uint8_t*)blk + blk->nxtblk + ALIGN(sizeof(blk_t)));
		return blk;
	}
	return NULL;
}

void *mem_realloc(void *addr, size_t size)
{
	return NULL;
}
void mem_free(void *addr)
{

}
void mem_init(void);
void mem_dump(void)
{

}
