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

#include "envim/termview.h"
#include "envim/log.h"
#include "envim/main.h"
#include "envim/keymap.h"
#include "nvim_api.h"

#include <Edje.h>

#define COL_DEFAULT_BG 0
#define COL_DEFAULT_FG 1

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

typedef struct termview s_termview;
struct termview
{
   Evas_Object_Smart_Clipped_Data __clipped_data; /* Required by Evas */

   s_nvim *nvim;
   Evas_Object *textgrid;
   Evas_Object *cursor;
   Eina_Hash *palettes;

   Eina_Stringshare *font_name;
   unsigned int font_size;
   unsigned int cell_w;
   unsigned int cell_h;
   unsigned int rows;
   unsigned int cols;

   unsigned int palette_id_generator;

   struct {
      uint8_t fg;
      uint8_t bg;
      uint8_t sp;
      Eina_Bool reverse;
      Eina_Bool italic;
      Eina_Bool bold;
      Eina_Bool underline;
      Eina_Bool undercurl;
   } current;

   Eina_Rectangle scroll;

   /* Writing position */
   unsigned int x;
   unsigned int y;
};

static void
_textgrid_mouse_move_cb(void *data EINA_UNUSED,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event EINA_UNUSED)
{
}

static void
_input_keys_cb(s_nvim *nvim EINA_UNUSED,
               t_int keys,
               void *data EINA_UNUSED)
{
   INF("%"PRIu64" keys written", keys);
}

static void
_termview_key_down_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event)
{
   s_termview *const sd = data;
   const Evas_Event_Key_Down *const ev = event;
   const char *send = ev->string;

   /* If ev->string is not set, we will try to load  ev->key from the keymap */
   if (! send && ev->key)
     send = keymap_get(ev->key);

   /* If a key is availab,e pass it to neovim and update the ui */
   if (send)
     {
        nvim_input(sd->nvim, send, _input_keys_cb, NULL, NULL);
        edje_object_signal_emit(sd->cursor, "key,down", "envim");
     }
   else
     ERR("Unhandled key '%s'", ev->key);
}

static void
_termview_focus_in_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event EINA_UNUSED)
{
   s_termview *const sd = data;
   edje_object_signal_emit(sd->cursor, "focus,in", "envim");
}

static void
_termview_focus_out_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event EINA_UNUSED)
{
   s_termview *const sd = data;
   edje_object_signal_emit(sd->cursor, "focus,out", "envim");
}


static void
_smart_add(Evas_Object *obj)
{
   s_termview *const sd = calloc(1, sizeof(s_termview));
   if (EINA_UNLIKELY(! sd))
     {
        CRI("Failed to allocate termview structure");
        return;
     }

   /* Create the smart object */
   evas_object_smart_data_set(obj, sd);
   _parent_sc.add(obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_DOWN, _termview_key_down_cb, sd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_IN, _termview_focus_in_cb, sd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_OUT, _termview_focus_out_cb, sd);

   Evas *const evas = evas_object_evas_get(obj);
   Evas_Object *o;

   /* Textgrid setup */
   sd->textgrid = o = evas_object_textgrid_add(evas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_member_add(o, obj);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE, _textgrid_mouse_move_cb, sd);
   evas_object_textgrid_cell_size_get(o, (int*)&sd->cell_w, (int*)&sd->cell_h);
   evas_object_show(o);

   /* Cursor setup */
   sd->cursor = o = edje_object_add(evas);
   edje_object_file_set(o, main_edje_file_get(), "envim/cursor");
   evas_object_pass_events_set(o, EINA_TRUE);
   evas_object_propagate_events_set(o, EINA_FALSE);
   evas_object_smart_member_add(o, obj);
   evas_object_show(o);

   /* Creation of the palette items cache */
   sd->palettes = eina_hash_int32_new(NULL);
   if (EINA_UNLIKELY(! sd->palettes))
     {
        CRI("Failed to create hash for color palettes");
        return;
     }

   termview_fg_color_set(obj, 255, 255, 255, 255);

   /* Palette item #0 is the default BG */
   sd->palette_id_generator = 2; /* BG + FG */

}

static void
_smart_del(Evas_Object *obj)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   evas_object_del(sd->textgrid);
   evas_object_del(sd->cursor);
   eina_hash_free(sd->palettes);
   eina_stringshare_del(sd->font_name);
   evas_object_event_callback_del(sd->textgrid, EVAS_CALLBACK_MOUSE_MOVE,
                                  _textgrid_mouse_move_cb);
   evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
                                  _termview_key_down_cb);
}

