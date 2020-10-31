/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/types.h>
#include <eovim/gui.h>
#include <eovim/nvim.h>
#include <eovim/main.h>
#include <eovim/log.h>
#include <eovim/nvim_api.h>

#include "gui_private.h"

#define FOREACH_GRID(Gui, Variable) \
	Eina_List *list___; \
	struct grid *Variable; \
	EINA_LIST_FOREACH((Gui)->grids, list___, Variable)


static void _tabs_shown_cb(void *data, Evas_Object *obj, const char *emission, const char *source);



static void _caps_lock_alert(struct gui *const gui)
{
	if (!gui->capslock_warning) {
		/* Don't show the capslock alert in theme if deactivated */
		if (gui->theme.react_to_caps_lock)
			elm_layout_signal_emit(gui->layout, "eovim,capslock,on", "eovim");

		/* Tell neovim we activated caps lock */
		nvim_helper_autocmd_do(gui->nvim, "EovimCapsLockOn", NULL, NULL);
		gui->capslock_warning = EINA_TRUE;
	}
}

static void _caps_lock_dismiss(struct gui *const gui)
{
	if (gui->capslock_warning) {
		/* Don't hide the capslock alert in theme if deactivated */
		if (gui->theme.react_to_caps_lock)
			elm_layout_signal_emit(gui->layout, "eovim,capslock,off", "eovim");

		/* Tell neovim we deactivated caps lock */
		nvim_helper_autocmd_do(gui->nvim, "EovimCapsLockOff", NULL, NULL);
		gui->capslock_warning = EINA_FALSE;
	}
}

static void _focus_in_cb(void *const data, Evas_Object *const obj EINA_UNUSED, void *const event EINA_UNUSED)
{
	struct gui *const gui = data;
	cursor_focus_set(gui->cursor, EINA_TRUE);

	/* XXX this is a transcient function call. */
	struct grid *const first_grid = eina_list_data_get(gui->grids);
	//evas_object_focus_set(gui->termview, EINA_TRUE);
	grid_focus_in(first_grid);

	/* When entering back on the Eovim window, the user may have pressed
	 * Caps_Lock outside of Eovim's context. So we have to make sure when
	 * entering Eovim again, that we are on the same page with the input
	 * events. */
	const Evas_Lock *const lock = evas_key_lock_get(gui->evas);
	if (evas_key_lock_is_set(lock, "Caps_Lock"))
		_caps_lock_alert(gui);
	else
		_caps_lock_dismiss(gui);
}

static void _focus_out_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
	struct gui *const gui = data;
	cursor_focus_set(gui->cursor, EINA_FALSE);
}

