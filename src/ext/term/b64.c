#include "b64.h"
#include <string.h>
#include <sys/stat.h>

const char b64_alphabet[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_chr(char c) {
  const char *p = strchr(b64_alphabet, c);
  return p ? (int)(p - b64_alphabet) : -1;
}

int base64_encode(const uint8_t *in, int in_len, char *out, int out_size) {
  int out_len = ((in_len + 2) / 3) * 4;
  if (out_size < out_len + 1) return 0;
  for (int i = 0; i < in_len; i += 3) {
    unsigned v = ((unsigned)in[i] << 16);
    if (i + 1 < in_len) v |= (unsigned)in[i+1] << 8;
    if (i + 2 < in_len) v |= in[i+2];
    int n = in_len - i;
    out[0] = b64_alphabet[(v >> 18) & 0x3F];
    out[1] = n > 1 ? b64_alphabet[(v >> 12) & 0x3F] : '=';
    out[2] = n > 2 ? b64_alphabet[(v >>  6) & 0x3F] : '=';
    out[3] = b64_alphabet[ v        & 0x3F];
    out += 4;
  }
  *out = '\0';
  return out_len;
}

int base64_decode(const char *in, uint8_t *out, int out_size) {
  int oi = 0;
  while (*in) {
    int idx[4], n = 0;
    for (int k = 0; k < 4; k++) {
      while (*in == ' ') in++;
      if (*in == '=' || !*in) { in++; continue; }
      int v = b64_chr(*in++);
      if (v < 0) return -1;
      idx[n++] = v;
    }
    if (n < 2) break;
    if (oi >= out_size) break;
    out[oi++] = (uint8_t)((idx[0] << 2) | (idx[1] >> 4));
    if (n >= 3 && oi < out_size) out[oi++] = (uint8_t)((idx[1] << 4) | (idx[2] >> 2));
    if (n >= 4 && oi < out_size) out[oi++] = (uint8_t)((idx[2] << 6) | idx[3]);
  }
  return oi;
}
