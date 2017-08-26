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


static void
_textgrid_mouse_move_cb(void *data EINA_UNUSED,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
}

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

   /* Window setup */
   gui->win = elm_win_util_standard_add("envim", "Envim");
   elm_win_autodel_set(gui->win, EINA_TRUE);

   /* Main Layout setup */
   gui->layout = _layout_item_add(gui, "envim/main");
   if (EINA_UNLIKELY(! gui->layout))
     {
        CRI("Failed to get layout item");
        goto fail;
     }
   elm_win_resize_object_add(gui->win, gui->layout);

   /* Font setup (FIXME: use a config) */
   gui->font_name = eina_stringshare_add("Mono");
   gui->font_size = 14;

   /* Textgrid setup */
   Evas *const evas = evas_object_evas_get(gui->layout);
   Evas_Object *o;
   gui->textgrid = o = evas_object_textgrid_add(evas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE, _textgrid_mouse_move_cb, gui);
   evas_object_textgrid_font_set(o, gui->font_name, (int)gui->font_size);
   evas_object_textgrid_cell_size_get(o, (int*)&gui->cell_w, (int*)&gui->cell_h);

   //evas_object_textgrid_palette_set(o, EVAS_TEXTGRID_PALETTE_STANDARD, PAL_X,
   //                                 255, 255, 36, 255);
   //evas_object_textgrid_palette_set(o, EVAS_TEXTGRID_PALETTE_STANDARD, PAL_Y,
   //                                 255, 0, 36, 255);

   elm_layout_content_set(gui->layout, "envim.main.view", gui->textgrid);

   evas_object_show(gui->textgrid);
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
        eina_stringshare_del(gui->font_name);
     }
}

void
gui_resize(s_gui *gui,
           unsigned int cols,
           unsigned int rows)
{
   /*
    * To resize the gui, we first resize the textgrid with the new amount
    * of columns and rows, then we resize the window. UI widgets will
    * automatically be sized to the window's frame.
    *
    * XXX: This won't work with the tabline!
    */
   evas_object_textgrid_size_set(gui->textgrid, (int)cols, (int)rows);
   evas_object_resize(gui->win, (int)(cols * gui->cell_w),
                      (int)(rows * gui->cell_h));
}
