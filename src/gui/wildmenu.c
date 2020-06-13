/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/gui.h>
#include <eovim/log.h>

#include "gui_private.h"

struct wildmenu {
	struct popupmenu pop;
};
static_assert(offsetof(struct wildmenu, pop) == 0, "popupmenu must be the first element");

#define WILDMENU_GET(PopUp) ((struct wildmenu *)PopUp)

static Elm_Genlist_Item_Class *_wildmenu_itc = NULL;

static Evas_Object *wildmenu_resuable_content_get(void *const data, Evas_Object *const obj,
						  const char *const part EINA_UNUSED,
						  Evas_Object *old)
{
	Eina_Stringshare *const item = data;
	struct wildmenu *const menu = evas_object_data_get(obj, "wildmenu");
	const struct popupmenu *const pop = &menu->pop;
	old = popupmenu_item_use(pop, obj, old);

	Eina_Strbuf *const buf = pop->sbuf;
	eina_strbuf_reset(buf);
	eina_strbuf_append_printf(buf, " %s", item);
	evas_object_textblock_text_markup_set(old, eina_strbuf_string_get(buf));
	return old;
}

static void wildmenu_item_del(void *const data, Evas_Object *const obj EINA_UNUSED)
{
	Eina_Stringshare *const item = data;
	eina_stringshare_del(item);
}

void gui_wildmenu_append(struct gui *const gui, Eina_Stringshare *const item)
{
	struct wildmenu *const wm = gui->wildmenu;
	popupmenu_append(&wm->pop, (void *)item);
}

static void wildmenu_resize(struct popupmenu *const pop)
{
	if (pop->item_height < 0)
		return;

	int win_height;
	evas_object_geometry_get(pop->gui->win, NULL, NULL, NULL, &win_height);

	const int max_height = (int)((float)win_height * 0.8f); /* 80% of the window's height */
	int height = (pop->item_height * (int)pop->items_count) + 2;
	if (height > max_height)
		height = max_height;
	evas_object_size_hint_min_set(pop->spacer, -1, height);
}

void gui_wildmenu_show(struct gui *const gui, const unsigned int pos EINA_UNUSED)
{
	struct wildmenu *const wm = gui->wildmenu;
	struct popupmenu *const pop = &wm->pop;
	gui->active_popup = pop;

	evas_object_show(pop->table);
	evas_object_show(pop->genlist);
	pop->iface->resize(pop);
}

static const struct popupmenu_interface wildmenu_iface = {
	.hide = NULL,
	.resize = &wildmenu_resize,
};

struct wildmenu *gui_wildmenu_add(struct gui *const gui)
{
	struct wildmenu *const wm = calloc(1, sizeof(*wm));
	if (EINA_UNLIKELY(!wm)) {
		CRI("Failed to allocate memory for wildmenu");
		return NULL;
	}
	struct popupmenu *const pop = &wm->pop;
	popupmenu_setup(pop, gui, &wildmenu_iface, _wildmenu_itc);
	evas_object_data_set(pop->genlist, "wildmenu", wm);

	elm_layout_content_set(gui->layout, "eovim.wildmenu", pop->table);

	return wm;
}

void gui_wildmenu_del(struct wildmenu *const wm)
{
	popupmenu_del(&wm->pop);
	free(wm);
}

void gui_wildmenu_style_set(struct wildmenu *const wm, const Evas_Textblock_Style *const style,
			    const unsigned int cell_w, const unsigned int cell_h)
{
	popupmenu_style_changed(&wm->pop, style, cell_w, cell_h);
}

Eina_Bool gui_wildmenu_init(void)
{
	_wildmenu_itc = elm_genlist_item_class_new();
	if (EINA_UNLIKELY(!_wildmenu_itc)) {
		CRI("Failed to create genlist item class");
		return EINA_FALSE;
	}
	_wildmenu_itc->item_style = "full";
	_wildmenu_itc->func.reusable_content_get = &wildmenu_resuable_content_get;
	_wildmenu_itc->func.del = &wildmenu_item_del;
	return EINA_TRUE;
}

void gui_wildmenu_shutdown(void)
{
	elm_genlist_item_class_free(_wildmenu_itc);
}
