/* Minimal COBS implementation
 * - No dynamic memory
 * - Returns 0 on error
 */
#include "cobs.h"

#include <stdbool.h>

size_t cobs_encode(const uint8_t* in, size_t len, uint8_t* out, size_t out_max)
{
  if (!out || (!in && len)) {
    return 0;
  }
  /* Worst case overhead: +1 code byte per 254 bytes + 1 final code. For small len, out_max >= len+1
   * is safe. */
  size_t  read_index = 0, write_index = 1; /* reserve first code byte */
  size_t  code_index = 0;                  /* where to write code */
  uint8_t code       = 1;
  if (out_max == 0) {
    return 0;
  }
  /* Place holder for first code */
  code_index = 0;
  if (out_max < 1) {
    return 0;
  }
  out[code_index] = 0; /* filled later */

  while (read_index < len) {
    if (in[read_index] == 0) {
      /* write code and start a new block */
      out[code_index] = code;
      code            = 1;
      code_index      = write_index++;
      if (write_index > out_max) {
        return 0;
      }
      out[code_index] = 0; /* placeholder */
      read_index++;
    } else {
      if (write_index >= out_max) {
        return 0;
      }
      out[write_index++] = in[read_index++];
      code++;
      if (code == 0xFF) {
        /* block full */
        out[code_index] = 0xFF;
        code            = 1;
        code_index      = write_index++;
        if (write_index > out_max) {
          return 0;
        }
        out[code_index] = 0; /* placeholder for next code */
      }
    }
  }
  /* write last code */
  out[code_index] = code;
  return write_index;
}

size_t cobs_decode(const uint8_t* in, size_t len, uint8_t* out, size_t out_max)
{
  if (!out || !in) {
    return 0;
  }
  size_t read_index = 0, write_index = 0;
  while (read_index < len) {
    uint8_t code = in[read_index++];
    if (code == 0) {
      return 0; /* invalid */
    }
    /* copy (code-1) bytes */
    for (uint8_t i = 1; i < code; i++) {
      if (read_index >= len) {
        return 0; /* truncated */
      }
      if (write_index >= out_max) {
        return 0;
      }
      out[write_index++] = in[read_index++];
    }
    /* if not at end of packet, insert an implicit zero (unless block length was max 0xFF) */
    if (read_index < len && code != 0xFF) {
      if (write_index >= out_max) {
        return 0;
      }
      out[write_index++] = 0;
    }
  }
  return write_index;
}
