/*
 * Copyright (c) 2018 Jean Guyomarc'h
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

#include "event.h"

static Eina_Stringshare *_shapes[] =
{
   [CURSOR_SHAPE_BLOCK] = "block",
   [CURSOR_SHAPE_HORIZONTAL] = "horizontal",
   [CURSOR_SHAPE_VERTICAL] = "vertical",
};

static Eina_Hash *_modes;

static s_mode *
_mode_new(Eina_Stringshare *name,
          const char *short_name,
          size_t short_size)
{
   s_mode *const mode = calloc(1, sizeof(s_mode) + short_size);
   if (EINA_UNLIKELY(! mode))
     {
        CRI("Failed to allocate memory");
        return NULL;
     }

   mode->name = eina_stringshare_ref(name);
   memcpy(mode->short_name, short_name, short_size);
   mode->short_name[short_size] = '\0';

   return mode;
}

static void
_mode_free(s_mode *mode)
{
   eina_stringshare_del(mode->name);
   free(mode);
}

static e_cursor_shape
_mode_cursor_shape_convert(Eina_Stringshare *shape_name)
{
   /* Try to match every possible shape. On match, return the index in the
    * table, which is the e_cursor_shape value. */
   for (size_t i = 0u; i < EINA_C_ARRAY_LENGTH(_shapes); i++)
     if (shape_name == _shapes[i]) { return i; }

   /* Nothing found?! Fallback to block */
   ERR("Failed to find a cursor shape for '%s'", shape_name);
   return CURSOR_SHAPE_BLOCK;
}

static Eina_Bool
_mode_info_set(const msgpack_object_array *params)
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
        s_mode *mode = eina_hash_find(_modes, name);
        if (! mode)
          {
             const msgpack_object_str *const sname = &(_GET_OBJ(KW_SHORT_NAME)->via.str);
             mode = _mode_new(name, sname->ptr, sname->size);
             eina_hash_direct_add(_modes, mode->name, mode);
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
                      mode->cursor_shape = _mode_cursor_shape_convert(shr);
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


/*****************************************************************************/

Eina_Bool
nvim_event_mode_info_set(s_nvim *nvim EINA_UNUSED,
                         const msgpack_object_array *args)
{
   Eina_Bool ret = EINA_TRUE;
   for (unsigned int i = 1u; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const params = &(obj->via.array);
        CHECK_ARGS_COUNT(params, ==, 2);

        ret &= _mode_info_set(params);
     }
   return ret;
}

Eina_Bool
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

        const s_mode *const mode = eina_hash_find(_modes, name);
        gui_mode_update(&nvim->gui, mode);
        eina_stringshare_del(name);
     }
   return EINA_TRUE;
}

/*****************************************************************************/

Eina_Bool
mode_init(void)
{
   int i = 0;

   /* Create stringshares from the static strings. This allows much much
    * faster string comparison later on */
   for (; i < (int)EINA_C_ARRAY_LENGTH(_shapes); i++)
     {
       _shapes[i] = eina_stringshare_add(_shapes[i]);
       if (EINA_UNLIKELY(! _shapes[i]))
         {
            CRI("Failed to create stringshare");
            goto fail;
         }
     }

   /* Create the hash map that will contain the modes */
   _modes = eina_hash_stringshared_new(EINA_FREE_CB(_mode_free));
   if (EINA_UNLIKELY(! _modes))
     {
        CRI("Failed to create Eina_Hash");
        goto fail;
     }

   return EINA_TRUE;

fail:
   for (--i; i >= 0; --i)
     eina_stringshare_del(_shapes[i]);
   return EINA_FALSE;
}

void
mode_shutdown(void)
{
   for (size_t i = 0u; i < EINA_C_ARRAY_LENGTH(_shapes); i++)
     eina_stringshare_del(_shapes[i]);
}
