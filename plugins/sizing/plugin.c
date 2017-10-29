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

#include <Eovim.h>

static Eina_Bool
eovim_sizing_eval(s_nvim *nvim EINA_UNUSED,
                  const msgpack_object_array *args EINA_UNUSED)
{
   printf("This is the plugin\n");
   return EINA_TRUE;
}
EOVIM_PLUGIN_SYMBOL(eovim_sizing_eval);

EINA_MODULE_LICENSE("MIT");
EINA_MODULE_AUTHOR("Jean Guyomarc'h");
EINA_MODULE_VERSION(EOVIM_VERSION);
EINA_MODULE_DESCRIPTION("Sizing plugin to modifies the dimensions of Eovim from Neovim")
