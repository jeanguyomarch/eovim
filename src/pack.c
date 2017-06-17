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

/*============================================================================*
 *                                 Packing API                                *
 *============================================================================*/

void
pack_object(msgpack_packer *pk,
            const s_object *obj)
{
   msgpack_pack_int64(pk, obj->id);
}

void
pack_buffer(msgpack_packer *pk,
            const s_buffer *buf)
{
   pack_object(pk, &buf->obj);
}

void
pack_window(msgpack_packer *pk,
            const s_window *win)
{
   pack_object(pk, &win->obj);
}

void
pack_tabpage(msgpack_packer *pk,
             const s_tabpage *tab)
{
   pack_object(pk, &tab->obj);
}

void
pack_non_implemented(msgpack_packer *pk EINA_UNUSED,
                     const void *obj EINA_UNUSED)
{
   CRI("This is unimplemented. Type is too generic to be packed");
}

void
pack_boolean(msgpack_packer *pk,
             Eina_Bool boolean)
{
   if (boolean == EINA_FALSE) msgpack_pack_false(pk);
   else msgpack_pack_true(pk);
}

void
pack_stringshare(msgpack_packer *pk,
                 Eina_Stringshare *str)
{
   const size_t len = (size_t)eina_stringshare_strlen(str);
   msgpack_pack_bin(pk, len);
   msgpack_pack_bin_body(pk, str, len);
}

void
pack_position(msgpack_packer *pk,
              s_position pos)
{
   msgpack_pack_array(pk, 2);
   msgpack_pack_int64(pk, pos.x);
   msgpack_pack_int64(pk, pos.y);
}

void
pack_list_of_windows(msgpack_packer *pk,
                     const Eina_List *list_win)
{
   Eina_List *l;
   const s_window *win;

   EINA_LIST_FOREACH(list_win, l, win)
      pack_window(pk, win);
}

void
pack_list_of_buffers(msgpack_packer *pk,
                     const Eina_List *list_buf)
{
   Eina_List *l;
   const s_buffer *buf;

   EINA_LIST_FOREACH(list_buf, l, buf)
      pack_buffer(pk, buf);
}

void
pack_list_of_tabpages(msgpack_packer *pk,
                      const Eina_List *list_tab)
{
   Eina_List *l;
   const s_tabpage *tab;

   EINA_LIST_FOREACH(list_tab, l, tab)
      pack_tabpage(pk, tab);
}

void
pack_list_of_strings(msgpack_packer *pk,
                     const Eina_List *list_str)
{
   Eina_List *l;
   Eina_Stringshare *str;

   EINA_LIST_FOREACH(list_str, l, str)
      pack_stringshare(pk, str);
}


/*============================================================================*
 *                                Unpacking API                               *
 *============================================================================*/

#define ARGS_CHECK_SIZE(ARGS, SIZE, RET)                                       \
   if (EINA_UNLIKELY(ARGS->size != (SIZE)))                                    \
     {                                                                         \
        ERR("Array contains %"PRIu32" elements instead of %u", ARGS->size, SIZE); \
        return RET;                                                            \
     }

Eina_Bool
pack_boolean_get(const msgpack_object_array *args)
{
   ARGS_CHECK_SIZE(args, 1, EINA_FALSE);
   if (EINA_UNLIKELY(args->ptr[0].type != MSGPACK_OBJECT_BOOLEAN))
     {
        ERR("Object does not contain a boolean value");
        return EINA_FALSE;
     }
   return (args->ptr[0].via.boolean == 0) ? EINA_FALSE : EINA_TRUE;
}

s_position
pack_position_get(const msgpack_object_array *args)
{
   ARGS_CHECK_SIZE(args, 2, position_make(-1, -1));

   if (EINA_UNLIKELY((args->ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&
                     (args->ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER)))
     {
        ERR("Object does not contain two positive integers");
        return position_make(-1, -1);
     }
   return position_make(args->ptr[0].via.i64, args->ptr[1].via.i64);
}

t_int
pack_int_get(const msgpack_object_array *args)
{
   ARGS_CHECK_SIZE(args, 1, 0);

   if (EINA_UNLIKELY((args->ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&
                     (args->ptr[0].type != MSGPACK_OBJECT_NEGATIVE_INTEGER)))
     {
        ERR("Object does not contain an integer");
        return 0;
     }
   return args->ptr[0].via.i64;
}

Eina_Stringshare *
pack_stringshare_get(const msgpack_object_array *args)
{
   ARGS_CHECK_SIZE(args, 1, NULL);

   if (EINA_UNLIKELY(args->ptr[0].type != MSGPACK_OBJECT_STR))
     {
        ERR("Object does not contain a string");
        return NULL;
     }
   const msgpack_object_str *const str = &(args->ptr[0].via.str);
   return eina_stringshare_add_length(str->ptr, str->size);
}

s_object *
pack_object_get(const msgpack_object_array *args)
{
   CRI("Unimplemented"); (void) args;
   return NULL;
}

s_window *
pack_window_get(const msgpack_object_array *args)
{
   CRI("Unimplemented"); (void) args;
   return NULL;
}

s_buffer *
pack_buffer_get(const msgpack_object_array *args)
{
   CRI("Unimplemented"); (void) args;
   return NULL;
}

s_tabpage *
pack_tabpage_get(const msgpack_object_array *args)
{
   CRI("Unimplemented"); (void) args;
   return NULL;
}

Eina_List *
pack_tabpages_get(const msgpack_object_array *args)
{
   Eina_List *list = NULL;
   s_tabpage *tab = NULL;
   for (unsigned int i = 0; i < args->size; i++)
     {
        if (EINA_UNLIKELY(args->ptr[i].type != MSGPACK_OBJECT_EXT))
          {
             ERR("Expected MSGPACK_OBJECT_EXT type");
             goto fail;
          }

        const msgpack_object_ext *const obj = &(args->ptr[i].via.ext);
        if (EINA_UNLIKELY(obj->type != PACK_EXT_TABPAGE))
          {
             ERR("Subtype is not PACK_EXT_TABPAGE");
             goto fail;
          }
        if (EINA_UNLIKELY(obj->size != 1))
          {
             ERR("One element is expected but %"PRIu32" were provided", obj->size);
             goto fail;
          }

        const t_int id = obj->ptr[0];

        list = eina_list_append(list, tab);
     }
   return list;

fail:
   EINA_LIST_FREE(list, tab);
   return NULL;
}
