/**
 * @file ui_numeric.c
 * @brief Numeric value helpers for UI elements.
 */
#include "ui_numeric.h"

#include "ui_protocol.h"

void numeric_store(uint8_t id, int value, uint8_t aux)
{
  if (id >= g_protocol_state.element_count) {
    return;
  }
  if (value < -32768) {
    value = -32768;
  } else if (value > 32767) {
    value = 32767;
  }
  ur_barrel_state_t* st = ur_barrel_get_or_add(&g_protocol_state.runtime, id);
  if (!st) {
    return;
  }
  st->value = (int16_t) value;
  st->aux   = aux;
}

void numeric_set_value(uint8_t id, int value)
{
  if (id >= g_protocol_state.element_count) {
    return;
  }
  if (value < -32768) {
    value = -32768;
  } else if (value > 32767) {
    value = 32767;
  }
  ur_barrel_state_t* st = ur_barrel_get_or_add(&g_protocol_state.runtime, id);
  if (!st) {
    return;
  }
  st->value = (int16_t) value;
}

void numeric_set_aux(uint8_t id, uint8_t aux)
{
  if (id >= g_protocol_state.element_count) {
    return;
  }
  ur_barrel_state_t* st = ur_barrel_get_or_add(&g_protocol_state.runtime, id);
  if (!st) {
    return;
  }
  st->aux = aux;
}
