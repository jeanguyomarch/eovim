/*
 * Copyright (c) 2017-2018 Jean Guyomarc'h
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
#include "eovim/nvim_event.h"
#include "eovim/nvim_helper.h"
#include "eovim/log.h"
#include "eovim/mode.h"
#include "eovim/main.h"

enum
{
   HANDLER_ADD,
   HANDLER_DEL,
   HANDLER_DATA,
   HANDLER_ERROR,
   __HANDLERS_LAST /* Sentinel */
};

struct tab
{
   EINA_INLIST;
   Eina_Stringshare *name;
   unsigned int id;
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

   /* When this variable is se to EINA_TRUE, we won't process the response.
    * This is here so we can go through the error message without crashing too
    * early. */
   Eina_Bool do_not_process = EINA_FALSE;

   /* Get the request from the pending requests list. */
   const uint32_t req_id = (uint32_t)args->ptr[1].via.u64;
   Eina_List *const req_item = nvim_api_request_find(nvim, req_id);
   if (EINA_UNLIKELY(! req_item))
     {
        /* We probably should wait for the error message to be fetched back.
         * So we start by throwing an error, then we tell that the response
         * shall not be processed. */
        CRI("Uh... received a response to request %"PRIu32", but it was not "
            "registered. Something wrong happend somewhere!", req_id);
        do_not_process = EINA_TRUE;
     }
   else
     DBG("Received response to request %"PRIu32, req_id);

   /* If 3rd arg is an array, this is an error message. */
   const msgpack_object_type err_type = args->ptr[2].type;
   if (err_type == MSGPACK_OBJECT_ARRAY)
     {
        const msgpack_object_array *const err_args = &(args->ptr[2].via.array);
        if (EINA_UNLIKELY(err_args->size != 2))
          {
             ERR("Error response is supposed to have two arguments");
             goto fail_req;
          }
        if (EINA_UNLIKELY(err_args->ptr[1].type != MSGPACK_OBJECT_STR))
          {
             ERR("Error response is supposed to contain a string");
             goto fail_req;
          }
        const msgpack_object_str *const e = &(err_args->ptr[1].via.str);
        Eina_Stringshare *const err = eina_stringshare_add_length(e->ptr,
                                                                  e->size);
        if (EINA_UNLIKELY(! err))
          {
             CRI("Failed to create stringshare");
             goto fail_req;
          }

        CRI("Neovim reported an error: %s", err);
        eina_stringshare_del(err);
        goto fail_req;
     }
   else if (err_type != MSGPACK_OBJECT_NIL)
     {
        ERR("Error argument is of handled type 0x%x", err_type);
        goto fail_req;
     }

   /* At this point, we have had the chance to read the error message. If we
    * did, we would have exited earlier. If not, exit right now. */
   if (EINA_UNLIKELY(do_not_process == EINA_TRUE))
     goto fail_req;

   /* 4th argment, which contain the returned parameters */
   const msgpack_object *const result = &(args->ptr[3]);
   nvim_api_request_call(nvim, req_item, result);

   /* Now that we have found the request, we can remove it */
   nvim_api_request_free(nvim, req_item);
   return EINA_TRUE;

fail_req:
   nvim_api_request_free(nvim, req_item);
fail:
   return EINA_FALSE;
}

static Eina_Stringshare *
_stringshare_extract(const msgpack_object *obj)
{
   if (obj->type == MSGPACK_OBJECT_STR)
     {
        const msgpack_object_str *const str = &(obj->via.str);
        return eina_stringshare_add_length(str->ptr, str->size);
     }
   else if (obj->type == MSGPACK_OBJECT_BIN)
     {
        const msgpack_object_bin *const bin = &(obj->via.bin);
        return eina_stringshare_add_length(bin->ptr, bin->size);
     }
   else
     {
        ERR("Second argument in notification is expected to be a string "
            "(or BIN string), but it is of type 0x%x", obj->type);
        return NULL;
     }
}

static Eina_Bool
_handle_notification(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   /*
    * 2nd argument must be a string (or bin string).
    * It contains the METHOD to be called for the notification.
    * We decore the string as a stringshare, to feed it to our table of
    * methods.
    */
   Eina_Stringshare *const method = _stringshare_extract(&(args->ptr[1]));
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
        goto del_method;
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
             CRI("Expected at least one argument. Got zero.");
             continue; /* Try next element */
          }
        Eina_Stringshare *const command = _stringshare_extract(&(cmd->ptr[0]));
        if (EINA_UNLIKELY(! command))
          {
             CRI("Failed to create stringshare from command object");
             continue; /* Try next element */
          }
        nvim_event_dispatch(nvim, method, command, cmd);
        eina_stringshare_del(command);
     }

   eina_stringshare_del(method);
   return EINA_TRUE;

