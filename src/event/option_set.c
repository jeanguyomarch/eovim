/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

/** Dictionary that associates a callback handler to each option name */
static Eina_Hash *_options = NULL;

/**
 * Type of functions that shall handle options modifiers.
 *
 * @param[in] nvim Global nvim instance context
 * @param[in] value Generic msgpack value provided to the option
 * @return EINA_TRUE on success, EINA_FALSE otherwise
 */
typedef Eina_Bool (*f_opt_set)(struct nvim *nvim, const msgpack_object *value);

#define OPTIONS(X)                                                                                 \
	X(arabicshape, _arabicshape_set)                                                           \
	X(ambiwidth, _ambiwidth_set)                                                               \
	X(emoji, _emoji_set)                                                                       \
	X(guifont, _guifont_set)                                                                   \
	X(guifontset, _guifontset_set)                                                             \
	X(guifontwide, _guifontwide_set)                                                           \
	X(linespace, _linespace_set)                                                               \
	X(showtabline, _showtabline_set)                                                           \
	X(termguicolors, _termguicolors_set)                                                       \
	X(ext_popupmenu, _ext_popupmenu_set)                                                       \
	X(ext_tabline, _ext_tabline_set)                                                           \
	X(ext_cmdline, _ext_cmdline_set)                                                           \
	X(ext_wildmenu, _ext_wildmenu_set)                                                         \
	X(ext_linegrid, _ext_linegrid_set)                                                         \
	X(ext_hlstate, _ext_hlstate_set)                                                           \
	/**/

/*****************************************************************************/

