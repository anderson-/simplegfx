#include "tracker.h"
#include "simplegfx.h"
#include "ext/term/simpleterm.h"
#include "ext/audio/sfxr.h"
#include "ext/audio/sfxr_presets.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

extern int gfx_headless_stdin_eof __attribute__((weak));

#define TR_CH 4
#define TR_ROWS 64
#define TR_PAT 8
#define TR_ORD 32
#define TR_INS 16
#define EMPTY -1
#define CUT -2

enum { M_PATTERN, M_ORDER, M_INSTR, M_HELP };
enum { F_NOTE, F_INST, F_VOL, F_FX, F_PARAM };
enum {
  FX_NONE, FX_UP, FX_DN, FX_VIB, FX_ARP, FX_RET, FX_PARAM,
  FX_SPEED = 9, FX_VOLSL = 10, FX_JUMP = 11, FX_BREAK = 13
};

typedef struct { signed char note, inst, vol, fx, param; } tr_cell;
typedef struct { tr_cell c[TR_CH]; } tr_row;
typedef struct { tr_row row[TR_ROWS]; } tr_pattern;
typedef struct { float p[GFXA_SFXR_PARAM_COUNT]; char used; } tr_inst;
typedef struct {
  tr_pattern pat[TR_PAT];
  int order[TR_ORD], order_len, bpm, ticks;
  tr_inst inst[TR_INS];
} tr_song;
typedef struct {
  int note, inst, vol, fx, param, retrig, arp_step;
  float base_period, slide_period, vib_phase;
} tr_voice;
typedef struct {
  volatile int has, op, vol;
  volatile float period, vib_spd, vib_dep;
  struct sfxr_state *sfxr;
} tr_acmd;
typedef struct {
  tr_song s;
  tr_song undo;
  tr_voice v[TR_CH];
  tr_acmd cmd[TR_CH];
  int mode, row, chan, field, cur_inst, cur_pat, cur_ord, top;
  int playing, play_ord, play_row, tick, follow, dirty, quit, help_prev;
  int ord_cursor, inst_param, preview_note, octave, mute, solo, has_undo;
  tr_cell clip;
  char path[64];
  long next_ms;
} tracker;

static const char *pnames[GFXA_SFXR_PARAM_COUNT] = {
  "Wave","Attack","Sustain","Punch","Decay","BaseFreq","FreqLimit",
  "FreqRamp","FreqDRamp","VibDepth","VibSpeed","ArpMod","ArpSpeed",
  "Duty","DutyRamp","Repeat","PhaserOff","PhaserRamp","LPF","LPFRamp",
  "LPFRes","HPF","HPFRamp","Volume"
};

static long now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

static tr_cell empty_cell(void) { tr_cell c = {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY}; return c; }

static void song_init(tr_song *s) {
  memset(s, 0, sizeof(*s));
  for (int p = 0; p < TR_PAT; p++)
    for (int r = 0; r < TR_ROWS; r++)
      for (int c = 0; c < TR_CH; c++) s->pat[p].row[r].c[c] = empty_cell();
  s->order_len = 1; s->order[0] = 0; s->bpm = 125; s->ticks = 6;
  for (int i = 0; i < TR_INS; i++) {
    gfxa_sfxr_defaults(s->inst[i].p);
    s->inst[i].p[GFXA_SFXR_WAVE_TYPE] = i % 3;
    s->inst[i].p[GFXA_SFXR_BASE_FREQ] = 0.45f;
    s->inst[i].p[GFXA_SFXR_DECAY] = 0.28f;
    s->inst[i].p[GFXA_SFXR_SUSTAIN_TIME] = 0.12f;
    s->inst[i].used = i == 0;
  }
}

