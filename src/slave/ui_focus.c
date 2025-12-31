/**
 * @file ui_focus.c
 * @brief Focus/navigation helpers for UI.
 */
#include "ui_focus.h"

#include "ui_protocol.h"
#include "ui_runtime.h"
#include "ui_tree.h"

/** Resolve active navigation context element (screen or nested target). */
static uint8_t nav_active_context(void)
{
  if (g_protocol_state.nav_depth == 0u) {
    return find_screen_id_by_ordinal(g_protocol_state.active_screen);
  }
  uint8_t top = (uint8_t) (g_protocol_state.nav_depth - 1u);
  if (top >= NAV_STACK_MAX_DEPTH) {
    return INVALID_ELEMENT_ID;
  }
  return g_protocol_state.nav_stack[top].target_element;
}

/** Sync active_local_screen based on current navigation stack. */
static void nav_update_active_local_screen(void)
{
  g_protocol_state.active_local_screen = INVALID_ELEMENT_ID;
  if (g_protocol_state.nav_depth == 0u) {
    return;
  }
  uint8_t top = (uint8_t) (g_protocol_state.nav_depth - 1u);
  if (top >= NAV_STACK_MAX_DEPTH) {
    return;
  }
  if (g_protocol_state.nav_stack[top].type == (uint8_t) NAV_CTX_LOCAL_SCREEN) {
    g_protocol_state.active_local_screen = g_protocol_state.nav_stack[top].target_element;
  }
}

/** Return 1 if target_id is in the current navigation stack. */
static uint8_t nav_target_active(uint8_t target_id)
{
  if (target_id == INVALID_ELEMENT_ID) {
    return 0u;
  }
  uint8_t depth = g_protocol_state.nav_depth;
  if (depth > NAV_STACK_MAX_DEPTH) {
    depth = NAV_STACK_MAX_DEPTH;
  }
  for (uint8_t i = 0; i < depth; i++) {
    if (g_protocol_state.nav_stack[i].target_element == target_id) {
      return 1u;
    }
  }
  return 0u;
}

/** Return non-zero if a screen element is a local screen (child of TEXT). */
static uint8_t screen_is_local(uint8_t screen_id)
{
  if (screen_id >= g_protocol_state.element_count) {
    return 0u;
  }
  if (g_protocol_state.elements[screen_id].type != ELEMENT_SCREEN) {
    return 0u;
  }
  uint8_t parent = g_protocol_state.elements[screen_id].parent_id;
  if (parent == INVALID_ELEMENT_ID || parent >= g_protocol_state.element_count) {
    return 0u;
  }
  return (g_protocol_state.elements[parent].type == ELEMENT_TEXT) ? 1u : 0u;
}

uint8_t protocol_is_element_visible(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return 0u;
  }
  uint8_t context      = nav_active_context();
  uint8_t extra_screen = INVALID_ELEMENT_ID;
  if (g_protocol_state.nav_depth == 0u && g_protocol_state.screen_anim.active != 0u) {
    extra_screen = find_screen_id_by_ordinal(g_protocol_state.screen_anim.from_screen);
    if (extra_screen == context) {
      extra_screen = INVALID_ELEMENT_ID;
    }
  }
  if (context == INVALID_ELEMENT_ID && extra_screen == INVALID_ELEMENT_ID) {
    return 0u;
  }
  uint8_t visible = 0u;
  if (g_protocol_state.nav_depth == 0u) {
    if (context != INVALID_ELEMENT_ID) {
      visible = is_descendant_of(eid, context);
    }
    if (visible == 0u && extra_screen != INVALID_ELEMENT_ID) {
      visible = is_descendant_of(eid, extra_screen);
    }
  } else {
    uint8_t top = (uint8_t) (g_protocol_state.nav_depth - 1u);
    if (top >= NAV_STACK_MAX_DEPTH) {
      return 0u;
    }
    const nav_stack_entry_t* entry = &g_protocol_state.nav_stack[top];
    visible = (eid == entry->target_element) ? 1u : is_descendant_of(eid, entry->target_element);
  }
  if (visible == 0u) {
    return 0u;
  }
  uint8_t root_screen = element_root_screen(eid);
  if (root_screen == INVALID_ELEMENT_ID) {
    return 0u;
  }
  if (screen_is_local(root_screen) != 0u && nav_target_active(root_screen) == 0u) {
    return 0u;
  }
  uint8_t current = eid;
  for (uint16_t depth = 0; depth < g_protocol_state.element_count; depth++) {
    if (current == INVALID_ELEMENT_ID) {
      break;
    }
    if (current >= g_protocol_state.element_count) {
      break;
    }
    const element_t* current_el = &g_protocol_state.elements[current];
    if (current_el->type == ELEMENT_LIST_VIEW) {
      uint8_t owner_text = current_el->parent_id;
      if (owner_text != INVALID_ELEMENT_ID && owner_text < g_protocol_state.element_count) {
        const element_t* owner_el = &g_protocol_state.elements[owner_text];
        if (owner_el->type == ELEMENT_TEXT) {
          uint8_t list_parent = owner_el->parent_id;
          if (list_parent != INVALID_ELEMENT_ID && list_parent < g_protocol_state.element_count) {
            const element_t* list_parent_el = &g_protocol_state.elements[list_parent];
            if (list_parent_el->type == ELEMENT_LIST_VIEW) {
              if (nav_target_active(current) == 0u) {
                return 0u;
              }
            }
          }
        }
      }
    }
    current = current_el->parent_id;
  }
  return 1u;
}

