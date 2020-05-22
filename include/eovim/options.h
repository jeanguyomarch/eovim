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

#ifndef __EOVIM_OPTIONS_H__
#define __EOVIM_OPTIONS_H__

#include "eovim/types.h"

typedef struct
{
   s_geometry geometry;

   const char *config_path;
   const char *nvim_prog;
   const char *theme;

   Eina_Bool fullscreen;
   Eina_Bool maximized; /**< Eovim will run in a maximized window */
   Eina_Bool forbidden;
} s_options;

typedef enum
{
   OPTIONS_RESULT_ERROR,
   OPTIONS_RESULT_QUIT,
   OPTIONS_RESULT_CONTINUE,
} e_options_result;

e_options_result options_parse(int argc, const char *argv[], s_options *opts);
void options_defaults_set(s_options *opts);

#endif /* ! __EOVIM_OPTIONS_H__ */
