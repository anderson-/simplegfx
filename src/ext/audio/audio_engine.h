#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "simplegfx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFXA_ENGINE_CHANNELS  4
#define GFXA_ENGINE_FADE_MS  16
#define GFXA_SAMPLE_RATE     16000

/* ── Engine API (portátil, mesmo código para todos os backends) ────────── */

/* Inicializa o engine. Cria a task de áudio no ESP32.
 * Chamado automaticamente na primeira gfxa_engine_play() se necessário. */
void gfxa_engine_init(int sample_rate);

/* Toca em qualquer canal livre. Retorna o índice do canal ou -1.
 * O engine chama dtor(userdata) quando o canal terminar. */
int  gfxa_engine_play(audio_fill_fn fn, void *userdata, void (*dtor)(void*));

/* Toca em um canal específico (para .mod futuro). */
int  gfxa_engine_play_on(int ch, audio_fill_fn fn, void *userdata, void (*dtor)(void*));

/* Para um canal com fade-out. */
void gfxa_engine_stop(int ch);

/* Para todos os canais com fade-out. */
void gfxa_engine_stop_all(void);

/* Controles do canal */
void gfxa_engine_set_volume(int ch, float vol);  /* 0.0 – 1.0 */
void gfxa_engine_set_pan(int ch, float pan);      /* -1.0 esq, 0 centro, 1.0 dir */

/* Estado */
bool gfxa_engine_is_playing(void);         /* true se algum canal ativo */
bool gfxa_engine_is_channel_active(int ch);

/* Espera bloqueante até todos os canais ficarem inativos.
 * Usa gfx_delay(1) internamente. */
void gfxa_engine_wait(void);

/* Mixer: chamado pelo backend para preencher um buffer de saída.
 * Retorna o número de samples escritos (sempre n, exceto no shutdown). */
int  gfxa_engine_mix(int16_t *buf, int n);

/* Desliga o engine e a task de áudio. */
void gfxa_engine_deinit(void);

#ifdef __cplusplus
}
#endif
