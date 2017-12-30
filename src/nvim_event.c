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
#include "eovim/nvim.h"
#include "eovim/nvim_event.h"
#include "eovim/msgpack_helper.h"
#include "eovim/gui.h"
#include "eovim/mode.h"

typedef enum
{
   /* Highlight keywords */
   KW_FOREGROUND,
   KW_BACKGROUND,
   KW_SPECIAL,
   KW_REVERSE,
   KW_ITALIC,
   KW_BOLD,
   KW_UNDERLINE,
   KW_UNDERCURL,

   /* Mode info set keywords */
   KW_CURSOR_SHAPE,
   KW_CELL_PERCENTAGE,
   KW_BLINKWAIT,
   KW_BLINKON,
   KW_BLINKOFF,
   KW_HL_ID,
   KW_ID_LM,
   KW_NAME,
   KW_SHORT_NAME,
   KW_MOUSE_SHAPE,

   KW_MODE_NORMAL,

   __KW_LAST,

   /* Aliases */
   KW_HIGHLIGHT_START = KW_FOREGROUND,
   KW_HIGHLIGHT_END = KW_UNDERCURL,
   KW_MODE_INFO_START = KW_CURSOR_SHAPE,
   KW_MODE_INFO_END = KW_MOUSE_SHAPE,
   KW_MODES_START = KW_MODE_NORMAL,
   KW_MODES_END = KW_MODE_NORMAL,
} e_kw;

typedef struct
{
   Eina_Stringshare *name; /**< Name of the method */
   Eina_Hash *callbacks; /**< Table of callbacks associated with the method */
} s_method;

typedef enum
{
   E_METHOD_REDRAW, /**< The "redraw" method */
   E_METHOD_EOVIM, /**< The "eovim" method */

   __E_METHOD_LAST
} e_method;

/* Array of callbacks for each method. It is NOT a hash table as we will support
 * (for now) very little number of methods (around two). It is much faster to
 * search sequentially among arrays of few cells than a map of two. */
static s_method _methods[__E_METHOD_LAST];

/* Cache of stringshared keywords */
static Eina_Stringshare *_keywords[__KW_LAST];
#define KW(Name) _keywords[Name]

/*
 * When checking the count of args, we have to subtract 1 from the total
 * args count of the object, as the first argument is the command name
 * itself.
 */
