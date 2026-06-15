#pragma once
#include "sfxr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFXA_SFXR_PRESET_COUNT 9

void gfxa_sfxr_preset(int idx, afx_t *p);
void gfxa_sfxr_random(afx_t *p);
void gfxa_sfxr_mutate(afx_t *p);

#ifdef __cplusplus
}
#endif
