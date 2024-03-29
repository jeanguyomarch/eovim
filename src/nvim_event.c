/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/types.h>
#include <eovim/nvim.h>
#include <eovim/nvim_event.h>
#include <eovim/msgpack_helper.h>
#include <eovim/gui.h>
#include "event/event.h"

struct method {
	Eina_Stringshare *name; /**< Name of the method */
	Eina_Hash *callbacks; /**< Table of callbacks associated with the method */
	Eina_Bool (*batch_end_func)(struct nvim *); /**< Function called after a batch ends */
};

typedef enum {
	E_METHOD_REDRAW, /**< The "redraw" method */
	E_METHOD_EOVIM, /**< The "eovim" method */

	__E_METHOD_LAST
} e_method;

/* Array of callbacks for each method. It is NOT a hash table as we will support
 * (for now) very little number of methods (around two). It is much faster to
 * search sequentially among arrays of few cells than a map of two. */
static struct method _methods[__E_METHOD_LAST];

static Eina_Bool nvim_event_flush(struct nvim *const nvim,
				  const msgpack_object_array *const args EINA_UNUSED)
{
	termview_flush(nvim->gui.termview);
	return EINA_TRUE;
}

static Eina_Bool nvim_event_update_menu(struct nvim *nvim EINA_UNUSED,
					const msgpack_object_array *args EINA_UNUSED)
{
	DBG("Unimplemented");
	return EINA_TRUE;
}

static Eina_Bool nvim_event_busy_start(struct nvim *nvim,
				       const msgpack_object_array *args EINA_UNUSED)
{
	gui_busy_set(&nvim->gui, EINA_TRUE);
	return EINA_TRUE;
}

static Eina_Bool nvim_event_busy_stop(struct nvim *nvim,
				      const msgpack_object_array *args EINA_UNUSED)
{
	gui_busy_set(&nvim->gui, EINA_FALSE);
	return EINA_TRUE;
}

static Eina_Bool nvim_event_mouse_on(struct nvim *nvim,
				     const msgpack_object_array *args EINA_UNUSED)
{
	nvim_mouse_enabled_set(nvim, EINA_TRUE);
	return EINA_TRUE;
}

static Eina_Bool nvim_event_mouse_off(struct nvim *nvim,
				      const msgpack_object_array *args EINA_UNUSED)
{
	nvim_mouse_enabled_set(nvim, EINA_FALSE);
	return EINA_TRUE;
}

static Eina_Bool nvim_event_bell(struct nvim *nvim, const msgpack_object_array *args EINA_UNUSED)
{
	gui_bell_ring(&nvim->gui);
	return EINA_TRUE;
}

static Eina_Bool nvim_event_visual_bell(struct nvim *nvim EINA_UNUSED,
					const msgpack_object_array *args EINA_UNUSED)
{
	DBG("Unimplemented");
	return EINA_TRUE;
}

static Eina_Bool nvim_event_suspend(struct nvim *nvim EINA_UNUSED,
				    const msgpack_object_array *args EINA_UNUSED)
{
	/* Nothing to do */
	return EINA_TRUE;
}

static Eina_Bool nvim_event_set_title(struct nvim *nvim, const msgpack_object_array *args)
{
	CHECK_BASE_ARGS_COUNT(args, ==, 1);
	ARRAY_OF_ARGS_EXTRACT(args, params);
	CHECK_ARGS_COUNT(params, ==, 1);

	Eina_Stringshare *const title = MPACK_STRING_EXTRACT(&params->ptr[0], return EINA_FALSE);
	gui_title_set(&nvim->gui, title);
	eina_stringshare_del(title);

	return EINA_TRUE;
}

static Eina_Bool nvim_event_set_icon(struct nvim *nvim EINA_UNUSED,
				     const msgpack_object_array *args EINA_UNUSED)
{
	/* Do nothing. Seems it can be safely ignored. */
	return EINA_TRUE;
}

const struct method *nvim_event_method_find(Eina_Stringshare *const method_name)
{
	/* Go sequentially through the list of methods we know about, so we can
   * find out the callbacks table for that method, to try to find what matches
   * 'command'. */
	for (size_t i = 0u; i < EINA_C_ARRAY_LENGTH(_methods); i++) {
		const struct method *const method = &(_methods[i]);
		if (method->name == method_name) /* Fond the method */
		{
			return method;
		}
	}

	WRN("Unknown method '%s'", method_name);
	return NULL;
}

