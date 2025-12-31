/**
 * @file ui_runtime.c
 * @brief Shared arena helpers for runtime nodes and attributes.
 */
#include "ui_runtime.h"

#include "status_codes.h"
#include "ui_protocol.h" /* for g_protocol_state */

#include <string.h>

void ur_init(ui_runtime_t* rt)
{
	if (!rt) {
		return;
	}
	memset(rt, 0, sizeof(*rt));
}

void* ur__ptr(ui_runtime_t* rt, ur_off_t off)
{
	return (off == 0) ? (void*) 0 : (void*) (rt->arena + off);
}

ur_off_t ur__off(ui_runtime_t* rt, void* p)
{
	if (!p) {
		return 0;
	}
	uintptr_t base = (uintptr_t) rt->arena;
	uintptr_t cur  = (uintptr_t) p;
	if (cur < base || cur >= base + (uintptr_t) UI_ATTR_ARENA_CAP) {
		return 0;
	}
	return (ur_off_t) (cur - base);
}

void* ur__alloc_tail(ui_runtime_t* rt, uint16_t size)
{
	uint16_t head = rt->head_used;
	if ((uint32_t) head + (uint32_t) rt->used_tail + (uint32_t) size > (uint32_t) UI_ATTR_ARENA_CAP) {
		return (void*) 0; /* out of space */
	}
	uint16_t new_off = (uint16_t) (UI_ATTR_ARENA_CAP - rt->used_tail - size);
	rt->used_tail    = (uint16_t) (rt->used_tail + size);
	return (void*) (rt->arena + new_off);
}

ur_list_state_t* ur_list_find(ui_runtime_t* rt, uint8_t element_id)
{
	ur_off_t cur = rt->lists_head_off;
	while (cur) {
		ur_list_node_t* n = (ur_list_node_t*) ur__ptr(rt, cur);
		if (!n) break;
		if (n->st.element_id == element_id) return &n->st;
		cur = n->next_off;
	}
	return (ur_list_state_t*) 0;
}

ur_list_state_t* ur_list_get_or_add(ui_runtime_t* rt, uint8_t element_id)
{
	ur_list_state_t* s = ur_list_find(rt, element_id);
	if (s) return s;
	ur_list_node_t* n = (ur_list_node_t*) ur__alloc_tail(rt, (uint16_t) sizeof(ur_list_node_t));
	if (!n) return 0;
	n->next_off            = rt->lists_head_off;
	n->st.element_id       = element_id;
	n->st.cursor           = 0;
	n->st.top_index        = 0;
	n->st.visible_rows     = 4;
	n->st.anim_active      = 0;
	n->st.anim_dir         = 0;
	n->st.anim_pix         = 0;
	n->st.pending_top      = 0;
	n->st.pending_cursor   = 0;
	n->st.last_text_child  = UR_INVALID_ELEMENT_ID;
	rt->lists_head_off     = ur__off(rt, n);
	return &n->st;
}

ur_trigger_state_t* ur_trigger_find(ui_runtime_t* rt, uint8_t element_id)
{
	ur_off_t cur = rt->triggers_head_off;
	while (cur) {
		ur_trigger_node_t* n = (ur_trigger_node_t*) ur__ptr(rt, cur);
		if (!n) break;
		if (n->st.element_id == element_id) return &n->st;
		cur = n->next_off;
	}
	return (ur_trigger_state_t*) 0;
}

ur_trigger_state_t* ur_trigger_get_or_add(ui_runtime_t* rt, uint8_t element_id)
{
	ur_trigger_state_t* found = ur_trigger_find(rt, element_id);
	if (found) return found;
	ur_trigger_node_t* n = (ur_trigger_node_t*) ur__alloc_tail(rt, (uint16_t) sizeof(ur_trigger_node_t));
	if (!n) return (ur_trigger_state_t*) 0;
	n->next_off           = rt->triggers_head_off;
	n->st.element_id      = element_id;
	n->st.version         = 0;
	rt->triggers_head_off = ur__off(rt, n);
	return &n->st;
}

ur_barrel_state_t* ur_barrel_find(ui_runtime_t* rt, uint8_t element_id)
{
	ur_off_t cur = rt->barrels_head_off;
	while (cur) {
		ur_barrel_node_t* n = (ur_barrel_node_t*) ur__ptr(rt, cur);
		if (!n) break;
		if (n->st.element_id == element_id) return &n->st;
		cur = n->next_off;
	}
	return (ur_barrel_state_t*) 0;
}

