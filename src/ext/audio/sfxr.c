#include "sfxr.h"
#include "simpleaudio.h"
#include "simplegfx.h"
#include "simplemath.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static float norm_signed(float v) {
  return (v - 0.5f) * 2.0f;
}

void gfxa_sfxr_defaults(float params[GFXA_SFXR_PARAM_COUNT]) {
  params[GFXA_SFXR_WAVE_TYPE]      = 0.0f;
  params[GFXA_SFXR_ATTACK]         = 0.0f;
  params[GFXA_SFXR_SUSTAIN_TIME]   = 0.3f;
  params[GFXA_SFXR_SUSTAIN_PUNCH]  = 0.0f;
  params[GFXA_SFXR_DECAY]          = 0.4f;
  params[GFXA_SFXR_BASE_FREQ]      = 0.3f;
  params[GFXA_SFXR_FREQ_LIMIT]     = 0.0f;
  params[GFXA_SFXR_FREQ_RAMP]      = 0.5f;
  params[GFXA_SFXR_FREQ_DRAMP]     = 0.5f;
  params[GFXA_SFXR_VIB_STRENGTH]   = 0.0f;
  params[GFXA_SFXR_VIB_SPEED]      = 0.0f;
  params[GFXA_SFXR_ARP_MOD]        = 0.5f;
  params[GFXA_SFXR_ARP_SPEED]      = 0.0f;
  params[GFXA_SFXR_DUTY]           = 0.0f;
  params[GFXA_SFXR_DUTY_RAMP]      = 0.5f;
  params[GFXA_SFXR_REPEAT_SPEED]   = 0.0f;
  params[GFXA_SFXR_PHA_OFFSET]     = 0.5f;
  params[GFXA_SFXR_PHA_RAMP]       = 0.5f;
  params[GFXA_SFXR_LPF_FREQ]       = 1.0f;
  params[GFXA_SFXR_LPF_RAMP]       = 0.5f;
  params[GFXA_SFXR_LPF_RESONANCE]  = 0.0f;
  params[GFXA_SFXR_HPF_FREQ]       = 0.0f;
  params[GFXA_SFXR_HPF_RAMP]       = 0.5f;
  params[GFXA_SFXR_SOUND_VOL]      = 0.5f;
}

/* ── sfxr_state ─────────────────────────────────────────────────────────── */

struct sfxr_state {
  float period;
  float period_max;
  float pmul;
  float pslide;
  float duty;
  float dslide;
  float arp_mult;
  int arp_time;
  int rpt_elapsed;
  int rpt_time;
  int freq_cut;
  int phase;
  int wave_shape;
  float fltw;
  float fltw_d;
  float fltdmp;
  float fltp;
  float fltdp;
  float flthp;
  float flthp_d;
  float fltphp;
  int en_lpf;
  float vib_speed;
  float vib_amp;
  float vib_phase;
  int env_len[3];
  int env_stage;
  int env_elapsed;
  float env_punch;
  float flg_buf[128];
  float flg_off;
  float flg_sl;
  int flg_pos;
  float gain;
  float noise[32];
  int summands;
  int summed;
  float accum;
  int sample_rate;
  int t;
  int done;
};

/* ── Pool ───────────────────────────────────────────────────────────────── */

#if defined(ESP_PLATFORM) || defined(ESP32)
#ifndef GFXA_SFXR_POOL_SIZE
#define GFXA_SFXR_POOL_SIZE 16
#endif

static struct sfxr_state s_pool[GFXA_SFXR_POOL_SIZE];
static volatile uint8_t s_pool_used[GFXA_SFXR_POOL_SIZE];

static struct sfxr_state *sfxr_alloc_state(void) {
  for (int i = 0; i < GFXA_SFXR_POOL_SIZE; i++) {
    if (!s_pool_used[i] &&
        __sync_bool_compare_and_swap(&s_pool_used[i], 0, 1)) {
      memset(&s_pool[i], 0, sizeof(s_pool[i]));
      return &s_pool[i];
    }
  }
  return NULL;
}

