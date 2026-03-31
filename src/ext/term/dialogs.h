#pragma once

#include "ansiutils.h"
#include "simpleterm.h"
#include "stdcmds.h"
#include <string.h>

// Box drawing characters (single line)
#define BOX_TL  "\xda"  // Top left corner
#define BOX_TR  "\xbf"  // Top right corner
#define BOX_BL  "\xc0"  // Bottom left corner
#define BOX_BR  "\xd9"  // Bottom right corner
#define BOX_H   "\xc4"  // Horizontal line
#define BOX_V   "\xb3"  // Vertical line
#define BOX_TM  "\xc2"  // T-junction middle top
#define BOX_BM  "\xc1"  // T-junction middle bottom
#define BOX_LM  "\xc3"  // T-junction middle left
#define BOX_RM  "\xb4"  // T-junction middle right
#define BOX_C   "\xc5"  // Cross junction

// Box drawing characters (double line)
#define BOX_TL2 "\xc9"  // Top left corner
#define BOX_TR2 "\xbb"  // Top right corner
#define BOX_BL2 "\xc8"  // Bottom left corner
#define BOX_BR2 "\xbc"  // Bottom right corner
#define BOX_H2  "\xcd"  // Horizontal line
#define BOX_V2  "\xba"  // Vertical line
#define BOX_TM2 "\xcb"  // T-junction middle top
#define BOX_BM2 "\xca"  // T-junction middle bottom
#define BOX_LM2 "\xcc"  // T-junction middle left
#define BOX_RM2 "\xb9"  // T-junction middle right
#define BOX_C2  "\xce"  // Cross junction

// Scroll indicator arrows
#define ARROW_UP   "\x18"
#define ARROW_DOWN "\x19"
#define ARROW_LEFT "\x1b"
#define ARROW_RIGHT "\x1a"
#define CHECK_MARK "\xfb"
#define BULLET_POINT "\xf9"

void draw_screen(int cf, int cb, int bt, int pl, int mh, int mv, char mc, int ph, int pv, char pc, char (*content)(int x, int y, int w, int h)) {
  gfxt_set_rendering(1);
  gfxt_clear();
  int w, h;
  gfxt_get_size(&w, &h);
  h--;
  if (cf >= 0 && cb >= 0) {
    gfxt_printf("\x1b[%d;%dm", cb + 40, cf + 30);
  }
  int bw = bt ? 1 : 0;
  int bx = mh, bxr = w - 1 - mh;
  int by = mv, byr = h - pl - mv;
  int cx = mh + bw + ph, cxr = w - 1 - mh - bw - ph;
  int cy = mv + bw + pv, cyr = h - pl - mv - bw - pv;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (x < mh || x > w - 1 - mh || y < mv || y > h - pl - mv) {
        gfxt_putchar(mc);
      } else if (bt && x == bx  && y == by)  { gfxt_print(bt == 1 ? BOX_TL : BOX_TL2);
      } else if (bt && x == bxr && y == by)  { gfxt_print(bt == 1 ? BOX_TR : BOX_TR2);
      } else if (bt && x == bx  && y == byr) { gfxt_print(bt == 1 ? BOX_BL : BOX_BL2);
      } else if (bt && x == bxr && y == byr) { gfxt_print(bt == 1 ? BOX_BR : BOX_BR2);
      } else if (bt && (y == by || y == byr) && x > bx && x < bxr) { gfxt_print(bt == 1 ? BOX_H : BOX_H2);
      } else if (bt && (x == bx || x == bxr) && y > by && y < byr) { gfxt_print(bt == 1 ? BOX_V : BOX_V2);
      } else if (x < cx || x > cxr || y < cy || y > cyr) {
        gfxt_putchar(pc);
      } else {
        gfxt_putchar(content(x - cx, y - cy, w - 2 * mh - 2 * bw - 2 * ph, h - 2 * mv - 2 * bw - 2 * pv + !pl));
      }
    }
  }
  gfxt_set_rendering(0);
}

static int ansi_seq_len(const char *p) {
  if (*p != '\x1b' || *(p+1) != '[') return 0;
  int i = 2;
  while (p[i] && p[i] != 'm') i++;
  return p[i] == 'm' ? i + 1 : 0;
}

