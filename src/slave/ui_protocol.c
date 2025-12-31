/**
 * @file ui_protocol.c
 * @brief UI protocol implementation for SPI-based display communication.
 *
 * This module implements the complete UI protocol stack including:
 * - JSON parsing and element creation
 * - Command dispatching and response handling
 * - Element state management and attribute storage
 * - Protocol state machine and error handling
 *
 * The protocol supports real-time UI updates via SPI commands and can
 * dynamically create and modify display elements from JSON descriptions.
 */
/* =========================================================================
 * UI Protocol Implementation (full)
 * ========================================================================= */
#include "ui_protocol.h"

#include "ui_focus.h"
#include "ui_numeric.h"
#include "ui_tree.h"
/* Always include hardware headers; native build substitutes stub versions via test/hal_stub. */
#include "ch32fun.h"
#include "debug_led.h"
#include "ssd1306_driver.h"
/* COBS for response encoding */
#include "cobs.h"
#include "status_codes.h"
/* SPI TX DMA for responses */
#include <string.h>

#include "spi_slave_dma.h"

/* Forward declarations for internal RX helpers implemented later in this file */
static inline void spi_rx_reset(void);
static int         handle_element_object(const char* os, const char* oe);
// Global flag for render request from JSON processing
volatile uint8_t g_render_requested = 0;
/* Overlay command handlers declared in header */
/**
 * @brief Barrel edit state helpers using numeric_aux field.
 * Bit7: editing flag, Bit0..6: snapshot index.
 */
uint8_t barrel_is_editing(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return 0u;
  }
  return (uint8_t) ((protocol_numeric_aux(eid) & 0x80u) ? 1u : 0u);
}

/* Screen/ancestor helpers moved to ui_tree.c */
/* Focus/navigation helpers moved to ui_focus.c */
/* Small helpers and TX buffer (kept local to this module) */
/** Return non-zero if the character is ASCII whitespace. */
static inline int is_space_char(char c)
{
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}
/** Return non-zero if the character is ASCII digit. */
static inline int is_digit_char(char c)
{
  return (c >= '0' && c <= '9');
}
/** Return standard parse failure code. */
static inline int err(void) { return RES_PARSE_FAIL; }

/* Logging provided via weak symbol in hw_weak_defaults.c */

/* Outgoing SPI response buffer */
static uint8_t g_tx_buf[SPI_BUFFER_SIZE];
static uint8_t g_tx_len = 0;

/* RX (COBS-encoded) collector until LEN bytes are collected */
/* RX state for unified frame format: [SYNC0][SYNC1][LEN][COBS_DATA] */
static volatile uint8_t g_rx_enc_buf[112];
static volatile uint8_t g_rx_enc_len   = 0; /* collected COBS bytes */
static volatile uint8_t g_rx_frame_len = 0; /* expected COBS length from LEN */
static volatile uint8_t g_rx_frame_ready = 0; /* full frame collected */

/* RX inter-byte timeout bookkeeping (not used here but preserved for parity) */
static volatile uint32_t g_rx_last_byte_ms = 0;

typedef enum {
  RX_STATE_WAIT_SYNC0,
  RX_STATE_WAIT_SYNC1,
  RX_STATE_WAIT_LEN,
  RX_STATE_COLLECT_COBS
} rx_state_t;

static volatile rx_state_t g_rx_state = RX_STATE_WAIT_SYNC0;

static volatile uint8_t g_rx_overrun = 0u;

/* Deferred SPI TX queue (single frame, max 64 bytes). */
#define SPI_TX_QUEUE_SIZE 64u
static uint8_t g_tx_queue[SPI_TX_QUEUE_SIZE];
static uint8_t g_tx_queue_len = 0u;
static uint8_t g_tx_queue_pending = 0u;

/* Standby request flag; handled by main loop. */
volatile uint8_t g_request_standby = 0;

/* Placeholder for compatibility with older code paths; reserved for future ISR offload. */
static void protocol_tx_process_queue(void)
{
  if (g_tx_queue_pending == 0u) {
    return;
  }
  if (spi_slave_tx_dma_is_complete() == 0) {
    return;
  }
  spi_slave_tx_dma_start((const uint8_t*) g_tx_queue, g_tx_queue_len);
  g_tx_queue_pending = 0u;
  g_tx_queue_len     = 0u;
}

void protocol_service_deferred_ops(void)
{
  if (g_rx_overrun != 0u) {
    spi_rx_reset();
    g_rx_frame_ready = 0u;
    g_rx_overrun     = 0u;
  }

  /* Prioritize any queued TX frame before processing new RX data. */
  protocol_tx_process_queue();

  if (g_rx_frame_ready != 0u) {
    static uint8_t decoded_frame[SPI_BUFFER_SIZE];
    size_t         dec = cobs_decode((const uint8_t*) g_rx_enc_buf,
                             g_rx_enc_len,
                             (uint8_t*) &decoded_frame,
                             sizeof(decoded_frame));
    if (dec <= SPI_BUFFER_SIZE && dec >= 1) {
      uint8_t  command    = decoded_frame[0];
      uint8_t* payload    = (dec > 1) ? (decoded_frame + 1) : (uint8_t*) 0;
      uint8_t  length     = (dec > 1) ? (uint8_t) (dec - 1) : 0;
      int      cmd_result = handle_binary_command(command, payload, length);
      if (cmd_result != PROTOCOL_RESP_SENT) {
        uint8_t rc             = protocol_map_result_to_rc(cmd_result);
        uint8_t ret_payload[1] = {rc};
        protocol_send_response(command, ret_payload, 1);
      }
    }
    g_rx_frame_ready = 0u;
    spi_rx_reset();
  }

  /* Start queued TX after RX processing if DMA is idle. */
  protocol_tx_process_queue();
}

/** Set a render request flag (the main loop starts rendering). */
void protocol_request_render(void)
{
  g_render_requested = 1;
}

/** Reset SPI RX framing state for the next COBS frame. */
static inline void spi_rx_reset(void)
{
  g_rx_enc_len      = 0;
  g_rx_frame_len    = 0;
  g_rx_state        = RX_STATE_WAIT_SYNC0;
  g_rx_last_byte_ms = 0;
}

/* IRQ hook called from SPI1_IRQHandler to ingest a received byte.
 * In native builds register structures are stubbed; logic remains harmless. */
