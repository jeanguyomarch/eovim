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
#include "eovim/mode.h"
#include "eovim/main.h"
#include "eovim/log.h"
#include "eovim/nvim_api.h"
#include "eovim/config.h"
#include <Elementary.h>

static const char *const _nvim_data_key = "nvim";
typedef enum
{
   THEME_MSG_BG = 0,
   THEME_MSG_CMDLINE_INFO = 1,
} e_theme_msg;

static Elm_Genlist_Item_Class *_compl_simple_itc = NULL;
static Elm_Genlist_Item_Class *_wildmenu_itc = NULL;

static void _wildmenu_resize(s_gui *gui);

static inline s_nvim *
_nvim_get(const Evas_Object *obj)
{
   return evas_object_data_get(obj, _nvim_data_key);
}

static void
_focus_in_cb(void *data,
             Evas_Object *obj EINA_UNUSED,
             void *event EINA_UNUSED)
{
   s_gui *const gui = data;
   evas_object_focus_set(gui->termview, EINA_TRUE);
}

static Evas_Object *
_layout_item_add(Evas_Object *parent,
                 const char *group)
{
   Evas_Object *const o = elm_layout_add(parent);
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
_prefs_show_cb(void *data,
               Evas_Object *obj EINA_UNUSED,
               const char *emission EINA_UNUSED,
               const char *source EINA_UNUSED)
{
   s_nvim *const nvim = data;
   prefs_show(&nvim->gui);
}

static void
_prefs_hide_cb(void *data,
               Evas_Object *obj EINA_UNUSED,
               const char *emission EINA_UNUSED,
               const char *source EINA_UNUSED)
{
   s_nvim *const nvim = data;
   s_gui *const gui = &nvim->gui;
   prefs_hide(gui);
   config_save(nvim->config);

   elm_object_focus_set(gui->layout, EINA_FALSE);
   evas_object_focus_set(gui->termview, EINA_TRUE);
}

void
gui_size_recalculate(s_gui *gui)
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

static void
_completion_genlist_sizing_eval(s_gui *gui,
                                const Elm_Genlist_Item *item)
{
   Evas_Object *const obj = gui->completion.obj;

   /* Get the size of the completion item */
   const Evas_Object *const widget = elm_object_item_part_content_get(
      item, "elm.swallow.content"
   );
   int item_w, item_h;
   evas_object_geometry_get(widget, NULL, NULL, &item_w, &item_h);


   /* Get the position where the completion panel was triggerred */
   int px, py;
   termview_cell_to_coords(gui->termview, gui->completion.items.at_x,
                           gui->completion.items.at_y, &px, &py);

   /* Termview dimensions */
   int tv_orig_y, tv_width, tv_height;
   evas_object_geometry_get(gui->termview, NULL, &tv_orig_y, &tv_width, &tv_height);

   /* Calculate the (my) ideal height */
   const int visible_items = MIN((int)gui->completion.items.count, 10);
   const int height = ((visible_items + 1) * item_h);

   /* Termview cell height */
   int cell_h;
   termview_cell_size_get(gui->termview, NULL, (unsigned int *)&cell_h);

   int resize_h;
   int pos_y;
   if (py + cell_h + height < tv_height)
     {
        /* (1) If there is room below the insertion point, try to place the
         * completion panel on the line BELOW. */
        resize_h = height;
        pos_y = cell_h + py;
     }
   else if (py - height > 0)
     {
        /* (2) If no room below, try to place the completion panel ABOVE */
        resize_h = height;
        pos_y = py - resize_h;
     }
   else
     {
        /* (3) Oh noo, perfect size does not fit anywhere. Take the side where
         * we have the most room and fill it completely */
        if (py > tv_height / 2) /* Place it above */
          {
             resize_h = py - tv_orig_y;
             pos_y = py - resize_h;
          }
        else /* Place it below */
          {
             resize_h = tv_height - (py + cell_h);
             pos_y = py + cell_h;
          }
     }

   const int max_width = tv_width - px;
   evas_object_move(obj, px, pos_y);
   evas_object_resize(obj, MIN(800, max_width), resize_h); /* XXX Find better*/
   evas_object_show(obj);
}

static void
_realized_cb(void *data,
             Evas_Object *genlist EINA_UNUSED,
             void *info)
{
   s_gui *const gui = data;
   const Elm_Genlist_Item *const item = info;

   /* If the genlist's sizing has already been evaluated, don't do it twice */
   if (! gui->completion.items.calculated)
     {
        gui->completion.items.calculated = EINA_TRUE;
        _completion_genlist_sizing_eval(gui, item);
     }
}

Eina_Bool
gui_add(s_gui *gui,
        s_nvim *nvim)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(gui, EINA_FALSE);

   const s_config *const config = nvim->config;
   const char *const edje_file = main_edje_file_get();
   Evas_Object *o;
   gui->nvim = nvim;

   gui->cache = eina_strbuf_new();
   if (EINA_UNLIKELY(! gui->cache))
     {
        CRI("Failed to create cache string buffer");
        goto fail;
     }

   /* Window setup */
   gui->win = elm_win_util_standard_add("eovim", "Eovim");
   elm_win_autodel_set(gui->win, EINA_TRUE);
   Evas *const evas = evas_object_evas_get(gui->win);

   /* Main Layout setup */
   gui->layout = _layout_item_add(gui->win, "eovim/main");
   if (EINA_UNLIKELY(! gui->layout))
     {
        CRI("Failed to get layout item");
        goto fail;
     }
   gui->edje = elm_layout_edje_get(gui->layout);
   elm_layout_signal_callback_add(gui->layout, "config,open", "eovim",
                                  _prefs_show_cb, nvim);
   elm_layout_signal_callback_add(gui->layout, "config,close", "eovim",
                                  _prefs_hide_cb, nvim);
   elm_win_resize_object_add(gui->win, gui->layout);
   evas_object_smart_callback_add(gui->win, "focus,in", _focus_in_cb, gui);

   /* ========================================================================
    * Completion GUI objects
    * ===================================================================== */

   o = gui->completion.obj = edje_object_add(evas);
   edje_object_file_set(o, edje_file, "eovim/completion");
   evas_object_smart_member_add(o, gui->layout);

   /* Create the completion genlist, and attach it to the theme layout.
    * It shall not be subject to focus. */
   o = gui->completion.gl = elm_genlist_add(gui->layout);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(o, "realized", _realized_cb, gui);
   elm_object_tree_focus_allow_set(o, EINA_FALSE);
   edje_object_part_swallow(gui->completion.obj, "eovim.completion", o);
   evas_object_data_set(o, _nvim_data_key, nvim);
   evas_object_show(o);

   /* ========================================================================
    * Termview GUI objects
    * ===================================================================== */

   gui->termview = termview_add(gui->layout, nvim);
   termview_font_set(gui->termview, config->font_name, config->font_size);
   elm_layout_content_set(gui->layout, "eovim.main.view", gui->termview);

   /* ========================================================================
    * Command-Line GUI objects
    * ===================================================================== */

   gui->cmdline.obj = edje_object_part_swallow_get(gui->edje, "eovim.cmdline");
   gui->cmdline.info = edje_object_part_swallow_get(gui->edje, "eovim.cmdline_info");

   /* Table: will hold both the spacer and the genlist */
   gui->cmdline.table = o = elm_table_add(gui->layout);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_layout_content_set(gui->layout, "eovim.wildmenu", o);

   /* Spacer: to make the genlist fit a given size */
   gui->cmdline.spacer = o = evas_object_rectangle_add(evas);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_color_set(o, 0, 0, 0, 0);
   elm_table_pack(gui->cmdline.table, o, 0, 0, 1, 1);

   /* Menu: the genlist that will hold the wildmenu items */
   gui->cmdline.menu = o = elm_genlist_add(gui->layout);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
   elm_genlist_homogeneous_set(o, EINA_TRUE);
   elm_genlist_mode_set(o, ELM_LIST_COMPRESS);
   elm_object_tree_focus_allow_set(o, EINA_FALSE);
   elm_table_pack(gui->cmdline.table, o, 0, 0, 1, 1);
   evas_object_show(gui->cmdline.menu);

   /* ========================================================================
    * Finalize GUI
    * ===================================================================== */

   gui_cmdline_hide(gui);
   evas_object_show(gui->termview);
   evas_object_show(gui->layout);
   evas_object_show(gui->win);
   gui_resize(gui, nvim->opts->geometry.w, nvim->opts->geometry.h);
   return EINA_TRUE;

fail:
   if (gui->cache) eina_strbuf_free(gui->cache);
   evas_object_del(gui->win);
   return EINA_FALSE;
}