static int song_save(tr_song *s, const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) return 1;
  fprintf(f, "TRK1 %d %d %d\n", s->bpm, s->ticks, s->order_len);
  for (int i = 0; i < s->order_len; i++) fprintf(f, "%d%c", s->order[i], i + 1 == s->order_len ? '\n' : ' ');
  for (int p = 0; p < TR_PAT; p++)
    for (int r = 0; r < TR_ROWS; r++) {
      fprintf(f, "P %d %d", p, r);
      for (int c = 0; c < TR_CH; c++) {
        tr_cell *x = &s->pat[p].row[r].c[c];
        fprintf(f, " %d %d %d %d %d", x->note, x->inst, x->vol, x->fx, x->param);
      }
      fputc('\n', f);
    }
  for (int i = 0; i < TR_INS; i++) {
    fprintf(f, "I %d %d", i, s->inst[i].used);
    for (int p = 0; p < GFXA_SFXR_PARAM_COUNT; p++) fprintf(f, " %.5f", s->inst[i].p[p]);
    fputc('\n', f);
  }
  fclose(f);
  return 0;
}

static int song_load(tr_song *s, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) return 1;
  char tag[8];
  int bpm, ticks, olen;
  if (fscanf(f, "%7s %d %d %d", tag, &bpm, &ticks, &olen) != 4 || strcmp(tag, "TRK1") != 0) {
    fclose(f); return 1;
  }
  song_init(s);
  s->bpm = bpm; s->ticks = ticks; s->order_len = olen;
  if (s->order_len < 1) s->order_len = 1;
  if (s->order_len > TR_ORD) s->order_len = TR_ORD;
  for (int i = 0; i < s->order_len; i++) fscanf(f, "%d", &s->order[i]);
  while (fscanf(f, "%7s", tag) == 1) {
    if (tag[0] == 'P') {
      int p, r; if (fscanf(f, "%d %d", &p, &r) != 2) break;
      for (int c = 0; c < TR_CH; c++) {
        int a,b,d,e,g; if (fscanf(f, "%d %d %d %d %d", &a,&b,&d,&e,&g) != 5) break;
        if (p >= 0 && p < TR_PAT && r >= 0 && r < TR_ROWS)
          s->pat[p].row[r].c[c] = (tr_cell){a,b,d,e,g};
      }
    } else if (tag[0] == 'I') {
      int i, u; if (fscanf(f, "%d %d", &i, &u) != 2) break;
      for (int p = 0; p < GFXA_SFXR_PARAM_COUNT; p++) {
        float v; if (fscanf(f, "%f", &v) != 1) break;
        if (i >= 0 && i < TR_INS) s->inst[i].p[p] = v;
      }
      if (i >= 0 && i < TR_INS) s->inst[i].used = u;
    }
  }
  fclose(f);
  return 0;
}

static float note_period(int note) {
  float hz = 440.0f * powf(2.0f, (note - 69) / 12.0f);
  return (float)GFXA_SAMPLE_RATE * 8.0f / hz;
}

static void note_name(int note, char out[4]) {
  static const char *n[] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
  if (note == EMPTY) { strcpy(out, "..."); return; }
  if (note == CUT) { strcpy(out, "^^^"); return; }
  snprintf(out, 4, "%s%d", n[note % 12], note / 12 - 1);
}

static int hexv(int k) {
  if (k >= '0' && k <= '9') return k - '0';
  if (k >= 'a' && k <= 'f') return k - 'a' + 10;
  if (k >= 'A' && k <= 'F') return k - 'A' + 10;
  return -1;
}

static int key_note(tracker *t, int k) {
  const char *lo = "zsxdcvgbhnjm";
  const char *hi = "q2w3er5t6y7u";
  int base = (t->octave + 1) * 12;
  for (int i = 0; lo[i]; i++) if (k == lo[i]) return base + i;
  for (int i = 0; hi[i]; i++) if (k == hi[i]) return base + 12 + i;
  return EMPTY;
}

static void undo_snap(tracker *t) { t->undo = t->s; t->has_undo = 1; }
static void post(tracker *t, int ch, tr_acmd *cmd);

static int audible(tracker *t, int ch) {
  if (t->solo) return (t->solo & (1 << ch)) != 0;
  return (t->mute & (1 << ch)) == 0;
}

static void chan_vol(tracker *t, int ch) {
  tr_acmd a = {0};
  a.vol = audible(t, ch) ? t->v[ch].vol : 0;
  a.vib_spd = -1.0f;
  post(t, ch, &a);
}

