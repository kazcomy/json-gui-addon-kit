/**
 * @file ui_tree.c
 * @brief UI tree helpers (parent/child/screen lookups).
 */
#include "ui_tree.h"

#include "ui_protocol.h"

uint8_t list_item_count(uint8_t list_eid)
{
  uint8_t cnt = 0;
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    const element_t* el = &g_protocol_state.elements[i];
    if (el->parent_id == list_eid && el->type == ELEMENT_TEXT) {
      cnt++;
    }
  }
  return cnt;
}

uint8_t list_row_count(uint8_t list_eid)
{
  uint8_t count = 0u;
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    const element_t* el = &g_protocol_state.elements[i];
    if (el->parent_id != list_eid) {
      continue;
    }
    if (el->type != ELEMENT_TEXT) {
      continue;
    }
    if (protocol_is_element_visible(i) == 0u) {
      continue;
    }
    count++;
  }
  return count;
}

uint8_t list_child_by_index(uint8_t list_eid, uint8_t row_index)
{
  uint8_t count = 0u;
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    const element_t* child = &g_protocol_state.elements[i];
    if (child->parent_id != list_eid) {
      continue;
    }
    if (child->type != ELEMENT_TEXT) {
      continue;
    }
    if (protocol_is_element_visible(i) == 0u) {
      continue;
    }
    if (count == row_index) {
      return (uint8_t) i;
    }
    count++;
  }
  return INVALID_ELEMENT_ID;
}

uint8_t list_row_index_of_text(uint8_t list_eid, uint8_t text_eid)
{
  if (list_eid >= g_protocol_state.element_count) {
    return INVALID_ELEMENT_ID;
  }
  if (text_eid >= g_protocol_state.element_count) {
    return INVALID_ELEMENT_ID;
  }
  uint8_t row = 0u;
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    const element_t* child = &g_protocol_state.elements[i];
    if (child->parent_id != list_eid) {
      continue;
    }
    if (child->type != ELEMENT_TEXT) {
      continue;
    }
    if (protocol_is_element_visible(i) == 0u) {
      continue;
    }
    if (i == text_eid) {
      return row;
    }
    row++;
  }
  return INVALID_ELEMENT_ID;
}

uint8_t text_inline_barrel_id(uint8_t text_eid)
{
  for (uint8_t eid = 0; eid < g_protocol_state.element_count; eid++) {
    const element_t* child = &g_protocol_state.elements[eid];
    if (child->parent_id != text_eid) {
      continue;
    }
    if (child->type == ELEMENT_BARREL) {
      return eid;
    }
  }
  return INVALID_ELEMENT_ID;
}

uint8_t element_parent_list(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return INVALID_ELEMENT_ID;
  }
  uint8_t current = g_protocol_state.elements[eid].parent_id;
  for (uint16_t depth = 0; depth < g_protocol_state.element_count; depth++) {
    if (current == INVALID_ELEMENT_ID) {
      break;
    }
    if (current >= g_protocol_state.element_count) {
      break;
    }
    if (g_protocol_state.elements[current].type == ELEMENT_LIST_VIEW) {
      return current;
    }
    current = g_protocol_state.elements[current].parent_id;
  }
  return INVALID_ELEMENT_ID;
}

uint8_t find_screen_id_by_ordinal(uint8_t sord)
{
  uint8_t seen = 0U;
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    if (g_protocol_state.elements[i].type == ELEMENT_SCREEN &&
        protocol_screen_role(i) == OVERLAY_NONE &&
        g_protocol_state.elements[i].parent_id == INVALID_ELEMENT_ID) {
      if (seen == sord) {
        return i;
      }
      seen++;
    }
  }
  return INVALID_ELEMENT_ID;
}

uint8_t find_screen_ordinal_by_id(uint8_t screen_id)
{
  if (screen_id >= g_protocol_state.element_count) {
    return INVALID_ELEMENT_ID;
  }
  if (g_protocol_state.elements[screen_id].type != ELEMENT_SCREEN) {
    return INVALID_ELEMENT_ID;
  }
  if (g_protocol_state.elements[screen_id].parent_id != INVALID_ELEMENT_ID) {
    return INVALID_ELEMENT_ID;
  }
  uint8_t ord = 0u;
  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    if (g_protocol_state.elements[i].type != ELEMENT_SCREEN) {
      continue;
    }
    if (protocol_screen_role(i) != OVERLAY_NONE) {
      continue;
    }
    if (i == screen_id) {
      return ord;
    }
    ord++;
  }
  return INVALID_ELEMENT_ID;
}

uint8_t element_root_screen(uint8_t eid)
{
  if (eid >= g_protocol_state.element_count) {
    return INVALID_ELEMENT_ID;
  }
  uint8_t current = eid;
  for (uint16_t depth = 0; depth < g_protocol_state.element_count; depth++) {
    if (current == INVALID_ELEMENT_ID) {
      break;
    }
    if (current >= g_protocol_state.element_count) {
      break;
    }
    if (g_protocol_state.elements[current].type == ELEMENT_SCREEN) {
      return current;
    }
    current = g_protocol_state.elements[current].parent_id;
  }
  return INVALID_ELEMENT_ID;
}

uint8_t is_descendant_of(uint8_t eid, uint8_t ancestor)
{
  if (ancestor == INVALID_ELEMENT_ID) {
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
    if (current == ancestor) {
      return 1u;
    }
    current = g_protocol_state.elements[current].parent_id;
  }
  return 0u;
}
