/**
 * @file ssd1306_driver.c
 * @brief Implementation of minimal-memory SSD1306 driver (128x32/64) with
 *        asynchronous page rendering and non-blocking chunked I2C DMA transfer.
 */
/* =========================================================================
 * SSD1306 Driver Implementation
 * Memory-efficient 128x32 tile-based rendering using 128-byte shared buffer.
 * ========================================================================= */
#include "ssd1306_driver.h"

#include <stdio.h>
#include <string.h>

#include "debug_led.h"
#include "gfx_font.h"
#include "gfx_shared.h"
#include "status_codes.h"

/* Max raw data payload bytes per I2C DMA burst (excludes 1 control byte). */
#define I2C_BUFFER_LIMIT 28
/* Ping-pong buffers: each holds a control byte + payload. We double-buffer to build
 * the next chunk while the previous DMA is still draining. */
static uint8_t bulk_buffer[2][I2C_BUFFER_LIMIT + 2];
static uint8_t bulk_index = 0; /* index of buffer being filled next */

/** I2C device configuration for SSD1306 display. */
static i2c_device_t g_i2c_dev = {.clkr = 400000,
                                 .type = I2C_ADDR_7BIT,
                                 .addr = 0x3C,
                                 .regb = 1,
                                 .tout = 2000};
static uint8_t       g_height  = SSD1306_HEIGHT; /**< Current display height (32 or 64). */
static uint8_t       g_pages   = SSD1306_PAGES;  /**< Current number of pages (4 or 8). */

/* Forward declarations for helpers used before their definitions */
static void ssd1306_set_addr(uint8_t page_start, uint8_t page_end);
static int  ssd1306_commands(const uint8_t* cmds, int cmds_len);

/*
 * Burst command helper: send N command bytes in one I2C transaction:
 * [0x00][cmd0][cmd1]...[cmdN-1]
 * Uses DMA backend (i2c_write_raw_dma). Caller ensures cmds_len >0.
 */
/* ================= Non-blocking low level streaming transfer ================= */
typedef struct {
  uint8_t        active;    /**< 1 while a multi-chunk transfer is active */
  uint8_t        control;   /**< Control byte (0x00 commands / 0x40 data) */
  const uint8_t* bytes;     /**< Source pointer */
  int            total_len; /**< Total length remaining to send */
  int            sent;      /**< Bytes already queued (not counting in-flight) */
} ssd1306_dma_xfer_state_t;

static ssd1306_dma_xfer_state_t g_xfer;

/** Kick off a non-blocking I2C transfer using the DMA backend. */
static void ssd1306_dma_xfer_start(uint8_t control, const uint8_t* bytes, int len)
{
  /* If display frame async is active, we do NOT block: higher layer should avoid starting
     conflicting transfers. (Old code blocked here; now we rely on sequencing in caller.) */
  g_xfer.active    = 1U;
  g_xfer.control   = control;
  g_xfer.bytes     = bytes;
  g_xfer.total_len = len;
  g_xfer.sent      = 0;
}

/** \brief Progress the non-blocking transfer state machine.
 *  Call from main loop (and before starting new frame segments). */
static void ssd1306_dma_xfer_process(void)
{
  if (!g_xfer.active) {
    return;
  }
  /* If a DMA chunk still in flight, just return */
  if (i2c_tx_dma_busy()) {
    return;
  }
  if (g_xfer.sent >= g_xfer.total_len) {
    /* All data issued */
    g_xfer.active = 0U;
    return;
  }
  int     remaining = g_xfer.total_len - g_xfer.sent;
  int     chunk     = remaining > I2C_BUFFER_LIMIT ? I2C_BUFFER_LIMIT : remaining;
  uint8_t bi        = bulk_index ^ 1u; /* pick opposite buffer */
  /* Build next chunk */
  bulk_buffer[bi][0] = g_xfer.control;
  memcpy(&bulk_buffer[bi][1], &g_xfer.bytes[g_xfer.sent], (size_t) chunk);
  /* Launch DMA (assumes g_i2c_dev is valid after ssd1306_init) */
  int r = i2c_write_raw_dma(&g_i2c_dev, bulk_buffer[bi], (size_t) chunk + 1U);
  if (r != 0) {
    /* Abort on error */
    g_xfer.active = 0U;
    return;
  }
  bulk_index = bi;
  g_xfer.sent += chunk;
}

