/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

#define TAB_INDEX_GET(Obj)                                                                         \
	({                                                                                         \
		uint8_t tab_index___;                                                              \
		if (EINA_UNLIKELY(!_tab_index_get(Obj, &tab_index___))) {                          \
			return EINA_FALSE;                                                         \
		}                                                                                  \
		tab_index___;                                                                      \
	})

static Eina_Bool _tab_index_get(const msgpack_object *obj, uint8_t *index)
{
	/* It shall be of EXT type */
	CHECK_TYPE(obj, MSGPACK_OBJECT_EXT, EINA_FALSE);

	/* EXT type shall have subtype 2 (INT >= 0) */
	const msgpack_object_ext *const o = &(obj->via.ext);
	CHECK_TYPE(o, MSGPACK_OBJECT_POSITIVE_INTEGER, EINA_FALSE);

	/* I only handle indexes of size 1 byte */
	if (EINA_UNLIKELY(o->size != 1)) {
		CRI("Oops. I received a integer of %" PRIu32 " bytes. I don't know "
		    "how to handle that!",
		    o->size);
		return EINA_FALSE;
	}

	/* Get the index, on one byte */
	*index = (uint8_t)(o->ptr[0]);
	return EINA_TRUE;
}

Eina_Bool nvim_event_tabline_update(struct nvim *nvim, const msgpack_object_array *args)
{
	/*
    * The tabline update message accepts an array of two arguments
    * - the active tab of type EXT:2 (POSITIVE INT).
    * - the complete list of tabs, with a key "tab" which is the index
    *   and a key "name" which is the title of the tab
    */
	//   [{"tab"=>(ext: 2)"\x02", "name"=>"b"}, {"tab"=>(ext: 2)"\x01", "name"=>"a"}]
	CHECK_BASE_ARGS_COUNT(args, ==, 1);
	ARRAY_OF_ARGS_EXTRACT(args, params);
	CHECK_ARGS_COUNT(params, ==, 2);

	struct gui *const gui = &nvim->gui;

	/* Get the currently active tab */
	const msgpack_object *const o_sel = &(params->ptr[0]);
	const uint8_t current = TAB_INDEX_GET(o_sel);

	ARRAY_OF_ARGS_EXTRACT(params, tabs);

	gui_tabs_reset(gui);

	/* If we have no tabs or just one, we consider we have no tab at all,
    * and update the UI accordingly. And we stop here. */
	if (tabs->size <= 1) {
		gui_tabs_hide(gui);
		return EINA_TRUE;
	}

	gui_tabs_show(gui);

	for (unsigned int i = 0; i < tabs->size; i++) {
		const msgpack_object *const o_tab = &tabs->ptr[i];
		const msgpack_object_map *const map = MPACK_MAP_EXTRACT(o_tab, goto fail);
		const msgpack_object *o_key, *o_val;
		unsigned int it;

		uint8_t tab_id = UINT8_MAX;
		Eina_Stringshare *tab_name = NULL;

		MPACK_MAP_ITER (map, it, o_key, o_val) {
			const msgpack_object_str *const key =
				MPACK_STRING_OBJ_EXTRACT(o_key, goto fail);
			if (!strncmp(key->ptr, "tab", key->size))
				tab_id = TAB_INDEX_GET(o_val);
			else if (!strncmp(key->ptr, "name", key->size))
				tab_name = MPACK_STRING_EXTRACT(o_val, goto fail);
			else
				ERR("Invalid key name");
		}

		if ((!tab_name) || (tab_id == UINT8_MAX)) {
			ERR("Failed to extract tab information");
			continue;
		}
		gui_tabs_add(gui, tab_name, tab_id, (tab_id == current) ? EINA_TRUE : EINA_FALSE);
	}

	return EINA_TRUE;
fail:
	return EINA_FALSE;
}