static int sfxr_fill(int16_t *buf, int n, void *user) {
  struct sfxr_state *s = (struct sfxr_state *)user;
  if (!buf) { gfxa_sfxr_destroy(s); return 0; }
  return gfxa_sfxr_read(s, buf, n);
}

static void post(tracker *t, int ch, tr_acmd *cmd) {
  if (ch < 0 || ch >= TR_CH) return;
  while (t->cmd[ch].has) gfx_delay(1);
  t->cmd[ch] = *cmd;
  t->cmd[ch].has = 1;
}

static void audio_ctrl(chan_t *chan, void *data) {
  tr_acmd *c = (tr_acmd *)data;
  if (!c || !c->has) return;
  if (c->vol >= 0) chan->vol = c->vol / 128.0f;
  if (c->op == 1 && c->sfxr) gfxa_chan_play(chan, sfxr_fill, c->sfxr);
  else if (c->op == 2) gfxa_chan_stop(chan);
  if (chan->data) {
    struct sfxr_state *s = (struct sfxr_state *)chan->data;
    if (c->period > 0.0f) gfxa_sfxr_set_period(s, c->period);
    if (c->vib_spd >= 0.0f) gfxa_sfxr_set_vibrato(s, c->vib_spd, c->vib_dep);
  }
  c->sfxr = NULL; c->has = 0;
}

static void play_note(tracker *t, int ch, int note, int inst, int vol) {
  if (inst < 0) inst = t->v[ch].inst >= 0 ? t->v[ch].inst : t->cur_inst;
  if (inst < 0 || inst >= TR_INS) return;
  struct sfxr_state *s = gfxa_sfxr_create(t->s.inst[inst].p);
  if (!s) return;
  float per = note_period(note);
  gfxa_sfxr_set_period(s, per);
  int real_vol = vol >= 0 ? vol : t->v[ch].vol;
  tr_acmd c = {0}; c.op = 1; c.vol = audible(t, ch) ? real_vol : 0; c.period = per;
  c.vib_spd = -1.0f; c.sfxr = s; post(t, ch, &c);
  t->v[ch].note = note; t->v[ch].inst = inst; t->v[ch].base_period = per;
  t->v[ch].slide_period = per; if (vol >= 0) t->v[ch].vol = vol;
}

static void cut_note(tracker *t, int ch) {
  tr_acmd c = {0}; c.op = 2; c.vol = -1; c.vib_spd = -1.0f; post(t, ch, &c);
}

static void row_event(tracker *t) {
  int pat = t->s.order[t->play_ord];
  for (int ch = 0; ch < TR_CH; ch++) {
    tr_cell *c = &t->s.pat[pat].row[t->play_row].c[ch];
    tr_voice *v = &t->v[ch];
    if (c->inst != EMPTY) v->inst = c->inst;
    if (c->vol != EMPTY) {
      v->vol = c->vol;
      chan_vol(t, ch);
    }
    if (c->fx != EMPTY) { v->fx = c->fx; v->param = c->param == EMPTY ? 0 : c->param; }
    if (v->fx == FX_SPEED) {
      if (v->param < 32) t->s.ticks = v->param > 0 ? v->param : 1;
      else t->s.bpm = v->param;
    } else if (v->fx == FX_JUMP && t->s.order_len > 0) {
      t->play_ord = v->param % t->s.order_len; t->play_row = TR_ROWS - 1;
    } else if (v->fx == FX_BREAK) {
      t->play_row = (v->param > 0 ? v->param : 0) - 1;
    }
    v->retrig = 0; v->arp_step = 0;
    if (c->note == CUT) cut_note(t, ch);
    else if (c->note != EMPTY) play_note(t, ch, c->note, v->inst, v->vol);
  }
}