static Eina_Bool _arabicshape_set(struct nvim *nvim EINA_UNUSED,
				  const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _ambiwidth_set(struct nvim *nvim EINA_UNUSED,
				const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _emoji_set(struct nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _guifont_set(struct nvim *const nvim, const msgpack_object *const value)
{
	/* See the wiki for details on what is accepted and what is not. The evas_textblock
	 * will parse a subset of the fontconfig format:
	 *   https://www.freedesktop.org/software/fontconfig/fontconfig-user.html
	 *
	 * We expect something like:
	 *   DejaVu\ Sans\ Mono-xxx:style=Book
	 *   ^~~~~~~~~~~~~~~~~^^^,^^~~~~~~~~~~~ fontconfig styles
	 *     font_name       | |
	 *     a single dash --' '-- font size
	 *
	 * We will pass to the textblock as the font name: font_name + fontconfig styles
	 * The font size is silently extracted.
	 */
	struct gui *const gui = &nvim->gui;
	const msgpack_object_str *const val = MPACK_STRING_OBJ_EXTRACT(value, goto fail);
	if (val->size == 0) {
		return EINA_TRUE;
	}

	/* Havind a NUL-terminating string would just be so much better */
	char *const str = strndup(val->ptr, val->size);
	if (EINA_UNLIKELY(!str)) {
		CRI("Alloc failed");
		goto fail;
	}

	/* We are now trying to find a pattern such as: FontName-FontSize:Extra */
	char *const sep = strchr(str, '-');
	if (EINA_UNLIKELY(!sep)) {
		/* TODO: real message */
		ERR("invalid guifont='%s'. Expected: Fontname-Fontsize[:extra]'", str);
		goto clean;
	}

	/* Parse the fontsize. We forbid a subset of fantasist sizes to both
	 * catch parsing failures (returned value would be ULONG_MAX) and to
	 * prevent invalid use of other APIs */
	char *end = sep;
	const unsigned long fontsize = strtoul(sep + 1, &end, 10);
	if (fontsize > UINT_MAX) {
		ERR("Font size in '%s' (%lu) is too big", str, fontsize);
		goto clean;
	} else if (*end == '\0') {
		/* No extra parameter, isolate the font name */
		*sep = '\0';
	} else if (*end == ':') {
		/* Extra parameters: append them to the fontname */
		const char *const endstr = str + val->size + 1;
		assert(endstr > end);
		memcpy(sep, end, (size_t)(endstr - end));
	} else {
		ERR("Failed to parse the font size in '%s'", str);
		goto clean;
	}

	gui_font_set(gui, str, (unsigned int)fontsize);
	free(str);
	return EINA_TRUE;

clean:
	free(str);
fail:
	return EINA_FALSE;
}

static Eina_Bool _guifontset_set(struct nvim *nvim EINA_UNUSED,
				 const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _guifontwide_set(struct nvim *nvim EINA_UNUSED,
				  const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _linespace_set(struct nvim *const nvim, const msgpack_object *const value)
{
	if (EINA_UNLIKELY(value->type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
		ERR("A positive integer is expected for 'linespace'");
		return EINA_FALSE;
	}

	const uint64_t val = value->via.u64;
	gui_linespace_set(&nvim->gui, (unsigned int)val);
	return EINA_TRUE;
}

static Eina_Bool _showtabline_set(struct nvim *nvim EINA_UNUSED,
				  const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _termguicolors_set(struct nvim *nvim EINA_UNUSED,
				    const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _ext_popupmenu_set(struct nvim *nvim EINA_UNUSED,
				    const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _ext_tabline_set(struct nvim *nvim EINA_UNUSED,
				  const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _ext_cmdline_set(struct nvim *nvim EINA_UNUSED,
				  const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _ext_wildmenu_set(struct nvim *nvim EINA_UNUSED,
				   const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _ext_linegrid_set(struct nvim *nvim EINA_UNUSED,
				   const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

static Eina_Bool _ext_hlstate_set(struct nvim *nvim EINA_UNUSED,
				  const msgpack_object *value EINA_UNUSED)
{
	/* XXX unimplemented for now */
	return EINA_TRUE;
}

/*****************************************************************************/

Eina_Bool nvim_event_option_set(struct nvim *nvim, const msgpack_object_array *args)
{
	/* We expect the arguments that come after the very first parameter (name of
    * the event, so 'option_set' in this case) to be a set of pairs:
    *
    * [
    *   'option_set',
    *   [
    *     [keyword-name (type: string), value (type: any)],
    *     ...
    *   ]
    * ]
    */
	CHECK_BASE_ARGS_COUNT(args, >=, 1u);
	for (uint32_t i = 1u; i < args->size; i++) {
		/* Get a pair (option name + option value) */
		const msgpack_object_array *const opt =
			MPACK_ARRAY_EXTRACT(&args->ptr[i], goto fail);
		CHECK_ARGS_COUNT(opt, ==, 2u);

		/* Get the name of the option as a stringshare */
		Eina_Stringshare *const key = MPACK_STRING_EXTRACT(&opt->ptr[0], goto fail);

		/* Find the handler for the option */
		const f_opt_set func = eina_hash_find(_options, key);
		if (EINA_UNLIKELY(!func)) {
			DBG("Unknown 'option_set' keyword argument '%s'", key);
			eina_stringshare_del(key);
			continue; /* Try the next keyword-argument */
		}

		/* Execute the handler of the option */
		const Eina_Bool ok = func(nvim, &opt->ptr[1]);
		if (EINA_UNLIKELY(!ok)) {
			ERR("Failed to process option with keyword '%s'", key);
			eina_stringshare_del(key);
			return EINA_FALSE;
		}
		eina_stringshare_del(key);
	}

	return EINA_TRUE;
fail:
	return EINA_FALSE;
}

Eina_Bool option_set_init(void)
{
	_options = eina_hash_stringshared_new(NULL);
	if (EINA_UNLIKELY(!_options)) {
		CRI("Failed to create hash table");
		return EINA_FALSE;
	}

	struct opt {
		const char *const name;
		const f_opt_set func;
	};

	const struct opt opts[] = {
#define GEN_OPT(Kw, Func)                                                                          \
	{                                                                                          \
		.name = #Kw,                                                                       \
		.func = &Func,                                                                     \
	},
		OPTIONS(GEN_OPT)
#undef GEN_OPT
	};

	for (size_t i = 0u; i < EINA_C_ARRAY_LENGTH(opts); i++) {
		Eina_Stringshare *const str = eina_stringshare_add(opts[i].name);
		if (EINA_UNLIKELY(!str)) {
			CRI("Failed to create stringshare");
			goto fail;
		}
		if (EINA_UNLIKELY(!eina_hash_direct_add(_options, str, opts[i].func))) {
			CRI("Failed to add item in hash");
			eina_stringshare_del(str);
			goto fail;
		}
	}

	return EINA_TRUE;
fail:
	eina_hash_free(_options);
	return EINA_FALSE;
}

void option_set_shutdown(void)
{
	eina_hash_free(_options);
}
