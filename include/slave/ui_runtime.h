/**
 * @file ui_runtime.h
 * @brief Shared arena storage and runtime helpers for UI attributes/state.
 *
 * The arena is split logically:
 * - Head: per-element tables + attribute entries (append-only during provisioning)
 * - Tail: runtime nodes (lists/triggers/barrels) allocated on demand
 */
#ifndef UI_RUNTIME_H
#define UI_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Layout mode constants (renderer uses absolute only; kept for compatibility). */
#define LAYOUT_ABSOLUTE 0u
#define LAYOUT_HORIZONTAL 1u
#define LAYOUT_VERTICAL 2u
#define LAYOUT_GRID 3u

/* Arena capacity (bytes). Tune for RAM. */
#ifndef UI_ATTR_ARENA_CAP
#define UI_ATTR_ARENA_CAP 768u
#endif

/* Attribute tags. */
typedef enum {
  UI_ATTR_TAG_TEXT        = 0x10, /* len + bytes */
  UI_ATTR_TAG_SCREEN_ROLE = 0x11  /* role byte */
} ui_attr_tag_t;

/* -------------------------------------------------------------------------- */
/* Entry struct representations (packed)                                      */
/* These provide a readable mapping for code reviewers without altering the  */
/* binary layout previously described. They are not required by the public   */
/* API but help when inspecting arena contents or extending attribute types. */
/* -------------------------------------------------------------------------- */

#if defined(__GNUC__)
#define UI_ATTR_PACKED __attribute__((packed))
#else
#define UI_ATTR_PACKED
#endif

/* POSITION entries are no longer stored in arena. Each element owns a position
 * entry in the per-element tables allocated from the shared arena head. */

typedef struct UI_ATTR_PACKED {
  uint8_t tag;        /**< UI_ATTR_TAG_TEXT */
  uint8_t element_id; /**< Owning element id */
  uint8_t len;        /**< Allocated payload size in bytes INCLUDING NUL terminator (>=1) */
  uint8_t data[];     /**< Flexible array (size-1 bytes for text, followed by at least one NUL) */
} ui_attr_text_entry_t;

typedef struct UI_ATTR_PACKED {
  uint8_t tag;        /**< UI_ATTR_TAG_SCREEN_ROLE */
  uint8_t element_id; /**< Owning screen element id */
  uint8_t role;       /**< overlay_role_t value */
} ui_attr_screen_role_entry_t;

/* Size helper macros for skip logic (text remains variable). */
#define UI_ATTR_SIZE_TEXT_HDR        ((uint16_t)3u) /* tag + element_id + len */
#define UI_ATTR_SIZE_SCREEN_ROLE     ((uint16_t)3u) /* tag + element_id + role */

/** Compact element reference: parent id and type (packed). */
typedef struct {
  uint8_t parent_id; /**< Parent element id, or 0xFF for root. */
  uint8_t type;      /**< Element type (see element_types.h). */
} __attribute__((packed)) ui_element_ref_t;

static inline uint8_t calculate_text_width(const char* text, uint8_t font_size)
{
  if (!text) return 0;
  uint8_t len = 0u; while (text[len]) len++;
  return (uint8_t)(len * font_size);
}

/* -------------------------------------------------------------------------- */
/* Inline helpers for internal / diagnostic use                               */
/* These are header-only casts; implementation code may choose to ignore them */
/* to keep flash minimal.                                                     */
/* -------------------------------------------------------------------------- */
static inline ui_attr_text_entry_t* ui_attr_as_text(void* e) { return (ui_attr_text_entry_t*)e; }

#ifndef UR_INVALID_ELEMENT_ID
#define UR_INVALID_ELEMENT_ID 0xFFu
#endif

/** Offset type inside arena; 0 means NULL. */
typedef uint16_t ur_off_t;

/** Runtime arena context held inside protocol state. */
typedef struct ui_runtime_t {
  uint16_t head_used;            /**< Bytes consumed from arena head by tables/attributes */
  uint16_t used_tail;            /**< Bytes consumed from arena tail by runtime nodes */
  uint16_t attr_base;            /**< Offset to first attribute entry within arena */
  /* Linked list heads (offsets from base) */
  ur_off_t triggers_head_off;    /**< Head of trigger linked list (offset) */
  ur_off_t lists_head_off;       /**< Head of list-state linked list */
  ur_off_t barrels_head_off;     /**< Head of barrel-state linked list */
  uint8_t  arena[UI_ATTR_ARENA_CAP]; /**< Shared arena storage */
} ui_runtime_t;

