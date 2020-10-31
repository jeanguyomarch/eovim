/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_GUI_H__
#define __EOVIM_GUI_H__

#include <Elementary.h>

#include "eovim/types.h"




struct gui {
	Evas *evas;
	Evas_Object *win;
	Evas_Object *layout;
	Evas_Object *edje;
	Evas_Object *sizing_textgrid;

	struct wildmenu *wildmenu;
	struct completion *completion;
	struct cursor *cursor;
	struct popupmenu *active_popup;
	struct cmdline *cmdline;

	/* Configuration parameters of the theme */
	struct {
		Eina_Bool bell_enabled;
		Eina_Bool react_to_key_presses;
		Eina_Bool react_to_caps_lock;
		Eina_Bool cursor_cuts_ligatures;
	} theme;

	struct {
		Eina_Hash *styles;
		Eina_Strbuf *text;

		Eina_Hash *hl_groups;

		/* Map of strings that associates to a kind identifier (used by completion) to
		 * a style string that is compatible with Evas_textblock */
		Eina_Hash *kind_styles;
		/* Map of strings that associates a cmdline prompt to
		 * a style string that is compatible with Evas_textblock */
		Eina_Hash *cmdline_styles;

		Evas_Textblock_Style *object;
		union color default_fg;
		union color default_bg;
		union color default_sp;

		Eina_Stringshare *font_name;
		unsigned int font_size;
		unsigned int line_gap;
	} style;

	struct nvim *nvim;
	Eina_Inarray *tabs;

	Eina_List *grids;

	/** Keep track of how many times gui_busy_set() was called. This prevents
    * useless calls to the theme or nested set issues */
	int busy_count;


	Eina_Bool pending_style_update;
	Eina_Bool need_nvim_resize;

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

void gui_linespace_set(struct gui *gui, unsigned int linespace);

void gui_bell_ring(struct gui *gui);

void gui_cmdline_show(struct gui *gui, const char *content, Eina_Stringshare *prompt,
		      Eina_Stringshare *firstc);
void gui_cmdline_hide(struct gui *gui);

void gui_size_recalculate(struct gui *gui);

void gui_active_popupmenu_hide(struct gui *gui);
void gui_active_popupmenu_select_nth(struct gui *gui, ssize_t index);
void gui_completion_reset(struct gui *gui);

void gui_cmdline_cursor_pos_set(struct gui *gui, size_t pos);

void gui_redraw_end(struct gui *gui);
void gui_flush(struct gui *gui);

void gui_title_set(struct gui *gui, const char *title);
void gui_font_set(struct gui *gui, const char *font_name, unsigned int font_size);

struct grid *gui_grid_get(const struct gui *gui, size_t index);
void grid_cell_size_set(struct grid *sd, unsigned int cell_w, unsigned int cell_h);
void gui_style_update_defer(struct gui *gui);
void gui_resize_defer(struct gui *gui);


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
