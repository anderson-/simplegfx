#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void txt_set_size(int w, int h);
void txt_get_size(int *w, int *h);
void txt_set_colors(uint8_t fg, uint8_t bg);
void txt_set_cursor(int x, int y);
void txt_get_cursor(int *x, int *y);
void txt_clear(void);
void txt_putc(char c);
void txt_write(const char *s);
void txt_scroll(int lines);
char* txt_get_text_buffer(void);
uint8_t* txt_get_fg_buffer(void);
uint8_t* txt_get_bg_buffer(void);
void txt_set_color(uint8_t fg, uint8_t bg);
void txt_get_color(uint8_t *fg, uint8_t *bg);

#ifdef __cplusplus
}
#endif
