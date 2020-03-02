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
static bool init_pg_multiblk(int pgnum, size_t size);
/*
 * init pgs with huge blk
 */
static bool init_pgs_singleblk(int pgnum, size_t size);
/*
 * returns first free blk, if not found return is NULL
 */
static blk_t* get_freeblk_pg(int pgnum);
/*
 * returns next block inside current page
 * if no such block is found returns NULL
 */
static blk_t* get_nextblk(blk_t* blk, int pgnum);
static int get_pg_region(void*addr);
static blk_t* get_blk_region(void*addr, int pgnum);
static bool busy_region(void*addr);
static int get_amount_freeblk_pg(int pg);
static int get_amount_blk_pg(int pg);
static void pg_refresh_blkinfo(int pgnum);
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
 * if not found return -1,
 * page must have at least 1 free block
 */
static int get_pg_class(size_t size)
{
	for(int i=0; i<PG_AMOUNT; i++) {
		if(pgs[i].st == pg_multiblk &&
				pgs[i].blkinfo.size == size&&
				pgs[i].freeblkamount > 0) {
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

static int get_amount_freeblk_pg(int pg)
{
	int num = 0;
	for(blk_t*blk = (blk_t*)&array[pg*PG_SIZE]; blk != NULL;) {
		if(!blk->busy)
			num++;
		blk = get_nextblk(blk, pg);
	}
	return num;
}


static int get_amount_blk_pg(int pg)
{
	int num = 0;
	for(blk_t*blk = (blk_t*)&array[pg*PG_SIZE]; blk != NULL;) {
		num++;
		blk = get_nextblk(blk, pg);
	}
	return num;
}

static bool init_pg_multiblk(int pgnum, size_t size)
{
	if(pgs[pgnum].st != pg_free ||
			size > PG_HALF_SIZE || pgnum > PG_AMOUNT) {
		return false;
	}
	pgs[pgnum].st = pg_multiblk;
	pgs[pgnum].blkinfo.size = size;
	pgs[pgnum].firstfreeblk = (blk_t*)&array[pgnum*PG_SIZE];
	blk_t* iblk = (blk_t*)&array[pgnum*PG_SIZE];
	// fill blk info
	while(iblk != NULL) {
		iblk->busy = false;
		iblk->nxtblk = size;
		// goto next
		iblk = get_nextblk(iblk, pgnum);
	}
	// save info about free blks
	pgs[pgnum].freeblkamount = get_amount_freeblk_pg(pgnum);
	return true;
}
/*
 * returns first free blk, if not found return is NULL
 */
static blk_t* get_freeblk_pg(int pgnum)
{
	pg_t *pg = &pgs[pgnum];
	// verify if its applicable
	if(pg->st != pg_multiblk)
		return NULL;
	blk_t*iblk = (blk_t*)&array[pgnum*PG_SIZE];
	while(iblk != NULL) {
		if(!iblk->busy)
			return iblk;
		// goto next
		iblk = get_nextblk(iblk, pgnum);
	}
	return NULL;
}

/*
 * returns next block inside current page
 * if no such block is found returns NULL
 */
static blk_t* get_nextblk(blk_t* blk, int pgnum)
{
	// if not applicable
	if(pgs[pgnum].st != pg_multiblk)
		return NULL;
	uint8_t* nxt = ((uint8_t*)blk + blk->nxtblk + ALIGN(sizeof(blk_t)));
	if(BLK_START(nxt) + blk->nxtblk < &array[(pgnum+1)*PG_SIZE] && nxt > &array[(pgnum)*PG_SIZE]) {
		return (blk_t*)nxt;
	}
	return NULL;
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
		// NULL means no free blk
		if(blk != NULL) {
			assert(!blk->busy);
			blk->busy = true;
			// because changing block info we need to update page info
			pg_refresh_blkinfo(pgnum);
			return BLK_START(blk);
		}
	}
	return NULL;
}

static void pg_refresh_blkinfo(int pgnum)
{
	assert(pgnum >= 0);
	pgs[pgnum].firstfreeblk = get_freeblk_pg(pgnum);
	pgs[pgnum].freeblkamount = get_amount_freeblk_pg(pgnum);
	if(pgs[pgnum].freeblkamount == get_amount_blk_pg(pgnum)) {
		// set pg state to abs free
		pgs[pgnum].st = pg_free;
	}
}

static int get_pg_region(void*addr)
{
	for(int pg=0; pg<PG_AMOUNT; pg++) {
		if((uint8_t*)addr < &array[pg*PG_SIZE] &&
				(uint8_t*)addr >= &array[(pg-1)*PG_SIZE]) {
			return pg-1;
		}
	}
	return -1;
}

static blk_t* get_blk_region(void*addr, int pgnum)
{
	for(blk_t* b=(blk_t*)&array[pgnum*PG_SIZE]; b!=NULL;) {
		if(addr >= (void*)b && (uint8_t*)addr < BLK_START(b) + b->nxtblk) {
			return b;
		}
		b = get_nextblk(b, pgnum);
	}
	return NULL;
}
static bool busy_region(void*addr)
{
	int pg = get_pg_region(addr);
	blk_t*b = get_blk_region(addr, pg);
	if(b == NULL) {
		pr_err("busy_region(): request out of region");
		return false;
	}
	return b->busy;
}

void *mem_realloc(void *addr, size_t size)
{
	return NULL;
}
void mem_free(void *addr)
{
	int pg = get_pg_region(addr);
	if(pg < 0 || pgs[pg].st == pg_free) {
		pr_err("invalid mem_free() call");
		return;
	}
	blk_t* b = get_blk_region(addr, pg);
	assert(b != NULL);
	if(!b->busy) {
		pr_err("invalid mem_free() call, blk was already free");
		return;
	}
	if(pgs[pg].st == pg_singleblk) {
		// TODO
	} else if(pgs[pg].st == pg_multiblk) {
		b->busy = false;
		// we need to update this page info
		pg_refresh_blkinfo(pg);
	}
}
void mem_init(void);
void mem_dump(void)
{
	pr_info("\tmem dump:");
	const int chperpg = 80;
	const int bperchar = PG_SIZE / chperpg;
	// pgs
	for(int pg=0; pg<PG_AMOUNT; pg++) {
		//blocks
		printf("[%i] addr=0x%04x\t", pg, &array[pg*PG_SIZE]);
		blk_t* prev;
		for(uint8_t* addr=&array[pg*PG_SIZE];
				addr < &array[(pg+1)*PG_SIZE]; addr += bperchar) {
			blk_t*blk = get_blk_region(addr, pg);
			if(blk == NULL)
				putchar('?');
			else if(blk != prev)
				putchar('!');
			else
				putchar(busy_region(addr)? '#':'-');
			prev = blk;
		}
		puts("");
	}
}
