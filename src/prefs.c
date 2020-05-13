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

#include "eovim/prefs.h"
#include "eovim/config.h"
#include "eovim/nvim.h"
#include "eovim/nvim_api.h"
#include "eovim/plugin.h"
#include "eovim/main.h"
#include "eovim/log.h"
#include "eovim/gui.h"
#include "contrib/contrib.h"
#include <Elementary.h>

static Elm_Genlist_Item_Class *_plug_itc = NULL;
static const char *const _nvim_data_key = "nvim";

static inline s_nvim *
_nvim_get(const Evas_Object *obj)
{
   return evas_object_data_get(obj, _nvim_data_key);
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

/*============================================================================*
 *                          Cursor Reaction Handling                          *
 *============================================================================*/

static void
_key_react_changed_cb(void *data,
                      Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   s_gui *const gui = data;
   const Eina_Bool react = elm_check_state_get(obj);
   config_key_react_set(gui->nvim->config, react);
}

static Evas_Object *
_config_key_react_add(s_gui *gui,
                      Evas_Object *parent)
{
   const s_config *const config = gui->nvim->config;

   /* Frame container */
   Evas_Object *const f = _frame_add(parent, "Cursor Settings");

   /* Checkbox */
   Evas_Object *const chk = elm_check_add(f);
   evas_object_size_hint_align_set(chk, 0.0, 0.0);
   elm_object_text_set(chk, "React to key presses");
   elm_check_state_set(chk, config->key_react);
   evas_object_smart_callback_add(chk, "changed", _key_react_changed_cb, gui);
   evas_object_show(chk);

   elm_object_content_set(f, chk);
   return f;
}

/*============================================================================*
 *                            Bell Config Handling                            *
 *============================================================================*/

static void
_bell_mute_changed_cb(void *data,
                      Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   s_gui *const gui = data;
   const Eina_Bool mute = elm_check_state_get(obj);
   config_bell_mute_set(gui->nvim->config, mute);
}

static Evas_Object *
_config_bell_add(s_gui *gui,
                 Evas_Object *parent)
{
   const s_config *const config = gui->nvim->config;

   /* Frame container */
   Evas_Object *const f = _frame_add(parent, "Bell Settings");

   /* Settings box */
   Evas_Object *const box = elm_box_add(f);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, 0.0);
   evas_object_show(box);

   /* Bell mute checkbox */
   Evas_Object *const mute = elm_check_add(box);
   evas_object_size_hint_align_set(mute, 0.0, 0.0);
   elm_object_text_set(mute, "Mute the audible bell");
   elm_check_state_set(mute, config->mute_bell);
   evas_object_smart_callback_add(mute, "changed", _bell_mute_changed_cb, gui);
   evas_object_show(mute);
   elm_box_pack_end(box, mute);

   elm_object_content_set(f, box);
   return f;
}

/*============================================================================*
 *                      Caps Lock Warning Config Handling                     *
 *============================================================================*/

static void
_caps_lock_changed_cb(void *data,
                      Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   s_gui *const gui = data;
   const Eina_Bool alert = elm_check_state_get(obj);
   config_caps_lock_alert_set(gui->nvim->config, alert);

   /* If we have just disabled alerts in theme, but an alert was currently
    * running, we shall stop it right now.
    * Conversely, if we enable back alerts and there is an alert pending,
    * trigger the alert animation. */
   if (gui_caps_lock_warning_get(gui))
     {
        const char *const sig = (alert)
          ? "eovim,capslock,on" : "eovim,capslock,off";
        elm_layout_signal_emit(gui->layout, sig, "eovim");
     }
}

static Evas_Object *
_config_caps_lock_add(s_gui *gui,
                      Evas_Object *parent)
{
   const s_config *const config = gui->nvim->config;

   /* Frame container */
   Evas_Object *const f = _frame_add(parent, "Caps Lock Settings");

   /* Settings box */
   Evas_Object *const box = elm_box_add(f);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, 0.0);
   evas_object_show(box);

   /* Caps lock alert checkbox */
   Evas_Object *const alert = elm_check_add(box);
   evas_object_size_hint_align_set(alert, 0.0, 0.0);
   elm_object_text_set(alert, "Alert when Caps Lock is On");
   elm_check_state_set(alert, config->alert_capslock);
   evas_object_smart_callback_add(alert, "changed", _caps_lock_changed_cb, gui);
   evas_object_show(alert);
   elm_box_pack_end(box, alert);

   elm_object_content_set(f, box);
   return f;
}

