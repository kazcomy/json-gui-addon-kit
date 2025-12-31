/**
 * @file ui_protocol.h
 * @brief UI protocol core definitions (expanded spec implementation).
 */
#ifndef UI_PROTOCOL_H
#define UI_PROTOCOL_H
#include <stdint.h>

#ifndef SCREEN_ANIM_PIXELS_PER_FRAME
#define SCREEN_ANIM_PIXELS_PER_FRAME 8 /* 128px / 8px = 16 frames (~250ms @16ms frame) */
#endif


/**
 * @brief Screen transition animation runtime state.
 */
typedef struct {
  uint8_t active;      /**< Non-zero while a screen slide animation is running */
  uint8_t from_screen; /**< Source screen ordinal */
  uint8_t to_screen;   /**< Destination screen ordinal */
  int16_t offset_px;   /**< Accumulated offset in pixels (0..128) */
  int8_t  dir;         /**< +1 = slide left (next screen enters from right), -1 = slide right */
} screen_anim_state_t;

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
/* Always include hardware headers; native build substitutes stubs (tool/hal_stub). */
#include "ch32fun.h"
#include "element_types.h"
#include "ssd1306_driver.h"
#include "ui_runtime.h"
#include "ui_buttons.h"



/* ---------------- Command IDs ---------------- */
#define SPI_FRAME_START 0xAA
#define SPI_CMD_PING 0x00
/* Unified JSON element command */
#define SPI_CMD_JSON 0x01
#define SPI_CMD_JSON_ABORT 0x03
#define SPI_CMD_SET_ACTIVE_SCREEN 0x10
#define SPI_CMD_SET_CURSOR 0x13
#define SPI_CMD_NAVIGATE_MENU 0x14
#define SPI_CMD_SET_ANIMATION 0x16
#define SPI_CMD_GET_STATUS 0x20
#define SPI_CMD_SCROLL_TO_SCREEN 0x21
#define SPI_CMD_GET_ELEMENT_STATE 0x22
/* Overlay screen control (was popup): payload [screen_id,(dur_lo,dur_hi,flags optional)] */
#define SPI_CMD_SHOW_OVERLAY 0x30
/* Input events */
#define SPI_CMD_INPUT_EVENT 0x41
/* Power management */
#define SPI_CMD_GOTO_STANDBY 0x50
/* Debug utilities */

/* Limits */
#define SPI_BUFFER_SIZE 64
#ifndef SPI_RESP_SYNC0
#define SPI_RESP_SYNC0 0xA5u
#endif
#ifndef SPI_RESP_SYNC1
#define SPI_RESP_SYNC1 0x5Au
#endif
#ifndef INVALID_ELEMENT_ID
#define INVALID_ELEMENT_ID 0xFFu
#endif

/* Legacy JSON tokenizer structs removed: unified parsing no longer tokenizes. */

/* Error codes (parser) */
#define UI_ERR_TOKEN_UNTERMINATED_STRING (-1)
#define UI_ERR_TOKEN_INVALID_CHAR (-2)
#define UI_ERR_PARSE_EXPECT_OBJECT_START (-1)
#define UI_ERR_PARSE_EXPECT_COLON (-2)
#define UI_ERR_PARSE_EXPECT_ARRAY_START (-3)
#define UI_ERR_PARSE_CHILD_ARRAY_INVALID (-5)
#define UI_ERR_PARSE_OUT_OF_ELEMENTS (-6)
#define UI_ERR_PARSE_EXPECT_COLON_IN_OBJECT (-7)
#define UI_ERR_PARSE_UNEXPECTED_EOF_IN_OBJECT (-8)
#define UI_ERR_PARSE_UNKNOWN_KEY_COLON_MISSING (-10)

/* Element ref */
typedef ui_element_ref_t element_t;

/* Per-element animation system removed: list/screen/overlay animations are handled
 * by their dedicated runtime structs in ui_runtime (screen_anim_state_t locally, overlay_runtime_t).
 */

/* Digit editor runtime state now provided by ui_runtime (ur_digit_editor_t). */

/* Overlay screen roles */
typedef enum {
  OVERLAY_NONE = 0,
  OVERLAY_FULL = 1
} overlay_role_t;

/** @brief Navigation stack context types for hierarchical list navigation. */
typedef enum {
  NAV_CTX_LIST = 0u,         /**< Nested list entered via list item. */
  NAV_CTX_LOCAL_SCREEN = 1u  /**< Local screen entered via list item. */
} nav_context_type_t;

#ifndef NAV_STACK_MAX_DEPTH
#define NAV_STACK_MAX_DEPTH 4u
#endif

/** @brief Stack entry used to restore state when unwinding nested navigation. */
typedef struct {
  uint8_t type;               /**< nav_context_type_t discriminator. */
  uint8_t target_element;     /**< Entered element id (list or screen). */
  uint8_t return_list;        /**< Parent list id to restore focus/cursor. */
  uint8_t saved_cursor;       /**< Parent list cursor snapshot. */
  uint8_t saved_top;          /**< Parent list top_index snapshot. */
  uint8_t saved_focus;        /**< Focus element prior to push. */
  uint8_t saved_active_screen;/**< Root screen ordinal prior to push. */
} nav_stack_entry_t;

