#pragma once

// ════════════════════════════════════════════════════════════════════════
//  startcmds.h — run start_lang code from the simpleterm shell
//
//  Usage:
//    start <code>              foreground (blocking)
//    start demo mandelbrot     mandelbrot in start_lang (foreground)
//    start bg [N] <code>       background, N steps per frame
//    start bg stop             stop background
//    start bg status           show status
//
//  In your app:
//    #include "ext/start/startcmds.h"
//    gfxt_start_cmd_reg();          // in gfx_app(init)
//    start_process_data(s);         // in gfx_process_data()
//
//  Build setup:
//    INCLUDES  += -Ilib/start/src -Ilib/start/lib/tools
//    SOURCES   += lib/start/src/start_lang.c
// ════════════════════════════════════════════════════════════════════════

#include "ext/term/simpleterm.h"
#include "ext/term/ansiutils.h"
#include <string.h>
#include <stdlib.h>
#include <start_lang.h>

// ── Terminal-aware I/O for start_lang ─────────────────────────────────
// (PRINT/INPUT tokens . and , call these directly; ext functions call
//  the gfxt_ versions too, but those go through the ext[] array)

#define STDF_IGN_PRINT
#define STDF_IGN_INPUT
#include <stdf.h>

int8_t f_print(State *s) { gfxt_putchar((char)s->_m[0]); return 0; }
int8_t f_input(State *s) {
  char c = gfxt_getchar();
  s->_m[0] = (uint8_t)(c == 27 ? 0 : c);
  return 0;
}

// ── Ext functions (callable by name from start_lang code) ──────────────

static int8_t x_print(State *s)    { gfxt_putchar((char)s->_m[0]); return 0; }
static int8_t x_printstr(State *s) { gfxt_print((const char*)s->_m); return 0; }
static int8_t x_printnum(State *s) {
  char b[24];
  int n;
  if      (s->_type == INT8)  n = snprintf(b,sizeof(b),"%d",(int8_t)s->reg.i8[0]),  s->reg.i8[0]=(uint8_t)n;
  else if (s->_type == INT16) n = snprintf(b,sizeof(b),"%d",(int16_t)s->reg.i16[0]),s->reg.i16[0]=(uint16_t)n;
  else if (s->_type == INT32) n = snprintf(b,sizeof(b),"%d",(int32_t)s->reg.i32),   s->reg.i32=(uint32_t)n;
  else                        n = snprintf(b,sizeof(b),"%g",(double)s->reg.f32),    s->reg.f32=(float)n;
  gfxt_print(b); return 0;
}
static int8_t x_input(State *s) {
  char c = gfxt_getchar();
  s->_m[0] = (uint8_t)(c == 27 ? 0 : c);
  return 0;
}
static int8_t x_rand(State *s) {
  static uint32_t seed = 0;
  if (!seed) seed = gfx_time();
  seed = seed * 1103515245 + 12345;
  s->reg.i32 = (seed / 65536) % 32768;
  return 0;
}
static int8_t x_cmd(State *s) {
  gfxt_run_cmd((const char*)s->_m);
  return 0;
}
static int8_t x_undef(State *s)     { gfxt_printf(TERM_RED "?%s" TERM_RESET,(char*)s->_id); return 0; }

Function ext[] = {
  {(uint8_t*)"",       NULL},       // exception handler (NULL = skip)
  {(uint8_t*)"PRINT",  x_print},
  {(uint8_t*)"PC",     x_print},
  {(uint8_t*)"PRINTSTR",x_printstr},
  {(uint8_t*)"PS",     x_printstr},
  {(uint8_t*)"PRINTNUM",x_printnum},
  {(uint8_t*)"PN",     x_printnum},
  {(uint8_t*)"IN",     x_input},
  {(uint8_t*)"INPUT",  x_input},
  {(uint8_t*)"RAND",   x_rand},
  {(uint8_t*)"CMD",    x_cmd},
  {(uint8_t*)"SHELL",  x_cmd},
  {NULL, x_undef},
};

// ── Step callback ─────────────────────────────────────────────────────

static int bg_stop_flag = 0;

int8_t step_callback(State *s) {
  (void)s;
  return bg_stop_flag;
}

