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

#include "eovim/termview.h"
#include "eovim/log.h"
#include "eovim/main.h"
#include "eovim/keymap.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim.h"

#include <Edje.h>

#define COL_DEFAULT_BG 0
#define COL_DEFAULT_FG 1
#define COL_REVERSE_FG 2

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

typedef struct
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
      Eina_Bool reverse;
      Eina_Bool italic;
      Eina_Bool bold;
      Eina_Bool underline;
   } current; /**< Current style */

   Eina_Rectangle scroll; /**< Scrolling region */

   /* When mouse drag starts, we store in here the button that was pressed when
    * dragging was initiated. Since there is no button 0, we use 0 as a value
    * telling that there is no dragging */
   int mouse_drag; /**< Doing mouse drag */

   /* Writing position */
   unsigned int x; /**< Cursor X */
   unsigned int y; /**< Cursor Y */
} s_termview;

#include "termcolors.x"

static void
_coords_to_cell(const s_termview *sd,
                int px, int py,
                unsigned int *cell_x, unsigned int *cell_y)
{
   int ox, oy; /* Textgrid origin */
   int ow, oh; /* Textgrid size */

   evas_object_geometry_get(sd->textgrid, &ox, &oy, &ow, &oh);

   /* Clamp cell_x in [0 ; cols[ */
   if (px < ox) { *cell_x = 0; }
   else if (px - ox >= ow) { *cell_x = sd->cols - 1; }
   else { *cell_x = (unsigned int)((px - ox) / (int)sd->cell_w); }

   /* Clamp cell_y in [0 ; rows[ */
   if (py < oy) { *cell_y = 0; }
   else if (py - oy >= oh) { *cell_y = sd->rows - 1; }
   else { *cell_y = (unsigned int)((py - oy) / (int)sd->cell_h); }
}

static const char *
_mouse_button_to_string(int button)
{
   switch (button)
     {
      case 3:
         return "Right";
      case 2:
         return "Middle";
      case 1: /* Fall through */
      default:
        return "Left";
     }
}

static void
_mouse_event(s_termview *sd, const char *event,
             unsigned int cx, unsigned int cy,
             int btn)
{
   char input[64];

   /* If mouse is NOT enabled, we don't handle mouse events */
   if (! nvim_mouse_enabled_get(sd->nvim)) { return; }

   /* Determine which button we pressed */
   const char *const button = _mouse_button_to_string(btn);

   /* Convert the mouse input as an input format. */
   const int bytes = snprintf(input, sizeof(input),
                              "<%s%s><%u,%u>", button, event, cx, cy);

   nvim_api_input(sd->nvim, input, (unsigned int)bytes);
}

static void
_textgrid_mouse_move_cb(void *data,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event)
{
   s_termview *const sd = data;

   /* If there is no mouse drag, nothing to do! */
   if (! sd->mouse_drag) { return; }

   const Evas_Event_Mouse_Move *const ev = event;
   unsigned int cx, cy;

   _coords_to_cell(sd, ev->cur.canvas.x, ev->cur.canvas.y, &cx, &cy);
   _mouse_event(sd, "Drag", cx, cy, sd->mouse_drag);
}

static void
_textgrid_mouse_up_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event)
{
   s_termview *const sd = data;
   const Evas_Event_Mouse_Up *const ev = event;
   unsigned int cx, cy;

   _coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
   _mouse_event(sd, "Release", cx, cy, ev->button);
   sd->mouse_drag = 0; /* Disable mouse dragging */
}

static void
_textgrid_mouse_down_cb(void *data,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event)
{
   s_termview *const sd = data;
   const Evas_Event_Mouse_Down *const ev = event;
   unsigned int cx, cy;

   _coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
   _mouse_event(sd, "Mouse", cx, cy, ev->button);
   sd->mouse_drag = ev->button; /* Enable mouse dragging */
}

static void
_textgrid_mouse_wheel_cb(void *data,
                         Evas *e EINA_UNUSED,
                         Evas_Object *obj EINA_UNUSED,
                         void *event)
{
   s_termview *const sd = data;
   const Evas_Event_Mouse_Wheel *const ev = event;

   /* If mouse is NOT enabled, we don't handle mouse events */
   if (! nvim_mouse_enabled_get(sd->nvim)) { return; }

   const char *const dir = (ev->z < 0) ? "Up" : "Down";

   char input[64];
   unsigned int cx, cy;
   _coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
   const int bytes = snprintf(input, sizeof(input),
                              "<ScrollWheel%s><%u,%u>", dir, cx, cy);
   nvim_api_input(sd->nvim, input, (unsigned int)bytes);
}

