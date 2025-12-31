/**
 * @file ui_renderer.c
 * @brief UI Renderer for tile-based display rendering.
 *
 * This module provides the rendering engine for the UI system:
 * - Tile-based rendering over SSD1306 driver
 * - Element type-specific rendering functions
 * - Text rendering with font support
 * - Numeric value formatting and display
 * - Viewport clipping and scrolling support
 * - Popup and overlay rendering
 *
 * The renderer works in conjunction with the UI protocol and
 * attribute system to display dynamic user interfaces.
 */
/* ============================================================================
 * UI Renderer
 * Tile-based rendering over SSD1306 driver.
 * ============================================================================ */
#include <stddef.h>
#include <string.h>

#include "element_types.h"
#include "gfx_font.h"
#include "gfx_shared.h"
#include "ssd1306_driver.h"
#include "ui_runtime.h"
#include "ui_layout.h"
#include "ui_protocol.h"
#include "debug_led.h"

#ifndef RENDER_MAX_DECIMALS
#define RENDER_MAX_DECIMALS 2u
#endif

/* Forward declarations */
static void draw_masked_text(int16_t     x,
                             int16_t     pixel_y,
                             const char* text,
                             uint8_t     viewport_top,
                             uint8_t     viewport_bottom,
                             uint8_t     page_top);
static void invert_row_region(uint8_t start_x,
                              uint8_t width,
                              int16_t pixel_y,
                              uint8_t viewport_top,
                              uint8_t viewport_bottom,
                              uint8_t page_top);
static uint8_t text_highlight_width(const char* text);
static uint8_t edit_blink_visible(void);

/**

/* Helper: render only overlay children (TEXT only) for the given tile. */
static void render_overlay_children_tile(uint8_t tile_y, uint8_t overlay_sid)
{
  if (overlay_sid == 0xFF) return;
  if (overlay_sid >= g_protocol_state.element_count) return;
  if (g_protocol_state.elements[overlay_sid].type != ELEMENT_SCREEN) return;

  uint8_t page_top      = (uint8_t) (tile_y * SSD1306_PAGE_HEIGHT);
  /* Keep overlay fixed on the display horizontally: ignore scroll_x and screen ordinals. */

  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    const element_t* elem = &g_protocol_state.elements[i];
    /* Only TEXT is supported on overlay for size reasons */
    if (elem->type != ELEMENT_TEXT) continue;
    /* Resolve owning screen by climbing parents */
    uint8_t parent = elem->parent_id;
    while (parent != 0xFF && g_protocol_state.elements[parent].type != ELEMENT_SCREEN) {
      parent = g_protocol_state.elements[parent].parent_id;
    }
    if (parent != overlay_sid) continue; /* not under overlay screen */

    int16_t global_x = 0;
    int16_t draw_y   = 0;
    if (ui_layout_compute_element(i, &global_x, &draw_y) != 0) {
      continue;
    }
    const char* txt = ui_attr_get_text(&g_protocol_state.runtime, i);
    draw_masked_text(global_x,
                     draw_y,
                     txt,
                     page_top,
                     (uint8_t) (page_top + SSD1306_PAGE_HEIGHT - 1),
                     page_top);
  }
}

/**
 * @brief Return inclusive highlight width for a text string at base scale.
 *
 * The inclusive width can be passed directly to invert_row_region().
 * A value of zero indicates a single-column highlight.
 */
static uint8_t text_highlight_width(const char* text)
{
  uint16_t width_pixels = 0U;

  if ((text == NULL) || (*text == '\0')) {
    width_pixels = (uint16_t) GFX_FONT_CHAR_WIDTH;
  } else {
    const char* ptr = text;
    while (*ptr != '\0') {
      width_pixels += (uint16_t) GFX_FONT_CHAR_WIDTH;
      ptr++;
      if (*ptr != '\0') {
        width_pixels++;
      }
      if (width_pixels >= (uint16_t) SSD1306_WIDTH) {
        width_pixels = (uint16_t) SSD1306_WIDTH;
        break;
      }
    }
  }

  if (width_pixels == 0U) {
    width_pixels = (uint16_t) GFX_FONT_CHAR_WIDTH;
  }
  if (width_pixels > (uint16_t) SSD1306_WIDTH) {
    width_pixels = (uint16_t) SSD1306_WIDTH;
  }
  if (width_pixels == 0U) {
    width_pixels = 1U;
  }

  return (uint8_t) (width_pixels - 1U);
}

