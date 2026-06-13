#include "rtttl.h"
#include "simplegfx.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static void (*_rtttl_cb)(bool) = NULL;

void gfxa_rtttl_set_callback(void (*cb)(bool)) {
  _rtttl_cb = cb;
}

static int note_index(char c) {
  switch ((char)tolower((unsigned char)c)) {
    case 'c': return 0;
    case 'd': return 2;
    case 'e': return 4;
    case 'f': return 5;
    case 'g': return 7;
    case 'a': return 9;
    case 'b': return 11;
    default: return -1;
  }
}

static int note_freq(int octave, int note) {
  int n = (octave + 1) * 12 + note - 69;
  int k = n / 12;
  int r = n % 12;
  if (r < 0) { r += 12; k -= 1; }
  uint32_t ratio = 65536;
  for (int i = 0; i < r; i++)
    ratio = (uint32_t)(((uint64_t)ratio * 69432) >> 16);
  int64_t freq = (int64_t)440 * ratio;
  if (k >= 0) freq <<= k;
  else freq >>= -k;
  return (int)((freq + 32768) >> 16);
}

static const char *skip_number(const char *p, int *value) {
  int n = 0;
  bool any = false;
  while (*p >= '0' && *p <= '9') {
    n = n * 10 + (*p - '0');
    ++p;
    any = true;
  }
  if (any) *value = n;
  return p;
}

int gfxa_rtttl_play(const char *rtttl, void (*cb)(bool)) {
  if (!rtttl || !rtttl[0]) {
    return 1;
  }
  const char *p = strchr(rtttl, ':');
  if (!p) {
    return 1;
  }
  ++p;
  int def_dur = 4;
  int def_oct = 6;
  int bpm = 63;
  const char *notes = strchr(p, ':');
  if (notes) {
    char header[96];
    size_t len = (size_t)(notes - p);
    if (len >= sizeof(header)) len = sizeof(header) - 1;
    memcpy(header, p, len);
    header[len] = '\0';
    char *tok = strtok(header, ",");
    while (tok) {
      if (tok[0] == 'd' && tok[1] == '=') def_dur = atoi(tok + 2);
      if (tok[0] == 'o' && tok[1] == '=') def_oct = atoi(tok + 2);
      if (tok[0] == 'b' && tok[1] == '=') bpm = atoi(tok + 2);
      tok = strtok(NULL, ",");
    }
    p = notes + 1;
  }
  int whole = (60 * 1000 / (bpm > 0 ? bpm : 63)) * 4;
  const char *start = p;
  while (*p && (p - start) < 512) {
    while ((*p == ' ' || *p == ',') && (p - start) < 512) ++p;
    if (!*p) break;
    int duration = def_dur;
    int octave = def_oct;
    p = skip_number(p, &duration);
    char c = *p ? *p++ : 'p';
    bool sharp = false;
    bool dotted = false;
    if (*p == '#') {
      sharp = true;
      ++p;
    }
    if (*p == '.') {
      dotted = true;
      ++p;
    }
    if (*p >= '0' && *p <= '9') octave = *p++ - '0';
    if (*p == '.') {
      dotted = true;
      ++p;
    }
    int ms = whole / (duration > 0 ? duration : def_dur);
    if (dotted) ms += ms / 2;
    int ni = note_index(c);
    if (ni < 0) {
      if (cb) cb(false);
      if (_rtttl_cb) _rtttl_cb(false);
      gfx_beep(0, ms);
    } else {
      if (sharp) ++ni;
      if (cb) cb(true);
      if (_rtttl_cb) _rtttl_cb(true);
      gfx_beep((int)note_freq(octave, ni), ms);
    }
    if (*p == ',') ++p;
  }
  if (cb) cb(false);
  if (_rtttl_cb) _rtttl_cb(false);
  if ((p - start) >= 512) {
    return 1;
  }
  return 0;
}