void ui_spi_rx_irq(void)
{
  uint32_t sr = SPI1->STATR;
  if (sr & SPI_STATR_OVR) {
    (void) SPI1->DATAR; /* clear OVR */
    g_rx_overrun = 1u;
    return;
  }
  uint8_t b = (uint8_t) SPI1->DATAR;
  if (g_rx_frame_ready != 0u) {
    return;
  }
  switch (g_rx_state) {
    case RX_STATE_WAIT_SYNC0:
      if (b == SPI_RESP_SYNC0) {
        g_rx_state = RX_STATE_WAIT_SYNC1;
      }
      break;
    case RX_STATE_WAIT_SYNC1:
      if (b == SPI_RESP_SYNC1) {
        g_rx_state = RX_STATE_WAIT_LEN;
      } else {
        g_rx_state = RX_STATE_WAIT_SYNC0;
      }
      break;
    case RX_STATE_WAIT_LEN:
      g_rx_frame_len = b;
      g_rx_enc_len   = 0;
      if (g_rx_frame_len > 0 && g_rx_frame_len <= sizeof(g_rx_enc_buf)) {
        g_rx_state = RX_STATE_COLLECT_COBS;
      } else {
        g_rx_state = RX_STATE_WAIT_SYNC0;
      }
      break;
    case RX_STATE_COLLECT_COBS:
      if (g_rx_enc_len < sizeof(g_rx_enc_buf)) {
        g_rx_enc_buf[g_rx_enc_len++] = b;
        if (g_rx_enc_len >= g_rx_frame_len) {
          g_rx_frame_ready = 1u;
          g_rx_state       = RX_STATE_WAIT_SYNC0;
        }
      } else {
        g_rx_state   = RX_STATE_WAIT_SYNC0;
        g_rx_overrun = 1u;
      }
      break;
    default:
      g_rx_state = RX_STATE_WAIT_SYNC0;
      break;
  }
}

/* Forward prototype for ping before dispatcher */
static int  cmd_ping(uint8_t* p, uint8_t l);
/* cmd_goto_standby is declared in ui_protocol.h */
/**
 * @brief Handle ping command and return protocol information.
 * @param p Command payload (unused)
 * @param l Payload length (must be 0)
 * @return Protocol response status
 */
static int cmd_ping(uint8_t* p, uint8_t l)
{
  /* unused: p (ping has no payload) */
  if (l != 0) {
    return RES_BAD_LEN;
  }
  uint8_t out[4];
  out[0] = RC_OK;
  out[1] = (uint8_t) g_protocol_state.protocol_version;
  out[2] = (uint8_t) (g_protocol_state.capabilities & 0xFF);
  out[3] = (uint8_t) ((g_protocol_state.capabilities >> 8) & 0xFF);
  protocol_send_response(SPI_CMD_PING, out, 4);
  return PROTOCOL_RESP_SENT;
}

/**
 * @brief Map internal result codes to protocol response codes.
 * @param r Internal result code
 * @return Protocol response code
 */
uint8_t protocol_map_result_to_rc(int r)
{
  if (r == RES_OK) {
    return RC_OK;
  }
  if (r == PROTOCOL_RESP_SENT) {
    return RC_OK;
  }
  switch (r) {
    case RES_BAD_LEN: return RC_BAD_LEN;
    case RES_BAD_STATE: return RC_BAD_STATE;
    case RES_UNKNOWN_ID: return RC_UNKNOWN_ID;
    case RES_RANGE: return RC_RANGE;
    case RES_INTERNAL: return RC_INTERNAL;
    case RES_NO_SPACE: return RC_NO_SPACE;
    case RES_PARSE_FAIL: return RC_PARSE_FAIL;
    default: return RC_INTERNAL;
  }
}

/**
 * @brief Send a protocol response frame via SPI.
 * @param cmd Command ID that this response corresponds to
 * @param payload Response payload data
 * @param len Length of payload data
 * @return 0 on success
 */
int protocol_send_response(uint8_t cmd, const uint8_t* payload, uint8_t len)
{
  g_tx_buf[0] = SPI_RESP_SYNC0;
  g_tx_buf[1] = SPI_RESP_SYNC1;
  size_t enc  = cobs_encode(payload, len, (uint8_t*) g_tx_buf + 3, (size_t) (sizeof(g_tx_buf) - 3));
  g_tx_buf[2] = enc;
  if (enc == 0) {
    return RES_INTERNAL;
  }
  g_tx_len = (uint8_t) (2 + enc + 1);

  if (g_tx_queue_pending != 0u) {
    return RES_BAD_STATE;
  }
  if (spi_slave_tx_dma_is_complete() == 0) {
    if (g_tx_len > SPI_TX_QUEUE_SIZE) {
      return RES_BAD_LEN;
    }
    memcpy(g_tx_queue, g_tx_buf, g_tx_len);
    g_tx_queue_len     = g_tx_len;
    g_tx_queue_pending = 1u;
    return RES_OK;
  }
  spi_slave_tx_dma_start((const uint8_t*) g_tx_buf, g_tx_len);
  return RES_OK;
}



#ifndef PROTOCOL_USE_SPI
#define PROTOCOL_USE_SPI 0
#endif

/** Global protocol state instance. */
protocol_state_t g_protocol_state = {0};

/**
 * @brief Return allocated per-element capacity (0 if not initialized).
 */
uint8_t protocol_element_capacity(void)
{
  return g_protocol_state.element_capacity;
}

/**
 * @brief Get overlay role for a screen element.
 * @param element_id Screen element id
 * @return overlay_role_t value (OVERLAY_NONE if not set or invalid)
 */
uint8_t protocol_screen_role(uint8_t element_id)
{
  if (element_id >= g_protocol_state.element_count) {
    return OVERLAY_NONE;
  }
  if (g_protocol_state.elements == NULL) {
    return OVERLAY_NONE;
  }
  if (g_protocol_state.elements[element_id].type != ELEMENT_SCREEN) {
    return OVERLAY_NONE;
  }
  uint8_t role = OVERLAY_NONE;
  if (ui_attr_get_screen_role(&g_protocol_state.runtime, element_id, &role) != RES_OK) {
    return OVERLAY_NONE;
  }
  return role;
}

int16_t protocol_numeric_value(uint8_t element_id)
{
  ur_barrel_state_t* st = ur_barrel_find(&g_protocol_state.runtime, element_id);
  if (!st) {
    return 0;
  }
  return st->value;
}

uint8_t protocol_numeric_aux(uint8_t element_id)
{
  ur_barrel_state_t* st = ur_barrel_find(&g_protocol_state.runtime, element_id);
  if (!st) {
    return 0;
  }
  return st->aux;
}

