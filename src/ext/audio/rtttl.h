#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int gfxa_rtttl_play(const char *rtttl, void (*cb)(bool));
void gfxa_rtttl_set_callback(void (*cb)(bool));

#ifdef __cplusplus
}
#endif
