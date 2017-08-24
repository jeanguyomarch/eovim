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
   msgpack_unpacker_destroy(&nvim->unpacker);
   eina_hash_free(nvim->buffers);
   eina_hash_free(nvim->windows);
   eina_hash_free(nvim->tabpages);
   free(nvim);
}

static void
_free_tabpage_cb(void *data)
{
   s_tabpage *const tab = data;
   (void) tab;
}

static void
_free_window_cb(void *data)
{
   s_window *const win = data;
   (void) win;
}

static void
_free_buffer_cb(void *data)
{
   s_buffer *const buf = data;
   (void) buf;
}

static Eina_List *
_nvim_request_find(const s_nvim *nvim,
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

static inline s_nvim *
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
   const uint32_t req_id = (uint32_t)args->ptr[1].via.u64;
   Eina_List *const req_item = _nvim_request_find(nvim, req_id);
   if (EINA_UNLIKELY(! req_item))
     {
        CRI("Uh... received a response to request %"PRIu32", but is was not "
            "registered. Something wrong happend somewhere!", req_id);
        goto fail;
     }
   DBG("Received response to request %"PRIu32, req_id);

   /* Found the request, we can now get the data contained within the list */
   const s_request *const req = eina_list_data_get(req_item);

   /* Now that we have found the request, we can remove it from the pending
    * list */
   nvim->requests = eina_list_remove_list(nvim->requests, req_item);

   /* If 3rd arg is an array, this is an error message. */
   const msgpack_object_type err_type = args->ptr[2].type;
   if (err_type == MSGPACK_OBJECT_ARRAY)
     {
        const msgpack_object_array *const err_args = &(args->ptr[2].via.array);
        if (EINA_UNLIKELY(err_args->size != 2))
          {
             ERR("Error response is supposed to have two arguments");
             goto fail;
          }
        if (EINA_UNLIKELY(err_args->ptr[1].type != MSGPACK_OBJECT_STR))
          {
             ERR("Error response is supposed to contain a string");
             goto fail;
          }
        const msgpack_object_str *const e = &(err_args->ptr[1].via.str);
        Eina_Stringshare *const err = eina_stringshare_add_length(e->ptr,
                                                                  e->size);
        if (EINA_UNLIKELY(! err))
          {
             CRI("Failed to create stringshare");
             goto fail;
          }

        CRI("Neovim reported an error: %s", err);
        eina_stringshare_del(err);

        if (req->error_callback)
          req->error_callback(nvim, req, req->callback_data);
     }
   else if (err_type != MSGPACK_OBJECT_NIL)
     {
        ERR("Error argument is of handled type 0x%x", err_type);
        goto fail;
     }

   /*
    * 4th argument contains the returned parameters It may be an ARRAY, if
    * there are several parameters, or just a signle object if there is only
    * ONE returned parameter.
    * In the latter case, we encapsulate it within an array to have a unified
    * interface.
    */
   const msgpack_object_array *out_args = &(args->ptr[3].via.array);
   msgpack_object_array formatted_arg;
   const msgpack_object_type answer_type = args->ptr[3].type;
   if (answer_type != MSGPACK_OBJECT_ARRAY)
     {
        formatted_arg.size = 1;
        formatted_arg.ptr = &(args->ptr[3]);
        out_args = &formatted_arg;
     }

   /* And finally call the handler associated to the request type */
   return nvim_api_response_dispatch(nvim, req, out_args);
fail:
   return EINA_FALSE;
}