static Eina_Bool
_paste_cb(void *data,
          Evas_Object *obj EINA_UNUSED,
          Elm_Selection_Data *ev)
{
   EINA_SAFETY_ON_FALSE_RETURN_VAL(ev->format == ELM_SEL_FORMAT_TEXT, EINA_FALSE);

   s_termview *const sd = data;
   const char *const string = ev->data;
   Eina_Bool ret = EINA_TRUE;
   unsigned int escaped_len = 0;

   for (unsigned int i = 0; i < ev->len; i++)
     {
        /* We know that if we type '<' we must escape it as "<lt>", so we will
         * hav to write 4 characters? Otherwise, there is no escaping, we will
         * write only one character */
        escaped_len += (string[i] == '<') ? 4 : 1;
     }

   /* We can only pass UINT_MAX bytes to neovim, so if the input is greater
    * than that (lol), we will truncate the paste... such a shame...
    */
   if (EINA_UNLIKELY(escaped_len > UINT_MAX))
     {
        ERR("Integer overflow. Pasting will be truncated.");
        ret = EINA_FALSE; /* We will not copy everything */
     }

   if (escaped_len != ev->len)
     {
        /* If we have some escaping to do, we have to make copies of the input
         * data, as we need to modify it. That's not great... but we how it
         * goes. */
        char *const escaped = malloc(escaped_len + 1);
        if (EINA_UNLIKELY(! escaped))
          {
             CRI("Failed to allocate memory");
             return EINA_FALSE;
          }
        char *ptr = escaped;
         for (unsigned int i = 0; i < ev->len; i++)
           {
             if (string[i] == '<')
               {
                  strcpy(ptr, "<lt>");
                  ptr += 4;
               }
             else *(ptr++) = string[i];
           }
         escaped[escaped_len] = '\0';

         /* Send the data, then free the temporayy storage */
        nvim_api_input(sd->nvim, escaped, escaped_len);
        free(escaped);
     }
   else
     {
        /* Cool! No escaping to do! Send the pasted data as they are. */
        nvim_api_input(sd->nvim, string, (unsigned int)ev->len);
     }

   return ret;
}

static void
_termview_key_down_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event)
{
   s_termview *const sd = data;
   const Evas_Event_Key_Down *const ev = event;
   const Evas_Modifier *const mod = ev->modifiers;
   const char *send = ev->string;
   unsigned int send_size;

   const Eina_Bool ctrl = evas_key_modifier_is_set(mod, "Control");
   const Eina_Bool shift = evas_key_modifier_is_set(mod, "Shift");

   if (ctrl && shift)
     {
        /* When we hit CTRL+SHIFT, we will handle single character commands,
         * such as CTRL+SHIFT+C and CTRL+SHIFT+V.
         * To avoid string comparison, we match the first character only.
         * However, some key names have multiple-character strings (Backspace).
         * To avoid matching against them, we discard these
         */
        const char *const key = ev->key;
        if ((! key) || (key[1] != '\0')) { return; /* Ignore */ }

        switch (key[0])
        {
         case 'V':
            elm_cnp_selection_get(sd->textgrid,
                                  ELM_SEL_TYPE_CLIPBOARD, ELM_SEL_FORMAT_TEXT,
                                  _paste_cb, sd);
            break;

         default: /* Please the compiler! */
            break;
        }
        /* Stop right here. Do nothing more. */
        return;
     }

   /* If ev->string is not set, we will try to load  ev->key from the keymap */
   if (! send && ev->key)
     {
        send = keymap_get(ev->key);
        send_size = (unsigned int)eina_stringshare_strlen(send);
     }
   else
     send_size = (unsigned int)strlen(send);

   /* If a key is availabe pass it to neovim and update the ui */
   if (send)
     {
        /* Escape the less than character '<' */
        if ((send[0] == '<') && (send_size == 1))
          {
             send = "<lt>";
             send_size = 4;
          }
        nvim_api_input(sd->nvim, send, send_size);
        edje_object_signal_emit(sd->cursor, "key,down", "eovim");
     }
   else
     DBG("Unhandled key '%s'", ev->key);
}

static void
_termview_focus_in_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event EINA_UNUSED)
{
   s_termview *const sd = data;
   edje_object_signal_emit(sd->cursor, "focus,in", "eovim");
}

