/**
 * @file ui_numeric.h
 * @brief Numeric value helpers for UI elements.
 */
#ifndef UI_NUMERIC_H
#define UI_NUMERIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void numeric_store(uint8_t id, int value, uint8_t aux);
void numeric_set_value(uint8_t id, int value);
void numeric_set_aux(uint8_t id, uint8_t aux);

#ifdef __cplusplus
}
#endif

#endif /* UI_NUMERIC_H */
