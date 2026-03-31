#include <stdio.h>
#include <string.h>
#include "microcuts.h"
#include "microcuts/src/microcuts.h"
#include "simplegfx.h"
#include "simpleterm.h"
#include "ansiutils.h"
#include "dialogs.h"

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


const char* map_unicode_char_utf8(char c) {
  switch (c) {
    case '\xda': return "┌";
    case '\xbf': return "┐";
    case '\xc0': return "└";
    case '\xd9': return "┘";
    case '\xc4': return "─";
    case '\xb3': return "│";
    case '\xc2': return "┬";
    case '\xc1': return "┴";
    case '\xc3': return "├";
    case '\xb4': return "┤";
    case '\xc5': return "┼";
    case '\xc9': return "╔";
    case '\xbb': return "╗";
    case '\xc8': return "╚";
    case '\xbc': return "╝";
    case '\xcd': return "═";
    case '\xba': return "║";
    case '\xcb': return "╦";
    case '\xca': return "╩";
    case '\xcc': return "╠";
    case '\xb9': return "╣";
    case '\xce': return "╬";
    case '\x18': return "↑";
    case '\x19': return "↓";
    case '\x1b': return "←";
    case '\x1a': return "→";
    case '\xfb': return "✓";
    case '\xf9': return "•";
    default: {
      static char buf[2];
      buf[0] = c;
      buf[1] = '\0';
      return buf;
    }
  }
}

