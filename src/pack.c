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
pack_non_implemented(msgpack_packer *pk EINA_UNUSED,
                     const void *obj EINA_UNUSED)
{
   CRI("This is unimplemented. Type is too generic to be packed");
}

static Eina_Bool
_pack_dict_obj(const Eina_Hash *dict EINA_UNUSED,
               const void *key,
               void *data,
               void *fdata)
{
   msgpack_packer *const pk = fdata;
   Eina_Stringshare *const key_str = key;
   const Eina_Value *const value = data;
   const size_t key_size = (size_t)eina_stringshare_strlen(key_str);

   msgpack_pack_str(pk, key_size);
   msgpack_pack_str_body(pk, key, key_size);
   pack_object(pk, value);

   return EINA_TRUE;
}

void
pack_dictionary(msgpack_packer *pk,
                const Eina_Hash *dict)
{
   if (dict)
     {
        msgpack_pack_map(pk, (size_t)eina_hash_population(dict));
        eina_hash_foreach(dict, _pack_dict_obj, pk);
     }
   else
     msgpack_pack_map(pk, 0);
}

void
pack_boolean(msgpack_packer *pk,
             Eina_Bool boolean)
{
   static int (*const funcs[])(msgpack_packer *) = {
      msgpack_pack_false,
      msgpack_pack_true,
   };
   funcs[!!boolean](pk);
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
pack_list_of_strings(msgpack_packer *pk,
                     const Eina_List *list_str)
{
   Eina_List *l;
   Eina_Stringshare *str;

   EINA_LIST_FOREACH(list_str, l, str)
      pack_stringshare(pk, str);
}

void
pack_object(msgpack_packer *pk,
            const Eina_Value *object)
{
   const Eina_Value_Type *const type = eina_value_type_get(object);
   if (type == ENVIM_VALUE_TYPE_BOOL)
     {
        Eina_Bool value;
        eina_value_get(object, &value);
        value ? msgpack_pack_true(pk) : msgpack_pack_false(pk);
     }
   else
     {
        ERR("Unhandled object type '%s'", eina_value_type_name_get(type));
     }
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
   return pack_single_stringshare_get(&args->ptr[0]);
}

Eina_Stringshare *
pack_single_stringshare_get(const msgpack_object *obj)
{
   if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_STR))
     {
        ERR("Object does not contain a string");
        return NULL;
     }
   else
     {
        const msgpack_object_str *const str = &(obj->via.str);
        return eina_stringshare_add_length(str->ptr, str->size);
     }
}

Eina_Value *
pack_object_get(const msgpack_object_array *args)
{
   ARGS_CHECK_SIZE(args, 1, NULL);
   return pack_single_object_get(&args->ptr[0]);
}

Eina_Value *
pack_single_object_get(const msgpack_object *obj)
{
   Eina_Value *value = NULL;
   switch (obj->type)
     {
      case MSGPACK_OBJECT_ARRAY:
         /*
          * The array is a bit tricky, as the array is a set of single
          * objects. So this function actually has to call itself.
          * We stock a list of eina value in an eina value.
          */
           {
              const msgpack_object_array *const arr = &(obj->via.array);
              value = eina_value_list_new(ENVIM_VALUE_TYPE_NESTED);
              for (unsigned int i = 0; i < arr->size; i++)
                {
                   const msgpack_object *const arr_obj = &(arr->ptr[i]);
                   Eina_Value *const sub_value = pack_single_object_get(arr_obj);
                   /* Ignore values that could not be unpacked */
                   if (EINA_UNLIKELY(! sub_value)) { continue; }
                   eina_value_list_append(value, sub_value);
                }
           }
         break;

      case MSGPACK_OBJECT_MAP:
         /*
          * The map is similar to the array. The sole difference being that
          * elements are stored in an Eina_Hash instead of an Eina_List.
          * Key elements are all of type string!
          */
           {
              const msgpack_object_map *const map = &(obj->via.map);
              value = eina_value_hash_new(ENVIM_VALUE_TYPE_NESTED, 0);
              for (unsigned int i = 0; i < map->size; i++)
                {
                   const msgpack_object *const key = &(map->ptr[i].key);
                   const msgpack_object *const val = &(map->ptr[i].val);

                   Eina_Stringshare *const key_shr = pack_single_stringshare_get(key);
                   if (EINA_UNLIKELY(! key_shr)) { continue; }
                   Eina_Value *const sub_value = pack_single_object_get(val);
                   if (EINA_UNLIKELY(! sub_value))
                     {
                        eina_stringshare_del(key_shr);
                        continue;
                     }
                   eina_value_hash_set(value, key_shr, sub_value);
                }
           }
         break;


         /* Positive and negative integers are both handled as 64-bits int */
      case MSGPACK_OBJECT_POSITIVE_INTEGER: /* Fall through */
      case MSGPACK_OBJECT_NEGATIVE_INTEGER:
         value = eina_value_new(EINA_VALUE_TYPE_INT64);
         eina_value_set(value, obj->via.i64);
         break;

      case MSGPACK_OBJECT_BOOLEAN:
         value = eina_value_new(ENVIM_VALUE_TYPE_BOOL);
         eina_value_set(value, obj->via.boolean);
         break;

      case MSGPACK_OBJECT_STR:
           {
              Eina_Stringshare *const shr = eina_stringshare_add_length(
                 obj->via.str.ptr, obj->via.str.size
              );
              if (EINA_UNLIKELY(! shr))
                {
                   CRI("Failed to create stringshare from msgpack data");
                   return NULL;
                }
              value = eina_value_new(EINA_VALUE_TYPE_STRINGSHARE);
              eina_value_set(value, shr);
           }
         break;

      default:
         CRI("Unhandled object type 0x%x", obj->type); break;
     }

   return value;
}