Eina_Bool nvim_event_method_dispatch(struct nvim *const nvim, const struct method *const method,
				     Eina_Stringshare *const command,
				     const msgpack_object_array *const args)
{
	/* Grab the callback for the command. If we could find none,
   * that's an error. Otherwise we call it. In both cases, the
   * execution of the function will be terminated. */
	const f_event_cb cb = eina_hash_find(method->callbacks, command);
	if (EINA_UNLIKELY(!cb)) {
		WRN("Failed to get callback for command '%s' of method '%s'", command,
		    method->name);
		return EINA_FALSE;
	}
	return cb(nvim, args);
}

Eina_Bool nvim_event_method_batch_end(struct nvim *const nvim, const struct method *const method)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(method, EINA_FALSE);
	return (method->batch_end_func != NULL) ? method->batch_end_func(nvim) : EINA_TRUE;
}

typedef struct {
	const char *const name; /**< Name of the event */
	const unsigned int size; /**< Size of @p name */
	const f_event_cb func; /**< Callback function */
} s_method_ctor;

#define CB_CTOR(Name, Func)                                                                        \
	{                                                                                          \
		.name = (Name), .size = sizeof(Name) - 1, .func = (Func)                           \
	}

static Eina_Bool _command_add(Eina_Hash *table, const char *name, unsigned int name_len,
			      f_event_cb func)
{
	/* Create the key to be inserted in the table */
	Eina_Stringshare *const cmd = eina_stringshare_add_length(name, name_len);
	if (EINA_UNLIKELY(!cmd)) {
		CRI("Failed to create stringshare from '%s', size %u", name, name_len);
		goto fail;
	}

	/* Add the value 'func' to the key 'cmd' in the table */
	if (EINA_UNLIKELY(!eina_hash_add(table, cmd, func))) {
		CRI("Failed to register callback %p for command '%s'", func, cmd);
		goto add_fail;
	}

	return EINA_TRUE;
add_fail:
	eina_stringshare_del(cmd);
fail:
	return EINA_FALSE;
}

static Eina_Hash *_method_callbacks_table_build(const s_method_ctor *ctors,
						unsigned int ctors_count)
{
	/* Generate a hash table that will contain the callbacks to be called for
    * each event sent by neovim. */
	Eina_Hash *const cb = eina_hash_stringshared_new(NULL);
	if (EINA_UNLIKELY(!cb)) {
		CRI("Failed to create hash table to hold callbacks");
		goto fail;
	}

	/* Add the events in the callbacks table. */
	for (unsigned int i = 0; i < ctors_count; i++) {
		const Eina_Bool ok = _command_add(cb, ctors[i].name, ctors[i].size, ctors[i].func);
		if (EINA_UNLIKELY(!ok)) {
			CRI("Failed to register command '%s'", ctors[i].name);
			goto hash_free;
		}
	}

	return cb;
hash_free:
	eina_hash_free(cb);
fail:
	return NULL;
}

static void _method_free(struct method *method)
{
	/* This function is always used on statically-allocated containers.
    * As such, free() should not be called on 'method' */
	if (method->name)
		eina_stringshare_del(method->name);
	if (method->callbacks)
		eina_hash_free(method->callbacks);
}

static Eina_Bool _nvim_event_redraw_end(struct nvim *const nvim)
{
	termview_redraw_end(nvim->gui.termview);
	return EINA_TRUE;
}

