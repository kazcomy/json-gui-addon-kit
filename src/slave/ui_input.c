/**
 * @file ui_input.c
 * @brief Input handling for UI (buttons/events).
 */
#include "ui_input.h"

#include "ssd1306_driver.h"
#include "status_codes.h"
#include "ui_focus.h"
#include "ui_numeric.h"
#include "ui_protocol.h"
#include "ui_runtime.h"
#include "ui_tree.h"

/* Optional hook; weak so builds without an override still link cleanly. */
__attribute__((weak)) void protocol_up_button_pressed(void);

/** Compute effective list window size based on display height and list position. */
static uint8_t list_effective_window(uint8_t list_eid, const ur_list_state_t* state)
{
  uint8_t desired = (state != NULL && state->visible_rows != 0u) ? state->visible_rows : 4u;
  uint16_t display_h = (uint16_t) ssd1306_height();
  if (display_h == 0u) {
    display_h = (uint16_t) SSD1306_HEIGHT;
  }
  uint8_t max_rows = (display_h >= 64u) ? 8u : 6u;
  if (desired > max_rows) {
    desired = max_rows;
  }
  uint8_t px = 0u;
  uint8_t py = 0u;
  uint8_t pf = 0u;
  uint8_t pl = 0u;
  if (ui_attr_get_position(&g_protocol_state.runtime, list_eid, &px, &py, &pf, &pl) != 0) {
    return desired;
  }
  if (pl != LAYOUT_ABSOLUTE) {
    return desired;
  }
  uint16_t base_y = (uint16_t) py;
  if (display_h <= base_y) {
    return 1u;
  }
  uint16_t remaining = (uint16_t) (display_h - base_y);
  uint8_t  avail     = (uint8_t) (remaining / (uint16_t) SSD1306_PAGE_HEIGHT);
  if (avail == 0u) {
    avail = 1u;
  }
  if (desired > avail) {
    desired = avail;
  }
  return desired;
}

/** Start edit-mode blink animation. */
static void protocol_edit_blink_start(void)
{
  g_protocol_state.edit_blink_active  = 1u;
  g_protocol_state.edit_blink_phase   = 1u;
  g_protocol_state.edit_blink_counter = 0u;
}

/** Return 1 if any barrel element is currently in edit mode. */
static uint8_t protocol_edit_blink_any_active(void)
{
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    if (g_protocol_state.elements[i].type == ELEMENT_BARREL) {
      if (barrel_is_editing(i)) {
        return 1u;
      }
    }
  }
  return 0u;
}

/** Stop blink animation when no editable elements remain active. */
static void protocol_edit_blink_stop_if_unused(void)
{
  if (g_protocol_state.edit_blink_active == 0u) {
    return;
  }
  if (protocol_edit_blink_any_active() != 0u) {
    return;
  }
  g_protocol_state.edit_blink_active  = 0u;
  g_protocol_state.edit_blink_phase   = 1u;
  g_protocol_state.edit_blink_counter = 0u;
}

/** Enter edit mode for a barrel element and snapshot its current value. */
static void barrel_begin_edit(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return;
  }
  int16_t cur  = protocol_numeric_value(eid);
  uint8_t snap = (cur < 0) ? 0u : (uint8_t) cur;
  numeric_set_aux(eid, (uint8_t) (0x80u | (snap & 0x7Fu)));
  protocol_edit_blink_start();
}

/** Cancel barrel edit and restore snapshot value. */
static void barrel_cancel_edit(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return;
  }
  uint8_t snap = (uint8_t) (protocol_numeric_aux(eid) & 0x7Fu);
  numeric_set_value(eid, snap);
  numeric_set_aux(eid, snap);
}

/** Commit barrel edit and stop blink if no other edits remain. */
static void barrel_commit_edit(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return;
  }
  int16_t v   = protocol_numeric_value(eid);
  uint8_t cur = (uint8_t) ((v < 0) ? 0 : (v & 0x7F));
  numeric_set_aux(eid, (uint8_t) (cur & 0x7Fu));
  protocol_edit_blink_stop_if_unused();
}