static void
_smart_resize(Evas_Object *obj,
              Evas_Coord w,
              Evas_Coord h)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   evas_object_resize(sd->textgrid, w, h);
   evas_object_smart_changed(obj);

   termview_resize(obj,
                  (unsigned int)w / sd->cell_w,
                  (unsigned int)h / sd->cell_h, EINA_FALSE);
}

static void
_smart_move(Evas_Object *obj,
            Evas_Coord x EINA_UNUSED,
            Evas_Coord y EINA_UNUSED)
{
   evas_object_smart_changed(obj);
}

static void
_smart_calculate(Evas_Object *obj)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);

   /* Place the textgrid */
   evas_object_move(sd->textgrid, ox, oy);
   evas_object_resize(sd->textgrid,
                      (int)(sd->cols * sd->cell_w),
                      (int)(sd->rows * sd->cell_h));

   /* Place the cursor */
   evas_object_move(sd->cursor,
                    ox + (int)(sd->x * sd->cell_w),
                    oy + (int)(sd->y * sd->cell_h));
   evas_object_resize(sd->cursor,
                      (int)sd->cell_w,
                      (int)sd->cell_h);
}

Eina_Bool
termview_init(void)
{
   static Evas_Smart_Class sc;

   evas_object_smart_clipped_smart_set(&_parent_sc);
   sc           = _parent_sc;
   sc.name      = "termview";
   sc.version   = EVAS_SMART_CLASS_VERSION;
   sc.add       = _smart_add;
   sc.del       = _smart_del;
   sc.resize    = _smart_resize;
   sc.move      = _smart_move;
   sc.calculate = _smart_calculate;
   _smart = evas_smart_class_new(&sc);

   return EINA_TRUE;
}

void
termview_shutdown(void)
{}

static void
_termview_nvim_set(Evas_Object *obj,
                  s_nvim *nvim)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   sd->nvim = nvim;
}

Evas_Object *
termview_add(Evas_Object *parent,
             s_nvim *nvim)
{
   Evas *const e  = evas_object_evas_get(parent);
   Evas_Object *const obj = evas_object_smart_add(e, _smart);
   _termview_nvim_set(obj, nvim);

   return obj;
}

void
termview_font_set(Evas_Object *obj,
                  const char *font_name,
                  unsigned int font_size)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   evas_object_textgrid_font_set(sd->textgrid, font_name, (int)font_size);
   evas_object_textgrid_cell_size_get(sd->textgrid,
                                      (int*)&sd->cell_w, (int*)&sd->cell_h);
}

void
termview_cell_size_get(const Evas_Object *obj,
                       unsigned int *w,
                       unsigned int *h)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   if (w) *w = sd->cell_w;
   if (h) *h = sd->cell_h;
}

void
termview_size_get(const Evas_Object *obj,
                  unsigned int *cols,
                  unsigned int *rows)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   if (cols) *cols = sd->cols;
   if (rows) *rows = sd->rows;
}

void
termview_resize(Evas_Object *obj,
                unsigned int cols,
                unsigned int rows,
                Eina_Bool request)
{
   s_termview *const sd = evas_object_smart_data_get(obj);

   evas_object_textgrid_size_set(sd->textgrid, (int)cols, (int)rows);
   sd->cols = cols;
   sd->rows = rows;

   /*
    * If request is TRUE, it means that the resizing request comes from neovim
    * itself. It would make no sense to tell back neovim we want to resize the
    * ui to the value it gave us.
    * When this value is FALSE, it is a request from the window manager (the
    * user manually resized a window). In this case, we tell neovim its display
    * space has changed
    */
   if (! request)
     nvim_ui_try_resize(sd->nvim, cols, rows, NULL, NULL, NULL);
}

void
termview_clear(Evas_Object *obj)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   Evas_Object *const grid = sd->textgrid;

   /*
    * Go through each line (row) in the textgrid, and reset all the cells.
    * Memset() is an efficient way to do that as it will reset both the
    * codepoint and the attributes.
    */
   for (unsigned int y = 0; y < sd->rows; y++)
     {
        Evas_Textgrid_Cell *const cells = evas_object_textgrid_cellrow_get(
           grid, (int)y
        );
        memset(cells, 0, sizeof(Evas_Textgrid_Cell) * sd->cols);
        evas_object_textgrid_cellrow_set(grid, (int)y, cells);
     }
   evas_object_textgrid_update_add(grid, 0, 0, (int)sd->cols, (int)sd->rows);

   /* Reset the writing position to (0,0) */
   sd->x = 0;
   sd->y = 0;
}