static void tick_effects(tracker *t) {
  for (int ch = 0; ch < TR_CH; ch++) {
    tr_voice *v = &t->v[ch];
    if (v->note < 0) continue;
    float p = v->slide_period, vib_spd = -1.0f, vib_dep = 0.0f;
    int send = 0;
    if (v->fx == FX_UP) { p -= (v->param ? v->param : 1) * 0.4f; send = 1; }
    if (v->fx == FX_DN) { p += (v->param ? v->param : 1) * 0.4f; send = 1; }
    if (v->fx == FX_VIB) {
      vib_spd = ((v->param >> 4) + 1) / 15.0f;
      vib_dep = (v->param & 15) / 15.0f;
      send = 1;
    }
    if (v->fx == FX_ARP) {
      int a = (v->param >> 4) & 15, b = v->param & 15;
      int add = v->arp_step == 1 ? a : (v->arp_step == 2 ? b : 0);
      p = v->base_period / powf(2.0f, add / 12.0f);
      v->arp_step = (v->arp_step + 1) % 3; send = 1;
    }
    if (v->fx == FX_RET && v->param > 0 && ++v->retrig >= v->param) {
      v->retrig = 0; play_note(t, ch, v->note, v->inst, v->vol);
    }
    if (v->fx == FX_VOLSL) {
      int up = (v->param >> 4) & 15, dn = v->param & 15;
      v->vol += up ? up : -dn;
      if (v->vol < 0) v->vol = 0; if (v->vol > 255) v->vol = 255;
      chan_vol(t, ch);
    }
    if (v->fx == FX_PARAM && v->param < GFXA_SFXR_PARAM_COUNT) {
      int pi = v->param;
      t->s.inst[v->inst].p[pi] += 0.01f;
      if (t->s.inst[v->inst].p[pi] > 1.0f) t->s.inst[v->inst].p[pi] = 0.0f;
    }
    if (p < 8.0f) p = 8.0f; v->slide_period = p;
    if (send) { tr_acmd a = {0}; a.vol = -1; a.period = p; a.vib_spd = vib_spd; a.vib_dep = vib_dep; post(t, ch, &a); }
  }
}

static int tick_ms(tracker *t) { return 2500 / (t->s.bpm > 0 ? t->s.bpm : 125); }

static void advance_tick(tracker *t) {
  long n = now_ms();
  if (!t->playing || n < t->next_ms) return;
  t->next_ms = n + tick_ms(t);
  if (t->tick == 0) row_event(t); else tick_effects(t);
  if (++t->tick >= t->s.ticks) {
    t->tick = 0; t->play_row++;
    if (t->play_row >= TR_ROWS) {
      t->play_row = 0; t->play_ord = (t->play_ord + 1) % t->s.order_len;
      t->cur_pat = t->s.order[t->play_ord];
    }
  }
  if (t->follow) { t->row = t->play_row; t->cur_ord = t->play_ord; t->cur_pat = t->s.order[t->cur_ord]; }
  t->dirty = 1;
}

static void start_play(tracker *t, int from_cursor) {
  t->playing = 1; t->tick = 0; t->next_ms = 0;
  t->play_ord = from_cursor ? t->cur_ord : 0;
  t->play_row = from_cursor ? t->row : 0;
}

static void stop_play(tracker *t) {
  t->playing = 0; t->tick = 0; t->play_row = 0; t->play_ord = 0;
  for (int c = 0; c < TR_CH; c++) cut_note(t, c);
}

static void cell_text(tr_cell *c, char *out) {
  char n[4]; note_name(c->note, n);
  sprintf(out, "%s %02X %02X %02X %02X", n,
    c->inst == EMPTY ? 0 : c->inst, c->vol == EMPTY ? 0 : c->vol,
    c->fx == EMPTY ? 0 : c->fx, c->param == EMPTY ? 0 : c->param);
  if (c->inst == EMPTY) memcpy(out + 4, "..", 2);
  if (c->vol == EMPTY) memcpy(out + 7, "..", 2);
  if (c->fx == EMPTY) memcpy(out + 10, "..", 2);
  if (c->param == EMPTY) memcpy(out + 13, "..", 2);
}