/** \brief Blocking helper built atop non-blocking streaming state.
 *  @param control 0x00 commands / 0x40 data
 *  @param bytes   Source pointer
 *  @param len     Number of bytes
 *  @return 0 on success, negative on invalid args
 */
static int ssd1306_dma_xfer_block(uint8_t control, const uint8_t* bytes, int len)
{
  if (!bytes || len <= 0) {
    return RES_BAD_LEN;
  }
  /* Ensure any prior streaming finished (defensive) */
  while (g_xfer.active) {
    ssd1306_dma_xfer_process();
  }
  ssd1306_dma_xfer_start(control, bytes, len);
  while (g_xfer.active) {
    ssd1306_dma_xfer_process();
  }
  return RES_OK;
}

/* ================= Asynchronous frame render (main-loop driven) ================ */
typedef struct {
  uint8_t active;             /* 1 while an async full-frame render in progress */
  uint8_t page;               /* current page (tile index) being sent */
  uint8_t stage;              /* multi-stage state (see enum) */
  void (*cb)(uint8_t tile_y); /* user render callback */
  uint8_t rerender_pending;   /* request to rerun another frame after finish */
} ssd1306_async_state_t;

static ssd1306_async_state_t g_async;

/* Async render stages */
enum {
  SSD1306_ASYNC_STAGE_ADDR         = 0, /* Need to set column/page address */
  SSD1306_ASYNC_STAGE_BUILD        = 1, /* Build shared buffer page via callback */
  SSD1306_ASYNC_STAGE_STREAM_START = 2, /* Start non-blocking transfer of page bytes */
  SSD1306_ASYNC_STAGE_STREAMING    = 3, /* Streaming in progress (chunks) */
};

/** Return non-zero while an async full-frame render is active. */
int ssd1306_render_async_busy(void)
{
  return g_async.active ? 1 : 0;
}
/** If active, request a single rerender after completion. */
void ssd1306_render_async_request_rerender(void)
{
  if (g_async.active) {
    g_async.rerender_pending = 1;
  }
}

/** Begin async rendering; returns error if already active. */
int ssd1306_render_async_begin(void (*render_callback)(uint8_t tile_y))
{
  if (g_async.active) {
    return RES_BAD_STATE; /* already active */
  }
  g_async.active           = 1;
  g_async.page             = 0;
  g_async.stage            = SSD1306_ASYNC_STAGE_ADDR;
  g_async.cb               = render_callback;
  g_async.rerender_pending = 0;
  debug_log_event(DEBUG_LED_EVT_RENDER_START,
                  (uint8_t) ((g_pages > 0U) ? (g_pages - 1U) : 0U));
  return RES_OK;
}

/** \brief Attempt to start async render or, if already active with same callback, request rerender.
 *  @param render_callback Callback used to build each page.
 *  @return 0 started, 1 queued rerender, negative on error.
 */
int ssd1306_render_async_start_or_request(void (*render_callback)(uint8_t tile_y))
{
  if (!ssd1306_render_async_busy()) {
    return ssd1306_render_async_begin(render_callback);
  }
  /* Busy: just flag rerender. (We don't compare callback pointer to save code size) */
  ssd1306_render_async_request_rerender();
  return 1;
}

/** Helper to start page streaming (page is reserved for future use). */
static void ssd1306_async_start_page_stream(uint8_t page)
{
  uint8_t* shared_buf = gfx_get_shared_buffer();
  /* Kick non-blocking streaming of 128 bytes */
  ssd1306_dma_xfer_start(0x40, shared_buf, SSD1306_WIDTH);
  (void) page; /* page not needed here but kept for potential future logic */
}