// ── Demos ─────────────────────────────────────────────────────────────

// Builds mandelbrot source sized to terminal (lines-1 × cols)
static char *mandel_src(void) {
  int tw = 78, th = 24;
  gfxt_get_size(&tw, &th);
  int h = th - 1;
  int w = tw;
  char head[48];
  snprintf(head, sizeof(head), "sLL^%d!>L^!>CC^%d!>C^!>", h, w);
  const char *body =
    "fCX^>CY^>TX^>TY^>TZ^>X^>Y^>bI^"
    "sL 1- [ sCC;C! 1- ["
    "  b0I! f0X!Y!"
    "  i0sCC;efCX!3/ i0sC;efCX@/ 2-"
    "  i0sLL;efCY!2/ i0sL;efCY@/ 1-"
    "  bI 10! ["
    "    f X;TX!* Y;TY!* 0TZ! TX;TZ+ TY;TZ+ 4?> ( x )"
    "    Y 2* X;Y* CY;Y+ TX;X! TY;X- CX;X+"
    "    bI 1- ??"
    "  ]"
    "  bI;TZ! 38+ ."
    "  sC 1- ??"
    "]"
    "  b10TZ! ."
    "  sL 1- ??"
    "]";
  char *src = (char*)malloc(strlen(head) + strlen(body) + 1);
  sprintf(src, "%s%s", head, body);
  return src;
}

static const char *SRC_HELLO  = "\"Hello, World!\" PS";
static const char *SRC_PRIMES = "iN^ i> iMASK^ i> iSUM^ i> iD^ i> bIS_PRIME^ b> iQ^ i> iI^ i> i_STR_^ i"
  "2 N! i1 MASK! i0 SUM! t[ i2 D! b1 IS_PRIME! t[ iN; iD ?< ~(x:) i0 Q! i0 I! t[ iN; iI ?< ~(x:) "
  "1 iQ; 1+ iQ; 1 iI; 1+ iI; t] t[ iD; iQ ?g ~(x:) i0 I! t[ iD; iI ?< ~(x:) 1 iQ; 1- iQ; 1 iI; "
  "1+ iI; t] t] i0 iQ ?= ( b0 IS_PRIME! x: ) 1 iD; 1+ iD; t] bIS_PRIME; ?? ( i0 I! t[ iN; iI ?< "
  "~(x:) 1 iSUM; 1+ iSUM; 1 iI; 1+ iI; t] iMASK; iSUM ?> ( i1 MASKw< i0 iMASK ?= ( i_STR_; \"done\" "
  "PS b10@.@ x: ) iN; PN b10@.@ ) )1 iN; 1+ iN; t]";

// ── Background state ──────────────────────────────────────────────────

static State  *bg_root       = NULL;
static int     bg_fixed      = 0;     // >0 steps/frame, 0 adaptive, <0 every |n| frames
static uint32_t bg_steps     = 0;
static uint32_t bg_start     = 0;
static uint32_t bg_frame     = 0;     // frame counter for throttle (<0) mode
static char     bg_src[512]  = {0};

static void bg_stop(void) {
  if (bg_root) { st_state_free(bg_root); bg_root = NULL; }
}

static int bg_init(const char *code, int fixed) {
  bg_stop();
  size_t len = strlen(code);
  if (!len || len >= sizeof(bg_src)) return -1;
  strcpy(bg_src, code);
  bg_root = (State*)calloc(1, sizeof(State));
  bg_root->src = (uint8_t*)bg_src;
  st_state_init(bg_root);
  bg_fixed  = fixed;
  bg_steps  = 0;
  bg_frame  = 0;
  bg_start  = gfx_time();
  return 0;
}

// ── Call this from gfx_process_data() ─────────────────────────────────