void
gui_del(s_gui *gui)
{
   if (EINA_LIKELY(gui != NULL))
     {
        eina_strbuf_free(gui->cache);
        evas_object_del(gui->win);
     }
}

static void
_die_cb(void *data,
        Evas_Object *obj EINA_UNUSED,
        void *info EINA_UNUSED)
{
   s_gui *const gui = data;
   gui_del(gui);
}

void
gui_die(s_gui *gui,
        const char *fmt, ...)
{
   /* Hide the termview */
   Evas_Object *const view = elm_layout_content_unset(gui->layout, "eovim.main.view");
   evas_object_hide(view);

   /* Collect the message */
   char text[512];
   va_list args;
   va_start(args, fmt);
   vsnprintf(text, sizeof(text), fmt, args);
   va_end(args);

   /* Create a button to allow the user to quit */
   Evas_Object *const btn = elm_button_add(gui->layout);
   evas_object_smart_callback_add(btn, "clicked", _die_cb, gui);
   elm_object_text_set(btn, "Quit");

   /* Send a popup to notify the error and terminate */
   Evas_Object *const pop = elm_popup_add(gui->layout);
   elm_object_text_set(pop, text);
   evas_object_smart_callback_add(pop, "dismissed", _die_cb, gui);
   elm_object_part_content_set(pop, "button1", btn);
   evas_object_show(pop);
}

