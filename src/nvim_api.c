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
#include "eovim/log.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim_event.h"
#include "eovim/nvim.h"

struct request
{
   /* The name field is pointless, but there is a bug in eina_chained_mempool,
    * and if the structure's size is less than a poitner's size, terrible
    * things happen */
   const char *name;
   uint32_t uid;
};

/* Mempool to allocate the requests */
static Eina_Mempool *_mempool = NULL;
/* Events */
static Eina_Hash *_callbacks = NULL;

static s_request *
_request_new(s_nvim *nvim,
             const char *rpc_name,
             size_t rpc_name_len)
{
   s_request *const req = eina_mempool_malloc(_mempool, sizeof(s_request));
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to allocate memory for a request object");
        return NULL;
     }

   req->name = rpc_name;
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
   Eina_List *it = NULL;

   EINA_LIST_FOREACH(nvim->requests, it, req)
     {
        if (req->uid == req_id) { break; }
     }
   return it;
}

void
nvim_api_request_free(s_nvim *nvim,
                      Eina_List *req_item)
{
   s_request *const req = eina_list_data_get(req_item);
   nvim->requests = eina_list_remove_list(nvim->requests, req_item);
   eina_mempool_free(_mempool, req);
}

Eina_Bool
nvim_api_ui_attach(s_nvim *nvim,
                   unsigned int width, unsigned int height,
                   Eina_Bool true_colors)
{
   const char api[] = "nvim_ui_attach";
   s_request *const req = _request_new(nvim, api, sizeof(api) - 1);
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to create request");
        return EINA_FALSE;
     }

   msgpack_packer *const pk = &nvim->packer;
   msgpack_pack_array(pk, 3);
   msgpack_pack_int64(pk, width);
   msgpack_pack_int64(pk, height);

   /* Pack the options */
   msgpack_pack_map(pk, 2);
   msgpack_pack_str(pk, 3); /* 'rgb' key */
   msgpack_pack_str_body(pk, "rgb", 3);
   /* 'rgb' value */
   if (true_colors) msgpack_pack_true(pk);
   else msgpack_pack_false(pk);
   msgpack_pack_str(pk, 13); /* 'ext_popupmenu' key */
   msgpack_pack_str_body(pk, "ext_popupmenu", 13);
   msgpack_pack_true(pk); /* 'ext_popupmenu' value */

   nvim->true_colors = true_colors;

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
nvim_api_event_dispatch(s_nvim *nvim,
                        Eina_Stringshare *command,
                        const msgpack_object_array *args)
{
   const f_event_cb cb = eina_hash_find(_callbacks, command);
   if (EINA_UNLIKELY(! cb))
     {
        CRI("Failed to get callback for command '%s'", command);
        return EINA_FALSE;
     }
   return cb(nvim, args);
}


Eina_Bool
nvim_api_init(void)
{
   struct {
      const char *const name;
      const unsigned int size;
      const f_event_cb func;
   } const ctor[] = {
#define CB(Name, Func) { .name = Name, .size = sizeof(Name) - 1, .func = Func }

   CB("resize", nvim_event_resize),
   CB("clear", nvim_event_clear),
   CB("eol_clear", nvim_event_eol_clear),
   CB("cursor_goto", nvim_event_cursor_goto),
   CB("mode_info_set", nvim_event_mode_info_set),
   CB("update_menu", nvim_event_update_menu),
   CB("busy_start", nvim_event_busy_start),
   CB("busy_stop", nvim_event_busy_stop),
   CB("mouse_on", nvim_event_mouse_on),
   CB("mouse_off", nvim_event_mouse_off),
   CB("mode_change", nvim_event_mode_change),
   CB("set_scroll_region", nvim_event_set_scroll_region),
   CB("scroll", nvim_event_scroll),
   CB("highlight_set", nvim_event_highlight_set),
   CB("put", nvim_event_put),
   CB("bell", nvim_event_bell),
   CB("visual_bell", nvim_event_visual_bell),
   CB("flush", nvim_event_flush),
   CB("update_fg", nvim_event_update_fg),
   CB("update_bg", nvim_event_update_bg),
   CB("update_sp", nvim_event_update_sp),
   CB("suspend", nvim_event_suspend),
   CB("set_title", nvim_event_set_title),
   CB("set_icon", nvim_event_set_icon),
   CB("popupmenu_show", nvim_event_popupmenu_show),
   CB("popupmenu_hide", nvim_event_popupmenu_hide),
   CB("popupmenu_select", nvim_event_popupmenu_select),
   CB("tabline_update", nvim_event_tabline_update),
#undef CB
   };

   _mempool = eina_mempool_add("chained_mempool", "s_request",
                               NULL, sizeof(s_request), 8);
   if (EINA_UNLIKELY(! _mempool))
     {
        CRI("Failed to initialize chained mempool");
        goto fail;
     }

   /* Generate a hash table that will contain the callbacks to be called for
    * each event sent by neovim. */
   _callbacks = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(! _callbacks))
     {
        CRI("Failed to create hash table to hold callbacks");
        goto hash_fail;
     }

   /*
    * Add the events in the hash table. It is the data descriptor that is
    * templated and not this function, as this will yield more compact code.
    */
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(ctor); i++)
     {
        Eina_Stringshare *const cmd = eina_stringshare_add_length(
           ctor[i].name, ctor[i].size
        );
        if (EINA_UNLIKELY(! cmd))
          {
             CRI("Failed to create stringshare from '%s', size %u",
                 ctor[i].name, ctor[i].size);
             goto fill_hash_fail;
          }
        if (! eina_hash_add(_callbacks, cmd, ctor[i].func))
          {
             CRI("Failed to register callback %p for command '%s'",
                 ctor[i].func, cmd);
             eina_stringshare_del(cmd);
             goto fill_hash_fail;
          }
     }

   return EINA_TRUE;
fill_hash_fail:
   eina_hash_free(_callbacks);
hash_fail:
   eina_mempool_del(_mempool);
fail:
   return EINA_FALSE;
}

void
nvim_api_shutdown(void)
{
   eina_mempool_del(_mempool);
   eina_hash_free(_callbacks);
}
