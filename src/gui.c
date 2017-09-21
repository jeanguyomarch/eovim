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

#include "eovim/types.h"
#include "eovim/gui.h"
#include "eovim/nvim.h"
#include "eovim/main.h"
#include "eovim/log.h"
#include "eovim/nvim_api.h"
#include "eovim/config.h"
#include "contrib/contrib.h"
#include <Elementary.h>

static const double _font_min = 4.0;
static const double _font_max = 72.0;

typedef struct
{
   Eina_Stringshare *name;
   Eina_Stringshare *fancy;
} s_font;

typedef enum
{
   THEME_MSG_BG         = 0,
   THEME_MSG_COMPL      = 1,
} e_theme_msg;

static Elm_Genlist_Item_Class *_itc = NULL;
static Elm_Genlist_Item_Class *_font_itc = NULL;

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

static void
_config_show_cb(void *data,
                Evas_Object *obj EINA_UNUSED,
                const char *emission EINA_UNUSED,
                const char *source EINA_UNUSED)
{
   s_nvim *const nvim = data;
   gui_config_show(&nvim->gui);
}

static void
_config_hide_cb(void *data,
                Evas_Object *obj EINA_UNUSED,
                const char *emission EINA_UNUSED,
                const char *source EINA_UNUSED)
{
   s_nvim *const nvim = data;
   s_gui *const gui = &nvim->gui;
   gui_config_hide(gui);
   config_save(nvim->config);

   elm_object_focus_set(gui->layout, EINA_FALSE);
   evas_object_focus_set(gui->termview, EINA_TRUE);
}

static Evas_Object *
_frame_add(Evas_Object *parent,
           const char *text)
{
   Evas_Object *f;

   f = elm_frame_add(parent);
   elm_frame_autocollapse_set(f, EINA_FALSE);
   elm_object_text_set(f, text);
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(f, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(f);

   return f;
}

static void
_recalculate_gui_size(s_gui *gui)
{
   int tv_w, tv_h;
   unsigned int w, h;
   termview_cell_size_get(gui->termview, &w, &h);
   evas_object_geometry_get(gui->termview, NULL, NULL, &tv_w, &tv_h);
   const unsigned int cols = (unsigned)tv_w / w;
   const unsigned int rows = (unsigned)tv_h / h;
   gui_resize(gui, cols, rows);
   termview_refresh(gui->termview);
}


/*============================================================================*
 *                          Background Color Handling                         *
 *============================================================================*/

static void
_bg_color_changed_cb(void *data,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   s_gui *const gui = data;
   s_config *const config = gui->nvim->config;
   int r, g, b, a;

   if (config->use_bg_color)
     {
        elm_colorselector_color_get(obj, &r, &g, &b, &a);
        config_bg_color_set(config, r, g, b, a);
        gui_bg_color_set(gui, r, g, b, a);
     }
}

static void
_bg_sel_changed_cb(void *data,
                   Evas_Object *obj,
                   void *event_info EINA_UNUSED)
{
   s_gui *const gui = data;
   const Eina_Bool use_bg = elm_check_state_get(obj);
   /* FIXME use elm_frame_collapse_go(). Prettier, but does not work */
   elm_frame_collapse_set(gui->config.bg_sel_frame, !use_bg);
   config_use_bg_color_set(gui->nvim->config, use_bg);

   if (use_bg)
     _bg_color_changed_cb(gui, gui->config.bg_sel, NULL);
   else
     elm_layout_signal_emit(gui->layout, "eovim,background,unmask", "eovim");
}

static Evas_Object *
_config_bg_add(s_gui *gui,
               Evas_Object *parent)
{
   const s_config *config = gui->nvim->config;

   Evas_Object *const f = _frame_add(parent, "Background Settings");
   Evas_Object *const box = elm_box_add(f);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, 0.0);

   /* check that toggles the color selector */
   Evas_Object *const check = elm_check_add(parent);
   evas_object_size_hint_align_set(check, 0.0, 0.0);
   elm_object_text_set(check, "Enable unified colored background");
   elm_check_state_set(check, config->use_bg_color);
   evas_object_smart_callback_add(check, "changed", _bg_sel_changed_cb, gui);

   /* frame that allows the color selector to collapse */
   Evas_Object *const sel_frame = _frame_add(parent, "Unified Color Selection");
   elm_frame_autocollapse_set(sel_frame, EINA_FALSE);
   elm_frame_collapse_set(sel_frame, ! config->use_bg_color);

   /* color selector */
   Evas_Object *const sel = elm_colorselector_add(parent);
   evas_object_smart_callback_add(sel, "changed", _bg_color_changed_cb, gui);
   evas_object_size_hint_weight_set(sel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sel, EVAS_HINT_FILL, 0.0);
   const s_config_color *const col = config->bg_color;
   elm_colorselector_color_set(sel, col->r, col->g, col->b, col->a);

   elm_box_pack_start(box, check);
   elm_box_pack_end(box, sel_frame);
   elm_object_content_set(sel_frame, sel);
   elm_object_content_set(f, box);

   evas_object_show(box);
   evas_object_show(sel_frame);
   evas_object_show(sel);
   evas_object_show(check);

   gui->config.bg_sel = sel;
   gui->config.bg_sel_frame = sel_frame;

   return f;
}

