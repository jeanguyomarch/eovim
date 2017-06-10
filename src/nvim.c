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

enum
{
   HANDLER_ADD,
   HANDLER_DEL,
   HANDLER_DATA,
   HANDLER_ERROR,
   __HANDLERS_LAST /* Sentinel, don't use */
};

typedef struct
{
   int type;
   uint64_t id;
} s_response;

static Ecore_Event_Handler *_event_handlers[__HANDLERS_LAST];
static Eina_Hash *_nvim_instances = NULL;


/*============================================================================*
 *                                 Private API                                *
 *============================================================================*/

static void
_nvim_free_cb(void *data)
{
   /*
    * It is this function that is actually responsible in freeing the
    * resources reserved by a s_nvim structure. When entering this function,
    * data is a valid instacne to be freed.
    */

   s_nvim *const nvim = data;

   msgpack_sbuffer_destroy(&nvim->sbuffer);
   free(nvim);
}

static Eina_List *
_nvim_request_find(const s_nvim *nvim,
                   uint64_t req_id)
{
   const s_request *req;
   Eina_List *it = NULL;

   EINA_LIST_FOREACH(nvim->requests, it, req)
     {
        if (req->uid == req_id) { break; }
     }
   return it;
}

static uint64_t
_nvim_get_next_uid(s_nvim *nvim)
{
   return nvim->request_id++;
}

static s_nvim *
_nvim_get(const Ecore_Exe *exe)
{
   return eina_hash_find(_nvim_instances, &exe);
}

static Eina_Bool
_handle_request_response(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   /* 2nd arg should be an integer */
   if (args->ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER)
     {
        ERR("Second argument in response is expected to be an integer");
        goto fail;
     }

   /* Get the request from the pending requests list. */
   const uint64_t req_id = args->ptr[1].via.u64;
   Eina_List *const req_item = _nvim_request_find(nvim, req_id);
   if (EINA_UNLIKELY(! req_item))
     {
        CRI("Mh... received a response to request %"PRIu64", but is was not "
            "registered. Something wrong happend somewhere!", req_id);
        goto fail;
     }
   DBG("Received response to request %"PRIu64, req_id);

   /* Found the request, we can now get the data contained within the list */
   s_request *const req = eina_list_data_get(req_item);

   /* Now that we have found the request, we can remove it from the pending
    * list */
   nvim->requests = eina_list_remove_list(nvim->requests, req_item);

   /* 4th arg should be an array */
   if (args->ptr[3].type != MSGPACK_OBJECT_ARRAY)
     {
        ERR("Fourth argument in response is expected to be an array");
        goto fail;
     }

   /* And finally call the handler associated to the request type */
   const s_request_info *const info = request_info_get(req);
   const msgpack_object_array *const out_args = &(args->ptr[3].via.array);

   if ((out_args->size < info->out_min_args) ||
       (out_args->size > info->out_max_args))
     {
        ERR("Invalid arguments in response to '%s': %u have been provided and"
            " is not in range [%u,%u]",
            info->method, out_args->size,
            info->out_min_args, info->out_max_args);
        goto fail;
     }
   info->handler(nvim, req, out_args);

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}


/*============================================================================*
 *                       Nvim Processes Events Handlers                       *
 *============================================================================*/