/*============================================================================*
 *                            Plugins List Handling                           *
 *============================================================================*/

static char *
_plug_text_get(void *data,
               Evas_Object *obj EINA_UNUSED,
               const char *part EINA_UNUSED)
{
   const s_plugin *const plug = data;
   return strdup(plug->name);
}

static Evas_Object *
_plug_content_get(void *data,
                  Evas_Object *obj,
                  const char *part)
{
   const s_plugin *const plug = data;
   if (strcmp(part, "elm.swallow.end") != 0) { return NULL; }

   Evas_Object *const ic = elm_icon_add(obj);
   elm_image_resizable_set(ic, EINA_FALSE, EINA_FALSE);

   /* Compose the path to the image that is used to indicate the status of
    * the plugin (loaded or not)
    */
   const char *const dir = (main_in_tree_is())
      ? SOURCE_DATA_DIR
      : elm_app_data_dir_get();
   const char *const file = (plug->loaded)
      ? "led_light.png"
      : "led_dark.png";
   Eina_Strbuf *const buf = eina_strbuf_new();
   if (EINA_UNLIKELY(! buf))
     {
        CRI("Failed to create string buffer");
        evas_object_del(ic);
        return NULL;
     }
   eina_strbuf_append_printf(buf, "%s/images/%s", dir, file);
   elm_image_file_set(ic, eina_strbuf_string_get(buf), NULL);
   eina_strbuf_free(buf);

   return ic;
}

static void
_plug_toggle(void *data,
             Evas_Object *obj,
             void *info)
{
   s_gui *const gui = data;
   s_config *const config = gui->nvim->config;
   Elm_Object_Item *const item = info;
   s_plugin *const plug = elm_object_item_data_get(item);

   /* If the plugin was loaded, unload it and remove if from the default loaded
    * plugins.  If the plugin was not loaded, load it, and add it to the
    * default loaded plugins.  After these operations refresh the genlist to
    * observe the visual change */
   if (plug->loaded)
     {
        plugin_unload(plug);
        config_plugin_del(config, plug);
     }
   else
     {
        const Eina_Bool loaded = plugin_load(plug);
        if (loaded) config_plugin_add(config, plug);
     }
   elm_genlist_realized_items_update(obj);
}


static void
_flip_to(void *data,
         Evas_Object *obj EINA_UNUSED,
         void *info EINA_UNUSED)
{
   Elm_Object_Item *const item = data;
   elm_naviframe_item_promote(item);
}

static Evas_Object *
_prefs_box_new(Evas_Object *parent)
{
   Evas_Object *const box = elm_box_add(parent);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_align_set(box, 0.5, 0.0);
   evas_object_show(box);
   return box;
}

static Evas_Object *
_theme_prefs_new(s_gui *gui)
{
   Evas_Object *const box = _prefs_box_new(gui->prefs.nav);
   Evas_Object *const bell = _config_bell_add(gui, box);
   Evas_Object *const react = _config_key_react_add(gui, box);
   Evas_Object *const caps_lock = _config_caps_lock_add(gui, box);

   elm_box_pack_end(box, bell);
   elm_box_pack_end(box, react);
   elm_box_pack_end(box, caps_lock);
   return box;
}

/*============================================================================*
 *                           Neovim Preferences Page                          *
 *============================================================================*/

static Evas_Object *
_nvim_prefs_check_add(Evas_Object *table,
                      const char *text,
                      int row)
{
   Evas_Object *const chk = elm_check_add(table);
   elm_object_style_set(chk, "toggle");
   elm_table_pack(table, chk, 1, row, 1, 1);
   evas_object_show(chk);

   Evas_Object *const label = elm_label_add(table);
   evas_object_size_hint_align_set(label, 1.0, EVAS_HINT_FILL);
   elm_object_text_set(label, text);
   elm_table_pack(table, label, 0, row, 1, 1);
   evas_object_show(label);

   return chk;
}

static void
_ext_tabs_changed_cb(void *data,
                     Evas_Object *obj,
                     void *info EINA_UNUSED)
{
   s_gui *const gui = data;
   const Eina_Bool ext = elm_check_state_get(obj);
   config_ext_tabs_set(gui->nvim->config, ext);
}

static void
_ext_popup_changed_cb(void *data,
                      Evas_Object *obj,
                      void *info EINA_UNUSED)
{
   s_gui *const gui = data;
   const Eina_Bool ext = elm_check_state_get(obj);
   config_ext_popup_set(gui->nvim->config, ext);
}