void start_process_data(gfx_step_t *s) {
  if (!bg_root) return;

  // throttle mode (< 0): 1 step every |bg_fixed| frames
  if (bg_fixed < 0) {
    bg_frame++;
    if (bg_frame < (uint32_t)(-bg_fixed)) return;
    bg_frame = 0;
  }

  // how many steps this frame
  int n;
  if (bg_fixed > 0) {
    n = bg_fixed;                          // fixed
  } else if (bg_fixed == 0) {
    n = 256;                               // adaptive: cap, real limit is time
  } else {
    n = 1;                                 // throttle: 1 step
  }

  uint32_t budget = s && s->budget ? (uint32_t)s->budget : 16;  // ms per frame
  uint32_t t0 = (bg_fixed == 0) ? gfx_time() : 0;

  for (int i = 0; i < n && bg_root; i++) {
    State *sub = bg_root;
    while (sub->sub) sub = sub->sub;
    if (step_callback(sub)) break;

    int8_t r = st_step(bg_root);
    bg_steps++;

    if (r == LOOP_ST) {
      uint32_t ms = gfx_dt(bg_start);
      gfxt_printf(TERM_GREEN "\n[start] %lu steps in %.2fs" TERM_RESET "\n",
                  (unsigned long)bg_steps, ms / 1000.0);
      bg_stop(); return;
    }
    if (r >= JM_ERR0 || r == BL_PREV) {
      gfxt_printf(TERM_RED "\n[start] error %d" TERM_RESET "\n", r);
      bg_stop(); return;
    }

    // adaptive: stop if frame budget exhausted
    if (bg_fixed == 0 && gfx_dt(t0) >= budget) break;
  }
}

// ── Demo name → code resolver ───────────────────────────────────────
//  Returns code string; sets *free_code = 1 if caller must free().
//  Returns NULL for unknown names.
static const char *demo_resolve(const char *name, int *free_code) {
  *free_code = 0;
  if      (!strcmp(name,"hello")    || !strcmp(name,"h"))     return SRC_HELLO;
  else if (!strcmp(name,"mandelbrot")||!strcmp(name,"mb"))   { *free_code = 1; return mandel_src(); }
  else if (!strcmp(name,"primes")   || !strcmp(name,"p"))    return SRC_PRIMES;
  return NULL;
}

// ── Command: start ────────────────────────────────────────────────────

