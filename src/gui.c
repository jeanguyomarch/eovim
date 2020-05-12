/*
 * Copyright (c) 2017-2019 Jean Guyomarc'h
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
#include "eovim/nvim_completion.h"
#include "eovim/nvim.h"
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
   THEME_MSG_COMPLETION_KIND = 2,
} e_theme_msg;

typedef struct
{
   Eina_Stringshare *name;
   unsigned int id;
} s_tab;

static Elm_Genlist_Item_Class *_compl_itc = NULL;
static Elm_Genlist_Item_Class *_wildmenu_itc = NULL;

static void _wildmenu_resize(s_gui *gui);
static void _tabs_shown_cb(void *data, Evas_Object *obj, const char *emission, const char *source);

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

static void
_focus_out_cb(void *data,
              Evas_Object *obj EINA_UNUSED,
              void *event EINA_UNUSED)
{
   s_gui *const gui = data;
   evas_object_focus_set(gui->termview, EINA_FALSE);
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
_win_close_cb(void *data,
              Evas_Object *obj EINA_UNUSED,
              void *info EINA_UNUSED)
{
   /* When closing the neovim window, send to neovim the :quitall! command
    * so it will be naturally terminated.
    *
    * TODO: see if they are unsaved files ...
    */
   s_nvim *const nvim = data;
   const char cmd[] = ":quitall!";
   nvim_api_command(nvim, cmd, sizeof(cmd) - 1, NULL, NULL);
}

Eina_Bool
gui_add(s_gui *gui,
        s_nvim *nvim)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(gui, EINA_FALSE);

   const char *const edje_file = main_edje_file_get();
   Evas_Object *o;
   gui->nvim = nvim;

   gui->cache = eina_strbuf_new();
   if (EINA_UNLIKELY(! gui->cache))
     {
        CRI("Failed to create cache string buffer");
        goto fail;
     }

   gui->tabs = eina_inarray_new(sizeof(unsigned int), 4);
   if (EINA_UNLIKELY(! gui->tabs))
     {
        CRI("Failed to create inline array");
        goto fail;
     }

   /* Window setup */
   gui->win = elm_win_util_standard_add("eovim", "Eovim");
   elm_win_autodel_set(gui->win, EINA_TRUE);
   evas_object_smart_callback_add(gui->win, "delete,request", _win_close_cb, nvim);
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
   elm_layout_signal_callback_add(gui->layout, "eovim,tabs,shown", "eovim",
                                  _tabs_shown_cb, nvim);
   elm_win_resize_object_add(gui->win, gui->layout);
   evas_object_smart_callback_add(gui->win, "focus,in", _focus_in_cb, gui);
   evas_object_smart_callback_add(gui->win, "focus,out", _focus_out_cb, gui);

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
   evas_object_size_hint_align_set(o, 0.0, EVAS_HINT_FILL);
   elm_scroller_policy_set(o, ELM_SCROLLER_POLICY_AUTO, ELM_SCROLLER_POLICY_OFF);
   elm_object_tree_focus_allow_set(o, EINA_FALSE);
   // FIXME Okay, this is fishy. I tell the genlist not to be homogenous (which
   // is that I think I want), items are screwed up horizontally. Maybe a bug
   // in elm_genlist???
   // FIXME elm_genlist_homogeneous_set(o, EINA_TRUE);
   evas_object_data_set(o, _nvim_data_key, nvim);
   edje_object_part_swallow(gui->completion.obj, "eovim.completion", o);
   evas_object_show(o);

   /* ========================================================================
    * Termview GUI objects
    * ===================================================================== */

   gui->termview = termview_add(gui->layout, nvim);
   termview_font_set(gui->termview, "Sans Mono", 14);
   elm_layout_content_set(gui->layout, "eovim.main.view", gui->termview);

   /* ========================================================================
    * Command-Line GUI objects
    * ===================================================================== */

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
   evas_object_show(o);

   /* ========================================================================
    * Finalize GUI
    * ===================================================================== */

   gui_cmdline_hide(gui);
   gui_wildmenu_clear(gui);
   evas_object_show(gui->termview);
   evas_object_show(gui->layout);
   evas_object_show(gui->win);
   return EINA_TRUE;