/** Count selectable text options for a barrel element. */
static uint8_t barrel_options_count(uint8_t barrel_id)
{
  uint8_t count = 0;
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    const element_t* el = &g_protocol_state.elements[i];
    if (el->parent_id == barrel_id && el->type == ELEMENT_TEXT) {
      count++;
    }
  }
  return count;
}

/** UI actions derived from button release events. */
typedef enum {
  UI_ACTION_UP = 0,
  UI_ACTION_DOWN,
  UI_ACTION_LEFT,
  UI_ACTION_RIGHT,
  UI_ACTION_OK,
  UI_ACTION_BACK,
  UI_ACTION_INVALID
} ui_action_t;

/** Focus kinds used by the input state machine. */
typedef enum {
  UI_FOCUS_NONE = 0,
  UI_FOCUS_LIST,
  UI_FOCUS_BARREL,
  UI_FOCUS_TRIGGER,
  UI_FOCUS_OTHER
} ui_focus_kind_t;

/** Navigation context of the current view stack. */
typedef enum {
  UI_NAV_ROOT = 0,
  UI_NAV_LIST,
  UI_NAV_LOCAL_SCREEN
} ui_nav_ctx_t;

/** Cached input context resolved from protocol state. */
typedef struct {
  uint8_t         focused_id;
  ui_focus_kind_t focus_kind;
  uint8_t         barrel_editing;
  ui_nav_ctx_t    nav_ctx;
  uint8_t         nav_target;
} ui_input_ctx_t;

/** List-row action classification for OK handling. */
typedef enum {
  LIST_ROW_NONE = 0,
  LIST_ROW_INLINE_BARREL,
  LIST_ROW_NESTED_LIST,
  LIST_ROW_LOCAL_SCREEN
} list_row_action_t;

/* Forward declarations for helpers referenced before definition. */
static void barrel_focus_parent_list(uint8_t barrel_id, uint8_t restore_row);

/** Convert a button index into an input action token. */
static ui_action_t ui_action_from_button(uint8_t button)
{
  switch (button) {
    case UI_BUTTON_UP: return UI_ACTION_UP;
    case UI_BUTTON_DOWN: return UI_ACTION_DOWN;
    case UI_BUTTON_LEFT: return UI_ACTION_LEFT;
    case UI_BUTTON_RIGHT: return UI_ACTION_RIGHT;
    case UI_BUTTON_OK: return UI_ACTION_OK;
    case UI_BUTTON_BACK: return UI_ACTION_BACK;
    default: return UI_ACTION_INVALID;
  }
}

/** Resolve the focus kind and barrel edit state for a focused element. */
static ui_focus_kind_t ui_focus_kind_from_element(uint8_t focused_id, uint8_t* out_barrel_editing)
{
  if (out_barrel_editing != NULL) {
    *out_barrel_editing = 0u;
  }
  if (focused_id == INVALID_ELEMENT_ID || focused_id >= g_protocol_state.element_count) {
    return UI_FOCUS_NONE;
  }
  uint8_t type = g_protocol_state.elements[focused_id].type;
  switch (type) {
    case ELEMENT_LIST_VIEW: return UI_FOCUS_LIST;
    case ELEMENT_BARREL:
      if (out_barrel_editing != NULL) {
        *out_barrel_editing = barrel_is_editing(focused_id);
      }
      return UI_FOCUS_BARREL;
    case ELEMENT_TRIGGER: return UI_FOCUS_TRIGGER;
    default: return UI_FOCUS_OTHER;
  }
}

/** Resolve current navigation context and top target element id. */
static ui_nav_ctx_t ui_nav_context(uint8_t* out_target)
{
  if (out_target != NULL) {
    *out_target = INVALID_ELEMENT_ID;
  }
  if (g_protocol_state.nav_depth == 0u) {
    return UI_NAV_ROOT;
  }
  uint8_t top = (uint8_t) (g_protocol_state.nav_depth - 1u);
  if (top >= NAV_STACK_MAX_DEPTH) {
    return UI_NAV_ROOT;
  }
  if (out_target != NULL) {
    *out_target = g_protocol_state.nav_stack[top].target_element;
  }
  return (g_protocol_state.nav_stack[top].type == (uint8_t) NAV_CTX_LOCAL_SCREEN)
           ? UI_NAV_LOCAL_SCREEN
           : UI_NAV_LIST;
}

