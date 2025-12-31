/**
 * @file ui_layout.c
 * @brief Layout phase for UI rendering (compute final coordinates + clip).
 */
#include "ui_layout.h"

#include "status_codes.h"
#include "ssd1306_driver.h"
#include "ui_runtime.h"
#include "ui_protocol.h"
#include "ui_tree.h"

int ui_layout_compute_element(uint8_t element_id, int16_t* out_x, int16_t* out_y)
{
  if (!out_x || !out_y) {
    return RES_BAD_LEN;
  }
  *out_x = 0;
  *out_y = 0;
  if (element_id >= g_protocol_state.element_count) {
    return RES_UNKNOWN_ID;
  }

  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t font = 8;
  uint8_t layout = LAYOUT_ABSOLUTE;
  if (ui_attr_get_position(&g_protocol_state.runtime, element_id, &x, &y, &font, &layout) != 0) {
    return RES_BAD_STATE;
  }
  if (layout != LAYOUT_ABSOLUTE) {
    return RES_BAD_STATE;
  }

  uint8_t owning_screen = INVALID_ELEMENT_ID;
  if (g_protocol_state.elements[element_id].type == ELEMENT_SCREEN &&
      g_protocol_state.elements[element_id].parent_id == INVALID_ELEMENT_ID) {
    owning_screen = element_id;
  } else {
    uint8_t parent = g_protocol_state.elements[element_id].parent_id;
    while (parent != INVALID_ELEMENT_ID) {
      if (g_protocol_state.elements[parent].type == ELEMENT_SCREEN &&
          g_protocol_state.elements[parent].parent_id == INVALID_ELEMENT_ID) {
        owning_screen = parent;
        break;
      }
      parent = g_protocol_state.elements[parent].parent_id;
    }
  }
  if (owning_screen == INVALID_ELEMENT_ID) {
    return RES_UNKNOWN_ID;
  }

  int16_t base_x = (int16_t) x;
  int16_t base_y = (int16_t) y;
  uint8_t role = protocol_screen_role(owning_screen);
  if (role == OVERLAY_NONE) {
    uint8_t screen_ord = find_screen_ordinal_by_id(owning_screen);
    if (screen_ord == INVALID_ELEMENT_ID) {
      return RES_UNKNOWN_ID;
    }
    base_x += (int16_t) ((int16_t) screen_ord * SSD1306_WIDTH);
    base_x -= g_protocol_state.scroll_x;
    if (g_protocol_state.screen_anim.active) {
      screen_anim_state_t* sa = &g_protocol_state.screen_anim;
      if (screen_ord == sa->from_screen || screen_ord == sa->to_screen) {
        base_x -= (int16_t) (sa->dir * sa->offset_px);
      }
    }
  }
  *out_x = base_x;
  *out_y = base_y;
  return RES_OK;
}
