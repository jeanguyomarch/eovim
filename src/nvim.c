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

#include "eovim/nvim_api.h"
#include "eovim/types.h"
#include "eovim/nvim.h"
#include "eovim/config.h"
#include "eovim/nvim_api.h"
#include "eovim/log.h"
#include "eovim/mode.h"

typedef void (*f_nvim_notif)(s_nvim *nvim, Eina_List *args);
enum
{
   HANDLER_ADD,
   HANDLER_DEL,
   HANDLER_DATA,
   HANDLER_ERROR,
   __HANDLERS_LAST /* Sentinel, don't use */
};

static Ecore_Event_Handler *_event_handlers[__HANDLERS_LAST];
static s_nvim *_nvim_instance = NULL;

/*============================================================================*
 *                                 Private API                                *
 *============================================================================*/

static inline s_nvim *
_nvim_get(void)
{
   /* We handle only one neovim instance */
   return _nvim_instance;
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
   Eina_List *const req_item = nvim_api_request_find(nvim, req_id);
   if (EINA_UNLIKELY(! req_item))
     {
        CRI("Uh... received a response to request %"PRIu32", but it was not "
            "registered. Something wrong happend somewhere!", req_id);
        goto fail;
     }
   DBG("Received response to request %"PRIu32, req_id);

   /* Now that we have found the request, we can remove it */
   nvim_api_request_free(nvim, req_item);

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
     }
   else if (err_type != MSGPACK_OBJECT_NIL)
     {
        ERR("Error argument is of handled type 0x%x", err_type);
        goto fail;
     }

   /* We won't handle the 4th argment, which contain the returned parameters */
   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static Eina_Stringshare *
_object_to_stringshare(const msgpack_object *obj)
{
   if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_STR))
     {
        ERR("Object does not contain a string. Type is 0x%x", obj->type);
        return NULL;
     }
   else
     {
        const msgpack_object_str *const str = &(obj->via.str);
        return eina_stringshare_add_length(str->ptr, str->size);
     }
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
   if (EINA_UNLIKELY(args->ptr[1].type != MSGPACK_OBJECT_STR))
     {
        ERR("Second argument in notification is expected to be a string");
        goto fail;
     }
   Eina_Stringshare *const method = _object_to_stringshare(&args->ptr[1]);
   if (EINA_UNLIKELY(! method))
     {
        CRI("Failed to create stringshare from Neovim method");
        goto fail;
     }
   DBG("Received notification '%s'", method);
   eina_stringshare_del(method); /* No need of it anymore... */

   /*
    * 3rd argument must be an array of objects
    */
   if (EINA_UNLIKELY(args->ptr[2].type != MSGPACK_OBJECT_ARRAY))
     {
        ERR("Third argument in notification is expected to be an array");
        goto fail;
     }
   const msgpack_object_array *const args_arr = &(args->ptr[2].via.array);
   /*
    * Go through the notification's commands. There are formatted of the form
    * [ command_name, Args... ]
    * So we expect arguments to be arrays of at least one element.
    * command_name must be a string!
    */
   for (unsigned int i = 0; i < args_arr->size; i++)
     {
        const msgpack_object *const arg = &(args_arr->ptr[i]);
        if (EINA_UNLIKELY(arg->type != MSGPACK_OBJECT_ARRAY))
          {
             CRI("Expected argument of type array. Got 0x%x.", arg->type);
             continue; /* Try next element */
          }
        const msgpack_object_array *const cmd = &(arg->via.array);
        if (EINA_UNLIKELY(cmd->size < 1))
          {
             CRI("Expected at least one argument. Got %"PRIu32, cmd->size);
             continue; /* Try next element */
          }
        const msgpack_object *const cmd_name_obj = &(cmd->ptr[0]);
        if (EINA_UNLIKELY(cmd_name_obj->type != MSGPACK_OBJECT_STR))
          {
             CRI("Expected command to be of type string. Got 0x%x", arg->type);
             continue; /* Try next element */
          }
        const msgpack_object_str *const cmd_obj = &(cmd_name_obj->via.str);
        Eina_Stringshare *command = eina_stringshare_add_length(
           cmd_obj->ptr, cmd_obj->size
        );
        if (EINA_UNLIKELY(! command))
          {
             CRI("Failed to create stringshare from command object");
             continue; /* Try next element */
          }
        nvim_api_event_dispatch(nvim, command, cmd);
     }

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
   s_nvim *const nvim = _nvim_get();
   const int pid = ecore_exe_pid_get(info->exe);

   /* Determine whether nvim died or not */
   Eina_Bool show_error = EINA_TRUE;
   if ((info->exited) && (info->exit_code == 0))
     show_error = EINA_FALSE;

   if (show_error)
     {
        // TODO gui error
        ERR("Process with PID %i died", pid);
     }
   else
     {
        INF("Process with PID %i terminated", pid);
        gui_del(&nvim->gui);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_nvim_received_data_cb(void *data EINA_UNUSED,
                       int   type EINA_UNUSED,
                       void *event)
{
   const Ecore_Exe_Event_Data *const info = event;
   s_nvim *const nvim = _nvim_get();
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

#if 0 /* Uncomment to roughly dump the received messages */
        msgpack_object_print(stderr, obj);
        fprintf(stderr, "\n--------\n");
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
   CRI("Received an error");
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
      const int event;
      const Ecore_Event_Handler_Cb callback;
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

   return EINA_TRUE;

fail:
   for (i--; (int)i >= 0; i--)
     ecore_event_handler_del(_event_handlers[i]);
   return EINA_FALSE;
}

void
nvim_shutdown(void)
{
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_event_handlers); i++)
     ecore_event_handler_del(_event_handlers[i]);
}