/* --- Type mapping helper (short + long names, size-optimized) --- */
/**
 * @brief Map type key string to element type enum.
 * @param s Type key string
 * @return Element type enum value, or 0xFF if invalid
 */
static uint8_t map_type_key(const char* s)
{
  /* Accept both legacy 2-letter tokens and new 1-letter tokens. */
  if (!s || s[0] == '\0') {
    return 0xFF;
  }
  /* New single-letter tokens */
  if (s[1] == '\0') {
    switch (s[0]) {
      case 's': return ELEMENT_SCREEN;
      case 't': return ELEMENT_TEXT;
      case 'l': return ELEMENT_LIST_VIEW;
      case 'b': return ELEMENT_BARREL;
      case 'i': return ELEMENT_TRIGGER;
      default: return 0xFF;
    }
  }
  /* Legacy two-letter tokens */
  if (s[2] == '\0') {
    if (s[0] == 't' && s[1] == 'e') return ELEMENT_TEXT;
    if (s[0] == 'l' && s[1] == 'i') return ELEMENT_LIST_VIEW;
    if (s[0] == 'b' && s[1] == 'a') return ELEMENT_BARREL;
    if (s[0] == 't' && s[1] == 'r') return ELEMENT_TRIGGER;
  }
  return 0xFF;
}

/** Reserve per-element storage from the shared arena head. */
static int protocol_reserve_element_storage(uint8_t capacity)
{
  if (capacity == 0u) {
    return RES_RANGE;
  }
  if (g_protocol_state.element_capacity != 0u) {
    return RES_BAD_STATE;
  }
  uint16_t need = (uint16_t) capacity * (uint16_t) (sizeof(element_t) + 2u);
  if (need > (uint16_t) sizeof(g_protocol_state.runtime.arena)) {
    return RES_NO_SPACE;
  }
  uint16_t off = 0u;
  uint8_t* base = &g_protocol_state.runtime.arena[0];
  g_protocol_state.elements = (element_t*) &base[off];
  off = (uint16_t) (off + (uint16_t) capacity * (uint16_t) sizeof(element_t));
  g_protocol_state.pos_x = &base[off];
  off = (uint16_t) (off + capacity);
  g_protocol_state.pos_y = &base[off];
  off = (uint16_t) (off + capacity);
  g_protocol_state.element_capacity = capacity;
  g_protocol_state.runtime.attr_base = off;
  g_protocol_state.runtime.head_used = off;
  memset(g_protocol_state.elements, 0xFF, (uint16_t) capacity * (uint16_t) sizeof(element_t));
  memset(g_protocol_state.pos_x, 0, capacity);
  memset(g_protocol_state.pos_y, 0, capacity);
  return RES_OK;
}

/* ------------------------------------------------------------------------- */
/* Small helpers */
/** Allocate and initialize a basic element with position. */
static uint8_t add_basic_element(uint8_t parent, uint8_t type, int x, int y)
{
  if (g_protocol_state.element_capacity == 0u) {
    return 0xFF;
  }
  if (g_protocol_state.element_count >= g_protocol_state.element_capacity) {
    return 0xFF;
  }
  if (g_protocol_state.elements == NULL || g_protocol_state.pos_x == NULL ||
      g_protocol_state.pos_y == NULL) {
    return 0xFF;
  }
  uint8_t    id = g_protocol_state.element_count++;
  element_t* el = &g_protocol_state.elements[id];
  el->parent_id = parent;
  el->type      = type;
  ui_attr_store_position(&g_protocol_state.runtime, id, (uint8_t) x, (uint8_t) y, 8, LAYOUT_ABSOLUTE);
  return id;
}
/* ------------------------------------------------------------------------- */

/* Command dispatcher implementation */
int handle_binary_command(uint8_t cmd, uint8_t* payload, uint8_t length)
{
  switch (cmd) {
    case SPI_CMD_PING: return cmd_ping(payload, length);
    case SPI_CMD_JSON: return cmd_json(payload, length);
    case SPI_CMD_JSON_ABORT: return cmd_json_abort(payload, length);
    case SPI_CMD_SET_ACTIVE_SCREEN:
      return cmd_set_active_screen(payload, length);
      /* Legacy element update opcodes are intentionally not dispatched anymore.
        Use SPI_CMD_JSON (0x01) with 'e' addressing for runtime updates. */
    case SPI_CMD_GET_STATUS: return cmd_get_status(payload, length);
    case SPI_CMD_SCROLL_TO_SCREEN:
      return cmd_scroll_to_screen(payload, length);
      /* List view update is host-side only now; no direct opcode dispatch. */
    case SPI_CMD_GET_ELEMENT_STATE: return cmd_get_element_state(payload, length);
  /* No error log feature */
  case SPI_CMD_SHOW_OVERLAY: return cmd_show_overlay(payload, length);
    case SPI_CMD_INPUT_EVENT: return cmd_input_event(payload, length);
    case SPI_CMD_GOTO_STANDBY: return cmd_goto_standby(payload, length);
    default: return RES_BAD_LEN;
  }
}

