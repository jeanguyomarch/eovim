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

#ifndef __ENVIM_GUI_H__
#define __ENVIM_GUI_H__

#include <Elementary.h>

#include "envim/termview.h"
#include "envim/types.h"

struct gui
{
   Evas_Object *win;
   Evas_Object *layout;
   Evas_Object *termview;

   struct {
      Evas_Object *box;
      Evas_Object *fg_col;
   } config;

   s_nvim *nvim;

   Eina_Bool use_mouse;
};

Eina_Bool gui_add(s_gui *gui, s_nvim *nvim);
void gui_del(s_gui *gui);
void gui_resize(s_gui *gui, unsigned int cols, unsigned int rows);
void gui_clear(s_gui *gui);
void gui_eol_clear(s_gui *gui);
void gui_put(s_gui *gui, const char *string, unsigned int size);
void gui_cursor_goto(s_gui *gui, unsigned int to_x, unsigned int to_y);
void gui_style_set(s_gui *gui, const s_termview_style *style);
void gui_update_fg(s_gui *gui, t_int color);
void gui_update_bg(s_gui *gui, t_int color);
void gui_update_sp(s_gui *gui, t_int color);
void gui_scroll_region_set(s_gui *gui, int x, int y, int w, int h);
void gui_scroll(s_gui *gui, int scroll);
void gui_busy_set(s_gui *gui, Eina_Bool busy);

void gui_mouse_enabled_set(s_gui *gui, Eina_Bool enable);
void gui_config_show(s_gui *gui);
void gui_config_hide(s_gui *gui);

#endif /* ! __ENVIM_GUI_H__ */