static void _win_close_cb(void *data, Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
{
	/* When closing the neovim window, send to neovim the :quitall! command
    * so it will be naturally terminated.
    *
    * TODO: see if they are unsaved files ...
    */
	struct nvim *const nvim = data;
	const char cmd[] = ":quitall!";
	nvim_api_command(nvim, cmd, sizeof(cmd) - 1, NULL, NULL);
}

Eina_Bool gui_add(struct gui *gui, struct nvim *nvim)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(gui, EINA_FALSE);

	gui->nvim = nvim;

	gui->tabs = eina_inarray_new(sizeof(unsigned int), 4);
	if (EINA_UNLIKELY(!gui->tabs)) {
		CRI("Failed to create inline array");
		goto fail;
	}

	/* Window setup */
	gui->win = elm_win_util_standard_add("eovim", "Eovim");
	elm_win_autodel_set(gui->win, EINA_TRUE);
	evas_object_smart_callback_add(gui->win, "delete,request", _win_close_cb, nvim);

	gui->evas = evas_object_evas_get(gui->win);

	/* Main Layout setup */
	gui->layout = gui_layout_item_add(gui->win, "eovim/main");
	if (EINA_UNLIKELY(!gui->layout)) {
		CRI("Failed to get layout item");
		goto fail;
	}
	gui->edje = elm_layout_edje_get(gui->layout);
	elm_layout_signal_callback_add(gui->layout, "eovim,tabs,shown", "eovim", _tabs_shown_cb,
				       nvim);
	elm_win_resize_object_add(gui->win, gui->layout);
	evas_object_smart_callback_add(gui->win, "focus,in", _focus_in_cb, gui);
	evas_object_smart_callback_add(gui->win, "focus,out", _focus_out_cb, gui);
	evas_object_smart_callback_add(gui->win, "focus,out", _focus_out_cb, gui);

	// XXX Leaks on failure.
	gui->style.kind_styles = eina_hash_stringshared_new(EINA_FREE_CB(&eina_stringshare_del));
	if (EINA_UNLIKELY(!gui->style.kind_styles)) {
		CRI("Failed to create hash map");
		goto fail;
	}
	gui->style.cmdline_styles = eina_hash_stringshared_new(EINA_FREE_CB(&eina_stringshare_del));
	if (EINA_UNLIKELY(!gui->style.cmdline_styles)) {
		CRI("Failed to create hash map");
		goto fail;
	}
	gui->style.hl_groups = eina_hash_stringshared_new(NULL);
	if (EINA_UNLIKELY(!gui->style.hl_groups)) {
		CRI("Failed to create hash map");
		goto fail;
	}

	/* A 1x1 cell matrix to retrieve the font size. Always invisible */
	gui->sizing_textgrid = evas_object_textgrid_add(gui->evas);
	evas_object_size_hint_weight_set(gui->sizing_textgrid, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(gui->sizing_textgrid, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_textgrid_size_set(gui->sizing_textgrid, 1, 1);


	/* ========================================================================
	 * Termview GUI objects
	 * ===================================================================== */

	gui->cursor = cursor_add(gui);

	gui->cmdline = cmdline_add(gui);

	struct grid *const grid = grid_add(gui);
	gui->grids = eina_list_append(gui->grids, grid);

	gui->wildmenu = gui_wildmenu_add(gui);
	if (EINA_UNLIKELY(!gui->wildmenu))
		goto fail;

	gui->completion = gui_completion_add(gui);
	if (EINA_UNLIKELY(!gui->completion))
		goto fail;

	/* ========================================================================
	 * Finalize GUI
	 * ===================================================================== */

	gui_font_set(gui, "Courier", 14);

	gui_cmdline_hide(gui);
	evas_object_show(gui->layout);
	evas_object_show(gui->win);
	return EINA_TRUE;

fail:
	if (gui->tabs)
		eina_inarray_free(gui->tabs);
	evas_object_del(gui->win);
	return EINA_FALSE;
}

void gui_del(struct gui *gui)
{
	EINA_SAFETY_ON_NULL_RETURN(gui);
	cursor_del(gui->cursor);
	cmdline_del(gui->cmdline);
	gui_wildmenu_del(gui->wildmenu);
	gui_completion_del(gui->completion);
	struct grid *grid;
	EINA_LIST_FREE(gui->grids, grid)
		grid_del(grid);

	eina_hash_free(gui->style.hl_groups);
	eina_hash_free(gui->style.cmdline_styles);
	eina_hash_free(gui->style.kind_styles);
	eina_inarray_free(gui->tabs);
	evas_object_del(gui->sizing_textgrid);
	evas_object_del(gui->win);
}

static void _die_cb(void *data, Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
{
	struct gui *const gui = data;
	gui_del(gui);
}

void gui_die(struct gui *gui, const char *fmt, ...)
{
	/* Hide the grid */
	Evas_Object *const view = elm_layout_content_unset(gui->layout, "eovim.main.view");
	evas_object_hide(view);

	/* Collect the message */
	char text[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);

	/* Create a button to allow the user to quit */
	Evas_Object *const btn = elm_button_add(gui->layout);
	evas_object_smart_callback_add(btn, "clicked", _die_cb, gui);
	elm_object_text_set(btn, "Quit");

	/* Send a popup to notify the error and terminate */
	Evas_Object *const pop = elm_popup_add(gui->layout);
	elm_object_text_set(pop, text);
	evas_object_smart_callback_add(pop, "dismissed", _die_cb, gui);
	elm_object_part_content_set(pop, "button1", btn);
	evas_object_show(pop);
}

void gui_font_set(struct gui *const gui, const char *const font_name, const unsigned int font_size)
{
	EINA_SAFETY_ON_NULL_RETURN(font_name);
	EINA_SAFETY_ON_FALSE_RETURN(font_size < INT_MAX);

	const Eina_Bool name_changed = eina_stringshare_replace(&gui->style.font_name, font_name);

	DBG("Using font '%s' with size '%u'", font_name, font_size);
	if (name_changed || (font_size != gui->style.font_size)) {
		gui->style.font_size = font_size;
		int cell_w, cell_h;
		evas_object_textgrid_font_set(gui->sizing_textgrid, gui->style.font_name,
					      (int)gui->style.font_size);
		evas_object_textgrid_cell_size_get(gui->sizing_textgrid, &cell_w, &cell_h);
		FOREACH_GRID(gui, grid)
			grid_cell_size_set(grid, (unsigned)cell_w, (unsigned)cell_h);
		gui_style_update_defer(gui);
		gui_resize_defer(gui);
	}
}

void gui_default_colors_set(struct gui *const gui, const union color fg, const union color bg,
			    const union color sp)
{
	const Eina_Bool changed = (gui->style.default_fg.value != fg.value) ||
				  (gui->style.default_bg.value != bg.value) ||
				  (gui->style.default_sp.value != sp.value);

	if (changed) {
		gui->style.default_fg = fg;
		gui->style.default_bg = bg;
		gui->style.default_sp = sp;
		gui_style_update_defer(gui);
		color_class_set("eovim.background", bg);
	}
}

void gui_busy_set(struct gui *gui, Eina_Bool busy)
{
	if (busy) {
		/* Trigger the busy signal only if the gui was not busy */
		if (++gui->busy_count == 1)
			elm_layout_signal_emit(gui->layout, "eovim,busy,on", "eovim");
	} else {
		/* Stop the busy signal only if the gui has one busy reference */
		if (--gui->busy_count == 0)
			elm_layout_signal_emit(gui->layout, "eovim,busy,off", "eovim");
		if (EINA_UNLIKELY(gui->busy_count < 0)) {
			ERR("busy count underflowed");
			gui->busy_count = 0;
		}
	}
}

void gui_active_popupmenu_hide(struct gui *const gui)
{
	popupmenu_hide(gui->active_popup);
}

void gui_active_popupmenu_select_nth(struct gui *const gui, const ssize_t index)
{
	popupmenu_select_nth(gui->active_popup, index);
}

void gui_bell_ring(struct gui *gui)
{
	/* Ring the bell, but only if it was not muted */
	if (gui->theme.bell_enabled)
		elm_layout_signal_emit(gui->layout, "eovim,bell,ring", "eovim");
}

static void _maximized_cb(void *const data, Evas_Object *const obj EINA_UNUSED,
			  void *const info EINA_UNUSED)
{
	/* We actually want a fullscreen windows. This callback is called when during
	 * the maximization of the window. We unregister this callback, so it is not called
	 * ever again, and actually trigger the fullscreen */
	struct gui *const gui = data;
	evas_object_smart_callback_del_full(gui->win, "resize", &_maximized_cb, data);
	elm_win_fullscreen_set(gui->win, EINA_TRUE);
}

void gui_ready_set(struct gui *const gui)
{
	/* XXX This is transitional */
	const struct grid *const grid = eina_list_data_get(gui->grids);
	Evas_Object *const obj = grid_textblock_get(grid);
	elm_layout_content_set(gui->layout, "eovim.main.view", obj);
	evas_object_show(obj);

	const struct options *const opts = gui->nvim->opts;

	/* For maximize and fullscreen, we just update the window's dimensions.
	 * When the resizing is finished, we will notify neovim */
	if (opts->maximized)
		elm_win_maximized_set(gui->win, EINA_TRUE);
	else if (opts->fullscreen) {
		/* This is not a typo, we really **maximize** the window. I'm not sure why, but
		 * if we fullscreen at this point, we never trigger resize functions. But if
		 * we first maximize and then fullscreen, we are good to go! */
		elm_win_maximized_set(gui->win, EINA_TRUE);
		evas_object_smart_callback_add(gui->win, "resize", &_maximized_cb, gui);
	}
}

void gui_title_set(struct gui *gui, const char *title)
{
	EINA_SAFETY_ON_NULL_RETURN(title);

	/* Set the title to the window, or just "Eovim" if it happens to be empty */
	elm_win_title_set(gui->win, (title[0]) ? title : "Eovim");
}

void gui_mode_update(struct gui *gui, const struct mode *mode)
{
	FOREACH_GRID(gui, grid)
		grid_cursor_mode_set(grid, mode);
}

/*============================================================================*
 *                                  TAB LINE                                  *
 *============================================================================*/

static void _tabs_shown_cb(void *data, Evas_Object *obj EINA_UNUSED,
			   const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
	/* After the tabs have been shown, we need to re-evaluate the layout,
    * so the size taken by the tabline will impact the main view */
	struct nvim *const nvim = data;
	struct gui *const gui = &nvim->gui;

	elm_layout_sizing_eval(gui->layout);
}

static void _tab_close_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED,
			  const char *src EINA_UNUSED)
{
	struct gui *const gui = data;
	struct nvim *const nvim = gui->nvim;
	const char cmd[] = ":tabclose";
	nvim_api_command(nvim, cmd, sizeof(cmd) - 1, NULL, NULL);
}

static void _tab_activate_cb(void *data, Evas_Object *obj, const char *sig EINA_UNUSED,
			     const char *src EINA_UNUSED)
{
	struct gui *const gui = data;

	/* The tab ID is stored as a raw value in a pointer field */
	const unsigned int id = (unsigned int)(uintptr_t)evas_object_data_get(obj, "tab_id");

	/* If the tab is already activated, do not activate it more */
	if (id == gui->active_tab)
		return;

	/* Find the tab index. This is more complex than it should be, but since
    * neovim only gives us its internal IDs, we have to convert them into the
    * tab IDs as they are ordered on screen (e.g from left to right).
    *
    * We go through the list of tabs, to find the current index we are on and
    * the index of the tab to be activated.
    */
	unsigned int *it;
	unsigned int active_index = UINT_MAX;
	unsigned int tab_index = UINT_MAX;
	EINA_INARRAY_FOREACH (gui->tabs, it) {
		/* When found a candidate, evaluate the index by some pointer
         * arithmetic.  This avoids to keep around a counter. At this
         * point, gui->active_tabs != id. */
		if (*it == id)
			tab_index = (unsigned)(it - (unsigned int *)gui->tabs->members);
		else if (*it == gui->active_tab)
			active_index = (unsigned)(it - (unsigned int *)gui->tabs->members);
	}
	if (EINA_UNLIKELY((tab_index == UINT_MAX) || (active_index == UINT_MAX))) {
		CRI("Something went wrong while finding the tab index: %u, %u", tab_index,
		    active_index);
		return;
	}

	/* Compose the command to select the tab. See :help tabnext. */
	const int diff = (int)active_index - (int)tab_index;
	char cmd[32];
	const int bytes = snprintf(cmd, sizeof(cmd), ":%c%utabnext", (diff < 0) ? '+' : '-',
				   (diff < 0) ? -diff : diff);
	nvim_api_command(gui->nvim, cmd, (size_t)bytes, NULL, NULL);
}

void gui_tabs_reset(struct gui *gui)
{
	gui->active_tab = 0;
	eina_inarray_flush(gui->tabs);
	edje_object_part_box_remove_all(gui->edje, "eovim.tabline", EINA_TRUE);
}

void gui_tabs_show(struct gui *gui)
{
	elm_layout_signal_emit(gui->layout, "eovim,tabs,show", "eovim");
}

void gui_tabs_hide(struct gui *gui)
{
	elm_layout_signal_emit(gui->layout, "eovim,tabs,hide", "eovim");
}

void gui_tabs_add(struct gui *gui, const char *name, unsigned int id, Eina_Bool active)
{
	/* Register the current tab */
	eina_inarray_push(gui->tabs, &id);

	Evas_Object *const edje = edje_object_add(gui->evas);
	evas_object_data_set(edje, "tab_id", (void *)(uintptr_t)id);
	edje_object_file_set(edje, main_edje_file_get(), "eovim/tab");
	edje_object_signal_callback_add(edje, "tab,close", "eovim", _tab_close_cb, gui);
	edje_object_signal_callback_add(edje, "tab,activate", "eovim", _tab_activate_cb, gui);
	evas_object_size_hint_align_set(edje, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(edje, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	edje_object_part_text_set(edje, "eovim.tab.title", name);
	evas_object_show(edje);

	if (active) {
		edje_object_signal_emit(edje, "eovim,tab,activate", "eovim");
		gui->active_tab = id;
	}
	edje_object_part_box_append(gui->edje, "eovim.tabline", edje);
}


Eina_Bool gui_caps_lock_warning_get(const struct gui *gui)
{
	return gui->capslock_warning;
}

Evas_Object *gui_layout_item_add(Evas_Object *const parent, const char *const group)
{
	Evas_Object *const o = elm_layout_add(parent);
	evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
	const char *const file = main_edje_file_get();

	const Eina_Bool ok = elm_layout_file_set(o, file, group);
	if (EINA_UNLIKELY(!ok)) {
		CRI("Failed to set layout from file '%s' for group '%s'", file, group);
		evas_object_del(o);
		return NULL;
	}
	return o;
}

void gui_style_update_defer(struct gui *const gui)
{
	gui->pending_style_update = EINA_TRUE;
}

void gui_resize_defer(struct gui *const gui)
{
	gui->need_nvim_resize = EINA_TRUE;
}

void gui_linespace_set(struct gui *const gui, const unsigned int linespace)
{
	gui->style.line_gap = linespace;
	gui_resize_defer(gui);
	gui_style_update_defer(gui);
}

void gui_redraw_end(struct gui *const gui)
{
	FOREACH_GRID(gui, grid)
		grid_redraw_end(grid);
}

void gui_flush(struct gui *const gui)
{
	FOREACH_GRID(gui, grid)
		grid_flush(grid);
}

struct grid *gui_grid_get(const struct gui *const gui, const size_t index)
{
	// TODO
	(void) index;
	return eina_list_data_get(gui->grids);
}