void
gui_resize(s_gui *gui,
           unsigned int cols,
           unsigned int rows)
{
   unsigned int cell_w, cell_h;
   termview_cell_size_get(gui->termview, &cell_w, &cell_h);

   /*
    * We set the resieing step of the window to the size of a cell of the
    * textgrid that is embedded within the termview.
    */
   elm_win_size_step_set(gui->win, (int)cell_w, (int)cell_h);

   /*
    * To resize the gui, we first resize the textgrid with the new amount
    * of columns and rows, then we resize the window. UI widgets will
    * automatically be sized to the window's frame.
    *
    * XXX: This won't work with the tabline!
    */
   const int w = (int)(cols * cell_w);
   const int h = (int)(rows * cell_h);
   evas_object_resize(gui->win, w, h);
   evas_object_size_hint_min_set(gui->termview, w, h);
}

void
gui_resized_confirm(s_gui *gui,
                    unsigned int cols,
                    unsigned int rows)
{
   termview_resized_confirm(gui->termview, cols, rows);
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
        const s_termview_color col =
           termview_color_decompose((uint32_t)color, gui->nvim->true_colors);
        termview_fg_color_set(gui->termview, col.r, col.g, col.b, col.a);
     }
}

void
gui_update_bg(s_gui *gui,
              t_int color)
{
   if (color >= 0)
     {
        const s_termview_color col =
           termview_color_decompose((uint32_t)color, gui->nvim->true_colors);
        gui_bg_color_set(gui, col.r, col.g, col.b, col.a);
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
   if (busy)
     {
        /* Trigger the busy signal only if the gui was not busy */
        if (++gui->busy_count == 1)
          elm_layout_signal_emit(gui->layout, "eovim,busy,on", "eovim");
     }
   else
     {
        /* Stop the busy signal only if the gui has one busy reference */
        if (--gui->busy_count == 0)
          elm_layout_signal_emit(gui->layout, "eovim,busy,off", "eovim");
        if (EINA_UNLIKELY(gui->busy_count < 0))
          {
             ERR("busy count underflowed");
             gui->busy_count = 0;
          }
     }
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

   edje_object_message_send(gui->edje, EDJE_MESSAGE_INT_SET, THEME_MSG_BG, msg);
}

static Evas_Object *
_completion_gl_content_get(void *data,
                           Evas_Object *obj,
                           const char *part EINA_UNUSED)
{
   const s_completion *const compl = data;
   const s_nvim *const nvim = _nvim_get(obj);
   const s_config *const cfg = nvim->config;
   char buf[1024];
   char *ptr = buf;
   size_t size = sizeof(buf);

   /* Prepare the completion text */
   int bytes = snprintf(ptr, size,
                        "<font='%s' font_size=%u align=left>",
                        cfg->font_name, cfg->font_size);
   ptr += bytes; size -= (size_t)bytes;
   if (compl->kind[0] == '\0')
     {
        bytes = snprintf(ptr, size, " %s", compl->word);
     }
   else
     {
        bytes = snprintf(ptr, size, " <hilight>%s</hilight> <b>%s</b>",
                         compl->kind, compl->word);
     }
   ptr += bytes; size -= (size_t)bytes;

   if (compl->menu[0] != '\0')
     snprintf(ptr, size, "<br>%s" ,compl->menu);

   /* Set the completion text */
   Evas_Object *const text = elm_label_add(obj);
   elm_object_text_set(text, buf);
   return text;
}



static void
_completion_sel_cb(void *data,
                   Evas_Object *obj EINA_UNUSED,
                   void *event)
{
   s_gui *const gui = data;
   const Elm_Genlist_Item *const item = event;
   const s_completion *const compl = elm_object_item_data_get(item);

   /* If we have a completion info, we shall display it */
   if (compl->info[0] != '\0')
     {
        edje_object_part_text_set(gui->completion.obj,
                                  "eovim.completion.info", compl->info);
        edje_object_signal_emit(gui->completion.obj,
                                "eovim,completion,info,show", "eovim");
     }

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

   /* Use a string buffer that will hold the input to be passed to neovim */
   Eina_Strbuf *const input = gui->cache;
   eina_strbuf_reset(input);

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
}

void
gui_completion_add(s_gui *gui,
                   s_completion *completion)
{
   Elm_Genlist_Item *const item = elm_genlist_item_append(
      gui->completion.gl, _compl_simple_itc, completion,
      NULL, ELM_GENLIST_ITEM_NONE,
      _completion_sel_cb, gui
   );
   elm_object_item_data_set(item, completion);
   gui->completion.items.count++;
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
                    int selected,
                    unsigned int x,
                    unsigned int y)
{
   /* Before starting to setup the ui, we mark the completion as incoming
    * from neovim */
   gui->completion.event = EINA_TRUE;
   gui->completion.items.at_x = x;
   gui->completion.items.at_y = y;

   Evas_Object *const obj = gui->completion.obj;

   /* Show the completion panel */
   edje_object_signal_emit(obj, "eovim,completion,show", "eovim");

   /* Select the appropriate item */
   if (selected >= 0)
     {
        Elm_Genlist_Item *const sel = _gl_nth_get(gui->completion.gl,
                                                  (unsigned int)selected);
        elm_genlist_item_selected_set(sel, EINA_TRUE);
     }
}

void
gui_completion_hide(s_gui *gui)
{
   Evas_Object *const obj = gui->completion.obj;
   edje_object_signal_emit(obj, "eovim,completion,hide", "eovim");
   evas_object_hide(obj);
}

void
gui_completion_clear(s_gui *gui)
{
   gui->completion.sel = NULL;
   memset(&gui->completion.items, 0, sizeof(gui->completion.items));
   elm_genlist_clear(gui->completion.gl);
}


void
gui_bell_ring(s_gui *gui)
{
   /* Ring the bell, but only if it was not muted */
   if (! gui->nvim->config->mute_bell)
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

static void
_wildmenu_item_del(void *data,
                   Evas_Object *obj EINA_UNUSED)
{
   Eina_Stringshare *const item = data;
   eina_stringshare_del(item);
}

static Evas_Object *
_wildmenu_content_get(void *data,
                      Evas_Object *obj,
                      const char *part EINA_UNUSED)
{
   Eina_Stringshare *const item = data;
   Evas_Object *const wdg = _layout_item_add(obj, "eovim/wildmenu_item");

   elm_object_part_text_set(wdg, "text", item);
   evas_object_show(wdg);
   return wdg;
}

Eina_Bool
gui_init(void)
{
   /* Completion list item class */
   _compl_simple_itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(! _compl_simple_itc))
     {
        CRI("Failed to create genlist item class");
        goto fail;
     }
   _compl_simple_itc->item_style = "full";
   _compl_simple_itc->func.text_get = NULL;
   _compl_simple_itc->func.content_get = _completion_gl_content_get;
   _compl_simple_itc->func.del = _completion_item_del;

   /* Wildmenu list item class */
   _wildmenu_itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(! _wildmenu_itc))
     {
        CRI("Failed to create genlist item class");
        goto free_compl;
     }
   _wildmenu_itc->item_style = "full";
   _wildmenu_itc->func.text_get = NULL;
   _wildmenu_itc->func.content_get = _wildmenu_content_get;
   _wildmenu_itc->func.del = _wildmenu_item_del;

   return EINA_TRUE;

free_compl:
   elm_genlist_item_class_free(_compl_simple_itc);
fail:
   return EINA_FALSE;
}