static Eina_Bool _method_redraw_init(e_method method_id)
{
	struct method *const method = &(_methods[method_id]);
	const s_method_ctor ctors[] = {
		CB_CTOR("mode_info_set", nvim_event_mode_info_set),
		CB_CTOR("update_menu", nvim_event_update_menu),
		CB_CTOR("busy_start", nvim_event_busy_start),
		CB_CTOR("busy_stop", nvim_event_busy_stop),
		CB_CTOR("mouse_on", nvim_event_mouse_on),
		CB_CTOR("mouse_off", nvim_event_mouse_off),
		CB_CTOR("mode_change", nvim_event_mode_change),
		CB_CTOR("bell", nvim_event_bell),
		CB_CTOR("visual_bell", nvim_event_visual_bell),
		CB_CTOR("suspend", nvim_event_suspend),
		CB_CTOR("set_title", nvim_event_set_title),
		CB_CTOR("set_icon", nvim_event_set_icon),
		CB_CTOR("popupmenu_show", nvim_event_popupmenu_show),
		CB_CTOR("popupmenu_hide", nvim_event_popupmenu_hide),
		CB_CTOR("popupmenu_select", nvim_event_popupmenu_select),
		CB_CTOR("tabline_update", nvim_event_tabline_update),
		CB_CTOR("cmdline_show", nvim_event_cmdline_show),
		CB_CTOR("cmdline_pos", nvim_event_cmdline_pos),
		CB_CTOR("cmdline_special_char", nvim_event_cmdline_special_char),
		CB_CTOR("cmdline_hide", nvim_event_cmdline_hide),
		CB_CTOR("cmdline_block_show", nvim_event_cmdline_block_show),
		CB_CTOR("cmdline_block_append", nvim_event_cmdline_block_append),
		CB_CTOR("cmdline_block_hide", nvim_event_cmdline_block_hide),
		CB_CTOR("option_set", nvim_event_option_set),
		CB_CTOR("flush", nvim_event_flush),
		CB_CTOR("default_colors_set", nvim_event_default_colors_set),
		CB_CTOR("hl_attr_define", nvim_event_hl_attr_define),
		CB_CTOR("hl_group_set", nvim_event_hl_group_set),
		CB_CTOR("grid_resize", nvim_event_grid_resize),
		CB_CTOR("grid_clear", nvim_event_grid_clear),
		CB_CTOR("grid_cursor_goto", nvim_event_grid_cursor_goto),
		CB_CTOR("grid_line", nvim_event_grid_line),
		CB_CTOR("grid_scroll", nvim_event_grid_scroll),
	};

	/* Register the name of the method as a stringshare */
	const char name[] = "redraw";
	method->name = eina_stringshare_add_length(name, sizeof(name) - 1);
	if (EINA_UNLIKELY(!method->name)) {
		CRI("Failed to create stringshare from string '%s'", name);
		goto fail;
	}

	method->batch_end_func = &_nvim_event_redraw_end;

	/* Build the callbacks table of the method */
	method->callbacks = _method_callbacks_table_build(ctors, EINA_C_ARRAY_LENGTH(ctors));
	if (EINA_UNLIKELY(!method->callbacks)) {
		CRI("Failed to create table of callbacks");
		goto free_name;
	}

	return EINA_TRUE;
free_name:
	eina_stringshare_del(method->name);
fail:
	return EINA_FALSE;
}

static Eina_Bool _method_eovim_init(e_method method_id)
{
	struct method *const method = &(_methods[method_id]);

	const s_method_ctor ctors[] = {
		CB_CTOR("reload", nvim_event_eovim_reload),
	};

	/* Register the name of the method as a stringshare */
	const char name[] = "eovim";
	method->name = eina_stringshare_add_length(name, sizeof(name) - 1);
	if (EINA_UNLIKELY(!method->name)) {
		CRI("Failed to create stringshare from string '%s'", name);
		goto fail;
	}

	/* Create an empty the callbacks table of the method */
	method->callbacks = _method_callbacks_table_build(ctors, EINA_C_ARRAY_LENGTH(ctors));
	if (EINA_UNLIKELY(!method->callbacks)) {
		CRI("Failed to create table of callbacks");
		goto free_name;
	}

	return EINA_TRUE;
free_name:
	eina_stringshare_del(method->name);
fail:
	return EINA_FALSE;
}

Eina_Bool nvim_event_init(void)
{
	/* Initialize the "redraw" method */
	if (EINA_UNLIKELY(!_method_redraw_init(E_METHOD_REDRAW))) {
		CRI("Failed to setup the redraw method");
		goto fail;
	}

	/* Initialize the "eovim" method */
	if (EINA_UNLIKELY(!_method_eovim_init(E_METHOD_EOVIM))) {
		CRI("Failed to setup the eovim method");
		goto del_methods;
	}

	/* Initialize the internals of 'mode_info_set' */
	if (EINA_UNLIKELY(!mode_init())) {
		CRI("Failed to initialize mode internals");
		goto del_methods;
	}

	/* Initialize the internals of option_set */
	if (EINA_UNLIKELY(!option_set_init())) {
		CRI("Failed to initialize 'option_set'");
		goto mode_deinit;
	}

	/* Initialize the internals of option_set */
	if (EINA_UNLIKELY(!event_linegrid_init())) {
		CRI("Failed to initialize 'linegrid'");
		goto option_deinit;
	}

	return EINA_TRUE;

option_deinit:
	option_set_shutdown();
mode_deinit:
	mode_shutdown();
del_methods:
	for (size_t j = 0u; j < EINA_C_ARRAY_LENGTH(_methods); j++)
		_method_free(&(_methods[j]));
fail:
	return EINA_FALSE;
}

void nvim_event_shutdown(void)
{
	event_linegrid_shutdown();
	option_set_shutdown();
	mode_shutdown();
	for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_methods); i++)
		_method_free(&(_methods[i]));
}
