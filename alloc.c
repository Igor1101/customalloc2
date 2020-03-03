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
static bool init_pgs_singleblk(int pgnum, pg_t pgex);
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
// alloc funs
static void *alloc_multiblk(size_t size);
static void* alloc_singleblk(pg_t pg);
// freeing
static void free_single(blk_t*b, int pg);
static void free_multiple(blk_t*b, int pg);
// realloc
static void* realloc_multiblk(int pg, blk_t*blk, size_t size);
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
		pg.blkinfo.pgsamount = (sz+ALIGN(sizeof(blk_t)) )/ PG_SIZE;
		// add remainder(1 page) if needed
		pg.blkinfo.pgsamount += (((sz + ALIGN(sizeof(blk_t))) % PG_SIZE) > 0) ? 1 : 0;
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
 * init pgs with huge blk
 */
static bool init_pgs_singleblk(int pgnum, pg_t pgex)
{
	for(int i=pgnum; i<pgnum+pgex.blkinfo.pgsamount; i++) {
		if(i >= PG_AMOUNT) {
			pr_err("init_pgs_singleblk() out of memory");
			return false;
		}
		if(pgs[i].st != pg_free) {
			pr_err("init_pgs_singleblk() requested pg nonfree");
			return false;
		}
	}
	pgs[pgnum] = pgex;
	pgs[pgnum].firstfreeblk = NULL;
	// fill other pages
	for(int i=pgnum; i<pgnum+pgex.blkinfo.pgsamount; i++) {
		pgs[i].st = pg_sintermediate;
	}
	pgs[pgnum].st = pg_singleblk;
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
static void *alloc_multiblk(size_t size)
{
	// find 1st page with the same size class
	int pgnum = get_pg_class(size);
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
	return NULL;
}

static void* alloc_singleblk(pg_t pg)
{
	assert(pg.st == pg_singleblk);
	int pgstart = get_pgs_free(pg.blkinfo.pgsamount);
	if(pgstart < 0)
		return NULL;
	// now make new blk inside these pages
	// firstly mark 1st page for this blk
	blk_t*b = (blk_t*)&array[pgstart*PG_SIZE];
	b->busy = true;
	b->nxtblk = PG_SIZE*pg.blkinfo.pgsamount;
	assert(init_pgs_singleblk(pgstart, pg));
	return BLK_START(b);
}
void *mem_alloc(size_t size)
{
	pg_t likelypg = calc_pg_blk(size);
	switch(likelypg.st) {
	case pg_multiblk:
		return alloc_multiblk(size);
	case pg_singleblk:
		return alloc_singleblk(likelypg);
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
		if((uint8_t*)addr < &array[(pg+1)*PG_SIZE] &&
				(uint8_t*)addr >= &array[(pg)*PG_SIZE]) {
			return pg;
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
	int pg = get_pg_region(addr);
	if(pg < 0 || pgs[pg].st == pg_free) {
		pr_err("invalid mem_realloc() call, pg is free or do not exist");
		return NULL;
	}
	blk_t* b = get_blk_region(addr, pg);
	assert(b != NULL);
	if(!b->busy) {
		pr_err("invalid mem_realloc() call, blk is free");
		return NULL;
	}
	switch(pgs[pg].st) {
	case pg_multiblk:
		return realloc_multiblk(pg, b, size);
	default:
		return NULL;
	}
}

static void* realloc_multiblk(int pg, blk_t*blk, size_t size)
{
	if(blk->nxtblk >= size) {
		// nothing to do
		return BLK_START(blk);
	}
	// otherwise get new blk
	void* newaddr = mem_alloc(size);
	if(newaddr == NULL) {
		pr_err("not enough memory to realloc");
		return NULL;
	}
	size_t oldsz = blk->nxtblk;
	memmove(newaddr, BLK_START(blk), oldsz);
	mem_free(blk);
	return newaddr;
}
void mem_free(void *addr)
{
	int pg = get_pg_region(addr);
	if(pg < 0 || pgs[pg].st == pg_free) {
		pr_err("invalid mem_free() call, pg is free or do not exist");
		return;
	}
	blk_t* b = get_blk_region(addr, pg);
	assert(b != NULL);
	if(!b->busy) {
		pr_err("invalid mem_free() call, blk was already free");
		return;
	}
	switch(pgs[pg].st) {
	case pg_singleblk:
		free_single(b, pg);
		break;
	case pg_multiblk:
		free_multiple(b, pg);
		break;
	}
}

static void free_single(blk_t*b, int pg)
{
	b->busy = false;
	for(int i=pg; i<pg+pgs[pg].blkinfo.pgsamount; i++) {
		pgs[i].st = pg_free;
	}
}
static void free_multiple(blk_t*b, int pg)
{
	b->busy = false;
	pg_refresh_blkinfo(pg);
}
void mem_init(void);
void mem_dump(void)
{
	pr_info("\tmem dump:");
	const int chperpg = 120;
	const int bperchar = PG_SIZE / chperpg;
	// pgs
	for(int pg=0; pg<PG_AMOUNT; pg++) {
		//blocks
		printf("[%i] addr=0x%04x\t%%", pg, &array[pg*PG_SIZE]);
		blk_t* prev;
		switch(pgs[pg].st) {
		case pg_free:
			for(uint8_t* addr=&array[pg*PG_SIZE];
				addr < &array[(pg+1)*PG_SIZE]; addr += bperchar) {
				putchar(' ');
			}
			break;
		case pg_sintermediate:
			for(uint8_t* addr=&array[pg*PG_SIZE];
				addr < &array[(pg+1)*PG_SIZE]; addr += bperchar) {
				putchar('#');
			}
			break;
		case pg_singleblk:
		case pg_multiblk:
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
		}
		puts("%");
	}
}
