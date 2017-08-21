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

   gui->layout = gui_layout_item_add(gui, "envim/main");
   if (EINA_UNLIKELY(! gui->layout))
     {
        CRI("Failed to get layout item");
        goto fail;
     }
   elm_win_resize_object_add(gui->win, gui->layout);

   evas_object_show(gui->layout);
   evas_object_show(gui->win);

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

Evas_Object *
gui_layout_item_add(const s_gui *gui,
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

Evas_Object *
gui_tabpage_add(s_gui *gui)
{
   const char group[] = "envim/tabpage";
   const char part[] = "envim.main.view";

   /* Create a GUI item for the tabpage. For now, this object will be put in
    * place of the tabpage view. When we will handle multiple tabs, we shall
    * manage this better
    */
   Evas_Object *const o = gui_layout_item_add(gui, group);
   if (EINA_UNLIKELY(! o))
     {
        CRI("Failed to get tabpage layout item (group '%s')", group);
        return NULL;
     }
   elm_layout_content_set(gui->layout, part, o); // XXX
   evas_object_show(o);
   return o;
}

Eina_Bool
gui_window_add(s_gui *gui,
               s_window *win)
{
   const char group[] = "envim/window";
   const char parent_part[] = "envim.tabpage.view";
   const char part[] = "envim.window.text";

   Evas *const evas = evas_object_evas_get(gui->win);
   Evas_Object *o;

   /* Create the window's layout */
   o = gui_layout_item_add(gui, group);
   if (EINA_UNLIKELY(! o))
     {
        CRI("Failed to get window layout item (group '%s')", group);
        goto fail;
     }
   elm_layout_content_set(win->parent->layout, parent_part, o); // XXX
   evas_object_show(o);
   win->layout = o;

   /* Create the textgrid for the window */
   o = evas_object_textgrid_add(evas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_textgrid_font_set(o, "Sans", 12); // XXX
   evas_object_textgrid_size_set(o, 80, 24); // XXX
   //nvim_win_size_set(NVIM_GUI_CONTAINER_GET(gui), win, 80, 24);
   elm_layout_content_set(win->layout, part, o); // XXX
   evas_object_show(o);
   win->textgrid = o;

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}
