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
void gfxt_set_history_handler(void (*_history_push_fn)(const char*), char* (*_history_prev_fn)(int));
void gfxt_set_scroll_handler(void (*_scroll_push_fn)(const char*, int), char* (*_scroll_prev_fn)(int));
void gfxt_set_eval_handler(int (*_eval_fn)(const char*));
void gfxt_set_prompt_handler(char* (*_prompt_fn)(void));
void gfxt_init(int width, int height);
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
void gfxt_get_size(int *w, int *h);

#ifdef EXPORT_STDIO
#define putchar gfxt_putchar
#define print gfxt_print
#define println gfxt_println
#define getchar gfxt_getchar
#define gets gfxt_gets
#define gets_s gfxt_gets
#define scanf gfxt_scanf
#endif

#ifdef TEST
char * get_buffer(void);
#endif

#ifdef __cplusplus
}
#endif