/** Collect the current input context for the state machine. */
static ui_input_ctx_t ui_input_ctx_collect(void)
{
  ui_input_ctx_t ctx;
  ctx.focused_id   = g_protocol_state.focused_element;
  ctx.focus_kind   = ui_focus_kind_from_element(ctx.focused_id, &ctx.barrel_editing);
  ctx.nav_ctx      = ui_nav_context(&ctx.nav_target);
  return ctx;
}

/** Handle screen slide input; returns 1 if handled. */
static uint8_t handle_screen_slide(ui_action_t action)
{
  if (g_protocol_state.nav_depth != 0u) {
    return 0u;
  }
  if (action == UI_ACTION_LEFT) {
    if (g_protocol_state.active_screen == 0u) {
      return 1u;
    }
    screen_anim_state_t* sa = &g_protocol_state.screen_anim;
    if (sa->active) {
      g_protocol_state.active_screen = sa->to_screen;
    }
    uint8_t target = (uint8_t) (g_protocol_state.active_screen - 1u);
    sa->active      = 1u;
    sa->from_screen = g_protocol_state.active_screen;
    sa->to_screen   = target;
    sa->offset_px   = 0;
    sa->dir         = -1;
    g_protocol_state.scroll_x      = (int16_t) sa->from_screen * 128;
    g_protocol_state.active_screen = target;
    protocol_clear_focus();
    return 1u;
  }
  if (action == UI_ACTION_RIGHT) {
    if ((uint8_t) (g_protocol_state.active_screen + 1u) >= g_protocol_state.screen_count) {
      return 1u;
    }
    screen_anim_state_t* sa = &g_protocol_state.screen_anim;
    if (sa->active) {
      g_protocol_state.active_screen = sa->to_screen;
    }
    uint8_t target = (uint8_t) (g_protocol_state.active_screen + 1u);
    sa->active      = 1u;
    sa->from_screen = g_protocol_state.active_screen;
    sa->to_screen   = target;
    sa->offset_px   = 0;
    sa->dir         = +1;
    g_protocol_state.scroll_x      = (int16_t) sa->from_screen * 128;
    g_protocol_state.active_screen = target;
    protocol_clear_focus();
    return 1u;
  }
  return 0u;
}

/** Move a list cursor up/down and update scroll animation if needed. */
static void list_move_cursor(uint8_t list_id, int8_t dir)
{
  ur_list_state_t* ls = ur_list_get_or_add(&g_protocol_state.runtime, list_id);
  if (ls == NULL) {
    return;
  }
  uint8_t row_count = list_row_count(list_id);
  if (row_count == 0u) {
    ls->cursor    = 0u;
    ls->top_index = 0u;
    return;
  }
  if (ls->cursor >= row_count) {
    ls->cursor = (uint8_t) (row_count - 1u);
  }
  uint8_t window = list_effective_window(list_id, ls);
  if (window == 0u) {
    window = 1u;
    uint8_t max_top = (row_count > window) ? (uint8_t) (row_count - window) : 0u;
    ls->top_index   = max_top;
  }
  if (dir < 0) {
    if (!ls->anim_active && ls->cursor > 0u) {
      uint8_t new_cursor = (uint8_t) (ls->cursor - 1u);
      if (new_cursor < ls->top_index) {
        uint8_t new_top = (ls->top_index > 0u) ? (uint8_t) (ls->top_index - 1u) : 0u;
        ls->anim_active    = 1u;
        ls->anim_dir       = -1;
        ls->anim_pix       = 0u;
        ls->pending_cursor = new_cursor;
        ls->pending_top    = new_top;
      } else {
        ls->cursor = new_cursor;
      }
    }
  } else {
    if (!ls->anim_active && (uint8_t) (ls->cursor + 1u) < row_count) {
      uint8_t new_cursor = (uint8_t) (ls->cursor + 1u);
      if (new_cursor >= (uint8_t) (ls->top_index + window)) {
        ls->anim_active    = 1u;
        ls->anim_dir       = +1;
        ls->anim_pix       = 0u;
        ls->pending_cursor = new_cursor;
        ls->pending_top    = (uint8_t) (ls->top_index + 1u);
      } else {
        ls->cursor = new_cursor;
      }
    }
  }
}