void
termview_eol_clear(Evas_Object *obj)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   Evas_Object *const grid = sd->textgrid;

   /*
    * Remove all characters from the cursor until the end of the textgrid line
    */
   Evas_Textgrid_Cell *const cells = evas_object_textgrid_cellrow_get(
      grid, (int)sd->y
   );
   memset(&cells[sd->x], 0, sizeof(Evas_Textgrid_Cell) * (sd->cols - sd->x));
   evas_object_textgrid_cellrow_set(grid, (int)sd->y, cells);
   evas_object_textgrid_update_add(grid, (int)sd->x, (int)sd->y,
                                   (int)(sd->cols - sd->x), 1);
}

void
termview_put(Evas_Object *obj,
             const char *string,
             unsigned int size)
{
   s_termview *const sd = evas_object_smart_data_get(obj);

   Evas_Textgrid_Cell *const cells = evas_object_textgrid_cellrow_get(
      sd->textgrid, (int)sd->y
   );

   if (EINA_UNLIKELY(sd->x + size > sd->cols))
     {
        ERR("String would overflow textgrid. Truncating.");
        size = sd->cols - sd->x;
     }

   for (unsigned int x = 0; x < size; x++)
     {
        Evas_Textgrid_Cell *const c = &(cells[x + sd->x]);
        c->codepoint = string[x];
        if (sd->current.reverse)
          {
             c->fg = sd->current.bg;
             c->bg = sd->current.fg;
          }
        else
          {
             c->fg = sd->current.fg;
             c->bg = sd->current.bg;
          }
        c->bold = !!sd->current.bold;
        c->italic = !!sd->current.italic;
        c->underline = !!sd->current.underline;
        c->bg_extended = 1;
        c->fg_extended = 1;
     }
   evas_object_textgrid_cellrow_set(sd->textgrid, (int)sd->y, cells);
   evas_object_textgrid_update_add(sd->textgrid,
                                   (int)sd->x, (int)sd->y, (int)size, 1);
   sd->x += size;
}

void
termview_cursor_goto(Evas_Object *obj,
                     unsigned int to_x,
                     unsigned int to_y)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   if (EINA_UNLIKELY(to_y > sd->rows))
     {
        ERR("Attempt to move cursor outside of known height.");
        to_y = sd->rows;
     }
   if (EINA_UNLIKELY(to_x > sd->cols))
     {
        ERR("Attempt to move cursor outside of known width.");
        to_x = sd->cols;
     }

   Evas_Coord x, y;
   evas_object_geometry_get(sd->textgrid, &x, &y, NULL, NULL);
   evas_object_move(sd->cursor,
                    x + (int)(to_x * sd->cell_w),
                    y + (int)(to_y * sd->cell_h));

   sd->x = to_x;
   sd->y = to_y;
}



static s_termview_color
_make_color(uint32_t col)
{
   const s_termview_color color = {
      .r = (uint8_t)((col & 0x00ff0000) >> 16),
      .g = (uint8_t)((col & 0x0000ff00) >> 8),
      .b = (uint8_t)((col & 0x000000ff) >> 0),
      .a = 0xff,
   };
   return color;
}

static uint8_t
_make_palette(s_termview *sd,
              s_termview_color color)
{
   if (color.a == 0x00) { return 0; }

   const uint8_t *const id_ptr = eina_hash_find(sd->palettes, &color);
   if (EINA_UNLIKELY(! id_ptr))
     {
        const unsigned int id = sd->palette_id_generator++;
        if (EINA_UNLIKELY(id > UINT8_MAX)) /* we have 256 colors max */
          {
             CRI("Palettes overflow. Need to implement a backup!");
             return 0;
          }

        evas_object_textgrid_palette_set(
           sd->textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED,
           (int)id,
           color.r, color.g, color.b, color.a
        );

        eina_hash_add(sd->palettes, &color, (void *)(uintptr_t)id);
        return (uint8_t)id;
     }
   return (uint8_t)(uintptr_t)(id_ptr);
}

static uint8_t
_make_palette_from_color(s_termview *sd,
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
        const s_termview_color col = _make_color((uint32_t)color);
        return _make_palette(sd, col);
     }
}

