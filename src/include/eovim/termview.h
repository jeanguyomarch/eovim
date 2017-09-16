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

#ifndef __EOVIM_TERMVIEW_H__
#define __EOVIM_TERMVIEW_H__

#include "eovim/types.h"
#include <Evas.h>

typedef struct termview_style s_termview_style;
typedef struct termview_color s_termview_color;

struct termview_color
{
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
};

struct termview_style
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


Eina_Bool termview_init(void);
void termview_shutdown(void);
Evas_Object *termview_add(Evas_Object *parent, s_nvim *nvim);
void termview_resize(Evas_Object *obj, unsigned int cols, unsigned int rows, Eina_Bool request);
void termview_font_set(Evas_Object *obj, const char *font_name, unsigned int font_size);
void termview_cell_size_get(const Evas_Object *obj, unsigned int *w, unsigned int *h);
void termview_size_get(const Evas_Object *obj, unsigned int *cols, unsigned int *rows);
void termview_clear(Evas_Object *obj);
void termview_eol_clear(Evas_Object *obj);
void termview_put(Evas_Object *obj, const char *string, unsigned int size);
void termview_cursor_goto(Evas_Object *obj, unsigned int to_x, unsigned int to_y);
void termview_style_set(Evas_Object *obj, const s_termview_style *style);
void termview_scroll_region_set(Evas_Object *obj, const Eina_Rectangle *region);
void termview_scroll(Evas_Object *obj, int count);
void termview_fg_color_set(Evas_Object *obj, int r, int g, int b, int a);
s_termview_color termview_color_decompose(uint32_t col);
void termview_cell_to_coords(const Evas_Object *obj, unsigned int cell_x, unsigned int cell_y, int *px, int *py);

#endif /* ! __EOVIM_TERMVIEW_H__ */
