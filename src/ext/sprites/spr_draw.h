/* Sprite draw helper — renders a sprite frame from data.h arrays.
   Use together with data.h:

       #include "ext/sprites/data.h"
       #include "ext/sprites/spr_draw.h"

       spr_draw(spr_data, spr_idx, spr_offs, ch, act, x, y, frame, size, NULL);

   Format (see data.h header):
     data[]   = interleaved 0,0,0,colorId + x,y,w,h commands
     idx[]    = action offsets into data, sequential per character
     offs[]   = start index in idx per character (last entry = total)
     Each action header: [nf(1), fw(1), fh(1), ncol(1), size_lo(1), size_hi(1),
                          palette(R,G,B)×ncol, frame_off(u16)×nf, frame_data] */
#pragma once
#include <stdint.h>
#include "simplegfx.h"

static inline void spr_draw(const uint8_t *data, const uint32_t *idx,
                            const uint16_t *offs,
                            int ch, int act, int x, int y, int t,
                            int size, const uint8_t *pal) {
    int nacts = offs[ch + 1] - offs[ch];
    act %= nacts;
    uint32_t off = idx[offs[ch] + act];
    const uint8_t *p = data + off;
    int nf = p[0], ncol = p[3], asz = p[4] | (p[5] << 8);
    int hsz = 6 + ncol * 3 + nf * 2;
    const uint8_t *dpal = pal ? pal : p + 6;
    const uint8_t *foffs = p + 6 + ncol * 3;
    const uint8_t *fdat = p + hsz;
    int fi = t % nf;
    int fo = foffs[fi * 2] | (foffs[fi * 2 + 1] << 8);
    int fe = (fi + 1 < nf) ? (foffs[(fi + 1) * 2] | (foffs[(fi + 1) * 2 + 1] << 8)) : (asz - hsz);
    const uint8_t *fp = fdat + fo, *end = fdat + fe;
    while (fp < end) {
        if (fp[0] == 0 && fp[1] == 0 && fp[2] == 0) {
            int ci = fp[3];
            gfx_set_color(dpal[ci * 3], dpal[ci * 3 + 1], dpal[ci * 3 + 2]);
            fp += 4;
        } else {
            gfx_fill_rect(x + fp[0] * size, y + fp[1] * size,
                          fp[2] * size, fp[3] * size);
            fp += 4;
        }
    }
}