static void sfxr_free_state(struct sfxr_state *s) {
  if (!s) return;
  for (int i = 0; i < GFXA_SFXR_POOL_SIZE; i++) {
    if (s == &s_pool[i]) {
      memset(s, 0, sizeof(*s));
      __sync_synchronize();
      s_pool_used[i] = 0;
      return;
    }
  }
}
#else
static struct sfxr_state *sfxr_alloc_state(void) {
  return (struct sfxr_state *)calloc(1, sizeof(struct sfxr_state));
}

static void sfxr_free_state(struct sfxr_state *s) {
  free(s);
}
#endif

/* ── Create ─────────────────────────────────────────────────────────────── */

struct sfxr_state *gfxa_sfxr_create(const float params[GFXA_SFXR_PARAM_COUNT]) {
  struct sfxr_state *s = sfxr_alloc_state();
  if (!s) return NULL;

  int wt = (int)clampf(params[GFXA_SFXR_WAVE_TYPE], 0, 4);
  float pa  = clampf(params[GFXA_SFXR_ATTACK], 0, 1);
  float pst = clampf(params[GFXA_SFXR_SUSTAIN_TIME], 0, 1);
  float ppu = clampf(params[GFXA_SFXR_SUSTAIN_PUNCH], 0, 1);
  float pd  = clampf(params[GFXA_SFXR_DECAY], 0, 1);
  float pbf = clampf(params[GFXA_SFXR_BASE_FREQ], 0, 1);
  float pfl = clampf(params[GFXA_SFXR_FREQ_LIMIT], 0, 1);
  float pfr = norm_signed(clampf(params[GFXA_SFXR_FREQ_RAMP], 0, 1));
  float pfd = norm_signed(clampf(params[GFXA_SFXR_FREQ_DRAMP], 0, 1));
  float pvstrength = clampf(params[GFXA_SFXR_VIB_STRENGTH], 0, 1);
  float pvspeed    = clampf(params[GFXA_SFXR_VIB_SPEED], 0, 1);
  float parp_mod = norm_signed(clampf(params[GFXA_SFXR_ARP_MOD], 0, 1));
  float parp_spd = clampf(params[GFXA_SFXR_ARP_SPEED], 0, 1);
  float pdu = clampf(params[GFXA_SFXR_DUTY], 0, 1);
  float pds = norm_signed(clampf(params[GFXA_SFXR_DUTY_RAMP], 0, 1));
  float prp = clampf(params[GFXA_SFXR_REPEAT_SPEED], 0, 1);
  float pho = norm_signed(clampf(params[GFXA_SFXR_PHA_OFFSET], 0, 1));
  float phs = norm_signed(clampf(params[GFXA_SFXR_PHA_RAMP], 0, 1));
  float plc = clampf(params[GFXA_SFXR_LPF_FREQ], 0, 1);
  float plrm = norm_signed(clampf(params[GFXA_SFXR_LPF_RAMP], 0, 1));
  float plr = clampf(params[GFXA_SFXR_LPF_RESONANCE], 0, 1);
  float phc = clampf(params[GFXA_SFXR_HPF_FREQ], 0, 1);
  float phrm = norm_signed(clampf(params[GFXA_SFXR_HPF_RAMP], 0, 1));
  float psndvol = clampf(params[GFXA_SFXR_SOUND_VOL], 0, 1);

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
    s->noise[i] = (float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f;
  s->summands = 44100 / s->sample_rate;
  if (s->summands < 1) s->summands = 1;
  s->summed = 0;
  s->accum = 0.0f;
  s->phase = 0;
  s->t = 0;
  return s;
}

/* ── Read samples ───────────────────────────────────────────────────────── */

int gfxa_sfxr_read(struct sfxr_state *s, int16_t *buf, int n) {
  if (s->done) return 0;
  int out = 0;
  while (out < n) {
    /* Repeat */
    if (s->rpt_time != 0) {
      s->rpt_elapsed++;
      if (s->rpt_elapsed >= s->rpt_time) {
        s->rpt_elapsed = 0;
        /* re-initForRepeat would need stored params — skipped for now */
      }
    }
    /* Arpeggio */
    if (s->arp_time != 0 && s->t >= s->arp_time) {
      s->arp_time = 0;
      s->period *= s->arp_mult;
    }
    /* Frequency slide */
    s->pmul += s->pslide;
    s->period *= s->pmul;
    if (s->period > s->period_max) {
      s->period = s->period_max;
      if (s->freq_cut) { s->done = 1; break; }
    }
    /* Vibrato */
    float rfperiod = s->period;
    if (s->vib_amp > 0.0f) {
      s->vib_phase += s->vib_speed;
      rfperiod = s->period * (1.0f + sinf(s->vib_phase) * s->vib_amp);
    }
    int iperiod = (int)rfperiod;
    if (iperiod < 8) iperiod = 8;
    /* Duty */
    s->duty += s->dslide;
    if (s->duty < 0.0f) s->duty = 0.0f;
    if (s->duty > 0.5f) s->duty = 0.5f;
    /* Envelope */
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
    /* Flanger */
    s->flg_off += s->flg_sl;
    int iphase = (int)fabsf(floorf(s->flg_off));
    if (iphase > 127) iphase = 127;
    /* HPF slide */
    s->flthp *= s->flthp_d;
    if (s->flthp < 0.00001f) s->flthp = 0.00001f;
    if (s->flthp > 0.1f) s->flthp = 0.1f;
    /* 8x oversampling */
    float sample = 0.0f;
    for (int si = 0; si < 8; si++) {
      float ss = 0.0f;
      s->phase++;
      if (s->phase >= iperiod) {
        s->phase %= iperiod;
        if (s->wave_shape == GFXA_SFXR_WAVE_NOISE)
          for (int j = 0; j < 32; j++)
            s->noise[j] = (float)(gfx_fast_rand() % 32768) / 16383.5f - 1.0f;
      }
      float fp = (float)s->phase / iperiod;
      switch (s->wave_shape) {
        case 0: ss = (fp < s->duty) ? 0.5f : -0.5f; break;
        case 1:
          if (fp < s->duty) ss = -1.0f + 2.0f * fp / s->duty;
          else ss = 1.0f - 2.0f * (fp - s->duty) / (1.0f - s->duty);
          break;
        case 2: ss = sinf(fp * 2.0f * (float)M_PI); break;
        case 3: ss = s->noise[(s->phase * 32 / iperiod) % 32]; break;
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
      s->flg_buf[s->flg_pos & 127] = ss;
      ss += s->flg_buf[(s->flg_pos - iphase + 128) & 127];
      s->flg_pos = (s->flg_pos + 1) & 127;
      sample += ss * ev;
    }
    /* Downsample */
    s->accum += sample;
    if (++s->summed < s->summands) { s->t++; continue; }
    s->summed = 0;
    sample = s->accum / s->summands;
    s->accum = 0.0f;
    /* Gain and output */
    sample = sample / 8.0f * s->gain;
    int32_t sample_out = (int32_t)floorf(sample * 32768.0f);
    if (sample_out >= 32768) sample_out = 32767;
    else if (sample_out < -32768) sample_out = -32768;
    buf[out++] = (int16_t)(sample_out & 0xFFFF);
    s->t++;
  }
  return out;
}

/* ── Getters/setters ────────────────────────────────────────────────────── */

void gfxa_sfxr_destroy(struct sfxr_state *s) {
  sfxr_free_state(s);
}

void gfxa_sfxr_set_vibrato(struct sfxr_state *s, float speed, float depth) {
  if (!s) return;
  s->vib_speed = speed * speed * 0.01f;
  s->vib_amp   = depth * 0.5f;
  s->vib_phase = 0.0f;
}

float gfxa_sfxr_get_period(const struct sfxr_state *s) {
  return s ? s->period : 0.0f;
}

void gfxa_sfxr_set_period(struct sfxr_state *s, float period) {
  if (!s) return;
  if (period < 8.0f) period = 8.0f;
  s->period = period;
}

bool gfxa_sfxr_is_done(const struct sfxr_state *s) {
  return s ? (bool)s->done : true;
}

/* ── Async playback via gfxa_play + fn(NULL,0,data) cleanup ─────────────── */

struct sfxr_play_ctx {
  struct sfxr_state *state;
  void (*cb)(bool);
};

static void (*_sfxr_cb)(bool) = NULL;

void gfxa_sfxr_set_callback(void (*cb)(bool)) {
  _sfxr_cb = cb;
}

/* Stream fill function: quando buf == NULL, é cleanup.
 * O mixer (simpleaudio.c) chama fn(NULL, 0, data) quando o som termina. */
static int _sfxr_fill(int16_t *buf, int n, void *user) {
  struct sfxr_play_ctx *ctx = (struct sfxr_play_ctx *)user;

  if (!buf) {
    /* Cleanup: o som terminou naturalmente ou foi parado */
    gfxa_sfxr_destroy(ctx->state);
    if (ctx->cb) ctx->cb(false);
    free(ctx);
    return 0;
  }

  return gfxa_sfxr_read(ctx->state, buf, n);
}

int gfxa_sfxr_play(const float params[GFXA_SFXR_PARAM_COUNT]) {
  struct sfxr_state *s = gfxa_sfxr_create(params);
  if (!s) return -1;

  struct sfxr_play_ctx *ctx = (struct sfxr_play_ctx *)malloc(sizeof(*ctx));
  if (!ctx) { gfxa_sfxr_destroy(s); return -1; }
  ctx->state = s;
  ctx->cb = _sfxr_cb;

  if (_sfxr_cb) _sfxr_cb(true);

  int ch = gfxa_play(_sfxr_fill, ctx, -1);
  if (ch < 0) {
    /* Falha ao alocar canal — faz cleanup manual */
    if (_sfxr_cb) _sfxr_cb(false);
    gfxa_sfxr_destroy(s);
    free(ctx);
    return -1;
  }

  return ch;
}

/* ── Pack/unpack: 24 floats [0,1] ↔ 24 bytes ───────────────────────────── */

int gfxa_sfxr_pack(const float params[GFXA_SFXR_PARAM_COUNT],
                   uint8_t packed[GFXA_SFXR_PARAM_COUNT]) {
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i++) {
    float v = params[i];
    if (i == GFXA_SFXR_WAVE_TYPE) {
      /* WAVE_TYPE é 0-3, normaliza para [0,1] */
      v = v / 3.0f;
      if (v < 0.0f) v = 0.0f;
      if (v > 1.0f) v = 1.0f;
    } else {
      if (v < 0.0f) v = 0.0f;
      if (v > 1.0f) v = 1.0f;
    }
    packed[i] = (uint8_t)(v * 255.0f + 0.5f);
  }
  return GFXA_SFXR_PARAM_COUNT;
}

void gfxa_sfxr_unpack(const uint8_t packed[GFXA_SFXR_PARAM_COUNT],
                      float params[GFXA_SFXR_PARAM_COUNT]) {
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i++) {
    float v = packed[i] / 255.0f;
    if (i == GFXA_SFXR_WAVE_TYPE)
      v = v * 3.0f;  /* desnormaliza para 0-3 */
    params[i] = v;
  }
}