static void draw_pattern(tracker *t) {
  int w, h; gfxt_get_size(&w, &h);
  if (w < 24 || h < 10) { gfxt_clear(); gfxt_printf("tracker: terminal too small (%dx%d)\n", w, h); return; }
  int rows = h - 4;
  if (t->row < t->top) t->top = t->row;
  if (t->row >= t->top + rows) t->top = t->row - rows + 1;
  gfxt_clear();
  gfxt_printf("TRK p%02X o%02X i%02X oct%d %s F%d M%X S%X\n",
    t->cur_pat, t->cur_ord, t->cur_inst, t->octave,
    t->playing ? "PLAY" : "STOP", t->follow, t->mute, t->solo);
  if (w >= 64) gfxt_printf("     CH0              CH1              CH2              CH3\n");
  else gfxt_printf("     CH%d\n", t->chan);
  for (int rr = 0; rr < rows && t->top + rr < TR_ROWS; rr++) {
    int r = t->top + rr;
    int play = t->playing && t->cur_pat == t->s.order[t->play_ord] && r == t->play_row;
    gfxt_printf("%c%02X%c ", r == t->row ? '>' : ' ', r, play ? '*' : ' ');
    int c0 = w >= 64 ? 0 : t->chan, c1 = w >= 64 ? TR_CH : t->chan + 1;
    for (int ch = c0; ch < c1; ch++) {
      char b[20]; cell_text(&t->s.pat[t->cur_pat].row[r].c[ch], b);
      gfxt_printf("%c%-15s", (r == t->row && ch == t->chan) ? '[' : ' ', b);
    }
    gfxt_printf("\n");
  }
  gfxt_printf("Arrows Tab/i Space Enter m/M mute/solo o/O oct f follow u undo w/l save/load %s\n", t->path);
}

static void draw_order(tracker *t) {
  gfxt_clear();
  gfxt_printf("ORDER/TIME  bpm %d  ticks/row %d  row_ms %d\n\n", t->s.bpm, t->s.ticks, tick_ms(t) * t->s.ticks);
  for (int i = 0; i < t->s.order_len; i++)
    gfxt_printf("%c%02X: pattern %02X%s\n", i == t->ord_cursor ? '>' : ' ', i, t->s.order[i],
      t->playing && i == t->play_ord ? "  PLAY" : "");
  gfxt_printf("\nUp/Dn select  Left/Right pattern  +/- bpm  </> ticks  a add  x delete  d dup  Tab pattern\n");
}

static void slider(const char *name, float v, int sel) {
  int n = (int)(v * 20.0f + 0.5f);
  if (n < 0) n = 0; if (n > 20) n = 20;
  gfxt_printf("%c%-11s [", sel ? '>' : ' ', name);
  for (int i = 0; i < 20; i++) gfxt_putchar(i < n ? '#' : '-');
  gfxt_printf("] %.2f\n", v);
}

static void draw_instr(tracker *t) {
  gfxt_clear();
  gfxt_printf("INSTR %02X  preview %s\n\n", t->cur_inst, "C-4");
  for (int i = 0; i < GFXA_SFXR_PARAM_COUNT; i++)
    slider(pnames[i], t->s.inst[t->cur_inst].p[i], i == t->inst_param);
  gfxt_printf("\nUp/Dn param  Left/Right edit  +/- big  n new  c copy  x clear  p preview  i pattern\n");
}

static void draw_help(tracker *t) {
  (void)t; gfxt_clear();
  gfxt_printf("TRACKER HELP\n\n");
  gfxt_printf("Pattern: zsxdcvgbhnjm/q2w3er5t6y7u notes, o/O octave, ^ cut, . clear.\n");
  gfxt_printf("FX: 01 up 02 down 03 vib 04 arp 05 retrig 06 sfxr 09 speed 0A vol 0B jump 0D break.\n");
  gfxt_printf("Arrows move, Ctrl-like keys vary by target; use Tab order, i instruments.\n");
  gfxt_printf("c/v/x cell, a/d row, [] cell transpose, {}/} pattern transpose, m/M mute/solo.\n");
  gfxt_printf("f follow, u undo, w save, l load, P clone pattern, C clear pattern.\n");
  gfxt_printf("Space play/pause, Enter play from row, s stop, q quit, ? back.\n");
}