/** Return 1 when edit blink should show the highlighted state. */
static uint8_t edit_blink_visible(void)
{
  if (g_protocol_state.edit_blink_active == 0u) {
    return 1u;
  }
  return g_protocol_state.edit_blink_phase;
}

/**
 * @brief Scale an inclusive highlight width to account for text scaling.
 */
/**
 * @brief Render the active screen via tile callback.
 *
 * Sets the active screen and triggers tile-based rendering.
 * Only works when the protocol system is initialized.
 *
 * @param screen_id ID of the screen to render
 */
/**
 * @brief Tile callback for rendering visible elements.
 *
 * This function is called by the SSD1306 driver for each tile (page).
 * It renders all visible elements that intersect with the current tile.
 *
 * @param tile_y Tile Y coordinate (0-3)
 */
void render_screen_tile(uint8_t tile_y)
{
  /* Overlay state snapshot */
  uint8_t overlay_sid = g_protocol_state.overlay.active_overlay_screen_id;
  uint8_t overlay_active = 0u;
  if (overlay_sid != 0xFF && overlay_sid < g_protocol_state.element_count &&
      g_protocol_state.elements[overlay_sid].type == ELEMENT_SCREEN &&
      protocol_screen_role(overlay_sid) == OVERLAY_FULL) {
    overlay_active = 1u;
  } else {
    overlay_sid = 0xFF;
  }

  if (overlay_active != 0u) {
    render_overlay_children_tile(tile_y, overlay_sid);
    return;
  }

  /* Resolve active screen element id (base screens only). */
  uint8_t active_screen_id = INVALID_ELEMENT_ID;
  {
    uint8_t ord = 0u;
    for (uint8_t j = 0; j < g_protocol_state.element_count; j++) {
      if (g_protocol_state.elements[j].type == ELEMENT_SCREEN &&
          g_protocol_state.elements[j].parent_id == INVALID_ELEMENT_ID &&
          protocol_screen_role(j) == OVERLAY_NONE) {
        if (ord == g_protocol_state.active_screen) {
          active_screen_id = j;
          break;
        }
        ord++;
      }
    }
  }

  for (uint8_t i = 0; i < g_protocol_state.element_count; i++) {
    element_t* elem = &g_protocol_state.elements[i];
    if (protocol_is_element_visible(i) == 0u) {
      continue;
    }
    /* Skip list children (handled in list branch) and barrel children (rendered by barrel) */
    if (elem->parent_id != 0xFF) {
      uint8_t parent_type = g_protocol_state.elements[elem->parent_id].type;
      if (parent_type == ELEMENT_LIST_VIEW) {
        if (elem->type == ELEMENT_TEXT) {
          continue;
        }
      }
      if (parent_type == ELEMENT_BARREL) {
        /* Barrel renders its selected child label; suppress direct child draw */
        continue;
      }
    }
    /* Resolve owning screen order */
    uint8_t parent_screen = elem->parent_id;
    while (parent_screen != 0xFF &&
           g_protocol_state.elements[parent_screen].type != ELEMENT_SCREEN) {
      parent_screen = g_protocol_state.elements[parent_screen].parent_id;
    }
    if (parent_screen == 0xFF) {
      continue;
    }
    uint8_t owning_screen = parent_screen;
    if (owning_screen < g_protocol_state.element_count) {
      uint8_t probe = owning_screen;
      for (uint16_t depth = 0; depth < g_protocol_state.element_count; depth++) {
        if (probe >= g_protocol_state.element_count) {
          break;
        }
        uint8_t ps = g_protocol_state.elements[probe].parent_id;
        if (ps == INVALID_ELEMENT_ID || ps >= g_protocol_state.element_count) {
          break;
        }
        if (g_protocol_state.elements[ps].type == ELEMENT_SCREEN) {
          owning_screen = ps;
          probe         = ps;
        } else {
          probe = ps;
        }
      }
    }
    if (protocol_screen_role(owning_screen) != OVERLAY_NONE) {
      continue;
    }
    int16_t global_x = 0;
    int16_t global_y = 0;
    if (ui_layout_compute_element(i, &global_x, &global_y) != 0) {
      continue;
    }
    /* Symmetric culling window so content coming from left can be processed.
      We still clip per-pixel in draw paths. */
    if (global_x < -143 || global_x > 143) {
      continue;
    }
    /* Keep signed x for masked/text rendering; clamp when API needs uint8. */
    uint8_t draw_x = (global_x < 0) ? 0u : (uint8_t) global_x;
    
    /* For non-list elements, skip coarse tile filter and rely on fine-grained
      checks inside draw paths (handles negative y during overlay slide). */

    if (elem->type == ELEMENT_TEXT) {
      const char* txt = ui_attr_get_text(&g_protocol_state.runtime, i);
      /* Use masked text drawer to handle horizontal clipping (negative x).
        Limit viewport to current tile to avoid uint8 wrap on negative y. */
      uint8_t page_top = (uint8_t) (tile_y * SSD1306_PAGE_HEIGHT);
      int16_t draw_y   = global_y;
      draw_masked_text(global_x,
                       draw_y,
                       txt,
                       page_top,
                       (uint8_t) (page_top + SSD1306_PAGE_HEIGHT - 1),
                       page_top);
      if (i == g_protocol_state.focused_element &&
          owning_screen == active_screen_id && !g_protocol_state.screen_anim.active) {
        uint8_t page_top = (uint8_t) (tile_y * SSD1306_PAGE_HEIGHT);
        uint8_t highlight_width = text_highlight_width(txt);
        if (highlight_width < 18u) {
          highlight_width = 18u;
        }
        invert_row_region(draw_x,
                          highlight_width,
                          (int16_t) global_y,
                          (uint8_t) global_y,
                          (uint8_t) (global_y + 7),
                          page_top);
      }
    } else if (elem->type == ELEMENT_LIST_VIEW) {
      {
        ur_list_state_t* ls = ur_list_get_or_add(&g_protocol_state.runtime, i);
        if (!ls) continue;
        int16_t base_global_x = global_x; /* includes scroll and slide anim */
        int16_t base_y        = global_y;
        if (base_y < 0) {
          base_y = 0;
        }
        uint8_t window        = ls->visible_rows ? ls->visible_rows : 4;
        uint8_t max_rows      = (ssd1306_height() >= 64u) ? 8u : 6u;
        if (window > max_rows) {
          window = max_rows;
        }
        int8_t  dir             = ls->anim_active ? ls->anim_dir : 0;
        uint8_t pix             = ls->anim_active ? ls->anim_pix : 0;
        uint8_t top             = ls->top_index;
        uint8_t viewport_top    = (uint8_t) base_y;
        uint8_t viewport_bottom = (uint8_t) (base_y + window * 8 - 1);
        uint8_t ic             = 0; /* recompute child count */
        for (uint8_t e2 = 0; e2 < g_protocol_state.element_count; e2++) {
          const element_t* child = &g_protocol_state.elements[e2];
          if (child->parent_id == i && child->type == ELEMENT_TEXT) {
            ic++;
          }
        }
        uint8_t first = top;
        if (dir == -1 && top > 0) first = (uint8_t) (top - 1);
        uint8_t last = (uint8_t) (top + window - 1);
        if (dir == +1 && (uint8_t) (top + window) < ic) last = (uint8_t) (top + window);
        uint8_t page_top = (uint8_t) (tile_y * SSD1306_PAGE_HEIGHT);
        for (uint8_t r = first; r <= last && r < ic; r++) {
          int16_t pixel_y;
          if (dir == 0) {
            pixel_y = base_y + ((int16_t) r - (int16_t) top) * 8;
          } else if (dir == +1) {
            pixel_y = base_y + ((int16_t) r - (int16_t) top) * 8 - pix;
          } else { /* dir == -1 */
            if (r == (uint8_t) (top - 1)) {
              pixel_y = base_y - 8 + pix;
            } else {
              pixel_y = base_y + ((int16_t) r - (int16_t) top) * 8 + pix;
            }
          }
          if ((pixel_y + 7) < viewport_top || pixel_y > viewport_bottom) {
            continue;
          }
          if (pixel_y > (int16_t) (page_top + SSD1306_PAGE_HEIGHT - 1) ||
              (pixel_y + 7) < page_top) {
            continue;
          }
          /* find r-th child eid */
          uint8_t kk = 0;
          uint8_t item_eid = 0xFF;
          for (uint8_t e2 = 0; e2 < g_protocol_state.element_count; e2++) {
            const element_t* child = &g_protocol_state.elements[e2];
            if (child->parent_id != i) continue;
            if (child->type != ELEMENT_TEXT) continue;
            if (kk == r) { item_eid = e2; break; }
            kk++;
          }
          if (item_eid == 0xFF) continue;
          uint8_t ix = 0, iy_rel = 0, f2 = 0, lay2 = 0;
          if (ui_attr_get_position(&g_protocol_state.runtime, item_eid, &ix, &iy_rel, &f2, &lay2) !=
              0) {
            continue;
          }
          int16_t item_global_x = (int16_t) (base_global_x + ix);
          if (item_global_x < -143 || item_global_x > 143) {
            continue;
          }
          /* signed X kept for masked draw */
          element_t* item_el = &g_protocol_state.elements[item_eid];
          if (item_el->type == ELEMENT_TEXT) {
            const char* itxt = ui_attr_get_text(&g_protocol_state.runtime, item_eid);
            draw_masked_text(item_global_x,
                             pixel_y,
                             itxt,
                             viewport_top,
                             viewport_bottom,
                             page_top);
          }
          uint8_t highlight = 0;
          if (!ls->anim_active) {
            highlight = (r == ls->cursor);
          } else {
            highlight = (r == ls->cursor || r == ls->pending_cursor);
          }
          uint8_t list_has_focus = (g_protocol_state.focused_element == i) ? 1u : 0u;
          if (highlight && owning_screen == active_screen_id &&
              !g_protocol_state.screen_anim.active && list_has_focus) {
            int16_t marker_gx = (int16_t) (item_global_x - 6);
            draw_masked_text(marker_gx,
                             pixel_y,
                             ">",
                             viewport_top,
                             viewport_bottom,
                             page_top);
          }
        }
      }
    } else if (elem->type == ELEMENT_BARREL) {
      int16_t selection = protocol_numeric_value(i);
      if (selection < 0) {
        selection = 0;
      }
      uint8_t      page_top       = (uint8_t) (tile_y * SSD1306_PAGE_HEIGHT);
      const char*  highlight_text = NULL;
      char         label_buf[8];
      uint8_t      child_ix       = 0u;
      uint8_t      inline_list_selected = 0u;
      uint8_t      parent_text          = g_protocol_state.elements[i].parent_id;
      if (parent_text != INVALID_ELEMENT_ID &&
          g_protocol_state.elements[parent_text].type == ELEMENT_TEXT) {
        uint8_t list_parent = g_protocol_state.elements[parent_text].parent_id;
        if (list_parent != INVALID_ELEMENT_ID &&
            g_protocol_state.elements[list_parent].type == ELEMENT_LIST_VIEW) {
          ur_list_state_t* ls_parent = ur_list_get_or_add(&g_protocol_state.runtime, list_parent);
          if (ls_parent != NULL && list_parent == g_protocol_state.focused_element &&
              ls_parent->anim_active == 0u && owning_screen == active_screen_id &&
              !g_protocol_state.screen_anim.active) {
            uint8_t row_index = 0u;
            for (uint8_t scan = 0; scan < g_protocol_state.element_count; scan++) {
              const element_t* candidate = &g_protocol_state.elements[scan];
              if (candidate->parent_id != list_parent) {
                continue;
              }
              if (candidate->type != ELEMENT_TEXT) {
                continue;
              }
              if (scan == parent_text) {
                break;
              }
              row_index++;
            }
            if (row_index == ls_parent->cursor) {
              inline_list_selected = 1u;
            }
          }
        }
      }
      for (uint8_t cid = 0; cid < g_protocol_state.element_count; cid++) {
        const element_t* child = &g_protocol_state.elements[cid];
        if (child->parent_id != i || child->type != ELEMENT_TEXT) {
          continue;
        }
        if (child_ix == (uint8_t) selection) {
          const char* txt = ui_attr_get_text(&g_protocol_state.runtime, cid);
          if (txt != NULL) {
            int16_t draw_y = global_y;
            uint8_t y_u8 = (draw_y < 0) ? 0u : (uint8_t) draw_y;
            draw_masked_text(global_x,
                             draw_y,
                             txt,
                             y_u8,
                             (uint8_t) (y_u8 + 7),
                             page_top);
            highlight_text = txt;
          }
          break;
        }
        child_ix++;
      }
      if (highlight_text == NULL) {
        uint8_t y_off = (uint8_t) ((global_y < 0) ? 0 : (global_y % SSD1306_PAGE_HEIGHT));
        int     len   = 0;
        int     v     = (int) selection;
        label_buf[len++] = '[';
        if (v > 99) {
          v %= 100;
        }
        if (v > 9) {
          label_buf[len++] = (char) ('0' + (v / 10));
          label_buf[len++] = (char) ('0' + (v % 10));
        } else {
          label_buf[len++] = (char) ('0' + v);
        }
        label_buf[len++] = ']';
        label_buf[len]   = '\0';
        ssd1306_tile_text(draw_x, y_off, label_buf);
        highlight_text = label_buf;
      }
      uint8_t editing = barrel_is_editing(i);
      uint8_t blink_on = (editing != 0u && g_protocol_state.edit_blink_active != 0u)
                            ? edit_blink_visible()
                            : 1u;
      uint8_t should_invert = 0u;
      if (i == g_protocol_state.focused_element && owning_screen == active_screen_id &&
          !g_protocol_state.screen_anim.active) {
        if (editing == 0u || blink_on != 0u) {
          should_invert = 1u;
        }
      } else if (inline_list_selected != 0u) {
        should_invert = 1u;
      }
      if (should_invert != 0u) {
        const char* highlight_ref = (highlight_text != NULL) ? highlight_text : "";
        uint8_t     highlight_width = text_highlight_width(highlight_ref);
        int16_t draw_y = global_y;
        uint8_t y_u8 = (draw_y < 0) ? 0u : (uint8_t) draw_y;
        invert_row_region(draw_x,
                          highlight_width,
                          draw_y,
                          y_u8,
                          (uint8_t) (y_u8 + 7),
                          page_top);
      }
    }
  }
}