const char *content_text(int x, int y, int w, int h, const char *text, int cf, int cb) {
  static struct { const char *start; int vlen; int centered; int wrap_cont; } lines[128];
  static char last_ansi[12];
  static int nlines;
  static char out[128];

  if (x == 0 && y == 0) {
    nlines = 0;
    last_ansi[0] = '\0';
    const char *p = text;
    while (*p && nlines < 128) {
      const char *seg = p;
      int seq;

      const char *peek = p;
      while ((seq = ansi_seq_len(peek)) > 0) peek += seq;
      int centered = 1;
      if (*peek == '\r') { centered = 0; p = peek + 1; }

      while (*p && *p != '\n') p++;
      const char *seg_end = p;

      int vlen = 0;
      const char *q = seg;
      while (q < seg_end) {
        if ((seq = ansi_seq_len(q)) > 0) { q += seq; continue; }
        if (*q == '\r') { q++; continue; }
        vlen++; q++;
      }

      if (vlen <= w) {
        lines[nlines].start = seg;
        lines[nlines].vlen = vlen;
        lines[nlines].centered = centered;
        lines[nlines].wrap_cont = 0;
        nlines++;
      } else {
        const char *src = seg;
        char cur[12]; cur[0] = '\0';
        int first = 1;
        while (src < seg_end && nlines < 128) {
          const char *q2 = src;
          int vi = 0;
          const char *last_sp = NULL;
          while (q2 < seg_end && vi < w) {
            if ((seq = ansi_seq_len(q2)) > 0) {
              if (seq < (int)sizeof(cur)) { memcpy(cur, q2, seq); cur[seq] = '\0'; }
              q2 += seq; continue;
            }
            if (*q2 == '\r') { q2++; continue; }
            if (*q2 == ' ') last_sp = q2;
            vi++; q2++;
          }
          const char *cut = (q2 < seg_end && last_sp) ? last_sp : q2;
          int sv = 0;
          const char *qq = src;
          while (qq < cut) {
            if ((seq = ansi_seq_len(qq)) > 0) { qq += seq; continue; }
            if (*qq == '\r') { qq++; continue; }
            sv++; qq++;
          }
          lines[nlines].start = src;
          lines[nlines].vlen = sv;
          lines[nlines].centered = centered;
          lines[nlines].wrap_cont = !first;
          nlines++;
          first = 0;
          src = (q2 < seg_end && last_sp) ? last_sp + 1 : q2;
        }
      }
      if (*p == '\n') p++;
    }
  }

  int start_y = (h - nlines) / 2;
  int li = y - start_y;
  if (li < 0 || li >= nlines) { out[0] = ' '; out[1] = '\0'; return out; }

  int vlen = lines[li].vlen;
  int start_x = lines[li].centered ? (w - vlen) / 2 : 0;

  if (x == w - 1) {
    snprintf(out, sizeof(out), "\x1b[0m\x1b[%d;%dm ", cb + 40, cf + 30);
    return out;
  }

  int col = x - start_x;
  if (col < 0 || col >= vlen) { out[0] = ' '; out[1] = '\0'; return out; }

  int oi = 0;
  if (x == start_x) {
    oi += snprintf(out + oi, sizeof(out) - oi, "\x1b[%d;%dm", cb + 40, cf + 30);
    if (lines[li].wrap_cont && last_ansi[0])
      oi += snprintf(out + oi, sizeof(out) - oi, "%s", last_ansi);
  }

  const char *p = lines[li].start;
  char cur_ansi[32]; int cur_ansi_len = 0;
  int vi = 0, seq;

  while (*p && *p != '\n') {
    if ((seq = ansi_seq_len(p)) > 0) {
      if (seq < (int)sizeof(cur_ansi)) { memcpy(cur_ansi, p, seq); cur_ansi_len = seq; }
      if (vi == col) { memcpy(out + oi, p, seq); oi += seq; }
      p += seq; continue;
    }
    if (*p == '\r') { p++; continue; }
    if (vi == col) {
      if (x == start_x && cur_ansi_len) { memcpy(out + oi, cur_ansi, cur_ansi_len); oi += cur_ansi_len; }
      out[oi++] = *p;
      out[oi] = '\0';
      // update last_ansi when leaving last col of this line
      if (x == start_x + vlen - 1 && cur_ansi_len && cur_ansi_len < (int)sizeof(last_ansi))
        memcpy(last_ansi, cur_ansi, cur_ansi_len + 1);
      return out;
    }
    vi++; p++;
  }

  out[0] = ' '; out[1] = '\0';
  return out;
}