/* Basic commands */
int cmd_set_active_screen(uint8_t* p, uint8_t l)
{
  if (l != 1) {
    return RES_BAD_LEN;
  }
  uint8_t sid = p[0];
  /* Accept only base screen ordinals in range */
  if (sid >= g_protocol_state.screen_count) {
    return RES_RANGE;
  }
  g_protocol_state.active_screen = sid;
  g_protocol_state.scroll_x      = (int16_t) sid * (int16_t) SSD1306_WIDTH;
  g_protocol_state.screen_anim.active    = 0u;
  g_protocol_state.screen_anim.offset_px = 0;
  g_protocol_state.screen_anim.dir       = 0;
  g_protocol_state.screen_anim.from_screen = sid;
  g_protocol_state.screen_anim.to_screen   = sid;
  /* Screen change: auto-focus first focusable element on new screen */
  protocol_focus_first_on_screen(sid);
  debug_log_event(DEBUG_LED_EVT_SET_ACTIVE_SCREEN, (uint8_t) (sid & 0x07u));
  /* Async rendering is driven by the main loop. */
  return RES_OK;
}
int cmd_get_status(uint8_t* p, uint8_t l)
{
  /* unused: p,l (GET_STATUS carries no payload) */
  uint8_t flags = 0u;
  if (g_protocol_state.initialized) {
    flags |= STATUS_FLAG_INITIALIZED;
  }
  if (g_protocol_state.status_dirty) {
    flags |= STATUS_FLAG_DIRTY;
  }
  if (g_protocol_state.overlay.active_overlay_screen_id != 0xFF) {
    flags |= STATUS_FLAG_OVERLAY;
  }
  uint8_t  out[1 + 1 + 1 + 1 + 1 + 1 + 4]; /* RC+flags+elem+screen+active+ver+dirty_id+reserved */
  out[0] = RC_OK; /* RC */
  out[1] = flags;
  out[2] = g_protocol_state.element_count;
  out[3] = g_protocol_state.screen_count;
  out[4] = g_protocol_state.active_screen;
  out[5] = (uint8_t) g_protocol_state.protocol_version;
  out[6] = (g_protocol_state.status_dirty != 0u)
             ? g_protocol_state.status_dirty_id
             : INVALID_ELEMENT_ID;
  out[7] = 0u;
  out[8] = 0u;
  out[9] = 0u;
  protocol_send_response(SPI_CMD_GET_STATUS, out, 10);
  g_protocol_state.status_dirty    = 0u;
  g_protocol_state.status_dirty_id = INVALID_ELEMENT_ID;
  return PROTOCOL_RESP_SENT;
}
int cmd_scroll_to_screen(uint8_t* p, uint8_t l)
{
  if (l == 1) {
    /* If a horizontal slide animation is in progress, ignore host snap to avoid overriding user
     * animation. */
    if (g_protocol_state.screen_anim.active) {
      /* ignore snap during slide */
      return RES_OK;
    }
    uint8_t sid = p[0];
    if (sid >= g_protocol_state.screen_count) {
      return RES_RANGE;
    }
    g_protocol_state.active_screen = sid;
    g_protocol_state.scroll_x      = (int16_t) sid * 128;
    debug_log_event(DEBUG_LED_EVT_SCROLL_TO_SCREEN, (uint8_t) (sid & 0x07u));
    return RES_OK;
  }
  if (l == 3) {
    /* If a horizontal slide animation is in progress, ignore host snap to avoid overriding user
     * animation. */
    if (g_protocol_state.screen_anim.active) {
      /* ignore snap during slide */
      return RES_OK;
    }
    int16_t off = (int16_t) (p[0] | (p[1] << 8));
    uint8_t sid = p[2];
    if (sid >= g_protocol_state.screen_count) {
      return RES_RANGE;
    }
    int16_t max_off = (int16_t) (g_protocol_state.screen_count - 1) * SSD1306_WIDTH;
    if (off < 0)
      off = 0;
    if (off > max_off)
      off = max_off;

    g_protocol_state.active_screen = sid;
    g_protocol_state.scroll_x      = off;
    debug_log_event(DEBUG_LED_EVT_SCROLL_TO_SCREEN, (uint8_t) (sid & 0x07u));
    return RES_OK;
  }
  return RES_BAD_LEN;
}
 

/* No debug error code defines; GET/CLEAR_ERROR_LOG commands unsupported. */

/* Overlay controls */
int cmd_show_overlay(uint8_t* p, uint8_t l)
{
  if (l < 1) {
    return RES_BAD_LEN;
  }
  uint8_t  sid  = p[0];
  uint16_t dur  = 1200; /* default duration in ms */
  uint8_t  mask = 0;
  if (l >= 3) {
    dur = (uint16_t) (p[1] | (uint16_t) (p[2] << 8));
    if (!dur) {
      dur = 1;
    }
  }
  if (l >= 4) {
    uint8_t f = p[3];
    mask      = (uint8_t) ((f & 0x01u) ? 1u : 0u);
  }
  if (sid >= g_protocol_state.element_count) {
    return RES_UNKNOWN_ID;
  }
  if (g_protocol_state.elements[sid].type != ELEMENT_SCREEN) {
    return RES_BAD_STATE;
  }
  /* Only allow if this screen has an overlay role assigned */
  if (protocol_screen_role(sid) != OVERLAY_FULL) {
    return RES_BAD_STATE;
  }
  g_protocol_state.overlay.active_overlay_screen_id = sid;
  g_protocol_state.overlay.remaining_ms             = dur;
  g_protocol_state.overlay.mask_input               = mask;
  g_protocol_state.overlay.prev_focus               = g_protocol_state.focused_element;
  debug_log_event(DEBUG_LED_EVT_SHOW_OVERLAY, (uint8_t) (sid & 0x07u));
  protocol_clear_focus();
  g_render_requested = 1;
  return RES_OK;
}

void protocol_overlay_cleared(void)
{
  uint8_t prev_focus = g_protocol_state.overlay.prev_focus;
  g_protocol_state.overlay.prev_focus = INVALID_ELEMENT_ID;
  if (prev_focus != INVALID_ELEMENT_ID) {
    protocol_set_focus(prev_focus);
    if (g_protocol_state.focused_element != INVALID_ELEMENT_ID) {
      return;
    }
  }
  protocol_focus_first_on_screen(g_protocol_state.active_screen);
  if (g_protocol_state.focused_element == INVALID_ELEMENT_ID) {
    protocol_clear_focus();
  }
}

/* GoToStandby: set flag for main loop to perform display_off + standby entry. */
int cmd_goto_standby(uint8_t* p, uint8_t l)
{
  /* unused: p (standby has no payload) */
  if (l == 0) {
    g_request_standby = 1;
  }
  /* No response is sent for this command to avoid CS timing races */
  return PROTOCOL_RESP_SENT;
}


/* Focus handling moved to ui_focus.c */

/* Element state query */
int cmd_get_element_state(uint8_t* payload, uint8_t length)
{
  if (length != 1)
    return RES_BAD_LEN;
  uint8_t eid = payload[0];
  if (eid >= g_protocol_state.element_count)
    return RES_UNKNOWN_ID;
  element_t* el = &g_protocol_state.elements[eid];
  uint8_t    out[1 + 12]; /* RC + data (fits all variants) */
  memset(out, 0, sizeof(out));
  out[0] = RC_OK;
  out[1] = el->type;
  if (el->type == ELEMENT_TEXT) {
    const char* t = ui_attr_get_text(&g_protocol_state.runtime, eid);
    uint8_t     l = (uint8_t) (t ? strlen(t) : 0);
    if (l + 3 > sizeof(out))
      l = (uint8_t) (sizeof(out) - 3);
    out[2] = l;
    if (t && l)
      memcpy(&out[3], t, l);
    protocol_send_response(SPI_CMD_GET_ELEMENT_STATE, out, (uint8_t) (3 + l));
    return PROTOCOL_RESP_SENT;
  }
  if (el->type == ELEMENT_TRIGGER) {
    ur_trigger_state_t* ts = ur_trigger_find(&g_protocol_state.runtime, eid);
    if (ts) {
      out[2] = ts->version;
      protocol_send_response(SPI_CMD_GET_ELEMENT_STATE, out, 3);
      return PROTOCOL_RESP_SENT;
    }
    return RES_RANGE;
  }
  if (el->type == ELEMENT_BARREL) {
    int16_t v = protocol_numeric_value(eid);
    out[2]    = (uint8_t) v;
    out[3]    = (uint8_t) (v >> 8);
    protocol_send_response(SPI_CMD_GET_ELEMENT_STATE, out, 4);
    return PROTOCOL_RESP_SENT;
  }
  out[2] = 0xFF;
  protocol_send_response(SPI_CMD_GET_ELEMENT_STATE, out, 3);
  return PROTOCOL_RESP_SENT;
}

