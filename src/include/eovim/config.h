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

struct config
{
   unsigned int version;
   unsigned int font_size;
   Eina_Stringshare *font_name;
   Eina_Bool mute_bell;

   /* Internals */
   char *path;
};

Eina_Bool config_init(void);
void config_shutdown(void);
void config_free(s_config *config);
void config_save(s_config *config);
void config_font_size_set(s_config *config, unsigned int font_size);
void config_font_name_set(s_config *config, Eina_Stringshare *font_name);
void config_bell_mute_set(s_config *config, Eina_Bool mute);
s_config *config_load(const char *filename);

#endif /* ! __EOVIM_CONFIG_H__ */
