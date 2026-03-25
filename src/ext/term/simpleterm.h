#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gfxt_init(int w_chars, int h_chars, void (*eval_fn)(const char*), const char* (*prompt_fn)(void));
void gfxt_printf(const char *format, ...);
void gfxt_clear();
void gfxt_draw(int x, int y, int size);
void gfxt_on_key(uint8_t key);

#ifdef __cplusplus
}
#endif