void draw_text_screen(int cf, int cb, int bt, int pl, int mh, int mv, char mc, int ph, int pv, char pc, char *content) {
  gfxt_set_rendering(1);
  gfxt_clear();
  int w, h;
  gfxt_get_size(&w, &h);
  h--;
  if (cf >= 0 && cb >= 0) {
    gfxt_printf("\x1b[%d;%dm", cb + 40, cf + 30);
  }
  int bw = bt ? 1 : 0;
  int bx = mh, bxr = w - 1 - mh;
  int by = mv, byr = h - pl - mv;
  int cx = mh + bw + ph, cxr = w - 1 - mh - bw - ph;
  int cy = mv + bw + pv, cyr = h - pl - mv - bw - pv;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (x < mh || x > w - 1 - mh || y < mv || y > h - pl - mv) {
        gfxt_putchar(mc);
      } else if (bt && x == bx  && y == by)  { gfxt_print(bt == 1 ? BOX_TL : BOX_TL2);
      } else if (bt && x == bxr && y == by)  { gfxt_print(bt == 1 ? BOX_TR : BOX_TR2);
      } else if (bt && x == bx  && y == byr) { gfxt_print(bt == 1 ? BOX_BL : BOX_BL2);
      } else if (bt && x == bxr && y == byr) { gfxt_print(bt == 1 ? BOX_BR : BOX_BR2);
      } else if (bt && (y == by || y == byr) && x > bx && x < bxr) { gfxt_print(bt == 1 ? BOX_H : BOX_H2);
      } else if (bt && (x == bx || x == bxr) && y > by && y < byr) { gfxt_print(bt == 1 ? BOX_V : BOX_V2);
      } else if (x < cx || x > cxr || y < cy || y > cyr) {
        gfxt_putchar(pc);
      } else {
        gfxt_print(content_text(x - cx, y - cy, w - 2 * mh - 2 * bw - 2 * ph, h - 2 * mv - 2 * bw - 2 * pv + !pl, content, cf, cb));
      }
    }
  }
  gfxt_set_rendering(0);
}

int cmd_text_screen_demo(const char *args) {
  char text_content[] = TERM_CYAN "Lorem ipsum dolor sit amet, " TERM_YELLOW "consectetur adipiscing elit. " TERM_GREEN "Sed do eiusmod tempor incididunt " TERM_BLUE "ut aliqua.\n\n" TERM_MAGENTA "\rUt enim ad minim veniam.";
  for (int bt = 0; bt <= 2; bt++) {
    for (int mh = 0; mh <= 2; mh++) {
      for (int mv = 0; mv <= 2; mv++) {
        for (int ph = 0; ph <= 2; ph++) {
          for (int pv = 0; pv <= 2; pv++) {
            draw_text_screen(-1, -1, bt, 0, mh, mv, ' ', ph, pv, ' ', text_content);
            if (*args) {
              cmd_sleep(args);
              if (gfxt_getchar_nb()) {
                return 1;
              }
            } else {
              gfxt_getchar();
            }
          }
        }
      }
    }
  }
  return 0;
}

// ─── Dialog helpers ───────────────────────────────────────────────────────────
// All dialogs block on gfxt_getchar(), redraw on each input, and use TERM_*
// macros to highlight the active selection.

// Build a Y/N prompt string into `out` (size `outsz`).
// `sel` == 0 → Yes highlighted, 1 → No highlighted.
static void _dlg_yn_str(char *out, int outsz, const char *msg, int sel) {
  snprintf(out, outsz,
    "%s\n\n"
    "%s[ Yes ]%s   %s[ No ]%s",
    msg,
    sel == 0 ? TERM_BG_GREEN TERM_WHITE : TERM_RESET, TERM_RESET,
    sel == 1 ? TERM_BG_RED   TERM_WHITE : TERM_RESET, TERM_RESET);
}

