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
}

int cmd_text_screen_demo(const char *args) {
  char text_content[1024] = TERM_CYAN "Lorem ipsum dolor sit amet, " TERM_YELLOW "consectetur adipiscing elit. " TERM_GREEN "Sed do eiusmod tempor incididunt " TERM_BLUE "ut aliqua.\n\n" TERM_MAGENTA "\rUt enim ad minim veniam.";
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

void dialog_test_cmd_reg() {
  gfxt_register_cmd("tsdemo", "show text screen demo", cmd_text_screen_demo);
}