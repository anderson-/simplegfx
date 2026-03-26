#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile char gfxt_stdin;

void gfxt_init(int w_chars, int h_chars, void (*eval_fn)(const char*), const char* (*prompt_fn)(void));
void gfxt_putchar(char c);
int gfxt_print(const char *str);
int gfxt_println(const char *str);
int gfxt_printf(const char *format, ...);
void gfxt_clear();
char gfxt_getchar();
char* gfxt_gets(char *str, int count);
int gfxt_scanf(const char *format, ...);
void gfxt_draw(int x, int y, int size);
void gfxt_on_key(uint8_t key);

#ifdef EXPORT_STDIO
#define putchar gfxt_putchar
#define print gfxt_print
#define println gfxt_println
#define getchar gfxt_getchar
#define gets gfxt_gets
#define gets_s gfxt_gets
#define scanf gfxt_scanf
#endif

#ifdef __cplusplus
}
#endif
