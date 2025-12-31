/**
 * @file ui_buttons.h
 * @brief Shared logical button indices used across master and slave.
 */
#ifndef UI_BUTTONS_H
#define UI_BUTTONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logical button indices expected by SPI_CMD_INPUT_EVENT.
 */
typedef enum {
  UI_BUTTON_UP = 0u,    /**< Up navigation button. */
  UI_BUTTON_DOWN = 1u,  /**< Down navigation button. */
  UI_BUTTON_OK = 2u,    /**< OK/confirm button. */
  UI_BUTTON_BACK = 3u,  /**< Back/cancel button. */
  UI_BUTTON_LEFT = 4u,  /**< Left navigation button. */
  UI_BUTTON_RIGHT = 5u, /**< Right navigation button. */
  UI_BUTTON_COUNT = 6u  /**< Total number of logical buttons. */
} ui_button_index_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_BUTTONS_H */