/* JSON helper functions */
/** Extract an integer value for a key from a JSON object span. */
static int extract_int_key(const char* s, const char* e, const char* key, int* out)
{
  if (!s || !e || !key || !*key)
    return RES_PARSE_FAIL;
  const size_t klen = strlen(key);
  for (const char* p = s; p + (klen + 3) <= e; ++p) {
    if (*p != '"')
      continue;
    /* Quick length and prefix check: "key" */
    if ((size_t) (e - p) <= (klen + 2))
      break;
    if (p[1] != key[0])
      continue;
    /* Compare inside quotes */
    size_t i = 0;
    for (; i < klen && (p[1 + i] == key[i]); ++i) {}
    if (i != klen)
      continue;
    if (p[1 + klen] != '"')
      continue;
    const char* q = p + 1 + klen + 1; /* past closing quote */
    /* Skip spaces */
    while (q < e && is_space_char(*q))
      q++;
    if (q >= e || *q != ':')
      continue;
    q++;
    while (q < e && is_space_char(*q))
      q++;
    if (q >= e)
      return RES_PARSE_FAIL;
    /* Optional quoted number support ("-123") */
    if (*q == '"')
      q++;
    int sign = 1;

    int val  = 0;
    if (q < e && *q == '-') {
      sign = -1;
      q++;
    }
    int any = 0;
    while (q < e && is_digit_char(*q)) {
      val = val * 10 + (*q - '0');
      q++;
      any = 1;
    }
    if (!any)
      return RES_PARSE_FAIL;
    *out = sign * val;
    return 0;
  }
  return RES_PARSE_FAIL;
}
/** Extract a string value for a key from a JSON object span. */
static int extract_string_key(const char* s,
                              const char* e,
                              const char* key,
                              char*       out,
                              size_t      outsz)
{
  if (!s || !e || !key || !*key || !out || outsz == 0)
    return RES_PARSE_FAIL;
  const size_t klen = strlen(key);
  for (const char* p = s; p + (klen + 3) <= e; ++p) {
    if (*p != '"')
      continue;
    if ((size_t) (e - p) <= (klen + 2))
      break;
    if (p[1] != key[0])
      continue;
    size_t i = 0;
    for (; i < klen && (p[1 + i] == key[i]); ++i) {}
    if (i != klen)
      continue;
    if (p[1 + klen] != '"')
      continue;
    const char* q = p + 1 + klen + 1;
    while (q < e && is_space_char(*q))
      q++;
    if (q >= e || *q != ':')
      continue;
    q++;
    while (q < e && is_space_char(*q))
      q++;
    if (q >= e || *q != '"')
      return RES_PARSE_FAIL;
    q++;
    size_t idx = 0;
    while (q < e && *q != '"' && idx + 1 < outsz) {
      out[idx++] = *q++;
    }
    if (q >= e)
      return RES_PARSE_FAIL;
    out[idx] = '\0';
    return 0;
  }
  return RES_PARSE_FAIL;
}
void protocol_element_changed(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return;
  }
  g_protocol_state.status_dirty    = 1u;
  g_protocol_state.status_dirty_id = eid;
}
void protocol_reset_state(void)
{
  memset(&g_protocol_state, 0, sizeof(g_protocol_state));
  g_protocol_state.active_screen   = 0;
  g_protocol_state.screen_count    = 0;
  g_protocol_state.element_count   = 0;
  g_protocol_state.element_capacity = 0;
  g_protocol_state.elements        = NULL;
  g_protocol_state.pos_x           = NULL;
  g_protocol_state.pos_y           = NULL;
  g_protocol_state.scroll_x        = 0;
  g_protocol_state.initialized     = 0;
  g_protocol_state.status_dirty    = 0u;
  g_protocol_state.status_dirty_id = INVALID_ELEMENT_ID;
  g_protocol_state.trigger_count   = 0;
  g_protocol_state.header_seen     = 0u;

  g_protocol_state.overlay.active_overlay_screen_id = INVALID_ELEMENT_ID;
  g_protocol_state.overlay.remaining_ms             = 0;
  g_protocol_state.overlay.mask_input               = 0;
  g_protocol_state.overlay.prev_focus               = INVALID_ELEMENT_ID;
  g_protocol_state.protocol_version                 = 1;
  g_protocol_state.capabilities                     = 0;
  g_protocol_state.focused_element                  = INVALID_ELEMENT_ID;
  g_protocol_state.input_source                     = INPUT_SRC_NONE;
  g_protocol_state.nav_depth                        = 0;
  g_protocol_state.active_local_screen              = INVALID_ELEMENT_ID;
  for (uint8_t i = 0; i < NAV_STACK_MAX_DEPTH; i++) {
    g_protocol_state.nav_stack[i].type               = (uint8_t) NAV_CTX_LIST;
    g_protocol_state.nav_stack[i].target_element     = INVALID_ELEMENT_ID;
    g_protocol_state.nav_stack[i].return_list        = INVALID_ELEMENT_ID;
    g_protocol_state.nav_stack[i].saved_cursor       = 0u;
    g_protocol_state.nav_stack[i].saved_top          = 0u;
    g_protocol_state.nav_stack[i].saved_focus        = INVALID_ELEMENT_ID;
    g_protocol_state.nav_stack[i].saved_active_screen = 0u;
  }
  memset(&g_protocol_state.screen_anim, 0, sizeof(g_protocol_state.screen_anim));
  ur_init(&g_protocol_state.runtime);
}
void protocol_init(void)
{
  protocol_reset_state();
  g_protocol_state.protocol_version = 1;
  g_protocol_state.capabilities     = 0;
}