/* Overlay runtime state */
typedef struct {
  uint8_t  active_overlay_screen_id; /**< Element id of overlay screen; 0xFF if none */
  uint16_t remaining_ms;             /**< Remaining display time */
  uint8_t  mask_input;               /**< When non-zero, input is masked while overlay is active */
  uint8_t  prev_focus;               /**< Focus element prior to showing overlay */
} overlay_runtime_t;

/* List runtime states are provided by ui_runtime (ur_list_state_t). */

/* Input source mode */
#define INPUT_SRC_NONE 0u
#define INPUT_SRC_SPI 1u
#define INPUT_SRC_LOCAL 2u

/** Hook for deferred ISR work (SPI RX/TX processing in main loop). */
void protocol_service_deferred_ops(void);
/** Request a render; safe to call from ISR (sets a flag only). */
void protocol_request_render(void);
void protocol_overlay_cleared(void);
void ui_spi_rx_irq(void);

/* Global protocol state */
/* Defined here to leverage later typedefs such as element_t. */
typedef struct {
  uint8_t              active_screen;
  uint8_t              screen_count;
  uint8_t              element_count;
  uint8_t              element_capacity; /**< Allocated capacity for per-element tables. */
  element_t*           elements;         /**< Per-element parent/type table (shared arena). */
  /* Absolute positions per element (x,y). Stored in shared arena. */
  uint8_t*             pos_x;
  uint8_t*             pos_y;
  int16_t              scroll_x;
  uint8_t              initialized;
  uint8_t              status_dirty; /**< Non-zero when an element changed since last GET_STATUS. */
  uint8_t              status_dirty_id; /**< Most recent changed element id (0xFF if none). */
  /* List states stored in ui_runtime arena */
  /* Triggers moved to runtime arena-backed linked list (no MAX_TRIGGERS cap). */
  uint8_t              trigger_count; /* maintained for compatibility (count during build) */
  /* legacy array removed: triggers handled by ui_runtime arena */
  /* Shared arena for attributes (head) and runtime nodes (tail). */
  ui_runtime_t         runtime;
  overlay_runtime_t    overlay;
  uint8_t              protocol_version;
  uint32_t             capabilities;
  uint8_t              focused_element; /* 0xFF = none */
  uint8_t              input_source;    /* one of INPUT_SRC_* */
  nav_stack_entry_t    nav_stack[NAV_STACK_MAX_DEPTH]; /**< Navigation stack entries. */
  uint8_t              active_local_screen; /**< Current local screen id when nested (0xFF if none). */
  /* Hierarchical navigation depth: 0 = root (screen-level). When >0, LEFT/RIGHT must not slide
   * screens. */
  uint8_t nav_depth;
  screen_anim_state_t screen_anim; /**< Horizontal screen slide animation state */
  /* Edit blink state for visual feedback during editing */
  uint8_t             edit_blink_active;  /**< Non-zero when blink is active */
  uint8_t             edit_blink_phase;   /**< Current blink phase (0=dim, 1=bright) */
  uint8_t             edit_blink_counter; /**< Frame counter for blink timing */
  uint8_t             header_seen;        /**< Non-zero after header element is parsed (header is required). */
} protocol_state_t;

/* Popup auto event mask bits removed (overlay model no longer uses them). */

/* Generic response RC codes */
#define RC_OK 0x00
#define RC_BAD_LEN 0x01
#define RC_BAD_STATE 0x02
#define RC_UNKNOWN_ID 0x03
#define RC_RANGE 0x04
#define RC_INTERNAL 0x05
#define RC_PARSE_FAIL 0x0B
#define RC_NO_SPACE 0x0C
#define RC_STREAM_ERR 0x0D

/* Handler return sentinel: response already sent (do not auto RC frame) */
#define PROTOCOL_RESP_SENT 0x7F

/* API */
/** Initialize protocol state and hardware backends. */
void protocol_init(void);
/** Initialize SPI slave transport and DMA. */
void spi_init(void);
/* Compact SPI RX state for diagnostics: enc_len | (rx_cnt<<8) | (frame_ready<<16) | (tx_active<<24)
 */

/* Inter-byte RX timeout (ms). If no byte arrives for this duration while a packet is in progress,
 * the partial packet is dropped. Coarse granularity depends on get_system_time_ms() tick. */