static int cmd_start(const char *args) {
  if (!args || !*args) goto help;

  // bare stop / status shortcuts
  if (!strcmp(args, "stop"))   { bg_stop(); gfxt_println(TERM_GREEN "stopped" TERM_RESET); return 0; }
  if (!strcmp(args, "status")) {
    if (bg_root) {
      const char *mode = bg_fixed > 0 ? "fixed" : bg_fixed < 0 ? "slow" : "adaptive";
      gfxt_printf(TERM_GREEN "bg %s %lu steps" TERM_RESET "\n", mode, (unsigned long)bg_steps);
    } else {
      gfxt_println(TERM_BBLACK "bg idle" TERM_RESET);
    }
    return 0;
  }

  // bg stop / bg status / bg [N] <code>
  if (strncmp(args, "bg ", 3) == 0) {
    const char *a = args + 3;
    if      (!strcmp(a, "stop"))   { bg_stop(); gfxt_println(TERM_GREEN "stopped" TERM_RESET); return 0; }
    else if (!strcmp(a, "status")) {
      if (bg_root) {
        const char *mode = bg_fixed > 0 ? "fixed" : bg_fixed < 0 ? "slow" : "adaptive";
        gfxt_printf(TERM_GREEN "bg %s %lu steps" TERM_RESET "\n", mode, (unsigned long)bg_steps);
      }
      else         gfxt_println(TERM_BBLACK "bg idle" TERM_RESET);
      return 0;
    }

    // bg [N] <code>
    int n = 0;
    if ((*a >= '0' && *a <= '9') || *a == '-') { n = atoi(a); while (*a && *a != ' ') a++; while (*a == ' ') a++; }
    if (!*a) { gfxt_println("usage: start bg [N] <code>"); return 1; }

    // bg demo <name>
    if (strncmp(a, "demo", 4) == 0) {
      const char *name = a + 4; while (*name == ' ') name++;
      if (!*name) { gfxt_println("usage: start bg [N] demo <name>"); return 1; }
      int free_code = 0;
      const char *code = demo_resolve(name, &free_code);
      if (!code) { gfxt_printf(TERM_RED "unknown: %s" TERM_RESET "\n", name); return 1; }
      if (bg_init(code, n)) { gfxt_println(TERM_RED "code too long" TERM_RESET); if (free_code) free((void*)code); return 1; }
      if (free_code) free((void*)code);
      const char *mode = n > 0 ? "fixed" : n < 0 ? "slow" : "adaptive";
      gfxt_printf(TERM_GREEN "bg %s demo %s" TERM_RESET "\n", mode, name);
      return 0;
    }

    if (bg_init(a, n)) { gfxt_println(TERM_RED "code too long" TERM_RESET); return 1; }
    const char *mode = n > 0 ? "fixed" : n < 0 ? "slow" : "adaptive";
    gfxt_printf(TERM_GREEN "bg %s" TERM_RESET "\n", mode);
    return 0;
  }

  // demo
  if (strncmp(args, "demo", 4) == 0) {
    const char *name = args + 4; while (*name == ' ') name++;
    if (!*name) {
      gfxt_printf("demos:\n"
        "  " TERM_GREEN "start demo hello" TERM_RESET "\n"
        "  " TERM_GREEN "start demo mandelbrot" TERM_RESET "\n"
        "  " TERM_GREEN "start demo primes" TERM_RESET "\n");
      return 0;
    }

    int free_code = 0;
    const char *code = demo_resolve(name, &free_code);
    if (!code) { gfxt_printf(TERM_RED "unknown: %s" TERM_RESET "\n", name); return 1; }

    int save_pager_quit = pager_quit;
    pager_quit = 1;
    pager_lines = 0;

    bg_stop_flag = 0;
    State *st = (State*)calloc(1, sizeof(State));
    char *cp = strdup(code);
    if (free_code) free((void*)code);
    st->src = (uint8_t*)cp;
    uint32_t t0 = gfx_time();
    int8_t r = st_run(st);
    uint32_t ms = gfx_dt(t0);

    pager_quit = save_pager_quit;

    if (r >= JM_ERR0 || r == BL_PREV)
      gfxt_printf(TERM_RED "\n[start] error %d" TERM_RESET "  %lums\n", r, (unsigned long)ms);
    else
      gfxt_printf(TERM_GREEN "\n[start] ok" TERM_RESET "  %lums\n", (unsigned long)ms);
    st_state_free(st); free(cp);
    return 0;
  }

  // raw code (foreground)
  {
    gfxt_printf(TERM_CYAN "[start] %s" TERM_RESET "\n", args);

    int save_pager_quit = pager_quit;
    pager_quit = 1;
    pager_lines = 0;

    bg_stop_flag = 0;
    State *st = (State*)calloc(1, sizeof(State));
    char *cp = strdup(args);
    st->src = (uint8_t*)cp;
    uint32_t t0 = gfx_time();
    int8_t r = st_run(st);
    uint32_t ms = gfx_dt(t0);

    pager_quit = save_pager_quit;

    if (r >= JM_ERR0 || r == BL_PREV)
      gfxt_printf(TERM_RED "\n[start] error %d" TERM_RESET "  %lums\n", r, (unsigned long)ms);
    else
      gfxt_printf(TERM_GREEN "\n[start] ok" TERM_RESET "  %lums\n", (unsigned long)ms);
    st_state_free(st); free(cp);
    return 0;
  }

help:
  gfxt_printf(
    TERM_CYAN "start" TERM_RESET " — run start_lang code\n"
    "  " TERM_YELLOW "start <code>" TERM_RESET "           foreground\n"
    "  " TERM_YELLOW "start demo hello" TERM_RESET "       hello world\n"
    "  " TERM_YELLOW "start demo mandelbrot" TERM_RESET "  mandelbrot\n"
    "  " TERM_YELLOW "start demo primes" TERM_RESET "      prime numbers\n"
    "  " TERM_YELLOW "start bg [N] <code>" TERM_RESET "    N>0 fixed, N<0 slow, N=0 adaptive\n"
    "  " TERM_YELLOW "start bg stop" TERM_RESET "          stop background\n"
    "  " TERM_YELLOW "start bg status" TERM_RESET "        show status\n"
  );
  return 0;
}

static void gfxt_start_cmd_reg(void) {
  gfxt_register_cmd("start", "run start_lang code (demo/bg/raw)", cmd_start);
}