static void
_termview_focus_out_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event EINA_UNUSED)
{
   s_termview *const sd = data;
   edje_object_signal_emit(sd->cursor, "focus,out", "eovim");
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
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN, _textgrid_mouse_down_cb, sd);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP, _textgrid_mouse_up_cb, sd);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL, _textgrid_mouse_wheel_cb, sd);
   evas_object_textgrid_cell_size_get(o, (int*)&sd->cell_w, (int*)&sd->cell_h);
   evas_object_show(o);

   /* Cursor setup */
   sd->cursor = o = edje_object_add(evas);
   edje_object_file_set(o, main_edje_file_get(), "eovim/cursor");
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

   evas_object_textgrid_palette_set(
      sd->textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED,
      COL_REVERSE_FG, 0, 0, 0, 255
   );

   /* Set a default foreground color */
   termview_fg_color_set(obj, 255, 215, 175, 255);

   /* Palette item #0 is the default BG */
   sd->palette_id_generator = 3; /* BG + FG + REV_FG */
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

   const unsigned int cols = (unsigned int)w / sd->cell_w;
   const unsigned int rows = (unsigned int)h / sd->cell_h;

   if (EINA_LIKELY((cols > 0) && (rows > 0)))
     termview_resize(obj, cols, rows, EINA_FALSE);
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
termview_refresh(Evas_Object *obj)
{
   s_termview *const sd = evas_object_smart_data_get(obj);
   evas_object_textgrid_update_add(sd->textgrid, 0, 0, (int)sd->cols, (int)sd->rows);
}

void
termview_font_set(Evas_Object *obj,
                  const char *font_name,
                  unsigned int font_size)
{
   EINA_SAFETY_ON_NULL_RETURN(font_name);
   EINA_SAFETY_ON_TRUE_RETURN(font_size == 0);

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

   /* When we resize the termview, we have reset the scrolling region to the
    * whole termview. */
   const Eina_Rectangle region = {
      .x = 0,
      .y = 0,
      .w = (int)cols - 1,
      .h = (int)rows - 1,
   };
   termview_scroll_region_set(obj, &region);

   /*
    * If request is TRUE, it means that the resizing request comes from neovim
    * itself. It would make no sense to tell back neovim we want to resize the
    * ui to the value it gave us.
    * When this value is FALSE, it is a request from the window manager (the
    * user manually resized a window). In this case, we tell neovim its display
    * space has changed
    */
   if (! request)
     nvim_api_ui_try_resize(sd->nvim, cols, rows);
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
             const Eina_Unicode *ustring,
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
        c->codepoint = ustring[x];
        if (sd->current.reverse)
          {
             if (sd->current.bg == COL_DEFAULT_BG)
               c->fg = COL_REVERSE_FG;
             else
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
   termview_cursor_goto(obj, sd->x + size, sd->y);
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



s_termview_color
termview_color_decompose(uint32_t col,
                         Eina_Bool true_colors)
{
   if (true_colors)
     {
        /*
         * When true colors are requested, we have to decompose
         * a 24-bits color. Alpha is always maximal (full opacity).
         */
        const s_termview_color color = {
           .r = (uint8_t)((col & 0x00ff0000) >> 16),
           .g = (uint8_t)((col & 0x0000ff00) >> 8),
           .b = (uint8_t)((col & 0x000000ff) >> 0),
           .a = 0xff,
        };
        return color;
     }
   else
     {
        /*
         * When terminal colors are requested, we must check there is no
         * overflow ([0;255]), then we access the true colors used to paint
         * the terminal color. See 'src/termcolors.x'.
         */
        if (EINA_UNLIKELY(col >= 256))
          {
             ERR("Color %u is not a terminal color", col);
             col = 255;
          }
        return _termcolors[col];
     }
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
                         Eina_Bool fg)
{
   if (color < 0)
     {
        return (fg) ? COL_DEFAULT_FG : COL_DEFAULT_BG;
     }
   else
     {
        const s_termview_color col = termview_color_decompose((uint32_t)color,
                                                              sd->nvim->true_colors);
        return _make_palette(sd, col);
     }
}

void
termview_style_set(Evas_Object *obj,
                   const s_termview_style *style)
{
   s_termview *const sd = evas_object_smart_data_get(obj);

   sd->current.fg = _make_palette_from_color(sd, style->fg_color, EINA_TRUE);
   sd->current.bg = _make_palette_from_color(sd, style->bg_color, EINA_FALSE);
   sd->current.reverse = style->reverse;
   sd->current.italic = style->italic;
   sd->current.bold = style->bold;
   sd->current.underline = style->underline || style->undercurl;

   static Eina_Bool show_warning = EINA_TRUE;
   if (EINA_UNLIKELY(style->undercurl && show_warning))
     {
        WRN("Undercurl was requested but is not supportd.");
        show_warning = EINA_FALSE;
     }
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
   const size_t width = sizeof(Evas_Textgrid_Cell) * ((unsigned)sd->scroll.w + 1);
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

void
termview_cell_to_coords(const Evas_Object *obj,
                        unsigned int cell_x,
                        unsigned int cell_y,
                        int *px,
                        int *py)
{
   const s_termview *const sd = evas_object_smart_data_get(obj);

   int wx, wy;
   evas_object_geometry_get(sd->textgrid, &wx, &wy, NULL, NULL);

   if (px) *px = (int)(cell_x * sd->cell_w) + wx;
   if (py) *py = (int)(cell_y * sd->cell_h) + wy;
}