void
gui_shutdown(void)
{
   elm_genlist_item_class_free(_wildmenu_itc);
   elm_genlist_item_class_free(_compl_simple_itc);
}

void
gui_title_set(s_gui *gui,
              const char *title)
{
   EINA_SAFETY_ON_NULL_RETURN(title);

   /* Set the title to the window, or just "Eovim" if it happens to be empty */
   elm_win_title_set(gui->win, (title[0]) ? title : "Eovim");
}

void
gui_fullscreen_set(s_gui *gui,
                   Eina_Bool fullscreen)
{
   elm_win_fullscreen_set(gui->win, fullscreen);
}

void
gui_mode_update(s_gui *gui,
                Eina_Stringshare *name)
{
   const s_mode *const mode = nvim_named_mode_get(gui->nvim, name);
   termview_cursor_mode_set(gui->termview, mode);
}

void
gui_cmdline_show(s_gui *gui,
                 const char *content,
                 const char *prompt EINA_UNUSED,
                 const char *firstc)
{
   const Edje_Message_String msg = {
      .str = (char *)firstc,
   };
   termview_cursor_visibility_set(gui->termview, EINA_FALSE);
   edje_object_message_send(gui->cmdline.info, EDJE_MESSAGE_STRING,
                            THEME_MSG_CMDLINE_INFO, (void *)(&msg));
   edje_object_part_text_set(gui->cmdline.obj, "eovim.cmdline.text", content);

   /* Show the completion panel */
   edje_object_signal_emit(gui->layout, "eovim,cmdline,show", "eovim");

   _wildmenu_resize(gui);
}

