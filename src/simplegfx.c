#include "simplegfx.h"

static font_t * _font = NULL;
static unsigned int seed = 0;

uint32_t elm = 0;
char * printf_buf = NULL;
int printf_size = 0;
int printf_len = 0;
int full_kb = 0;

int main(int argv, char** args) {
    if (gfx_setup() != 0) {
        return 1;
    }
    gfx_set_font(&font5x7);
    gfx_run();
    gfx_app(0);
    gfx_cleanup();
    return 0;
}

void gfx_set_font(font_t * font) {
    _font = font;
    if (_font == NULL) {
        _font = &font5x7;
    }
    if (_font->count == 0) {
        uint8_t * data = _font->data;
        for (int i = 0; i < 512 * _font->width; i++) {
            if (data[i] == 1 && data[i + 1] == 2
                && data[i + 2] == 3 && data[i + 3] == 4
                && data[i + 4] == 5) {
                _font->count = i / _font->width;
                break;
            }
        }
    }
}

font_t * gfx_get_font(void) {
    return _font;
}

int gfx_text(const char * text, int x, int y, int size) {
    if (text == NULL || _font == NULL) {
        return y;
    }
    int cx = x;
    int cy = y;
    font_t f = *_font;
    int fheight = f.height;
    int fwidth = f.width;
    cy += fheight * size;
    int len = (int)strlen(text);
    for (int i = 0; i < len; i++) {
        uint8_t C = (uint8_t)text[i];
        if (C == '\r' && text[i + 1] == '\n') {
            i++;
            cx = x;
            cy += fheight * size + size;
            if (cy > WINDOW_HEIGHT) {
                cy = y;
            }
            continue;
        }
        for (int c = 0; c < fwidth; c++) {
            for (uint8_t l = 0; l < fheight; l++) {
                uint8_t mask = 1 << (fheight - l - 1);
                if (f.data[C * fwidth + c] & mask) {
                    if (size == 1) {
                        gfx_point(cx + c, cy - l);
                    } else {
                        gfx_fill_rect(cx + c * size, cy - l * size, size, size);
                    }
                }
            }
        }
        cx += fwidth * size + size;
        if (cx > WINDOW_WIDTH) {
            cx = x;
            cy += fheight * size + size;
        }
    }
    cy += fheight * size + size;
    return cy;
}

int gfx_clear_text_buffer(void) {
    if (printf_buf != NULL) {
        free(printf_buf);
        printf_buf = NULL;
    }
    printf_size = 0;
    printf_len = 0;
    return 0;
}

int gfx_printf(const char * format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (len < 0) {
        return -1;
    }
    if (len + printf_len >= printf_size) {
        printf_size = (len + printf_len + 64) * 2;
        char * new_buf = (char *)realloc(printf_buf, printf_size);
        if (new_buf == NULL) {
            return -1;
        }
        printf_buf = new_buf;
    }
    va_start(args, format);
    vsnprintf(printf_buf + printf_len, printf_size - printf_len, format, args);
    va_end(args);
    printf_len += len;
    return len;
}

int gfx_font_table(int x, int y, int size) {
    if (_font == NULL) return y;
    font_t f = *_font;
    char t[2] = {0, 0};
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 32; j++) {
            t[0] = i * 32 + j;
            if ((uint8_t)t[0] >= f.count) break;
            gfx_text(t, x + j * (f.width * size + size),
                     y + i * (f.height * size + size), size);
        }
    }
    return y + 8 * (f.height * size + size);
}

int gfx_fast_rand(void) {
    if (seed == 0) seed = (unsigned int)time(NULL);
    seed = seed * 1103515245 + 12345;
    return (unsigned int)(seed / 65536) % 32768;
}