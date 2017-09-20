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

#ifndef __EOVIM_CONFIG_H__
#define __EOVIM_CONFIG_H__

#include "eovim/types.h"
#include <Eina.h>

typedef struct config_color s_config_color;

struct config_color
{
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
};

struct config
{
   unsigned int version;
   unsigned int font_size;
   Eina_Stringshare *font_name;
   s_config_color *bg_color;
   Eina_Bool use_bg_color;
};

Eina_Bool config_init(void);
void config_shutdown(void);
void config_bg_color_set(s_config *config, int r, int g, int b, int a);
void config_free(s_config *config);
void config_save(s_config *config);
void config_font_size_set(s_config *config, unsigned int font_size);
void config_font_name_set(s_config *config, Eina_Stringshare *font_name);
void config_use_bg_color_set(s_config *config, Eina_Bool use);
s_config *config_load(void);

#endif /* ! __EOVIM_CONFIG_H__ */