/** Advance async rendering state machine from the main loop. */
void ssd1306_render_async_process(void)
{
  if (!g_async.active) {
    return;
  }

  /* Always progress low-level transfer first (if any) */
  ssd1306_dma_xfer_process();

  switch (g_async.stage) {
    case SSD1306_ASYNC_STAGE_ADDR:
      if (i2c_tx_dma_busy()) {
        return; /* wait if something else sending */
      }
      debug_log_event(DEBUG_LED_EVT_RENDER_STAGE, (uint8_t) (g_async.page & 0x07u));
    ssd1306_set_addr(g_async.page, g_async.page);
      g_async.stage = SSD1306_ASYNC_STAGE_BUILD;
      break;
    case SSD1306_ASYNC_STAGE_BUILD:
      if (i2c_tx_dma_busy()) {
        return; /* defensive */
      }
      gfx_clear_shared_buffer();
      if (g_async.cb) {
        g_async.cb(g_async.page);
      }
      g_async.stage = SSD1306_ASYNC_STAGE_STREAM_START;
      break;
    case SSD1306_ASYNC_STAGE_STREAM_START:
      if (i2c_tx_dma_busy()) {
        return; /* ensure bus free */
      }
      ssd1306_async_start_page_stream(g_async.page);
      g_async.stage = SSD1306_ASYNC_STAGE_STREAMING;
      break;
    case SSD1306_ASYNC_STAGE_STREAMING:
      if (g_xfer.active) {
        /* Still streaming chunks */
        return;
      }
      /* Page transfer complete */
      g_async.page++;
      if (g_async.page >= g_pages) {
        debug_log_event(DEBUG_LED_EVT_RENDER_DONE,
                        g_async.rerender_pending ? 1u : 0u);
        g_async.active = 0;
        if (g_async.rerender_pending) {
          g_async.rerender_pending = 0;
          g_async.active           = 1;
          g_async.page             = 0;
          g_async.stage            = SSD1306_ASYNC_STAGE_ADDR;
          debug_log_event(DEBUG_LED_EVT_RENDER_START,
                          (uint8_t) ((g_pages > 0U) ? (g_pages - 1U) : 0U));
        }
        return;
      }
      g_async.stage = SSD1306_ASYNC_STAGE_ADDR;
      break;
    default:
      g_async.active = 0; /* invalid state fallback */
      break;
  }
}

/** Send a burst of command bytes using the DMA backend. */
static int ssd1306_commands(const uint8_t* cmds, int cmds_len)
{
  return ssd1306_dma_xfer_block(0x00, cmds, cmds_len);
}

/** Thin wrapper to send a single command byte. */
int ssd1306_command(uint8_t cmd)
{
  return ssd1306_dma_xfer_block(0x00, &cmd, 1);
}

/* bulk_buffer already defined above */

/** Send data bytes with the data control byte prefix. */
int ssd1306_send_data_bulk(const uint8_t* data_bytes, int count)
{
  return ssd1306_dma_xfer_block(0x40, data_bytes, count);
}
/* Set full column range and the given page range in one burst to shrink code size */
static void ssd1306_set_addr(uint8_t page_start, uint8_t page_end)
{
  uint8_t seq[6] = {SSD1306_CMD_SET_COL_ADDR,
                    0x00,
                    (uint8_t) (SSD1306_WIDTH - 1),
                    SSD1306_CMD_SET_PAGE_ADDR,
                    page_start,
                    page_end};
  (void) ssd1306_commands(seq, 6);
}

int ssd1306_init(void)
{
  i2c_init(&g_i2c_dev);

  /* Initialization sequence consolidated per SSD1306 datasheet */
  /* Order kept same as original discrete calls. */
  static const uint8_t init_seq[] = {SSD1306_CMD_DISPLAY_OFF,
                              SSD1306_CMD_SET_DISPLAY_CLOCK_DIV,
                              0x80,
                             // SSD1306_CMD_SET_MULTIPLEX,
                             // 0x1F, /* 32px default; ssd1306_set_height() can change later */
                            //  SSD1306_CMD_SET_DISPLAY_OFFSET,
                              0x00,
                            //  SSD1306_CMD_SET_START_LINE_0,
                              SSD1306_CMD_CHARGE_PUMP,
                              0x14,
                              SSD1306_CMD_MEMORY_MODE,
                              0x00, /* horizontal */
                              SSD1306_CMD_SEG_REMAP_127_0,
                              SSD1306_CMD_COM_SCAN_DEC,
                            //  SSD1306_CMD_SET_COMPINS,
                            //  0x02,
                              SSD1306_CMD_SET_CONTRAST,
                              0x8F,
                              SSD1306_CMD_SET_PRECHARGE,
                              0xF1,
                              SSD1306_CMD_SET_VCOM_DETECT,
                              0x40,
                              SSD1306_CMD_DISPLAY_ALL_ON_RESUME,
                              SSD1306_CMD_NORMAL_DISPLAY,
                              SSD1306_CMD_DEACTIVATE_SCROLL,
                              SSD1306_CMD_DISPLAY_ON};
  if (ssd1306_commands(init_seq, (int) sizeof(init_seq)) != 0) {
    return RES_INTERNAL;
  }
  return RES_OK;
}