/* ── Base64: 24 floats [0,1] quantizados em 6 bits cada → 24 chars ─────── */

/* Cada 4 params de 6 bits ocupam 3 bytes. 24 params / 4 * 3 = 18 bytes.
 * 18 bytes → 18/3*4 = 24 chars base64, sem padding.            */

static const char b64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_idx(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

/* 24 floats → 24 chars base64 */
int gfxa_sfxr_to_base64(const float params[GFXA_SFXR_PARAM_COUNT],
                        char *out, int out_size) {
  /* Buffer intermediário: 24 valores de 6 bits em 18 bytes */
  uint8_t bytes[18];
  int b = 0;
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i += 4) {
    /* WAVE_TYPE (i==0) é 0-3, normaliza linearmente; demais usam sqrt companding */
    int a = (i == 0)
      ? (int)(params[0] / 3.0f * 63.0f + 0.5f)
      : (int)(sqrtf(params[i]) * 63.0f + 0.5f);
    if (a < 0) a = 0; if (a > 63) a = 63;
    int c = (int)(sqrtf(params[i+2]) * 63.0f + 0.5f); if (c < 0) c = 0; if (c > 63) c = 63;
    int d = (int)(sqrtf(params[i+3]) * 63.0f + 0.5f); if (d < 0) d = 0; if (d > 63) d = 63;
    int bp = (int)(sqrtf(params[i+1]) * 63.0f + 0.5f); if (bp < 0) bp = 0; if (bp > 63) bp = 63;
    bytes[b++] = (uint8_t)((a << 2) | (bp >> 4));
    bytes[b++] = (uint8_t)((bp << 4) | (c >> 2));
    bytes[b++] = (uint8_t)((c << 6) | d);
  }

  if (out_size < 24 + 1) return 0;
  for (int i = 0; i < 18; i += 3) {
    unsigned v = ((unsigned)bytes[i] << 16) |
                 ((unsigned)bytes[i+1] << 8) |
                  (unsigned)bytes[i+2];
    out[0] = b64[(v >> 18) & 0x3F]; out++;
    out[0] = b64[(v >> 12) & 0x3F]; out++;
    out[0] = b64[(v >>  6) & 0x3F]; out++;
    out[0] = b64[ v        & 0x3F]; out++;
  }
  *out = '\0';
  return 24;
}

