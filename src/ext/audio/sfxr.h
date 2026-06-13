#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFXA_SFXR_PARAM_COUNT 24

enum {
  GFXA_SFXR_WAVE_TYPE,
  GFXA_SFXR_ATTACK,
  GFXA_SFXR_SUSTAIN_TIME,
  GFXA_SFXR_SUSTAIN_PUNCH,
  GFXA_SFXR_DECAY,
  GFXA_SFXR_BASE_FREQ,
  GFXA_SFXR_FREQ_LIMIT,
  GFXA_SFXR_FREQ_RAMP,
  GFXA_SFXR_FREQ_DRAMP,
  GFXA_SFXR_VIB_STRENGTH,
  GFXA_SFXR_VIB_SPEED,
  GFXA_SFXR_ARP_MOD,
  GFXA_SFXR_ARP_SPEED,
  GFXA_SFXR_DUTY,
  GFXA_SFXR_DUTY_RAMP,
  GFXA_SFXR_REPEAT_SPEED,
  GFXA_SFXR_PHA_OFFSET,
  GFXA_SFXR_PHA_RAMP,
  GFXA_SFXR_LPF_FREQ,
  GFXA_SFXR_LPF_RAMP,
  GFXA_SFXR_LPF_RESONANCE,
  GFXA_SFXR_HPF_FREQ,
  GFXA_SFXR_HPF_RAMP,
  GFXA_SFXR_SOUND_VOL
};

enum {
  GFXA_SFXR_WAVE_SQUARE,
  GFXA_SFXR_WAVE_SAWTOOTH,
  GFXA_SFXR_WAVE_SINE,
  GFXA_SFXR_WAVE_NOISE,
};

struct sfxr_state;

void gfxa_sfxr_defaults(float params[GFXA_SFXR_PARAM_COUNT]);

struct sfxr_state *gfxa_sfxr_create(const float params[GFXA_SFXR_PARAM_COUNT]);
int  gfxa_sfxr_read(struct sfxr_state *s, int16_t *buf, int n);
void gfxa_sfxr_destroy(struct sfxr_state *s);

void  gfxa_sfxr_set_vibrato(struct sfxr_state *s, float speed, float depth);
float gfxa_sfxr_get_period(const struct sfxr_state *s);
void  gfxa_sfxr_set_period(struct sfxr_state *s, float period);
bool  gfxa_sfxr_is_done(const struct sfxr_state *s);

int  gfxa_sfxr_play(const float params[GFXA_SFXR_PARAM_COUNT]);

int  gfxa_sfxr_pack(const float params[GFXA_SFXR_PARAM_COUNT],
                    uint8_t packed[GFXA_SFXR_PARAM_COUNT]);
void gfxa_sfxr_unpack(const uint8_t packed[GFXA_SFXR_PARAM_COUNT],
                      float params[GFXA_SFXR_PARAM_COUNT]);

int  gfxa_sfxr_to_base64(const float params[GFXA_SFXR_PARAM_COUNT],
                         char *out, int out_size);
int  gfxa_sfxr_from_base64(const char *str,
                           float params[GFXA_SFXR_PARAM_COUNT]);

#ifdef __cplusplus
}
#endif