// Returns 1 (Yes) or 0 (No). ESC / 'q' returns -1.
int dialog_yn(const char *msg) {
  int sel = 0;
  char buf[512];
  for (;;) {
    _dlg_yn_str(buf, sizeof(buf), msg, sel);
    draw_text_screen(-1, -1, 2, 0, 2, 1, ' ', 2, 1, ' ', buf);
    char k = gfxt_getchar();
    if (k == ANSI_CURSOR_LEFT  || k == ANSI_CURSOR_UP)   sel = 0;
    if (k == ANSI_CURSOR_RIGHT || k == ANSI_CURSOR_DOWN) sel = 1;
    if (k == '\r' || k == '\n') return !sel; // Yes=1, No=0
    if (k == 'y' || k == 'Y')  return 1;
    if (k == 'n' || k == 'N')  return 0;
    if (k == '\x1b' || k == 'q') return -1;
  }
}

// ─── Options dialog ───────────────────────────────────────────────────────────
// Build a vertical list of options; active item shown with BG_BLUE highlight.
static void _dlg_options_str(char *out, int outsz, const char *title,
                              const char **opts, int nopts, int sel) {
  int oi = 0;
  oi += snprintf(out + oi, outsz - oi, "%s\n\n", title);
  for (int i = 0; i < nopts && oi < outsz - 64; i++) {
    if (i == sel)
      oi += snprintf(out + oi, outsz - oi,
        TERM_BG_BLUE TERM_WHITE " > %s " TERM_RESET "\n", opts[i]);
    else
      oi += snprintf(out + oi, outsz - oi, "   %s\n", opts[i]);
  }
}

// Returns the index of the chosen option, or -1 on ESC/q.
int dialog_options(const char *title, const char **opts, int nopts) {
  int sel = 0;
  char buf[1024];
  for (;;) {
    _dlg_options_str(buf, sizeof(buf), title, opts, nopts, sel);
    draw_text_screen(-1, -1, 2, 0, 2, 1, ' ', 2, 1, ' ', buf);
    char k = gfxt_getchar();
    if (k == ANSI_CURSOR_UP)   { sel = (sel - 1 + nopts) % nopts; }
    if (k == ANSI_CURSOR_DOWN) { sel = (sel + 1) % nopts; }
    if (k == '\r' || k == '\n') return sel;
    if (k == '\x1b' || k == 'q') return -1;
  }
}

// ─── Text input dialog ────────────────────────────────────────────────────────
static void _dlg_input_str(char *out, int outsz, const char *prompt,
                            const char *val, int cursor) {
  char field[128];
  int vi = 0;
  for (int i = 0; val[i] && vi < (int)sizeof(field) - 32; i++, vi++) {
    if (i == cursor)
      vi += snprintf(field + vi, sizeof(field) - vi,
        TERM_BG_CYAN TERM_BLACK "%c" TERM_RESET, val[i]) - 1;
    else
      field[vi] = val[i];
  }
  // cursor at end
  if (cursor >= (int)strlen(val) && vi < (int)sizeof(field) - 16)
    vi += snprintf(field + vi, sizeof(field) - vi, TERM_BG_CYAN TERM_BLACK "_" TERM_RESET);
  field[vi] = '\0';
  snprintf(out, outsz, "%s\n\n\r[ %s ]", prompt, field);
}

// Fills `result` (max `maxlen` chars). Returns 1 on Enter, 0 on ESC.
int dialog_input(const char *prompt, char *result, int maxlen) {
  char val[256] = {0};
  int len = 0, cursor = 0;
  char buf[512];
  for (;;) {
    _dlg_input_str(buf, sizeof(buf), prompt, val, cursor);
    draw_text_screen(-1, -1, 2, 0, 2, 1, ' ', 2, 1, ' ', buf);
    char k = gfxt_getchar();
    if (k == '\r' || k == '\n') {
      int cp = maxlen - 1 < len ? maxlen - 1 : len;
      memcpy(result, val, cp); result[cp] = '\0';
      return 1;
    }
    if (k == '\x1b') return 0;
    if (k == ANSI_CURSOR_LEFT  && cursor > 0)   cursor--;
    if (k == ANSI_CURSOR_RIGHT && cursor < len)  cursor++;
    if ((k == '\b' || k == 127) && cursor > 0) {
      memmove(val + cursor - 1, val + cursor, len - cursor);
      len--; cursor--; val[len] = '\0';
    } else if (k >= 0x20 && k < 0x7f && len < maxlen - 1) {
      memmove(val + cursor + 1, val + cursor, len - cursor);
      val[cursor++] = k; len++; val[len] = '\0';
    }
  }
}

