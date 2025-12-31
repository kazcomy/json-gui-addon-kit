/**
 * @file gfx_shared.c
 * @brief Implementation of shared 128-byte tile buffer helpers.
 */
#include <stdint.h>
#include <string.h>

static uint8_t gfx_shared_buffer[128];

uint8_t* gfx_get_shared_buffer(void)
{
  return gfx_shared_buffer;
}

void gfx_clear_shared_buffer(void)
{
  memset(gfx_shared_buffer, 0, sizeof(gfx_shared_buffer));
}