/*============================================================================*
 *                             Font Size Handling                             *
 *============================================================================*/

static void
_font_size_cb(void *data,
              Evas_Object *obj,
              void *event_info EINA_UNUSED)
{
   s_gui *const gui = data;
   s_config *const config = gui->nvim->config;
   const unsigned int size = (unsigned int)elm_slider_value_get(obj);

   config_font_size_set(config, size);
   termview_font_set(gui->termview, config->font_name, size);
   _recalculate_gui_size(gui);
}

static Evas_Object *
_config_font_size_add(s_gui *gui,
                      Evas_Object *parent)
{
   const s_config *const config = gui->nvim->config;

   /* Frame container */
   Evas_Object *const f = _frame_add(parent, "Font Size Settings");

   /* Slider */
   Evas_Object *const sl = elm_slider_add(f);
   evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_slider_span_size_set(sl, 40);
   elm_slider_unit_format_set(sl, "%1.0f");
   elm_slider_indicator_format_set(sl, "%1.0f");
   elm_slider_min_max_set(sl, _font_min, _font_max);
   elm_slider_value_set(sl, config->font_size);
   evas_object_smart_callback_add(sl, "delay,changed", _font_size_cb, gui);
   evas_object_show(sl);

   elm_object_content_set(f, sl);
   return f;
}

/*============================================================================*
 *                             Font Name Handling                             *
 *============================================================================*/

static int
_font_sort_cb(const void *a, const void *b)
{
   return strcasecmp(a, b);
}

static void
_font_item_del(void *data,
               Evas_Object *obj EINA_UNUSED)
{
   s_font *const font = data;

   eina_stringshare_del(font->fancy);
   eina_stringshare_del(font->name);
   free(font);
}

static char *
_font_text_get(void *data,
               Evas_Object *obj EINA_UNUSED,
               const char *part EINA_UNUSED)
{
   const s_font *const font = data;
   return strndup(font->fancy, (size_t)eina_stringshare_strlen(font->fancy));
}

static void
_font_sel_cb(void *data,
             Evas_Object *obj EINA_UNUSED,
             void *event)
{
   s_gui *const gui = data;
   s_config *const config = gui->nvim->config;
   const Elm_Genlist_Item *const item = event;
   const s_font *const font = elm_object_item_data_get(item);

   /*
    * Write the font name in the config and change the font of the termview.
    */
   config_font_name_set(config, font->name);
   termview_font_set(gui->termview, config->font_name, config->font_size);
   _recalculate_gui_size(gui);
}