void
gui_cmdline_hide(s_gui *gui)
{
   edje_object_signal_emit(gui->layout, "eovim,cmdline,hide", "eovim");
   termview_cursor_visibility_set(gui->termview, EINA_TRUE);
}

void
gui_cmdline_cursor_pos_set(s_gui *gui,
                           size_t pos)
{
   gui->cmdline.cpos = pos;
   edje_object_part_text_cursor_pos_set(gui->cmdline.obj, "eovim.cmdline.text",
                                        EDJE_CURSOR_MAIN, (int)pos);
}

static inline void
_wildmenu_first_item_acquire(s_gui *gui)
{
   gui->cmdline.sel_item = elm_genlist_first_item_get(gui->cmdline.menu);
   gui->cmdline.sel_index = 0;
}

void
gui_wildmenu_select(s_gui *gui,
                    ssize_t index)
{
   if (index < 0)
     {
        /* Negative: nothing to be selected at all! */
        elm_genlist_item_selected_set(gui->cmdline.sel_item, EINA_FALSE);
     }
   else
     {
        /* The selection has been initiated by neovim */
        gui->cmdline.nvim_sel_event = EINA_TRUE;

        /* At this point, we need to select something, but if we didn't have
         * any item previously selected (this will happen after doing a full
         * circle among the wildmenu items), start selecting the first item in
         * the wildmenu. */
        if ( gui->cmdline.sel_index < 0)
          _wildmenu_first_item_acquire(gui);

        if (index >= gui->cmdline.sel_index)
          {
             /* We select an index that is after the current one */
             const ssize_t nb = index - gui->cmdline.sel_index;
             for (ssize_t i = 0; i < nb; i++)
               {
                  gui->cmdline.sel_item =
                     elm_genlist_item_next_get(gui->cmdline.sel_item);
               }
             elm_genlist_item_selected_set(gui->cmdline.sel_item, EINA_TRUE);
          }
        else /* index < gui->cmdline.sel_index */
          {
             /* We select an index that is before the current one */
             const ssize_t nb = gui->cmdline.sel_index - index;
             for (ssize_t i = 0; i < nb; i++)
               {
                  gui->cmdline.sel_item =
                     elm_genlist_item_prev_get(gui->cmdline.sel_item);
               }
             elm_genlist_item_selected_set(gui->cmdline.sel_item, EINA_TRUE);
          }
     }

   /* Bring in the item */
   elm_genlist_item_bring_in(gui->cmdline.sel_item, ELM_GENLIST_ITEM_SCROLLTO_IN);

   /* Finally, we update the index in cache, and select the item */
   gui->cmdline.sel_index = index;
}

void
gui_wildmenu_show(s_gui *gui)
{
   _wildmenu_resize(gui);

   /* The first selected item will be the first one in the genlist */
   _wildmenu_first_item_acquire(gui);
   gui->cmdline.nvim_sel_event = EINA_TRUE;
   elm_genlist_item_selected_set(gui->cmdline.sel_item, EINA_TRUE);
}