fail:
   if (gui->cache) eina_strbuf_free(gui->cache);
   if (gui->tabs) eina_inarray_free(gui->tabs);
   evas_object_del(gui->win);
   return EINA_FALSE;
}

void
gui_del(s_gui *gui)
{
   EINA_SAFETY_ON_NULL_RETURN(gui);
   eina_inarray_free(gui->tabs);
   eina_strbuf_free(gui->cache);
   evas_object_del(gui->win);
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

   /* XXX This is a bit of a hack. I'm not really sure why we have the issue,
    * but there is a little "jitter" when resizing the window:
    *   1) the window is resized to the expected size
    *   2) it is resized back (on the X-axis) to "a bit less".
    *
    * This makes the --geometry parameter create a window that does not respect
    * the initial size. I'm sure the problem is somewhere here, but I haven't
    * found its root yet. So, as a quickfix (which will probably stay for a
    * while...) prevent the window from being resized down until we have
    * effectively resize the window. When done, we will remove this barrier
    * (see gui_resized_confirm()).
    */
   evas_object_size_hint_min_set(gui->win, w, h);
}

void
gui_resized_confirm(s_gui *gui,
                    unsigned int cols,
                    unsigned int rows)
{
   termview_resized_confirm(gui->termview, cols, rows);

   /* see gui_resize() */
   evas_object_size_hint_min_set(gui->win, -1, -1);
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

/* TODO: special colors */
void
gui_update_sp(s_gui *gui EINA_UNUSED,
              t_int color)
{
   if (color >= 0)
     {
        DBG("Unimplemented");
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

static inline Evas_Object *
_compl_text_set(Evas_Object *layout,
                const s_gui *gui,
                const char *text)
{
   Evas_Object *const edje = elm_layout_edje_get(layout);
   edje_object_text_class_set(edje, "completion_text",
                              gui->font.name, (int)gui->font.size);
   elm_layout_text_set(layout, "text", text);
   return edje;
}

static void
_spacer_set(Evas_Object *obj,
            size_t char_w,
            size_t char_h,
            size_t len)
{
   /* This function set the minimum size for a spacer, and initializes common
    * parameters, such as invisible color and normal size hints. */
   const int size = (int)char_w * (int)len;
   evas_object_size_hint_weight_set(obj, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(obj, size, (int)char_h);
   evas_object_color_set(obj, 0, 0, 0, 0);
   evas_object_show(obj);
}

static Evas_Object *
_compl_content_get(void *data,
                   Evas_Object *obj,
                   const char *part)
{
   const s_completion *const compl = data;
   s_nvim *const nvim = _nvim_get(obj);
   s_gui *const gui = (s_gui *)(&nvim->gui);
   Evas_Object *o;

   if (! eina_streq(part, "elm.swallow.content")) { return NULL; }

   /* Each element of the completion is a table that is designed as follows:
    *
    * +------+------------+-----------------+
    * | kind |<----- Type | Completion ---->|
    * +------+------------+-----------------+
    *
    */

   Evas *const evas = evas_object_evas_get(obj);

   /* XXX: I don't know how this will work with large glyphs... Pretty sure we
    * are doomed. Hopefully this should never happen. */
   unsigned int char_w, char_h;
   termview_cell_size_get(gui->termview, &char_w, &char_h);

   /* Table that will hold the completion elements */
   Evas_Object *const table = o = elm_table_add(obj);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_padding_set(o, (int)char_w * 2, 0);
   evas_object_show(o);

   /* Column 0: the item giving the KIND of the completion. If we got an empty
    * string, then we don't bother with it. Nothing will be shown. */
   if (compl->kind[0] != '\0')
     {
        Evas_Object *const kind = _layout_item_add(table, "eovim/completion/kind");
        Evas_Object *const kind_edje = _compl_text_set(kind, gui, compl->kind);
        evas_object_show(kind);
        elm_table_pack(table, kind, 0, 0, 1, 1);

        /* For the 'kind' item, we will send a message to the edje theme, to update
         * the background of the text, and maybe change it as well */
        const Edje_Message_String msg = { .str = (char *)compl->kind };
        edje_object_message_send(kind_edje, EDJE_MESSAGE_STRING,
                                 THEME_MSG_COMPLETION_KIND, (void *)(&msg));
     }

   /* Column 1: spacer to make all the elements the same size */
   o = evas_object_rectangle_add(evas);
   _spacer_set(o, char_w, char_h, gui->completion.max_type_len);
   elm_table_pack(table, o, 1, 0, 1, 1);

   /* Column 1: type part, above the spacer */
   o = _layout_item_add(table, "eovim/completion/type");
   _compl_text_set(o, gui, compl->menu);
   elm_table_pack(table, o, 1, 0, 1, 1);
   evas_object_show(o);

   /* Column 2: spacer to make all the elements the same size */
   o = evas_object_rectangle_add(evas);
   _spacer_set(o, char_w, char_h, gui->completion.max_word_len);
   elm_table_pack(table, o, 2, 0, 1, 1);

   /* Column 2: completion contents */
   o = _layout_item_add(table, "eovim/completion/word");
   _compl_text_set(o, gui, compl->word);
   elm_table_pack(table, o, 2, 0, 1, 1);
   evas_object_show(o);

   return table;
}



static void
_completion_sel_cb(void *data,
                   Evas_Object *obj EINA_UNUSED,
                   void *event)
{
   s_gui *const gui = data;
   const Elm_Object_Item *const item = event;

   /* If the completion.event is set, the item was selected because of a
    * neovim event, not because the user did clic on the item.
    * So we reset the event, and abort the function right here */
   if (gui->completion.nvim_sel_event)
     {
        gui->completion.nvim_sel_event = EINA_FALSE;
        return;
     }

   /* Get the indexes of the currently selected item and the item we have
    * clicked on and we want to insert. */
   const int sel_idx = gui->completion.sel
      ? elm_genlist_item_index_get(gui->completion.sel)
      : 0; /* No item selected? Take the first one */
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
        for (int i = compl_idx; i < sel_idx; i++)
          eina_strbuf_append_length(input, "<C-p>", 5);
     }
   /* Send a signal to end the completion */
   eina_strbuf_append_length(input, "<C-y>", 5);

   /* Pass all these data to neovim and cleanup behind us */
   nvim_api_input(gui->nvim, eina_strbuf_string_get(input),
                  (unsigned int)eina_strbuf_length_get(input));
}

void
gui_completion_prepare(s_gui *gui, size_t items)
{
   gui->completion.items_count = items;
}

void
gui_completion_add(s_gui *gui,
                   s_completion *compl)
{
   elm_genlist_item_append(
      gui->completion.gl, _compl_itc, compl,
      NULL, ELM_GENLIST_ITEM_NONE,
      _completion_sel_cb, gui
   );
}

static Elm_Object_Item *
_gl_nth_get(const Evas_Object *gl,
            unsigned int index)
{
   /*
    * Find the N-th element in a genlist. We use a dichotomic search to
    * lower the algorithmic completxity.
    */
   const unsigned int items = elm_genlist_items_count(gl);
   const unsigned int half = items / 2;
   Elm_Object_Item *ret = NULL;

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
        Elm_Object_Item *const selected = elm_genlist_selected_item_get(gl);
        gui->completion.nvim_sel_event = EINA_FALSE;
        if (selected)
          elm_genlist_item_selected_set(selected, EINA_FALSE);
        gui->completion.sel = NULL;
     }
   else /* index is >= 0, we can safely cast it as unsigned */
     {
        Elm_Object_Item *const item = _gl_nth_get(gl, (unsigned int)index);
        gui->completion.nvim_sel_event = EINA_TRUE;
        elm_genlist_item_selected_set(item, EINA_TRUE);
        elm_genlist_item_bring_in(item, ELM_GENLIST_ITEM_SCROLLTO_IN);
        gui->completion.sel = item;
     }
}

void
gui_completion_show(s_gui *gui,
                    size_t max_word_len,
                    size_t max_menu_len,
                    int selected,
                    unsigned int x,
                    unsigned int y)
{
   gui->completion.max_word_len = max_word_len;
   gui->completion.max_type_len = max_menu_len;

   /*
    * When showing the completion, we will also proceed to a resizing
    * operation, to make the completion pop-up fit the screen best.
    *
    * So this function works in two parts:
    * - calculate the best size for the popup
    * - ensure the visibility of the completion popup
    */

   /* Get the absolute position where the completion panel was triggerred */
   int px, py;
   termview_cell_to_coords(gui->termview, x, y, &px, &py);

   /* Select the appropriate item */
   if (selected >= 0)
     {
        /* Selection is at the initiative of neovim */
        gui->completion.nvim_sel_event = EINA_TRUE;

        Elm_Object_Item *const sel = _gl_nth_get(gui->completion.gl,
                                                  (unsigned int)selected);
        elm_genlist_item_selected_set(sel, EINA_TRUE);
     }

   /*
    * Mhhh... okay... this is a bit experimental, but I think it will do.
    * Each element of the completion genlist has the same height, and a
    * known maximum width.
    *
    * So we will set the height as being the count of elements times the
    * height of one element. If it is greater than 60% of the window, or
    * cannot fit, it will be clamped.
    *
    * For the width, we need to calculate it. It is a bit wacky. We rely on
    * the font being monospace and not having fancy non-latin characters.
    * We know the maximum lengths of the strings to fit in a completion
    * item, so we multiply this length with the size of a termview cell, which
    * has the X-advance of glyphs. With Unicode characters, it will certainly
    * be wrong.
    */

   /* Termview dimensions */
   int tv_orig_y, tv_width, tv_height;
   evas_object_geometry_get(gui->termview, NULL, &tv_orig_y, &tv_width, &tv_height);

   /* Cell' size */
   int char_w, char_h;
   termview_cell_size_get(gui->termview,
                          (unsigned int *)(&char_w), (unsigned int *)(&char_h));

   /* This is the ideal width.
    *
    * Kind takes 1 char
    * Kind and type are separated by 1 char
    * Type has a max length.
    * Word has a max length.
    * Type and work are separated by 2 char
    * We add 2 extra chars for good measure.
    */
   const int ideal_width =
      (int)(gui->completion.max_type_len + gui->completion.max_word_len + 6) *
      char_w;

   /* This is the ideal height. We add 7 because it looks nice.
    * Maybe this will change with various fonts... that's lame. */
   const int ideal_height =
      (char_h + 7) *
      (int)gui->completion.items_count;

   /* Window's size */
   int win_w, win_h;
   evas_object_geometry_get(gui->win, NULL, NULL, &win_w, &win_h);

   /* We will not make the popup larger than 60% of the window's height */
   const int max_width = tv_width;
   const int max_height = (int)((float)win_h * 0.6f);
   int width = MIN(ideal_width, max_width);
   int height = MIN(ideal_height, max_height);

   /* Sizing */
   int pos_y;
   const int h_offset = 7;
   const int w_offset = 7;
   const int cell_h = char_h + h_offset; /* Line height a little offset */

   /* Ensure we have room to place the popup. We try to find the best place
    * that fits with the most completion contents. */
   if (py + cell_h + height < tv_height)
     {
        /* (1) If there is room below the insertion point, try to place the
         * completion panel on the line BELOW. */
        pos_y = cell_h + py;
     }
   else if (py - height > 0)
     {
        /* (2) If no room below, try to place the completion panel ABOVE */
        pos_y = py - height - h_offset;
     }
   else
     {
        /* (3) Oh noo, perfect size does not fit anywhere. Take the side where
         * we have the most room and fill it completely */
        if (py > tv_height / 2) /* Place it above */
          {
             height = py - tv_orig_y;
             pos_y = py - height - h_offset;
          }
        else /* Place it below */
          {
             height = tv_height - (py + cell_h);
             pos_y = py + cell_h;
          }
     }

   /* If the width is too big to fit, we reduce it */
   if (px + width > tv_width)
     {
        px = tv_width - width + w_offset;
        if (px < 0) px = w_offset;
        width = tv_width - px - w_offset;
     }

   /* Show the completion panel: we move it where the completion was, show it,
    * set make sure the **visible** part of the genlist fits it height,
    * and resize the final object
    */
   Evas_Object *const obj = gui->completion.obj;
   edje_object_signal_emit(obj, "eovim,completion,show", "eovim");
   evas_object_move(obj, px, pos_y);
   evas_object_resize(obj, width, height);
}

void
gui_completion_hide(s_gui *gui)
{
   Evas_Object *const obj = gui->completion.obj;
   edje_object_signal_emit(obj, "eovim,completion,hide", "eovim");
}

void
gui_completion_clear(s_gui *gui)
{
   gui->completion.sel = NULL;

   /* Reset the maximum sizes for the next instance of the completion */
   gui->completion.max_type_len = 0;
   gui->completion.max_word_len = 0;
   gui->completion.items_count = 0;
   elm_genlist_clear(gui->completion.gl);
}


void
gui_bell_ring(s_gui *gui)
{
   /* Ring the bell, but only if it was not muted */
   if (! gui->nvim->config->mute_bell)
     elm_layout_signal_emit(gui->layout, "eovim,bell,ring", "eovim");
}

void gui_ready_set(s_gui *gui)
{
   elm_layout_signal_emit(gui->layout, "eovim,ready", "eovim");
}

static void
_compl_item_del(void *data,
                Evas_Object *obj EINA_UNUSED)
{
   s_completion *const compl = data;
   nvim_completion_free(compl);
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
   Evas_Object *const wdg = _layout_item_add(obj, "eovim/wildmenu/item");

   elm_object_part_text_set(wdg, "text", item);
   evas_object_show(wdg);
   return wdg;
}

Eina_Bool
gui_init(void)
{
   /* Completion list item class */
   _compl_itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(! _compl_itc))
     {
        CRI("Failed to create genlist item class");
        goto fail;
     }
   _compl_itc->item_style = "full";
   _compl_itc->func.text_get = NULL;
   _compl_itc->func.content_get = _compl_content_get;
   _compl_itc->func.del = _compl_item_del;

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
   elm_genlist_item_class_free(_compl_itc);
fail:
   return EINA_FALSE;
}

void
gui_shutdown(void)
{
   elm_genlist_item_class_free(_wildmenu_itc);
   elm_genlist_item_class_free(_compl_itc);
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
gui_maximized_set(s_gui *gui,
                  Eina_Bool maximized)
{
   elm_win_maximized_set(gui->win, maximized);
}

void
gui_mode_update(s_gui *gui, const s_mode *mode)
{
   termview_cursor_mode_set(gui->termview, mode);
}

void
gui_cmdline_show(s_gui *gui,
                 const char *content,
                 const char *prompt,
                 const char *firstc)
{
   EINA_SAFETY_ON_NULL_RETURN(firstc);

   const Eina_Bool use_prompt = (firstc[0] == '\0');
   const char *const prompt_signal = (use_prompt)
     ? "eovim,cmdline,prompt,custom"
     : "eovim,cmdline,prompt,builtin";
   const Edje_Message_String msg = {
      .str = (char *) (use_prompt ? prompt : firstc),
   };

   termview_cursor_visibility_set(gui->termview, EINA_FALSE);
   edje_object_message_send(gui->edje, EDJE_MESSAGE_STRING,
                            THEME_MSG_CMDLINE_INFO, (void *)(&msg));
   edje_object_part_text_unescaped_set(gui->edje,
                                       "eovim.cmdline:eovim.cmdline.text",
                                       content);

   /* Show the completion panel */
   elm_layout_signal_emit(gui->layout, prompt_signal, "eovim");
   elm_layout_signal_emit(gui->layout, "eovim,cmdline,show", "eovim");

   _wildmenu_resize(gui);
}

void
gui_cmdline_hide(s_gui *gui)
{
   elm_layout_signal_emit(gui->layout, "eovim,cmdline,hide", "eovim");
   termview_cursor_visibility_set(gui->termview, EINA_TRUE);
}

void
gui_cmdline_cursor_pos_set(s_gui *gui,
                           size_t pos)
{
   gui->cmdline.cpos = pos;
   edje_object_part_text_cursor_pos_set(gui->edje,
                                       "eovim.cmdline:eovim.cmdline.text",
                                        EDJE_CURSOR_MAIN, (int)pos);
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
        if (gui->cmdline.sel_index < 0)
          {
             gui->cmdline.sel_item =
                elm_genlist_first_item_get(gui->cmdline.menu);
             gui->cmdline.sel_index = 0;
          }

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
}

static void
_wildmenu_sel_cb(void *data,
                 Evas_Object *obj EINA_UNUSED,
                 void *event)
{
   s_gui *const gui = data;
   const Elm_Object_Item *const item = event;

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

   /* For some strange reason, genlist indexing starts at 1! WTF. */
   const int item_idx = elm_genlist_item_index_get(item) - 1;
   const int sel_idx = (gui->cmdline.sel_index >= 0)
      ? (int)gui->cmdline.sel_index
      : 0; /* No item selected? Take the first one */

   /* No item selected? Initiate the completion. */
   if (! gui->cmdline.sel_item)
     eina_strbuf_append_length(input, "<C-n>", 5);

   /* To make neovim select the wildmenu item, we will write N times
    * <C-n> or <C-p> from the current index to the target one. When done,
    * we will insert <CR> to make the selection apply */
   if (sel_idx < item_idx)
     {
        for (int i = sel_idx; i < item_idx; i++)
          eina_strbuf_append_length(input, "<C-n>", 5);
     }
   else /* sel_idx >= item_idx */
     {
        for (int i = item_idx; i < sel_idx; i++)
          eina_strbuf_append_length(input, "<C-p>", 5);
     }

   /* Send a signal to end the wildmenu selection */
   eina_strbuf_append_length(input, "<CR>", 4);

   /* Pass all these data to neovim and cleanup behind us */
   nvim_api_input(gui->nvim, eina_strbuf_string_get(input),
                  (unsigned int)eina_strbuf_length_get(input));
}

void
gui_wildmenu_append(s_gui *gui,
                    Eina_Stringshare *item)
{
   Elm_Object_Item *const wild_item = elm_genlist_item_append(
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
_wild_item_realized(void *data,
                    Evas_Object *obj,
                    void *info EINA_UNUSED)
{
   /* This is a deferred call to _wildmenu_resize(). When done, unregister the
    * callback! It does not make sense to keep it around.
    */
   s_gui *const gui = data;
   _wildmenu_resize(gui);
   evas_object_smart_callback_del_full(obj, "realized", _wild_item_realized, data);
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
        evas_object_smart_callback_add(gl, "realized", _wild_item_realized, gui);
        return;
     }

   Evas_Object *const first = eina_list_data_get(realized);
   Evas_Object *const track = elm_object_item_track(first);
   int item_height;
   evas_object_geometry_get(track, NULL, NULL, NULL, &item_height);

   int win_h, menu_w;
   evas_object_geometry_get(gui->win, NULL, NULL, NULL, &win_h);
   evas_object_geometry_get(gui->cmdline.table, NULL, NULL, &menu_w, NULL);

   const int height = item_height * (int)(gui->cmdline.items_count) + 2;
   const int max_height = (int)((float)win_h * 0.8f); /* 80% of the window's height */

   if (height <= max_height)
     evas_object_size_hint_min_set(gui->cmdline.spacer, menu_w, height);
   else
     evas_object_size_hint_min_set(gui->cmdline.spacer, menu_w, max_height);
   elm_object_item_untrack(first);
   eina_list_free(realized);
}


/*============================================================================*
 *                                  TAB LINE                                  *
 *============================================================================*/

static void
_tabs_shown_cb(void *data,
               Evas_Object *obj EINA_UNUSED,
               const char *emission EINA_UNUSED,
               const char *source EINA_UNUSED)
{
   /* After the tabs have been shown, we need to re-evaluate the layout,
    * so the size taken by the tabline will impact the main view */
   s_nvim *const nvim = data;
   s_gui *const gui = &nvim->gui;

   elm_layout_sizing_eval(gui->layout);
}

static void
_tab_close_cb(void *data,
              Evas_Object *obj EINA_UNUSED,
              const char *sig EINA_UNUSED,
              const char *src EINA_UNUSED)
{
   s_gui *const gui = data;
   s_nvim *const nvim = gui->nvim;
   const char cmd[] = ":tabclose";
   nvim_api_command(nvim, cmd, sizeof(cmd) - 1, NULL, NULL);
}

static void
_tab_activate_cb(void *data,
                 Evas_Object *obj,
                 const char *sig EINA_UNUSED,
                 const char *src EINA_UNUSED)
{
   s_gui *const gui = data;

   /* The tab ID is stored as a raw value in a pointer field */
   const unsigned int id =
      (unsigned int)(uintptr_t)evas_object_data_get(obj, "tab_id");

   /* If the tab is already activated, do not activate it more */
   if (id == gui->active_tab)
     return;

   /* Find the tab index. This is more complex than it should be, but since
    * neovim only gives us its internal IDs, we have to convert them into the
    * tab IDs as they are ordered on screen (e.g from left to right).
    *
    * We go through the list of tabs, to find the current index we are on and
    * the index of the tab to be activated.
    */
   unsigned int *it;
   unsigned int active_index = UINT_MAX;
   unsigned int tab_index = UINT_MAX;
   EINA_INARRAY_FOREACH(gui->tabs, it)
     {
        /* When found a candidate, evaluate the index by some pointer
         * arithmetic.  This avoids to keep around a counter. At this
         * point, gui->active_tabs != id. */
        if (*it == id)
          tab_index = (unsigned)(it - (unsigned int*)gui->tabs->members);
        else if (*it == gui->active_tab)
          active_index = (unsigned)(it - (unsigned int*)gui->tabs->members);
     }
   if (EINA_UNLIKELY((tab_index == UINT_MAX) || (active_index == UINT_MAX)))
     {
        CRI("Something went wrong while finding the tab index: %u, %u",
            tab_index, active_index);
        return;
     }

   /* Compose the command to select the tab. See :help tabnext. */
   const int diff = (int)active_index - (int)tab_index;
   char cmd[32];
   const int bytes = snprintf(
      cmd, sizeof(cmd), ":%c%utabnext",
      (diff < 0) ? '+' : '-',
      (diff < 0) ? -diff : diff
   );
   nvim_api_command(gui->nvim, cmd, (size_t)bytes, NULL, NULL);
}

void
gui_tabs_reset(s_gui *gui)
{
   gui->active_tab = 0;
   eina_inarray_flush(gui->tabs);
   edje_object_part_box_remove_all(gui->edje, "eovim.tabline", EINA_TRUE);
}

void
gui_tabs_show(s_gui *gui)
{
   elm_layout_signal_emit(gui->layout, "eovim,tabs,show", "eovim");
}

void
gui_tabs_hide(s_gui *gui)
{
   elm_layout_signal_emit(gui->layout, "eovim,tabs,hide", "eovim");
}

void
gui_tabs_add(s_gui *gui,
             const char *name,
             unsigned int id,
             Eina_Bool active)
{
   Evas *const evas = evas_object_evas_get(gui->layout);

   /* Register the current tab */
   eina_inarray_push(gui->tabs, &id);

   Evas_Object *const edje = edje_object_add(evas);
   evas_object_data_set(edje, "tab_id", (void *)(uintptr_t)id);
   edje_object_file_set(edje, main_edje_file_get(), "eovim/tab");
   edje_object_signal_callback_add(edje, "tab,close", "eovim",
                                   _tab_close_cb, gui);
   edje_object_signal_callback_add(edje, "tab,activate", "eovim",
                                   _tab_activate_cb, gui);
   evas_object_size_hint_align_set(edje, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(edje, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   edje_object_part_text_set(edje, "eovim.tab.title", name);
   evas_object_show(edje);

   if (active)
     {
        edje_object_signal_emit(edje, "eovim,tab,activate", "eovim");
        gui->active_tab = id;
     }
   edje_object_part_box_append(gui->edje, "eovim.tabline", edje);
}

void
gui_caps_lock_alert(s_gui *gui)
{
   if (! gui->capslock_warning)
     {
        /* Don't show the capslock alert in theme if deactivated */
        const s_config *const cfg = gui->nvim->config;
        if (cfg->alert_capslock)
          elm_layout_signal_emit(gui->layout, "eovim,capslock,on", "eovim");

        /* Tell neovim we activated caps lock */
        nvim_helper_autocmd_do(gui->nvim, "EovimCapsLockOn");
        gui->capslock_warning = EINA_TRUE;
     }
}

void
gui_caps_lock_dismiss(s_gui *gui)
{
   if (gui->capslock_warning)
     {
        /* Don't hide the capslock alert in theme if deactivated */
        const s_config *const cfg = gui->nvim->config;
        if (cfg->alert_capslock)
          elm_layout_signal_emit(gui->layout, "eovim,capslock,off", "eovim");

        /* Tell neovim we deactivated caps lock */
        nvim_helper_autocmd_do(gui->nvim, "EovimCapsLockOff");
        gui->capslock_warning = EINA_FALSE;
     }
}

Eina_Bool
gui_caps_lock_warning_get(const s_gui *gui)
{
   return gui->capslock_warning;
}
