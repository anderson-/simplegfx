#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ansi_reset(void);
int ansi_feed(char c);
int* ansi_get_params(void);
int ansi_get_param_count(void);
void ansi_color_to_rgb(int code, uint8_t *r, uint8_t *g, uint8_t *b);

#ifdef __cplusplus
}
#endif