/** Return non-zero if the element type participates in focus traversal. */
static int element_focusable(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return 0;
  }
  element_t* el = &g_protocol_state.elements[eid];
  switch (el->type) {
    case ELEMENT_LIST_VIEW:
    case ELEMENT_NUMBER_EDIT:
    case ELEMENT_TRIGGER:
    case ELEMENT_BARREL: return 1;
    default: return 0;
  }
}

/** Focus the first visible focusable element under the given owner. */
static void protocol_focus_first_under(uint8_t owner_id)
{
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    if (protocol_is_element_visible(i) == 0u) {
      continue;
    }
    if ((i != owner_id) && (is_descendant_of(i, owner_id) == 0u)) {
      continue;
    }
    if (element_focusable(i) != 0) {
      protocol_set_focus(i);
      return;
    }
  }
  protocol_clear_focus();
}

void protocol_register_local_screen(uint8_t screen_id, uint8_t owner_text)
{
  if (screen_id >= g_protocol_state.element_count) {
    return;
  }
  if (owner_text >= g_protocol_state.element_count) {
    owner_text = INVALID_ELEMENT_ID;
  }
  if (owner_text != INVALID_ELEMENT_ID) {
    g_protocol_state.elements[screen_id].parent_id = owner_text;
  }
}

uint8_t protocol_text_local_screen(uint8_t text_id)
{
  if (text_id >= g_protocol_state.element_count) {
    return INVALID_ELEMENT_ID;
  }
  for (uint8_t eid = 0; eid < g_protocol_state.element_count; eid++) {
    if (g_protocol_state.elements[eid].parent_id == text_id &&
        g_protocol_state.elements[eid].type == ELEMENT_SCREEN) {
      return eid;
    }
  }
  return INVALID_ELEMENT_ID;
}

uint8_t nav_push_list(uint8_t parent_list, uint8_t target_list)
{
  if (g_protocol_state.nav_depth >= NAV_STACK_MAX_DEPTH) {
    return 0u;
  }
  ur_list_state_t* parent_state = ur_list_get_or_add(&g_protocol_state.runtime, parent_list);
  ur_list_state_t* child_state  = ur_list_get_or_add(&g_protocol_state.runtime, target_list);
  if (!parent_state || !child_state) {
    return 0u;
  }
  nav_stack_entry_t* entry = &g_protocol_state.nav_stack[g_protocol_state.nav_depth];
  entry->type               = (uint8_t) NAV_CTX_LIST;
  entry->target_element     = target_list;
  entry->return_list        = parent_list;
  entry->saved_cursor       = parent_state->cursor;
  entry->saved_top          = parent_state->top_index;
  entry->saved_focus        = g_protocol_state.focused_element;
  entry->saved_active_screen = g_protocol_state.active_screen;
  child_state->cursor       = 0u;
  child_state->top_index    = 0u;
  child_state->anim_active  = 0u;
  child_state->anim_pix     = 0u;
  child_state->anim_dir     = 0;
  g_protocol_state.nav_depth = (uint8_t) (g_protocol_state.nav_depth + 1u);
  nav_update_active_local_screen();
  protocol_set_focus(target_list);
  return 1u;
}

uint8_t nav_push_local_screen(uint8_t parent_list, uint8_t screen_id)
{
  if (g_protocol_state.nav_depth >= NAV_STACK_MAX_DEPTH) {
    return 0u;
  }
  ur_list_state_t* parent_state = ur_list_get_or_add(&g_protocol_state.runtime, parent_list);
  if (!parent_state) {
    return 0u;
  }
  nav_stack_entry_t* entry = &g_protocol_state.nav_stack[g_protocol_state.nav_depth];
  entry->type               = (uint8_t) NAV_CTX_LOCAL_SCREEN;
  entry->target_element     = screen_id;
  entry->return_list        = parent_list;
  entry->saved_cursor       = parent_state->cursor;
  entry->saved_top          = parent_state->top_index;
  entry->saved_focus        = g_protocol_state.focused_element;
  entry->saved_active_screen = g_protocol_state.active_screen;
  uint8_t new_ord = find_screen_ordinal_by_id(screen_id);
  if (new_ord != INVALID_ELEMENT_ID) {
    g_protocol_state.active_screen = new_ord;
    g_protocol_state.scroll_x      = (int16_t) ((int16_t) new_ord * 128);
  }
  g_protocol_state.nav_depth = (uint8_t) (g_protocol_state.nav_depth + 1u);
  nav_update_active_local_screen();
  protocol_focus_first_under(screen_id);
  if (g_protocol_state.focused_element == INVALID_ELEMENT_ID) {
    protocol_set_focus(parent_list);
  }
  return 1u;
}

