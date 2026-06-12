#ifndef SIMPLEMATH_H
#define SIMPLEMATH_H

#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265f
#endif


#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int _seed;

int gfx_fast_rand(void);

int32_t gfx_fast_isin(int32_t phase);
int32_t gfx_fast_icos(int32_t phase);

#ifdef __cplusplus
}
#endif

#endif
