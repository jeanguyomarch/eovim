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

#include "Envim.h"

static Evas_Object *
_layout_item_add(const s_gui *gui,
                 const char *group)
{
   Evas_Object *const o = elm_layout_add(gui->win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   const Eina_Bool ok = elm_layout_file_set(o, main_edje_file_get(), group);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to set layout");
        goto fail;
     }

   return o;
fail:
   evas_object_del(o);
   return NULL;
}

Eina_Bool
gui_init(void)
{
   return EINA_TRUE;
}

void
gui_shutdown(void)
{
}

Eina_Bool
gui_add(s_gui *gui)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(gui, EINA_FALSE);

   gui->win = elm_win_util_standard_add("envim", "Envim");
   elm_win_autodel_set(gui->win, EINA_TRUE);

   gui->layout = _layout_item_add(gui, "envim/main");
   if (EINA_UNLIKELY(! gui->layout))
     {
        CRI("Failed to get layout item");
        goto fail;
     }
   elm_win_resize_object_add(gui->win, gui->layout);

   evas_object_show(gui->layout);
   evas_object_show(gui->win);
   evas_object_resize(gui->win, 800, 600);

   return EINA_TRUE;
fail:
   evas_object_del(gui->win);
   return EINA_FALSE;
}

void
gui_del(s_gui *gui)
{
   if (EINA_LIKELY(gui != NULL))
     {
        evas_object_del(gui->win);
     }
}