uint8_t nav_pop(void)
{
  if (g_protocol_state.nav_depth == 0u) {
    return 0u;
  }
  uint8_t top_index = (uint8_t) (g_protocol_state.nav_depth - 1u);
  if (top_index >= NAV_STACK_MAX_DEPTH) {
    return 0u;
  }
  nav_stack_entry_t entry = g_protocol_state.nav_stack[top_index];
  g_protocol_state.nav_depth = top_index;
  nav_update_active_local_screen();
  if (entry.return_list != INVALID_ELEMENT_ID) {
    ur_list_state_t* parent_state =
      ur_list_get_or_add(&g_protocol_state.runtime, entry.return_list);
    if (parent_state) {
      parent_state->cursor      = entry.saved_cursor;
      parent_state->top_index   = entry.saved_top;
      parent_state->anim_active = 0u;
      parent_state->anim_pix    = 0u;
      parent_state->anim_dir    = 0;
    }
  }
  if (entry.type == (uint8_t) NAV_CTX_LOCAL_SCREEN) {
    g_protocol_state.active_screen = entry.saved_active_screen;
    g_protocol_state.scroll_x      = (int16_t) ((int16_t) g_protocol_state.active_screen * 128);
  }
  if (entry.return_list != INVALID_ELEMENT_ID) {
    protocol_set_focus(entry.return_list);
  } else {
    protocol_clear_focus();
  }
  return 1u;
}

void protocol_set_focus(uint8_t element_id)
{
  if (element_id >= g_protocol_state.element_count) {
    return;
  }
  if (protocol_is_element_visible(element_id) == 0u) {
    return;
  }
  if (element_focusable(element_id) == 0) {
    return;
  }
  g_protocol_state.focused_element = element_id;
}

uint8_t protocol_get_focused(void)
{
  return g_protocol_state.focused_element;
}

void protocol_clear_focus(void)
{
  g_protocol_state.focused_element = INVALID_ELEMENT_ID;
}

void protocol_focus_next(void)
{
  uint8_t count = g_protocol_state.element_count;
  if (count == 0u) {
    protocol_clear_focus();
    return;
  }
  uint8_t start = (g_protocol_state.focused_element == INVALID_ELEMENT_ID)
                    ? 0u
                    : (uint8_t) ((g_protocol_state.focused_element + 1u) % count);
  for (uint16_t step = 0; step < count; step++) {
    uint8_t candidate = (uint8_t) ((start + step) % count);
    if (protocol_is_element_visible(candidate) == 0u) {
      continue;
    }
    if (element_focusable(candidate) == 0) {
      continue;
    }
    protocol_set_focus(candidate);
    return;
  }
  protocol_clear_focus();
}

void protocol_focus_prev(void)
{
  uint8_t count = g_protocol_state.element_count;
  if (count == 0u) {
    protocol_clear_focus();
    return;
  }
  int16_t start = (g_protocol_state.focused_element == INVALID_ELEMENT_ID)
                    ? (int16_t) (count - 1u)
                    : (int16_t) g_protocol_state.focused_element - 1;
  for (uint16_t step = 0; step < count; step++) {
    if (start < 0) {
      start = (int16_t) (count - 1u);
    }
    uint8_t candidate = (uint8_t) start;
    if (protocol_is_element_visible(candidate) == 0u) {
      start--;
      continue;
    }
    if (element_focusable(candidate) == 0) {
      start--;
      continue;
    }
    protocol_set_focus(candidate);
    return;
  }
  protocol_clear_focus();
}

void protocol_focus_first_on_screen(uint8_t sord)
{
  if (g_protocol_state.nav_depth != 0u) {
    return;
  }
  uint8_t screen_eid = find_screen_id_by_ordinal(sord);
  if (screen_eid == INVALID_ELEMENT_ID) {
    protocol_clear_focus();
    return;
  }
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    if (protocol_is_element_visible(i) == 0u) {
      continue;
    }
    if (is_descendant_of(i, screen_eid) == 0u) {
      continue;
    }
    if (element_focusable(i) == 0) {
      continue;
    }
    protocol_set_focus(i);
    return;
  }
  protocol_clear_focus();
}
