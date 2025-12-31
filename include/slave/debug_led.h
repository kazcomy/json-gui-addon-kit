/**
 * @file debug_led.h
 * @brief Debug LED event logging helpers.
 */
#ifndef DEBUG_LED_H
#define DEBUG_LED_H

#include <stdint.h>

#include "debug_led_events.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Queue an LED diagnostic event.
 *
 * @param type  Event type (mapped to pulse count).
 * @param value Optional 0-7 encoded parameter (e.g. screen id); set to 0xFF to omit.
 */
void debug_log_event(uint8_t type, uint8_t value);

/**
 * @brief Drive pending LED pulses; call regularly from the main loop.
 */
void debug_led_process(void);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_LED_H */