void
termview_style_set(Evas_Object *obj,
                   const s_termview_style *style)
{
   s_termview *const sd = evas_object_smart_data_get(obj);

   sd->current.fg = _make_palette_from_color(
      sd, style->fg_color, EINA_TRUE, style->reverse
   );
   sd->current.bg = _make_palette_from_color(
      sd, style->bg_color, EINA_FALSE, style->reverse
   );
   sd->current.sp = _make_palette_from_color(
      sd, style->sp_color, EINA_TRUE, style->reverse
   );
   sd->current.reverse = style->reverse;
   sd->current.italic = style->italic;
   sd->current.bold = style->bold;
   sd->current.underline = style->underline;
   sd->current.undercurl = style->undercurl;
}

void
termview_scroll_region_set(Evas_Object *obj,
                           const Eina_Rectangle *region)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   memcpy(&sd->scroll, region, sizeof(Eina_Rectangle));
}

void
termview_scroll(Evas_Object *obj,
                int count)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   Evas_Object *const grid = sd->textgrid;
   const size_t width = sizeof(Evas_Textgrid_Cell) * (unsigned)sd->scroll.w + 1;
   const int end_of_scroll = sd->scroll.y + sd->scroll.h;
   Evas_Textgrid_Cell *src, *dst, *tmp;

   if (count > 0) /* Scroll text upwards */
     {
        /*
         * We are scrolling text upwards. Line N will be overwriten by line
         * N+(count). The last lines are cleared.
         *
         * +------------------+     +------------------+
         * | Line 0           |  ,> | Line 1           |
         * +------------------+ /   +------------------+
         * | Line 1           |' ,> | Line 2           |
         * +------------------+ /   +------------------+
         * | Line 2           |'    | xxxxxx           |
         * +------------------+     +------------------+
         *
         */

        for (int i = sd->scroll.y + count; i <= end_of_scroll; i++)
          {
             const int j = i - count; /* destination */

             dst = evas_object_textgrid_cellrow_get(grid, j);
             src = evas_object_textgrid_cellrow_get(grid, i);

             memcpy(&dst[sd->scroll.x], &src[sd->scroll.x], width);
             evas_object_textgrid_cellrow_set(grid, j, dst);
          }
        /* Clear the lines left after the scrolling */
        for (int i = end_of_scroll; i > end_of_scroll - count; i--)
          {
             tmp = evas_object_textgrid_cellrow_get(grid, i);
             memset(&tmp[sd->scroll.x], 0, width);
             evas_object_textgrid_cellrow_set(grid, i, tmp);
          }
     }
   else /* Scroll text downwards. count is NEGATIVE!!! */
     {
        /*
         * We are scrolling text downwards. Line N+(count) will be overwriten
         * by line N. The first lines are cleared.
         *
         * +------------------+     +------------------+
         * | Line 0           |,    | xxxxxx           |
         * +------------------+ \   +------------------+
         * | Line 1           |, '> | Line 0           |
         * +------------------+ \   +------------------+
         * | Line 2           |  '> | Line 1           |
         * +------------------+     +------------------+
         *
         */

        for (int i = end_of_scroll; i >= sd->scroll.y - count; i--)
          {
             const int j = i + count; /* destination */

             dst = evas_object_textgrid_cellrow_get(grid, i);
             src = evas_object_textgrid_cellrow_get(grid, j);

             memcpy(&dst[sd->scroll.x], &src[sd->scroll.x], width);
             evas_object_textgrid_cellrow_set(grid, j, dst);
          }
        /* Clear the lines left after the scrolling */
        for (int i = sd->scroll.y; i < sd->scroll.y - count; i++)
          {
             tmp = evas_object_textgrid_cellrow_get(grid, i);
             memset(&tmp[sd->scroll.x], 0, width);
             evas_object_textgrid_cellrow_set(grid, i, tmp);
          }
     }

   /* Finally, mark the update */
   evas_object_textgrid_update_add(grid, sd->scroll.x, sd->scroll.y,
                                   sd->scroll.w + 1, sd->scroll.h + 1);
}

void
termview_fg_color_set(Evas_Object *obj,
                      int r, int g, int b, int a)
{
   s_termview *const sd = evas_object_smart_data_get(obj);

   /* Set the new value of the default foreground text. */
   evas_color_argb_premul(a, &r, &g, &b);
   evas_object_textgrid_palette_set(
      sd->textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED,
      COL_DEFAULT_FG, r, g, b, a
   );

   /* Update the whole textgrid to reflect the change */
   evas_object_textgrid_update_add(sd->textgrid, 0, 0,
                                   (int)sd->cols, (int)sd->rows);
}
