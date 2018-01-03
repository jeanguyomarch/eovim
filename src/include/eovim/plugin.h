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

#ifndef __EOVIM_PLUGIN_H__
#define __EOVIM_PLUGIN_H__

#include "eovim/types.h"
#include <Eina.h>

typedef struct
{
   EINA_INLIST;

   Eina_Stringshare *name;
   Eina_Module *module;
   f_event_cb callback;
   Eina_Bool loaded;
} s_plugin;


Eina_Inlist *plugin_list_new(void);
void plugin_list_free(Eina_Inlist *list);
unsigned int plugin_list_load(const Eina_Inlist *plugins, const Eina_List *load_list);

Eina_Bool plugin_load(s_plugin *plugin);
Eina_Bool plugin_unload(s_plugin *plugin);
void plugin_enabled_set(Eina_Bool enabled);
Eina_Bool plugin_enabled_get(void);

Eina_Bool plugin_init(void);
void plugin_shutdown(void);

#endif /* ! __EOVIM_PLUGIN_H__ */