/* 24 chars base64 → 24 floats */
int gfxa_sfxr_from_base64(const char *str,
                          float params[GFXA_SFXR_PARAM_COUNT]) {
  uint8_t bytes[18];
  int bi = 0;

  /* Decode base64 para 18 bytes */
  while (*str && bi < 18) {
    int a, bv, c, d;
    while (*str && *str <= ' ') str++;
    if (!*str || *str == '=') break;
    if ((a = b64_idx(*str++)) < 0) return 1;
    while (*str && *str <= ' ') str++;
    if (!*str || *str == '=') { bytes[bi++] = (uint8_t)(a << 2); break; }
    if ((bv = b64_idx(*str++)) < 0) return 1;
    while (*str && *str <= ' ') str++;
    if (!*str || *str == '=') { bytes[bi++] = (uint8_t)((a << 2) | (bv >> 4)); break; }
    if ((c = b64_idx(*str++)) < 0) return 1;
    while (*str && *str <= ' ') str++;
    if (!*str || *str == '=') {
      bytes[bi++] = (uint8_t)((a << 2) | (bv >> 4));
      bytes[bi++] = (uint8_t)((bv << 4) | (c >> 2));
      break;
    }
    if ((d = b64_idx(*str++)) < 0) return 1;
    bytes[bi++] = (uint8_t)((a << 2) | (bv >> 4));
    bytes[bi++] = (uint8_t)((bv << 4) | (c >> 2));
    bytes[bi++] = (uint8_t)((c << 6) | d);
  }

  if (bi != 18) return 1;

  /* Unpack 18 bytes para 24 valores de 6 bits e dequantiza */
  int pi = 0;
  for (int i = 0; i < 18; i += 3) {
    float fa = (bytes[i] >> 2) / 63.0f;
    float fb = ((bytes[i] & 0x03) << 4 | (bytes[i+1] >> 4)) / 63.0f;
    float fc = ((bytes[i+1] & 0x0F) << 2 | (bytes[i+2] >> 6)) / 63.0f;
    float fd = (bytes[i+2] & 0x3F) / 63.0f;
    /* WAVE_TYPE (pi==0) desnormaliza linear; demais sqrt expand */
    if (pi == 0) params[pi++] = fa * 3.0f;
    else         params[pi++] = fa * fa;
    params[pi++] = fb * fb;
    params[pi++] = fc * fc;
    params[pi++] = fd * fd;
  }
  return 0;
}
