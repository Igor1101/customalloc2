/*
 * RNG.h
 *
 *  Created on: Feb 16, 2019
 *      Author: igor
 */

#ifndef RNG_H_
#define RNG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include "RNG.h"

typedef struct {
        void(*init)(void);
        int (*get_int)(int from, int to);
        float (*get_float)(float from, float to);
        bool is_initialized;
} RNG_t;

extern RNG_t RNG;

void RNG_init(void);

#ifdef __cplusplus
}
#endif

#endif /* RNG_H_ */
