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

typedef struct gui_style s_gui_style;

typedef struct gui_color s_gui_color;

struct gui_color
{
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
};

struct gui_style
{
   t_int fg_color;
   t_int bg_color;
   t_int sp_color;
   Eina_Bool reverse;
   Eina_Bool italic;
   Eina_Bool bold;
   Eina_Bool underline;
   Eina_Bool undercurl;
};

struct gui
{
   Evas_Object *win;
   Evas_Object *layout;
   Evas_Object *textgrid; /* XXX One for tab */

   Eina_Hash *palettes;
   Eina_Stringshare *font_name;
   unsigned int font_size;
   unsigned int cell_w;
   unsigned int cell_h;
   unsigned int rows;
   unsigned int cols;

   unsigned int palette_id_generator;

   struct {
      uint8_t fg;
      uint8_t bg;
      uint8_t sp;
      Eina_Bool reverse;
      Eina_Bool italic;
      Eina_Bool bold;
      Eina_Bool underline;
      Eina_Bool undercurl;
   } current;


   /* Writing position */
   unsigned int x;
   unsigned int y;
};

Eina_Bool gui_init(void);
void gui_shutdown(void);
Eina_Bool gui_add(s_gui *gui);
void gui_del(s_gui *gui);
void gui_resize(s_gui *gui, unsigned int cols, unsigned int rows);
void gui_clear(s_gui *gui);
void gui_put(s_gui *gui, const char *string, unsigned int size);
void gui_cursor_goto(s_gui *gui, unsigned int to_x, unsigned int to_y);
void gui_style_set(s_gui *gui, const s_gui_style *style);
void gui_update_fg(s_gui *gui, t_int color);
void gui_update_bg(s_gui *gui, t_int color);
void gui_update_sp(s_gui *gui, t_int color);

#endif /* ! __ENVIM_GUI_H__ */