static Evas_Object *
_config_font_name_add(s_gui *gui,
                      Evas_Object *parent)
{
   const s_config *const config = gui->nvim->config;

   /* Frame container */
   Evas_Object *const f = _frame_add(parent, "Font Name Settings");
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   /* Fonts list */
   Evas_Object *const gl = elm_genlist_add(f);
   evas_object_size_hint_weight_set(gl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(f, gl);
   evas_object_show(gl);

   /* Get a sorted list of all available fonts */
   Evas *const evas = evas_object_evas_get(parent);
   Eina_List *fonts = evas_font_available_list(evas);
   fonts = eina_list_sort(fonts, eina_list_count(fonts), _font_sort_cb);

   /* Add each font to the list of possible fonts */
   Eina_List *l;
   const char *fname;
   EINA_LIST_FOREACH(fonts, l, fname)
     {
        s_font *const font = malloc(sizeof(s_font));
        if (EINA_UNLIKELY(! font))
          {
             CRI("Failed to allocate memory");
             goto fail;
          }
        const int ret = contrib_parse_font_name(fname, &font->name, &font->fancy);
        if (EINA_UNLIKELY(ret < 0))
          {
             WRN("Failed to parse font '%s'", fname);
             free(font);
             continue;
          }

        elm_genlist_item_append(
           gl, _font_itc, font, NULL, ELM_GENLIST_ITEM_NONE, _font_sel_cb, gui
        );
     }

   evas_font_available_list_free(evas, fonts);

   return f;

fail:
   evas_font_available_list_free(evas, fonts);
   evas_object_del(f);
   return NULL;
}

void
gui_config_show(s_gui *gui)
{
   if (gui->config.box) { return; }

   Evas_Object *const box = elm_box_add(gui->layout);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_align_set(box, 0.5, 0.0);
   evas_object_show(box);

   Evas_Object *o;

   o = _config_bg_add(gui, box);
   elm_box_pack_end(box, o);

   o = _config_font_size_add(gui, box);
   elm_box_pack_end(box, o);

   o = _config_font_name_add(gui, box);
   elm_box_pack_end(box, o);

   elm_layout_content_set(gui->layout, "eovim.config.box", box);

   elm_layout_signal_emit(gui->layout, "config,show", "eovim");
   evas_object_focus_set(gui->termview, EINA_FALSE);


   gui->config.box = box;
}

void
gui_config_hide(s_gui *gui)
{
   elm_layout_signal_emit(gui->layout, "config,hide", "eovim");

   elm_layout_content_unset(gui->layout, "eovim.config.box");
   evas_object_del(gui->config.box);
   gui->config.box = NULL;
}

Eina_Bool
gui_add(s_gui *gui,
        s_nvim *nvim)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(gui, EINA_FALSE);

   const s_config *const config = nvim->config;
   Evas_Object *o;
   gui->nvim = nvim;

   /* Window setup */
   gui->win = elm_win_util_standard_add("eovim", "Eovim");
   elm_win_autodel_set(gui->win, EINA_TRUE);

   /* Main Layout setup */
   gui->layout = _layout_item_add(gui, "eovim/main");
   if (EINA_UNLIKELY(! gui->layout))
     {
        CRI("Failed to get layout item");
        goto fail;
     }
   gui->edje = elm_layout_edje_get(gui->layout);
   elm_layout_signal_callback_add(gui->layout, "config,open", "eovim",
                                  _config_show_cb, nvim);
   elm_layout_signal_callback_add(gui->layout, "config,close", "eovim",
                                  _config_hide_cb, nvim);
   elm_win_resize_object_add(gui->win, gui->layout);
   evas_object_smart_callback_add(gui->win, "focus,in", _focus_in_cb, gui);

   gui->termview = termview_add(gui->layout, nvim);
   termview_font_set(gui->termview, config->font_name, config->font_size);

   /* Create the completion genlist, and attach it to the theme layout.
    * It shall not be subject to focus. */
   o = gui->completion.gl = elm_genlist_add(gui->layout);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_layout_content_set(gui->layout, "eovim.completion", o);
   elm_object_tree_focus_allow_set(o, EINA_FALSE);

   /*
    * We set the resieing step of the window to the size of a cell of the
    * textgrid that is embedded within the termview.
    */
   unsigned int cell_w, cell_h;
   termview_cell_size_get(gui->termview, &cell_w, &cell_h);
   elm_win_size_step_set(gui->win, (int)cell_w, (int)cell_h);

   elm_layout_content_set(gui->layout, "eovim.main.view", gui->termview);

   evas_object_show(gui->termview);
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
        const Eina_Unicode *ustring,
        unsigned int size)
{
   termview_put(gui->termview, ustring, size);
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
gui_update_fg(s_gui *gui,
              t_int color)
{
   if (color >= 0)
     {
        const s_termview_color col = termview_color_decompose((uint32_t)color,
                                                              gui->nvim->true_colors);
        termview_fg_color_set(gui->termview, col.r, col.g, col.b, col.a);
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
     termview_scroll(gui->termview, scroll);
}

void
gui_busy_set(s_gui *gui,
             Eina_Bool busy)
{
   const char *const signal = (busy) ? "eovim,busy,on" : "eovim,busy,off";
   elm_layout_signal_emit(gui->layout, signal, "eovim");
}

void
gui_bg_color_set(s_gui *gui,
                 int r, int g, int b, int a)
{
   Edje_Message_Int_Set *msg;

   msg = alloca(sizeof(*msg) + (sizeof(int) * 4));
   msg->count = 4;
   msg->val[0] = r;
   msg->val[1] = g;
   msg->val[2] = b;
   msg->val[3] = a;

   elm_layout_signal_emit(gui->layout, "eovim,background,mask", "eovim");
   edje_object_message_send(gui->edje, EDJE_MESSAGE_INT_SET, THEME_MSG_BG, msg);
}

static char *
_completion_gl_text_get(void *data,
                        Evas_Object *obj EINA_UNUSED,
                        const char *part EINA_UNUSED)
{
   const s_completion *const compl = data;
   return strndup(compl->word, (size_t)eina_stringshare_strlen(compl->word));
}

static void
_completion_sel_cb(void *data,
                   Evas_Object *obj EINA_UNUSED,
                   void *event)
{
   s_gui *const gui = data;
   const Elm_Genlist_Item *const item = event;

   /* If the completion.event is set, the item was selected because of a
    * neovim event, not because the user did clic on the item.
    * So we reset the event, and abort the function right here */
   if (gui->completion.event)
     {
        gui->completion.event = EINA_FALSE;
        return;
     }

   /* Get the indexes of the currently selected item and the item we have
    * clicked on and we want to insert. */
   const int sel_idx = gui->completion.sel
      ? elm_genlist_item_index_get(gui->completion.sel)
      : 1; /* No item selected? Take the first one */
   const int compl_idx = elm_genlist_item_index_get(item);

   /* Create a string buffer that will hold the input to be passed to neovim */
   Eina_Strbuf *const input = eina_strbuf_new();
   if (EINA_UNLIKELY(! input))
     {
        CRI("Failed to create string buffer");
        return;
     }

   /* If the item to be completed is greater than the selected item, spam the
    * <C-n> to make neovim advance the selection. Otherwise, the <C-p>.
    * If the indexes are the same, do nothing. */
   if (sel_idx < compl_idx)
     {
        for (int i = sel_idx; i < compl_idx; i++)
          eina_strbuf_append_length(input, "<C-n>", 5);
     }
   else if (sel_idx > compl_idx)
     {
        for (int i = compl_idx; i >= sel_idx; i--)
          eina_strbuf_append_length(input, "<C-p>", 5);
     }
   /* Send a signal to end the completion */
   eina_strbuf_append_length(input, "<C-y>", 5);

   /* Pass all these data to neovim and cleanup behind us */
   nvim_api_input(gui->nvim, eina_strbuf_string_get(input),
                  (unsigned int)eina_strbuf_length_get(input));
   eina_strbuf_free(input);
}

void
gui_completion_add(s_gui *gui,
                   s_completion *completion)
{
   elm_genlist_item_append(
      gui->completion.gl, _itc, completion,
      NULL, ELM_GENLIST_ITEM_NONE,
      _completion_sel_cb, gui
   );
}

static Elm_Genlist_Item *
_gl_nth_get(const Evas_Object *gl,
            unsigned int index)
{
   /*
    * Find the N-th element in a genlist. We use a dichotomic search to
    * lower the algorithmic completxity.
    */
   const unsigned int items = elm_genlist_items_count(gl);
   const unsigned int half = items / 2;
   Elm_Genlist_Item *ret = NULL;

   if (EINA_UNLIKELY(index >= items))
     {
        ERR("Attempt to get item %u out of %u", index, items);
     }
   else if (index <= half)
     {
        ret = elm_genlist_first_item_get(gl);
        for (unsigned int i = 0; i < index; i++)
          ret = elm_genlist_item_next_get(ret);
     }
   else
     {
        ret = elm_genlist_last_item_get(gl);
        for (unsigned int i = items - 2; i >= index; i--)
          ret = elm_genlist_item_prev_get(ret);
     }

   return ret;
}

void
gui_completion_selected_set(s_gui *gui,
                            int index)
{
   Evas_Object *const gl = gui->completion.gl;

   /*
    * If the index is negative, we unselect the previously selected items.
    * Otherwise we select the item at the provded index.
    */

   if (index < 0)
     {
        Elm_Genlist_Item *const selected = elm_genlist_selected_item_get(gl);
        gui->completion.event = EINA_FALSE;
        if (selected)
          elm_genlist_item_selected_set(selected, EINA_FALSE);
        gui->completion.sel = NULL;
     }
   else /* index is >= 0, we can safely cast it as unsigned */
     {
        Elm_Genlist_Item *const item = _gl_nth_get(gl, (unsigned int)index);
        gui->completion.event = EINA_TRUE;
        elm_genlist_item_selected_set(item, EINA_TRUE);
        elm_genlist_item_bring_in(item, ELM_GENLIST_ITEM_SCROLLTO_IN);
        gui->completion.sel = item;
     }
}

void
gui_completion_show(s_gui *gui,
                    unsigned int selected,
                    unsigned int x,
                    unsigned int y)
{
   /* Before starting to setup the ui, we mark the completion as incoming
    * from neovim */
   gui->completion.event = EINA_TRUE;

   /* Show the completion panel */
   elm_layout_signal_emit(gui->layout, "eovim,completion,show", "eovim");

   /* Position the completion panel */
   int px, py;
   /*
    * TODO This function should calculate the best place where to place the
    * damn thing. For now we place it one row below the insertion point.
    * This is pretty dumb as the completion panel would NOT be visible when
    * editing the bottom lines of a window!
    */
   y++;

   termview_cell_to_coords(gui->termview, x, y, &px, &py);

   Edje_Message_Int_Set *msg;
   msg = alloca(sizeof(*msg) + (sizeof(int) * 2));
   msg->count = 4;
   msg->val[0] = px;
   msg->val[1] = py;
   msg->val[2] = px + 400;
   msg->val[3] = py + 100;
   edje_object_message_send(gui->edje, EDJE_MESSAGE_INT_SET, THEME_MSG_COMPL, msg);

   /* Select the appropriate item */
   Elm_Genlist_Item *const sel = _gl_nth_get(gui->completion.gl, selected);
   elm_genlist_item_selected_set(sel, EINA_TRUE);
}

void
gui_completion_hide(s_gui *gui)
{
   elm_layout_signal_emit(gui->layout, "eovim,completion,hide", "eovim");
}

void
gui_completion_clear(s_gui *gui)
{
   gui->completion.sel = NULL;
   elm_genlist_clear(gui->completion.gl);
}


void
gui_bell_ring(s_gui *gui)
{
   elm_layout_signal_emit(gui->layout, "eovim,bell,ring", "eovim");
}

static void
_completion_item_del(void *data,
                     Evas_Object *obj EINA_UNUSED)
{
   s_completion *const compl = data;

   eina_stringshare_del(compl->word);
   eina_stringshare_del(compl->kind);
   eina_stringshare_del(compl->menu);
   eina_stringshare_del(compl->info);
   free(compl);
}

Eina_Bool
gui_init(void)
{
   /* Completion list item class */
   _itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(! _itc))
     {
        CRI("Failed to create genlist item class");
        return EINA_FALSE;
     }
   _itc->item_style = "default";
   _itc->func.text_get = _completion_gl_text_get;
   _itc->func.del = _completion_item_del;

   /* Font list item class */
   _font_itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(! _font_itc))
     {
        CRI("Failed to create genlist item class");
        goto fail;
     }
   _font_itc->item_style = "default";
   _font_itc->func.text_get = _font_text_get;
   _font_itc->func.del = _font_item_del;

   return EINA_TRUE;
fail:
   elm_genlist_item_class_free(_itc);
   return EINA_FALSE;
}

void
gui_shutdown(void)
{
   elm_genlist_item_class_free(_font_itc);
   elm_genlist_item_class_free(_itc);
}
