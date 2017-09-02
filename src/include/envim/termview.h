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

#ifndef __ENVIM_TERMVIEW_H__
#define __ENVIM_TERMVIEW_H__

typedef struct termview_style s_termview_style;
typedef struct termview_color s_termview_color;

#include "envim/types.h"
#include "envim_termview.eo.h"
#include <Evas.h>

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

Evas_Object *termview_add(Evas_Object *parent);

#endif /* ! __ENVIM_TERMVIEW_H__ */