/** Resolve the selected text element for a list (returns 0 if none). */
static uint8_t list_selected_text(uint8_t list_id, uint8_t* out_text_id)
{
  if (out_text_id != NULL) {
    *out_text_id = INVALID_ELEMENT_ID;
  }
  ur_list_state_t* ls = ur_list_get_or_add(&g_protocol_state.runtime, list_id);
  if (ls == NULL) {
    return 0u;
  }
  uint8_t row_count = list_row_count(list_id);
  if (row_count == 0u) {
    ls->cursor    = 0u;
    ls->top_index = 0u;
    return 0u;
  }
  if (ls->cursor >= row_count) {
    ls->cursor = (uint8_t) (row_count - 1u);
  }
  uint8_t child = list_child_by_index(list_id, ls->cursor);
  if (child == INVALID_ELEMENT_ID) {
    return 0u;
  }
  if (out_text_id != NULL) {
    *out_text_id = child;
  }
  return 1u;
}

/** Find a nested list child under a text element. */
static uint8_t list_find_nested_list(uint8_t text_id)
{
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    if (g_protocol_state.elements[i].parent_id == text_id &&
        g_protocol_state.elements[i].type == ELEMENT_LIST_VIEW) {
      return i;
    }
  }
  return INVALID_ELEMENT_ID;
}

/** Resolve the OK action target for the selected list row. */
static list_row_action_t list_resolve_row_action(uint8_t list_id, uint8_t* out_target)
{
  if (out_target != NULL) {
    *out_target = INVALID_ELEMENT_ID;
  }
  uint8_t text_id = INVALID_ELEMENT_ID;
  if (list_selected_text(list_id, &text_id) == 0u) {
    return LIST_ROW_NONE;
  }
  uint8_t inline_barrel = text_inline_barrel_id(text_id);
  if (inline_barrel != INVALID_ELEMENT_ID) {
    if (out_target != NULL) {
      *out_target = inline_barrel;
    }
    return LIST_ROW_INLINE_BARREL;
  }
  uint8_t nested_list = list_find_nested_list(text_id);
  if (nested_list != INVALID_ELEMENT_ID) {
    if (out_target != NULL) {
      *out_target = nested_list;
    }
    return LIST_ROW_NESTED_LIST;
  }
  uint8_t local_screen = protocol_text_local_screen(text_id);
  if (local_screen != INVALID_ELEMENT_ID) {
    if (out_target != NULL) {
      *out_target = local_screen;
    }
    return LIST_ROW_LOCAL_SCREEN;
  }
  return LIST_ROW_NONE;
}

/** Handle inline barrel selection from a list row. */
static void list_handle_inline_barrel(uint8_t barrel_id)
{
  protocol_set_focus(barrel_id);
  if (!barrel_is_editing(barrel_id)) {
    barrel_begin_edit(barrel_id);
    return;
  }
  barrel_commit_edit(barrel_id);
  protocol_element_changed(barrel_id);
  barrel_focus_parent_list(barrel_id, 0u);
}

/** Handle OK action on a list view element. */
static void list_handle_ok(uint8_t list_id)
{
  uint8_t target = INVALID_ELEMENT_ID;
  list_row_action_t action = list_resolve_row_action(list_id, &target);
  switch (action) {
    case LIST_ROW_INLINE_BARREL:
      list_handle_inline_barrel(target);
      break;
    case LIST_ROW_NESTED_LIST:
      (void) nav_push_list(list_id, target);
      break;
    case LIST_ROW_LOCAL_SCREEN:
      (void) nav_push_local_screen(list_id, target);
      break;
    default:
      break;
  }
}

