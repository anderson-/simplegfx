#pragma once

#include "ansiutils.h"
#include "simpleterm.h"
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

int simple_screen(const char *args) {
  gfxt_clear();
  gfxt_printf("Simple Screen\n");
  gfxt_printf("Press any key to continue...\n");
  gfxt_getchar();
  return 0;
}

//#define gfxt_print(str) printf("%s", str)
//#define gfxt_println(str) printf("%s\n", str)
//#define gfxt_putchar(ch) printf("%c", ch)

void draw_screen(int cf, int cb, int bt, int pl, int mh, int mv, char mc, int ph, int pv, char pc, char (*content)(int x, int y)) {
  gfxt_clear();
  int w, h;
  gfxt_get_size(&w, &h);
  h--;
  if (cf >= 0 && cb >= 0) {
    gfxt_printf("\x1b[%d;%dm", cb + 40, cf + 30);
  }
  // bt is a style enum (0=none, 1=single, 2=double); border thickness is always 0 or 1
  int bw = bt ? 1 : 0;
  // border positions (h already decremented above)
  int bx = mh,          bxr = w - 1 - mh;
  int by = mv,          byr = h - pl - mv;
  // content region
  int cx = mh + bw + ph, cxr = w - 1 - mh - bw - ph;
  int cy = mv + bw + pv, cyr = h - pl - mv - bw - pv;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // margin
      if (x < mh || x > w - 1 - mh || y < mv || y > h - pl - mv) {
        gfxt_putchar(mc);
      // border corners and lines
      } else if (bt && x == bx  && y == by)  { gfxt_print(bt == 1 ? BOX_TL : BOX_TL2);
      } else if (bt && x == bxr && y == by)  { gfxt_print(bt == 1 ? BOX_TR : BOX_TR2);
      } else if (bt && x == bx  && y == byr) { gfxt_print(bt == 1 ? BOX_BL : BOX_BL2);
      } else if (bt && x == bxr && y == byr) { gfxt_print(bt == 1 ? BOX_BR : BOX_BR2);
      } else if (bt && (y == by || y == byr) && x > bx && x < bxr) { gfxt_print(bt == 1 ? BOX_H : BOX_H2);
      } else if (bt && (x == bx || x == bxr) && y > by && y < byr) { gfxt_print(bt == 1 ? BOX_V : BOX_V2);
      // padding
      } else if (x < cx || x > cxr || y < cy || y > cyr) {
        gfxt_putchar(pc);
      // content
      } else {
        gfxt_putchar(content(x, y));
      }
    }
  }
}

char content_test(int x, int y) {
  return 'A' + (x + y) % 26;
}

int full_screen(const char *args) {
  draw_screen( // border + margin + padding
    2,  // foreground color
    4,  // background color
    1,  // border type
    0,  // prompt line
    1,  // margin horizontal
    1,  // margin vertical
    'm', // margin character
    1,  // padding horizontal
    1,  // padding vertical
    'p',  // padding character
    content_test  // content function
  );

  char c = gfxt_getchar();
  gfxt_printf("You pressed: %d\n", c);
  return 0;
}

void dialog_test_cmd_reg() {
  gfxt_register_cmd("simple", "show simple screen", simple_screen);
  gfxt_register_cmd("full", "show full screen", full_screen);
}