#define CHECK_BASE_ARGS_COUNT(Args, Op, Count) \
   if (EINA_UNLIKELY(! ((Args)->size - 1 Op (Count)))) { \
      CRI("Invalid argument count. (%u %s %u) is false", \
          (Args)->size - 1, # Op, (Count)); \
      return EINA_FALSE; \
   }

#define CHECK_ARGS_COUNT(Args, Op, Count) \
   if (EINA_UNLIKELY(! ((Args)->size Op (Count)))) { \
      CRI("Invalid argument count. (%u %s %u) is false", \
          (Args)->size, # Op, (Count)); \
      return EINA_FALSE; \
   }

#define GET_ARG(Args, Index, Type, Get) \
   if (EINA_UNLIKELY(! _arg_ ## Type ## _get((Args), (Index), (Get)))) { \
      return EINA_FALSE; \
   }

#define ARRAY_OF_ARGS_EXTRACT(Args, Ret) \
   const msgpack_object_array *const Ret = _array_of_args_extract(Args); \
   if (EINA_UNLIKELY(! Ret)) { return EINA_FALSE; }

#define CHECK_TYPE(Obj, Type, ...) \
   if (EINA_UNLIKELY((Obj)->type != Type)) { \
      CRI("Expected type 0x%x. Got 0x%x", Type, (Obj)->type); \
      return __VA_ARGS__; \
   }


static const msgpack_object_array *
_array_of_args_extract(const msgpack_object_array *args)
{
   const msgpack_object *const obj = &(args->ptr[1]);
   CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, NULL);
   return &(obj->via.array);
}

static Eina_Bool
_arg_t_int_get(const msgpack_object_array *args,
               unsigned int index,
               t_int *arg)
{
   const msgpack_object *const obj = &(args->ptr[index]);
   if (EINA_UNLIKELY((obj->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&
                     (obj->type != MSGPACK_OBJECT_NEGATIVE_INTEGER)))
     {
        CRI("Expected an integer type for argument %u. Got 0x%x",
            index, obj->type);
        return EINA_FALSE;
     }
   *arg = obj->via.i64;
   return EINA_TRUE;
}

static Eina_Bool
_arg_stringshare_get(const msgpack_object_array *args,
                     unsigned int index,
                     Eina_Stringshare **arg)
{
   const msgpack_object *const obj = &(args->ptr[index]);
   CHECK_TYPE(obj, MSGPACK_OBJECT_STR, EINA_FALSE);
   const msgpack_object_str *const str = &(obj->via.str);
   *arg = eina_stringshare_add_length(str->ptr, str->size);
   if (EINA_UNLIKELY(! *arg))
     {
        CRI("Failed to create stringshare");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Eina_Bool
_arg_bool_get(const msgpack_object_array *args,
              unsigned int index,
              Eina_Bool *arg)
{
   const msgpack_object *const obj = &(args->ptr[index]);
   CHECK_TYPE(obj, MSGPACK_OBJECT_BOOLEAN, EINA_FALSE);
   *arg = obj->via.boolean;
   return EINA_TRUE;
}


static Eina_Bool
nvim_event_resize(s_nvim *nvim,
                  const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   t_int rows, columns;
   GET_ARG(params, 0, t_int, &columns);
   GET_ARG(params, 1, t_int, &rows);

   gui_resized_confirm(&nvim->gui, (unsigned int)columns, (unsigned int)rows);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_clear(s_nvim *nvim,
                 const msgpack_object_array *args EINA_UNUSED)
{
   gui_clear(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_eol_clear(s_nvim *nvim,
                     const msgpack_object_array *args EINA_UNUSED)
{
   gui_eol_clear(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_cursor_goto(s_nvim *nvim,
                       const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   t_int row, column;
   GET_ARG(params, 0, t_int, &row);
   GET_ARG(params, 1, t_int, &column);

   gui_cursor_goto(&nvim->gui, (unsigned int)column, (unsigned int)row);

   return EINA_TRUE;
}

static Eina_Bool
_mode_info_set(s_nvim *nvim,
               const msgpack_object_array *params)
{
   /* First arg: boolean */
   Eina_Bool cursor_style_enabled;
   GET_ARG(params, 0, bool, &cursor_style_enabled);
   /* Second arg: an array that contains ONE map */
   ARRAY_OF_ARGS_EXTRACT(params, kw_params);
   CHECK_ARGS_COUNT(params, >=, 1);

   /* Go through all the arguments. They are expected to be maps */
   for (unsigned int i = 0; i < kw_params->size; i++)
     {
        const msgpack_object *o = &(kw_params->ptr[i]);
        CHECK_TYPE(o, MSGPACK_OBJECT_MAP, EINA_FALSE);
        const msgpack_object_map *const map = &(o->via.map);

        /* We will store for each map, pointers to the values */
        const msgpack_object *objs[KW_MODE_INFO_END - KW_MODE_INFO_START + 1];
        memset(objs, 0, sizeof(objs));

        /* For each element in the map */
        for (unsigned int j = 0; j < map->size; j++)
          {
             /* Get the key as a string object */
             const msgpack_object_kv *const kv = &(map->ptr[j]);
             const msgpack_object *const key = &(kv->key);
             if (EINA_UNLIKELY(key->type != MSGPACK_OBJECT_STR))
               {
                  CRI("Key is expected to be a string. Got type 0x%x", key->type);
                  continue; /* Pass on */
               }
             const msgpack_object_str *const key_str = &(key->via.str);

             /* Create a stringshare from the key */
             Eina_Stringshare *const key_shr = eina_stringshare_add_length(
                key_str->ptr, key_str->size
             );
             if (EINA_UNLIKELY(! key_shr))
               {
                  CRI("Failed to create stringshare");
                  continue; /* Pass on */
               }

             /* Go through all the known keywords to fill the 'objs' structure
              * We are actually creating an other map from this one for easier
              * access */
             for (unsigned int k = KW_MODE_INFO_START, x = 0; k <= KW_MODE_INFO_END; k++, x++)
               {
                  if (key_shr == KW(k))
                    {
                       objs[x] = &(kv->val);
                       break;
                    }
               }
             eina_stringshare_del(key_shr);
          }

#define _GET_OBJ(Kw) objs[(Kw) - KW_MODE_INFO_START]

        /* Now that we have filled the 'objs' structure, handle the mode */
        Eina_Stringshare *const name = eina_stringshare_add_length(
           _GET_OBJ(KW_NAME)->via.str.ptr, _GET_OBJ(KW_NAME)->via.str.size
        );
        s_mode *mode = (s_mode *)nvim_named_mode_get(nvim, name);
        if (! mode)
          {
             const msgpack_object_str *const sname = &(_GET_OBJ(KW_SHORT_NAME)->via.str);
             mode = mode_new(name, sname->ptr, sname->size);
             nvim_mode_add(nvim, mode);
          }
        eina_stringshare_del(name);

#define _GET_INT(Kw, Set)                                                     \
   if ((o = _GET_OBJ(Kw))) {                                                      \
        if (EINA_UNLIKELY((o->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&     \
                          (o->type != MSGPACK_OBJECT_NEGATIVE_INTEGER)))      \
          CRI("Expected an integer type. Got 0x%x", o->type);                 \
        else Set = (unsigned int)o->via.i64;                                  \
   }

        if ((o = _GET_OBJ(KW_CURSOR_SHAPE)))
          {
            if (EINA_UNLIKELY(o->type != MSGPACK_OBJECT_STR))
              CRI("Expected a string type. Got 0x%x", o->type);
            else
              {
                 Eina_Stringshare *const shr = eina_stringshare_add_length(
                    o->via.str.ptr, o->via.str.size
                 );
                 if (EINA_UNLIKELY(! shr))
                   CRI("Failed to create stringshare");
                 else
                   {
                      mode->cursor_shape = mode_cursor_shape_convert(shr);
                      eina_stringshare_del(shr);
                   }
              }
          }

        _GET_INT(KW_BLINKWAIT, mode->blinkwait);
        _GET_INT(KW_BLINKON, mode->blinkon);
        _GET_INT(KW_BLINKOFF, mode->blinkoff);
        _GET_INT(KW_HL_ID, mode->hl_id);
        _GET_INT(KW_ID_LM, mode->id_lm);
        _GET_INT(KW_CELL_PERCENTAGE, mode->cell_percentage);
     }

#undef _GET_INT
#undef _GET_OBJ

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_mode_info_set(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   Eina_Bool ret = EINA_TRUE;
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const params = &(obj->via.array);
        CHECK_ARGS_COUNT(params, ==, 2);

        ret &= _mode_info_set(nvim, params);
     }
   return ret;
}

static Eina_Bool
nvim_event_update_menu(s_nvim *nvim EINA_UNUSED,
                       const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_busy_start(s_nvim *nvim,
                      const msgpack_object_array *args EINA_UNUSED)
{
   gui_busy_set(&nvim->gui, EINA_TRUE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_busy_stop(s_nvim *nvim,
                     const msgpack_object_array *args EINA_UNUSED)
{
   gui_busy_set(&nvim->gui, EINA_FALSE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_mouse_on(s_nvim *nvim,
                    const msgpack_object_array *args EINA_UNUSED)
{
   nvim_mouse_enabled_set(nvim, EINA_TRUE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_mouse_off(s_nvim *nvim,
                     const msgpack_object_array *args EINA_UNUSED)
{
   nvim_mouse_enabled_set(nvim, EINA_FALSE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_mode_change(s_nvim *nvim,
                       const msgpack_object_array *args)
{
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);

        ARRAY_OF_ARGS_EXTRACT(args, params);
        CHECK_ARGS_COUNT(params, ==, 2);

        Eina_Stringshare *name;
        t_int index;
        GET_ARG(params, 1, t_int, &index);
        GET_ARG(params, 0, stringshare, &name);

        gui_mode_update(&nvim->gui, name);
        eina_stringshare_del(name);
     }
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_set_scroll_region(s_nvim *nvim,
                             const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 4);

   t_int top, bot, left, right;
   GET_ARG(params, 0, t_int, &top);
   GET_ARG(params, 1, t_int, &bot);
   GET_ARG(params, 2, t_int, &left);
   GET_ARG(params, 3, t_int, &right);

   gui_scroll_region_set(&nvim->gui, (int)top, (int)bot, (int)left, (int)right);

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_scroll(s_nvim *nvim,
                  const msgpack_object_array *args)
{
   /*
    * It appears that in some cases (when the sh*tty command popup opens,
    * because a message is open in another castle), the scroll command changes,
    * and we get ["scroll", [N], [N]] instead of ["scroll", [N]].  I'm not sure
    * if this is supposed to happen, but that's what happens!  So we should be
    * ready to handle X arguments to "scroll" instead of just one
    */
   for (unsigned int i = 1; i < args->size; i++) /* 1 to exclude "scroll" */
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);

        t_int scroll;
        GET_ARG(arr, 0, t_int, &scroll);
        gui_scroll(&nvim->gui, (int)scroll);
     }

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_highlight_set(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, >=, 1);

   /* Array that holds pointer to values within hash tables.
    * We must ensure this array has been zeroes-out since we will rely on
    * NULL pointers to check whether a value was provided or not */
   const msgpack_object *objs[KW_HIGHLIGHT_END - KW_HIGHLIGHT_START + 1];
   memset(objs, 0, sizeof(objs));

   /*
    * highlight arguments are arrays containing maps.
    */
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);
        const msgpack_object *const arr_arg = &(arr->ptr[0]);
        CHECK_TYPE(arr_arg, MSGPACK_OBJECT_MAP, EINA_FALSE);

        /*
         * Okay, we now have a map. We go through all its key-value pairs
         * and match the keys with the expected keys. If we have a match,
         * we grab a pointer to the value, and pass on to the next pair.
         */
        const msgpack_object_map *const map = &(arr_arg->via.map);
        for (unsigned int j = 0; j < map->size; j++)
          {
             const msgpack_object_kv *const kv = &(map->ptr[j]);
             const msgpack_object *const key_obj = &(kv->key);
             CHECK_TYPE(key_obj, MSGPACK_OBJECT_STR, EINA_FALSE);
             const msgpack_object_str *const key = &(key_obj->via.str);
             Eina_Stringshare *const shr_key = eina_stringshare_add_length(
                key->ptr, key->size
             );
             if (EINA_UNLIKELY(! shr_key))
               {
                  CRI("Failed to create stringshare");
                  return EINA_FALSE;
               }

             /*
              * Keys matching. I think this is pretty efficient, as we are
              * comparing stringshares! So key matching is just a pointer
              * comparison. There are not much keys to be matched against
              * (< 10), so linear walk is good enough -- probably faster than
              * hashing.
              */
             for (unsigned int k = KW_HIGHLIGHT_START, x = 0; k <= KW_HIGHLIGHT_END; k++, x++)
               {
                  if (shr_key == KW(k))
                    {
                       objs[x] = &(kv->val);
                       break;
                    }
               }

             eina_stringshare_del(shr_key);
          }
     }

   s_termview_style style = {
      .fg_color = -1,
      .bg_color = -1,
      .sp_color = -1,
      .reverse = EINA_FALSE,
      .italic = EINA_FALSE,
      .bold = EINA_FALSE,
      .underline = EINA_FALSE,
      .undercurl = EINA_FALSE,
   };
   const msgpack_object *o;

   /*
    * At this point, we have collected everything we could. We check for
    * types to ensure that we will correctly use the memory.
    */

#define _GET_INT(Kw, Set)                                                     \
   if ((o = objs[Kw])) {                                                      \
        if (EINA_UNLIKELY((o->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&     \
                          (o->type != MSGPACK_OBJECT_NEGATIVE_INTEGER)))      \
          CRI("Expected an integer type. Got 0x%x", o->type);                 \
        else Set = o->via.i64;                                                \
   }

#define _GET_BOOL(Kw, Set)                                                    \
   if ((o = objs[Kw])) {                                                      \
        if (EINA_UNLIKELY(o->type != MSGPACK_OBJECT_BOOLEAN))                 \
          CRI("Expected an integer type. Got 0x%x", o->type);                 \
        else Set = o->via.boolean;                                            \
   }

   _GET_INT(KW_FOREGROUND, style.fg_color);
   _GET_INT(KW_BACKGROUND, style.bg_color);
   _GET_INT(KW_SPECIAL, style.sp_color);
   _GET_BOOL(KW_REVERSE, style.reverse);
   _GET_BOOL(KW_ITALIC, style.italic);
   _GET_BOOL(KW_BOLD, style.bold);
   _GET_BOOL(KW_UNDERLINE, style.underline);
   _GET_BOOL(KW_UNDERCURL, style.undercurl);

#undef _GET_INT
#undef _GET_BOOL

   gui_style_set(&nvim->gui, &style);

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_put(s_nvim *nvim,
               const msgpack_object_array *args)
{
   /*
    * The 'put' command is much more complicated than what it should be.
    * We are suppose to receive a string, but we actually do receive arrays
    * of strings.
    */
   CHECK_BASE_ARGS_COUNT(args, >=, 1);

   /* Reset the put decode buffer */
   eina_ustrbuf_reset(nvim->decode);

   /*
    * Go through all the arguments. Each argument is an array that contains
    * exactly one string. We concatenate all these strings together in one
    * static buffer (this is always called from the main thread, so no
    * concurrency shit).
    */
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);
        const msgpack_object *const arr_arg = &(arr->ptr[0]);
        CHECK_TYPE(arr_arg, MSGPACK_OBJECT_STR, EINA_FALSE);

        const msgpack_object_str *const str = &(arr_arg->via.str);
        int index = 0;
        const Eina_Unicode cp = eina_unicode_utf8_next_get(str->ptr, &index);
        if (EINA_UNLIKELY(cp == 0))
          {
             ERR("Failed to decode utf-8 string. Skipping.");
             continue;
          }
        eina_ustrbuf_append_char(nvim->decode, cp);
     }

   /* Finally, pass the string and its length to the gui */
   gui_put(&nvim->gui,
           eina_ustrbuf_string_get(nvim->decode),
           (unsigned int)eina_ustrbuf_length_get(nvim->decode));

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_bell(s_nvim *nvim,
                const msgpack_object_array *args EINA_UNUSED)
{
   gui_bell_ring(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_visual_bell(s_nvim *nvim EINA_UNUSED,
                       const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_update_fg(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int color;
   GET_ARG(params, 0, t_int, &color);
   gui_update_fg(&nvim->gui, color);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_update_bg(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int color;
   GET_ARG(params, 0, t_int, &color);
   gui_update_bg(&nvim->gui, color);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_update_sp(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int color;
   GET_ARG(params, 0, t_int, &color);
   gui_update_sp(&nvim->gui, color);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_suspend(s_nvim *nvim EINA_UNUSED,
                   const msgpack_object_array *args EINA_UNUSED)
{
   /* Nothing to do */
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_set_title(s_nvim *nvim EINA_UNUSED,
                     const msgpack_object_array *args EINA_UNUSED)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   Eina_Stringshare *const title =
      EOVIM_MSGPACK_STRING_EXTRACT(&params->ptr[0], fail);
   gui_title_set(&nvim->gui, title);
   eina_stringshare_del(title);

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static Eina_Bool
nvim_event_set_icon(s_nvim *nvim EINA_UNUSED,
                    const msgpack_object_array *args EINA_UNUSED)
{
   /* Do nothing. Seems it can be safely ignored. */
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_popupmenu_show(s_nvim *nvim,
                          const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 4);

   const msgpack_object *const data_obj = &(params->ptr[0]);
   CHECK_TYPE(data_obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
   const msgpack_object_array *const data = &(data_obj->via.array);
   s_gui *const gui = &nvim->gui;

   t_int selected, row, col;
   GET_ARG(params, 1, t_int, &selected);
   GET_ARG(params, 2, t_int, &row);
   GET_ARG(params, 3, t_int, &col);

   /* Okay, now this gets annoying. We will proceed in two passes on the
    * completion items. The gui uses a genlist, which is hardcore to use.
    * When we append an item to the genlist, it is asynchroneously created,
    * and without prior knowledge of all the compltion items, we canot
    * properly size the genlist.
    *
    * So, we go through all the items once. We do all the type checks, and
    * calculate the maximum length of the word and menu items.
    * These maxima will be used to size the genlist later. Since all checks
    * are done in the first pass, the second pass does not need to perform
    * them again.
    */
   size_t max_len_word = 0;
   size_t max_len_menu = 0;
   for (unsigned int i = 0; i < data->size; i++)
     {
        CHECK_TYPE(&data->ptr[i], MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const completion = &(data->ptr[i].via.array);
        CHECK_ARGS_COUNT(completion, ==, 4);

        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[0], fail); /* word */
        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[1], fail); /* kind */
        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[2], fail); /* menu */
        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[3], fail); /* info */

        max_len_word = MAX(max_len_word, completion->ptr[0].via.str.size);
        max_len_menu = MAX(max_len_menu, completion->ptr[2].via.str.size);
     }

   gui_completion_prepare(gui, data->size, max_len_word, max_len_menu);

   /* Second pass. Remember, no checks required. */
   for (unsigned int i = 0; i < data->size; i++)
     {
        const msgpack_object_array *const completion = &(data->ptr[i].via.array);

        s_completion *const compl = malloc(sizeof(s_completion));
        if (EINA_UNLIKELY(! compl))
          {
             CRI("Failed to allocate memory for completion");
             return EINA_FALSE;
          }
#define STRINGSHARE_NEW(Obj) \
   eina_stringshare_add_length((Obj).via.str.ptr, (Obj).via.str.size)
        compl->word = STRINGSHARE_NEW(completion->ptr[0]);
        compl->kind = STRINGSHARE_NEW(completion->ptr[1]);
        compl->menu = STRINGSHARE_NEW(completion->ptr[2]);
        compl->info = STRINGSHARE_NEW(completion->ptr[3]);
#undef STRINGSHARE_GET

        gui_completion_add(gui, compl);
     }

   gui_completion_show(gui, (int)selected,
                       (unsigned int)col, (unsigned int)row);

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static Eina_Bool
nvim_event_popupmenu_hide(s_nvim *nvim,
                          const msgpack_object_array *args EINA_UNUSED)
{
   gui_completion_hide(&nvim->gui);
   gui_completion_clear(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_popupmenu_select(s_nvim *nvim,
                            const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int selected;
   GET_ARG(params, 0, t_int, &selected);

   gui_completion_selected_set(&nvim->gui, (int)selected);

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_tabline_update(s_nvim *nvim EINA_UNUSED,
                          const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_cmdline_show(s_nvim *nvim,
                        const msgpack_object_array *args)
{
   Eina_Bool ret = EINA_FALSE;
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 6);

   /*
    * The arguments of cmdline_show are:
    *
    * [0]: content
    * [1]: cursor position (int)
    * [2]: first character (string)
    * [3]: prompt (string)
    * [4]: indentation (int)
    * [5]: level (int)
    */
   const msgpack_object_array *const content =
      EOVIM_MSGPACK_ARRAY_EXTRACT(&params->ptr[0], fail);
   const int64_t pos =
      EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[1], fail);
   Eina_Stringshare *const firstc =
      EOVIM_MSGPACK_STRING_EXTRACT(&params->ptr[2], fail);
   Eina_Stringshare *const prompt =
      EOVIM_MSGPACK_STRING_EXTRACT(&params->ptr[3], del_firstc);
   const int64_t indent =
      EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[4], del_prompt);
   //const int64_t level =
   //   EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[5], del_prompt);

   /* Create the string buffer, which will hold the content of the cmdline */
   Eina_Strbuf *const buf = eina_strbuf_new();
   if (EINA_UNLIKELY(! buf))
     {
        CRI("Failed to allocate memory");
        return EINA_FALSE;
     }

   /* Add to the content of the command-line the number of spaces the text
    * should be indented of */
   for (size_t i = 0; i < (size_t)indent; i++)
     eina_strbuf_append_char(buf, ' ');

   for (unsigned int i = 0; i < content->size; i++)
     {
        const msgpack_object_array *const cont =
           EOVIM_MSGPACK_ARRAY_EXTRACT(&content->ptr[i], del_buf);

        /* The map will contain highlight attributes */
        //const msgpack_object_map *const map =
        //   EOVIM_MSGPACK_MAP_EXTRACT(&cont->ptr[0], del_buf);

        /* Extract the content of the command-line */
        const msgpack_object *const cont_o = &(cont->ptr[1]);
        EOVIM_MSGPACK_STRING_CHECK(cont_o, del_buf);
        const msgpack_object_str *const str = &(cont_o->via.str);
        eina_strbuf_append_length(buf, str->ptr, str->size);
     }

   gui_cmdline_show(&nvim->gui, eina_strbuf_string_get(buf), prompt, firstc);

   /* Set the cursor position within the command-line */
   gui_cmdline_cursor_pos_set(&nvim->gui, (size_t)pos);

   ret = EINA_TRUE;
del_buf:
   eina_strbuf_free(buf);
del_prompt:
   eina_stringshare_del(prompt);
del_firstc:
   eina_stringshare_del(firstc);
fail:
   return ret;
}

static Eina_Bool
nvim_event_cmdline_pos(s_nvim *nvim,
                       const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   /* First argument if the position, second is the level. Level is not
    * handled for now.
    */
   const int64_t pos =
      EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[0], fail);
   //const int64_t level =
   //   EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[1], fail);

   gui_cmdline_cursor_pos_set(&nvim->gui, (size_t)pos);
   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static Eina_Bool
nvim_event_cmdline_special_char(s_nvim *nvim EINA_UNUSED,
                                const msgpack_object_array *args EINA_UNUSED)
{
   CRI("unimplemented");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_cmdline_hide(s_nvim *nvim,
                        const msgpack_object_array *args EINA_UNUSED)
{
   gui_cmdline_hide(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_cmdline_block_show(s_nvim *nvim EINA_UNUSED,
                              const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Blocks in cmdline is currently not supported. Sorry.");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_cmdline_block_append(s_nvim *nvim EINA_UNUSED,
                                const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Blocks in cmdline is currently not supported. Sorry.");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_cmdline_block_hide(s_nvim *nvim EINA_UNUSED,
                              const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Blocks in cmdline is currently not supported. Sorry.");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_wildmenu_show(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);
   s_gui *const gui = &nvim->gui;

   /* Go through all the items to be added to the wildmenu, and populate the
    * UI interface */
   const msgpack_object_array *const content =
      EOVIM_MSGPACK_ARRAY_EXTRACT(&params->ptr[0], fail);
   for (unsigned int i = 0; i < content->size; i++)
     {
        Eina_Stringshare *const item =
           EOVIM_MSGPACK_STRING_EXTRACT(&(content->ptr[i]), fail);
        gui_wildmenu_append(gui, item);
     }
   gui_wildmenu_show(gui);

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static Eina_Bool
nvim_event_wildmenu_hide(s_nvim *nvim,
                         const msgpack_object_array *args EINA_UNUSED)
{
   gui_wildmenu_clear(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_wildmenu_select(s_nvim *nvim,
                           const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   const int64_t index = EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[0], fail);
   gui_wildmenu_select(&nvim->gui, index);
   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

Eina_Bool
nvim_event_dispatch(s_nvim *nvim,
                    Eina_Stringshare *method_name,
                    Eina_Stringshare *command,
                    const msgpack_object_array *args)
{
   /* Go sequentially through the list of methods we know about, so we can
    * find out the callbacks table for that method, to try to find what matches
    * 'command'. */
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_methods); i++)
     {
        const s_method *const method = &(_methods[i]);
        if (method->name == method_name) /* Fond the method */
          {
             /* Grab the callback for the command. If we could find none,
              * that's an error. Otherwise we call it. In both cases, the
              * execution of the function will be terminated. */
             const f_event_cb cb = eina_hash_find(method->callbacks, command);
             if (EINA_UNLIKELY(! cb))
               {
                  CRI("Failed to get callback for command '%s' of method '%s'",
                      command, method_name);
                  return EINA_FALSE;
               }
             else
               return cb(nvim, args);
          }
     }

   /* At this point, we didn't find the method. */
   CRI("Unknown method '%s'", method_name);
   return EINA_FALSE;
}


typedef struct
{
   const char *const name; /**< Name of the event */
   const unsigned int size; /**< Size of @p name */
   const f_event_cb func; /**< Callback function */
} s_method_ctor;

#define CB_CTOR(Name, Func) \
{ .name = (Name), .size = sizeof(Name) - 1, .func = (Func) }

static Eina_Bool
_command_add(Eina_Hash *table,
             const char *name,
             unsigned int name_len,
             f_event_cb func)
{
   /* Create the key to be inserted in the table */
   Eina_Stringshare *const cmd = eina_stringshare_add_length(
      name, name_len
   );
   if (EINA_UNLIKELY(! cmd))
     {
        CRI("Failed to create stringshare from '%s', size %u",
            name, name_len);
        goto fail;
     }

   /* Add the value 'func' to the key 'cmd' in the table */
   if (EINA_UNLIKELY(! eina_hash_add(table, cmd, func)))
     {
        CRI("Failed to register callback %p for command '%s'",
            func, cmd);
        goto add_fail;
     }

   return EINA_TRUE;
add_fail:
   eina_stringshare_del(cmd);
fail:
   return EINA_FALSE;
}

static Eina_Hash *
_method_callbacs_table_build(const s_method_ctor *ctors,
                             unsigned int ctors_count)
{
   /* Generate a hash table that will contain the callbacks to be called for
    * each event sent by neovim. */
   Eina_Hash *const cb = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(! cb))
     {
        CRI("Failed to create hash table to hold callbacks");
        goto fail;
     }

   /* Add the events in the callbacks table. */
   for (unsigned int i = 0; i < ctors_count; i++)
     {
        const Eina_Bool ok =
           _command_add(cb, ctors[i].name, ctors[i].size, ctors[i].func);
        if (EINA_UNLIKELY(! ok))
          {
             CRI("Failed to register command '%s'", ctors[i].name);
             goto hash_free;
          }
     }

   return cb;
hash_free:
   eina_hash_free(cb);
fail:
   return NULL;
}

static void
_method_free(s_method *method)
{
   /* This function is always used on statically-allocated containers.
    * As such, free() should not be called on 'method' */
   if (method->name) eina_stringshare_del(method->name);
   if (method->callbacks) eina_hash_free(method->callbacks);
}

static Eina_Bool
_method_redraw_init(e_method method_id)
{
   s_method *const method = &(_methods[method_id]);
   const s_method_ctor ctors[] = {
      CB_CTOR("resize", nvim_event_resize),
      CB_CTOR("clear", nvim_event_clear),
      CB_CTOR("eol_clear", nvim_event_eol_clear),
      CB_CTOR("cursor_goto", nvim_event_cursor_goto),
      CB_CTOR("mode_info_set", nvim_event_mode_info_set),
      CB_CTOR("update_menu", nvim_event_update_menu),
      CB_CTOR("busy_start", nvim_event_busy_start),
      CB_CTOR("busy_stop", nvim_event_busy_stop),
      CB_CTOR("mouse_on", nvim_event_mouse_on),
      CB_CTOR("mouse_off", nvim_event_mouse_off),
      CB_CTOR("mode_change", nvim_event_mode_change),
      CB_CTOR("set_scroll_region", nvim_event_set_scroll_region),
      CB_CTOR("scroll", nvim_event_scroll),
      CB_CTOR("highlight_set", nvim_event_highlight_set),
      CB_CTOR("put", nvim_event_put),
      CB_CTOR("bell", nvim_event_bell),
      CB_CTOR("visual_bell", nvim_event_visual_bell),
      CB_CTOR("update_fg", nvim_event_update_fg),
      CB_CTOR("update_bg", nvim_event_update_bg),
      CB_CTOR("update_sp", nvim_event_update_sp),
      CB_CTOR("suspend", nvim_event_suspend),
      CB_CTOR("set_title", nvim_event_set_title),
      CB_CTOR("set_icon", nvim_event_set_icon),
      CB_CTOR("popupmenu_show", nvim_event_popupmenu_show),
      CB_CTOR("popupmenu_hide", nvim_event_popupmenu_hide),
      CB_CTOR("popupmenu_select", nvim_event_popupmenu_select),
      CB_CTOR("tabline_update", nvim_event_tabline_update),
      CB_CTOR("cmdline_show", nvim_event_cmdline_show),
      CB_CTOR("cmdline_pos", nvim_event_cmdline_pos),
      CB_CTOR("cmdline_special_char", nvim_event_cmdline_special_char),
      CB_CTOR("cmdline_hide", nvim_event_cmdline_hide),
      CB_CTOR("cmdline_block_show", nvim_event_cmdline_block_show),
      CB_CTOR("cmdline_block_append", nvim_event_cmdline_block_append),
      CB_CTOR("cmdline_block_hide", nvim_event_cmdline_block_hide),
      CB_CTOR("wildmenu_show", nvim_event_wildmenu_show),
      CB_CTOR("wildmenu_hide", nvim_event_wildmenu_hide),
      CB_CTOR("wildmenu_select", nvim_event_wildmenu_select),
   };

   /* Register the name of the method as a stringshare */
   const char name[] = "redraw";
   method->name = eina_stringshare_add_length(name, sizeof(name) - 1);
   if (EINA_UNLIKELY(! method->name))
     {
        CRI("Failed to create stringshare from string '%s'", name);
        goto fail;
     }

   /* Build the callbacks table of the method */
   method->callbacks =
      _method_callbacs_table_build(ctors, EINA_C_ARRAY_LENGTH(ctors));
   if (EINA_UNLIKELY(! method->callbacks))
     {
        CRI("Failed to create table of callbacks");
        goto free_name;
     }

   return EINA_TRUE;
free_name:
   eina_stringshare_del(method->name);
fail:
   return EINA_FALSE;
}

static Eina_Bool
_method_eovim_init(e_method method_id)
{
   s_method *const method = &(_methods[method_id]);

   /* Register the name of the method as a stringshare */
   const char name[] = "eovim";
   method->name = eina_stringshare_add_length(name, sizeof(name) - 1);
   if (EINA_UNLIKELY(! method->name))
     {
        CRI("Failed to create stringshare from string '%s'", name);
        goto fail;
     }

   /* Create an empty the callbacks table of the method */
   method->callbacks = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(! method->callbacks))
     {
        CRI("Failed to create table of callbacks");
        goto free_name;
     }

   return EINA_TRUE;
free_name:
   eina_stringshare_del(method->name);
fail:
   return EINA_FALSE;
}

Eina_Bool
nvim_event_plugin_register(const char *command,
                           f_event_cb callback)
{
   /* Add the callback for the given command to the eovim method */
   Eina_Hash *const table = _methods[E_METHOD_EOVIM].callbacks;
   const Eina_Bool ok =
      _command_add(table, command, (unsigned int)strlen(command), callback);
   if (EINA_UNLIKELY(! ok))
     CRI("Failed to register plugin event '%s'", command);
   return ok;
}

Eina_Bool
nvim_event_init(void)
{
   const char *const keywords[] = {
      [KW_FOREGROUND] = "foreground",
      [KW_BACKGROUND] = "background",
      [KW_SPECIAL] = "special",
      [KW_REVERSE] = "reverse",
      [KW_ITALIC] = "italic",
      [KW_BOLD] = "bold",
      [KW_UNDERLINE] = "underline",
      [KW_UNDERCURL] = "undercurl",
      [KW_CURSOR_SHAPE] = "cursor_shape",
      [KW_CELL_PERCENTAGE] = "cell_percentage",
      [KW_BLINKWAIT] = "blinkwait",
      [KW_BLINKON] = "blinkon",
      [KW_BLINKOFF] = "blinkoff",
      [KW_HL_ID] = "hl_id",
      [KW_ID_LM] = "id_lm",
      [KW_NAME] = "name",
      [KW_SHORT_NAME] = "short_name",
      [KW_MOUSE_SHAPE] = "mouse_shape",
      [KW_MODE_NORMAL] = "normal",
   };
   int i;
   for (i = 0; i < __KW_LAST; i++)
     {
        _keywords[i] = eina_stringshare_add(keywords[i]);
        if (EINA_UNLIKELY(! _keywords[i]))
          {
             CRI("Failed to create stringshare");
             goto fail;
          }
     }

   /* Initialize the "redraw" method */
   if (EINA_UNLIKELY(! _method_redraw_init(E_METHOD_REDRAW)))
     {
        CRI("Failed to setup the redraw method");
        goto del_methods;
     }

   /* Initialize the "eovim" method */
   if (EINA_UNLIKELY(! _method_eovim_init(E_METHOD_EOVIM)))
     {
        CRI("Failed to setup the eovim method");
        goto del_methods;
     }

   return EINA_TRUE;

del_methods:
   for (unsigned int j = 0; j < EINA_C_ARRAY_LENGTH(_methods); j++)
     _method_free(&(_methods[j]));
fail:
   for (i--; i >= 0; i--)
     eina_stringshare_del(_keywords[i]);
   return EINA_FALSE;
}

void
nvim_event_shutdown(void)
{
   for (unsigned int i = 0; i < __KW_LAST; i++)
     eina_stringshare_del(_keywords[i]);
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_methods); i++)
     _method_free(&(_methods[i]));
}
