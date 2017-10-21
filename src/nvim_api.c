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
#include "eovim/config.h"
#include "eovim/log.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim_event.h"
#include "eovim/nvim.h"

struct request
{
   struct {
      f_nvim_api_cb func;
      void *data;
   } cb;
   uint32_t uid;
};

/* Mempool to allocate the requests */
static Eina_Mempool *_mempool = NULL;


static s_request *
_request_new(s_nvim *nvim,
             const char *rpc_name,
             size_t rpc_name_len)
{
   s_request *const req = eina_mempool_calloc(_mempool, sizeof(s_request));
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to allocate memory for a request object");
        return NULL;
     }

   req->uid = nvim_next_uid_get(nvim);
   DBG("Preparing request '%s' with id %"PRIu32, rpc_name, req->uid);

   /* Clear the serialization buffer before pushing a new request */
   msgpack_sbuffer_clear(&nvim->sbuffer);

   /* Keep the request around */
   nvim->requests = eina_list_append(nvim->requests, req);

   msgpack_packer *const pk = &nvim->packer;
   /*
    * Pack the message! It is an array of four (4) items:
    *  - the rpc type:
    *    - 0 is a request
    *  - the unique identifier for a request
    *  - the method (API string)
    *  - the arguments count as an array.
    */
   msgpack_pack_array(pk, 4);
   msgpack_pack_int(pk, 0);
   msgpack_pack_uint32(pk, req->uid);
   msgpack_pack_bin(pk, rpc_name_len);
   msgpack_pack_bin_body(pk, rpc_name, rpc_name_len);

   return req;
}

static void
_request_cleanup(s_nvim *nvim,
                 s_request *req)
{
   Eina_List *const list = nvim_api_request_find(nvim, req->uid);
   if (EINA_LIKELY(list != NULL))
     {
        nvim_api_request_free(nvim, list);
     }
}

static Eina_Bool
_request_send(s_nvim *nvim,
              s_request *req)
{
   /* Finally, send that to the slave neovim process */
   const Eina_Bool ok = ecore_exe_send(
      nvim->exe, nvim->sbuffer.data, (int)nvim->sbuffer.size
   );
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to send %zu bytes to neovim", nvim->sbuffer.size);
        _request_cleanup(nvim, req);
        return EINA_FALSE;
     }
   DBG("Sent %zu bytes to neovim", nvim->sbuffer.size);
   return EINA_TRUE;
}

Eina_List *
nvim_api_request_find(const s_nvim *nvim,
                      uint32_t req_id)
{
   const s_request *req;
   Eina_List *it;

   EINA_LIST_FOREACH(nvim->requests, it, req)
     {
        if (req->uid == req_id) { return it; }
     }
   return NULL;
}

void
nvim_api_request_free(s_nvim *nvim,
                      Eina_List *req_item)
{
   s_request *const req = eina_list_data_get(req_item);
   nvim->requests = eina_list_remove_list(nvim->requests, req_item);
   eina_mempool_free(_mempool, req);
}

void nvim_api_request_call(s_nvim *nvim,
                           const Eina_List *req_item,
                           const msgpack_object *result)
{
   const s_request *const req = eina_list_data_get(req_item);
   if (req->cb.func) req->cb.func(nvim, req->cb.data, result);
}

Eina_Bool
nvim_api_ui_attach(s_nvim *nvim,
                   unsigned int width,
                   unsigned int height)
{
   const char api[] = "nvim_ui_attach";
   s_request *const req = _request_new(nvim, api, sizeof(api) - 1);
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to create request");
        return EINA_FALSE;
     }

   const s_config *const cfg = nvim->config;

   msgpack_packer *const pk = &nvim->packer;
   msgpack_pack_array(pk, 3);
   msgpack_pack_int64(pk, width);
   msgpack_pack_int64(pk, height);

   /* Pack the options. There are 2: rgb & ext_popupmenu. */
   msgpack_pack_map(pk, 2);

   /* Pack the RGB option (boolean) */
     {
        const char key[] = "rgb";
        const size_t len = sizeof(key) - 1;
        msgpack_pack_str(pk, len);
        msgpack_pack_str_body(pk, key, len);
        if (cfg->true_colors) msgpack_pack_true(pk);
        else msgpack_pack_false(pk);
        nvim->true_colors = cfg->true_colors;
     }

   /* Pack the External popupmemnu */
     {
        const char key[] = "ext_popupmenu";
        const size_t len = sizeof(key) - 1;
        msgpack_pack_str(pk, len);
        msgpack_pack_str_body(pk, key, len);
        if (cfg->ext_popup) msgpack_pack_true(pk);
        else msgpack_pack_false(pk);
     }

   return _request_send(nvim, req);
}

