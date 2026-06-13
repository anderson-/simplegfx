#pragma once
#include "sfxr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFXA_SFXR_PRESET_COUNT 9

void gfxa_sfxr_preset(int idx, float params[GFXA_SFXR_PARAM_COUNT]);

const char *gfxa_sfxr_preset_name(int idx);

void gfxa_sfxr_random(float params[GFXA_SFXR_PARAM_COUNT]);
void gfxa_sfxr_mutate(float params[GFXA_SFXR_PARAM_COUNT]);

#ifdef __cplusplus
}
#endif
