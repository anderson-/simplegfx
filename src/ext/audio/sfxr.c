#include "sfxr.h"
#include "simpleaudio.h"
#include "simplegfx.h"
#include "simplemath.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define SFXR_SLOTS 8

static sfxr_state_t s_slots[SFXR_SLOTS];
static volatile bool s_playing[SFXR_SLOTS];

void gfxa_sfxr_defaults(afx_t *p) {
  memset(p, 0, sizeof(*p));
  p->sustain_time  = 76;   // 0.3 * 255
  p->decay         = 102;  // 0.4 * 255
  p->base_freq     = 76;   // 0.3 * 255
  p->freq_ramp     = 128;  // signed center
  p->freq_dramp    = 128;
  p->arp_mod       = 128;
  p->duty_ramp     = 128;
  p->pha_offset    = 128;
  p->pha_ramp      = 128;
  p->lpf_freq      = 255;  // 1.0 * 255
  p->lpf_ramp      = 128;
  p->hpf_ramp      = 128;
  p->sound_vol     = 128;  // 0.5 * 255
}

static void rebuild(sfxr_state_t *s, const afx_t *p) {
  int wt = p->wave_type;
  if (wt > 3) wt = 3;

  float pa  = p->attack        / 255.0f;
  float pst = p->sustain_time  / 255.0f;
  float ppu = p->sustain_punch / 255.0f;
  float pd  = p->decay         / 255.0f;
  float pbf = p->base_freq     / 255.0f;
  float pfl = p->freq_limit    / 255.0f;
  float pfr = p->freq_ramp     / 127.5f - 1.0f;  // signed
  float pfd = p->freq_dramp    / 127.5f - 1.0f;
  float pvstrength = p->vib_strength / 255.0f;
  float pvspeed    = p->vib_speed    / 255.0f;
  float parp_mod   = p->arp_mod      / 127.5f - 1.0f;
  float parp_spd   = p->arp_speed    / 255.0f;
  float pdu = p->duty         / 255.0f;
  float pds = p->duty_ramp    / 127.5f - 1.0f;
  float prp = p->repeat_speed / 255.0f;
  float pho = p->pha_offset   / 127.5f - 1.0f;
  float phs = p->pha_ramp     / 127.5f - 1.0f;
  float plc = p->lpf_freq     / 255.0f;
  float plrm = p->lpf_ramp    / 127.5f - 1.0f;
  float plr = p->lpf_resonance / 255.0f;
  float phc = p->hpf_freq     / 255.0f;
  float phrm = p->hpf_ramp    / 127.5f - 1.0f;
  float psndvol = p->sound_vol / 255.0f;

  s->sample_rate = GFXA_SAMPLE_RATE;
  s->wave_shape = wt;
  s->fltw = plc * plc * plc * 0.1f;
  s->en_lpf = (plc != 1.0f);
  s->fltw_d = 1.0f + plrm * 0.0001f;
  s->fltdmp = 5.0f / (1.0f + plr * plr * 20.0f) * (0.01f + s->fltw);
  if (s->fltdmp > 0.8f) s->fltdmp = 0.8f;
  s->fltp = 0.0f;
  s->fltdp = 0.0f;
  s->flthp = phc * phc * 0.1f;
  s->flthp_d = 1.0f + phrm * 0.0003f;
  s->fltphp = 0.0f;
  s->vib_speed = pvspeed * pvspeed * 0.01f;
  s->vib_amp   = pvstrength * 0.5f;
  s->vib_phase = 0.0f;
  s->env_len[0] = (int)(pa * pa * 100000.0f);
  s->env_len[1] = (int)(pst * pst * 100000.0f);
  s->env_len[2] = (int)(pd * pd * 100000.0f);
  s->env_stage = 0;
  s->env_elapsed = 0;
  s->env_punch = ppu;
  memset(s->flg_buf, 0, sizeof(s->flg_buf));
  s->flg_off = (pho >= 0.0f ? 1.0f : -1.0f) * (pho * pho) * 126.0f;
  s->flg_sl  = (phs >= 0.0f ? 1.0f : -1.0f) * (phs * phs);
  s->flg_pos = 0;
  s->rpt_time = (int)((1.0f - prp) * (1.0f - prp) * 20000.0f + 32.0f);
  if (prp <= 0.0f) s->rpt_time = 0;
  s->gain = expf(psndvol) - 1.0f;
  s->period = 100.0f / (pbf * pbf + 0.001f);
  s->period_max = 100.0f / (pfl * pfl + 0.001f);
  s->freq_cut = (pfl > 0.0f);
  s->pmul = 1.0f - (pfr * pfr * pfr) * 0.01f;
  s->pslide = -(pfd * pfd * pfd) * 0.000001f;
  s->duty = 0.5f - pdu * 0.5f;
  s->dslide = -pds * 0.00005f;
  if (parp_mod >= 0.0f)
    s->arp_mult = 1.0f - (parp_mod * parp_mod) * 0.9f;
  else
    s->arp_mult = 1.0f + (parp_mod * parp_mod) * 10.0f;
  s->arp_time = (int)((1.0f - parp_spd) * (1.0f - parp_spd) * 20000.0f + 32.0f);
  if (parp_spd >= 1.0f) s->arp_time = 0;
  s->rpt_elapsed = 0;
  for (int i = 0; i < 32; i++)
    s->noise[i] = (int16_t)(((float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f) * 32767.0f);
  s->summands = 44100 / s->sample_rate;
  if (s->summands < 1) s->summands = 1;
  s->summed = 0;
  s->accum = 0.0f;
  s->phase = 0;
  s->t = 0;
  s->done = 0;
}

static int _sfxr_fill(int16_t *buf, int n, void *user) {
  sfxr_state_t *s = (sfxr_state_t *)user;

  if (!buf) {
    int id = (int)(s - s_slots);
    s_playing[id] = false;
    return 0;
  }

  if (s->done) return 0;
  int out = 0;
  while (out < n) {
    if (s->rpt_time != 0) {
      s->rpt_elapsed++;
      if (s->rpt_elapsed >= s->rpt_time) {
        s->rpt_elapsed = 0;
      }
    }
    if (s->arp_time != 0 && s->t >= s->arp_time) {
      s->arp_time = 0;
      s->period *= s->arp_mult;
    }
    s->pmul += s->pslide;
    s->period *= s->pmul;
    if (s->period > s->period_max) {
      s->period = s->period_max;
      if (s->freq_cut) { s->done = 1; break; }
    }
    float rfperiod = s->period;
    if (s->vib_amp > 0.0f) {
      s->vib_phase += s->vib_speed;
      int32_t viphase = (int32_t)(s->vib_phase * (32768.0f / (float)M_PI));
      rfperiod = s->period * (1.0f + (gfx_fast_isin(viphase) / 32767.0f) * s->vib_amp);
    }
    int iperiod = (int)rfperiod;
    if (iperiod < 8) iperiod = 8;
    s->duty += s->dslide;
    if (s->duty < 0.0f) s->duty = 0.0f;
    if (s->duty > 0.5f) s->duty = 0.5f;
    if (++s->env_elapsed > s->env_len[s->env_stage]) {
      s->env_elapsed = 0;
      if (++s->env_stage > 2) { s->done = 1; break; }
    }
    float envf = s->env_len[s->env_stage] > 0
      ? (float)s->env_elapsed / s->env_len[s->env_stage] : 0.0f;
    float ev;
    if (s->env_stage == 0) ev = envf;
    else if (s->env_stage == 1) ev = 1.0f + (1.0f - envf) * 2.0f * s->env_punch;
    else ev = 1.0f - envf;
    s->flg_off += s->flg_sl;
    int iphase = (int)fabsf(floorf(s->flg_off));
    if (iphase > 127) iphase = 127;
    s->flthp *= s->flthp_d;
    if (s->flthp < 0.00001f) s->flthp = 0.00001f;
    if (s->flthp > 0.1f) s->flthp = 0.1f;
    float sample = 0.0f;
    for (int si = 0; si < 8; si++) {
      float ss = 0.0f;
      s->phase++;
      if (s->phase >= iperiod) {
        s->phase %= iperiod;
        if (s->wave_shape == GFXA_SFXR_WAVE_NOISE)
          for (int j = 0; j < 32; j++)
            s->noise[j] = (int16_t)(((float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f) * 32767.0f);
      }
      float fp = (float)s->phase / iperiod;
      switch (s->wave_shape) {
        case 0: ss = (fp < s->duty) ? 0.5f : -0.5f; break;
        case 1:
          if (fp < s->duty) ss = -1.0f + 2.0f * fp / s->duty;
          else ss = 1.0f - 2.0f * (fp - s->duty) / (1.0f - s->duty);
          break;
        case 2: ss = gfx_fast_isin((int32_t)(s->phase * (65536.0f / iperiod))) / 32767.0f; break;
        case 3: ss = (float)s->noise[(s->phase * 32 / iperiod) % 32] / 32767.0f; break;
      }
      float pp = s->fltp;
      s->fltw *= s->fltw_d;
      if (s->fltw < 0.0f) s->fltw = 0.0f;
      if (s->fltw > 0.1f) s->fltw = 0.1f;
      if (s->en_lpf) {
        s->fltdp += (ss - s->fltp) * s->fltw;
        s->fltdp -= s->fltdp * s->fltdmp;
      } else {
        s->fltp = ss;
        s->fltdp = 0.0f;
      }
      s->fltp += s->fltdp;
      s->fltphp += s->fltp - pp;
      s->fltphp -= s->fltphp * s->flthp;
      ss = s->fltphp;
      s->flg_buf[s->flg_pos & 127] = (int16_t)(ss * 32767.0f);
      ss += (float)s->flg_buf[(s->flg_pos - iphase + 128) & 127] / 32767.0f;
      s->flg_pos = (s->flg_pos + 1) & 127;
      sample += ss * ev;
    }
    s->accum += sample;
    if (++s->summed < s->summands) { s->t++; continue; }
    s->summed = 0;
    sample = s->accum / s->summands;
    s->accum = 0.0f;
    sample = sample / 8.0f * s->gain;
    int32_t sample_out = (int32_t)floorf(sample * 32768.0f);
    if (sample_out >= 32768) sample_out = 32767;
    else if (sample_out < -32768) sample_out = -32768;
    buf[out++] = (int16_t)(sample_out & 0xFFFF);
    s->t++;
  }
  return out;
}

sfxr_state_t *gfxa_sfxr_play(int id, const afx_t *p, int ch) {
  if (id < 0) {
    for (id = 0; id < SFXR_SLOTS; id++)
      if (!s_playing[id]) break;
    if (id >= SFXR_SLOTS) return NULL;
  }
  sfxr_state_t *s = &s_slots[id];
  s_playing[id] = true;
  s->slot_id = id;
  rebuild(s, p);
  s->audio_ch = gfxa_play(_sfxr_fill, s, ch);
  if (s->audio_ch < 0) {
    s_playing[id] = false;
    return NULL;
  }
  return s;
}
