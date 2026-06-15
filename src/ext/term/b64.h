#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int base64_encode(const uint8_t *in, int in_len, char *out, int out_size);
int base64_decode(const char *in, uint8_t *out, int out_size);

#ifdef __cplusplus
}
#endif
