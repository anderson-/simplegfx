#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  GFXA_SFXR_WAVE_SQUARE,
  GFXA_SFXR_WAVE_SAWTOOTH,
  GFXA_SFXR_WAVE_SINE,
  GFXA_SFXR_WAVE_NOISE,
};

// Unsigned fields: 0 = 0.0, 255 = 1.0
// Signed fields:   0 = -1.0, 128 = 0.0, 255 = +1.0
typedef struct {
  uint8_t wave_type;      // 0=square, 1=sawtooth, 2=sine, 3=noise
  uint8_t attack;         // unsigned
  uint8_t sustain_time;   // unsigned
  uint8_t sustain_punch;  // unsigned
  uint8_t decay;          // unsigned
  uint8_t base_freq;      // unsigned
  uint8_t freq_limit;     // unsigned
  uint8_t freq_ramp;      // signed, 128=center
  uint8_t freq_dramp;     // signed, 128=center
  uint8_t vib_strength;   // unsigned
  uint8_t vib_speed;      // unsigned
  uint8_t arp_mod;        // signed, 128=center
  uint8_t arp_speed;      // unsigned
  uint8_t duty;           // unsigned
  uint8_t duty_ramp;      // signed, 128=center
  uint8_t repeat_speed;   // unsigned
  uint8_t pha_offset;     // signed, 128=center
  uint8_t pha_ramp;       // signed, 128=center
  uint8_t lpf_freq;       // unsigned
  uint8_t lpf_ramp;       // signed, 128=center
  uint8_t lpf_resonance;  // unsigned
  uint8_t hpf_freq;       // unsigned
  uint8_t hpf_ramp;       // signed, 128=center
  uint8_t sound_vol;      // unsigned
} afx_t;

typedef struct {
  int slot_id;
  int audio_ch;
  float period;
  float period_max;
  float pmul;
  float pslide;
  float duty;
  float dslide;
  float arp_mult;
  float fltw;
  float fltw_d;
  float fltdmp;
  float fltp;
  float fltdp;
  float flthp;
  float flthp_d;
  float fltphp;
  float vib_speed;
  float vib_amp;
  float vib_phase;
  float env_punch;
  float flg_off;
  float flg_sl;
  float gain;
  float accum;
  int arp_time;
  int phase;
  int env_len[3];
  int env_elapsed;
  int summands;
  int summed;
  int t;
  int16_t flg_buf[128];
  int16_t noise[32];
  uint16_t rpt_elapsed;
  uint16_t rpt_time;
  uint16_t sample_rate;
  uint8_t freq_cut;
  uint8_t wave_shape;
  uint8_t en_lpf;
  uint8_t env_stage;
  uint8_t flg_pos;
  uint8_t done;
} sfxr_state_t;

void gfxa_sfxr_defaults(afx_t *p);
sfxr_state_t *gfxa_sfxr_play(int id, const afx_t *p, int ch);

#ifdef __cplusplus
}
#endif
