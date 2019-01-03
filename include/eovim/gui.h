/*
 * Copyright (c) 2017-2018 Jean Guyomarc'h
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __EOVIM_GUI_H__
#define __EOVIM_GUI_H__

#include <Elementary.h>

#include "eovim/termview.h"
#include "eovim/prefs.h"
#include "eovim/types.h"

struct gui
{
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

   s_prefs prefs;

   s_nvim *nvim;
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
typedef enum
{
   GUI_TABLINE_NEVER = 0, /**< Never show the tabline */
   GUI_TABLINE_AT_LEAST_TWO = 1,
     /**< Show the tabline if at least two tabs are open */
   GUI_TABLINE_ALWAYS = 2, /**< Always show the tabline */
} e_gui_tabline;

/**
 * This enumeration is mapped to the possible values of the 'ambiwidth' VIM
 * parameter: https://neovim.io/doc/user/options.html#'ambiwidth'.
 * This tells VIM what to do with characters with ambiguous width classes.
 *
 * - "single": same width as US-characters
 * - "double": twice the width of ASCII characters
 */
typedef enum
{
   GUI_AMBIWIDTH_SINGLE, /**< Same width as US-characters */
   GUI_AMBIWIDTH_DOUBLE, /**< Twice the width of ASCII */
} e_gui_ambiwidth;

/**
 * This strutured type contains all the UI options that NVIM sends through the
 * 'options_set' message of the 'redraw' method.
 */
typedef struct
{
   const char *guifont;
     /**< https://neovim.io/doc/user/options.html#'guifont' */
   const char *guifontset;
     /**< https://neovim.io/doc/user/options.html#'guifontset' */
   const char *guifontwide;
     /**< https://neovim.io/doc/user/options.html#'guifontwide' */
   e_gui_tabline show_tabline;
     /**< Options to show the tabline */
   e_gui_ambiwidth ambiwidth;
     /**< Ambiguous width options */
   int linespace;
     /**< Spacing between two lines */
   Eina_Bool ext_cmdline; /**< State of the cmdline */
   Eina_Bool ext_popupmenu; /**< State of the popupmenu */
   Eina_Bool ext_tabline; /**< State of the tabline */
   Eina_Bool ext_wildmenu; /**< State of the wildmenu */
   Eina_Bool termguicolors; /**< State of the GUI colors */
   Eina_Bool arabicshape;
     /**< https://neovim.io/doc/user/options.html#'arabicshape' */
   Eina_Bool emoji;
     /**< https://neovim.io/doc/user/options.html#'emoji' */
} s_gui_option;

Eina_Bool gui_init(void);
void gui_shutdown(void);

Eina_Bool gui_add(s_gui *gui, s_nvim *nvim);
void gui_del(s_gui *gui);
void gui_resize(s_gui *gui, unsigned int cols, unsigned int rows);
void gui_resized_confirm(s_gui *gui, unsigned int cols, unsigned int rows);
void gui_clear(s_gui *gui);
void gui_eol_clear(s_gui *gui);
void gui_put(s_gui *gui, const Eina_Unicode *ustring, unsigned int size);
void gui_cursor_goto(s_gui *gui, unsigned int to_x, unsigned int to_y);
void gui_style_set(s_gui *gui, const s_termview_style *style);
void gui_update_fg(s_gui *gui, t_int color);
void gui_update_bg(s_gui *gui, t_int color);
void gui_update_sp(s_gui *gui, t_int color);
void gui_scroll_region_set(s_gui *gui, int x, int y, int w, int h);
void gui_scroll(s_gui *gui, int scroll);
void gui_busy_set(s_gui *gui, Eina_Bool busy);
void gui_bg_color_set(s_gui *gui, int r, int g, int b, int a);
void gui_config_show(s_gui *gui);
void gui_config_hide(s_gui *gui);
void gui_die(s_gui *gui, const char *fmt, ...) EINA_PRINTF(2, 3);

void gui_completion_prepare(s_gui *gui, size_t items, size_t max_word_len, size_t max_menu_len);
void gui_completion_show(s_gui *gui, int selected, unsigned int x, unsigned int y);
void gui_completion_hide(s_gui *gui);
void gui_completion_clear(s_gui *gui);
void gui_completion_add(s_gui *gui, s_completion *completion);
void gui_completion_selected_set(s_gui *gui, int index);

void gui_bell_ring(s_gui *gui);
void gui_fullscreen_set(s_gui *gui, Eina_Bool fullscreen);
void gui_maximized_set(s_gui *gui, Eina_Bool maximized);

void gui_cmdline_show(s_gui *gui, const char *content,
                      const char *prompt, const char *firstc);
void gui_cmdline_hide(s_gui *gui);

void gui_size_recalculate(s_gui *gui);

void gui_wildmenu_clear(s_gui *gui);
void gui_wildmenu_append(s_gui *gui, Eina_Stringshare *item);
void gui_wildmenu_show(s_gui *gui);
void gui_wildmenu_select(s_gui *gui, ssize_t index);

void gui_cmdline_cursor_pos_set(s_gui *gui, size_t pos);

void gui_title_set(s_gui *gui, const char *title);

void gui_tabs_reset(s_gui *gui);
void gui_tabs_add(s_gui *gui, const char *name, unsigned int id, Eina_Bool active);
void gui_tabs_show(s_gui *gui);
void gui_tabs_hide(s_gui *gui);

void gui_caps_lock_alert(s_gui *gui);
void gui_caps_lock_dismiss(s_gui *gui);
Eina_Bool gui_caps_lock_warning_get(const s_gui *gui);

void gui_mode_update(s_gui *gui, const s_mode *mode);

#endif /* ! __EOVIM_GUI_H__ */
