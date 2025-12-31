/* Minimal gfx_master application: initializes SPI in master mode and performs basic testing */
#include <string.h>

#include "ch32fun.h"
#include "demo_json.h"
#include "master_spi.h"
#include "ui_buttons.h"

#ifndef MASTER_SPI_PIN_FORCE_HIGH
#define MASTER_SPI_PIN_FORCE_HIGH 0
#endif

#ifndef MASTER_ENABLE_LOCAL_BUTTONS
#define MASTER_ENABLE_LOCAL_BUTTONS 0
#endif

/** Drive SPI-related pins high as GPIO outputs for hardware verification. */
static void master_spi_pins_force_high(void)
{
  /* Ensure GPIOC + AFIO clocks are on for pin configuration. */
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
  funPinMode(PC0, GPIO_CFGLR_OUT_50Mhz_PP); /* CS */
  funPinMode(PC5, GPIO_CFGLR_OUT_50Mhz_PP); /* SCK */
  funPinMode(PC6, GPIO_CFGLR_OUT_50Mhz_PP); /* MOSI */
  funPinMode(PC7, GPIO_CFGLR_OUT_50Mhz_PP); /* MISO */
  funDigitalWrite(PC0, 1);
  funDigitalWrite(PC5, 1);
  funDigitalWrite(PC6, 1);
  funDigitalWrite(PC7, 1);
  while (1) {
    ;
  }
}

/** Busy-wait delay in milliseconds for master-side bring-up. */
static void delay_ms(uint32_t ms)
{
  /* CH32V003 @ 48MHz: 48,000 cycles per ms
   * Loop unrolling to reduce for-loop overhead
   * Each unrolled block: 3x16 NOPs + minimal loop overhead
   */
  const uint32_t iterations_per_ms = 1000;
  uint32_t cnt = ms * iterations_per_ms;
  for (uint32_t i = 0; i < cnt; i++) {
      /* Unrolled 3x16 NOPs per iteration for efficiency */
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
  }
}

/* -------------------------------------------------------------------------- */
/* Command reader helpers (master side)                                       */
/* -------------------------------------------------------------------------- */

/** Local command IDs absent from master_spi.h (read-type commands). */
#ifndef SPI_CMD_GET_ELEMENT_STATE
#define SPI_CMD_GET_ELEMENT_STATE 0x22u
#endif
#ifndef SPI_CMD_GET_ERROR_LOG
#define SPI_CMD_GET_ERROR_LOG 0x23u
#endif
#ifndef SPI_CMD_CLEAR_ERROR_LOG
#define SPI_CMD_CLEAR_ERROR_LOG 0x24u
#endif
#ifndef SPI_CMD_SET_ACTIVE_SCREEN
#define SPI_CMD_SET_ACTIVE_SCREEN 0x10u
#endif
#ifndef SPI_CMD_SCROLL_TO_SCREEN
#define SPI_CMD_SCROLL_TO_SCREEN 0x21u
#endif
#ifndef SPI_CMD_JSON_ABORT
#define SPI_CMD_JSON_ABORT 0x03u
#endif
#ifndef SPI_CMD_GOTO_STANDBY
#define SPI_CMD_GOTO_STANDBY 0x50u
#endif
#ifndef STATUS_FLAG_DIRTY
#define STATUS_FLAG_DIRTY 0x02u
#endif
#ifndef ELEMENT_TRIGGER
#define ELEMENT_TRIGGER 14u
#endif

#define DEMO_TRIGGER_EID 28u
#define DEMO_TRG_DET_OVERLAY_SID 2u
#define DEMO_TRG_DET_OVERLAY_MS 1500u

/**
 * @brief Parsed payload for PING response.
 */
typedef struct MasterPingInfo {
  uint8_t rc;       /**< Return code (0 = OK) */
  uint8_t version;  /**< Protocol version */
  uint16_t caps;    /**< Capability bitfield (lo|hi) */
} MasterPingInfo;

/**
 * @brief Parsed payload for GET_STATUS.
 */
typedef struct MasterStatus {
  uint8_t rc;            /**< Return code (0 = OK) */
  uint8_t flags;         /**< flags bitfield */
  uint8_t element_count; /**< total elements */
  uint8_t screen_count;  /**< screens */
  uint8_t active_screen; /**< current screen index */
  uint8_t version;       /**< protocol version */
  uint8_t dirty_id;      /**< Most recent changed element id (valid when flags has dirty). */
} MasterStatus;

/**
 * @brief Error log entry (from GET_ERROR_LOG).
 */