static Eina_Bool
_handle_notification(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   /*
    * 2nd argument must be a string.
    * It contains the METHOD to be called for the notification.
    * We decore the string as a stringshare, to feed it to our table of
    * methods.
    */
   if (args->ptr[1].type != MSGPACK_OBJECT_STR)
     {
        ERR("Second argument in notification is expected to be a string");
        goto fail;
     }
   const msgpack_object_str *const method_obj = &(args->ptr[1].via.str);
   Eina_Stringshare *const method = eina_stringshare_add_length(
      method_obj->ptr, method_obj->size
   );
   if (EINA_UNLIKELY(! method))
     {
        CRI("Failed to create stringshare from Neovim method");
        goto fail;
     }
   DBG("Received notification '%s'", method);

   /*
    * 3rd argument must be an array of objects
    */

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static void
_load_lines(s_nvim *nvim, Eina_List *lines, void *udata)
{
   Eina_List *l;
   Eina_Stringshare *line;

   EINA_LIST_FOREACH(lines, l, line)
     {
        printf("=> %s\n", line);
     }
}

static void
_reload_buf(s_nvim *nvim, t_int buf, void *data)
{
   nvim_buf_get_lines(nvim, buf, 0, 5, EINA_FALSE, _load_lines, NULL, NULL);
}

static void
_reload_wins(s_nvim *nvim, Eina_List *windows, void *data)
{
   s_tabpage *const parent_tab = data;
   Eina_List *l;
   const t_int *win_id_fake_ptr;

   EINA_LIST_FOREACH(windows, l, win_id_fake_ptr)
     {
        const t_int win_id = (t_int)win_id_fake_ptr;
        s_window *win = eina_hash_find(nvim->windows, &win_id);
        if (! win)
          {
             win = window_new(win_id, parent_tab, &nvim->gui);
             if (EINA_UNLIKELY(! win))
               {
                  CRI("Failed to create new window");
                  continue;
               }
             eina_hash_add(nvim->windows, &win_id, win);
          }
        nvim_win_get_buf(nvim, win_id, _reload_buf, NULL, win);
     }
}

static void
_reload(s_nvim *nvim, Eina_List *tabpages,
        void *data EINA_UNUSED)
{
   Eina_List *l;
   const t_int *tab_id_fake_ptr;

   EINA_LIST_FOREACH(tabpages, l, tab_id_fake_ptr)
     {
        const t_int tab_id = (t_int)tab_id_fake_ptr;
        s_tabpage *tab = eina_hash_find(nvim->tabpages, &tab_id);
        if (! tab)
          {
             tab = tabpage_new(tab_id, &nvim->gui);
             if (! tab)
               {
                  CRI("Failed to create tabpage");
                  continue;
               }
             eina_hash_add(nvim->tabpages, &tab_id, tab);
          }
        nvim_tabpage_list_wins(nvim, tab_id, _reload_wins, NULL, tab);
     }
}



static void
_init(void *data)
{
   s_nvim *const nvim = data;

   nvim_ui_attach(nvim, 80, 24, NULL, NULL, NULL, NULL);
 //  nvim_list_tabpages(nvim, _reload, NULL, NULL);
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
   msgpack_unpacker *const unpacker = &nvim->unpacker;
   const size_t recv_size = (size_t)info->size;

   DBG("Incoming data from PID %u (size %zu)", ecore_exe_pid_get(info->exe), recv_size);


   /*
    * We have received something from NeoVim. We now must deserialize this.
    */
   msgpack_unpacked result;

   /* Resize the unpacking buffer if need be */
   if (msgpack_unpacker_buffer_capacity(unpacker) < recv_size)
     {
        const bool ret = msgpack_unpacker_reserve_buffer(unpacker, recv_size);
        if (! ret)
          {
             ERR("Memory reallocation of %zu bytes failed", recv_size);
             goto end;
          }
     }
   /* This seems to be required, but that's plain inefficiency */
   memcpy(msgpack_unpacker_buffer(unpacker), info->data, recv_size);
   msgpack_unpacker_buffer_consumed(unpacker, recv_size);

   msgpack_unpacked_init(&result);
   for (;;)
     {
        const msgpack_unpack_return ret = msgpack_unpacker_next(unpacker, &result);
        if (ret == MSGPACK_UNPACK_CONTINUE) { break; }
        else if (ret != MSGPACK_UNPACK_SUCCESS)
          {
             ERR("Error while unpacking data from neovim (0x%x)", ret);
             goto end_unpack;
          }
        msgpack_object obj = result.data;

        /* Uncomment to roughly dump the received messages */
#if 1
        msgpack_object_print(stderr, obj);
        fprintf(stderr, "\n");
#endif

        if (obj.type != MSGPACK_OBJECT_ARRAY)
          {
             ERR("Unexpected msgpack type 0x%x", obj.type);
             goto end_unpack;
          }

        const msgpack_object_array *const args = &obj.via.array;
        const unsigned int response_args_count = 4;
        const unsigned int notif_args_count = 3;
        if ((args->size != response_args_count) &&
            (args->size != notif_args_count))
          {
             ERR("Unexpected count of arguments: %u.", args->size);
             goto end_unpack;
          }

        if (args->ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER)
          {
             ERR("First argument in response is expected to be an integer");
             goto end_unpack;
          }
        switch (args->ptr[0].via.u64)
          {
           case 1:
              _handle_request_response(nvim, args);
              break;

           case 2:
              _handle_notification(nvim, args);
              break;

           default:
              ERR("Invalid message identifier %"PRIu64, args->ptr[0].via.u64);
              goto end_unpack;
          }
     } /* End of message unpacking */

end_unpack:
   msgpack_unpacked_destroy(&result);
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

uint32_t
nvim_get_next_uid(s_nvim *nvim)
{
   return nvim->request_id++;
}

s_nvim *
nvim_new(const char *program,
         unsigned int args_count,
         const char *const argv[])
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(program, NULL);

   /* We will modify a global variable here */
   EINA_MAIN_LOOP_CHECK_RETURN_VAL(NULL);

   Eina_Bool ok;

   /* Forge the command-line for the nvim program. We manually enforce
    * --embed and --headless, because we are the gui client, and forward all
    * the options to the command-line. We are using an Eina_Strbuf to make no
    * assumption on the size of the command-line.
    */
   Eina_Strbuf *const cmdline = eina_strbuf_new();
   if (EINA_UNLIKELY(! cmdline))
     {
        CRI("Failed to create strbuf");
        goto fail;
     }
   ok = eina_strbuf_append_printf(cmdline, "\"%s\" --embed --headless --", program);
   for (unsigned int i = 0; i < args_count; i++)
     {
        ok &= eina_strbuf_append_printf(cmdline, " \"%s\"", argv[i]);
     }
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to correctly format the command line");
        goto del_strbuf;
     }

   /* First, create the nvim data */
   s_nvim *const nvim = calloc(1, sizeof(s_nvim));
   if (! nvim)
     {
        CRI("Failed to create nvim structure");
        goto del_strbuf;
     }

   nvim->tabpages = eina_hash_int64_new(_free_tabpage_cb);
   nvim->windows = eina_hash_int64_new(_free_window_cb);
   nvim->buffers = eina_hash_int64_new(_free_buffer_cb);
   /* TODO errors checking */

   /* Initialze msgpack for RPC */
   msgpack_sbuffer_init(&nvim->sbuffer);
   msgpack_packer_init(&nvim->packer, &nvim->sbuffer, msgpack_sbuffer_write);
   msgpack_unpacker_init(&nvim->unpacker, 2048);

   /* Create the GUI window */
   if (EINA_UNLIKELY(! gui_add(&nvim->gui)))
     {
        CRI("Failed to set up the graphical user interface");
        goto del_mem;
     }

   /* Last step: create the neovim process */
   nvim->exe = ecore_exe_pipe_run(
      eina_strbuf_string_get(cmdline),
      ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_WRITE | ECORE_EXE_PIPE_ERROR  |
      ECORE_EXE_TERM_WITH_PARENT,
      nvim
   );
   if (! nvim->exe)
     {
        CRI("Failed to execute nvim instance");
        goto del_win;
     }
   DBG("Running %s", eina_strbuf_string_get(cmdline));
   eina_strbuf_free(cmdline);

   /* Before leaving, we register the process in the running instances table */
   ok = eina_hash_direct_add(_nvim_instances, &nvim->exe, nvim);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to register nvim instance in the hash table");
        goto del_win;
     }

   ecore_job_add(_init, nvim);
   return nvim;

del_win:
   gui_del(&nvim->gui);
del_mem:
   free(nvim);
del_strbuf:
   eina_strbuf_free(cmdline);
fail:
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

s_tabpage *
nvim_tabpage_add(s_nvim *nvim, t_int id)
{
   s_tabpage *const tab = tabpage_new(id, &nvim->gui);
   if (EINA_UNLIKELY(! tab))
     {
        CRI("Failed to create tabpage");
        goto fail;
     }
   if (EINA_UNLIKELY(! eina_hash_add(nvim->tabpages, &id, tab)))
     {
        CRI("Failed to add tab into hash table");
        goto fail;
     }
   return tab;

fail:
   if (tab) tabpage_free(tab);
   return NULL;
}

void
nvim_win_size_set(s_nvim *nvim,
                  s_window *win,
                  unsigned int width,
                  unsigned int height)
{
   nvim_win_set_width(nvim, win->id, width, NULL, NULL, NULL);
   nvim_win_set_height(nvim, win->id, height, NULL, NULL, NULL);
}
