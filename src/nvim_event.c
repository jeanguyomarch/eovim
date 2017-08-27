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

#include "Envim.h"

typedef enum
{
   KW_FOREGROUND,
   KW_BACKGROUND,
   KW_SPECIAL,
   KW_REVERSE,
   KW_ITALIC,
   KW_BOLD,
   KW_UNDERLINE,
   KW_UNDERCURL,

   __KW_LAST /* Sentinel */
} e_kw;
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


static const msgpack_object_array *
_array_of_args_extract(const msgpack_object_array *args)
{
   const msgpack_object *const obj = &(args->ptr[1]);
   if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_ARRAY))
     {
        CRI("Expected array type. Got 0x%x", obj->type);
        return NULL;
     }
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


Eina_Bool
nvim_event_resize(s_nvim *nvim,
                  const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   t_int rows, columns;
   GET_ARG(params, 0, t_int, &rows);
   GET_ARG(params, 1, t_int, &columns);

   gui_resize(&nvim->gui, (unsigned int)rows, (unsigned int)columns);
   return EINA_TRUE;
}

Eina_Bool
nvim_event_clear(s_nvim *nvim,
                 const msgpack_object_array *args EINA_UNUSED)
{
   gui_clear(&nvim->gui);
   return EINA_TRUE;
}

Eina_Bool
nvim_event_eol_clear(s_nvim *nvim EINA_UNUSED,
                     const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}

Eina_Bool
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

Eina_Bool
nvim_event_mode_info_set(s_nvim *nvim EINA_UNUSED,
                         const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_update_menu(s_nvim *nvim EINA_UNUSED,
                       const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_busy_start(s_nvim *nvim EINA_UNUSED,
                      const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_busy_stop(s_nvim *nvim EINA_UNUSED,
                     const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_mouse_on(s_nvim *nvim EINA_UNUSED,
                    const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_mouse_off(s_nvim *nvim EINA_UNUSED,
                     const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_mode_change(s_nvim *nvim EINA_UNUSED,
                       const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_set_scroll_region(s_nvim *nvim EINA_UNUSED,
                             const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_scroll(s_nvim *nvim EINA_UNUSED,
                  const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}

Eina_Bool
nvim_event_highlight_set(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, >=, 1);

   /* Array that holds pointer to values within hash tables.
    * We must ensure this array has been zeroes-out since we will rely on
    * NULL pointers to check whether a value was provided or not */
   const msgpack_object *objs[__KW_LAST];
   memset(objs, 0, sizeof(objs));

   /*
    * highlight arguments are arrays containing maps.
    */
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_ARRAY))
          {
             CRI("Expected an array type. Got 0x%x", obj->type);
             return EINA_FALSE;
          }
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);
        const msgpack_object *const arr_arg = &(arr->ptr[0]);
        if (EINA_UNLIKELY(arr_arg->type != MSGPACK_OBJECT_MAP))
          {
             CRI("Expected a map type. Got 0x%x", arr_arg->type);
             return EINA_FALSE;
          }

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
             if (EINA_UNLIKELY(key_obj->type != MSGPACK_OBJECT_STR))
               {
                  CRI("Expected a string type. Got 0x%x", key_obj->type);
                  return EINA_FALSE;
               }
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
             for (unsigned int k = 0; k < __KW_LAST; k++)
               {
                  if (shr_key == KW(k))
                    {
                       objs[k] = &(kv->val);
                       break;
                    }
               }

             eina_stringshare_del(shr_key);
          }
     }

   s_gui_style style = {
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

Eina_Bool
nvim_event_put(s_nvim *nvim,
               const msgpack_object_array *args)
{
   /*
    * The 'put' command is much more complicated than what it should be.
    * We are suppose to receive a string, but we actually do receive arrays
    * of strings.
    */
   CHECK_BASE_ARGS_COUNT(args, >=, 1);

   static char buf[2048];
   const char *const max = &(buf[sizeof(buf)]);
   char *ptr = buf;

   /*
    * Go through all the arguments. Each argument is an array that contains
    * exactly one string. We concatenate all these strings together in one
    * static buffer (this is always called from the main thread, so no
    * concurrency shit).
    */
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_ARRAY))
          {
             CRI("Expected an array type. Got 0x%x", obj->type);
             return EINA_FALSE;
          }
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);
        const msgpack_object *const arr_arg = &(arr->ptr[0]);
        if (EINA_UNLIKELY(arr_arg->type != MSGPACK_OBJECT_STR))
          {
             CRI("Expected a string type. Got 0x%x", arr_arg->type);
             return EINA_FALSE;
          }

        const msgpack_object_str *const str = &(arr_arg->via.str);
        if (EINA_UNLIKELY(ptr + str->size >= max))
          {
             CRI("String is too big! Truncating!");
             break;
          }
        memcpy(ptr, str->ptr, str->size);
        ptr += str->size;
     }
   /* This is just for printing purposes. We actually don't care if the final
    * string is NUL-terminated or not. */
   *ptr = '\0';

   /* Finally, pass the string and its length to the gui */
   gui_put(&nvim->gui, buf, (unsigned int)(ptr - buf));

   return EINA_TRUE;
}

Eina_Bool
nvim_event_bell(s_nvim *nvim EINA_UNUSED,
                const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_visual_bell(s_nvim *nvim EINA_UNUSED,
                       const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_flush(s_nvim *nvim EINA_UNUSED,
                 const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
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
Eina_Bool
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
Eina_Bool
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
Eina_Bool
nvim_event_suspend(s_nvim *nvim EINA_UNUSED,
                   const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_set_title(s_nvim *nvim EINA_UNUSED,
                     const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_set_icon(s_nvim *nvim EINA_UNUSED,
                    const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_popupmenu_show(s_nvim *nvim EINA_UNUSED,
                          const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_popupmenu_hide(s_nvim *nvim EINA_UNUSED,
                          const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_popupmenu_select(s_nvim *nvim EINA_UNUSED,
                            const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
}
Eina_Bool
nvim_event_tabline_update(s_nvim *nvim EINA_UNUSED,
                          const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Unimplemented");
   return EINA_TRUE;
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
   return EINA_TRUE;

fail:
   for (i--; i >= 0; i--)
     eina_stringshare_del(_keywords[i]);
   return EINA_FALSE;
}

void
nvim_event_shutdown(void)
{
   for (unsigned int i = 0; i < __KW_LAST; i++)
     {
        eina_stringshare_del(_keywords[i]);
     }
}
