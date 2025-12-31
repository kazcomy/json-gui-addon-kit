/**
 * @file ssd1306_driver.h
 * @brief Minimal-memory SSD1306 driver (128x32/64) with 128-byte shared tile
 * buffer.
 */
#ifndef SSD1306_DRIVER_H
#define SSD1306_DRIVER_H

#include <stdint.h>

#include "ch32fun.h"
#include "gfx_shared.h"
#include "i2c_custom.h"

/** I2C address for SSD1306. */
#define SSD1306_I2C_ADDR 0x3C
/** Display width in pixels. */
#define SSD1306_WIDTH 128
/** Display height in pixels (default at boot). Runtime is configurable. */
#define SSD1306_HEIGHT 32
/** Number of 8-pixel-tall pages (default at boot). Runtime is configurable. */
#define SSD1306_PAGES 4
/** Height of a page (in pixels). */
#define SSD1306_PAGE_HEIGHT 8
/* Full-frame size varies with height (pages). Query via ssd1306_framebuffer_size(). */

/** Basic color constants. */
#define BLACK 0
#define WHITE 1

// Common command values (subset)
#define SSD1306_CMD_DISPLAY_OFF 0xAE
#define SSD1306_CMD_DISPLAY_ON 0xAF
#define SSD1306_CMD_SET_DISPLAY_CLOCK_DIV 0xD5
#define SSD1306_CMD_SET_MULTIPLEX 0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_CMD_SET_START_LINE_0 0x40
#define SSD1306_CMD_CHARGE_PUMP 0x8D
#define SSD1306_CMD_MEMORY_MODE 0x20
#define SSD1306_CMD_SEG_REMAP_127_0 0xA1
#define SSD1306_CMD_COM_SCAN_DEC 0xC8
#define SSD1306_CMD_SET_COMPINS 0xDA
#define SSD1306_CMD_SET_CONTRAST 0x81
#define SSD1306_CMD_SET_PRECHARGE 0xD9
#define SSD1306_CMD_SET_VCOM_DETECT 0xDB
#define SSD1306_CMD_DISPLAY_ALL_ON_RESUME 0xA4
#define SSD1306_CMD_NORMAL_DISPLAY 0xA6
#define SSD1306_CMD_DEACTIVATE_SCROLL 0x2E
#define SSD1306_CMD_ACTIVATE_SCROLL 0x2F
#define SSD1306_CMD_HORIZ_SCROLL_RIGHT 0x26
#define SSD1306_CMD_HORIZ_SCROLL_LEFT 0x27
#define SSD1306_CMD_SET_COL_ADDR 0x21
#define SSD1306_CMD_SET_PAGE_ADDR 0x22

/** Scroll direction for SSD1306 hardware horizontal scroll. */
typedef enum {
  SSD1306_SCROLL_RIGHT = SSD1306_CMD_HORIZ_SCROLL_RIGHT,
  SSD1306_SCROLL_LEFT  = SSD1306_CMD_HORIZ_SCROLL_LEFT
} ssd1306_scroll_dir_t;

/** Scroll speed codes for SSD1306 hardware scroll (per datasheet). */
typedef enum {
  SSD1306_SCROLL_5_FRAMES   = 0x00,
  SSD1306_SCROLL_64_FRAMES  = 0x01,
  SSD1306_SCROLL_128_FRAMES = 0x02,
  SSD1306_SCROLL_256_FRAMES = 0x03,
  SSD1306_SCROLL_3_FRAMES   = 0x04,
  SSD1306_SCROLL_4_FRAMES   = 0x05,
  SSD1306_SCROLL_25_FRAMES  = 0x06,
  SSD1306_SCROLL_2_FRAMES   = 0x07
} ssd1306_scroll_speed_t;

/** Initialize driver. Returns 0 on success. */
int ssd1306_init(void);
/** Clear the entire display to black. */
void ssd1306_clear(void);

/** Set/clear a pixel within the current 8px-high tile buffer row. */
void ssd1306_tile_pixel(uint8_t x, uint8_t y, uint8_t color);
/** Render text into the current tile buffer with vertical offset (-7..+7). */
void ssd1306_tile_text(uint8_t x, int8_t y_offset, const char* text);

/** Send one-byte command. */
int ssd1306_command(uint8_t cmd);
/** Send a data span using I2C in chunks (internal helper). */
int ssd1306_send_data_bulk(const uint8_t* data, int len);
/** Write a full 128-byte page to the display. */
int ssd1306_write_page(uint8_t page, const uint8_t* data);

/** \brief Begin asynchronous full-frame (tile-based) render.
 * Starts sending all pages using internal ping-pong DMA without blocking.
 * If a transfer is already in progress returns -1.
 * The supplied callback is the same form as ssd1306_render_tiles.
 */
int ssd1306_render_async_begin(void (*render_callback)(uint8_t tile_y));
/** \brief Progress asynchronous transfer state machine.
 * Call frequently in main loop to feed next chunks when DMA becomes idle. */
void ssd1306_render_async_process(void);
/** \brief Query if asynchronous transfer active (1=active,0=idle). */
int ssd1306_render_async_busy(void);
/** \brief Request another full render after current completes.
 * Semantics:
 * - If NO frame is active: does NOT start one (just sets a flag? NO) - caller should
 *   normally use start_or_request() instead. This function is intended to be
 *   called ONLY when you already know a frame is in progress and you want a
 *   single follow-up frame with the *latest* state.
 * - Multiple calls while a frame is active DO NOT queue multiple frames;
 *   a single rerender_pending flag is set (coalescing). When the active frame
 *   finishes, exactly one new frame begins (if the flag is still set).
 * - Any intermediate state changes between now and frame completion are naturally
 *   folded into that next frame (you always get the freshest state, not stale snapshots).
 */
void ssd1306_render_async_request_rerender(void);
/** \brief Start async render if idle; else coalesce into one future rerender.
 * Behavior:
 * 1. If idle: immediately starts a new frame (callback will be used for each page).
 * 2. If busy: sets rerender flag (if not already set) and returns quickly.
 * 3. Return values: 0 = started now, 1 = queued (coalesced), <0 = error (e.g. NULL cb).
 * Rationale:
 *    Avoids race-prone code like: if(!busy){begin();} else {flag=1;}
 *    and prevents frame backlog explosion (always at most one extra pending frame).
 * Practical mental model:
 *    "Ensure at least one fresh frame after current one finishes".
 */
int ssd1306_render_async_start_or_request(void (*render_callback)(uint8_t tile_y));

/** \brief Query low-level I2C chunk transfer active.
 * This is a finer-grained status than ssd1306_render_async_busy(): it is
 * true while a DMA chunk is in flight even if the higher-level frame state
 * machine might be between stages. Normally UI code should use
 * ssd1306_render_async_busy(). Exposed mainly for timing-sensitive diagnostics
 * or future interrupt-driven integration. Returns 1 if a DMA transfer is
 * active, 0 if idle.
 */
int ssd1306_dma_xfer_active(void);
/** Return current async stage (ADDR/BUILD/STREAM_START/STREAMING). Idle returns 0xFF. */
uint8_t ssd1306_get_render_stage(void);

/** \brief Get current display height in pixels (32 or 64). */
uint8_t ssd1306_height(void);
/** \brief Get current number of 8px-tall pages (4 or 8). */
uint8_t ssd1306_pages(void);
/** \brief Set display height and re-apply geometry settings (32 or 64).
 *  Returns 0 on success, negative on invalid height.
 */
int ssd1306_set_height(uint8_t height);

#endif /* SSD1306_DRIVER_H */