void protocol_tick_animations(void)
{
  /* Timebase for overlay countdown and animation throttle. */
  static uint32_t last_anim_ms = 0;
  static uint32_t last_overlay_ms = 0;
  uint32_t        now = get_system_time_ms();
  if (last_overlay_ms == 0u) {
    last_overlay_ms = now;
  }
  uint32_t elapsed_ms = (uint32_t) (now - last_overlay_ms);
  last_overlay_ms = now;

  if (g_protocol_state.overlay.active_overlay_screen_id != 0xFF &&
      g_protocol_state.overlay.remaining_ms > 0u) {
    uint32_t remaining = g_protocol_state.overlay.remaining_ms;
    if (elapsed_ms >= remaining) {
      remaining = 0u;
    } else {
      remaining -= elapsed_ms;
    }
    g_protocol_state.overlay.remaining_ms = (uint16_t) remaining;
    if (remaining == 0u) {
      uint8_t cleared_overlay = g_protocol_state.overlay.active_overlay_screen_id;
      g_protocol_state.overlay.active_overlay_screen_id = 0xFF;
      protocol_overlay_cleared();
      g_render_requested = 1;
      debug_log_event(DEBUG_LED_EVT_OVERLAY_CLEAR,
                      (cleared_overlay != 0xFFu) ? (uint8_t) (cleared_overlay & 0x07u)
                                                 : 0xFFu);
    }
  }

  /* Frame throttle for animations. */
  if ((uint32_t) (now - last_anim_ms) < PROTOCOL_ANIM_FRAME_MS) {
    return; /* not time for next frame */
  }
  last_anim_ms = now;

  /* No scroll_x easing: horizontal motion handled by screen_anim blending only. */
  /* Screen slide animation (logical active_screen already set to target).
     We animate visual offset_px until reaching 128px; renderer will blend.
   */
  if (g_protocol_state.screen_anim.active) {
    screen_anim_state_t* sa   = &g_protocol_state.screen_anim;
    int16_t              step = SCREEN_ANIM_PIXELS_PER_FRAME;
    if (step <= 0)
      step = 1;
    sa->offset_px = (int16_t) (sa->offset_px + step);
    if (sa->offset_px >= 128) {
      sa->active    = 0;
      sa->offset_px = 0;
      /* Snap base scroll to new active screen position */
      g_protocol_state.scroll_x = (int16_t) g_protocol_state.active_screen * 128;
      /* Re-assign focus now that the slide animation has finished */
      protocol_focus_first_on_screen(g_protocol_state.active_screen);
      protocol_request_render();
    }
  }
  uint8_t any_anim = 0;
  {
    ur_off_t cur = g_protocol_state.runtime.lists_head_off;
    while (cur) {
      ur_list_node_t* n = (ur_list_node_t*) ur__ptr(&g_protocol_state.runtime, cur);
      if (!n) break;
      ur_list_state_t* ls = &n->st;
      if (ls->anim_active) {
        any_anim = 1;
        if (ls->anim_pix < 8) {
          uint8_t step = LIST_ANIM_PIXELS_PER_FRAME;
          if (step == 0) step = 1;
          uint8_t remain = (uint8_t) (8 - ls->anim_pix);
          if (step > remain) step = remain;
          ls->anim_pix = (uint8_t) (ls->anim_pix + step);
          if (ls->anim_pix >= 8) {
            ls->top_index   = ls->pending_top;
            ls->cursor      = ls->pending_cursor;
            ls->anim_active = 0;
            ls->anim_dir    = 0;
            ls->anim_pix    = 0;
          }
        }
      }
      cur = n->next_off;
    }
  }
  if (any_anim || g_protocol_state.screen_anim.active) {
    protocol_request_render();
  }

  if (g_protocol_state.edit_blink_active != 0u) {
    uint8_t counter = g_protocol_state.edit_blink_counter;
    counter = (uint8_t) (counter + 1u);
    if (counter >= EDIT_BLINK_PERIOD_FRAMES) {
      counter = 0u;
      g_protocol_state.edit_blink_phase = (uint8_t) (g_protocol_state.edit_blink_phase ^ 1u);
      protocol_request_render();
    }
    g_protocol_state.edit_blink_counter = counter;
  } else {
    g_protocol_state.edit_blink_counter = 0u;
    g_protocol_state.edit_blink_phase   = 1u;
  }

}

/* SPI transport initialization has been moved to spi_slave_transport_init() in spi_slave_dma.c */

/* ---------------- Streaming JSON command handlers (added) ---------------- */

/**
 * @brief Parse one top-level element JSON object ("{...}").
 * @param buf Pointer to raw JSON bytes.
 * @param len Length of bytes.
 * @return 0 on success, negative on validation failure.
 */
/**
 * @brief Parse and apply a single top-level element JSON object.
 *
 * Responsibilities:
 * - Lightweight validation of surrounding braces and non-empty content.
 * - Trimming of leading/trailing whitespace.
 * - Delegation to element creation logic (handle_element_object).
 *
 * Error handling is intentionally lenient: specific negative codes identify
 * structural issues; the caller may choose to ignore failures to allow best-effort
 * batch provisioning without aborting the entire sequence.
 *
 * @param buf Pointer to raw JSON bytes (not null-terminated).
 * @param len Length of bytes.
 * @return 0 on success, negative error code on validation failure.
 */
static int parse_single_element_object(const char* buf, uint8_t len)
{
  if (!buf || len < 2) {
    return RES_BAD_LEN;
  }
  /* Trim leading/trailing spaces */
  const char* s = buf;
  const char* e = buf + len - 1;
  while (s <= e && is_space_char(*s)) {
    s++;
  }
  while (e >= s && is_space_char(*e)) {
    e--;
  }
  if (s >= e) {
    return RES_PARSE_FAIL;
  }
  if (*s != '{' || *e != '}') {
    return RES_PARSE_FAIL;
  }
  return handle_element_object(s, e);
}

/** Apply one JSON element object with explicit flags; shared by cmd_json and tools. */
static int protocol_apply_json_object_internal(const char* buf, uint8_t len, uint8_t flags)
{
  int rc = 0;
  if (flags & JSON_FLAG_HEAD) {
    protocol_reset_state();
  }
  if (len > 0u && buf) {
    rc = parse_single_element_object(buf, len);
  }
  if (flags & JSON_FLAG_COMMIT) {
    if (g_protocol_state.element_capacity == 0u) {
      if (rc == RES_OK) {
        rc = RES_BAD_STATE;
      }
      return rc;
    }
    /* Immediate render */
    g_protocol_state.initialized = 1;
    g_render_requested           = 1;
    debug_log_event(DEBUG_LED_EVT_JSON_COMMIT, 0u);
  }
  return rc;
}

