#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFXA_SFXR_PARAM_COUNT 24

#define GFXA_SFXR_WAVE_TYPE      0
#define GFXA_SFXR_ATTACK         1
#define GFXA_SFXR_SUSTAIN_TIME   2
#define GFXA_SFXR_SUSTAIN_PUNCH  3
#define GFXA_SFXR_DECAY          4
#define GFXA_SFXR_BASE_FREQ      5
#define GFXA_SFXR_FREQ_LIMIT     6
#define GFXA_SFXR_FREQ_RAMP      7
#define GFXA_SFXR_FREQ_DRAMP     8
#define GFXA_SFXR_VIB_STRENGTH   9
#define GFXA_SFXR_VIB_SPEED     10
#define GFXA_SFXR_ARP_MOD       11
#define GFXA_SFXR_ARP_SPEED     12
#define GFXA_SFXR_DUTY          13
#define GFXA_SFXR_DUTY_RAMP     14
#define GFXA_SFXR_REPEAT_SPEED  15
#define GFXA_SFXR_PHA_OFFSET    16
#define GFXA_SFXR_PHA_RAMP      17
#define GFXA_SFXR_LPF_FREQ      18
#define GFXA_SFXR_LPF_RAMP      19
#define GFXA_SFXR_LPF_RESONANCE 20
#define GFXA_SFXR_HPF_FREQ      21
#define GFXA_SFXR_HPF_RAMP      22
#define GFXA_SFXR_SOUND_VOL     23

#define GFXA_SFXR_WAVE_SQUARE   0
#define GFXA_SFXR_WAVE_SAWTOOTH 1
#define GFXA_SFXR_WAVE_SINE     2
#define GFXA_SFXR_WAVE_NOISE    3

struct sfxr_state;

struct sfxr_state *gfxa_sfxr_create(const float params[GFXA_SFXR_PARAM_COUNT]);
int  gfxa_sfxr_read(struct sfxr_state *s, int16_t *buf, int n);
void gfxa_sfxr_destroy(struct sfxr_state *s);

void gfxa_sfxr_play(const float params[GFXA_SFXR_PARAM_COUNT]);
void gfxa_sfxr_play_str(const char *param_str);
void gfxa_sfxr_play_json(const char *json_str);
void gfxa_sfxr_set_callback(void (*cb)(bool));

int  gfxa_sfxr_parse(const char *str, float params[GFXA_SFXR_PARAM_COUNT]);
int  gfxa_sfxr_parse_json(const char *json, float params[GFXA_SFXR_PARAM_COUNT]);
void gfxa_sfxr_defaults(float params[GFXA_SFXR_PARAM_COUNT]);

#ifdef __cplusplus
}
#endif