static void draw(tracker *t) {
  if (t->mode == M_PATTERN) draw_pattern(t);
  else if (t->mode == M_ORDER) draw_order(t);
  else if (t->mode == M_INSTR) draw_instr(t);
  else draw_help(t);
  t->dirty = 0;
}

static void edit_hex(tracker *t, int v) {
  tr_cell *c = &t->s.pat[t->cur_pat].row[t->row].c[t->chan];
  signed char *f[] = {&c->note, &c->inst, &c->vol, &c->fx, &c->param};
  if (t->field == F_NOTE) return;
  int old = *f[t->field] == EMPTY ? 0 : *f[t->field];
  old = ((old << 4) | v) & 0xff;
  if (t->field == F_INST) old %= TR_INS;
  if (t->field == F_FX) old &= 15;
  *f[t->field] = old;
}

static void transpose_cell(tr_cell *c, int d) {
  if (c->note >= 0) {
    c->note += d;
    if (c->note < 0) c->note = 0;
    if (c->note > 119) c->note = 119;
  }
}

static void transpose_pattern(tracker *t, int d) {
  tr_pattern *p = &t->s.pat[t->cur_pat];
  for (int r = 0; r < TR_ROWS; r++)
    for (int c = 0; c < TR_CH; c++) transpose_cell(&p->row[r].c[c], d);
}

static void clear_pattern(tracker *t) {
  for (int r = 0; r < TR_ROWS; r++)
    for (int c = 0; c < TR_CH; c++) t->s.pat[t->cur_pat].row[r].c[c] = empty_cell();
}

static void clone_pattern(tracker *t) {
  int np = (t->cur_pat + 1) % TR_PAT;
  t->s.pat[np] = t->s.pat[t->cur_pat];
  t->cur_pat = np;
  t->s.order[t->cur_ord] = np;
}

static void ins_row(tracker *t) {
  tr_pattern *p = &t->s.pat[t->cur_pat];
  for (int r = TR_ROWS - 1; r > t->row; r--) p->row[r] = p->row[r - 1];
  for (int c = 0; c < TR_CH; c++) p->row[t->row].c[c] = empty_cell();
}

static void del_row(tracker *t) {
  tr_pattern *p = &t->s.pat[t->cur_pat];
  for (int r = t->row; r < TR_ROWS - 1; r++) p->row[r] = p->row[r + 1];
  for (int c = 0; c < TR_CH; c++) p->row[TR_ROWS - 1].c[c] = empty_cell();
}

