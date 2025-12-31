/**
 * @file ui_layout.h
 * @brief Layout phase helpers for UI rendering.
 */
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Compute final coordinates for an element in the current screen/animation state. */
int ui_layout_compute_element(uint8_t element_id, int16_t* out_x, int16_t* out_y);

#ifdef __cplusplus
}
#endif

#endif /* UI_LAYOUT_H */