typedef struct MasterErrEntry {
  uint8_t code;  /**< error code */
  uint8_t elem;  /**< element id */
  uint8_t info0; /**< extra info 0 */
  uint8_t info1; /**< extra info 1 */
} MasterErrEntry;

/**
 * @brief Execute PING and parse response.
 * @param out Pointer to output struct.
 * @return 0 on success, non-zero on protocol error.
 */
static int master_read_ping(MasterPingInfo* out)
{
  uint8_t resp[8] = {0};
  uint8_t rlen    = (uint8_t) sizeof(resp);
  int     r       = master_send_command(SPI_CMD_PING, (const uint8_t*) 0, 0, resp, &rlen);
  if ((r < 4) || (rlen < 4)) {
    return -1;
  }
  out->rc      = resp[0];
  out->version = resp[1];
  out->caps    = (uint16_t) ((uint16_t) resp[2] | ((uint16_t) resp[3] << 8));
  return (out->rc == 0u) ? 0 : (int) out->rc;
}

/**
 * @brief Execute GET_STATUS and parse response.
 * @param out Pointer to output struct.
 * @return 0 on success, non-zero on protocol error/RC.
 */
static int master_read_status(MasterStatus* out)
{
  uint8_t resp[16] = {0};
  uint8_t rlen     = (uint8_t) sizeof(resp);
  int     r        = master_send_command(SPI_CMD_GET_STATUS, (const uint8_t*) 0, 0, resp, &rlen);
  if ((r < 10) || (rlen < 10)) {
    return -1;
  }
  out->rc            = resp[0];
  out->flags         = resp[1];
  out->element_count = resp[2];
  out->screen_count  = resp[3];
  out->active_screen = resp[4];
  out->version       = resp[5];
  out->dirty_id      = resp[6];
  return (out->rc == 0u) ? 0 : (int) out->rc;
}

/**
 * @brief Execute GET_ELEMENT_STATE for a given element id.
 * @param eid Element id to query.
 * @param out_type Receives returned element type token.
 * @param out_data Buffer to receive the type-specific payload (excludes RC and type).
 * @param inout_len In: capacity of out_data; Out: actual bytes written.
 * @return 0 on success, non-zero on RC or protocol error.
 */
static int master_read_element_state(uint8_t eid, uint8_t* out_type, uint8_t* out_data, uint8_t* inout_len)
{
  uint8_t req[1]  = {eid};
  uint8_t resp[48] = {0};
  uint8_t rlen     = (uint8_t) sizeof(resp);
  int     r        = master_send_command(SPI_CMD_GET_ELEMENT_STATE, req, 1, resp, &rlen);
  if ((r < 3) || (rlen < 3)) {
    return -1;
  }
  if (out_type != 0) {
    *out_type = resp[1];
  }
  uint8_t payload_len = (uint8_t) (rlen - 2u);
  uint8_t to_copy     = (payload_len < *inout_len) ? payload_len : *inout_len;
  if ((out_data != 0) && (to_copy > 0u)) {
    (void) memcpy(out_data, &resp[2], to_copy);
  }
  *inout_len = to_copy;
  return (resp[0] == 0u) ? 0 : (int) resp[0];
}

/**
 * @brief Execute GET_ERROR_LOG and parse up to max_entries.
 * @param out_count Receives number of entries returned (clamped to max_entries).
 * @param entries Output array to fill.
 * @param max_entries Maximum entries that can be written to entries[].
 * @return 0 on success, non-zero on RC or protocol error.
 */
static int master_read_error_log(uint8_t* out_count, MasterErrEntry* entries, uint8_t max_entries)
{
  uint8_t resp[1 + 1 + 4 * 8] = {0}; /* RC + count + up to 8 entries */
  uint8_t rlen                 = (uint8_t) sizeof(resp);
  int     r                    = master_send_command(SPI_CMD_GET_ERROR_LOG, (const uint8_t*) 0, 0, resp, &rlen);
  if ((r < 2) || (rlen < 2)) {
    return -1;
  }
  if (resp[0] != 0u) {
    return (int) resp[0];
  }
  uint8_t count = resp[1];
  uint8_t avail = (uint8_t) ((rlen - 2u) / 4u);
  if (count > avail) {
    count = avail;
  }
  if (count > max_entries) {
    count = max_entries;
  }
  for (uint8_t i = 0; i < count; i++) {
    uint8_t off       = (uint8_t) (2u + (uint8_t) (i * 4u));
    entries[i].code   = resp[off + 0];
    entries[i].elem   = resp[off + 1];
    entries[i].info0  = resp[off + 2];
    entries[i].info1  = resp[off + 3];
  }
  *out_count = count;
  return 0;
}

