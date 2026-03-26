#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile char gfxt_stdin;

#define TERM_RESET      "\033[0m"
#define TERM_BOLD       "\033[1m"
#define TERM_BLACK      "\033[30m"
#define TERM_RED        "\033[31m"
#define TERM_GREEN      "\033[32m"
#define TERM_YELLOW     "\033[33m"
#define TERM_BLUE       "\033[34m"
#define TERM_MAGENTA    "\033[35m"
#define TERM_CYAN       "\033[36m"
#define TERM_WHITE      "\033[37m"
#define TERM_BBLACK     "\033[90m"
#define TERM_BRED       "\033[91m"
#define TERM_BGREEN     "\033[92m"
#define TERM_BYELLOW    "\033[93m"
#define TERM_BBLUE      "\033[94m"
#define TERM_BMAGENTA   "\033[95m"
#define TERM_BCYAN      "\033[96m"
#define TERM_BWHITE     "\033[97m"
#define TERM_BG_BLACK   "\033[40m"
#define TERM_BG_RED     "\033[41m"
#define TERM_BG_GREEN   "\033[42m"
#define TERM_BG_YELLOW  "\033[43m"
#define TERM_BG_BLUE    "\033[44m"
#define TERM_BG_MAGENTA "\033[45m"
#define TERM_BG_CYAN    "\033[46m"
#define TERM_BG_WHITE   "\033[47m"

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
