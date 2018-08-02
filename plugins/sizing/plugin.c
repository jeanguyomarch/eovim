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

#include "plugin_sizing.h"

#include <Eovim.h>
#include <Elementary.h>

EINA_MODULE_INIT(eovim_plugin_sizing_init);
EINA_MODULE_SHUTDOWN(eovim_plugin_sizing_shutdown);

static Eina_Bool
eovim_sizing_eval(s_nvim *nvim,
                  const msgpack_object_array *args)
{
  Evas_Object *const win = eovim_window_get(nvim);
  Eina_Hash *const params = eovim_plugin_sizing_parse(args);
  if (EINA_UNLIKELY(! params))
  { return EINA_FALSE; }

  Eina_Bool status = EINA_TRUE;
  Eina_Stringshare *const aspect = eina_hash_find(params, S_ASPECT);
  if (aspect)
  {
    if (aspect == S_FULLSCREEN_ON)
    { elm_win_fullscreen_set(win, EINA_TRUE); }
    else if (aspect == S_FULLSCREEN_OFF)
    { elm_win_fullscreen_set(win, EINA_FALSE); }
    else if (aspect == S_FULLSCREEN_TOGGLE)
    {
      const Eina_Bool win_status = elm_win_fullscreen_get(win);
      if (win_status)
      { elm_win_fullscreen_set(win, EINA_FALSE); }
      else
      { elm_win_fullscreen_set(win, EINA_TRUE); }
    }
    else
    {
      ERR("Unknown aspect parameter: '%s'", aspect);
      status = EINA_FALSE;
    }
  }
  eina_hash_free(params);
  return status;
}
EOVIM_PLUGIN_SYMBOL(eovim_sizing_eval);

EINA_MODULE_LICENSE("MIT");
EINA_MODULE_AUTHOR("Jean Guyomarc'h");
EINA_MODULE_VERSION(EOVIM_VERSION);
EINA_MODULE_DESCRIPTION("Sizing plugin to modify the dimensions of Eovim")
