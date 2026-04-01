#ifndef SIMPLEFONT_H
#define SIMPLEFONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSTOP 1,2,3,4,5

typedef struct {
  uint8_t width;
  uint8_t height;
  uint8_t * data;
  uint16_t count;
} font_t;

extern font_t font5x7;

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

#ifdef __cplusplus
}
#endif

#endif