ur_barrel_state_t* ur_barrel_get_or_add(ui_runtime_t* rt, uint8_t element_id)
{
	ur_barrel_state_t* s = ur_barrel_find(rt, element_id);
	if (s) return s;
	ur_barrel_node_t* n = (ur_barrel_node_t*) ur__alloc_tail(rt, (uint16_t) sizeof(ur_barrel_node_t));
	if (!n) return (ur_barrel_state_t*) 0;
	n->next_off       = rt->barrels_head_off;
	n->st.element_id  = element_id;
	n->st.aux         = 0;
	n->st.value       = 0;
	rt->barrels_head_off = ur__off(rt, n);
	return &n->st;
}

/* ------------------------------------------------------------------------- */
/** Attribute arena helpers (head allocation). */

uint16_t ui_attr_get_memory_usage(ui_runtime_t* rt)
{
	if (!rt) {
		return 0u;
	}
	return rt->head_used;
}

/** Return byte length of a single attribute entry starting at p. */
static uint16_t ui_attr_skip_entry(const uint8_t* p)
{
	uint8_t tag = p[0];
	switch (tag) {
		case UI_ATTR_TAG_TEXT: {
			uint8_t size = p[2];
			return (uint16_t)(UI_ATTR_SIZE_TEXT_HDR + size); /* tag,element_id,len,data (includes NUL space) */
		}
		case UI_ATTR_TAG_SCREEN_ROLE: return UI_ATTR_SIZE_SCREEN_ROLE;
		default: return 0u; /* Corrupt */
	}
}

/** Locate an attribute entry by element id + tag inside the arena. */
static uint8_t* ui_attr_find(ui_runtime_t* rt, uint8_t element_id, uint8_t tag)
{
	if (!rt) {
		return 0;
	}
	if (element_id >= g_protocol_state.element_capacity) return 0;
	uint16_t off = rt->attr_base;
	uint8_t* base = &rt->arena[0];
	while (off < rt->head_used) {
		uint8_t* e = &base[off];
		if (e[0] == tag && e[1] == element_id) {
			return e;
		}
		uint16_t adv = ui_attr_skip_entry(e);
		if (adv == 0u) break;
		off = (uint16_t)(off + adv);
	}
	return 0;
}

/** Append a new attribute entry to the arena during provisioning. */
static int ui_attr_append(ui_runtime_t* rt,
                          uint8_t       element_id,
                          uint8_t       tag,
                          const void*   payload,
                          uint8_t       len_prefix,
                          uint16_t      payload_len)
{
	/* Appends are only allowed during JSON init/build phase (before COMMIT). */
	if (g_protocol_state.initialized) {
		return RES_BAD_STATE;
	}
	if (element_id >= g_protocol_state.element_capacity) {
		return RES_RANGE;
	}
	if (!rt) {
		return RES_BAD_STATE;
	}
	uint16_t need = (uint16_t)(2u + payload_len + (len_prefix ? 1u : 0u));
	if ((uint32_t) rt->head_used + (uint32_t) need + (uint32_t) rt->used_tail > (uint32_t) UI_ATTR_ARENA_CAP) {
		return RES_NO_SPACE;
	}
	/* Append at arena end. */
	uint8_t* e = &rt->arena[rt->head_used];
	e[0] = tag;
	e[1] = element_id;
	uint16_t pos = 2u;
	if (len_prefix) { e[2] = (uint8_t)payload_len; pos = 3u; }
	if (payload_len && payload) { memcpy(&e[pos], payload, payload_len); }
	rt->head_used = (uint16_t)(rt->head_used + need);
	return RES_OK;
}

int ui_attr_store_text_with_cap(ui_runtime_t* rt,
                                uint8_t       element_id,
                                const char*   text,
                                uint8_t       capacity)
{
	uint8_t len = 0u;
	if (text) { while (text[len]) len++; }
	uint8_t* e = ui_attr_find(rt, element_id, UI_ATTR_TAG_TEXT);
	if (e) {
		ui_attr_text_entry_t* t = (ui_attr_text_entry_t*)e;
		uint8_t size = t->len; /* store field now encodes allocated payload size incl NUL */
		if (size == 0u) return RES_NO_SPACE;
		/* Write up to size-1 and NUL-terminate */
		uint8_t w = (len < (uint8_t)(size - 1u)) ? len : (uint8_t)(size - 1u);
		if (w) memcpy(t->data, text, w);
		t->data[w] = '\0';
		return RES_OK;
	}
	/* Determine capacity: if zero, use len; protocol clamps to <=20 when provided */
	uint8_t cap = capacity ? capacity : len;
	/* Append header then write directly to arena to avoid large stack buffers. */
	int res = ui_attr_append(rt, element_id, UI_ATTR_TAG_TEXT, 0, 1u, (uint16_t)(cap + 1u));
	if (res != RES_OK) return res;
	uint8_t* e2 = ui_attr_find(rt, element_id, UI_ATTR_TAG_TEXT);
	if (!e2) return RES_UNKNOWN_ID;
	ui_attr_text_entry_t* t = (ui_attr_text_entry_t*)e2;
	uint8_t w = (len < cap) ? len : cap;
	if (w && text) memcpy(t->data, text, w);
	t->data[w] = '\0';
	return RES_OK;
}

