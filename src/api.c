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

/*
 * Note: this table shall be indexed starting from 1 (0 being a dummy space
 * that is reserved to the invalid value
 */
static Eina_Stringshare *_api_keys[__API_KEY_LAST] =
{
   [API_KEY_INVALID]    = NULL,
   [API_KEY_VERSION]    = "version",
   [API_KEY_FUNCTIONS]  = "functions",
   [API_KEY_TYPES]      = "types",
   [API_KEY_UI_EVENTS]  = "ui_events",
};

Eina_Bool
api_init(void)
{
   for (unsigned int i = 1; i < EINA_C_ARRAY_LENGTH(_api_keys); i++)
     {
        _api_keys[i] = eina_stringshare_add(_api_keys[i]);
     }
   return EINA_TRUE;
}

void
api_shutdown(void)
{
   for (unsigned int i = 1; i < EINA_C_ARRAY_LENGTH(_api_keys); i++)
     {
        eina_stringshare_del(_api_keys[i]);
        _api_keys[i] = NULL;
     }
}

e_api_key
api_key_for_string_get(Eina_Stringshare *key)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(key, API_KEY_INVALID);

   for (unsigned int i = 1; i < EINA_C_ARRAY_LENGTH(_api_keys); i++)
     {
         if (_api_keys[i] == key) { return i; }
     }
   return API_KEY_INVALID;
}

Eina_Bool
api_version_set(const msgpack_object *obj,
                s_version *version)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(obj, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(version, EINA_FALSE);

   if (obj->type != MSGPACK_OBJECT_MAP)
     {
        ERR("Expected map type of object");
        return EINA_FALSE;
     }
   const msgpack_object_map *const map = &(obj->via.map);

   /*  For now, we are just interested in the version X.Y.Z. */
   struct {
      unsigned int *const ptr;
      const char *const key;
   } const mapper[] = {
        { .key = "major", .ptr = &version->major, },
        { .key = "minor", .ptr = &version->minor, },
        { .key = "patch", .ptr = &version->patch, },
   };

   for (unsigned int i = 0; i < map->size; i++)
     {
        if (map->ptr[i].key.type != MSGPACK_OBJECT_STR)
          {
             ERR("Key is not of type string. Skipping.");
             continue;
          }
        const msgpack_object_str *const key = &(map->ptr[i].key.via.str);
        Eina_Bool found = EINA_FALSE;
        for (unsigned int j = 0; j < EINA_C_ARRAY_LENGTH(mapper); j++)
          {
            if (! strncmp(key->ptr, mapper[j].key, key->size))
              {
                 if (map->ptr[i].val.type != MSGPACK_OBJECT_POSITIVE_INTEGER)
                   {
                      ERR("Expected integer type for version key '%s'",
                          mapper[j].key);
                      return EINA_FALSE;
                   }

                 *mapper[j].ptr = (unsigned int)(map->ptr[i].val.via.u64);
                 found = EINA_TRUE;
                 break;
              }
          }

        /*
         * If the API parameter was not matched earlier, we don't handle it.
         * No worries, but maybe we will need this later. So we log that.
         */
        if (! found)
          {
             char buf[128];
             memcpy(buf, key->ptr, key->size);
             buf[key->size] = '\0';
             DBG("Unhandled version parameter '%s'", buf);
          }
     }

   return EINA_TRUE;
}
