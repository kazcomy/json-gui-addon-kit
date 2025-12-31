/* ============================================================================
 * GFX Font Configuration (canonical)
 * Centralized font selection and dimension macros for both minimal and gfx
 * paths.
 * ============================================================================
 */
#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "font_5x8.h"

#define GFX_FONT_DATA font_5x8
#define GFX_FONT_CHAR_WIDTH 5
#define GFX_FONT_CHAR_HEIGHT 8
#define GFX_FONT_CHAR_SPACING (GFX_FONT_CHAR_WIDTH + 1)
#define GFX_FONT_FIRST_CHAR 32
#define GFX_FONT_LAST_CHAR 130
#define GFX_FONT_CHAR_COUNT (GFX_FONT_LAST_CHAR - GFX_FONT_FIRST_CHAR + 1)

#endif /* GFX_FONT_H */