static void
_wildmenu_sel_cb(void *data,
                 Evas_Object *obj EINA_UNUSED,
                 void *event)
{
   s_gui *const gui = data;
   const Elm_Genlist_Item *const item = event;

   /* If an item was selected, but at the initiative of neovim, we will just
    * discard the event, to notify we have processed it, but will stop right
    * here, right now, because this function is made to handle user selection
    */
   if (gui->cmdline.nvim_sel_event)
     {
        gui->cmdline.nvim_sel_event = EINA_FALSE;
        return;
     }

   /* Use a string buffer that will hold the input to be passed to neovim */
   Eina_Strbuf *const input = gui->cache;
   eina_strbuf_reset(input);

   const ssize_t item_idx = (ssize_t)elm_genlist_item_index_get(item);
   const ssize_t sel_idx = (gui->cmdline.sel_index >= 0)
      ? gui->cmdline.sel_index
      : 1;

   /* To make neovim select the wildmenu item, we will write N times
    * <C-n> or <C-p> from the current index to the target one. When done,
    * we will insert <CR> to make the selection apply */
   if (sel_idx < item_idx)
     {
        for (ssize_t i = sel_idx; i < item_idx; i++)
          eina_strbuf_append_length(input, "<C-n>", 5);
     }
   else /* sel_idx >= item_idx */
     {
        for (ssize_t i = item_idx; i >= sel_idx; i--)
          eina_strbuf_append_length(input, "<C-p>", 5);
     }

   /* Send a signal to end the wildmenu selection */
   eina_strbuf_append_length(input, "<CR>", 4);

   /* Pass all these data to neovim and cleanup behind us */
   nvim_api_input(gui->nvim, eina_strbuf_string_get(input),
                  (unsigned int)eina_strbuf_length_get(input));
   printf("Wildmenu selection: %zi vs %zi\n", item_idx, sel_idx);
}

void
gui_wildmenu_append(s_gui *gui,
                    Eina_Stringshare *item)
{
   Elm_Genlist_Item *const wild_item = elm_genlist_item_append(
      gui->cmdline.menu, _wildmenu_itc, item,
      NULL, ELM_GENLIST_ITEM_NONE,
      _wildmenu_sel_cb, gui
   );
   elm_object_item_data_set(wild_item, (void *)item);

   /* And one more item! */
   gui->cmdline.items_count++;
}

void
gui_wildmenu_clear(s_gui *gui)
{
   gui->cmdline.items_count = 0;
   gui->cmdline.sel_item = NULL;
   gui->cmdline.sel_index = -1;
   elm_genlist_clear(gui->cmdline.menu);

   /* Give a height of zero to the area that contains the items, so it will
    * visually disappear from the screen. */
   int menu_w;
   evas_object_geometry_get(gui->cmdline.table, NULL, NULL, &menu_w, NULL);
   evas_object_size_hint_min_set(gui->cmdline.spacer, menu_w, 0);
}

static void
_item_realized(void *data,
               Evas_Object *obj EINA_UNUSED,
               void *info EINA_UNUSED)
{
   /* This is a deferred call to _wildmenu_resize(). When done, unregister the
    * callback! It does not make sense to keep it around.
    */
   s_gui *const gui = data;
   _wildmenu_resize(gui);
   evas_object_smart_callback_del_full(obj, "realized", _item_realized, data);
}

static void
_wildmenu_resize(s_gui *gui)
{
   Evas_Object *const gl = gui->cmdline.menu;

   /* If we have no items, don't bother to resize! */
   if (! gui->cmdline.items_count) { return; }

   Eina_List *const realized = elm_genlist_realized_items_get(gl);
   if (! realized)
     {
        /* No items have yet been drawn in the genlist. Too bad, attach a
         * callback so we will know when this will be done, and then go away!
         */
        evas_object_smart_callback_add(gl, "realized", _item_realized, gui);
        return;
     }

   Evas_Object *const first = elm_object_item_track(eina_list_data_get(realized));
   int item_height;
   evas_object_geometry_get(first, NULL, NULL, NULL, &item_height);

   int win_h, info_x, info_y, info_h, menu_w;
   evas_object_geometry_get(gui->win, NULL, NULL, NULL, &win_h);
   evas_object_geometry_get(gui->cmdline.info, &info_x, &info_y, NULL, &info_h);
   evas_object_geometry_get(gui->cmdline.table, NULL, NULL, &menu_w, NULL);

   const int height = item_height * (int)(gui->cmdline.items_count) + 2;
   const int max_height = (int)((float)win_h * 0.8f); /* 80% of the window's height */

   if (height <= max_height)
     evas_object_size_hint_min_set(gui->cmdline.spacer, menu_w, height);
   else
     evas_object_size_hint_min_set(gui->cmdline.spacer, menu_w, max_height);
}
