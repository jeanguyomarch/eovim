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

#include <Eovim.h>
#include <Elementary.h>

typedef enum
{
   KW_ASPECT,
   KW_FULLSCREEN_ON,
   KW_FULLSCREEN_OFF,
   KW_FULLSCREEN_TOGGLE,

   __KW_LAST
} e_kw;

static Eina_Stringshare *_keywords[__KW_LAST];
#define KW(Val) _keywords[(Val)]

static int _sizing_log_dom = -1;

#define DBG(...) EINA_LOG_DOM_DBG(_sizing_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_sizing_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_sizing_log_dom, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_sizing_log_dom, __VA_ARGS__)
#define CRI(...) EINA_LOG_DOM_CRIT(_sizing_log_dom, __VA_ARGS__)

static Eina_Bool
eovim_sizing_init(void)
{
   /* Create a log domain for the plugin */
   _sizing_log_dom = eina_log_domain_register("eovim-sizing", EINA_COLOR_GREEN);
   if (EINA_UNLIKELY(_sizing_log_dom < 0))
     {
        EINA_LOG_CRIT("Failed to create log domain for the sizing plugin");
        goto fail;
     }

   /* Register the possible keywords as stringshares */
   const char *const ctor[__KW_LAST] = {
      [KW_ASPECT] = "aspect",
      [KW_FULLSCREEN_ON] = "fullscreen_on",
      [KW_FULLSCREEN_OFF] = "fullscreen_off",
      [KW_FULLSCREEN_TOGGLE] = "fullscreen_toggle",
   };
   int i = 0;
   for (; i < __KW_LAST; i++)
     {
        _keywords[i] = eina_stringshare_add(ctor[i]);
        if (EINA_UNLIKELY(! _keywords[i]))
          {
             CRI("Failed to create stringshare from '%s'", ctor[i]);
             goto shr_fail;
          }
     }

   return EINA_TRUE;
shr_fail:
   for (--i; i >= 0; --i)
     eina_stringshare_del(_keywords[i]);
   eina_log_domain_unregister(_sizing_log_dom);
fail:
   return EINA_FALSE;
}
EINA_MODULE_INIT(eovim_sizing_init);


static void
eovim_sizing_shutdown(void)
{
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_keywords); i++)
     eina_stringshare_del(_keywords[i]);
   eina_log_domain_unregister(_sizing_log_dom);
}
EINA_MODULE_SHUTDOWN(eovim_sizing_shutdown);


static Eina_Bool
eovim_sizing_eval(s_nvim *nvim,
                  const msgpack_object_array *args)
{
   Evas_Object *const win = eovim_window_get(nvim);

   /* Go through the method arguments. Exclude the first one ("sizing") */
   for (unsigned int i = 1; i < args->size; i++)
     {
        /* Iterate over the list of arguments */
        const msgpack_object_array *const arr =
           EOVIM_MSGPACK_ARRAY_EXTRACT(&(args->ptr[i]), fail);
        for (unsigned int j = 0; j < arr->size; j++)
          {
             /* Extract key-value object from the arguments */
             const msgpack_object *const obj = &(arr->ptr[j]);
             const msgpack_object_map *const map =
                EOVIM_MSGPACK_MAP_EXTRACT(obj, fail);

             /* Go through the arguments passed to the map */
             const msgpack_object *key_obj, *val_obj;
             size_t it;
             EOVIM_MSGPACK_MAP_ITER(map, it, key_obj, val_obj)
               {
                  Eina_Stringshare *const key =
                     EOVIM_MSGPACK_STRING_EXTRACT(key_obj, fail);
                  /* Handle the "aspect" keyword */
                  if (key == KW(KW_ASPECT))
                    {
                       Eina_Stringshare *const val =
                          EOVIM_MSGPACK_STRING_EXTRACT(val_obj, fail);

                       if (val == KW(KW_FULLSCREEN_ON)) /* Set fullscreen */
                         {
                            elm_win_fullscreen_set(win, EINA_TRUE);
                         }
                       else if (val == KW(KW_FULLSCREEN_OFF)) /* Remove fullscreen */
                         {
                            elm_win_fullscreen_set(win, EINA_FALSE);
                         }
                       else if (val == KW(KW_FULLSCREEN_TOGGLE)) /* Toggle fullscreen */
                         {
                            const Eina_Bool status = elm_win_fullscreen_get(win);
                            if (status)
                              elm_win_fullscreen_set(win, EINA_FALSE);
                            else
                              elm_win_fullscreen_set(win, EINA_TRUE);
                         }
                       else
                         ERR("Invalid argument '%s' for keyword '%s'", val, key);
                    }
                  else
                    ERR("Invalid keyword '%s'", key);
               }
          }
     }
   return EINA_TRUE;
fail:
   return EINA_FALSE;
}
EOVIM_PLUGIN_SYMBOL(eovim_sizing_eval);

EINA_MODULE_LICENSE("MIT");
EINA_MODULE_AUTHOR("Jean Guyomarc'h");
EINA_MODULE_VERSION(EOVIM_VERSION);
EINA_MODULE_DESCRIPTION("Sizing plugin to modify the dimensions of Eovim")
