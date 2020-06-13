/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

Eina_Bool nvim_event_cmdline_show(struct nvim *nvim, const msgpack_object_array *args)
{
	Eina_Bool ret = EINA_FALSE;
	CHECK_BASE_ARGS_COUNT(args, ==, 1);
	ARRAY_OF_ARGS_EXTRACT(args, params);
	CHECK_ARGS_COUNT(params, ==, 6);

	/*
	 * The arguments of cmdline_show are:
	 *
	 * [0]: content
	 * [1]: cursor position (int)
	 * [2]: first character (string)
	 * [3]: prompt (string)
	 * [4]: indentation (int)
	 * [5]: level (int)
	 */
	const msgpack_object_array *const content = MPACK_ARRAY_EXTRACT(&params->ptr[0], goto fail);
	const int64_t pos = MPACK_INT64_EXTRACT(&params->ptr[1], goto fail);
	Eina_Stringshare *const firstc = MPACK_STRING_EXTRACT(&params->ptr[2], goto fail);
	Eina_Stringshare *const prompt = MPACK_STRING_EXTRACT(&params->ptr[3], goto del_firstc);
	const int64_t indent = MPACK_INT64_EXTRACT(&params->ptr[4], goto del_prompt);
	//const int64_t level =
	//   MPACK_INT64_EXTRACT(&params->ptr[5], del_prompt);

	/* Create the string buffer, which will hold the content of the cmdline */
	Eina_Strbuf *const buf = eina_strbuf_new();
	if (EINA_UNLIKELY(!buf)) {
		CRI("Failed to allocate memory");
		goto del_prompt;
	}

	/* Add to the content of the command-line the number of spaces the text
	 * should be indented of */
	for (size_t i = 0; i < (size_t)indent; i++)
		eina_strbuf_append_char(buf, ' ');

	for (unsigned int i = 0; i < content->size; i++) {
		const msgpack_object_array *const cont =
			MPACK_ARRAY_EXTRACT(&content->ptr[i], goto del_buf);

		/* The map will contain highlight attributes */
		//const msgpack_object_map *const map =
		//   MPACK_MAP_EXTRACT(&cont->ptr[0], goto del_buf);

		/* Extract the content of the command-line */
		const msgpack_object *const cont_o = &(cont->ptr[1]);
		MPACK_STRING_CHECK(cont_o, goto del_buf);
		const msgpack_object_str *const str = &(cont_o->via.str);
		eina_strbuf_append_length(buf, str->ptr, str->size);
	}

	gui_cmdline_show(&nvim->gui, eina_strbuf_string_get(buf), prompt, firstc);

	/* Set the cursor position within the command-line */
	gui_cmdline_cursor_pos_set(&nvim->gui, (size_t)pos);

	ret = EINA_TRUE;
del_buf:
	eina_strbuf_free(buf);
del_prompt:
	eina_stringshare_del(prompt);
del_firstc:
	eina_stringshare_del(firstc);
fail:
	return ret;
}

Eina_Bool nvim_event_cmdline_pos(struct nvim *nvim, const msgpack_object_array *args)
{
	CHECK_BASE_ARGS_COUNT(args, ==, 1);
	ARRAY_OF_ARGS_EXTRACT(args, params);
	CHECK_ARGS_COUNT(params, ==, 2);

	/* First argument if the position, second is the level. Level is not
    * handled for now.
    */
	const int64_t pos = MPACK_INT64_EXTRACT(&params->ptr[0], return EINA_FALSE);
	//const int64_t level =
	//   MPACK_INT64_EXTRACT(&params->ptr[1], fail);

	gui_cmdline_cursor_pos_set(&nvim->gui, (size_t)pos);
	return EINA_TRUE;
}

Eina_Bool nvim_event_cmdline_special_char(struct nvim *nvim EINA_UNUSED,
					  const msgpack_object_array *args EINA_UNUSED)
{
	CRI("unimplemented");
	return EINA_TRUE;
}

Eina_Bool nvim_event_cmdline_hide(struct nvim *nvim, const msgpack_object_array *args EINA_UNUSED)
{
	gui_cmdline_hide(&nvim->gui);
	return EINA_TRUE;
}

Eina_Bool nvim_event_cmdline_block_show(struct nvim *nvim EINA_UNUSED,
					const msgpack_object_array *args EINA_UNUSED)
{
	CRI("Blocks in cmdline is currently not supported. Sorry.");
	return EINA_TRUE;
}

Eina_Bool nvim_event_cmdline_block_append(struct nvim *nvim EINA_UNUSED,
					  const msgpack_object_array *args EINA_UNUSED)
{
	CRI("Blocks in cmdline is currently not supported. Sorry.");
	return EINA_TRUE;
}

Eina_Bool nvim_event_cmdline_block_hide(struct nvim *nvim EINA_UNUSED,
					const msgpack_object_array *args EINA_UNUSED)
{
	CRI("Blocks in cmdline is currently not supported. Sorry.");
	return EINA_TRUE;
}
