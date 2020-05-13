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

#include "contrib/contrib.h"
#include "eovim/termview.h"
#include "eovim/log.h"
#include "eovim/gui.h"
#include "eovim/main.h"
#include "eovim/keymap.h"
#include "eovim/config.h"
#include "eovim/nvim_helper.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim.h"

#include <Edje.h>
#include <Ecore_Input.h>

enum
{
   COL_DEFAULT_BG,
   COL_DEFAULT_FG,
   COL_REVERSE_FG,
   COL_GENERATOR_START /* Should be the last element */
};

enum
{
   THEME_MSG_BLINK_SET = 0,
   THEME_MSG_COLOR_SET = 1,
   THEME_MSG_MAY_BLINK_SET = 2,
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

struct termview;
typedef void (*f_cursor_calc)(struct termview *sd, Evas_Coord x, Evas_Coord y);

struct termview
{
   Evas_Object_Smart_Clipped_Data __clipped_data; /* Required by Evas */

   s_nvim *nvim;
   Evas_Object *textgrid;
   Evas_Object *cursor;
   Eina_Hash *palettes;

   unsigned int cell_w;
   unsigned int cell_h;
   unsigned int rows;
   unsigned int cols;

   unsigned int palette_id_generator;

   /* The rows and columns that neovim uses to display its text. It is
    * important to keep them around, as this allows to arbitrate positions
    * upon resizing. For example, if I spawn neovim with a size 80x24, then
    * neovim will start to send me data for a 80x24 screen. Normal. But if
    * I resize while it is sending me this information (happens!), then
    * what neovim sends me is plain wrong, as the interface was updated.
    * That's were these two have a role to play! */
   unsigned int nvim_rows;
   unsigned int nvim_cols;

   struct {
      uint8_t fg;
      uint8_t bg;
      Eina_Bool reverse;
      Eina_Bool italic;
      Eina_Bool bold;
      Eina_Bool underline;
   } current; /**< Current style */

   Eina_Rectangle scroll; /**< Scrolling region */

   struct {
      /* When mouse drag starts, we store in here the button that was pressed
       * when dragging was initiated. Since there is no button 0, we use 0 as a
       * value telling that there is no dragging */
      int btn;
      unsigned int prev_cx; /**< Previous X position */
      unsigned int prev_cy; /**< Previous Y position */
   } mouse_drag;

   /* Writing position */
   unsigned int x; /**< Cursor X */
   unsigned int y; /**< Cursor Y */
   const s_mode *mode;
   f_cursor_calc cursor_calc;