/** Format a fixed-point number into a buffer with up to RENDER_MAX_DECIMALS. */
/** Draw text within a vertical clip window and current tile page. */
static __attribute__((unused)) void draw_masked_text(int16_t     x,
                                                     int16_t     pixel_y,
                                                     const char* text,
                                                     uint8_t     viewport_top,
                                                     uint8_t     viewport_bottom,
                                                     uint8_t     page_top)
{
  if (!text) {
    return;
  }
  if (pixel_y > viewport_bottom || (pixel_y + 7) < viewport_top) {
    return;
  }
  if (pixel_y > (int16_t) (page_top + 7) || (pixel_y + 7) < page_top) {
    return;
  }
  uint8_t* buf = gfx_get_shared_buffer();
  int16_t  cx  = x;
  while (*text && cx < (int16_t) SSD1306_WIDTH) {
    uint8_t ch = (uint8_t) *text;
    if (ch < GFX_FONT_FIRST_CHAR || ch > GFX_FONT_LAST_CHAR) {
      ch = GFX_FONT_FIRST_CHAR;
    }
    const uint8_t* glyph = GFX_FONT_DATA[ch - GFX_FONT_FIRST_CHAR];
    for (uint8_t col = 0; col < GFX_FONT_CHAR_WIDTH && cx < (int16_t) SSD1306_WIDTH; col++) {
      uint8_t col_bits = glyph[col];
      if (col_bits) {
        uint8_t out_bits = 0;
        for (uint8_t b = 0; b < 8; b++) {
          if (!(col_bits & (1u << b))) {
            continue;
          }
          int16_t gy = pixel_y + b;
          if (gy < viewport_top || gy > viewport_bottom) {
            continue;
          }
          if (gy < page_top || gy > (int16_t) (page_top + 7)) {
            continue;
          }
          out_bits |= (uint8_t) (1u << (gy - page_top));
        }
        if (cx >= 0) {
          buf[(uint8_t) cx] |= out_bits;
        }
      }
      cx++;
    }
    if (cx < (int16_t) SSD1306_WIDTH) {
      cx++;
    }
    text++;
  }
}

/** Invert a horizontal region within the current tile page. */
static void invert_row_region(uint8_t start_x,
                              uint8_t width,
                              int16_t pixel_y,
                              uint8_t viewport_top,
                              uint8_t viewport_bottom,
                              uint8_t page_top)
{
  if (pixel_y > viewport_bottom || (pixel_y + 7) < viewport_top) {
    return;
  }
  if (pixel_y > (int16_t) (page_top + 7) || (pixel_y + 7) < page_top) {
    return;
  }
  if (start_x >= SSD1306_WIDTH) {
    return;
  }
  if (start_x + width >= SSD1306_WIDTH) {
    width = (uint8_t) (SSD1306_WIDTH - 1 - start_x);
  }
  uint8_t* buf = gfx_get_shared_buffer();
  for (uint8_t cx = 0; cx <= width; cx++) {
    uint8_t mask = 0;
    for (uint8_t b = 0; b < 8; b++) {
      int16_t gy = page_top + b;
      if (gy < pixel_y || gy > pixel_y + 7) {
        continue;
      }
      if (gy < viewport_top || gy > viewport_bottom) {
        continue;
      }
      mask |= (uint8_t) (1u << b);
    }
    buf[start_x + cx] ^= mask;
  }
}