/**
 * @brief Clear error log (RC only).
 * @return 0 on success, non-zero on RC or protocol error.
 */
static int master_clear_error_log(void)
{
  uint8_t resp[2] = {0};
  uint8_t rlen    = (uint8_t) sizeof(resp);
  int     r       = master_send_command(SPI_CMD_CLEAR_ERROR_LOG, (const uint8_t*) 0, 0, resp, &rlen);
  if ((r < 1) || (rlen < 1)) {
    return -1;
  }
  return (resp[0] == 0u) ? 0 : (int) resp[0];
}

/** Set active screen. */
static inline int master_set_active_screen(uint8_t screen_id)
{
  uint8_t pl[1] = {screen_id};
  uint8_t rc[1] = {0};
  uint8_t rl    = sizeof(rc);
  int     r     = master_send_command(SPI_CMD_SET_ACTIVE_SCREEN, pl, 1, rc, &rl);
  return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
}

/** Scroll to screen (simple form). */
static inline int master_scroll_to_screen(uint8_t screen_id)
{
  uint8_t pl[1] = {screen_id};
  uint8_t rc[1] = {0};
  uint8_t rl    = sizeof(rc);
  int     r     = master_send_command(SPI_CMD_SCROLL_TO_SCREEN, pl, 1, rc, &rl);
  return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
}

/** Abort ongoing JSON provisioning (safety). */
static inline int master_json_abort(void)
{
  uint8_t rc[1] = {0};
  uint8_t rl    = sizeof(rc);
  int     r     = master_send_command(SPI_CMD_JSON_ABORT, (const uint8_t*) 0, 0, rc, &rl);
  return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
}

#if MASTER_ENABLE_LOCAL_BUTTONS
/** Forward input event (index 0..5, event 0 release / 1 press). */
static inline int master_input_event(uint8_t index, uint8_t event)
{
  uint8_t pl[2] = {index, event};
  uint8_t rc[1] = {0};
  uint8_t rl    = sizeof(rc);
  int     r     = master_send_command(SPI_CMD_INPUT_EVENT, pl, 2, rc, &rl);
  return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
}
#endif

/** Enter standby (no response expected). */
static inline int master_goto_standby(void)
{
  return master_send_command_no_response(SPI_CMD_GOTO_STANDBY, (const uint8_t*) 0, 0);
}

/**
 * @brief Helper to send a simple command ignoring returned payload.
 * @param cmd Command opcode.
 * @param pl Pointer to payload (can be NULL if plen==0).
 * @param plen Payload length.
 */
static void send_simple(uint8_t cmd, const uint8_t* pl, uint8_t plen)
{
  uint8_t resp[32] = {0};
  uint8_t rlen     = sizeof(resp);
  (void) master_send_command(cmd, pl, plen, resp, &rlen);
}

/**
 * @brief Show popup by id with optional duration and flags.
 * @param popup_id Element id of the popup
 * @param duration_ms Duration in ms (0 for default)
 * @param mask_input 0/1 to gate inputs while visible
 */
static inline void master_show_overlay(uint8_t screen_id, uint16_t duration_ms, uint8_t mask_input)
{
  if (duration_ms == 0) {
    uint8_t pl[1] = {screen_id};
    send_simple(SPI_CMD_SHOW_OVERLAY, pl, 1);
    return;
  }
  uint8_t pl[4];
  pl[0] = screen_id;
  pl[1] = (uint8_t) (duration_ms & 0xFF);
  pl[2] = (uint8_t) ((duration_ms >> 8) & 0xFF);
  pl[3] = (uint8_t) (mask_input ? 1u : 0u);
  send_simple(SPI_CMD_SHOW_OVERLAY, pl, 4);
}

/** SHOW_OVERLAY with RC parsing. */
static inline int master_show_overlay_rc(uint8_t screen_id, uint16_t duration_ms, uint8_t mask_input)
{
  uint8_t rc[1] = {0};
  uint8_t rl    = sizeof(rc);
  if (duration_ms == 0) {
    uint8_t pl[1] = {screen_id};
    int     r     = master_send_command(SPI_CMD_SHOW_OVERLAY, pl, 1, rc, &rl);
    return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
  }
  uint8_t pl[4];
  pl[0] = screen_id;
  pl[1] = (uint8_t) (duration_ms & 0xFF);
  pl[2] = (uint8_t) ((duration_ms >> 8) & 0xFF);
  pl[3] = (uint8_t) (mask_input ? 1u : 0u);
  int r = master_send_command(SPI_CMD_SHOW_OVERLAY, pl, 4, rc, &rl);
  return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
}



