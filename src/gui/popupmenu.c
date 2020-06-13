/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/nvim_api.h>
#include <eovim/gui.h>
#include <eovim/log.h>

#include "gui_private.h"

static void popupmenu_select_func(void *const data, Evas_Object *const obj EINA_UNUSED,
				  void *const event)
{
	struct popupmenu *const menu = data;
	const Elm_Object_Item *const item = event;

	/* If an item was selected, but at the initiative of neovim, we will
	 * just discard the event, to notify we have processed it, but will
	 * stop right here, right now, because this function is made to handle
	 * user selection */
	if (menu->nvim_sel_event) {
		menu->nvim_sel_event = EINA_FALSE;
		return;
	}

	/* Use a string buffer that will hold the input to be passed to neovim */
	Eina_Strbuf *const input = menu->sbuf;
	eina_strbuf_reset(input);

	/* For some strange reason, genlist indexing starts at 1! WTF. */
	const int item_idx = elm_genlist_item_index_get(item) - 1;
	/* No item selected? Take the first one */
	const int sel_idx = (menu->sel_index >= 0) ? (int)menu->sel_index : 0;

	/* No item selected? Initiate the completion. */
	if (!menu->sel_item)
		eina_strbuf_append_length(input, "<C-n>", 5);

	/* To make neovim select the wildmenu item, we will write N times
	 * <C-n> or <C-p> from the current index to the target one. When done,
	 * we will insert <CR> to make the selection apply */
	if (sel_idx < item_idx) {
		for (int i = sel_idx; i < item_idx; i++)
			eina_strbuf_append_length(input, "<C-n>", 5);
	} else {
		for (int i = item_idx; i < sel_idx; i++)
			eina_strbuf_append_length(input, "<C-p>", 5);
	}

	/* Send a signal to end the menu selection */
	eina_strbuf_append_length(input, "<CR>", 4);
	menu->sel_done = EINA_TRUE;

	/* Pass all these data to neovim and cleanup behind us */
	nvim_api_input(menu->gui->nvim, eina_strbuf_string_get(input),
		       (unsigned int)eina_strbuf_length_get(input));
}

static void popupmenu_item_realized_cb(void *const data, Evas_Object *const obj,
				       void *const info EINA_UNUSED)
{
	struct popupmenu *const pop = data;

	/* Retrieving the size of the realized item seems tricky... */
	Eina_List *const realized = elm_genlist_realized_items_get(obj);
	Evas_Object *const first = eina_list_data_get(realized);
	Evas_Object *const track = elm_object_item_track(first);
	evas_object_geometry_get(track, NULL, NULL, NULL, &pop->item_height);
	elm_object_item_untrack(first);
	eina_list_free(realized);

	/* When we have detected the height (may not be the first time), we unregister the
	 * callback. The height value is cached, stop trying to process it. */
	if (pop->item_height > 0) {
		evas_object_smart_callback_del_full(obj, "realized", &popupmenu_item_realized_cb,
						    data);
		if (pop->iface->resize)
			pop->iface->resize(pop);
	}
}