   Eina_List *seq_compose;
};

static void
_keys_send(struct termview *sd,
           const char *keys,
           unsigned int size)
{
   const s_config *const config = sd->nvim->config;
   nvim_api_input(sd->nvim, keys, size);
   if (config->key_react)
     edje_object_signal_emit(sd->cursor, "key,down", "eovim");
}


static inline Eina_Bool
_composing_is(const struct termview *sd)
{
   /* Composition is pending if the seq_compose list is not empty */
   return (sd->seq_compose == NULL) ? EINA_FALSE : EINA_TRUE;
}

static inline void
_composition_reset(struct termview *sd)
{
   /* Discard all elements within the list */
   Eina_Stringshare *str;
   EINA_LIST_FREE(sd->seq_compose, str)
      eina_stringshare_del(str);
}

static inline void
_composition_add(struct termview *sd, const char *key)
{
   /* Add the key as a stringshare in the seq list. Hence, starting the
    * composition */
   Eina_Stringshare *const shr = eina_stringshare_add(key);
   sd->seq_compose = eina_list_append(sd->seq_compose, shr);
}

/*
 * This function returns EINA_TRUE when the key was handled within this
 * functional unit. When it returns EINA_FALSE, the caller should handle the
 * key itself.
 */
static Eina_Bool
_compose(struct termview *sd, const char *key)
{
  if (_composing_is(sd))
    {
       char *res = NULL;

       /* When composition is enabled, we want to skip modifiers, and only feed
        * non-modified keys to the composition engine */
       if (contrib_key_is_modifier(key))
         return EINA_TRUE;

       /* Add the current key to the composition list, and compute */
       _composition_add(sd, key);
       const Ecore_Compose_State state = ecore_compose_get(sd->seq_compose, &res);
       if (state == ECORE_COMPOSE_DONE)
         {
            /* We composed! Write the composed key! */
            _composition_reset(sd);
            if (EINA_LIKELY(NULL != res))
              {
                 _keys_send(sd, res, (unsigned int)strlen(res));
                 free(res);
                 return EINA_TRUE;
              }
         }
       else if (state == ECORE_COMPOSE_NONE)
         {
            /* The composition yield nothing. Reset */
            _composition_reset(sd);
         }
    }
  else /* Not composing yet */
    {
       /* Add the key to the composition engine */
       _composition_add(sd, key);
       const Ecore_Compose_State state = ecore_compose_get(sd->seq_compose, NULL);
       if (state != ECORE_COMPOSE_MIDDLE)
         {
            /* Nope, this does not allow composition */
            _composition_reset(sd);
         }
       else { return EINA_TRUE; } /* Composing.... */
    }

  /* Delegate the key to the caller */
  return EINA_FALSE;
}

static inline Eina_Bool
_unfinished_resizing_is(const struct termview *sd)
{
   /* If neovim cols/rows are different from the termview cols/rows, we are
    * resizing the UI, and we must avoid processing old neovim data */
   return ((sd->nvim_rows != sd->rows) || (sd->nvim_cols != sd->cols));
}

static void
_cursor_calc_block(struct termview *sd,
                   Evas_Coord x, Evas_Coord y)
{
   /* Place the cursor at (x,y) and set its size to a cell */
   evas_object_move(sd->cursor,
                    x + (int)(sd->x * sd->cell_w),
                    y + (int)(sd->y * sd->cell_h));
   evas_object_resize(sd->cursor,
                      (int)sd->cell_w,
                      (int)sd->cell_h);
}

static void
_cursor_calc_vertical(struct termview *sd,
                      Evas_Coord x, Evas_Coord y)
{
   /* Place the cursor at (x,y) and set its width to mode->cell_percentage
    * of a cell's width */
   evas_object_move(sd->cursor,
                    x + (int)(sd->x * sd->cell_w),
                    y + (int)(sd->y * sd->cell_h));

   unsigned int w = (sd->cell_w * sd->mode->cell_percentage) / 100u;
   if (w == 0) w = 1; /* We don't want an invisible cursor */
   evas_object_resize(sd->cursor, (int)w, (int)sd->cell_h);
}

static void
_cursor_calc_horizontal(struct termview *sd,
                        Evas_Coord x, Evas_Coord y)
{
   /* Place the cursor at the bottom of (x,y) and set its height to
    * mode->cell_percentage of a cell's height */

   unsigned int h = (sd->cell_h * sd->mode->cell_percentage) / 100u;
   if (h == 0) h = 1; /* We don't want an invisible cursor */

   evas_object_move(sd->cursor,
                    x + (int)(sd->x * sd->cell_w),
                    y + (int)((sd->y * sd->cell_h) + (sd->cell_h - h)));
   evas_object_resize(sd->cursor, (int)sd->cell_w, (int)h);
}


static void
_coords_to_cell(const struct termview *sd,
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
_mouse_event(struct termview *sd, const char *event,
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
_termview_mouse_move_cb(void *data,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event)
{
   struct termview *const sd = data;

   /* If there is no mouse drag, nothing to do! */
   if (! sd->mouse_drag.btn) { return; }

   const Evas_Event_Mouse_Move *const ev = event;
   unsigned int cx, cy;

   _coords_to_cell(sd, ev->cur.canvas.x, ev->cur.canvas.y, &cx, &cy);

   /* Did we move? If not, stop right here */
   if ((cx == sd->mouse_drag.prev_cx) && (cy == sd->mouse_drag.prev_cy))
      return;

   /* At this point, we have actually moved the mouse while holding a mouse
    * button, hence dragging. Send the event then update the current mouse
    * position. */
   _mouse_event(sd, "Drag", cx, cy, sd->mouse_drag.btn);

   sd->mouse_drag.prev_cx = cx;
   sd->mouse_drag.prev_cy = cy;
}

static void
_termview_mouse_up_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event)
{
   struct termview *const sd = data;
   const Evas_Event_Mouse_Up *const ev = event;
   unsigned int cx, cy;

   _coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
   _mouse_event(sd, "Release", cx, cy, ev->button);
   sd->mouse_drag.btn = 0; /* Disable mouse dragging */
}

static void
_termview_mouse_down_cb(void *data,
                        Evas *e EINA_UNUSED,
                        Evas_Object *obj EINA_UNUSED,
                        void *event)
{
   struct termview *const sd = data;
   const Evas_Event_Mouse_Down *const ev = event;
   unsigned int cx, cy;

   _coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);

   /* When pressing down the mouse, we just registered the first values thay
    * may be used for dragging with the mouse. */
   sd->mouse_drag.prev_cx = cx;
   sd->mouse_drag.prev_cy = cy;

   _mouse_event(sd, "Mouse", cx, cy, ev->button);
   sd->mouse_drag.btn = ev->button; /* Enable mouse dragging */
}

static void
_termview_mouse_wheel_cb(void *data,
                         Evas *e EINA_UNUSED,
                         Evas_Object *obj EINA_UNUSED,
                         void *event)
{
   struct termview *const sd = data;
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

   struct termview *const sd = data;
   const char *const string = ev->data;
   Eina_Bool ret = EINA_TRUE;
   size_t escaped_len = 0;

   for (size_t i = 0; i < ev->len; i++)
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
        nvim_api_input(sd->nvim, escaped, (unsigned int)escaped_len);
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
   struct termview *const sd = data;
   const Evas_Event_Key_Down *const ev = event;
   const Evas_Modifier *const mod = ev->modifiers;
   unsigned int send_size;
   const s_keymap *const keymap = keymap_get(ev->key);
   char nvim_compose = '\0';
   char buf[32];
   const char *send;
   const char caps[] = "Caps_Lock";
   s_gui *const gui = &(sd->nvim->gui);

   /* Did we press the Caps_Lock key? We can either have enabled or disabled
    * caps lock. */
   if (!strcmp(ev->key, caps)) /* Caps lock is pressed */
     {
        if (evas_key_lock_is_set(ev->locks, caps)) /* DISABLE */
          {
             /* If we press caps lock with prior caps locks, it means we
              * just DISABLED the caps lock */
             gui_caps_lock_dismiss(gui);
          }
        else /* ENABLE */
          {
             /* If we press caps lock without prior caps locks, it means we
              * just ENABLED the caps lock */
             gui_caps_lock_alert(gui);
          }
     }

   /* Try the composition. When this function returns EINA_TRUE, it already
    * worked out, nothing more to do. */
   if (_compose(sd, ev->key))
     return;

   /* Skip the AltGr key. */
   if (!strcmp(ev->key, "ISO_Level3_Shift") ||
       !strcmp(ev->key, "AltGr"))
     return;

   const Eina_Bool ctrl = evas_key_modifier_is_set(mod, "Control");
   const Eina_Bool shift = evas_key_modifier_is_set(mod, "Shift");
   const Eina_Bool super = evas_key_modifier_is_set(mod, "Super");
   const Eina_Bool alt = evas_key_modifier_is_set(mod, "Alt");

   /* Register modifiers. Ctrl and shit are special: we enable composition
    * only if the key is present in the keymap (it is a special key). */
   if (ctrl && keymap) { nvim_compose = 'C'; }
   if (shift && keymap) { nvim_compose = 'S'; }
   if (super) { nvim_compose = 'D'; }
   if (alt) { nvim_compose = 'M'; }

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
            elm_cnp_selection_get(gui->win,
                                  ELM_SEL_TYPE_CLIPBOARD, ELM_SEL_FORMAT_TEXT,
                                  _paste_cb, sd);
            break;

         default: /* Please the compiler! */
            break;
        }
        /* Stop right here. Do nothing more. */
        return;
     }

   if (nvim_compose)
     {
        const char *const key = keymap ? keymap->name : ev->string;
        if (! key) { return; }
        /* If we can compose, create an aggregate string we will send to
         * neovim. */
        send_size = (unsigned int)snprintf(buf, sizeof(buf), "<%c-%s>",
                                           nvim_compose, key);
        send = buf;
     }
   else if (keymap)
     {
        send_size = (unsigned int)snprintf(buf, sizeof(buf), "<%s>",
                                           keymap->name);
        send = buf;
     }
   else if (ev->string)
     {
        send = ev->string;
        send_size = (unsigned int)strlen(send);
     }
   else
     {
        /* Only pass ev->key if it is not a complex string. This allow to
         * filter out things like Control_L. */
        const unsigned int size = (unsigned int)strlen(ev->key);
        send_size = (size == 1) ? size : 0;
        send = ev->key; /* Never NULL */
     }

   /* If a key is availabe pass it to neovim and update the ui */
   if (EINA_LIKELY(send_size > 0))
     _keys_send(sd, send, send_size);
   else
     DBG("Unhandled key '%s'", ev->key);
}

