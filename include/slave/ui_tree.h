/**
 * @file ui_tree.h
 * @brief UI tree helpers (parent/child/screen lookups).
 */
#ifndef UI_TREE_H
#define UI_TREE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t list_item_count(uint8_t list_eid);
uint8_t list_row_count(uint8_t list_eid);
uint8_t list_child_by_index(uint8_t list_eid, uint8_t row_index);
uint8_t list_row_index_of_text(uint8_t list_eid, uint8_t text_eid);
uint8_t text_inline_barrel_id(uint8_t text_eid);
uint8_t element_parent_list(uint8_t eid);
uint8_t element_root_screen(uint8_t eid);
uint8_t find_screen_id_by_ordinal(uint8_t sord);
uint8_t find_screen_ordinal_by_id(uint8_t screen_id);
uint8_t is_descendant_of(uint8_t eid, uint8_t ancestor);

#ifdef __cplusplus
}
#endif

#endif /* UI_TREE_H */
