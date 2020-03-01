/*
 * alloc.h
 *
 *  Created on: Feb 9, 2020
 *      Author: igor
 */

#ifndef ALLOC_H_
#define ALLOC_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
/****** settings **********/
// must be 2*P
#define PG_SIZE 1024
#define PG_AMOUNT 8
/**************************/
// definitions, macro
#define PG_HALF_SIZE ((PG_SIZE)>>1)
#define ALIGNMENT 4 // must be a power of 2
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define BLK_START(X) ((X)+ALIGN(sizeof(blk_t)))
#define BLK_HEADER(X) ((X)-ALIGN(sizeof(blk_t)))
typedef enum {
	pg_free = 0,
	pg_multiblk,
	pg_singleblk,
	pg_sintermediate
} pg_blkstate_t;

union size_u {
	/* block size if it is < half page size */
	unsigned size;
	/* next pages amount used by same block */
	unsigned pgsamount;
	/* first block on this page */
};

typedef struct {
	size_t nxtblk;
	bool busy:1;
} blk_t;
typedef struct {
	union size_u blkinfo;
	blk_t* firstfreeblk;
	pg_blkstate_t st:2;
} pg_t;

// functions
extern void *mem_alloc(size_t size);
extern void *mem_realloc(void *addr, size_t size);
extern void mem_free(void *addr);
extern void mem_init(void);
extern void mem_dump(void);


#endif /* ALLOC_H_ */