Eina_List *
pack_array_get(const msgpack_object_array *args)
{
   Eina_List *list = NULL;

   for (unsigned int i = 0; i < args->size; i++)
     {
        Eina_Value *const value = pack_single_object_get(&args->ptr[i]);
        if (EINA_UNLIKELY(! value))
          {
             ERR("Failed to decode object. Skipping.");
             continue;
          }
        list = eina_list_append(list, value);
     }

   return list;
}

Eina_List *
pack_strings_get(const msgpack_object_array *args)
{
   Eina_List *list = NULL;

   for (unsigned int i = 0; i < args->size; i++)
     {
        /* We expect to receive strigns only. Skip non-strings with an error
         * message, but keep going */
        if (args->ptr[i].type != MSGPACK_OBJECT_STR)
          {
             ERR("Expected EXT type but got 0x%x", args->ptr[i].type);
             continue;
          }

        /* Create a stringshare from the msgpack string */
        const msgpack_object_str *const s = &(args->ptr[i].via.str);
        /* We may receive NIL lines for empty buffers. Skip them. */
        if (! s->ptr) { continue; }
        Eina_Stringshare *const str = eina_stringshare_add_length(s->ptr, s->size);
        if (EINA_UNLIKELY(! str))
          {
             CRI("Failed to create stringshare of size %"PRIu32, s->size);
             continue;
          }

        /* And finally store it in the list */
        list = eina_list_append(list, str);
     }

   return list;
}

void *
pack_non_implemented_get(const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Non implemented");
   return NULL;
}


/*============================================================================*
 *                                Dispatch API                                *
 *============================================================================*/

#define CHECK_ARG_TYPE(Arg, Type, ...) \
   if (EINA_UNLIKELY(eina_value_type_get(Arg) != Type)) { \
      CRI("Invalid type (expected %s, got %s)", \
          eina_value_type_name_get(Type), \
          eina_value_type_name_get(eina_value_type_get(Arg))); return __VA_ARGS__; }

Eina_Bool
pack_dispatch_boolean(Eina_Value *arg)
{
   CHECK_ARG_TYPE(arg, ENVIM_VALUE_TYPE_BOOL, EINA_FALSE);

   Eina_Bool value;
   eina_value_get(arg, &value);
   return value;
}

t_int
pack_dispatch_integer(Eina_Value *arg)
{
   CHECK_ARG_TYPE(arg, EINA_VALUE_TYPE_INT64, 0);

   t_int value;
   eina_value_get(arg, &value);
   return value;
}

Eina_Stringshare *
pack_dispatch_stringshare(Eina_Value *arg)
{
   CHECK_ARG_TYPE(arg, EINA_VALUE_TYPE_STRINGSHARE, 0);

   Eina_Stringshare *value;
   eina_value_get(arg, &value);
   return value;
}

Eina_List *
pack_dispatch_list(Eina_Value *arg EINA_UNUSED)
{
   CRI("Not implemented");
   return NULL;
}

void *
pack_dispatch_tabpage(Eina_Value *arg EINA_UNUSED)
{
   CRI("Not implemented");
   return NULL;
}

Eina_Hash *
pack_dispatch_hash(Eina_Value *arg EINA_UNUSED)
{
   CRI("Not implemented");
   return NULL;
}