static void key_pattern(tracker *t, int k) {
  tr_cell *c = &t->s.pat[t->cur_pat].row[t->row].c[t->chan];
  int n = key_note(t, k), hv = hexv(k);
  if (k == EVT_KEY_UP && t->row > 0) t->row--;
  else if (k == EVT_KEY_DOWN && t->row < TR_ROWS - 1) t->row++;
  else if (k == EVT_KEY_LEFT) { if (t->field > 0) t->field--; else if (t->chan > 0) { t->chan--; t->field = F_PARAM; } }
  else if (k == EVT_KEY_RIGHT) { if (t->field < F_PARAM) t->field++; else if (t->chan < TR_CH - 1) { t->chan++; t->field = 0; } }
  else if (k == '\t') t->mode = M_ORDER;
  else if (k == 'i' || k == EVT_KEY_CTRLTAB) t->mode = M_INSTR;
  else if (k == ' ') t->playing ? stop_play(t) : start_play(t, 0);
  else if (k == '\n') start_play(t, 1);
  else if (k == 's') stop_play(t);
  else if (k == '?') { t->help_prev = t->mode; t->mode = M_HELP; }
  else if (k == 'o' && t->octave > 0) t->octave--;
  else if (k == 'O' && t->octave < 7) t->octave++;
  else if (k == 'f') t->follow = !t->follow;
  else if (k == 'm') { t->mute ^= 1 << t->chan; chan_vol(t, t->chan); }
  else if (k == 'M') { t->solo ^= 1 << t->chan; for (int ch = 0; ch < TR_CH; ch++) chan_vol(t, ch); }
  else if (k == 'u' && t->has_undo) { tr_song tmp = t->s; t->s = t->undo; t->undo = tmp; }
  else if (k == 'w') song_save(&t->s, t->path);
  else if (k == 'l') { undo_snap(t); song_load(&t->s, t->path); }
  else if (k == '^') { undo_snap(t); c->note = CUT; }
  else if (k == '.') { undo_snap(t); signed char *f[] = {&c->note, &c->inst, &c->vol, &c->fx, &c->param}; *f[t->field] = EMPTY; }
  else if (k == 'c') t->clip = *c;
  else if (k == 'v') { undo_snap(t); *c = t->clip; }
  else if (k == 'x') { undo_snap(t); t->clip = *c; *c = empty_cell(); }
  else if (k == 'a') { undo_snap(t); ins_row(t); }
  else if (k == 'd') { undo_snap(t); del_row(t); }
  else if (k == '[') { undo_snap(t); transpose_cell(c, -1); }
  else if (k == ']') { undo_snap(t); transpose_cell(c, 1); }
  else if (k == '{') { undo_snap(t); transpose_pattern(t, -1); }
  else if (k == '}') { undo_snap(t); transpose_pattern(t, 1); }
  else if (k == 'P') { undo_snap(t); clone_pattern(t); }
  else if (k == 'C') { undo_snap(t); clear_pattern(t); }
  else if (n != EMPTY) { undo_snap(t); c->note = n; c->inst = t->cur_inst; play_note(t, t->chan, n, t->cur_inst, c->vol); if (t->row < TR_ROWS - 1) t->row++; }
  else if (hv >= 0) { undo_snap(t); edit_hex(t, hv); }
}

static void key_order(tracker *t, int k) {
  if (k == EVT_KEY_CTRLTAB) t->mode = M_INSTR;
  else if (k == '\t') t->mode = M_PATTERN;
  else if (k == EVT_KEY_UP && t->ord_cursor > 0) t->ord_cursor--;
  else if (k == EVT_KEY_DOWN && t->ord_cursor < t->s.order_len - 1) t->ord_cursor++;
  else if (k == EVT_KEY_LEFT && t->s.order[t->ord_cursor] > 0) { undo_snap(t); t->s.order[t->ord_cursor]--; }
  else if (k == EVT_KEY_RIGHT && t->s.order[t->ord_cursor] < TR_PAT - 1) { undo_snap(t); t->s.order[t->ord_cursor]++; }
  else if (k == '+' && t->s.bpm < 255) { undo_snap(t); t->s.bpm++; }
  else if (k == '-' && t->s.bpm > 32) { undo_snap(t); t->s.bpm--; }
  else if (k == '>' && t->s.ticks < 16) { undo_snap(t); t->s.ticks++; }
  else if (k == '<' && t->s.ticks > 1) { undo_snap(t); t->s.ticks--; }
  else if (k == 'u' && t->has_undo) { tr_song tmp = t->s; t->s = t->undo; t->undo = tmp; }
  else if (k == 'w') song_save(&t->s, t->path);
  else if (k == 'l') { undo_snap(t); song_load(&t->s, t->path); }
  else if (k == 'a' && t->s.order_len < TR_ORD) { undo_snap(t); memmove(&t->s.order[t->ord_cursor+2], &t->s.order[t->ord_cursor+1], (t->s.order_len-t->ord_cursor-1)*sizeof(int)); t->s.order[t->ord_cursor+1] = t->s.order[t->ord_cursor]; t->s.order_len++; }
  else if (k == 'x' && t->s.order_len > 1) { undo_snap(t); memmove(&t->s.order[t->ord_cursor], &t->s.order[t->ord_cursor+1], (t->s.order_len-t->ord_cursor-1)*sizeof(int)); t->s.order_len--; if (t->ord_cursor >= t->s.order_len) t->ord_cursor = t->s.order_len - 1; }
  else if (k == 'd') { int np = t->s.order[t->ord_cursor] + 1; if (np < TR_PAT) { undo_snap(t); t->s.pat[np] = t->s.pat[t->s.order[t->ord_cursor]]; t->s.order[t->ord_cursor] = np; } }
  t->cur_ord = t->ord_cursor; t->cur_pat = t->s.order[t->cur_ord];
}