/** Restore focus to a parent list (optionally restoring row visibility). */
static void barrel_focus_parent_list(uint8_t barrel_id, uint8_t restore_row)
{
  uint8_t owning_list = element_parent_list(barrel_id);
  uint8_t parent_text = g_protocol_state.elements[barrel_id].parent_id;
  if (owning_list != INVALID_ELEMENT_ID) {
    protocol_set_focus(owning_list);
    if (g_protocol_state.focused_element == owning_list && restore_row != 0u) {
      ur_list_state_t* ls_restore = ur_list_get_or_add(&g_protocol_state.runtime, owning_list);
      if (ls_restore != NULL) {
        uint8_t row_count = list_row_count(owning_list);
        uint8_t window    = list_effective_window(owning_list, ls_restore);
        if (window == 0u) {
          window = 1u;
        }
        if (row_count == 0u) {
          ls_restore->cursor         = 0u;
          ls_restore->top_index      = 0u;
          ls_restore->pending_top    = 0u;
          ls_restore->pending_cursor = 0u;
        } else {
          uint8_t target_row = list_row_index_of_text(owning_list, parent_text);
          if (target_row == INVALID_ELEMENT_ID || target_row >= row_count) {
            target_row = (uint8_t) (row_count - 1u);
          }
          ls_restore->cursor = target_row;
          if (ls_restore->top_index > target_row) {
            ls_restore->top_index = target_row;
          } else {
            uint8_t upper_bound = (uint8_t) (ls_restore->top_index + window - 1u);
            if (target_row > upper_bound) {
              uint8_t new_top = (uint8_t) (target_row + 1u - window);
              if (new_top > target_row) {
                new_top = target_row;
              }
              ls_restore->top_index = new_top;
            }
          }
          ls_restore->pending_cursor = ls_restore->cursor;
          ls_restore->pending_top    = ls_restore->top_index;
        }
        ls_restore->anim_active = 0u;
        ls_restore->anim_pix    = 0u;
        ls_restore->anim_dir    = 0;
      }
    } else if (g_protocol_state.focused_element == INVALID_ELEMENT_ID) {
      protocol_focus_first_on_screen(g_protocol_state.active_screen);
    }
  } else {
    protocol_focus_first_on_screen(g_protocol_state.active_screen);
  }
  if (g_protocol_state.focused_element == INVALID_ELEMENT_ID) {
    protocol_clear_focus();
  }
}

/** Adjust a barrel selection while in edit mode. */
static void barrel_change_option(uint8_t barrel_id, int8_t dir)
{
  uint8_t option_count = barrel_options_count(barrel_id);
  if (option_count == 0u) {
    return;
  }
  int16_t current = protocol_numeric_value(barrel_id);
  uint8_t index   = (current < 0) ? 0u : (uint8_t) current;
  if (dir < 0) {
    index = (index == 0u) ? (uint8_t) (option_count - 1u) : (uint8_t) (index - 1u);
  } else {
    index = (uint8_t) ((index + 1u) % option_count);
  }
  numeric_set_value(barrel_id, index);
}

/** Handle UP/DOWN actions using the current focus kind. */
static void handle_action_updown(const ui_input_ctx_t* ctx, int8_t dir)
{
  if (ctx == NULL) {
    return;
  }
  if (ctx->focus_kind == UI_FOCUS_LIST) {
    list_move_cursor(ctx->focused_id, dir);
    return;
  }
  if (ctx->focus_kind == UI_FOCUS_BARREL && ctx->barrel_editing != 0u) {
    barrel_change_option(ctx->focused_id, dir);
    return;
  }
  if (dir < 0) {
    protocol_focus_prev();
  } else {
    protocol_focus_next();
  }
}