static Eina_Bool
_nvim_added_cb(void *data EINA_UNUSED,
               int   type EINA_UNUSED,
               void *event)
{
   const Ecore_Exe_Event_Add *const info = event;
   INF("Process with PID %i was created", ecore_exe_pid_get(info->exe));
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_nvim_deleted_cb(void *data EINA_UNUSED,
                 int   type EINA_UNUSED,
                 void *event)
{
   const Ecore_Exe_Event_Del *const info = event;
   INF("Process with PID %i died", ecore_exe_pid_get(info->exe));
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_nvim_received_data_cb(void *data EINA_UNUSED,
                       int   type EINA_UNUSED,
                       void *event)
{
   const Ecore_Exe_Event_Data *const info = event;
   s_nvim *const nvim = _nvim_get(info->exe);

   /* Deserialize the received message */
   msgpack_object deserialized;
   msgpack_unpack(info->data, (size_t)info->size, NULL,
                  &nvim->mempool, &deserialized);
   msgpack_object_print(stderr, deserialized);
   fprintf(stderr, "\n");

   if (deserialized.type != MSGPACK_OBJECT_ARRAY)
     {
        ERR("Unexpected msgpack type 0x%x", deserialized.type);
        goto end;
     }

   const msgpack_object_array *const args = &deserialized.via.array;
   const unsigned int expected_size = 4;
   if (args->size != expected_size)
     {
        ERR("Expected response as an array of %u elements. Got %u.",
            expected_size, args->size);
        goto end;
     }

   if (args->ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER)
     {
        ERR("First argument in response is expected to be an integer");
        goto end;
     }
   switch (args->ptr[0].via.u64)
     {
      case 1:
         _handle_request_response(nvim, args);
         break;

      case 2:
         ERR("Notification received. It is unimplemented :'(");
         goto end;

      default:
         ERR("Invalid message identifier %"PRIu64, args->ptr[0].via.u64);
         goto end;
     }

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_nvim_received_error_cb(void *data EINA_UNUSED,
                        int   type EINA_UNUSED,
                        void *event)
{
   const Ecore_Exe_Event_Data *const info = event;
   (void) info;
   return ECORE_CALLBACK_PASS_ON;
}



/*============================================================================*
 *                                 Public API                                 *
 *============================================================================*/

Eina_Bool
nvim_init(void)
{
   struct {
      int event;
      Ecore_Event_Handler_Cb callback;
   } const ctor[__HANDLERS_LAST] = {
      [HANDLER_ADD] = {
         .event = ECORE_EXE_EVENT_ADD,
         .callback = _nvim_added_cb,
      },
      [HANDLER_DEL] = {
         .event = ECORE_EXE_EVENT_DEL,
         .callback = _nvim_deleted_cb,
      },
      [HANDLER_DATA] = {
         .event = ECORE_EXE_EVENT_DATA,
         .callback = _nvim_received_data_cb,
      },
      [HANDLER_ERROR] = {
         .event = ECORE_EXE_EVENT_ERROR,
         .callback = _nvim_received_error_cb,
      },
   };
   unsigned int i;

   /* Create the handlers for all incoming events for spawned nvim instances */
   for (i = 0; i < EINA_C_ARRAY_LENGTH(_event_handlers); i++)
     {
        _event_handlers[i] = ecore_event_handler_add(ctor[i].event,
                                                     ctor[i].callback, NULL);
        if (EINA_UNLIKELY(! _event_handlers[i]))
          {
             CRI("Failed to create handler for event 0x%x", ctor[i].event);
             goto fail;
          }
     }

   /* Create a hash table that allows to retrieve nvim instances from the event
    * handlers */
   _nvim_instances = eina_hash_pointer_new(_nvim_free_cb);
   if (EINA_UNLIKELY(! _nvim_instances))
     {
        CRI("Failed to create hash table to hold running nvim instances");
        goto fail;
     }

   return EINA_TRUE;

fail:
   for (i--; (int)i >= 0; i--)
     ecore_event_handler_del(_event_handlers[i]);
   return EINA_FALSE;
}

void
nvim_shutdown(void)
{
   unsigned int i;

   for (i = 0; i < EINA_C_ARRAY_LENGTH(_event_handlers); i++)
     {
        ecore_event_handler_del(_event_handlers[i]);
        _event_handlers[i] = NULL;
     }
   eina_hash_free(_nvim_instances);
   _nvim_instances = NULL;
}

s_nvim *
nvim_new(void)
{
   /* We will modify a global variable here */
   EINA_MAIN_LOOP_CHECK_RETURN_VAL(NULL);

   /* First, create the nvim data */
   s_nvim *const nvim = calloc(1, sizeof(s_nvim));
   if (! nvim)
     {
        CRI("Failed to create nvim structure");
        return NULL;
     }

   /* Initialze msgpack for RPC */
   msgpack_sbuffer_init(&nvim->sbuffer);
   msgpack_packer_init(&nvim->packer, &nvim->sbuffer, msgpack_sbuffer_write);
   msgpack_zone_init(&nvim->mempool, 2048);

   /* Create the GUI window */
   nvim->win = elm_win_util_standard_add("envim", "Envim");
   elm_win_autodel_set(nvim->win, EINA_TRUE);

   /* Last step: create the neovim process */
   nvim->exe = ecore_exe_pipe_run(
      "nvim --embed --headless",
      ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_WRITE | ECORE_EXE_PIPE_ERROR  |
      ECORE_EXE_TERM_WITH_PARENT,
      nvim
   );
   if (! nvim->exe)
     {
        CRI("Failed to execute nvim instance");
        goto del_mem;
     }

   /* Before leaving, we register the process in the running instances table */
   const Eina_Bool ok = eina_hash_direct_add(_nvim_instances, &nvim->exe, nvim);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to register nvim instance in the hash table");
        /* TODO ERROR */
     }

   return nvim;

del_mem:
   free(nvim);
   return NULL;
}

void
nvim_free(s_nvim *nvim)
{
   /* We will modify a global variable here */
   EINA_MAIN_LOOP_CHECK_RETURN;

   if (nvim)
     {
        /*
         * The actual freeing of the s_nvim is handled by the hash table free
         * callback.
         */
        eina_hash_del_by_key(_nvim_instances, &nvim->exe);
     }
}

Eina_Bool
nvim_rpc(s_nvim *nvim,
         e_request request_type, ...)
{
   /* Better be paranoid than dead */
   EINA_SAFETY_ON_NULL_RETURN_VAL(nvim, EINA_FALSE);
   EINA_SAFETY_ON_FALSE_RETURN_VAL((request_type > REQUEST_NONE) &&
                                   (request_type < __REQUEST_LAST),
                                   EINA_FALSE);

   const uint64_t req_id = _nvim_get_next_uid(nvim);
   msgpack_packer *const pk = &nvim->packer;
   s_request *const req = request_new(req_id, request_type);
   const s_request_info *const info = request_info_get(req);
   const size_t method_len = (size_t)eina_stringshare_strlen(info->method);
   Eina_Bool ok;

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
   msgpack_pack_uint64(pk, req_id);
   msgpack_pack_bin(pk, method_len);
   msgpack_pack_bin_body(pk, info->method, method_len);
   msgpack_pack_array(pk, info->in_args_count);

   /* Keep the request around */
   nvim->requests = eina_list_append(nvim->requests, req);

   /* Finally, send that to the slave neovim process */
   ok = ecore_exe_send(nvim->exe, nvim->sbuffer.data, (int)nvim->sbuffer.size);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to send %zu bytes to neovim", nvim->sbuffer.size);
        return EINA_FALSE;
     }

   return EINA_TRUE;
}
