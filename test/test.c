#include <stdio.h>
#include <string.h>
#include "simplegfx.h"
#include "simpleterm.h"
#include "ansiutils.h"
#include "microcuts.h"

int simple_prompt = 0;

char* get_prompt(void) {
  return simple_prompt ? "> " : "\x1b[32mtest\x1b[m> ";
}

char* esc(char* s) {
  static char buf[1024];
  memset(buf, 0, sizeof(buf));
  char* p = buf;
  while (*s) {
    if (*s == '\n') {
      *p++ = '\\';
      *p++ = 'n';
    } else if (*s >= 32 && *s <= 126) {
      *p++ = *s;
    } else {
      sprintf(p, "\\x%02x", (unsigned char)*s);
      p += 4;
    }
    s++;
  }
  *p = '\0';
  strcpy(s, buf);
  return s;
}

void print_buffer() {
  printf("\n---\n\x1b[0m%s\x1b[0m\n---\n", get_buffer());
}

char history[5][64];
int history_count = 0;

void add_history(const char* cmd) {
  if (history_count < 5) {
    strcpy(history[history_count], cmd);
    history_count++;
  } else {
    for (int i = 0; i < 4; i++) {
      strcpy(history[i], history[i + 1]);
    }
    strcpy(history[4], cmd);
  }
}

char* get_history(int index) {
  return history[history_count - 1 - index];
}

void ansi_parser(void) {
  int state = 0, param_count = 0, params[8] = {0};

  // basic color
  assert_eq(ansi_feed('\x1b', &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('[',    &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('3',    &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('1',    &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('m',    &state, &param_count, params), ANSI_COLOR);
  assert_eq(params[0], 31);
  ansi_reset(&state, &param_count, params);

  // reset sequence \x1b[m
  assert_eq(ansi_feed('\x1b', &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('[',    &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('m',    &state, &param_count, params), ANSI_COLOR);
  assert_eq(params[0], 0);
  ansi_reset(&state, &param_count, params);

  assert_eq(ansi_feed('\x1b', &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('x',    &state, &param_count, params), NON_ANSI_CHAR);
  assert_eq(state, 0);  // must be reset

  assert_eq(ansi_feed('\x1b', &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('[',    &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed(' ',    &state, &param_count, params), ANSI_NONE);
  assert_eq(state, 0);  // state must be 0 after unknown char
  assert_eq(ansi_feed('a',    &state, &param_count, params), NON_ANSI_CHAR);

  assert_eq(ansi_feed('\x1b', &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('[',    &state, &param_count, params), ANSI_NONE);
  assert_eq(ansi_feed('\x1a', &state, &param_count, params), ANSI_NONE);
  assert_eq(state, 0);
}

void simpleterm(void) {
  gfx_set_font(&font5x7);
  gfxt_init(64, 6);
  gfxt_set_prompt_handler(get_prompt);
  gfxt_set_history_handler(add_history, get_history);

  // should start with prompt
  assert_escstr_eq(get_buffer(), "\x1b[0m\x1b[32mtest\x1b[0m> ", esc);
  gfxt_on_key('a');
  assert_escstr_eq(get_buffer(), "\x1b[0m\x1b[32mtest\x1b[0m> a", esc);
  gfxt_on_key('\n');
  assert_escstr_eq(get_buffer(), "\x1b[0m\x1b[32mtest\x1b[0m> a\n\x1b[31ma: not found\n\x1b[0m\x1b[0m\x1b[32mtest\x1b[0m> ", esc);
  simple_prompt = 1;
  gfxt_printf("\x1b[2J");
  gfxt_on_key('\n');
  assert_escstr_eq(get_buffer(), "\x1b[0m> ", esc);
  gfxt_on_key('a');
  gfxt_on_key('\n');
  assert_escstr_eq(get_buffer(), "\x1b[0m> a\n\x1b[31ma: not found\n\x1b[0m\x1b[0m> ", esc);
  gfxt_on_key('b');
  assert_escstr_eq(get_buffer(), "\x1b[0m> a\n\x1b[31ma: not found\n\x1b[0m\x1b[0m> b", esc);
  gfxt_on_key('\n');
  assert_escstr_eq(get_buffer(), "\x1b[0m> a\n\x1b[31ma: not found\n\x1b[0m\x1b[0m> b\n\x1b[31mb: not found\n\x1b[0m\x1b[0m> ", esc);
  gfxt_on_key('\x1b');
  gfxt_on_key('[');
  gfxt_on_key('A');
  assert_escstr_eq(get_buffer(), "\x1b[0m> a\n\x1b[31ma: not found\n\x1b[0m\x1b[0m> b\n\x1b[31mb: not found\n\x1b[0m\x1b[0m> b", esc);
  gfxt_on_key('\x1b');
  gfxt_on_key('[');
  gfxt_on_key('A');
  assert_escstr_eq(get_buffer(), "\x1b[0m> a\n\x1b[31ma: not found\n\x1b[0m\x1b[0m> b\n\x1b[31mb: not found\n\x1b[0m\x1b[0m> a", esc);
  gfxt_on_key('\x1b');
  gfxt_on_key('[');
  gfxt_on_key('B');
  assert_escstr_eq(get_buffer(), "\x1b[0m> a\n\x1b[31ma: not found\n\x1b[0m\x1b[0m> b\n\x1b[31mb: not found\n\x1b[0m\x1b[0m> b", esc);
  //print_buffer();
}

int main(void){
  start_tests();

  begin_section("ansi_parser");
  ansi_parser();
  end_section();

  begin_section("simpleterm");
  simpleterm();
  end_section();

  end_tests();

  return 0;
}