/** Handle OK actions using the current focus kind. */
static void handle_action_ok(const ui_input_ctx_t* ctx)
{
  if (ctx == NULL) {
    return;
  }
  switch (ctx->focus_kind) {
    case UI_FOCUS_NONE:
      protocol_focus_next();
      break;
    case UI_FOCUS_TRIGGER: {
      ur_trigger_state_t* ts = ur_trigger_get_or_add(&g_protocol_state.runtime, ctx->focused_id);
      if (ts != NULL) {
        ts->version++;
        protocol_element_changed(ctx->focused_id);
      }
    } break;
    case UI_FOCUS_BARREL:
      if (ctx->barrel_editing == 0u) {
        barrel_begin_edit(ctx->focused_id);
      } else {
        barrel_commit_edit(ctx->focused_id);
        protocol_element_changed(ctx->focused_id);
        barrel_focus_parent_list(ctx->focused_id, 0u);
      }
      break;
    case UI_FOCUS_LIST:
      list_handle_ok(ctx->focused_id);
      break;
    default:
      break;
  }
}

/** Handle BACK action with focus- and nav-aware fallbacks. */
static void handle_action_back(const ui_input_ctx_t* ctx)
{
  uint8_t handled = 0u;
  if (ctx == NULL) {
    return;
  }
  switch (ctx->focus_kind) {
    case UI_FOCUS_BARREL:
      if (ctx->barrel_editing != 0u) {
        barrel_cancel_edit(ctx->focused_id);
      }
      barrel_focus_parent_list(ctx->focused_id, 1u);
      handled = 1u;
      break;
    case UI_FOCUS_LIST:
      if (ctx->nav_ctx == UI_NAV_LIST && ctx->nav_target == ctx->focused_id) {
        if (nav_pop() == 0u) {
          protocol_clear_focus();
        }
      }
      handled = 1u;
      break;
    case UI_FOCUS_TRIGGER:
    case UI_FOCUS_OTHER: {
      uint8_t owning_list = element_parent_list(ctx->focused_id);
      if (owning_list != INVALID_ELEMENT_ID) {
        protocol_set_focus(owning_list);
        handled = 1u;
      }
    } break;
    case UI_FOCUS_NONE:
    default:
      break;
  }

  if (handled != 0u) {
    return;
  }
  if (ctx->nav_ctx != UI_NAV_ROOT) {
    if (nav_pop() == 0u) {
      protocol_clear_focus();
    }
    return;
  }
  if (ctx->focused_id != INVALID_ELEMENT_ID) {
    return;
  }
  protocol_focus_first_on_screen(g_protocol_state.active_screen);
  if (g_protocol_state.focused_element == INVALID_ELEMENT_ID) {
    protocol_clear_focus();
  }
}

/** Handle a button release event and update UI state. */
static void process_button_release(uint8_t button)
{
  ui_action_t action = ui_action_from_button(button);
  if (action == UI_ACTION_INVALID) {
    return;
  }
  if (g_protocol_state.screen_anim.active) {
    return;
  }
  if (action == UI_ACTION_UP && protocol_up_button_pressed) {
    protocol_up_button_pressed();
  }
  if (action == UI_ACTION_LEFT || action == UI_ACTION_RIGHT) {
    (void) handle_screen_slide(action);
    return;
  }

  ui_input_ctx_t ctx = ui_input_ctx_collect();
  switch (action) {
    case UI_ACTION_UP:
      handle_action_updown(&ctx, -1);
      break;
    case UI_ACTION_DOWN:
      handle_action_updown(&ctx, +1);
      break;
    case UI_ACTION_OK:
      handle_action_ok(&ctx);
      break;
    case UI_ACTION_BACK:
      handle_action_back(&ctx);
      break;
    default:
      break;
  }
}

int cmd_input_event(uint8_t* p, uint8_t l)
{
  if (l < 2) {
    return RES_BAD_LEN;
  }
  uint8_t idx = p[0];
  uint8_t evt = p[1];
  if (idx >= UI_BUTTON_COUNT) {
    return RES_RANGE;
  }
  if (g_protocol_state.overlay.active_overlay_screen_id != 0xFF &&
      g_protocol_state.overlay.mask_input) {
    if (idx != UI_BUTTON_OK) {
      return RES_OK;
    }
  }
  if (evt == 0) {
    process_button_release(idx);
    protocol_request_render();
  }
  return RES_OK;
}
