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
_focus_in_cb(void *data,
             Evas_Object *obj EINA_UNUSED,
             void *event EINA_UNUSED)
{
   s_gui *const gui = data;
   evas_object_focus_set(gui->termview, EINA_TRUE);
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
static Eina_Bool _cb(s_gui *gui)
{
 //  WRN("Config");
   static int i = 0;
 //  if (i % 2 == 0)
 //    gui_config_show(gui);
 //  else
 //    gui_config_hide(gui);
   i++;
   return 1;
}

static Evas_Object *
_frame_add(Evas_Object *parent,
           const char *text)
{
   Evas_Object *f;

   f = elm_frame_add(parent);
   elm_frame_autocollapse_set(f, EINA_TRUE);
   elm_object_text_set(f, text);
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(f, EVAS_HINT_FILL, 0.0);
   evas_object_show(f);

   return f;
}

static Evas_Object *
_config_box_add(s_gui *gui)
{
   Evas_Object *o;

   o = elm_box_add(gui->layout);
   elm_box_horizontal_set(o, EINA_FALSE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   evas_object_show(o);

   return o;
}

static Evas_Object *
_colorselector_add(Evas_Object *parent)
{
   Evas_Object *const o = elm_colorselector_add(parent);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);

   return o;
}

static void
_fg_color_changed_cb(void *data,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   s_gui *const gui = data;
   int r, g, b, a;

   elm_colorselector_color_get(obj, &r, &g, &b, &a);
   config_fg_color_set(gui->nvim->config, r, g, b, a);
   termview_fg_color_set(gui->termview, r, g, b, a);
}

static Evas_Object *
_config_fg_add(s_gui *gui, Evas_Object *parent)
{
   Evas_Object *const f = _frame_add(parent, "Default Foreground Color");
   Evas_Object *const sel = _colorselector_add(f);

   evas_object_smart_callback_add(sel, "changed", _fg_color_changed_cb, gui);

   elm_object_content_set(f, sel);

//   elm_box_pack_end(parent, f);
//   elm_layout_content_set(gui->layout, "envim.config.fg_color", btn);

   return f;
}

void
gui_config_show(s_gui *gui)
{
   if (gui->config.box) { return; }

   Evas_Object *const box = _config_box_add(gui);
   Evas_Object *o;

   o = _config_fg_add(gui, box);
   elm_box_pack_end(box, o);
   elm_layout_content_set(gui->layout, "envim.config.fg_color", box);

   elm_layout_signal_emit(gui->layout, "config,show", "envim");

   gui->config.box = box;
}


void
gui_config_hide(s_gui *gui)
{
  // elm_layout_signal_emit(gui->layout, "config,hide", "envim");
}

Eina_Bool
gui_add(s_gui *gui,
        s_nvim *nvim)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(gui, EINA_FALSE);

   gui->nvim = nvim;

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
   evas_object_smart_callback_add(gui->win, "focus,in", _focus_in_cb, gui);

   gui->termview = termview_add(gui->layout, nvim);
   /* FIXME Use a config */
   termview_font_set(gui->termview, "Mono", 14);

   /*
    * We set the resieing step of the window to the size of a cell of the
    * textgrid that is embedded within the termview.
    */
   unsigned int cell_w, cell_h;
   termview_cell_size_get(gui->termview, &cell_w, &cell_h);
   elm_win_size_step_set(gui->win, (int)cell_w, (int)cell_h);

   elm_layout_content_set(gui->layout, "envim.main.view", gui->termview);
   _config_fg_add(gui, gui->layout);

   evas_object_show(gui->termview);
   evas_object_show(gui->layout);
   evas_object_show(gui->win);

   ecore_timer_add(2.0, _cb, gui);

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

void
gui_resize(s_gui *gui,
           unsigned int cols,
           unsigned int rows)
{
   unsigned int cell_w, cell_h, old_cols, old_rows;

   termview_size_get(gui->termview, &old_cols, &old_rows);

   /* Don't resize if not needed */
   if ((old_cols == cols) && (old_rows == rows)) { return; }

   /*
    * To resize the gui, we first resize the textgrid with the new amount
    * of columns and rows, then we resize the window. UI widgets will
    * automatically be sized to the window's frame.
    *
    * XXX: This won't work with the tabline!
    */
   termview_resize(gui->termview, cols, rows, EINA_TRUE);

   termview_cell_size_get(gui->termview, &cell_w, &cell_h);
   evas_object_resize(gui->win, (int)(cols * cell_w),
                      (int)(rows * cell_h));
}

void
gui_clear(s_gui *gui)
{
   termview_clear(gui->termview);
}

void
gui_eol_clear(s_gui *gui)
{
   termview_eol_clear(gui->termview);
}

void
gui_put(s_gui *gui,
        const char *string,
        unsigned int size)
{
   termview_put(gui->termview, string, size);
}

void
gui_cursor_goto(s_gui *gui,
                unsigned int to_x,
                unsigned int to_y)
{
   termview_cursor_goto(gui->termview, to_x, to_y);
}


void
gui_style_set(s_gui *gui,
              const s_termview_style *style)
{
   termview_style_set(gui->termview, style);
}

void
gui_update_fg(s_gui *gui EINA_UNUSED,
              t_int color)
{
   if (color >= 0)
     {
        CRI("Unimplemented");
     }
}

void
gui_update_bg(s_gui *gui EINA_UNUSED,
              t_int color)
{
   if (color >= 0)
     {
        CRI("Unimplemented");
     }
}

void
gui_update_sp(s_gui *gui EINA_UNUSED,
              t_int color)
{
   if (color >= 0)
     {
        CRI("Unimplemented");
     }
}

void
gui_scroll_region_set(s_gui *gui,
                      int top,
                      int bot,
                      int left,
                      int right)
{
   const Eina_Rectangle region = {
      .x = left,
      .y = top,
      .w = right - left,
      .h = bot - top,
   };
   termview_scroll_region_set(gui->termview, &region);
}

void
gui_scroll(s_gui *gui,
           int scroll)
{
   if (scroll != 0)
     termview_scroll(gui->termview, (unsigned int)(abs(scroll)),
                     (scroll > 0) ? EINA_TRUE : EINA_FALSE);
}
