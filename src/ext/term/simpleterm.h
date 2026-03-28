#pragma once
#include <stdint.h>
#include "ansiutils.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile char gfxt_stdin;

#define MAX_COMMANDS 64

typedef struct {
  const char* name;
  int (*func)(const char*);
  const char* help;
} cmd_entry_t;

extern cmd_entry_t gfxt_cmd_registry[MAX_COMMANDS];
extern int gfxt_cmd_registry_len;

int gfxt_run_cmd(const char* line);
void gfxt_register_cmd(const char* name, const char* help, int (*func)(const char*));
void gfxt_init(int w_chars, int h_chars, const char* (*prompt_fn)(void), int (*eval_fn)(const char*), void (*scroll_push_fn)(const char*, int), const char* (*scroll_prev_fn)(int), const char* (*scroll_next_fn)(int), void (*history_push_fn)(const char*), const char* (*history_prev_fn)(int));
void gfxt_putchar(char c);
int gfxt_print(const char *str);
int gfxt_println(const char *str);
int gfxt_printf(const char *format, ...);
void gfxt_clear();
char gfxt_getchar();
char* gfxt_gets(char *str, int count);
int gfxt_scanf(const char *format, ...);
void gfxt_set_theme(int theme);
int gfxt_draw(int x, int y, int size);
void gfxt_on_key(uint8_t key);
void gfxt_set_busy(int busy);

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
