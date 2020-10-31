/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

/** Function type used to decode an attribute */
typedef Eina_Bool (*f_hl_attr_decode)(const msgpack_object *, struct style *);

/** Hash table that maps attributes names to decoding functions */
static Eina_Hash *_attributes;

/* The are quite a lot of attributes, which would lead to DEDIOUS hand-crafted
 * code. I'm not usually a fan of generating codes via macros, but I think
 * there is real gain here
 *
 *  Keyword             DecodeFunc              Field Name (of struct style)
 */
#define ATTRIBUTES(X)                                                                              \
	X(foreground, arg_color_get, fg_color)                                                     \
	X(background, arg_color_get, bg_color)                                                     \
	X(special, arg_color_get, sp_color)                                                        \
	X(reverse, arg_bool_get, reverse)                                                          \
	X(italic, arg_bool_get, italic)                                                            \
	X(bold, arg_bool_get, bold)                                                                \
	X(underline, arg_bool_get, underline)                                                      \
	X(undercurl, arg_bool_get, undercurl)                                                      \
	X(strikethrough, arg_bool_get, strikethrough)                                              \
	/**/

#define GEN_DECODERS(Kw, DecodeFunc, FieldName)                                                    \
	static Eina_Bool _attr_##Kw##_cb(const msgpack_object *const obj,                          \
					 struct style *const style)                       \
	{                                                                                          \
		return DecodeFunc(obj, &style->FieldName);                                         \
	}
ATTRIBUTES(GEN_DECODERS)
#undef GEN_DECODERS

Eina_Bool nvim_event_default_colors_set(struct nvim *const nvim,
					const msgpack_object_array *const args)
{
	/* We expect this:
	 *   ["default_colors_set", rgb_fg, rgb_bg, rgb_sp, cterm_fg, cterm_bg]
	 *
	 * For example:
	 *   ["default_colors_set", [16777215, 0, 16711680, 0, 0]]
	 *
	 * We don't care about cterm_bg and cterm_fg
	 *
	 * But note that we can have several arrays of "default colors". We
	 * will always use the last one.
	 */
	CHECK_BASE_ARGS_COUNT(args, >=, 1);

	const msgpack_object_array *const params = array_of_args_extract(args, args->size - 1);
	if (!params)
		return EINA_FALSE;
	CHECK_ARGS_COUNT(params, >=, 3);

	union color fg = { .value = 0 };
	union color bg = { .value = 0 };
	union color sp = { .value = 0 };

	GET_OPT_ARG(params, 0, color, &fg);
	GET_OPT_ARG(params, 1, color, &bg);
	GET_OPT_ARG(params, 2, color, &sp);

	gui_default_colors_set(&nvim->gui, fg, bg, sp);
	return EINA_TRUE;
}

static Eina_Bool hi_name_set(struct nvim *const nvim, const t_int id,
			     const msgpack_object *const hi_name)
{
	Eina_Stringshare *const key = MPACK_STRING_EXTRACT(hi_name, return EINA_FALSE);
	const struct style *const style = gui_style_get(&nvim->gui, id);
	if (EINA_UNLIKELY(!style)) {
		ERR("Failed find style with id %" PRIi64, id);
		return EINA_FALSE;
	}

	eina_hash_set(nvim->hl_groups, key, style);
	return EINA_TRUE;
}

