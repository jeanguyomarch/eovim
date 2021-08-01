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

	struct wildmenu *wildmenu;
	struct completion *completion;
	struct cursor *cursor;
	struct popupmenu *active_popup;
	struct cmdline *cmdline;

	struct {
		Eina_Stringshare *name;
		unsigned int size;
	} font;

	union color default_fg;

	/* Configuration parameters of the theme */
	struct {
		Eina_Bool bell_enabled;
		Eina_Bool react_to_key_presses;
		Eina_Bool react_to_caps_lock;
		Eina_Bool cursor_cuts_ligatures;
		Eina_Bool cursor_animated;
		double cursor_animation_duration;
		Ecore_Pos_Map cursor_animation_style;
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

Eina_Bool gui_init(void);
Eina_Bool gui_completion_init(void);
Eina_Bool gui_wildmenu_init(void);
void gui_shutdown(void);
void gui_completion_shutdown(void);
void gui_wildmenu_shutdown(void);

void gui_wildmenu_append(struct gui *gui, Eina_Stringshare *item);
void gui_wildmenu_show(struct gui *gui, unsigned int pos);

void gui_completion_append(struct gui *gui, const char *word, uint32_t word_size, const char *kind,
			   uint32_t kind_size, const char *menu, uint32_t menu_size,
			   const char *info, uint32_t info_size);
void gui_completion_show(struct gui *gui, unsigned int col, unsigned int row);

Eina_Bool gui_add(struct gui *gui, struct nvim *nvim);
void gui_del(struct gui *gui);
void gui_resize(struct gui *gui, unsigned int cols, unsigned int rows);
void gui_busy_set(struct gui *gui, Eina_Bool busy);
void gui_die(struct gui *gui, const char *fmt, ...) EINA_PRINTF(2, 3);
void gui_default_colors_set(struct gui *gui, union color fg, union color bg, union color sp);

void gui_bell_ring(struct gui *gui);

void gui_cmdline_show(struct gui *gui, const char *content, Eina_Stringshare *prompt,
		      Eina_Stringshare *firstc);
void gui_cmdline_hide(struct gui *gui);

void gui_size_recalculate(struct gui *gui);

void gui_active_popupmenu_hide(struct gui *gui);
void gui_active_popupmenu_select_nth(struct gui *gui, ssize_t index);
void gui_completion_reset(struct gui *gui);

void gui_cmdline_cursor_pos_set(struct gui *gui, size_t pos);

void gui_title_set(struct gui *gui, const char *title);
void gui_font_set(struct gui *gui, const char *font_name, unsigned int font_size);
void gui_font_size_update(struct gui *gui, long new_size);

void gui_tabs_reset(struct gui *gui);
void gui_tabs_add(struct gui *gui, const char *name, unsigned int id, Eina_Bool active);
void gui_tabs_show(struct gui *gui);
void gui_tabs_hide(struct gui *gui);

void gui_caps_lock_alert(struct gui *gui);
void gui_caps_lock_dismiss(struct gui *gui);
Eina_Bool gui_caps_lock_warning_get(const struct gui *gui);

void gui_ready_set(struct gui *gui);
void gui_mode_update(struct gui *gui, const struct mode *mode);
Eina_Bool gui_cmdline_enabled_get(const struct gui *gui);

#endif /* ! __EOVIM_GUI_H__ */
