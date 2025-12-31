/**
 * Minimal COBS encoder/decoder for small MCU use.
 * Frames are typically terminated on the wire with a single 0x00 delimiter.
 */
#ifndef COBS_H
#define COBS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns number of encoded bytes written, or 0 on error (e.g., out too small). */
size_t cobs_encode(const uint8_t* in, size_t len, uint8_t* out, size_t out_max);
/* Returns number of decoded bytes written, or 0 on error (malformed or out too small). */
size_t cobs_decode(const uint8_t* in, size_t len, uint8_t* out, size_t out_max);

#ifdef __cplusplus
}
#endif

#endif /* COBS_H */
