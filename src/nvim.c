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
static Eina_Hash *_nvim_instances = NULL;
static Eina_Hash *_callbacks = NULL;

/*============================================================================*
 *                                Commands API                                *
 *============================================================================*/

#define CHECK_ARGS_COUNT(Args, Count) \
   if (EINA_UNLIKELY(eina_list_count(Args) != (Count))) { \
      CRI("Invalid arguments count (%u instead of %u)", \
          eina_list_count(Args), (Count)); return; }

#define CHECK_ARG_TYPE(Arg, Type) \
   if (EINA_UNLIKELY(eina_value_type_get(Arg) != Type)) { \
      CRI("Invalid type (expected %s, got %s)", \
          eina_value_type_name_get(Type), \
          eina_value_type_name_get(eina_value_type_get(Arg))); return; }

static void
_nvim_update_fg(s_nvim *nvim, Eina_List *args)
{
   CHECK_ARGS_COUNT(args, 1);
   Eina_Value *const val = eina_list_data_get(args);
   CHECK_ARG_TYPE(val, EINA_VALUE_TYPE_INT64);

   int64_t value;
   eina_value_get(val, &value);
   WRN("Hey, this is update_fg: %"PRIi64, value);

   eina_value_free(val);
}

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
   free(nvim);
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


static void
_process_notif_arguments(s_nvim *nvim,
                         Eina_List *args)
{
   /*
    * First step, iterate over all the commands. They are of the form
    *           ["command": [args]]
    * within the list. That's this latter list we are unrolling...
    *
    * We also CONSUME the arguments: they are deallocated as we iterate
    * over them.
    */
   Eina_Value *top_arg;
   EINA_LIST_FREE(args, top_arg)
     {
        /*
         * Within the list of commands, we have a pair command, arguments,
         * within a list. We are not supposed to have anything else than
         * a list at tihs point of unrolling.
         */
        const Eina_Value_Type *const type = eina_value_type_get(top_arg);
        if (EINA_LIKELY(type == EINA_VALUE_TYPE_LIST))
          {
             /* Get the list of arguments (command + its arguments) */
             //Eina_Value_List sub_args;
             //eina_value_get(top_arg, &sub_args);
             Eina_Value *cmd_val, *arg_val;
             eina_value_list_get(top_arg, 0, &cmd_val);
             eina_value_list_get(top_arg, 1, &arg_val);

             /* The command is always the first element
              * It is stored as a stringshare.
              */
             //Eina_Value *const cmd_val = eina_list_data_get(sub_args.list);
             if (EINA_UNLIKELY(eina_value_type_get(cmd_val) != EINA_VALUE_TYPE_STRINGSHARE))
               {
                  ERR("First element is not a stringshare!? This is wrong!");
                  continue;
               }
             Eina_Stringshare *cmd;
             eina_value_get(cmd_val, &cmd);
             DBG("Command: %s", cmd);

                    /* Now that we have a command, call the callback associated to it */
             const f_nvim_notif func = eina_hash_find(_callbacks, cmd);
             if (EINA_UNLIKELY(! func))
               {
                  //       CRI("Unregistered callback for command %s", cmd);
                  continue;
               }

             /* The arguments are the second element (a list) */
             if (EINA_UNLIKELY(eina_value_type_get(arg_val) != EINA_VALUE_TYPE_LIST))
               {
                  ERR("Second element is not a list!? This is wrong!");
                  continue;
               }
             Eina_Value_List list_val;
             eina_value_get(arg_val, &list_val);

             /* Now, call the function with its arguments */
             func(nvim, list_val.list);

             /* Dealloc the values we got */
             eina_value_free(cmd_val);
             eina_value_free(arg_val);
          }
        else
          {
             ERR("Mh.... I'm pretty sure this should be a list");
          }
        eina_value_free(top_arg);
        //else if (type == EINA_VALUE_TYPE_HASH)
        //  {
        //     Eina_Value_Hash sub_args;
        //     eina_value_get(arg, &sub_args);
        //  }
        //else
        //  {
        //     INF("%s %s: %s",
        //         prefix,
        //         eina_value_type_name_get(type),
        //         eina_value_to_string(arg));
        //  }
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
   Eina_Stringshare *const method = pack_single_stringshare_get(&args->ptr[1]);
   if (EINA_UNLIKELY(! method))
     {
        CRI("Failed to create stringshare from Neovim method");
        goto fail;
     }
   DBG("Received notification '%s'", method);

   /*
    * 3rd argument must be an array of objects
    */
   if (EINA_UNLIKELY(args->ptr[2].type != MSGPACK_OBJECT_ARRAY))
     {
        ERR("Third argument in notification is expected to be an array");
        goto fail;
     }
   Eina_List *const objs = pack_array_get(&args->ptr[2].via.array);
   _process_notif_arguments(nvim, objs);

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
   struct cb {
      const char *const name;
      const unsigned int size;
      const f_nvim_notif func;
   } const callbacks[] = {
#define CB(Name, Func) { .name = Name, .size = sizeof(Name) - 1, .func = Func }
      CB("update_fg", _nvim_update_fg),
#undef CB
   };
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

   /* Create a hash table that allows to retrieve nvim instances from the event
    * handlers */
   _nvim_instances = eina_hash_pointer_new(_nvim_free_cb);
   if (EINA_UNLIKELY(! _nvim_instances))
     {
        CRI("Failed to create hash table to hold running nvim instances");
        goto fail;
     }

   /* Create a hash table that will hold the callbacks for nvim commands */
   _callbacks = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(! _callbacks))
     {
        CRI("Failed to create hash table to hold callbacks");
        goto hash_fail;
     }

   for (unsigned int j = 0; j < EINA_C_ARRAY_LENGTH(callbacks); j++)
     {
        const struct cb *const cb = &(callbacks[j]);
        Eina_Stringshare *const key = eina_stringshare_add_length(
           cb->name, cb->size
        );
        eina_hash_add(_callbacks, key, cb->func);
     }

   return EINA_TRUE;

hash_fail:
   eina_hash_free(_nvim_instances);
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
   eina_hash_free(_nvim_instances);
   eina_hash_free(_callbacks);
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

   nvim_ui_attach(nvim, 80, 24, NULL, NULL, NULL, NULL);
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
