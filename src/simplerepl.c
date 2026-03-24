#include <stdint.h>
#include <string.h>
#include "simplerepl.h"

#define MAX_LINE 128
#define HIST_COUNT 16

static char input_buf[MAX_LINE];
static int input_len = 0;
static int input_cursor = 0;
static char history[HIST_COUNT][MAX_LINE];
static int hist_count = 0;
static int hist_idx = 0;
static int hist_browse = -1;

static void (*repl_eval)(const char*) = NULL;
static const char* (*repl_prompt)(void) = NULL;

static int escape_state = 0;

void repl_init(void (*eval_fn)(const char*), const char* (*prompt_fn)(void)) {
  repl_eval = eval_fn;
  repl_prompt = prompt_fn;
  repl_clear();
}

void repl_clear(void) {
  input_len = 0;
  input_cursor = 0;
  input_buf[0] = '\0';
  hist_browse = -1;
  escape_state = 0;
}

void repl_add_history(const char *line) {
  if (strlen(line) >= MAX_LINE) return;

  strncpy(history[hist_idx], line, MAX_LINE - 1);
  history[hist_idx][MAX_LINE - 1] = '\0';
  hist_idx = (hist_idx + 1) % HIST_COUNT;
  if (hist_count < HIST_COUNT) hist_count++;
}

void repl_handle_key(uint8_t key, uint8_t modifiers) {
  if (escape_state == 1) {
    if (key == '[') {
      escape_state = 2;
      return;
    } else {
      escape_state = 0;
    }
  } else if (escape_state == 2) {
    escape_state = 0;
    switch (key) {
      case 'A': // Up
        if (hist_count > 0) {
          if (hist_browse == -1) hist_browse = hist_idx;
          hist_browse = (hist_browse - 1 + HIST_COUNT) % HIST_COUNT;
          if (hist_browse < hist_idx - hist_count + HIST_COUNT) hist_browse = (hist_idx - 1) % HIST_COUNT;
          strcpy(input_buf, history[hist_browse]);
          input_len = strlen(input_buf);
          input_cursor = input_len;
        }
        return;
      case 'B': // Down
        if (hist_browse != -1) {
          hist_browse = (hist_browse + 1) % HIST_COUNT;
          if (hist_browse == hist_idx || hist_browse >= hist_idx) {
            repl_clear();
          } else {
            strcpy(input_buf, history[hist_browse]);
            input_len = strlen(input_buf);
            input_cursor = input_len;
          }
        }
        return;
      case 'C': // Right
        if (input_cursor < input_len) input_cursor++;
        return;
      case 'D': // Left
        if (input_cursor > 0) input_cursor--;
        return;
    }
  }

  if (key == '\x1b') {
    escape_state = 1;
    return;
  }

  if (key == '\r' || key == '\n') {
    if (input_len > 0) {
      repl_add_history(input_buf);
      if (repl_eval) repl_eval(input_buf);
    }
    repl_clear();
    return;
  }

  if (key == 8 || key == 127) { // Backspace
    if (input_cursor > 0) {
      memmove(input_buf + input_cursor - 1, input_buf + input_cursor, input_len - input_cursor + 1);
      input_len--;
      input_cursor--;
    }
    return;
  }

  if (key >= 32 && key < 127) {
    if (input_len < MAX_LINE - 1) {
      memmove(input_buf + input_cursor + 1, input_buf + input_cursor, input_len - input_cursor + 1);
      input_buf[input_cursor] = key;
      input_len++;
      input_cursor++;
      input_buf[input_len] = '\0';
    }
  }
}

const char* repl_get_input(void) {
  return input_buf;
}

int repl_get_cursor(void) {
  return input_cursor;
}

int repl_get_length(void) {
  return input_len;
}

const char* repl_get_prompt(void) {
  if (repl_prompt) return repl_prompt();
  return "> ";
}