Eina_Bool nvim_event_hl_attr_define(struct nvim *const nvim, const msgpack_object_array *const args)
{
	/* We expect this:
	 *  ["hl_attr_define", id, rgb_attr, cterm_attr, info]
	 *
	 * For example:
	 *  ["hl_attr_define", [1, {}, {}, [<INFO>]],
	 *     [2, {"foreground"=>13882323, "background"=>11119017},
	 *          {"foreground"=>7, "background"=>242}, []] ... ]
	 *
	 * where <INFO> is  something like:
	 *	{"kind"=>"ui", "ui_name"=>"NormalFloat", "hi_name"=>"Pmenu", "id"=>396}
	 *
	 * Note that the "id" shall be the same than in the hl_attr_define.
	 *
	 * All arguments but "id" are optional.
	 * We ignore cterm_attr.
	 * We ignore info, because we have no use for ext_hlstate
	 */

	Eina_Bool ret = EINA_TRUE;
	CHECK_BASE_ARGS_COUNT(args, >=, 1u);
	/* For each array [id, rgb_attr, ...] */
	for (uint32_t i = 1u; i < args->size; i++) {
		const msgpack_object_array *const opt =
			MPACK_ARRAY_EXTRACT(&args->ptr[i], goto fail);
		CHECK_ARGS_COUNT(opt, >=, 4);

		/* First argument is the ID */
		t_int id;
		GET_ARG(opt, 0, t_int, &id);

		/* Grab the style to be changed */
		struct style *const style = gui_style_get(&nvim->gui, id);
		if (EINA_UNLIKELY(!style))
			goto fail;

		const msgpack_object_map *const map = MPACK_MAP_EXTRACT(&opt->ptr[1], continue);

		/* Iterate over each argument of the key-value map */
		const msgpack_object *o_key, *o_val;
		unsigned int it;
		MPACK_MAP_ITER (map, it, o_key, o_val) {
			Eina_Stringshare *const key = MPACK_STRING_EXTRACT(o_key, goto fail);
			const f_hl_attr_decode func = eina_hash_find(_attributes, key);
			if (EINA_UNLIKELY(!func))
				WRN("Unhandled attribute '%s'", key);
			else
				ret &= func(o_val, style);
			eina_stringshare_del(key);
		}

		/* Extract the 'info' argument */
		const msgpack_object_array *const info_arr =
			MPACK_ARRAY_EXTRACT(&opt->ptr[3], goto fail);
		if (EINA_UNLIKELY(info_arr->size == 0)) {
			ERR("Unexpected empty array");
			continue;
		}
		const msgpack_object_map *const info =
			MPACK_MAP_EXTRACT(&info_arr->ptr[info_arr->size - 1], continue);
		MPACK_MAP_ITER (info, it, o_key, o_val) {
			const msgpack_object_str *const key =
				MPACK_STRING_OBJ_EXTRACT(o_key, goto fail);
			if (_MSGPACK_STREQ(key, "hi_name"))
				ret &= hi_name_set(nvim, id, o_val);
		}
	}
	gui_style_update_defer(&nvim->gui);

	return ret;
fail:
	return EINA_FALSE;
}

Eina_Bool nvim_event_hl_group_set(struct nvim *const nvim EINA_UNUSED,
				  const msgpack_object_array *const args EINA_UNUSED)
{
	return EINA_TRUE;
}

Eina_Bool nvim_event_grid_resize(struct nvim *const nvim, const msgpack_object_array *const args)
{
	/* We expect this:
	 *   ["grid_resize", grid, width, height]
	 *
	 * Example:
	 *   ["grid_resize", [1, 120, 40]]
	 */

	CHECK_BASE_ARGS_COUNT(args, >=, 1u);
	for (uint32_t i = 1u; i < args->size; i++) {
		const msgpack_object_array *const opt =
			MPACK_ARRAY_EXTRACT(&args->ptr[i], goto fail);
		CHECK_ARGS_COUNT(opt, >=, 3);

		t_int grid_id, width, height;
		GET_ARG(opt, 0, t_int, &grid_id);
		/* TODO: for now, we don't implement multi_grid, so we just consider the
		 * grid ID ALWAYS refers to THE grid */
		EINA_SAFETY_ON_FALSE_RETURN_VAL(grid_id == 1, EINA_FALSE);
		GET_ARG(opt, 1, t_int, &width);
		GET_ARG(opt, 2, t_int, &height);

		grid_matrix_set(nvim->gui.grid, (unsigned)width, (unsigned)height);
	}
	return EINA_TRUE;

fail:
	return EINA_FALSE;
}