del_method:
   eina_stringshare_del(method);
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

   /* We consider that neovim crashed if it receives an uncaught signal */
   if (info->signalled)
     {
        ERR("Process with PID %i died of uncaught signal %i",
            pid, info->exit_signal);
        gui_die(
           &nvim->gui,
           "The Neovim process %i died. Eovim cannot continue its execution",
           pid
        );
     }
   else
     {
        INF("Process with PID %i terminated with exit code %i",
            pid, info->exit_code);
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

        if (EINA_UNLIKELY(obj.type != MSGPACK_OBJECT_ARRAY))
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

        if (EINA_UNLIKELY(args->ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER))
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
   const char *const msg = info->data;
   ERR("Error: %s", msg);
   return ECORE_CALLBACK_PASS_ON;
}

static void
_nvim_runtime_load(s_nvim *nvim)
{
   Eina_Strbuf *const buf = eina_strbuf_new();
   if (EINA_UNLIKELY(! buf))
     {
        CRI("Failed to create string buffer");
        return;
     }

   /* Compose the path to the runtime file */
   const char *const dir = (main_in_tree_is())
      ? SOURCE_DATA_DIR
      : elm_app_data_dir_get();
   eina_strbuf_append_printf(buf, "%s/vim/runtime.vim", dir);

   /* Load the runtime file in memory */
   const char *const filename = eina_strbuf_string_get(buf);
   Eina_File *const file = eina_file_open(filename, EINA_FALSE);
   if (EINA_UNLIKELY(! file))
     {
        CRI("Failed to create file from '%s'", filename);
        goto end;
     }
   char *const map = eina_file_map_all(file, EINA_FILE_POPULATE);
   if (EINA_UNLIKELY(! map))
     {
        CRI("Failed to map file '%s'", filename);
        goto close_file;
     }

   /* Send it to neovim */
   nvim_api_command(nvim, map, (unsigned int)strlen(map));

   /* Release everything */
   eina_file_map_free(file, map);
close_file:
   eina_file_close(file);
end:
   eina_strbuf_free(buf);
}

static void
_virtual_interface_init(s_nvim *nvim)
{
   nvim->hl_group_decode = nvim_helper_highlight_group_decode_noop;
}

static void
_virtual_interface_setup(s_nvim *nvim)
{
   /* Setting up the highliht group decoder virtual interface */
   if (NVIM_VERSION_EQ(nvim, 0, 2, 0))
     {
        ERR("You are running Neovim 0.2.0, which has a bug that prevents the "
            "cursor color to be deduced.");
     }
   else
     nvim->hl_group_decode = nvim_helper_highlight_group_decode;
}

static void
_version_decode_cb(s_nvim *nvim,
                   const s_version *version)
{
   char vstr[64];

   memcpy(&nvim->version, version, sizeof(s_version));
   snprintf(vstr, sizeof(vstr), "%u.%u.%u%c%s",
            version->major, version->minor, version->patch,
            version->extra[0] == '\0' ? '\0' : '-',
            version->extra);

   INF("Running Neovim version %s", vstr);
   if ((NVIM_VERSION_MAJOR(nvim) == 0) && (NVIM_VERSION_MINOR(nvim) < 2))
     {
        gui_die(&nvim->gui,
                "You are running neovim %s, which is unsupported. "
                "Please consider upgrading Neovim.", vstr);
     }
   /* We are now sure that we are running at least 0.2.0. */

   /* From neovim 0.2.1, the command-line can be externalized */
   if (NVIM_VERSION_PATCH(nvim) >= 1)
     {
        /* Cmdline and wildmenu are going by pair */
        nvim_api_ui_ext_cmdline_set(nvim, nvim->config->ext_cmdline);
        nvim_api_ui_ext_wildmenu_set(nvim, nvim->config->ext_cmdline);
     }

   /* Now that we know Neovim's version, setup the virtual interface, that will
    * prevent compatibilty issues */
   _virtual_interface_setup(nvim);
}