int pb_state = 0;
void print_buffer() {
  printf("\n---\n\x1b[0m");
  int w, h;
  gfxt_get_size(&w, &h);
  int x = 0, y = 0;
  char* buf = get_buffer();
  pb_state = 0;
  while (*buf) {
    if (*buf == '\x1b') {
      pb_state = 1;
    } else if (pb_state == 1 && *buf == '[') {
      pb_state = 2;
    } else if (pb_state == 2) {
      while (*buf && *buf != 'm') {
        buf++;
      }
      pb_state = 0;
      if (!*buf) {
        printf("\x1b[0m\n---\ninvalid escape sequence\n");
        return;
      }
    } else if (*buf == '\n') {
      printf("\n");
      x = 0;
      y++;
      if (y >= h) {
        break;
      }
    } else {
      printf("%s", map_unicode_char_utf8(*buf));
      x++;
      if (x >= w) {
        printf("\n");
        x = 0;
        y++;
        if (y >= h) {
          break;
        }
      }
    }
    buf++;
  }
  printf("\x1b[0m\n---\n");
  printf("%s", get_buffer());
  printf("\x1b[0m\n---\n");
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
  gfxt_set_prompt_handler(get_prompt);
  gfxt_set_history_handler(add_history, get_history);
  gfxt_init(64, 6);

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

char content(int x, int y) {
  return 'A' + (x + y) % 26;
}

char * cut_v(char * buf, int w, int h, int pl, int y) {
  char * result = malloc((h - !pl) + 1);
  y = y >= 0 ? y : w / 2;
  // copy the middle column
  for (int i = 0; i < h - !pl; i++) {
    result[i] = buf[y + i * w];
  }
  result[h - !pl] = '\0';
  return result;
}

char * cut_h(char * buf, int w, int h, int pl, int x) {
  char * result = malloc(w + 1);
  x = x >= 0 ? x : h / 2;
  for (int i = 0; i < w; i++) {
    result[i] = buf[x * w + i];
  }
  result[w] = '\0';
  return result;
}

void dialogs(void) {
  gfxt_init(0, 0);
  gfxt_init(10, 10);
  int pl = 0;
  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    1,  // border type
    pl,  // prompt line
    1,  // margin horizontal
    1,  // margin vertical
    'm', // margin character
    1,  // padding horizontal
    1,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  int w, h;
  gfxt_get_size(&w, &h);
  assert_eq(w, 10);
  assert_eq(h, 10);
  assert_eq(strlen(get_buffer()), w * (h - !pl));
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "m" BOX_H "p" "IJKL" "p" BOX_H, esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "m" BOX_V "p" "IJKL" "p" BOX_V "m", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, 0), "mmmmmmmmmm", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, 1), "m" BOX_TL BOX_H BOX_H BOX_H BOX_H BOX_H BOX_H BOX_TR "m", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, (h - !pl) - 1), "m" BOX_BL BOX_H BOX_H BOX_H BOX_H BOX_H BOX_H BOX_BR "m", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    2,  // border type
    pl,  // prompt line
    1,  // margin horizontal
    1,  // margin vertical
    'm', // margin character
    1,  // padding horizontal
    1,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_eq(strlen(get_buffer()), w * (h - !pl));
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "m" BOX_H2 "p" "IJKL" "p" BOX_H2, esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "m" BOX_V2 "p" "IJKL" "p" BOX_V2 "m", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, 0), "mmmmmmmmmm", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, 1), "m" BOX_TL2 BOX_H2 BOX_H2 BOX_H2 BOX_H2 BOX_H2 BOX_H2 BOX_TR2 "m", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, (h - !pl) - 1), "m" BOX_BL2 BOX_H2 BOX_H2 BOX_H2 BOX_H2 BOX_H2 BOX_H2 BOX_BR2 "m", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    1,  // border type
    pl,  // prompt line
    2,  // margin horizontal
    2,  // margin vertical
    'm', // margin character
    1,  // padding horizontal
    1,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "mm" BOX_H "p" "JK" "p" BOX_H "m", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "mm" BOX_V "p" "JK" "p" BOX_V "mm", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    1,  // border type
    pl,  // prompt line
    3,  // margin horizontal
    3,  // margin vertical
    'm', // margin character
    1,  // padding horizontal
    1,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "mmm" BOX_H "p" "" "p" BOX_H "mm", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "mmm" BOX_V "p" "" "p" BOX_V "mmm", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    1,  // border type
    pl,  // prompt line
    1,  // margin horizontal
    1,  // margin vertical
    'm', // margin character
    2,  // padding horizontal
    2,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "m" BOX_H "pp" "JK" "pp" BOX_H "", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "m" BOX_V "pp" "JK" "pp" BOX_V "m", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    1,  // border type
    pl,  // prompt line
    1,  // margin horizontal
    1,  // margin vertical
    'm', // margin character
    3,  // padding horizontal
    3,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "m" BOX_H "ppp" "" "ppp" BOX_H "", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "m" BOX_V "ppp" "" "ppp" BOX_V "m", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    1,  // border type
    pl,  // prompt line
    1,  // margin horizontal
    1,  // margin vertical
    'm', // margin character
    0,  // padding horizontal
    0,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "m" BOX_H "" "HIJKLM" "" BOX_H "", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "m" BOX_V "" "HIJKLM" "" BOX_V "m", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    1,  // border type
    pl,  // prompt line
    0,  // margin horizontal
    0,  // margin vertical
    'm', // margin character
    0,  // padding horizontal
    0,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "" BOX_H "" "GHIJKLMN" "" /*BOX_H ""*/, esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "" BOX_V "" "GHIJKLMN" "" BOX_V "", esc);

  draw_screen( // border + margin + padding
    -1,  // foreground color
    -1,  // background color
    0,  // border type
    pl,  // prompt line
    1,  // margin horizontal
    1,  // margin vertical
    'm', // margin character
    0,  // padding horizontal
    0,  // padding vertical
    'p',  // padding character
    content  // content function
  );
  assert_escstr_eq(cut_v(get_buffer(), w, h, pl, -1), "m" BOX_H "" "HIJKLM" "" BOX_H "", esc);
  assert_escstr_eq(cut_h(get_buffer(), w, h, pl, -1), "m" BOX_V "" "HIJKLM" "" BOX_V "m", esc);

  print_buffer(); exit(0);
}

int main(void){
  start_tests();

  begin_section("ansi_parser");
  ansi_parser();
  end_section();

  begin_section("simpleterm");
  simpleterm();
  end_section();


  begin_section("dialogs");
  dialogs();
  end_section();

  end_tests();

  return 0;
}