#if defined(UNIT_TEST) || defined(UI_MEMCALC)
int protocol_apply_json_object(const char* buf, uint8_t len, uint8_t flags)
{
  return protocol_apply_json_object_internal(buf, len, flags);
}
#endif

/**
 * @brief Handle unified JSON streaming command carrying exactly one element object.
 *
 * Payload layout: [flags][json-bytes]
 * Flags:
 *  - JSON_FLAG_HEAD   : Clear all protocol/UI state prior to applying object.
 *  - JSON_FLAG_COMMIT : Request immediate render after applying object.
 *
 * Behavior:
 *  - State reset occurs before parsing when HEAD is set.
 *  - Header element (t=h) is required before any non-header objects.
 *  - Element object is parsed best-effort; parse errors do not inhibit subsequent objects.
 *  - COMMIT sets render request flag enabling UI redraw on next loop iteration.
 *
 * @param p Pointer to payload buffer (flags + JSON bytes).
 * @param l Length of payload buffer.
 * @return RES_OK on success; negative on error.
 */
int cmd_json(uint8_t* p, uint8_t l)
{
  if (l == 0) {
    return RES_BAD_LEN; /* need at least flags */
  }
  uint8_t        flags    = p[0];
  const uint8_t* json     = p + 1;
  uint8_t        json_len = (uint8_t) (l - 1);
  return protocol_apply_json_object_internal((const char*) json, json_len, flags);
}
int cmd_json_abort(uint8_t* p, uint8_t l)
{
  /* unused: p,l (abort carries no payload) */
  return 0;
}

/* Element handler table (object-like dispatch using function pointers) */
typedef struct {
  uint8_t      parent_id;
  int          x;
  int          y;
  uint8_t      type_code;
  const char*  os;
  const char*  oe;
} element_create_ctx_t;

typedef struct {
  const char* os;
  const char* oe;
} element_update_ctx_t;

typedef struct {
  uint8_t type;
  int (*create)(const element_create_ctx_t* ctx);
  int (*update)(uint8_t id, const element_update_ctx_t* ctx);
} element_handler_t;

/** Create a screen element from parsed JSON context. */
static int handle_create_screen(const element_create_ctx_t* ctx)
{
  if (!ctx) {
    return err();
  }
  uint8_t sid = add_basic_element(ctx->parent_id, ELEMENT_SCREEN, ctx->x, ctx->y);
  if (sid == 0xFF) {
    return err();
  }
  if (ctx->parent_id == INVALID_ELEMENT_ID) {
    /* overlay role */
    int ov = 0;
    if (extract_int_key(ctx->os, ctx->oe, "ov", &ov) == 0) {
      if (ov < 0) {
        ov = 0;
      }
      if (ov > 1) {
        ov = 1;
      }
      if (ov != 0) {
        (void) ui_attr_store_screen_role(&g_protocol_state.runtime, sid, (uint8_t) ov);
      }
    }
    /* Count only base screens (overlay role NONE) */
    if (ov == 0) {
      g_protocol_state.screen_count++;
      if (g_protocol_state.screen_count == 1) {
        g_protocol_state.active_screen = 0;
      }
    }
  }
  uint8_t owner_text = INVALID_ELEMENT_ID;
  if (ctx->parent_id != INVALID_ELEMENT_ID) {
    uint8_t parent_type = g_protocol_state.elements[ctx->parent_id].type;
    if (parent_type == ELEMENT_TEXT) {
      owner_text = ctx->parent_id;
    } else if (parent_type == ELEMENT_LIST_VIEW) {
      ur_list_state_t* ls = ur_list_get_or_add(&g_protocol_state.runtime, ctx->parent_id);
      if (ls) {
        owner_text = ls->last_text_child;
      }
    }
  }
  protocol_register_local_screen(sid, owner_text);
  return 0;
}

/** Create a list element and initialize its runtime state. */
static int handle_create_list(const element_create_ctx_t* ctx)
{
  if (!ctx) {
    return err();
  }
  uint8_t lid = add_basic_element(ctx->parent_id, ELEMENT_LIST_VIEW, ctx->x, ctx->y);
  if (lid == 0xFF) {
    return err();
  }
  ur_list_state_t* ls = ur_list_get_or_add(&g_protocol_state.runtime, lid);
  if (ls) {
    ls->visible_rows = 4;
    ls->last_text_child = INVALID_ELEMENT_ID;
    int rows = 0;
    if (extract_int_key(ctx->os, ctx->oe, "r", &rows) == 0) {
      if (rows < 1) rows = 1;
      if (rows > 6) rows = 6;
      ls->visible_rows = (uint8_t) rows;
    }
  }
  return 0;
}

/** Create a text element, with list-item handling when nested under a list. */
static int handle_create_text(const element_create_ctx_t* ctx)
{
  if (!ctx) {
    return err();
  }
  uint8_t is_list_item = 0;
  uint8_t target_list  = INVALID_ELEMENT_ID;
  if (ctx->parent_id != INVALID_ELEMENT_ID) {
    if (g_protocol_state.elements[ctx->parent_id].type == ELEMENT_LIST_VIEW) {
      is_list_item = 1u;
      target_list  = ctx->parent_id;
    }
  }
  if (is_list_item) {
    uint8_t row_y = (uint8_t) (list_item_count(target_list) * 8);
    uint8_t id    = add_basic_element(ctx->parent_id, ELEMENT_TEXT, ctx->x, row_y);
    if (id == 0xFF) {
      return err();
    }
    char tb[21]; /* cap <= 20 + NUL */
    if (extract_string_key(ctx->os, ctx->oe, "tx", tb, sizeof(tb)) != 0) {
      tb[0] = '\0';
    }
    int cap = 0;
    (void) extract_int_key(ctx->os, ctx->oe, "c", &cap);
    if (cap < 0) cap = 0;
    if (cap > 20) cap = 20;
    ui_attr_store_text_with_cap(&g_protocol_state.runtime, id, tb, (uint8_t) cap);
    ur_list_state_t* ls = ur_list_get_or_add(&g_protocol_state.runtime, target_list);
    if (ls) {
      ls->last_text_child = id;
    }
    return 0;
  } else {
    uint8_t id = add_basic_element(ctx->parent_id, ELEMENT_TEXT, ctx->x, ctx->y);
    if (id == 0xFF) {
      return err();
    }
    char tb[21]; /* cap <= 20 + NUL */
    if (extract_string_key(ctx->os, ctx->oe, "tx", tb, sizeof(tb)) != 0) {
      tb[0] = '\0';
    }
    int cap = 0;
    (void) extract_int_key(ctx->os, ctx->oe, "c", &cap);
    if (cap < 0) cap = 0;
    if (cap > 20) cap = 20;
    ui_attr_store_text_with_cap(&g_protocol_state.runtime, id, tb, (uint8_t) cap);
    if (ctx->parent_id != INVALID_ELEMENT_ID &&
        g_protocol_state.elements[ctx->parent_id].type == ELEMENT_LIST_VIEW) {
      ur_list_state_t* ls = ur_list_get_or_add(&g_protocol_state.runtime, ctx->parent_id);
      if (ls) {
        ls->last_text_child = id;
      }
    }
    return 0;
  }
}