static void key_instr(tracker *t, int k) {
  float *p = t->s.inst[t->cur_inst].p, step = (k == '+' || k == '-') ? 0.05f : 0.01f;
  if (k == 'i' || k == '\t' || k == EVT_KEY_CTRLTAB) t->mode = M_PATTERN;
  else if (k == EVT_KEY_UP && t->inst_param > 0) t->inst_param--;
  else if (k == EVT_KEY_DOWN && t->inst_param < GFXA_SFXR_PARAM_COUNT - 1) t->inst_param++;
  else if ((k == EVT_KEY_LEFT || k == '-') && p[t->inst_param] > 0.0f) { undo_snap(t); p[t->inst_param] -= step; }
  else if ((k == EVT_KEY_RIGHT || k == '+') && p[t->inst_param] < 1.0f) { undo_snap(t); p[t->inst_param] += step; }
  else if (k == 'n') { undo_snap(t); t->cur_inst = (t->cur_inst + 1) % TR_INS; gfxa_sfxr_defaults(t->s.inst[t->cur_inst].p); }
  else if (k == 'c') { undo_snap(t); int ni = (t->cur_inst + 1) % TR_INS; t->s.inst[ni] = t->s.inst[t->cur_inst]; t->cur_inst = ni; }
  else if (k == 'x') { undo_snap(t); gfxa_sfxr_defaults(p); }
  else if (k == 'u' && t->has_undo) { tr_song tmp = t->s; t->s = t->undo; t->undo = tmp; }
  else if (k == 'w') song_save(&t->s, t->path);
  else if (k == 'l') { undo_snap(t); song_load(&t->s, t->path); }
  else if (k == 'p') play_note(t, 0, t->preview_note, t->cur_inst, 128);
  if (p[t->inst_param] < 0.0f) p[t->inst_param] = 0.0f;
  if (p[t->inst_param] > 1.0f) p[t->inst_param] = 1.0f;
}

static int cmd_tracker(const char *args) {
  tracker t; memset(&t, 0, sizeof(t)); song_init(&t.s);
  t.cur_inst = 0; t.preview_note = 60; t.clip = empty_cell(); t.follow = 1; t.octave = 4;
  snprintf(t.path, sizeof(t.path), "%s", (args && args[0]) ? args : "tracker.trk");
  song_load(&t.s, t.path);
  for (int c = 0; c < TR_CH; c++) {
    t.v[c].note = EMPTY; t.v[c].inst = EMPTY; t.v[c].vol = 128;
    t.cmd[c].vol = -1; t.cmd[c].vib_spd = -1.0f;
    gfxa_set_ctrl(c, audio_ctrl, &t.cmd[c]);
  }
  t.dirty = 1;
  while (!t.quit) {
    advance_tick(&t);
    if (t.dirty) draw(&t);
    int k = gfxt_getchar_nb();
    if (k) {
      gfxt_stdin = 0;
      if (k == 'q' && t.mode != M_HELP) t.quit = 1;
      else if (t.mode == M_HELP) t.mode = t.help_prev;
      else if (t.mode == M_PATTERN) key_pattern(&t, k);
      else if (t.mode == M_ORDER) key_order(&t, k);
      else if (t.mode == M_INSTR) key_instr(&t, k);
      t.dirty = 1;
    }
    if (&gfx_headless_stdin_eof && gfx_headless_stdin_eof && !gfxt_stdin) t.quit = 1;
    if (gfx_yeld) gfx_yeld(); else gfx_delay(1);
  }
  stop_play(&t);
  for (int c = 0; c < TR_CH; c++) gfxa_set_ctrl(c, NULL, NULL);
  gfxt_clear();
  return 0;
}

void gfxt_tracker_cmd_reg(void) {
  gfxt_register_cmd("tracker", "interactive SFXR tracker", cmd_tracker);
}