/** Clear all pages to black. */
void ssd1306_clear(void)
{
  /* Zero a single 128-byte tile and write it to each page */
  gfx_clear_shared_buffer();
  for (uint8_t page = 0; page < g_pages; page++) {
    ssd1306_set_addr(page, page);
    if (ssd1306_send_data_bulk(gfx_get_shared_buffer(), SSD1306_WIDTH) != 0) {
      return;
    }
  }
}

void ssd1306_tile_pixel(uint8_t x, uint8_t y, uint8_t color)
{
  if (x >= SSD1306_WIDTH || y >= SSD1306_PAGE_HEIGHT) {
    return;
  }
  uint8_t* shared_buf = gfx_get_shared_buffer();
  if (color) {
    shared_buf[x] |= (uint8_t) (1U << y);
  } else {
    shared_buf[x] &= (uint8_t) ~(1U << y);
  }
}

void ssd1306_tile_text(uint8_t x, int8_t y_offset, const char* text)
{
  if (y_offset <= -(int8_t) SSD1306_PAGE_HEIGHT ||
      y_offset >= (int8_t) SSD1306_PAGE_HEIGHT) {
    return;
  }
  uint8_t* shared_buf = gfx_get_shared_buffer();
  while (*text && x < SSD1306_WIDTH) {
    uint8_t char_index = (uint8_t) *text;
    if (char_index < GFX_FONT_FIRST_CHAR || char_index > GFX_FONT_LAST_CHAR) {
      char_index = GFX_FONT_FIRST_CHAR;
    }
    const uint8_t* glyph = GFX_FONT_DATA[char_index - GFX_FONT_FIRST_CHAR];
    for (int col = 0; col < GFX_FONT_CHAR_WIDTH && x < SSD1306_WIDTH; col++) {
      uint8_t column_data = glyph[col];
      uint8_t shifted     = (y_offset >= 0) ? (uint8_t) (column_data << y_offset)
                                            : (uint8_t) (column_data >> (-y_offset));
      shared_buf[x] |= shifted;
      x++;
    }
    if (x < SSD1306_WIDTH) {
      x++;
    }
    text++;
  }
}

int ssd1306_write_page(uint8_t page, const uint8_t* data)
{
  if (page >= g_pages) {
    return RES_BAD_LEN;
  }
  ssd1306_set_addr(page, page);
  return ssd1306_send_data_bulk(data, SSD1306_WIDTH);
}

/** \brief Return 1 while a low-level chunked DMA transfer is active, else 0. */
int ssd1306_dma_xfer_active(void)
{
  return g_xfer.active ? 1 : 0;
}

uint8_t ssd1306_get_render_stage(void)
{
  return g_async.active ? g_async.stage : 0xFFu;
}

/** Return current panel height (32 or 64). */
uint8_t ssd1306_height(void)
{
  return g_height;
}
/** Return current page count (height/8). */
uint8_t ssd1306_pages(void)
{
  return g_pages;
}
/** Set display height to 32 or 64 and update related registers. */
int ssd1306_set_height(uint8_t height)
{
  /* Accept exactly 32 or 64 */
  if (height != 32U && height != 64U) {
    return RES_RANGE;
  }
  g_height = height;
  g_pages  = (uint8_t) (g_height / SSD1306_PAGE_HEIGHT);
  /* Apply geometry-related commands: multiplex and page address range for next ops. */
  uint8_t seq[] = {
    SSD1306_CMD_SET_MULTIPLEX, 
    g_height - 1U,
    SSD1306_CMD_SET_DISPLAY_OFFSET,
    g_height == 32U ? 0x00 : 0x00,
    SSD1306_CMD_SET_COMPINS,
    g_height == 32U ? 0x02 : 0x12
  };
  /* Send multiplex only; page range is set per operation via helpers. */
  (void) ssd1306_commands(seq, sizeof(seq));
  return RES_OK;
}