Eina_Bool
nvim_api_ui_try_resize(s_nvim *nvim,
                       unsigned int width, unsigned height)
{
   const char api[] = "nvim_ui_try_resize";
   s_request *const req = _request_new(nvim, api, sizeof(api) - 1);
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to create request");
        return EINA_FALSE;
     }

   msgpack_packer *const pk = &nvim->packer;
   msgpack_pack_array(pk, 2);
   msgpack_pack_int64(pk, width);
   msgpack_pack_int64(pk, height);

   return _request_send(nvim, req);
}

Eina_Bool
nvim_api_eval(s_nvim *nvim,
              const char *input,
              unsigned int input_size,
              f_nvim_api_cb func,
              void *func_data)
{
   const char api[] = "nvim_eval";
   s_request *const req = _request_new(nvim, api, sizeof(api) - 1);
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to create request");
        return EINA_FALSE;
     }
   DBG("Evaluating VimL: %s", input);
   req->cb.func = func;
   req->cb.data = func_data;

   msgpack_packer *const pk = &nvim->packer;
   msgpack_pack_array(pk, 1);
   msgpack_pack_str(pk, input_size);
   msgpack_pack_str_body(pk, input, input_size);

   return _request_send(nvim, req);
}

Eina_Bool
nvim_api_command_output(s_nvim *nvim,
                        const char *input,
                        unsigned int input_size,
                        f_nvim_api_cb func,
                        void *func_data)
{
   const char api[] = "nvim_command_output";
   s_request *const req = _request_new(nvim, api, sizeof(api) - 1);
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to create request");
        return EINA_FALSE;
     }
   DBG("Running nvim command: %s", input);
   req->cb.func = func;
   req->cb.data = func_data;

   msgpack_packer *const pk = &nvim->packer;
   msgpack_pack_array(pk, 1);
   msgpack_pack_str(pk, input_size);
   msgpack_pack_str_body(pk, input, input_size);

   return _request_send(nvim, req);
}

Eina_Bool
nvim_api_var_integer_set(s_nvim *nvim,
                         const char *name,
                         int value)
{
   const char api[] = "nvim_set_var";
   s_request *const req = _request_new(nvim, api, sizeof(api) - 1);
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to create request");
        return EINA_FALSE;
     }
   const size_t name_size = strlen(name);

   msgpack_packer *const pk = &nvim->packer;
   msgpack_pack_array(pk, 2);
   msgpack_pack_str(pk, name_size);
   msgpack_pack_str_body(pk, name, name_size);
   msgpack_pack_int(pk, value);

   return _request_send(nvim, req);
}

Eina_Bool
nvim_api_input(s_nvim *nvim,
               const char *input,
               unsigned int input_size)
{
   const char api[] = "nvim_input";
   s_request *const req = _request_new(nvim, api, sizeof(api) - 1);
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to create request");
        return EINA_FALSE;
     }

   msgpack_packer *const pk = &nvim->packer;
   msgpack_pack_array(pk, 1);
   msgpack_pack_str(pk, input_size);
   msgpack_pack_str_body(pk, input, input_size);

   return _request_send(nvim, req);
}

Eina_Bool
nvim_api_init(void)
{
   _mempool = eina_mempool_add("chained_mempool", "s_request",
                               NULL, sizeof(s_request), 8);
   if (EINA_UNLIKELY(! _mempool))
     {
        CRI("Failed to initialize chained mempool");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

void
nvim_api_shutdown(void)
{
   eina_mempool_del(_mempool);
}
