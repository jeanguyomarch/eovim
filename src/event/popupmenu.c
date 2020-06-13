/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

static Eina_Bool wildmenu_show(struct nvim *const nvim, const ssize_t selected, t_int col,
			       const msgpack_object_array *const args)
{
	struct gui *const gui = &nvim->gui;

	/* Sanitize the column index. That's the position of the char to be completed */
	if (EINA_UNLIKELY((uint64_t)col > UINT_MAX)) {
		ERR("Invalid column number for wildmenu");
		col = 0;
	}

	/* Go through all the items to be added to the wildmenu, and populate
	 * the UI interface */
	for (unsigned int i = 0; i < args->size; i++) {
		CHECK_TYPE(&args->ptr[i], MSGPACK_OBJECT_ARRAY, EINA_FALSE);
		const msgpack_object_array *const item = &(args->ptr[i].via.array);
		CHECK_ARGS_COUNT(item, ==, 4);
		Eina_Stringshare *const name =
			MPACK_STRING_EXTRACT(&item->ptr[0], return EINA_FALSE);
		gui_wildmenu_append(gui, name);
	}
	gui_wildmenu_show(gui, (unsigned int)col);
	gui_active_popupmenu_select_nth(gui, selected);
	return EINA_TRUE;
}

Eina_Bool nvim_event_popupmenu_show(struct nvim *const nvim, const msgpack_object_array *const args)
{
	/* We receive this:
	 *   ["popupmenu_show", items, selected, row, col, grid]
	 *
	 * where items is an array of 4-tuples:
	 *   [word, kind, menu, info]
	 *
	 * For example:
	 *   ["popupmenu_show", [[["CMakeCache.txt", "", "", ""], ...
	 *
	 * Note that at some point, neovim had a "wildmenu" option, which was
	 * separated from the popupmenu... but not anymore. The wildmenu is send
	 * through the popupmenu_show event with a grid value of -1.
	 */
	CHECK_BASE_ARGS_COUNT(args, ==, 1);
	ARRAY_OF_ARGS_EXTRACT(args, params);
	CHECK_ARGS_COUNT(params, >=, 5);

	const msgpack_object *const data_obj = &(params->ptr[0]);
	CHECK_TYPE(data_obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
	const msgpack_object_array *const data = &(data_obj->via.array);
	struct gui *const gui = &nvim->gui;

	t_int selected, row, col, grid;
	GET_ARG(params, 1, t_int, &selected);
	GET_ARG(params, 2, t_int, &row);
	GET_ARG(params, 3, t_int, &col);
	GET_ARG(params, 4, t_int, &grid);

	/* This is the wildmenu! We may render it differently */
	if (grid == -1)
		return wildmenu_show(nvim, (ssize_t)selected, col, data);

	gui_completion_reset(gui);

	/* If we are here, this is the popupmenu used by completions */
	for (unsigned int i = 0u; i < data->size; i++) {
		CHECK_TYPE(&data->ptr[i], MSGPACK_OBJECT_ARRAY, EINA_FALSE);
		const msgpack_object_array *const completion = &(data->ptr[i].via.array);
		CHECK_ARGS_COUNT(completion, >=, 4);

		MPACK_STRING_CHECK(&completion->ptr[0], goto fail); /* word */
		MPACK_STRING_CHECK(&completion->ptr[1], goto fail); /* kind */
		MPACK_STRING_CHECK(&completion->ptr[2], goto fail); /* menu */
		MPACK_STRING_CHECK(&completion->ptr[3], goto fail); /* info */

		gui_completion_append(
			gui, completion->ptr[0].via.str.ptr, completion->ptr[0].via.str.size,
			completion->ptr[1].via.str.ptr, completion->ptr[1].via.str.size,
			completion->ptr[2].via.str.ptr, completion->ptr[2].via.str.size,
			completion->ptr[3].via.str.ptr, completion->ptr[3].via.str.size);
	}
	gui_completion_show(gui, (unsigned int)col, (unsigned int)row);
	gui_active_popupmenu_select_nth(gui, selected);

	return EINA_TRUE;
fail:
	return EINA_FALSE;
}

Eina_Bool nvim_event_popupmenu_hide(struct nvim *const nvim,
				    const msgpack_object_array *const args EINA_UNUSED)
{
	gui_active_popupmenu_hide(&nvim->gui);
	return EINA_TRUE;
}

Eina_Bool nvim_event_popupmenu_select(struct nvim *const nvim,
				      const msgpack_object_array *const args)
{
	/* We expect this:
	 *   ["popupmenu_select", selected]
	 *
	 * which can be something like this:
	 *   ["popupmenu_select", [3], [4], [5], [6], [7]]
	 *
	 * We always take the last selected index.
	 */
	CHECK_BASE_ARGS_COUNT(args, >=, 1);
	const msgpack_object_array *const params = array_of_args_extract(args, args->size - 1);
	if (EINA_UNLIKELY(!params))
		return EINA_FALSE;
	CHECK_ARGS_COUNT(params, >=, 1);

	t_int selected;
	GET_ARG(params, 0, t_int, &selected);
	gui_active_popupmenu_select_nth(&nvim->gui, (ssize_t)selected);

	return EINA_TRUE;
}
