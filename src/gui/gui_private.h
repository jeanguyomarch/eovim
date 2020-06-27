/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef EOVIM_GUI_PRIVATE_H__
#define EOVIM_GUI_PRIVATE_H__

#include <Elementary.h>
#include <stddef.h>
#include <assert.h>

struct gui;
struct wildmenu;
struct completion;

struct popupmenu_interface {
	void (*const hide)(struct popupmenu *);
	void (*const resize)(struct popupmenu *);
};

struct popupmenu {
	struct gui *gui;
	Elm_Genlist_Item_Class *itc; /**< non-owning pointer */
	const struct popupmenu_interface *iface;
	Evas_Object *genlist;
	Evas_Object *table;
	Evas_Object *spacer;
	Eina_Strbuf *sbuf;
	Elm_Object_Item *sel_item; /**< Selected item */
	const Evas_Textblock_Style *style;
	ssize_t sel_index; /**< Selected item index */
	size_t items_count;
	int item_height;
	int text_height;
	int cell_width;
	int cell_height;
	Eina_Bool nvim_sel_event; /**< Neovim initiated a selection */
	Eina_Bool sel_done; /**< When Eovim triggered a selection */
};

static inline unsigned int gui_style_hash(const char *const style)
{
	return (unsigned int)eina_hash_superfast(style, (int)strlen(style));
}

Evas_Object *gui_layout_item_add(Evas_Object *parent, const char *group);

void popupmenu_setup(struct popupmenu *pop, struct gui *gui,
		     const struct popupmenu_interface *iface, Elm_Genlist_Item_Class *itc);
void popupmenu_del(struct popupmenu *pop);
void popupmenu_hide(struct popupmenu *pop);
void popupmenu_clear(struct popupmenu *pop);
void popupmenu_append(struct popupmenu *pop, void *data);
void popupmenu_select_nth(struct popupmenu *pop, ssize_t index);
void popupmenu_style_changed(struct popupmenu *pop, const Evas_Textblock_Style *style,
			     unsigned int cell_w, unsigned int cell_h);

Evas_Object *popupmenu_item_use(const struct popupmenu *pop, Evas_Object *parent, Evas_Object *obj);

struct wildmenu *gui_wildmenu_add(struct gui *gui);
void gui_wildmenu_del(struct wildmenu *wm);
void gui_wildmenu_style_set(struct wildmenu *wm, const Evas_Textblock_Style *style,
			    unsigned int cell_w, unsigned int cell_h);
struct completion *gui_completion_add(struct gui *gui);
void gui_completion_style_set(struct completion *cmpl, const Evas_Textblock_Style *style,
			      unsigned int cell_w, unsigned int cell_h);
void gui_completion_del(struct completion *cmpl);

struct cmdline *cmdline_add(struct gui *gui);
void cmdline_del(struct cmdline *cmd);

/*****************************************************************************
 * Cursor Internal API
 *****************************************************************************/

struct cursor *cursor_add(struct gui *gui);
void cursor_del(struct cursor *cur);
void cursor_blink_disable(struct cursor *cur);
void cursor_color_set(struct cursor *cur, union color color);
void cursor_mode_set(struct cursor *cur, const struct mode *mode);
void cursor_focus_set(struct cursor *cur, Eina_Bool focused);
void gui_cursor_calc(struct gui *gui, int x, int y, int w, int h);
void gui_cursor_key_pressed(struct gui *gui);

static inline void color_class_set(const char *const name, const union color col)
{
	edje_color_class_set(name, col.r, col.g, col.b, col.a, 0, 0, 0, 0, 0, 0, 0, 0);
}

#endif /* ! EOVIM_GUI_PRIVATE_H__ */
