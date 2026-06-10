#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Total de parâmetros (matching jsfxr 1:1 + sound_vol) ──────────────── */

#define GFXA_SFXR_PARAM_COUNT 24

/* ── Índices dos parâmetros ────────────────────────────────────────────────
 * Todos são [0,1]. Parâmetros marcados como SIGNED são convertidos
 * internamente para [-1,1] via: signed = (param - 0.5) * 2
 *
 * A ordem segue fielmente o params_order do jsfxr.
 */

#define GFXA_SFXR_WAVE_TYPE      0   /* 0=square 1=saw 2=sine 3=noise */
#define GFXA_SFXR_ATTACK         1   /* p_env_attack */
#define GFXA_SFXR_SUSTAIN_TIME   2   /* p_env_sustain (duration) */
#define GFXA_SFXR_SUSTAIN_PUNCH  3   /* p_env_punch   (level) */
#define GFXA_SFXR_DECAY          4   /* p_env_decay */
#define GFXA_SFXR_BASE_FREQ      5   /* p_base_freq */
#define GFXA_SFXR_FREQ_LIMIT     6   /* p_freq_limit */
#define GFXA_SFXR_FREQ_RAMP      7   /* SIGNED: p_freq_ramp */
#define GFXA_SFXR_FREQ_DRAMP     8   /* SIGNED: p_freq_dramp */
#define GFXA_SFXR_VIB_STRENGTH   9   /* p_vib_strength */
#define GFXA_SFXR_VIB_SPEED     10   /* p_vib_speed */
#define GFXA_SFXR_ARP_MOD       11   /* SIGNED: p_arp_mod */
#define GFXA_SFXR_ARP_SPEED     12   /* p_arp_speed */
#define GFXA_SFXR_DUTY          13   /* p_duty */
#define GFXA_SFXR_DUTY_RAMP     14   /* SIGNED: p_duty_ramp */
#define GFXA_SFXR_REPEAT_SPEED  15   /* p_repeat_speed */
#define GFXA_SFXR_PHA_OFFSET    16   /* SIGNED: p_pha_offset */
#define GFXA_SFXR_PHA_RAMP      17   /* SIGNED: p_pha_ramp */
#define GFXA_SFXR_LPF_FREQ      18   /* p_lpf_freq */
#define GFXA_SFXR_LPF_RAMP      19   /* SIGNED: p_lpf_ramp */
#define GFXA_SFXR_LPF_RESONANCE 20   /* p_lpf_resonance */
#define GFXA_SFXR_HPF_FREQ      21   /* p_hpf_freq */
#define GFXA_SFXR_HPF_RAMP      22   /* SIGNED: p_hpf_ramp */
#define GFXA_SFXR_SOUND_VOL     23   /* sound_vol (ganho interno), default 0.5 */

/* ── Waveforms (apenas as 4 do jsfxr original) ─────────────────────────── */

#define GFXA_SFXR_WAVE_SQUARE   0
#define GFXA_SFXR_WAVE_SAWTOOTH 1
#define GFXA_SFXR_WAVE_SINE     2
#define GFXA_SFXR_WAVE_NOISE    3

/* ── API ────────────────────────────────────────────────────────────────── */

int  gfxa_sfxr_parse(const char *str, float params[GFXA_SFXR_PARAM_COUNT]);
int  gfxa_sfxr_parse_json(const char *json, float params[GFXA_SFXR_PARAM_COUNT]);
void gfxa_sfxr_defaults(float params[GFXA_SFXR_PARAM_COUNT]);

/**
 * Generate 16-bit signed PCM samples from SFXR parameters.
 *
 * Implementação fiel ao jsfxr (SoundEffect.getRawBuffer).
 *
 * @param params      24-element parameter array (see indices above)
 * @param samples     output buffer (must hold >= max_samples)
 * @param max_samples capacity of the output buffer
 * @param sample_rate sample rate in Hz (e.g. 16000)
 * @return number of samples written
 */
int gfxa_sfxr_generate(const float params[GFXA_SFXR_PARAM_COUNT],
                       int16_t *samples, int max_samples, int sample_rate);

void gfxa_sfxr_play(const float params[GFXA_SFXR_PARAM_COUNT]);
void gfxa_sfxr_play_str(const char *param_str);
void gfxa_sfxr_play_json(const char *json_str);
void gfxa_sfxr_set_callback(void (*cb)(bool));

#ifdef __cplusplus
}
#endif
