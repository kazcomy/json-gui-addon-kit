/**
 * @file debug_led_events.h
 * @brief Event identifier definitions for debug LED logging.
 */
#ifndef DEBUG_LED_EVENTS_H
#define DEBUG_LED_EVENTS_H

#include <stdint.h>

enum {
  DEBUG_LED_EVT_JSON_COMMIT        = 1u,
  DEBUG_LED_EVT_SET_ACTIVE_SCREEN  = 2u,
  DEBUG_LED_EVT_SCROLL_TO_SCREEN   = 3u,
  DEBUG_LED_EVT_SHOW_OVERLAY       = 4u,
  DEBUG_LED_EVT_OVERLAY_CLEAR      = 5u,
  DEBUG_LED_EVT_RENDER_SCREEN      = 6u,
  DEBUG_LED_EVT_RENDER_START       = 7u,
  DEBUG_LED_EVT_RENDER_STAGE       = 8u,
  DEBUG_LED_EVT_RENDER_DONE        = 9u
};

#endif /* DEBUG_LED_EVENTS_H */