static void
_ext_cmdline_changed_cb(void *data,
                        Evas_Object *obj,
                        void *info EINA_UNUSED)
{
   s_gui *const gui = data;
   const Eina_Bool ext = elm_check_state_get(obj);
   config_ext_cmdline_set(gui->nvim->config, ext);
}

static Evas_Object *
_nvim_prefs_plug_add(s_gui *gui,
                     Evas_Object *parent)
{
   const s_config *const config = gui->nvim->config;
   int row = 0;

   /* Frame container */
   Evas_Object *const f = _frame_add(parent, "Neovim Plug Settings");
   Evas_Object *const table = elm_table_add(f);
   elm_table_align_set(table, 0.5, 0.0);
   evas_object_show(table);

   /* Externalized tabs switch */
   Evas_Object *const tabs = _nvim_prefs_check_add(table, "Externalize Tabs", row++);
   evas_object_smart_callback_add(tabs, "changed", _ext_tabs_changed_cb, gui);
   elm_check_state_set(tabs, config->ext_tabs);
   evas_object_show(tabs);

   /* Completion popup switch */
   Evas_Object *const compl = _nvim_prefs_check_add(table, "Externalize Completion Popup", row++);
   evas_object_smart_callback_add(compl, "changed", _ext_popup_changed_cb, gui);
   elm_check_state_set(compl, config->ext_popup);
   evas_object_show(compl);

   /* Externalized command-line switch */
   Evas_Object *const cmdline = _nvim_prefs_check_add(table, "Externalize Command-Line", row++);
   evas_object_smart_callback_add(cmdline, "changed", _ext_cmdline_changed_cb, gui);
   elm_check_state_set(cmdline, config->ext_cmdline);
   evas_object_show(cmdline);

   elm_object_content_set(f, table);
   return f;
}

static Evas_Object *
_nvim_prefs_msg_add(Evas_Object *parent)
{
   Evas_Object *const f = _frame_add(parent, "Special Note");

   /* Message */
   Evas_Object *const info = elm_label_add(f);
   elm_object_text_set(info, "<warning><big>"
                       "You must restart Eovim to take into account your "
                       "modifications on this page.</big/</warning>");
   evas_object_show(info);
   elm_object_content_set(f, info);

   return f;
}

static Evas_Object *
_nvim_prefs_new(s_gui *gui)
{
   Evas_Object *const box = _prefs_box_new(gui->prefs.nav);

   Evas_Object *const msg = _nvim_prefs_msg_add(box);
   elm_box_pack_start(box, msg);

   Evas_Object *const plug = _nvim_prefs_plug_add(gui, box);
   elm_box_pack_end(box, plug);

   return box;
}

