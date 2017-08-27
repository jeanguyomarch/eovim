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

#define COL_DEFAULT_BG 0
#define COL_DEFAULT_FG 1

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

   gui->palettes = eina_hash_int32_new(NULL);
   if (EINA_UNLIKELY(! gui->palettes))
     {
        CRI("Failed to create hash for color palettes");
        goto fail;
     }


   evas_object_textgrid_palette_set(
      o, EVAS_TEXTGRID_PALETTE_EXTENDED,
      COL_DEFAULT_BG, 255, 255, 255, 0
   );
   evas_object_textgrid_palette_set(
      o, EVAS_TEXTGRID_PALETTE_EXTENDED,
      COL_DEFAULT_FG, 255, 255, 255, 255
   );
   gui->palette_id_generator = 2;

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
        eina_hash_free(gui->palettes);
     }
}

void
gui_resize(s_gui *gui,
           unsigned int cols,
           unsigned int rows)
{
   /* Don't resize if not needed */
   if ((gui->cols == cols) && (gui->rows == rows)) { return; }

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

   gui->cols = cols;
   gui->rows = rows;
}

void
gui_clear(s_gui *gui)
{
   /*
    * Go through each line (row) in the textgrid, and reset all the cells.
    * Memset() is an efficient way to do that as it will reset both the
    * codepoint and the attributes.
    */
   for (unsigned int y = 0; y < gui->rows; y++)
     {
        Evas_Textgrid_Cell *const cells = evas_object_textgrid_cellrow_get(
           gui->textgrid, (int)y
        );
        memset(cells, 0, sizeof(Evas_Textgrid_Cell) * gui->cols);
     }

   /* Reset the writing position to (0,0) */
   gui->x = 0;
   gui->y = 0;
}

void
gui_put(s_gui *gui,
        const char *string,
        unsigned int size)
{
   Evas_Textgrid_Cell *const cells = evas_object_textgrid_cellrow_get(
      gui->textgrid, (int)gui->y
   );

   if (EINA_UNLIKELY(gui->x + size > gui->cols))
     {
        ERR("String would overflow textgrid. Truncating.");
        size = gui->cols - gui->x;
     }

   for (unsigned int x = 0; x < size; x++)
     {
        Evas_Textgrid_Cell *const c = &(cells[x + gui->x]);
        c->codepoint = string[x];
        if (gui->current.reverse)
          {
             c->fg = gui->current.bg;
             c->bg = gui->current.fg;
          }
        else
          {
             c->fg = gui->current.fg;
             c->bg = gui->current.bg;
          }
        c->bold = !!gui->current.bold;
        c->italic = !!gui->current.italic;
        c->underline = !!gui->current.underline;
        c->bg_extended = 1;
        c->fg_extended = 1;
     }
   gui->x += size;
}

void
gui_cursor_goto(s_gui *gui,
                unsigned int to_x,
                unsigned int to_y)
{
   if (EINA_UNLIKELY(to_y > gui->rows))
     {
        ERR("Attempt to move cursor outside of known height.");
        to_y = gui->rows;
     }
   if (EINA_UNLIKELY(to_x > gui->cols))
     {
        ERR("Attempt to move cursor outside of known width.");
        to_x = gui->cols;
     }

   gui->x = to_x;
   gui->y = to_y;
}

static s_gui_color
_make_color(uint64_t col)
{
   const s_gui_color color = {
      .r = (uint8_t)(col & 0xff),
      .g = (uint8_t)(col & 0xff00) >> 8,
      .b = (uint8_t)(col & 0xff0000) >> 16,
      .a = 0xff,
   };
   return color;
}

static uint8_t
_make_palette(s_gui *gui,
              s_gui_color color)
{
   if (color.a == 0x00) { return 0; }

   const uint8_t *const id_ptr = eina_hash_find(gui->palettes, &color);
   if (EINA_UNLIKELY(! id_ptr))
     {
        const unsigned int id = gui->palette_id_generator++;
        if (EINA_UNLIKELY(id > UINT8_MAX)) /* we have 256 colors max */
          {
             CRI("Palettes overflow. Need to implement a backup!");
             return 0;
          }

        evas_object_textgrid_palette_set(
           gui->textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED,
           (int)id,
           color.r, color.g, color.b, color.a
        );

        eina_hash_add(gui->palettes, &color, (void *)(uintptr_t)id);
        return (uint8_t)id;
     }
   return (uint8_t)(uintptr_t)(id_ptr);
}

static uint8_t
_make_palette_from_color(s_gui *gui,
                         t_int color,
                         Eina_Bool fg,
                         Eina_Bool reverse)
{
   if (color < 0)
     {
        if (reverse)
          return (fg) ? COL_DEFAULT_BG : COL_DEFAULT_FG;
        else
          return (fg) ? COL_DEFAULT_FG : COL_DEFAULT_BG;
     }
   else
     {
        const s_gui_color col = _make_color((uint64_t)color);
        return _make_palette(gui, col);
     }
}

void
gui_style_set(s_gui *gui,
              const s_gui_style *style)
{
   gui->current.fg = _make_palette_from_color(
      gui, style->fg_color, EINA_TRUE, style->reverse
   );
   gui->current.bg = _make_palette_from_color(
      gui, style->bg_color, EINA_FALSE, style->reverse
   );
   gui->current.sp = _make_palette_from_color(
      gui, style->sp_color, EINA_TRUE, style->reverse
   );
   gui->current.reverse = style->reverse;
   gui->current.italic = style->italic;
   gui->current.bold = style->bold;
   gui->current.underline = style->underline;
   gui->current.undercurl = style->undercurl;
}
