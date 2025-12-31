/**
 * @file ui_input.h
 * @brief Input handling for UI (buttons/events).
 */
#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* p[0]=button_index, p[1]=event (0=release,1=press). */
int cmd_input_event(uint8_t* p, uint8_t l);

#ifdef __cplusplus
}
#endif

#endif /* UI_INPUT_H */