static Evas_Object *
_plugins_prefs_new(s_gui *gui)
{
   s_prefs *const prefs = &(gui->prefs);
   Evas_Object *const box = _prefs_box_new(prefs->nav);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   /* Frame container */
   Evas_Object *const f = _frame_add(box, "Plug-Ins List");
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(f, EVAS_HINT_FILL, EVAS_HINT_FILL);

   /* Genlist that contains the plugins */
   Evas_Object *const list = elm_genlist_add(f);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(list, "clicked,double", _plug_toggle, gui);

   /* Load the genlist with the list of plugins */
   const Eina_Inlist *const plugs = main_plugins_get();
   s_plugin *plug;
   EINA_INLIST_FOREACH(plugs, plug)
     {
        elm_genlist_item_append(
           list, _plug_itc, plug, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   elm_object_content_set(f, list);
   elm_box_pack_end(box, f);
   return box;
}

static void
_prefs_close_cb(void *const data,
               Evas_Object *const obj EINA_UNUSED,
               void *const info EINA_UNUSED)
{
   s_gui *const gui = data;
   elm_layout_signal_emit(gui->layout, "config,hide", "eovim");

   evas_object_hide(gui->prefs.win);
   elm_object_focus_set(gui->layout, EINA_FALSE);
   evas_object_focus_set(gui->termview, EINA_TRUE);
}

static void
_prefs_save_cb(void *const data,
               Evas_Object *const obj,
               void *const info)
{
   s_gui *const gui = data;
   s_nvim *const nvim = gui->nvim;

   _prefs_close_cb(gui, obj, info);
   config_save(nvim->config);
}


void
prefs_show(s_gui *gui)
{
   /* Create the preferences window only if the user makes at least one request
    * to consult preferences. Otherwise, they will never be built */
   if (gui->prefs.win) goto show;

   Evas_Object *const win = gui->prefs.win = elm_win_inwin_add(gui->win);

   /* Create the main box that holds the prefs together */
   Evas_Object *const pbox = gui->prefs.box = _prefs_box_new(win);
   elm_box_horizontal_set(pbox, EINA_FALSE);
   elm_win_inwin_content_set(win, pbox);
   elm_box_homogeneous_set(pbox, EINA_FALSE);
   evas_object_show(pbox);

   /* Box that holds the naviframe and the toolbar */
   Evas_Object *const box = _prefs_box_new(pbox);
   elm_box_horizontal_set(box, EINA_TRUE);
   elm_box_pack_start(pbox, box);
   elm_box_homogeneous_set(box, EINA_FALSE);
   evas_object_show(box);

   /* Buttons box */
   Evas_Object *const bbox = _prefs_box_new(pbox);
   evas_object_size_hint_weight_set(bbox, 0.0, 0.0);
   elm_box_homogeneous_set(bbox, EINA_FALSE);
   elm_box_horizontal_set(bbox, EINA_TRUE);
   elm_box_align_set(bbox, 0.5, 1.0);
   elm_box_pack_end(pbox, bbox);
   evas_object_show(bbox);

   /* Button to save and close preferences */
   {
     Evas_Object *const bt = elm_button_add(bbox);
     elm_object_text_set(bt, "Save and Close");
     evas_object_smart_callback_add(bt, "clicked", _prefs_save_cb, gui);
     elm_box_pack_end(bbox, bt);
     evas_object_show(bt);
   }

   /* Button to close preferences (temporary config) */
   {
     Evas_Object *const bt = elm_button_add(bbox);
     elm_object_text_set(bt, "Close");
     evas_object_smart_callback_add(bt, "clicked", _prefs_close_cb, gui);
     elm_box_pack_start(bbox, bt);
     evas_object_show(bt);
   }

   /* Naviframe: the widget that will manage prefs pages */
   Evas_Object *const nav = gui->prefs.nav = elm_naviframe_add(box);
   evas_object_size_hint_weight_set(nav, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(nav, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(nav);
   elm_box_pack_end(box, nav);

   /* Add a toolbar that keeps track of all the pages */
   Evas_Object *const tb = elm_toolbar_add(box);
   elm_toolbar_select_mode_set(tb, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_toolbar_menu_parent_set(tb, box);
   elm_toolbar_homogeneous_set(tb, EINA_TRUE);
   elm_toolbar_horizontal_set(tb, EINA_FALSE);
   elm_object_style_set(tb, "item_horizontal");
   evas_object_size_hint_weight_set(tb, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb, 0.5, EVAS_HINT_FILL);
   elm_box_pack_start(box, tb);
   evas_object_show(tb);

   /* Create the pages */
   Elm_Object_Item *const p1 =
      elm_naviframe_item_simple_push(nav, _theme_prefs_new(gui));
   Elm_Object_Item *const p3 =
      elm_naviframe_item_simple_push(nav, _nvim_prefs_new(gui));
   Elm_Object_Item *const p4 =
      elm_naviframe_item_simple_push(nav, _plugins_prefs_new(gui));

   char buf[1024];
   snprintf(buf, sizeof(buf), "%s/images/neovim.png",
            main_in_tree_is() ? SOURCE_DATA_DIR : elm_app_data_dir_get());

   /* Associate one item data in the segment control to each page */
   elm_toolbar_item_append(tb, "preferences-desktop-theme", "Theme", _flip_to, p1);
   elm_toolbar_item_append(tb, buf, "Neovim", _flip_to, p3);
   elm_toolbar_item_append(tb, "preferences-profile", "Plug-Ins", _flip_to, p4);

show:
   evas_object_focus_set(gui->termview, EINA_FALSE);
   evas_object_show(gui->prefs.win);
}

Eina_Bool
prefs_init(void)
{
   /* Plugins list item class */
   _plug_itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(! _plug_itc))
     {
        CRI("Failed to create genlist item class");
        return EINA_FALSE;
     }
   _plug_itc->item_style = "default";
   _plug_itc->func.text_get = _plug_text_get;
   _plug_itc->func.content_get = _plug_content_get;

   return EINA_TRUE;
}

void
prefs_shutdown(void)
{
   elm_genlist_item_class_free(_plug_itc);
}
