/*
 * RNG.c
 *
 *  Created on: Feb 16, 2019
 *      Author: igor
 */

#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include "RNG.h"

RNG_t RNG;
static float RNG_get_float( float from, float to );
static int RNG_get_int(int from, int to);

void RNG_init(void)
{
        time_t UNIX_time;
	srand((unsigned)time(&UNIX_time));
	RNG.get_float = RNG_get_float;
	RNG.get_int = RNG_get_int;
	RNG.init = RNG_init;
	RNG.is_initialized = true;
}

static int RNG_get_int(int from, int to)
{
	return (from + rand() / (RAND_MAX / (to - from + 1) + 1));
}

static float RNG_get_float( float from, float to )
{
        float scale;
	scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
	return from + scale * ( to - from );      /* [from, to] */
}
