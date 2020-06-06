/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_GUI_H__
#define __EOVIM_GUI_H__

#include <Elementary.h>

#include "eovim/termview.h"
#include "eovim/types.h"

struct gui {
	Evas_Object *win;
	Evas_Object *layout;
	Evas_Object *edje;
	Evas_Object *termview;

	/** The cache is a stringbuffer used to perform dynamic string operations.
    * Since the whole gui code is executed on the main thread, this is very
    * fine to use a global like this, as long as its use is restricted to a
    * known event scope.
    */
	Eina_Strbuf *cache;

	struct {
		Evas_Object *obj;
		Evas_Object *gl;
		Elm_Object_Item *sel;
		size_t items_count;
		size_t max_type_len;
		size_t max_word_len;
		Eina_Bool nvim_sel_event;
	} completion;

	struct {
		Evas_Object *menu;
		Evas_Object *table;
		Evas_Object *spacer;
		Elm_Object_Item *sel_item;
		size_t items_count;
		ssize_t sel_index;
		size_t cpos; /**< Cursor position */
		Eina_Bool nvim_sel_event; /**< Nvim initiated a selection */
	} cmdline;

	struct {
		Eina_Stringshare *name;
		unsigned int size;
	} font;

	/* Configuration parameters of the theme */
	struct {
		Eina_Bool bell_enabled;
		Eina_Bool react_to_key_presses;
		Eina_Bool react_to_caps_lock;
	} theme;

	struct nvim *nvim;
	Eina_Inarray *tabs;

	/** Keep track of how many times gui_busy_set() was called. This prevents
    * useless calls to the theme or nested set issues */
	int busy_count;

	/** True when the caps lock warning is on, False otherwise */
	Eina_Bool capslock_warning;
	unsigned int active_tab; /**< Identifier of the active tab */
};

/**
 * This enumeration is *DIRECTLY* mapped on the vim values of the 'showtabline'
 * parameter: https://neovim.io/doc/user/options.html#'showtabline'
 */
enum gui_tabline {
	GUI_TABLINE_NEVER = 0, /**< Never show the tabline */
	GUI_TABLINE_AT_LEAST_TWO = 1,
	/**< Show the tabline if at least two tabs are open */
	GUI_TABLINE_ALWAYS = 2, /**< Always show the tabline */
};

/**
 * This enumeration is mapped to the possible values of the 'ambiwidth' VIM
 * parameter: https://neovim.io/doc/user/options.html#'ambiwidth'.
 * This tells VIM what to do with characters with ambiguous width classes.
 *
 * - "single": same width as US-characters
 * - "double": twice the width of ASCII characters
 */
enum gui_ambiwidth {
	GUI_AMBIWIDTH_SINGLE, /**< Same width as US-characters */
	GUI_AMBIWIDTH_DOUBLE, /**< Twice the width of ASCII */
};

Eina_Bool gui_init(void);
void gui_shutdown(void);

Eina_Bool gui_add(struct gui *gui, struct nvim *nvim);
void gui_del(struct gui *gui);
void gui_resize(struct gui *gui, unsigned int cols, unsigned int rows);
void gui_busy_set(struct gui *gui, Eina_Bool busy);
void gui_config_show(struct gui *gui);
void gui_config_hide(struct gui *gui);
void gui_die(struct gui *gui, const char *fmt, ...) EINA_PRINTF(2, 3);
void gui_default_colors_set(struct gui *gui, union color fg, union color bg, union color sp);

void gui_completion_prepare(struct gui *gui, size_t items);
void gui_completion_show(struct gui *gui, size_t max_word_len, size_t max_menu_len, int selected,
			 unsigned int x, unsigned int y);
void gui_completion_hide(struct gui *gui);
void gui_completion_clear(struct gui *gui);
void gui_completion_add(struct gui *gui, struct completion *completion);
void gui_completion_selected_set(struct gui *gui, int index);

void gui_bell_ring(struct gui *gui);

void gui_cmdline_show(struct gui *gui, const char *content, const char *prompt, const char *firstc);
void gui_cmdline_hide(struct gui *gui);

void gui_size_recalculate(struct gui *gui);

void gui_wildmenu_clear(struct gui *gui);
void gui_wildmenu_append(struct gui *gui, Eina_Stringshare *item);
void gui_wildmenu_show(struct gui *gui);
void gui_wildmenu_select(struct gui *gui, ssize_t index);

void gui_cmdline_cursor_pos_set(struct gui *gui, size_t pos);

void gui_title_set(struct gui *gui, const char *title);
void gui_font_set(struct gui *gui, const char *font_name, unsigned int font_size);

void gui_tabs_reset(struct gui *gui);
void gui_tabs_add(struct gui *gui, const char *name, unsigned int id, Eina_Bool active);
void gui_tabs_show(struct gui *gui);
void gui_tabs_hide(struct gui *gui);

void gui_caps_lock_alert(struct gui *gui);
void gui_caps_lock_dismiss(struct gui *gui);
Eina_Bool gui_caps_lock_warning_get(const struct gui *gui);

void gui_ready_set(struct gui *gui);
void gui_mode_update(struct gui *gui, const struct mode *mode);

#endif /* ! __EOVIM_GUI_H__ */
