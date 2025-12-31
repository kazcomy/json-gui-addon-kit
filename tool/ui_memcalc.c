/**
 * @file ui_memcalc.c
 * @brief Host-side memory usage helpers backed by the real protocol parser.
 */
#include <stdint.h>

#include "status_codes.h"
#include "ui_protocol.h"

void ui_memcalc_reset(void)
{
  protocol_reset_state();
}

int ui_memcalc_apply_object(const char* buf, uint16_t len, uint8_t flags)
{
  if (len > 255u) {
    return RES_BAD_LEN;
  }
  return protocol_apply_json_object(buf, (uint8_t) len, flags);
}

void ui_memcalc_get_usage(uint16_t* head_used,
                          uint16_t* tail_used,
                          uint8_t*  element_count,
                          uint8_t*  element_capacity)
{
  if (head_used) {
    *head_used = g_protocol_state.runtime.head_used;
  }
  if (tail_used) {
    *tail_used = g_protocol_state.runtime.used_tail;
  }
  if (element_count) {
    *element_count = g_protocol_state.element_count;
  }
  if (element_capacity) {
    *element_capacity = g_protocol_state.element_capacity;
  }
}

uint16_t ui_memcalc_get_arena_cap(void)
{
  return (uint16_t) UI_ATTR_ARENA_CAP;
}
