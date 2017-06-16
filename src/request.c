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

#if 0
#define CHECK_ARG_TYPE(ARG, TYPE, ...) \
   do { \
      if (ARG.type != TYPE) { \
         ERR("Argument has a type value of 0x%x, but %s was expected", \
             ARG.type, #TYPE); \
         return __VA_ARGS__; \
      } \
   } while (0)

static void
_response_get_api_info(s_nvim *nvim,
                       const s_request *req,
                       const msgpack_object_array *args)
{
   /* 1st argument is the channel. We don't care */
   /* 2nd argument is the metadata */
   CHECK_ARG_TYPE(args->ptr[1], MSGPACK_OBJECT_MAP);

   char buf[1024];
   const msgpack_object_map *const map = &(args->ptr[1].via.map);
   for (unsigned int i = 0; i < map->size; i++)
     {
        if (map->ptr[i].key.type != MSGPACK_OBJECT_STR)
          {
             ERR("Key is not of type string. Skipping.");
             continue;
          }
        const msgpack_object_str *const key = &(map->ptr[i].key.via.str);
        Eina_Stringshare *const key_str = eina_stringshare_add_length(
           key->ptr, key->size
        );
        const msgpack_object *const val = &(map->ptr[i].val);

        const e_api_key api_key = api_key_for_string_get(key_str);
        switch (api_key)
          {
           case API_KEY_VERSION:
              api_version_set(val, &nvim->api.version);
              DBG("Neovim version: %u.%u.%u",
                  nvim->api.version.major,
                  nvim->api.version.minor,
                  nvim->api.version.patch);
              break;

           case API_KEY_FUNCTIONS:
              api_functions_set(val);
              break;

           default:
              WRN("Unhandled key '%s'", key_str);
              break;
          }
     }
}
#endif

s_request *
request_new(uint64_t req_uid,
            e_request req_type)
{
   s_request *const req = calloc(1, sizeof(req));
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to allocate memory for a request object");
        return NULL;
     }

   req->uid = req_uid;
   req->type = req_type;

   return req;
}

void
request_free(s_request *req)
{
   if (req)
     {
        free(req);
     }
}