#ifndef SPI_RX_INTERBYTE_TIMEOUT_MS
#define SPI_RX_INTERBYTE_TIMEOUT_MS 200u
#endif
/** Drop partial packets on inter-byte timeout; call periodically. */
void spi_rx_watchdog_poll(void);
/* Provided by main.c */
/** Monotonic system time in milliseconds (provided by main). */
uint32_t get_system_time_ms(void);
/** Dispatch one binary command frame. */
int handle_binary_command(uint8_t cmd, uint8_t* payload, uint8_t length);
/** Activate a screen by ordinal. */
int cmd_set_active_screen(uint8_t* payload, uint8_t length);
/** Set cursor for menus/lists. */
int cmd_set_cursor(uint8_t* payload, uint8_t length);
/** Navigate menu/list selection. */
int cmd_navigate_menu(uint8_t* payload, uint8_t length);
/** Report status and most recent changed element id. */
int cmd_get_status(uint8_t* payload, uint8_t length);
/** Scroll viewport to a specific screen or absolute offset. */
int cmd_scroll_to_screen(uint8_t* payload, uint8_t length);
/** Query element state for host synchronization. */
int cmd_get_element_state(uint8_t* payload, uint8_t length);
/** Show overlay screen with optional duration and input mask. */
int cmd_show_overlay(uint8_t* payload, uint8_t length);
/** Inject input event from host. */
int cmd_input_event(uint8_t* payload, uint8_t length);
/** Enter standby upon host request (no response sent). */
int cmd_goto_standby(uint8_t* payload, uint8_t length);
/* Unified JSON command (flags + single element JSON object) */
/** Process a compact JSON element update (single object). */
int cmd_json(uint8_t* p, uint8_t l);
/** Abort currently streaming/processing JSON (if any). */
int cmd_json_abort(uint8_t* p, uint8_t l);

#if defined(UNIT_TEST) || defined(UI_MEMCALC)
/** Apply one JSON element object with explicit flags (test/tool use only). */
int protocol_apply_json_object(const char* buf, uint8_t len, uint8_t flags);
#endif

/* JSON command flags (bit0=head(clear), bit1=commit(force render)) */
#define JSON_FLAG_HEAD 0x01u
#define JSON_FLAG_COMMIT 0x02u
/* GET_STATUS flags */
#define STATUS_FLAG_INITIALIZED 0x01u
#define STATUS_FLAG_DIRTY 0x02u
#define STATUS_FLAG_OVERLAY 0x04u
/** Mark an element as changed for GET_STATUS dirty reporting. */
void protocol_element_changed(uint8_t element_id);
/** Advance easing + list scroll animations; call every main loop iteration. */
void protocol_tick_animations(void);
/** Check if barrel element is currently being edited. */
uint8_t barrel_is_editing(uint8_t element_id);
/** Get allocated per-element capacity (0 if not initialized). */
uint8_t protocol_element_capacity(void);
/** Return overlay role for a screen element (OVERLAY_*). */
uint8_t protocol_screen_role(uint8_t element_id);

/** Render a whole screen immediately. */
/** Render a single 8px-high tile row (called by async driver). */
void render_screen_tile(uint8_t tile_y);
extern protocol_state_t g_protocol_state;
extern volatile uint8_t g_rx_path;
/** Set to 1 by cmd_goto_standby; polled by main loop to perform display_off and standby. */
extern volatile uint8_t g_request_standby;
/** Set to 1 when a render is requested (e.g., JSON commit or overlay clear). */
extern volatile uint8_t g_render_requested;

int16_t protocol_numeric_value(uint8_t element_id);
uint8_t protocol_numeric_aux(uint8_t element_id);

/* Focus API */
/** Set focus to specified element id. */
void    protocol_set_focus(uint8_t element_id);
/** Clear focus (no element focused). */
void    protocol_clear_focus(void);
/** Move focus to next focusable element. */
void    protocol_focus_next(void);
/** Move focus to previous focusable element. */
void    protocol_focus_prev(void);
/** Get currently focused element id (0xFF if none). */
uint8_t protocol_get_focused(void);
/** Determine whether the specified element is visible in the current navigation context. */
uint8_t protocol_is_element_visible(uint8_t element_id);
/* Auto-popup hooks are not used in the overlay model. */
/** Reset full protocol state to defaults. */
void protocol_reset_state(void);
/* Response helpers */
/** Send a response frame for a command. */
int     protocol_send_response(uint8_t cmd, const uint8_t* payload, uint8_t len);
/** Map internal result codes to protocol RC. */
uint8_t protocol_map_result_to_rc(int r);

/* Animation frame rate control */
#ifndef PROTOCOL_ANIM_FRAME_MS
#define PROTOCOL_ANIM_FRAME_MS 16 /* ~62.5 FPS baseline; adjust for power */
#endif
#ifndef LIST_ANIM_PIXELS_PER_FRAME
#define LIST_ANIM_PIXELS_PER_FRAME 1 /* rows are 8px high; 8 frames per row scroll */
#endif
/* Edit blink timing */
#ifndef EDIT_BLINK_PERIOD_FRAMES
#define EDIT_BLINK_PERIOD_FRAMES 30 /* ~500ms at 60fps for blink cycle */
#endif

#ifdef __cplusplus
}
#endif
#endif /* UI_PROTOCOL_H */
