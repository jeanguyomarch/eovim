/*
 * Copyright (c) 2017 Jean Guyomarc'h
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
#include "eovim/types.h"

struct gui
{
   Evas_Object *win;
   Evas_Object *layout;
   Evas_Object *edje;
   Evas_Object *termview;

   struct {
      Evas_Object *gl;
      Elm_Genlist_Item *sel;
      Eina_Bool event;
   } completion;

   struct {
      Evas_Object *box;
      Evas_Object *bg_sel;
      Evas_Object *bg_sel_frame;
      Evas_Object *fonts_gl;
   } config;

   s_nvim *nvim;

   /** Keep track of how many times gui_busy_set() was called. This prevents
    * useless calls to the theme or nested set issues */
   int busy_count;
};

Eina_Bool gui_init(void);
void gui_shutdown(void);

Eina_Bool gui_add(s_gui *gui, s_nvim *nvim);
void gui_del(s_gui *gui);
void gui_resize(s_gui *gui, unsigned int cols, unsigned int rows);
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
void gui_die(s_gui *gui, const char *fmt, ...);

void gui_completion_show(s_gui *gui, unsigned int selected, unsigned int x, unsigned int y);
void gui_completion_hide(s_gui *gui);
void gui_completion_clear(s_gui *gui);
void gui_completion_add(s_gui *gui, s_completion *completion);
void gui_completion_selected_set(s_gui *gui, int index);

void gui_bell_ring(s_gui *gui);
void gui_fullscreen_set(s_gui *gui, Eina_Bool fullscreen);

#endif /* ! __EOVIM_GUI_H__ */