Eina_Bool nvim_event_grid_clear(struct nvim *const nvim, const msgpack_object_array *const args)
{
	/* We expect this:
	 *   ["grid_clear", grid]
	 *
	 * Example:
	 *   ["grid_clear", [1]]
	 */

	CHECK_BASE_ARGS_COUNT(args, >=, 1u);
	for (uint32_t i = 1u; i < args->size; i++) {
		const msgpack_object_array *const opt =
			MPACK_ARRAY_EXTRACT(&args->ptr[i], goto fail);
		CHECK_ARGS_COUNT(opt, >=, 1);

		t_int grid_id;
		GET_ARG(opt, 0, t_int, &grid_id);
		/* TODO: for now, we don't implement multi_grid, so we just consider the
		 * grid ID ALWAYS refers to THE grid */
		EINA_SAFETY_ON_FALSE_RETURN_VAL(grid_id == 1, EINA_FALSE);
		grid_clear(nvim->gui.grid);
	}
	return EINA_TRUE;

fail:
	return EINA_FALSE;
}

Eina_Bool nvim_event_grid_cursor_goto(struct nvim *const nvim,
				      const msgpack_object_array *const args)
{
	/* We expect this:
	 *   ["grid_cursor_goto", grid, row, column]
	 *
	 * Example:
	 *   ["grid_cursor_goto", [1, 0, 0]]
	 */
	CHECK_BASE_ARGS_COUNT(args, >=, 1u);
	const msgpack_object_array *const opt =
		MPACK_ARRAY_EXTRACT(&args->ptr[args->size - 1], goto fail);
	CHECK_ARGS_COUNT(opt, >=, 1);

	t_int grid_id, row, col;
	GET_ARG(opt, 0, t_int, &grid_id);
	/* TODO: for now, we don't implement multi_grid, so we just consider the
	 * grid ID ALWAYS refers to THE grid */
	EINA_SAFETY_ON_FALSE_RETURN_VAL(grid_id == 1, EINA_FALSE);
	GET_ARG(opt, 1, t_int, &row);
	GET_ARG(opt, 2, t_int, &col);
	grid_cursor_goto(nvim->gui.grid, (unsigned)col, (unsigned)row);

	return EINA_TRUE;

fail:
	return EINA_FALSE;
}

Eina_Bool nvim_event_grid_line(struct nvim *const nvim, const msgpack_object_array *const args)
{
	/* We expect this:
	 *   ["grid_line", grid, row, col_start, cells]
	 * where cells is an array of:
	 *   [text(, hl_id, repeat)]
	 *
	 * Example:
	 *   ["grid_line", [1, 1, 0, [[" ", 76, 3], ["*"], ["-", 76, 43], ["*"]]], ...]
	 */
	CHECK_BASE_ARGS_COUNT(args, >=, 1u);
	for (uint32_t i = 1u; i < args->size; i++) {
		const msgpack_object_array *const opt =
			MPACK_ARRAY_EXTRACT(&args->ptr[i], goto fail);
		CHECK_ARGS_COUNT(opt, >=, 4);

		t_int grid_id, row, col;
		GET_ARG(opt, 0, t_int, &grid_id);
		/* TODO: for now, we don't implement multi_grid, so we just
		 * consider the grid ID ALWAYS refers to THE grid */
		EINA_SAFETY_ON_FALSE_RETURN_VAL(grid_id == 1, EINA_FALSE);
		GET_ARG(opt, 1, t_int, &row);
		GET_ARG(opt, 2, t_int, &col);

		/* If the style is not mentionned for a cell argument, we must
		 * re-use the last style seen */
		t_int style_id = INT64_C(0);

		const msgpack_object_array *const cells =
			MPACK_ARRAY_EXTRACT(&opt->ptr[3], goto fail);
		for (uint32_t j = 0u; j < cells->size; j++) {
			const msgpack_object_array *const info =
				MPACK_ARRAY_EXTRACT(&cells->ptr[j], goto fail);
			CHECK_ARGS_COUNT(info, >=, 1);

			const msgpack_object_str *const str =
				MPACK_STRING_OBJ_EXTRACT(&info->ptr[0], goto fail);
			t_int repeat = INT64_C(1);

			if (info->size >= 2)
				GET_ARG(info, 1, t_int, &style_id);
			if (info->size >= 3)
				GET_ARG(info, 2, t_int, &repeat);

			grid_line_edit(nvim->gui.grid, (unsigned int)row, (unsigned int)col,
					   str->ptr, (size_t)str->size, (uint32_t)style_id,
					   (size_t)repeat);

			col += repeat;
		}
	}
	return EINA_TRUE;

fail:
	return EINA_FALSE;
}

