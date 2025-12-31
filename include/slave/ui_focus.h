/**
 * @file ui_focus.h
 * @brief Focus/navigation helpers for UI.
 */
#ifndef UI_FOCUS_H
#define UI_FOCUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t protocol_is_element_visible(uint8_t element_id);
void protocol_register_local_screen(uint8_t screen_id, uint8_t owner_text);
uint8_t protocol_text_local_screen(uint8_t text_id);

void protocol_set_focus(uint8_t element_id);
uint8_t protocol_get_focused(void);
void protocol_clear_focus(void);
void protocol_focus_next(void);
void protocol_focus_prev(void);
void protocol_focus_first_on_screen(uint8_t screen_ordinal);

uint8_t nav_push_list(uint8_t parent_list, uint8_t target_list);
uint8_t nav_push_local_screen(uint8_t parent_list, uint8_t screen_id);
uint8_t nav_pop(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_FOCUS_H */
