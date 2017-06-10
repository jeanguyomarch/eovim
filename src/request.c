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

           default:
              WRN("Unhandled key '%s'", key_str);
              break;
          }
     }
}


static s_request_info _requests_api[__REQUEST_LAST] =
{
   [REQUEST_GET_API_INFO] = {
      .method = "nvim_get_api_info",
      .in_args_count = 0,
      .out_min_args = 2,
      .out_max_args = 2,
      .handler = _response_get_api_info,
   },
};


Eina_Bool
request_init(void)
{
   /*
    * Ahah, there is a trick here! As stringshares can only be obtained at run
    * time, we initialize the method with a constant string, which is then fed
    * back to the structure field after Eina has been initted.
    *
    * We obtain a 'method' field that self-describes its length and can be
    * easily compared with other things.
    */
   for (unsigned int i = 0; i < __REQUEST_LAST; i++)
     {
        s_request_info *const info = &(_requests_api[i]);
        info->method = eina_stringshare_add(info->method);
     }

   return EINA_TRUE;
}

void
request_shutdown(void)
{
   for (unsigned int i = 0; i < __REQUEST_LAST; i++)
     {
        s_request_info *const info = &(_requests_api[i]);
        eina_stringshare_del(info->method);
     }
}

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

const s_request_info *
request_info_get(const s_request *req)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(req, NULL);
   return &(_requests_api[req->type]);
}