/**
 * @brief Split a concatenated sequence of top-level JSON element objects and transmit each.
 *
 * Input format: back-to-back JSON objects without commas / array wrapper, e.g.:
 *   { ...screen... }{ ...text... }{ ...number... }
 *
 * Transmission rules:
 * - First object: sets JSON_FLAG_HEAD (clears remote state)
 * - Last object: sets JSON_FLAG_COMMIT (requests render)
 * - Intermediate objects: no flags
 *
 * Safety limits: objects larger than 120 bytes are skipped to avoid buffer overflow.
 */
static void send_combined_elements(const char* json)
{
  const char* p     = json;
  const char* end   = p + strlen((const char*) json);
  int         index = 0;
  while (p < end) {
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
      p++;
    }
    if (p >= end) {
      break;
    }
    if (*p != '{') {
      p++;
      continue;
    }
    const char* obj_start = p;
    int         depth     = 0;
    const char* q         = p;
    const char* obj_end   = NULL;
    while (q < end) {
      char c = *q;
      if (c == '{') {
        depth++;
      } else if (c == '}') {
        depth--;
        if (depth == 0) {
          obj_end = q;
          q++;
          break;
        }
      }
      q++;
    }
    if (!obj_end) {
      break;
    }
    uint16_t olen     = (uint16_t) (obj_end - obj_start + 1);
    uint16_t max_json = (uint16_t) (SPI_BUFFER_SIZE - 6);
    if (olen > max_json) {
      p = obj_end;
      continue;
    }
    uint8_t flags = 0;
    if (index == 0) {
      flags |= JSON_FLAG_HEAD;
    }
    /* look ahead */
    int         more = 0;
    const char* look = q;
    while (look < end) {
      if (*look == '{') {
        more = 1;
        break;
      }
      if (*look == 0) {
        break;
      }
      look++;
    }
    if (!more) {
      flags |= JSON_FLAG_COMMIT;
    }
    uint8_t buf[1 + 96];
    buf[0] = flags;
    memcpy(&buf[1], obj_start, olen);
    send_simple(SPI_CMD_JSON, buf, (uint8_t) (1 + olen));
    delay_ms(10);
    index++;
    p = q;
  }
}

/** Send a single JSON object with explicit flags; returns RC. */
static inline int master_json_send_object(const char* json_obj, uint8_t flags)
{
  uint16_t olen     = (uint16_t) strlen(json_obj);
  uint16_t max_json = (uint16_t) (SPI_BUFFER_SIZE - 6);
  if (olen == 0 || olen > max_json) {
    return -1;
  }
  uint8_t buf[1 + 96];
  buf[0] = flags;
  memcpy(&buf[1], json_obj, olen);
  uint8_t rc[1] = {0};
  uint8_t rl    = sizeof(rc);
  int     r     = master_send_command(SPI_CMD_JSON, buf, (uint8_t) (1 + olen), rc, &rl);
  return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
}

/** Scroll to screen with initial offset and target id. */
static inline int master_scroll_to_screen_with_offset(int16_t offset, uint8_t screen_id)
{
  uint8_t pl[3];
  pl[0] = (uint8_t) (offset & 0xFF);
  pl[1] = (uint8_t) ((offset >> 8) & 0xFF);
  pl[2] = screen_id;
  uint8_t rc[1] = {0};
  uint8_t rl    = sizeof(rc);
  int     r     = master_send_command(SPI_CMD_SCROLL_TO_SCREEN, pl, 3, rc, &rl);
  return (r >= 1 && rl >= 1 && rc[0] == 0u) ? 0 : -1;
}

#if MASTER_ENABLE_LOCAL_BUTTONS
/* -------------------------------------------------------------------------- */
/* Local button handling (master -> slave via SPI)                            */
/* -------------------------------------------------------------------------- */

/** Sentinel value to disable a button slot when no GPIO is wired. */
#ifndef MB_BUTTON_UNUSED
#define MB_BUTTON_UNUSED 0xFFu
#endif

/* Default wiring matches reference carrier; override via build flags if needed. */
#ifndef MB_BUTTON_UP_PIN
#define MB_BUTTON_UP_PIN PD6
#endif
#ifndef MB_BUTTON_DOWN_PIN
#define MB_BUTTON_DOWN_PIN PC4
#endif
#ifndef MB_BUTTON_OK_PIN
#define MB_BUTTON_OK_PIN PD4
#endif
#ifndef MB_BUTTON_BACK_PIN
#define MB_BUTTON_BACK_PIN PC3
#endif
#ifndef MB_BUTTON_LEFT_PIN
#define MB_BUTTON_LEFT_PIN PD2
#endif
#ifndef MB_BUTTON_RIGHT_PIN
#define MB_BUTTON_RIGHT_PIN PD5
#endif