Eina_Bool nvim_event_grid_scroll(struct nvim *const nvim, const msgpack_object_array *const args)
{
	/* We expect this:
	 *   ["grid_scroll", grid, top, bot, left, right, rows, cols]
	 *
	 * Example:
	 *   ["grid_scroll", [1, 33, 40, 0, 120, 6, 0]]
	 */
	CHECK_BASE_ARGS_COUNT(args, >=, 1u);
	for (uint32_t i = 1u; i < args->size; i++) {
		const msgpack_object_array *const opt =
			MPACK_ARRAY_EXTRACT(&args->ptr[i], goto fail);
		CHECK_ARGS_COUNT(opt, >=, 4);

		t_int grid_id, top, bot, left, right, rows, cols;
		GET_ARG(opt, 0, t_int, &grid_id);
		/* TODO: for now, we don't implement multi_grid, so we just
		 * consider the grid ID ALWAYS refers to THE grid */
		EINA_SAFETY_ON_FALSE_RETURN_VAL(grid_id == 1, EINA_FALSE);
		GET_ARG(opt, 1, t_int, &top);
		GET_ARG(opt, 2, t_int, &bot);
		GET_ARG(opt, 3, t_int, &left);
		GET_ARG(opt, 4, t_int, &right);
		GET_ARG(opt, 5, t_int, &rows);
		GET_ARG(opt, 6, t_int, &cols);
		EINA_SAFETY_ON_FALSE_RETURN_VAL(cols == 0, EINA_FALSE);

		grid_scroll(nvim->gui.grid, (int)top, (int)bot, (int)left, (int)right,
				(int)rows);
	}
	return EINA_TRUE;

fail:
	return EINA_FALSE;
}

Eina_Bool event_linegrid_init(void)
{
	struct hl_attr {
		const char *const name;
		const f_hl_attr_decode func;
	};

	const struct hl_attr attrs[] = {
#define GEN_ATTR(kw, _, __)                                                                        \
	{                                                                                          \
		.name = #kw,                                                                       \
		.func = &_attr_##kw##_cb,                                                          \
	},
		ATTRIBUTES(GEN_ATTR)
#undef GEN_ATTR
	};

	_attributes = eina_hash_stringshared_new(NULL);
	if (EINA_UNLIKELY(!_attributes)) {
		CRI("Failed to create hash table");
		return EINA_FALSE;
	}

	for (size_t i = 0; i < EINA_C_ARRAY_LENGTH(attrs); i++) {
		Eina_Stringshare *const str = eina_stringshare_add(attrs[i].name);
		if (EINA_UNLIKELY(!str)) {
			CRI("Failed to register stringshare");
			goto fail;
		}

		if (EINA_UNLIKELY(!eina_hash_direct_add(_attributes, str, attrs[i].func))) {
			CRI("Failed to add item in hash");
			eina_stringshare_del(str);
			goto fail;
		}
	}
	return EINA_TRUE;

fail:
	eina_hash_free(_attributes);
	return EINA_FALSE;
}

void event_linegrid_shutdown(void)
{
	eina_hash_free(_attributes);
}
