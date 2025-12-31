/**
 * @file font_5x8.h
 * @brief 5x8 Font Data for Basic Text Rendering.
 *
 * This module provides a complete 5x8 pixel font for ASCII characters 32-126.
 * The font is designed for small displays and provides good readability
 * while maintaining compact storage requirements.
 *
 * Font characteristics:
 * - Character size: 5x8 pixels
 * - Character spacing: 6 pixels (including 1-pixel gap)
 * - Supported range: ASCII 32 (space) to 126 (~)
 * - Storage: 5 bytes per character
 * - Format: LSB-first per column, top-to-bottom
 */
/* ============================================================================
 * 5x8 Font Data - Complete Implementation
 * ============================================================================
 */

#ifndef FONT_5X8_H
#define FONT_5X8_H
#include <stdint.h>

/**
 * @brief 5x8 font data array.
 */
extern const uint8_t font_5x8[][5];

#endif  // FONT_5X8_H