/** Create a barrel element and initialize its value. */
static int handle_create_barrel(const element_create_ctx_t* ctx)
{
  if (!ctx) {
    return err();
  }
  uint8_t id = add_basic_element(ctx->parent_id, ELEMENT_BARREL, ctx->x, ctx->y);
  if (id == 0xFF) {
    return err();
  }
  int val = 0;
  (void) extract_int_key(ctx->os, ctx->oe, "v", &val);
  if (val < 0) {
    val = 0;
  }
  /* v is selection index; max derives from child TEXT count at runtime */
  numeric_store(id, val, 0u);
  return 0;
}

/** Create a trigger element and allocate runtime tracking. */
static int handle_create_trigger(const element_create_ctx_t* ctx)
{
  if (!ctx) {
    return err();
  }
  uint8_t id = add_basic_element(ctx->parent_id, ELEMENT_TRIGGER, ctx->x, ctx->y);
  if (id == 0xFF) {
    return err();
  }
  ur_trigger_state_t* ts = ur_trigger_get_or_add(&g_protocol_state.runtime, id);
  if (!ts) {
    return err();
  }
  /* version already 0 on creation */
  g_protocol_state.trigger_count++; /* maintain heuristic count for status/debug */
  return 0;
}

/** Update an existing text element's attributes. */
static int handle_update_text(uint8_t id, const element_update_ctx_t* ctx)
{
  if (!ctx) {
    return 0;
  }
  char tb[21]; /* cap <= 20 + NUL */
  if (extract_string_key(ctx->os, ctx->oe, "tx", tb, sizeof(tb)) == 0) {
    (void) ui_attr_update_text(&g_protocol_state.runtime, id, tb);
  }
  /* TEXT does not mark dirty on update */
  return 0;
}

/** Update an existing barrel element's value. */
static int handle_update_barrel(uint8_t id, const element_update_ctx_t* ctx)
{
  if (!ctx) {
    return 0;
  }
  int v = 0;
  (void) extract_int_key(ctx->os, ctx->oe, "v", &v);
  numeric_set_value(id, v);
  /* Interactive widgets typically set change on commit; host updates are final.
     Keep change tracking minimal here to reduce ROM: don't mark; COMMIT redraw will reflect. */
  return 0;
}

static const element_handler_t k_element_handlers[] = {
  {ELEMENT_SCREEN,     handle_create_screen,  NULL},
  {ELEMENT_LIST_VIEW,  handle_create_list,    NULL},
  {ELEMENT_TEXT,       handle_create_text,    handle_update_text},
  {ELEMENT_BARREL,     handle_create_barrel,  handle_update_barrel},
  {ELEMENT_TRIGGER,    handle_create_trigger, NULL},
};

/** Find a handler entry for the given element type. */
static const element_handler_t* find_handler(uint8_t type)
{
  for (size_t i = 0; i < (sizeof(k_element_handlers) / sizeof(k_element_handlers[0])); i++) {
    if (k_element_handlers[i].type == type) {
      return &k_element_handlers[i];
    }
  }
  return NULL;
}

/* Single element handler extracted from JSON parser */
/** Dispatch one JSON element object to create/update handlers. */
static int handle_element_object(const char* os, const char* oe)
{
  char type_buf[16] = {0};
  extract_string_key(os, oe, "t", type_buf, sizeof(type_buf));
  if (type_buf[0] == 'h' && type_buf[1] == '\0') {
    int n = 0;
    if (extract_int_key(os, oe, "n", &n) != 0) {
      return err();
    }
    if (n <= 0 || n > 255) {
      return err();
    }
    int res = protocol_reserve_element_storage((uint8_t) n);
    if (res != RES_OK) {
      return res;
    }
    g_protocol_state.header_seen = 1u;
    return 0;
  }
  if (g_protocol_state.element_capacity == 0u) {
    return RES_BAD_STATE;
  }
  uint8_t tcode  = map_type_key(type_buf);
  int     parent = -1;
  extract_int_key(os, oe, "p", &parent);
  /* Update-by-id path: if 'e' present, treat as an update to an existing element. */
  int upd_id = -1;
  if (extract_int_key(os, oe, "e", &upd_id) == 0 && upd_id >= 0 &&
      upd_id < g_protocol_state.element_count) {
    element_t* uel = &g_protocol_state.elements[(uint8_t) upd_id];
    /* Optional type check: if provided and inconsistent, ignore. */
    if (type_buf[0] != '\0' && map_type_key(type_buf) != uel->type) {
      return 0; /* ignore mismatched type in update */
    }
    const element_handler_t* handler = find_handler(uel->type);
    if (handler != NULL && handler->update != NULL) {
      element_update_ctx_t uctx = {.os = os, .oe = oe};
      return handler->update((uint8_t) upd_id, &uctx);
    }
    /* Unsupported update target type: ignore */
    return 0;
  }
  uint8_t parent_id = INVALID_ELEMENT_ID;
  if (parent >= 0 && parent < g_protocol_state.element_count) {
    parent_id = (uint8_t) parent;
  }
  int x = 0;
  int y = 0;
  (void) extract_int_key(os, oe, "x", &x);
  (void) extract_int_key(os, oe, "y", &y);

  const element_handler_t* handler = find_handler(tcode);
  if (handler && handler->create) {
    element_create_ctx_t cctx = {
      .parent_id = parent_id,
      .x         = x,
      .y         = y,
      .type_code = tcode,
      .os        = os,
      .oe        = oe,
    };
    return handler->create(&cctx);
  }
  return 0;
}

/* Command implementations */
/* Command implementations */