/** Trigger node stored in arena; next is offset (little endian). */
typedef struct {
  uint8_t element_id;
  uint8_t version;
} ur_trigger_state_t;

typedef struct {
  uint16_t          next_off; /* 0 = null */
  ur_trigger_state_t st;      /* element_id + version */
} ur_trigger_node_t;

/* ---------------- List View runtime ---------------- */
typedef struct {
  uint8_t element_id;  /**< owning list element id */
  uint8_t cursor;      /**< selected row (index among child TEXT items) */
  uint8_t top_index;   /**< top visible row index */
  uint8_t visible_rows;/**< desired rows (1..6/8 depending on height) */
  uint8_t anim_active; /**< non-zero while animating */
  int8_t  anim_dir;    /**< -1 up, +1 down, 0 none */
  uint8_t anim_pix;    /**< 0..8 progress */
  uint8_t pending_top; /**< target after anim */
  uint8_t pending_cursor; /**< target after anim */
  uint8_t last_text_child; /**< Most recent TEXT child id during provisioning */
} ur_list_state_t;

typedef struct {
  uint16_t        next_off; /* 0 = null */
  ur_list_state_t st;
} ur_list_node_t;

/* ---------------- Barrel runtime ---------------- */
typedef struct {
  uint8_t element_id; /**< owning barrel element id */
  uint8_t aux;        /**< aux flags (bit7 edit, bit0..6 snapshot) */
  int16_t value;      /**< selection index */
} ur_barrel_state_t;

typedef struct {
  uint16_t          next_off; /* 0 = null */
  ur_barrel_state_t st;
} ur_barrel_node_t;

/* Small helpers (implemented in ui_runtime.c) */
void ur_init(ui_runtime_t* rt);
void* ur__ptr(ui_runtime_t* rt, ur_off_t off);
ur_off_t ur__off(ui_runtime_t* rt, void* p);
void* ur__alloc_tail(ui_runtime_t* rt, uint16_t size);

ur_trigger_state_t* ur_trigger_find(ui_runtime_t* rt, uint8_t element_id);
ur_trigger_state_t* ur_trigger_get_or_add(ui_runtime_t* rt, uint8_t element_id);

/* ---------------- List helpers ---------------- */
ur_list_state_t* ur_list_find(ui_runtime_t* rt, uint8_t element_id);
ur_list_state_t* ur_list_get_or_add(ui_runtime_t* rt, uint8_t element_id);

/* ---------------- Barrel helpers ---------------- */
ur_barrel_state_t* ur_barrel_find(ui_runtime_t* rt, uint8_t element_id);
ur_barrel_state_t* ur_barrel_get_or_add(ui_runtime_t* rt, uint8_t element_id);

/* ---------------- Attribute helpers (head allocation) ---------------- */
uint16_t ui_attr_get_memory_usage(ui_runtime_t* rt);
int ui_attr_store_text_with_cap(ui_runtime_t* rt,
                                uint8_t       element_id,
                                const char*   text,
                                uint8_t       capacity);
int ui_attr_store_text(ui_runtime_t* rt, uint8_t element_id, const char* text);
const char* ui_attr_get_text(ui_runtime_t* rt, uint8_t element_id);
int ui_attr_update_text(ui_runtime_t* rt, uint8_t element_id, const char* new_text);

int ui_attr_store_screen_role(ui_runtime_t* rt, uint8_t element_id, uint8_t role);
int ui_attr_get_screen_role(ui_runtime_t* rt, uint8_t element_id, uint8_t* out_role);

int ui_attr_store_position(ui_runtime_t* rt,
                           uint8_t       element_id,
                           uint8_t       x,
                           uint8_t       y,
                           uint8_t       font_size,
                           uint8_t       layout_type);
int ui_attr_get_position(ui_runtime_t* rt,
                         uint8_t       element_id,
                         uint8_t*      x,
                         uint8_t*      y,
                         uint8_t*      font_size,
                         uint8_t*      layout_type);

#ifdef __cplusplus
}
#endif

#endif /* UI_RUNTIME_H */
