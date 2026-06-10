#pragma once
#include "sfxr.h"

#define GFXA_SFXR_PRESET_COUNT 8

/* Preenche params com o gerador de preset índice idx (0..PRESET_COUNT-1) */
void gfxa_sfxr_preset(int idx, float params[GFXA_SFXR_PARAM_COUNT]);

/* Nome do preset índice idx */
const char *gfxa_sfxr_preset_name(int idx);

/* Parâmetros aleatórios */
void gfxa_sfxr_random(float params[GFXA_SFXR_PARAM_COUNT]);

/* Muta os parâmetros in-place */
void gfxa_sfxr_mutate(float params[GFXA_SFXR_PARAM_COUNT]);