Evas_Object *popupmenu_item_use(const struct popupmenu *const pop, Evas_Object *const parent,
				Evas_Object *obj)
{
	if (!obj) {
		Evas *const evas = evas_object_evas_get(parent);
		obj = evas_object_textblock_add(evas);
		evas_object_size_hint_weight_set(obj, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_size_hint_align_set(obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
		evas_object_textblock_valign_set(obj, 0.5);
	}
	evas_object_size_hint_min_set(obj, -1, pop->text_height);
	evas_object_textblock_style_set(obj, pop->style);
	return obj;
}

void popupmenu_style_changed(struct popupmenu *const pop, const Evas_Textblock_Style *const style,
			     const unsigned int cell_w, const unsigned int cell_h)
{
	assert((style != NULL) && (cell_w > 0) && (cell_h > 0));
	pop->item_height = -1;
	pop->style = style;
	pop->text_height = (int)(cell_h + (cell_h / 4u));
	pop->cell_width = (int)cell_w;
	pop->cell_height = (int)cell_h;
	evas_object_smart_callback_add(pop->genlist, "realized", &popupmenu_item_realized_cb, pop);
}

void popupmenu_hide(struct popupmenu *const pop)
{
	assert(pop != NULL);
	popupmenu_clear(pop);

	evas_object_size_hint_min_set(pop->spacer, -1, 0);
	evas_object_hide(pop->genlist);
	evas_object_hide(pop->table);

	if (pop->iface->hide)
		pop->iface->hide(pop);
}

void popupmenu_clear(struct popupmenu *const pop)
{
	elm_genlist_clear(pop->genlist);
	pop->items_count = 0;
	pop->sel_item = NULL;
	pop->sel_index = -1;
	pop->sel_done = EINA_FALSE;
	pop->nvim_sel_event = EINA_FALSE;
}

void popupmenu_select_nth(struct popupmenu *const pop, const ssize_t index)
{
	assert(pop != NULL);

	/* When eovim sends a selection, it makes the menu move. This causes neovim to
	 * notify that the menu did move. We ignore this case */
	if (pop->sel_done)
		return;

	/* A negative index means: unselect */
	if (index < 0) {
		if (pop->sel_item != NULL)
			elm_genlist_item_selected_set(pop->sel_item, EINA_FALSE);
		return;
	}
	assert(index >= 0); /* <-- At this point, index is non-negative */

	/* The selection has been initiated by neovim */
	pop->nvim_sel_event = EINA_TRUE;

	/* At this point, we need to select something, but if we didn't have
         * any item previously selected (this will happen after doing a full
         * circle among the popupmenu items), start selecting the first item in
         * the popupmenu. */
	if (pop->sel_item == NULL) {
		pop->sel_item = elm_genlist_first_item_get(pop->genlist);
		pop->sel_index = 0;
	}

	if (index >= pop->sel_index) {
		/* We select an index that is after the current one */
		const ssize_t nb = index - pop->sel_index;
		for (ssize_t i = 0; i < nb; i++)
			pop->sel_item = elm_genlist_item_next_get(pop->sel_item);
	} else {
		/* We select an index that is before the current one */
		const ssize_t nb = pop->sel_index - index;
		for (ssize_t i = 0; i < nb; i++)
			pop->sel_item = elm_genlist_item_prev_get(pop->sel_item);
	}

	/* Select the item, and show it to the user */
	elm_genlist_item_selected_set(pop->sel_item, EINA_TRUE);
	elm_genlist_item_bring_in(pop->sel_item, ELM_GENLIST_ITEM_SCROLLTO_IN);
	pop->sel_index = index;
}

void popupmenu_append(struct popupmenu *const pop, void *const data)
{
	EINA_SAFETY_ON_NULL_RETURN(data);
	EINA_SAFETY_ON_NULL_RETURN(pop);

	Elm_Object_Item *const wild_item =
		elm_genlist_item_append(pop->genlist, pop->itc, data, NULL, ELM_GENLIST_ITEM_NONE,
					&popupmenu_select_func, pop);
	if (EINA_UNLIKELY(!wild_item)) {
		ERR("Failed to add item in the genlist");
		return;
	}

	elm_object_item_data_set(wild_item, data);
	pop->items_count++;
}

void popupmenu_setup(struct popupmenu *const pop, struct gui *const gui,
		     const struct popupmenu_interface *const iface,
		     Elm_Genlist_Item_Class *const itc)
{
	assert((itc != NULL) && (pop != NULL) && (gui != NULL) && (iface != NULL));

	pop->gui = gui;
	pop->itc = itc;
	pop->iface = iface;

	Evas_Object *const parent = gui->layout;
	Evas *const evas = evas_object_evas_get(parent);
	Evas_Object *o;

	/* Table: will hold both the spacer and the genlist */
	pop->table = o = elm_table_add(parent);
	evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(o);

	/* Spacer: to make the genlist fit a given size */
	pop->spacer = o = evas_object_rectangle_add(evas);
	evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_color_set(o, 0, 0, 0, 0);
	elm_table_pack(pop->table, o, 0, 0, 1, 1);
	evas_object_show(o);

	/* Menu: the genlist that will hold the wildmenu items */
	pop->genlist = o = elm_genlist_add(parent);
	evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
	elm_genlist_homogeneous_set(o, EINA_TRUE);
	elm_genlist_mode_set(o, ELM_LIST_COMPRESS);
	elm_object_tree_focus_allow_set(o, EINA_FALSE);
	elm_table_pack(pop->table, o, 0, 0, 1, 1);

	/* Create a string buffer for fast string composition */
	pop->sbuf = eina_strbuf_new();
}

void popupmenu_del(struct popupmenu *const pop)
{
	eina_strbuf_free(pop->sbuf);
}