static const uint8_t master_button_pins[UI_BUTTON_COUNT] = {
  [UI_BUTTON_UP]    = MB_BUTTON_UP_PIN,
  [UI_BUTTON_DOWN]  = MB_BUTTON_DOWN_PIN,
  [UI_BUTTON_OK]    = MB_BUTTON_OK_PIN,
  [UI_BUTTON_BACK]  = MB_BUTTON_BACK_PIN,
  [UI_BUTTON_LEFT]  = MB_BUTTON_LEFT_PIN,
  [UI_BUTTON_RIGHT] = MB_BUTTON_RIGHT_PIN,
};

static uint8_t master_button_prev[UI_BUTTON_COUNT];

/** Configure optional local GPIO buttons for master-side input forwarding. */
static void master_buttons_setup(void)
{
  for (uint8_t i = 0; i < UI_BUTTON_COUNT; ++i) {
    uint8_t pin = master_button_pins[i];
    if (pin == MB_BUTTON_UNUSED) {
      master_button_prev[i] = 0u;
      continue;
    }
    funPinMode(pin, GPIO_CNF_IN_FLOATING);
    master_button_prev[i] = funDigitalRead(pin);
  }
}

/** Poll local buttons and forward release events to the slave. */
static void master_buttons_poll(void)
{
  for (uint8_t i = 0; i < UI_BUTTON_COUNT; ++i) {
    uint8_t pin = master_button_pins[i];
    if (pin == MB_BUTTON_UNUSED) {
      continue;
    }
    uint8_t value = funDigitalRead(pin);
    if ((master_button_prev[i] != 0u) && (value == 0u)) {
      (void) master_input_event(i, 0u);
    }
    master_button_prev[i] = value;
  }
}
#endif

int main(void)
{
  SystemInit();
/* Master-only: lower HCLK by changing HPRE after SystemInit.
   Requires FUNCONF_USE_CLK_SEC=0 to avoid CSS-triggered NMI. */
#ifdef MASTER_HPRE
  do {
    /* Clear HPRE[7:4] then apply MASTER_HPRE */
    uint32_t cfgr = RCC->CFGR0;
    cfgr &= ~(0xFu << 4);
    cfgr |= (MASTER_HPRE & (0xFu << 4));
    RCC->CFGR0 = cfgr;
  } while (0);
#endif
  funGpioInitAll();
#if MASTER_SPI_PIN_FORCE_HIGH
  master_spi_pins_force_high();
#endif

  /* Initialize SPI master */
  master_spi_init();

  /* Bring-up sequence */
  /* 1) Robust PING handshake before proceeding */
  {
    uint8_t ok = 0;
    while (!ok) {
      uint8_t resp[16] = {0};
      uint8_t rlen     = sizeof(resp);
      int     r        = master_send_command(SPI_CMD_PING, (const uint8_t*) 0, 0, resp, &rlen);
      /* Expect RC + version + caps_lo + caps_hi => 4 bytes */
      if (r == 4 && rlen >= 1 && resp[0] == 0x00) {
        ok = 1;
        break;
      }
      delay_ms(100);
    }
    if (!ok) {}
    /* proceed even if PING not confirmed to keep previous behavior */
  }
  /* 2) Provision elements for this scenario using header-provided demo JSON */
  delay_ms(100);
  send_combined_elements(demo_json_multi_flat);
  delay_ms(1000);
  /* 4) Periodic GET_STATUS */
#if MASTER_ENABLE_LOCAL_BUTTONS
  master_buttons_setup();
#endif
  uint8_t last_trigger_version = 0u;
  while (1) {
#if MASTER_ENABLE_LOCAL_BUTTONS
    master_buttons_poll();
#endif
    MasterStatus st;
    if (master_read_status(&st) == 0) {
      if (((st.flags & STATUS_FLAG_DIRTY) != 0u) && (st.dirty_id == DEMO_TRIGGER_EID)) {
        uint8_t type = 0u;
        uint8_t data[4] = {0u};
        uint8_t len = (uint8_t) sizeof(data);
        if (master_read_element_state(DEMO_TRIGGER_EID, &type, data, &len) == 0 &&
            type == ELEMENT_TRIGGER && len >= 1u) {
          uint8_t version = data[0];
          if (version != last_trigger_version) {
            last_trigger_version = version;
            master_show_overlay(DEMO_TRG_DET_OVERLAY_SID, DEMO_TRG_DET_OVERLAY_MS, 0u);
          }
        }
      }
    }
    delay_ms(100);
  }
}