// ─── Password input dialog ────────────────────────────────────────────────────
static void _dlg_pass_str(char *out, int outsz, const char *prompt, int len) {
  char stars[128];
  int i;
  for (i = 0; i < len && i < 127; i++) stars[i] = '*';
  stars[i] = '\0';
  snprintf(out, outsz, "%s\n\n\r[ %s_ ]", prompt, stars);
}

// Fills `result` (max `maxlen` chars). Returns 1 on Enter, 0 on ESC.
int dialog_password(const char *prompt, char *result, int maxlen) {
  char val[256] = {0};
  int len = 0;
  char buf[512];
  for (;;) {
    _dlg_pass_str(buf, sizeof(buf), prompt, len);
    draw_text_screen(-1, -1, 2, 0, 2, 1, ' ', 2, 1, ' ', buf);
    char k = gfxt_getchar();
    if (k == '\r' || k == '\n') {
      int cp = maxlen - 1 < len ? maxlen - 1 : len;
      memcpy(result, val, cp); result[cp] = '\0';
      return 1;
    }
    if (k == '\x1b') return 0;
    if ((k == '\b' || k == 127) && len > 0) { len--; val[len] = '\0'; }
    else if (k >= 0x20 && k < 0x7f && len < maxlen - 1) { val[len++] = k; val[len] = '\0'; }
  }
}

// ─── Demo commands ────────────────────────────────────────────────────────────
static int cmd_dialog_yn_demo(const char *args) {
  (void)args;
  int r = dialog_yn("Delete all files?\nThis cannot be undone.");
  if (r == 1)  draw_text_screen(-1, -1, 1, 0, 2, 1, ' ', 2, 1, ' ', TERM_GREEN "Confirmed!");
  else if (r == 0) draw_text_screen(-1, -1, 1, 0, 2, 1, ' ', 2, 1, ' ', TERM_RED "Cancelled.");
  else draw_text_screen(-1, -1, 1, 0, 2, 1, ' ', 2, 1, ' ', "Escaped.");
  gfxt_getchar();
  return 0;
}

static int cmd_dialog_opts_demo(const char *args) {
  (void)args;
  const char *opts[] = { "Option Alpha", "Option Beta", "Option Gamma", "Option Delta" };
  int r = dialog_options("Choose an option:", opts, 4);
  char msg[64];
  if (r >= 0) snprintf(msg, sizeof(msg), TERM_GREEN "Selected: %s", opts[r]);
  else        snprintf(msg, sizeof(msg), TERM_RED "Cancelled.");
  draw_text_screen(-1, -1, 1, 0, 2, 1, ' ', 2, 1, ' ', msg);
  gfxt_getchar();
  return 0;
}

static int cmd_dialog_input_demo(const char *args) {
  (void)args;
  char result[64] = {0};
  int r = dialog_input("Enter your name:", result, sizeof(result));
  char msg[128];
  if (r) snprintf(msg, sizeof(msg), TERM_GREEN "Hello, %s!", result);
  else   snprintf(msg, sizeof(msg), TERM_RED "Cancelled.");
  draw_text_screen(-1, -1, 1, 0, 2, 1, ' ', 2, 1, ' ', msg);
  gfxt_getchar();
  return 0;
}

static int cmd_dialog_pass_demo(const char *args) {
  (void)args;
  char result[64] = {0};
  int r = dialog_password("Enter password:", result, sizeof(result));
  char msg[128];
  if (r) snprintf(msg, sizeof(msg), TERM_GREEN "Got %d chars.", (int)strlen(result));
  else   snprintf(msg, sizeof(msg), TERM_RED "Cancelled.");
  draw_text_screen(-1, -1, 1, 0, 2, 1, ' ', 2, 1, ' ', msg);
  gfxt_getchar();
  return 0;
}

void dialog_test_cmd_reg() {
  gfxt_register_cmd("tsdemo",   "show text screen demo",     cmd_text_screen_demo);
  gfxt_register_cmd("dlgyn",   "yes/no dialog demo",        cmd_dialog_yn_demo);
  gfxt_register_cmd("dlgopts", "options list dialog demo",  cmd_dialog_opts_demo);
  gfxt_register_cmd("dlgin",   "text input dialog demo",    cmd_dialog_input_demo);
  gfxt_register_cmd("dlgpass", "password input dialog demo",cmd_dialog_pass_demo);
}