static void
_termview_focus_in_cb(void *data,
                      Evas *evas,
                      Evas_Object *obj EINA_UNUSED,
                      void *event EINA_UNUSED)
{
   Edje_Message_Int msg = { .val = 1 }; /* may_blink := TRUE */
   struct termview *const sd = data;
   s_gui *const gui = &(sd->nvim->gui);

   /* When entering back on the Eovim window, the user may have pressed
    * Caps_Lock outside of Eovim's context. So we have to make sure when
    * entering Eovim again, that we are on the same page with the input
    * events. */
   const Evas_Lock *const lock = evas_key_lock_get(evas);
   if (evas_key_lock_is_set(lock, "Caps_Lock"))
     gui_caps_lock_alert(gui);
   else
     gui_caps_lock_dismiss(gui);

   edje_object_message_send(sd->cursor, EDJE_MESSAGE_INT,
                            THEME_MSG_MAY_BLINK_SET, &msg);
   edje_object_signal_emit(sd->cursor, "focus,in", "eovim");
   edje_object_signal_emit(sd->cursor, "eovim,blink,start", "eovim");
}

static void
_termview_focus_out_cb(void *data,
                      Evas *e EINA_UNUSED,
                      Evas_Object *obj EINA_UNUSED,
                      void *event EINA_UNUSED)
{
   Edje_Message_Int msg = { .val = 0 }; /* may_blink := FALSE */

   struct termview *const sd = data;
   edje_object_message_send(sd->cursor, EDJE_MESSAGE_INT,
                            THEME_MSG_MAY_BLINK_SET, &msg);
   edje_object_signal_emit(sd->cursor, "eovim,blink,stop", "eovim");
   edje_object_signal_emit(sd->cursor, "focus,out", "eovim");
}


