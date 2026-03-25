#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void repl_init(void (*eval_fn)(const char*), const char* (*prompt_fn)(void));
void repl_clear(void);
void repl_add_history(const char *line);
void repl_handle_key(uint8_t key);
const char* repl_get_input(void);
int repl_get_cursor(void);
int repl_get_length(void);
const char* repl_get_prompt(void);

#ifdef __cplusplus
}
#endif