int ui_attr_store_text(ui_runtime_t* rt, uint8_t element_id, const char* text)
{
	return ui_attr_store_text_with_cap(rt, element_id, text, 0u);
}

const char* ui_attr_get_text(ui_runtime_t* rt, uint8_t element_id)
{
	uint8_t* e = ui_attr_find(rt, element_id, UI_ATTR_TAG_TEXT);
	if (!e) return 0;
	ui_attr_text_entry_t* t = (ui_attr_text_entry_t*)e;
	return (const char*)t->data; /* points to first char */
}

int ui_attr_update_text(ui_runtime_t* rt, uint8_t element_id, const char* new_text)
{
	uint8_t* e = ui_attr_find(rt, element_id, UI_ATTR_TAG_TEXT);
	if (!e) return RES_UNKNOWN_ID;
	ui_attr_text_entry_t* t = (ui_attr_text_entry_t*)e;
	uint8_t size = t->len; /* allocated payload size including NUL */
	uint8_t nlen = 0u;
	if (new_text) while (new_text[nlen]) nlen++;
	if (size == 0u) return RES_NO_SPACE;
	uint8_t w = (nlen < (uint8_t)(size - 1u)) ? nlen : (uint8_t)(size - 1u);
	if (w) memcpy(t->data, new_text, w);
	t->data[w] = '\0';
	return RES_OK;
}

int ui_attr_store_position(ui_runtime_t* rt,
                           uint8_t       element_id,
                           uint8_t       x,
                           uint8_t       y,
                           uint8_t       font_size,
                           uint8_t       layout_type)
{
	(void)rt;
	(void)font_size;  /* Fixed font size externally (currently 8). */
	(void)layout_type; /* Layout not stored (renderer uses absolute only). */
	if (g_protocol_state.pos_x == NULL || g_protocol_state.pos_y == NULL) { return RES_BAD_STATE; }
	if (element_id >= g_protocol_state.element_capacity) { return RES_RANGE; }
	g_protocol_state.pos_x[element_id] = x;
	g_protocol_state.pos_y[element_id] = y;
	return RES_OK;
}

int ui_attr_get_position(ui_runtime_t* rt,
                         uint8_t       element_id,
                         uint8_t*      x,
                         uint8_t*      y,
                         uint8_t*      font_size,
                         uint8_t*      layout_type)
{
	(void)rt;
	if (element_id >= g_protocol_state.element_count) { return RES_UNKNOWN_ID; }
	if (g_protocol_state.pos_x == NULL || g_protocol_state.pos_y == NULL) {
		return RES_BAD_STATE;
	}
	*x = g_protocol_state.pos_x[element_id];
	*y = g_protocol_state.pos_y[element_id];
	*layout_type = LAYOUT_ABSOLUTE;
	*font_size   = 8;
	return RES_OK;
}

int ui_attr_store_screen_role(ui_runtime_t* rt, uint8_t element_id, uint8_t role)
{
	uint8_t* e = ui_attr_find(rt, element_id, UI_ATTR_TAG_SCREEN_ROLE);
	if (e) {
		ui_attr_screen_role_entry_t* sr = (ui_attr_screen_role_entry_t*)e;
		sr->role = role;
		return RES_OK;
	}
	return ui_attr_append(rt, element_id, UI_ATTR_TAG_SCREEN_ROLE, &role, 0u, 1u);
}

int ui_attr_get_screen_role(ui_runtime_t* rt, uint8_t element_id, uint8_t* out_role)
{
	if (!out_role) {
		return RES_BAD_LEN;
	}
	uint8_t* e = ui_attr_find(rt, element_id, UI_ATTR_TAG_SCREEN_ROLE);
	if (!e) {
		return RES_UNKNOWN_ID;
	}
	ui_attr_screen_role_entry_t* sr = (ui_attr_screen_role_entry_t*)e;
	*out_role = sr->role;
	return RES_OK;
}
