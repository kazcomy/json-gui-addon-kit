/**
 * @file gfx_shared.h
 * @brief Shared 128-byte tile buffer helpers used by the SSD1306 driver.
 */
#ifndef GFX_SHARED_H
#define GFX_SHARED_H

#include <stdint.h>

#include "gfx_font.h"

/** Get pointer to the shared 128-byte tile buffer. */
uint8_t* gfx_get_shared_buffer(void);
/** Zero the contents of the shared tile buffer. */
void gfx_clear_shared_buffer(void);

#endif /* GFX_SHARED_H */
