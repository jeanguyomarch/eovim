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

#include <Eovim.h>
#include <Elementary.h>

static int _imageviewer_log_dom = -1;

#define DBG(...) EINA_LOG_DOM_DBG(_imageviewer_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_imageviewer_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_imageviewer_log_dom, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_imageviewer_log_dom, __VA_ARGS__)
#define CRI(...) EINA_LOG_DOM_CRIT(_imageviewer_log_dom, __VA_ARGS__)

static Eina_Bool
eovim_imageviewer_init(void)
{
   /* Create a log domain for the plugin */
   _imageviewer_log_dom =
      eina_log_domain_register("eovim-imageviewer", EINA_COLOR_BLUE);
   if (EINA_UNLIKELY(_imageviewer_log_dom < 0))
     {
        EINA_LOG_CRIT("Failed to create log domain for the imageviewer plugin");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}
EINA_MODULE_INIT(eovim_imageviewer_init);

static void
eovim_imageviewer_shutdown(void)
{
   eina_log_domain_unregister(_imageviewer_log_dom);
}
EINA_MODULE_SHUTDOWN(eovim_imageviewer_shutdown);

static void
_popup_close_cb(void *data,
                Evas *e EINA_UNUSED,
                Evas_Object *obj,
                void *event_info EINA_UNUSED)
{
   Evas_Object *const termview = data;

   evas_object_focus_set(termview, EINA_TRUE);
   evas_object_del(obj);
}

static Eina_Bool
_preview_add(s_nvim *nvim,
             const char *filename)
{
   Evas_Object *const win = eovim_window_get(nvim);
   Evas_Object *const termview = eovim_termview_get(nvim);

   Evas_Object *const pop = elm_popup_add(win);
   elm_object_style_set(pop, "transparent");
   evas_object_event_callback_add(pop, EVAS_CALLBACK_KEY_DOWN,
                                  _popup_close_cb, termview);

   Evas_Object *const img = elm_image_add(pop);
   elm_image_file_set(img, filename, NULL);
   evas_object_size_hint_weight_set(img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(img, EVAS_HINT_FILL, EVAS_HINT_FILL);

   int w, h, win_w, win_h;
   evas_object_geometry_get(win, NULL, NULL, &win_w, &win_h);
   elm_image_object_size_get(img, &w, &h);

   const int margin = 100;
   if (w + margin > win_w) w = win_w - margin;
   if (h + margin > win_h) h = win_h - margin;
   evas_object_size_hint_min_set(img, w, h);

   elm_object_content_set(pop, img);

   evas_object_show(img);
   evas_object_show(pop);
   evas_object_focus_set(termview, EINA_FALSE);
   elm_object_focus_set(pop, EINA_TRUE);
   evas_object_focus_set(pop, EINA_TRUE);

   return EINA_TRUE;
}


#define ARRAY_CHECK_SIZE(Arr, Size, Label) do { \
   if (EINA_UNLIKELY((Arr)->size != (Size))) { \
        CRI("Too many arguments provided (%u instead of %u)", (Arr)->size, (Size)); \
           goto Label; \
     } \
} while (0)

static Eina_Bool
eovim_imageviewer_eval(s_nvim *nvim,
                       const msgpack_object_array *args)
{
   ARRAY_CHECK_SIZE(args, 2, fail);

   const msgpack_object *const args_obj = &(args->ptr[1]);
   const msgpack_object_array *const args_arr =
      EOVIM_MSGPACK_ARRAY_EXTRACT(args_obj, fail);
   ARRAY_CHECK_SIZE(args_arr, 1, fail);

   const msgpack_object *const file_obj = &(args_arr->ptr[0]);
   Eina_Stringshare *const file =
      EOVIM_MSGPACK_STRING_EXTRACT(file_obj, fail);

   const Eina_Bool ret = _preview_add(nvim, file);
   eina_stringshare_del(file);

   return ret;
fail:
   return EINA_FALSE;
}
EOVIM_PLUGIN_SYMBOL(eovim_imageviewer_eval);

EINA_MODULE_LICENSE("MIT");
EINA_MODULE_AUTHOR("Jean Guyomarc'h");
EINA_MODULE_VERSION(EOVIM_VERSION);
EINA_MODULE_DESCRIPTION("Image Viewer to Preview Images from Eovim");