uint32_t
nvim_next_uid_get(s_nvim *nvim)
{
   return nvim->request_id++;
}

static void
_attach(void *data)
{
   s_nvim *const nvim = data;
   nvim_api_ui_attach(nvim, 80, 24);
}

s_nvim *
nvim_new(const char *program,
         unsigned int args_count,
         const char *const argv[])
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(program, NULL);

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

   /* Create the config */
   nvim->config = config_new();

   /* Initialze msgpack for RPC */
   msgpack_sbuffer_init(&nvim->sbuffer);
   msgpack_packer_init(&nvim->packer, &nvim->sbuffer, msgpack_sbuffer_write);
   msgpack_unpacker_init(&nvim->unpacker, 2048);

   /* Create the hash map that will contain the modes */
   nvim->modes = eina_hash_stringshared_new(EINA_FREE_CB(mode_free));
   if (EINA_UNLIKELY(! nvim->modes))
     {
        CRI("Failed to create Eina_Hash");
        goto del_mem;
     }

   /* Create the GUI window */
   if (EINA_UNLIKELY(! gui_add(&nvim->gui, nvim)))
     {
        CRI("Failed to set up the graphical user interface");
        goto del_hash;
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

   /* Before leaving, we register the nvim instance */
   _nvim_instance = nvim;

   ecore_job_add(_attach, nvim);
   return nvim;

del_win:
   gui_del(&nvim->gui);
del_hash:
   eina_hash_free(nvim->modes);
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
   if (nvim)
     {
        msgpack_sbuffer_destroy(&nvim->sbuffer);
        msgpack_unpacker_destroy(&nvim->unpacker);
        eina_hash_free(nvim->modes);
        if (nvim->mode.name) { eina_stringshare_del(nvim->mode.name); }
        free(nvim);
     }
}

void
nvim_mode_set(s_nvim *nvim,
              Eina_Stringshare *name,
              unsigned int index)
{
   if (nvim->mode.name) { eina_stringshare_del(nvim->mode.name); }
   nvim->mode.name = name;
   nvim->mode.index = index;
}

s_mode *
nvim_named_mode_get(const s_nvim *nvim,
                    Eina_Stringshare *name)
{
   return eina_hash_find(nvim->modes, name);
}

Eina_Bool
nvim_mode_add(s_nvim *nvim,
              s_mode *mode)
{
   return eina_hash_add(nvim->modes, mode->name, mode);
}

void
nvim_mouse_enabled_set(s_nvim *nvim,
                       Eina_Bool enable)
{
   nvim->mouse_enabled = !!enable;
}

Eina_Bool
nvim_mouse_enabled_get(const s_nvim *nvim)
{
   return nvim->mouse_enabled;
}