static void
_smart_add(Evas_Object *obj)
{
   struct termview *const sd = calloc(1, sizeof(struct termview));
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
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_MOVE, _termview_mouse_move_cb, sd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_DOWN, _termview_mouse_down_cb, sd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_UP, _termview_mouse_up_cb, sd);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_WHEEL, _termview_mouse_wheel_cb, sd);

   Evas *const evas = evas_object_evas_get(obj);
   Evas_Object *o;

   /* Textgrid setup */
   sd->textgrid = o = evas_object_textgrid_add(evas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_member_add(o, obj);
   evas_object_textgrid_cell_size_get(o, (int*)&sd->cell_w, (int*)&sd->cell_h);
   evas_object_show(o);

   /* Cursor setup */
   sd->cursor = o = edje_object_add(evas);
   sd->cursor_calc = _cursor_calc_block; /* Cursor will be a block by default */
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

   /* Set the index at which the extended palette will start */
   sd->palette_id_generator = COL_GENERATOR_START;
}

static void
_smart_del(Evas_Object *obj)
{
   struct termview *const sd = evas_object_smart_data_get(obj);
   evas_object_del(sd->textgrid);
   evas_object_del(sd->cursor);
   eina_hash_free(sd->palettes);
   _composition_reset(sd);
}

static void
_smart_resize(Evas_Object *obj,
              Evas_Coord w,
              Evas_Coord h)
{
   struct termview *const sd = evas_object_smart_data_get(obj);

   const unsigned int cols = (unsigned int)w / sd->cell_w;
   const unsigned int rows = (unsigned int)h / sd->cell_h;
   DBG("resizing termview to %u,%u pixels, or %ux%u", w, h, cols,rows);

   /* Don't resize if not needed */
   if ((cols == sd->cols) && (rows == sd->rows)) { return; }

   evas_object_textgrid_size_set(sd->textgrid, (int)cols, (int)rows);
   sd->cols = cols;
   sd->rows = rows;

   termview_resize(obj, cols, rows);
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
_smart_calculate(Evas_Object *obj)
{
   struct termview *const sd = evas_object_smart_data_get(obj);
   Evas_Coord ox, oy;
   evas_object_geometry_get(obj, &ox, &oy, NULL, NULL);

   /* Place the textgrid */
   evas_object_move(sd->textgrid, ox, oy);
   evas_object_resize(sd->textgrid,
                      (int)(sd->cols * sd->cell_w),
                      (int)(sd->rows * sd->cell_h));

   /* Place the cursor */
   sd->cursor_calc(sd, ox, oy);
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
   struct termview *const sd = evas_object_smart_data_get(obj);
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
   struct termview *const sd = evas_object_smart_data_get(obj);
   evas_object_textgrid_update_add(sd->textgrid, 0, 0, (int)sd->cols, (int)sd->rows);
}

void
termview_font_set(Evas_Object *obj,
                  const char *font_name,
                  unsigned int font_size)
{
   EINA_SAFETY_ON_NULL_RETURN(font_name);
   EINA_SAFETY_ON_TRUE_RETURN(font_size == 0);

   struct termview *const sd = evas_object_smart_data_get(obj);

   evas_object_textgrid_font_set(sd->textgrid, font_name, (int)font_size);
   evas_object_textgrid_cell_size_get(sd->textgrid,
                                      (int*)&sd->cell_w, (int*)&sd->cell_h);
}

void
termview_cell_size_get(const Evas_Object *obj,
                       unsigned int *w,
                       unsigned int *h)
{
   struct termview *const sd = evas_object_smart_data_get(obj);
   if (w) *w = sd->cell_w;
   if (h) *h = sd->cell_h;
}

void
termview_size_get(const Evas_Object *obj,
                  unsigned int *cols,
                  unsigned int *rows)
{
   struct termview *const sd = evas_object_smart_data_get(obj);
   if (cols) *cols = sd->cols;
   if (rows) *rows = sd->rows;
}

void
termview_resize(Evas_Object *obj,
                unsigned int cols,
                unsigned int rows)
{
   if (EINA_UNLIKELY((cols == 0) || (rows == 0))) { return; }

   struct termview *const sd = evas_object_smart_data_get(obj);


   /* When we resize the termview, we have reset the scrolling region to the
    * whole termview. */
   const Eina_Rectangle region = {
      .x = 0,
      .y = 0,
      .w = (int)cols - 1,
      .h = (int)rows - 1,
   };
   termview_scroll_region_set(obj, &region);
   nvim_api_ui_try_resize(sd->nvim, cols, rows);
}

void
termview_resized_confirm(Evas_Object *obj,
                         unsigned int cols,
                         unsigned int rows)
{
   /* Update the neovim columns and rows */
   struct termview *const sd = evas_object_smart_data_get(obj);
   sd->nvim_rows = rows;
   sd->nvim_cols = cols;
}

void
termview_clear(Evas_Object *obj)
{
   struct termview *const sd = evas_object_smart_data_get(obj);
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
   struct termview *const sd = evas_object_smart_data_get(obj);
   Evas_Object *const grid = sd->textgrid;

   if (EINA_UNLIKELY(_unfinished_resizing_is(sd))) { return; }

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
   struct termview *const sd = evas_object_smart_data_get(obj);

   if (EINA_UNLIKELY(_unfinished_resizing_is(sd))) { return; }

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
   struct termview *const sd = evas_object_smart_data_get(obj);

   if (EINA_UNLIKELY(_unfinished_resizing_is(sd))) { return; }

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

   sd->x = to_x;
   sd->y = to_y;
   sd->cursor_calc(sd, x, y);
}



struct termview_color
termview_color_decompose(uint32_t col)
{
  /*
   * When true colors are requested, we have to decompose
   * a 24-bits color. Alpha is always maximal (full opacity).
   */
  const struct termview_color color = {
     .r = (uint8_t)((col & 0x00ff0000) >> 16),
     .g = (uint8_t)((col & 0x0000ff00) >> 8),
     .b = (uint8_t)((col & 0x000000ff) >> 0),
     .a = 0xff,
  };
  return color;
}

static uint8_t
_make_palette(struct termview *sd,
              struct termview_color color)
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
_make_palette_from_color(struct termview *sd,
                         t_int color,
                         Eina_Bool fg)
{
   if (color < 0)
     {
        return (fg) ? COL_DEFAULT_FG : COL_DEFAULT_BG;
     }
   else
     {
        const struct termview_color col =
           termview_color_decompose((uint32_t)color);
        return _make_palette(sd, col);
     }
}

void
termview_style_set(Evas_Object *obj,
                   const struct termview_style *style)
{
   struct termview *const sd = evas_object_smart_data_get(obj);

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
   struct termview *const sd = evas_object_smart_data_get(obj);
   memcpy(&sd->scroll, region, sizeof(Eina_Rectangle));
}

void
termview_scroll(Evas_Object *obj,
                int count)
{
   struct termview *const sd = evas_object_smart_data_get(obj);
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
   struct termview *const sd = evas_object_smart_data_get(obj);

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
termview_fg_color_get(const Evas_Object *obj,
                      int *r, int *g, int *b, int *a)
{
   const struct termview *const sd = evas_object_smart_data_get(obj);
   evas_object_textgrid_palette_get(
      sd->textgrid, EVAS_TEXTGRID_PALETTE_EXTENDED,
      COL_DEFAULT_FG, r, g, b, a
   );
}

void
termview_cell_to_coords(const Evas_Object *obj,
                        unsigned int cell_x,
                        unsigned int cell_y,
                        int *px,
                        int *py)
{
   const struct termview *const sd = evas_object_smart_data_get(obj);

   int wx, wy;
   evas_object_geometry_get(sd->textgrid, &wx, &wy, NULL, NULL);

   if (px) *px = (int)(cell_x * sd->cell_w) + wx;
   if (py) *py = (int)(cell_y * sd->cell_h) + wy;
}


static void
_cursor_color_get(s_nvim *nvim,
                  const s_hl_group *hl_group)
{
   if (! hl_group->bg.used) { return; }

   /* Collect the fg and bg colors, and pass them to the edje theme */
   Edje_Message_Int_Set *msg;
   msg = alloca(sizeof(*msg) + (sizeof(int) * 3));
   msg->count = 3;
   msg->val[0] = hl_group->bg.r;
   msg->val[1] = hl_group->bg.g;
   msg->val[2] = hl_group->bg.b;

   struct termview *const sd = evas_object_smart_data_get(nvim->gui.termview);
   edje_object_message_send(sd->cursor, EDJE_MESSAGE_INT_SET,
                            THEME_MSG_COLOR_SET, msg);
}

void
termview_cursor_mode_set(Evas_Object *obj,
                         const s_mode *mode)
{
   /* Set sd->cursor_calc to the appropriate function that will calculate
    * the resizing and positionning of the cursor. We also keep track of
    * the mode. */
   struct termview *const sd = evas_object_smart_data_get(obj);
   const f_cursor_calc funcs[] = {
      [CURSOR_SHAPE_BLOCK] = _cursor_calc_block,
      [CURSOR_SHAPE_HORIZONTAL] = _cursor_calc_horizontal,
      [CURSOR_SHAPE_VERTICAL] = _cursor_calc_vertical,
   };

   /* Prepare the parameters to push to embryo */
   Edje_Message_Float_Set *msg;
   msg = alloca(sizeof(*msg) + (sizeof(double) * 3));
   msg->count = 3;
   msg->val[0] = (double)mode->blinkwait / 1000.0;
   msg->val[1] = (double)mode->blinkon / 1000.0;
   msg->val[2] = (double)mode->blinkoff / 1000.0;

   /* If the cursor was blinking, we stop the blinking */
   if (sd->mode && sd->mode->blinkon != 0)
     edje_object_signal_emit(sd->cursor, "eovim,blink,stop", "eovim");

   /* If we requested the cursor to blink, make it blink */
   if (mode->blinkon != 0)
     {
        edje_object_message_send(sd->cursor, EDJE_MESSAGE_FLOAT_SET,
                                 THEME_MSG_BLINK_SET, msg);
        edje_object_signal_emit(sd->cursor, "eovim,blink,start", "eovim");
     }

   /* Register the new mode and update the cursor calculation function. */
   sd->mode = mode;
   sd->cursor_calc = funcs[mode->cursor_shape];

   /* Send a request to neovim so we can get the color of the damn cursor.
    * It is not made easy!!!! */
   sd->nvim->hl_group_decode(sd->nvim, mode->hl_id, _cursor_color_get);
}

void
termview_cursor_visibility_set(Evas_Object *obj,
                               Eina_Bool visible)
{
   struct termview *const sd = evas_object_smart_data_get(obj);
   if (visible) evas_object_show(sd->cursor);
   else evas_object_hide(sd->cursor);
}