static void
_nvim_plugins_load(s_nvim *nvim)
{
   const Eina_List *const cfg_plugins = nvim->config->plugins;
   Eina_Inlist *const plugins = main_plugins_get();
   const unsigned int expect = eina_list_count(cfg_plugins);
   const unsigned int loaded = plugin_list_load(plugins, cfg_plugins);

   /* If we didn't load as many plugins as expected, warn */
   if (EINA_UNLIKELY(expect != loaded))
     WRN("The configuration expected %u plugins to be loaded, but "
         "only %u were", expect, loaded);
   else
     INF("Loaded %u plugins out of %u", loaded, expect);
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

s_nvim *
nvim_new(const s_options *opts,
         const char *const args[])
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(opts, NULL);

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
   ok = eina_strbuf_append_printf(cmdline, "\"%s\"", opts->nvim_prog);
   ok &= eina_strbuf_append(cmdline, " --embed --headless");
   if (opts->no_plugins) ok &= eina_strbuf_append(cmdline, " --noplugin");

   for (const char *arg = *args; arg != NULL; arg = *(++args))
     {
        ok &= eina_strbuf_append_printf(cmdline, " \"%s\"", arg);
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
   nvim->opts = opts;

   /* We will enable mouse handling by default. We do not receive the
    * information from neovim unless we change mode. This is annoying. */
   nvim->mouse_enabled = EINA_TRUE;

   nvim->decode = eina_ustrbuf_new();
   if (EINA_UNLIKELY(! nvim->decode))
     {
        CRI("Failed to create unicode string buffer");
        goto del_mem;
     }

   /* Create the config */
   nvim->config = config_load(opts->config_path);
   if (EINA_UNLIKELY(! nvim->config))
     {
        CRI("Failed to initialize a configuration");
        goto del_ustrbuf;
     }

   /* Load the plugins requested by the config */
   _nvim_plugins_load(nvim);

   /* Initialze msgpack for RPC */
   msgpack_sbuffer_init(&nvim->sbuffer);
   msgpack_packer_init(&nvim->packer, &nvim->sbuffer, msgpack_sbuffer_write);
   msgpack_unpacker_init(&nvim->unpacker, 2048);

   /* Create the hash map that will contain the modes */
   nvim->modes = eina_hash_stringshared_new(EINA_FREE_CB(mode_free));
   if (EINA_UNLIKELY(! nvim->modes))
     {
        CRI("Failed to create Eina_Hash");
        goto del_config;
     }

   /* Initialize the virtual interface to safe values (non-NULL pointers) */
   _virtual_interface_init(nvim);

   /* Create the neovim process */
   nvim->exe = ecore_exe_pipe_run(
      eina_strbuf_string_get(cmdline),
      ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_WRITE | ECORE_EXE_PIPE_ERROR  |
      ECORE_EXE_TERM_WITH_PARENT,
      nvim
   );
   if (EINA_UNLIKELY(! nvim->exe))
     {
        CRI("Failed to execute nvim instance");
        goto del_hash;
     }
   _nvim_instance = nvim;
   DBG("Running %s", eina_strbuf_string_get(cmdline));
   eina_strbuf_free(cmdline);
   nvim_api_ui_attach(nvim, opts->geometry.w, opts->geometry.h);
   nvim_helper_version_decode(nvim, _version_decode_cb);
   nvim_api_var_integer_set(nvim, "eovim_running", 1);
   _nvim_runtime_load(nvim);

   /* Create the GUI window */
   if (EINA_UNLIKELY(! gui_add(&nvim->gui, nvim)))
     {
        CRI("Failed to set up the graphical user interface");
        goto del_process;
     }
   gui_fullscreen_set(&nvim->gui, opts->fullscreen);

   return nvim;

del_process:
   ecore_exe_kill(nvim->exe);
del_hash:
   eina_hash_free(nvim->modes);
del_config:
   config_free(nvim->config);
del_ustrbuf:
   eina_ustrbuf_free(nvim->decode);
del_mem:
   free(nvim);
del_strbuf:
   eina_strbuf_free(cmdline);
fail:
   _nvim_instance = NULL;
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
        config_free(nvim->config);
        eina_ustrbuf_free(nvim->decode);
        free(nvim);
        _nvim_instance = NULL;
     }
}

const s_mode *
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
nvim_tab_add(s_nvim *nvim,
             Eina_Stringshare *name,
             unsigned int id)
{
   s_tab *tab;

   /* Check if the tab already exists. If it does, no change to do. */
   EINA_INLIST_FOREACH(nvim->tabs, tab)
     {
        if ((tab->name == name) && (tab->id == id))
          return EINA_FALSE;
     }

   tab = malloc(sizeof(s_tab));
   if (EINA_UNLIKELY(! tab))
     {
        CRI("Failed to allocate memory for tab object");
        return EINA_FALSE;
     }

   tab->name = name;
   tab->id = id;
   nvim->tabs = eina_inlist_append(nvim->tabs, EINA_INLIST_GET(tab));

   return EINA_TRUE;
}

Eina_Bool
nvim_mouse_enabled_get(const s_nvim *nvim)
{
   return nvim->mouse_enabled;
}
