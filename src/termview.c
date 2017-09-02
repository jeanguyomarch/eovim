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

#define COL_DEFAULT_BG 0
#define COL_DEFAULT_FG 1

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

typedef struct termview s_termview;
struct termview
{
   Evas_Object *textgrid;
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


   /* Writing position */
   unsigned int x;
   unsigned int y;
};

static void
_textgrid_mouse_move_cb(void *data EINA_UNUSED,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   WRN("Mouse move");
}

static void
_termview_key_down_cb(void *data EINA_UNUSED,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   WRN("Key down");
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

   evas_object_smart_data_set(obj, sd);
   _parent_sc.add(obj);


   /* Font setup (FIXME: use a config) */
   sd->font_name = eina_stringshare_add("Mono");
   sd->font_size = 14;

   /* Textgrid setup */
   Evas *const evas = evas_object_evas_get(obj);
   Evas_Object *o;
   sd->textgrid = o = evas_object_textgrid_add(evas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_member_add(o, obj);

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE, _textgrid_mouse_move_cb, sd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_KEY_DOWN, _termview_key_down_cb, sd);
   evas_object_textgrid_cell_size_get(o, (int*)&sd->cell_w, (int*)&sd->cell_h);

   sd->palettes = eina_hash_int32_new(NULL);
   if (EINA_UNLIKELY(! sd->palettes))
     {
        CRI("Failed to create hash for color palettes");
        return;
     }

   /* Palette item #0 is the default BG */
   evas_object_textgrid_palette_set(
      o, EVAS_TEXTGRID_PALETTE_EXTENDED,
      COL_DEFAULT_FG, 255, 255, 255, 255
   );
   sd->palette_id_generator = 2; /* BG + FG */

   evas_object_show(sd->textgrid);
}

static void
_smart_del(Evas_Object *obj)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   evas_object_del(sd->textgrid);
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
}

static void
_smart_move(Evas_Object *obj,
            Evas_Coord x EINA_UNUSED,
            Evas_Coord y EINA_UNUSED)
{
   evas_object_smart_changed(obj);
}

static void
_smart_calculate(Evas_Object *obj EINA_UNUSED)
{
   //s_termview *const sd = evas_object_smart_data_get(obj);
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

Evas_Object *
termview_add(Evas_Object *parent)
{
   Evas *const e  = evas_object_evas_get(parent);
   Evas_Object *const obj = evas_object_smart_add(e, _smart);

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
                unsigned int rows)
{
   s_termview *const sd = evas_object_smart_data_get(obj);

   evas_object_textgrid_size_set(sd->textgrid, (int)cols, (int)rows);
   sd->cols = cols;
   sd->rows = rows;
}

void
termview_clear(Evas_Object *obj)
{
   s_termview *const sd = evas_object_smart_data_get(obj);

   /*
    * Go through each line (row) in the textgrid, and reset all the cells.
    * Memset() is an efficient way to do that as it will reset both the
    * codepoint and the attributes.
    */
   for (unsigned int y = 0; y < sd->rows; y++)
     {
        Evas_Textgrid_Cell *const cells = evas_object_textgrid_cellrow_get(
           sd->textgrid, (int)y
        );
        memset(cells, 0, sizeof(Evas_Textgrid_Cell) * sd->cols);
     }

   /* Reset the writing position to (0,0) */
   sd->x = 0;
   sd->y = 0;
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

   sd->x = to_x;
   sd->y = to_y;
}



static s_termview_color
_make_color(uint64_t col)
{
   const s_termview_color color = {
      .r = (uint8_t)(col & 0xff),
      .g = (uint8_t)(col & 0xff00) >> 8,
      .b = (uint8_t)(col & 0xff0000) >> 16,
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
        const s_termview_color col = _make_color((uint64_t)color);
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

