/*
 * Copyright (c) 2017-2019 Jean Guyomarc'h
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
#include "eovim/nvim_request.h"
#include "eovim/nvim_helper.h"
#include "eovim/msgpack_helper.h"
#include "eovim/log.h"
#include "eovim/main.h"

static void _api_decode_cb(s_nvim *nvim, void *data, const msgpack_object *result);

/*============================================================================*
 *                                 Private API                                *
 *============================================================================*/

static void _ui_attached_cb(s_nvim *nvim, void *data EINA_UNUSED,
                            const msgpack_object *result EINA_UNUSED)
{
  gui_ready_set(&nvim->gui);
  nvim_helper_autocmd_do(nvim, "EovimReady");
}

static Eina_Bool
_handle_request(s_nvim *nvim, const msgpack_object_array *args)
{
   /* Retrieve the request identifier ****************************************/
   if (EINA_UNLIKELY(args->ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER))
     {
        ERR("Second argument in request is expected to be an integer");
        return EINA_FALSE;
     }
   const uint64_t long_req_id = args->ptr[1].via.u64;
   if (EINA_UNLIKELY(long_req_id > UINT32_MAX))
     {
        ERR("Request ID '%" PRIu64 " is too big", long_req_id);
        return EINA_FALSE;
     }
   const uint32_t req_id = (uint32_t)long_req_id;

   /* Retrieve the request arguments *****************************************/
   if (EINA_UNLIKELY(args->ptr[3].type != MSGPACK_OBJECT_ARRAY))
     {
        ERR("Fourth argument in request is expected to be an array");
        return EINA_FALSE;
     }
   const msgpack_object_array *const req_args = &(args->ptr[3].via.array);

   /* Retrieve the request name **********************************************/
   if (EINA_UNLIKELY(args->ptr[2].type != MSGPACK_OBJECT_STR))
     {
        ERR("Third argument in request is expected to be a string");
        return EINA_FALSE;
     }
   const msgpack_object_str *const str = &(args->ptr[2].via.str);
   Eina_Stringshare *const request =
     eina_stringshare_add_length(str->ptr, str->size);
   if (EINA_UNLIKELY(! request))
     {
        ERR("Failed to create stringshare");
        return EINA_FALSE;
     }

   const Eina_Bool ok = nvim_request_process(nvim, request, req_args, req_id);
   eina_stringshare_del(request);
   return ok;
}

static Eina_Bool
_handle_request_response(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   /* 2nd arg should be an integer */
   if (EINA_UNLIKELY(args->ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER))
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
   s_request *const req = nvim_api_request_find(nvim, req_id);
   if (EINA_UNLIKELY(! req))
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
   nvim_api_request_call(nvim, req, result);

   /* Now that we have found the request, we can remove it */
   nvim_api_request_free(nvim, req);
   return EINA_TRUE;

fail_req:
   nvim_api_request_free(nvim, req);
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
        const Eina_Bool ok = nvim_event_dispatch(nvim, method, command, cmd);
        if (EINA_UNLIKELY((! ok) &&
             (eina_log_domain_level_get("eovim") >= EINA_LOG_LEVEL_WARN)))
        {
          WRN("Command '%s' failed with input object:", command);
          fprintf(stderr, " -=> ");
          msgpack_object_print(stderr, *arg);
          fprintf(stderr, "\n");
        }
        eina_stringshare_del(command);
     }

   eina_stringshare_del(method);
   return EINA_TRUE;

del_method:
   eina_stringshare_del(method);
fail:
   return EINA_FALSE;
}

static Eina_Bool
_vimenter_request_cb(s_nvim *nvim EINA_UNUSED,
                     const msgpack_object_array *args EINA_UNUSED,
                     msgpack_packer *pk)
{
  msgpack_pack_nil(pk); /* Error */
  msgpack_pack_nil(pk); /* Result */
  return EINA_TRUE;
}

/*============================================================================*
 *                       Nvim Processes Events Handlers                       *
 *============================================================================*/

static Eina_Bool
_nvim_added_cb(void *data,
               int   type EINA_UNUSED,
               void *event)
{
#ifndef EFL_VERSION_1_22
   /* EFL versions 1.21 (and maybe 1.20 as well ??) have a bug. When coming out
    * of sleep/hibernation, a spurious event was sent, causing
    * ECORE_EXE_EVENT_ADD to be triggered with a NULL event, which is supposed
    * to be always set. This test prevent this spurious event to crash eovim */
   if (EINA_UNLIKELY(! event)) { return ECORE_CALLBACK_PASS_ON; }
#endif

   const Ecore_Exe_Event_Add *const info = event;
   INF("Process with PID %i was created", ecore_exe_pid_get(info->exe));

   /* Okay, at this point the neovim process is running! Great! Now, we can
    * start to retrieve the API information and trigger the vimenter autocmd.
    *
    * We can start attaching the UI on the fly.
    * See :help ui-startup for details.
    */
   s_nvim *const nvim = data;
   nvim_api_get_api_info(nvim, _api_decode_cb, NULL);

   nvim_helper_autocmd_vimenter_exec(nvim);
   const s_geometry *const geo = &nvim->opts->geometry;
   nvim_api_ui_attach(nvim, geo->w, geo->h, _ui_attached_cb, NULL);


   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_nvim_deleted_cb(void *data,
                 int   type EINA_UNUSED,
                 void *event)
{
   const Ecore_Exe_Event_Del *const info = event;
   s_nvim *const nvim = data;
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
_nvim_received_data_cb(void *data,
                       int   type EINA_UNUSED,
                       void *event)
{
   /* See https://github.com/msgpack-rpc/msgpack-rpc/blob/master/spec.md */
   const Ecore_Exe_Event_Data *const info = event;
   s_nvim *const nvim = data;
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
        else if (EINA_UNLIKELY(ret != MSGPACK_UNPACK_SUCCESS))
          {
             ERR("Error while unpacking data from neovim (0x%x)", ret);
             goto end_unpack;
          }
        const msgpack_object *const obj = &(result.data);

#if 1 /* Uncomment to roughly dump the received messages */
        msgpack_object_print(stderr, *obj);
        fprintf(stderr, "\n--------\n");
#endif

        if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_ARRAY))
          {
             ERR("Unexpected msgpack type 0x%x", obj->type);
             goto end_unpack;
          }

        const msgpack_object_array *const args = &(obj->via.array);
        const unsigned int response_args_count = 4u;
        const unsigned int notif_args_count = 3u;
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
           case 0: /* msgpack-rpc request */
              _handle_request(nvim, args);
              break;

           case 1: /* msgpack-rpc response */
              _handle_request_response(nvim, args);
              break;

           case 2: /* msgpack-rpc notification */
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

/* FIXME this is soooooo fragile */
static void
_nvim_runtime_load(s_nvim *nvim,
                   const char *filename)
{
   INF("Loading runtime file: '%s'", filename);

   Eina_Strbuf *const buf = eina_strbuf_new();
   if (EINA_UNLIKELY(! buf))
     {
        CRI("Failed to allocate string buffer");
        return;
     }
   eina_strbuf_append_printf(buf, ":source %s", filename);

   /* Send it to neovim */
   nvim_api_command(nvim, eina_strbuf_string_get(buf),
                    (unsigned int)eina_strbuf_length_get(buf),
                    NULL, NULL);
   eina_strbuf_free(buf);
}

static void
_nvim_builtin_runtime_load(s_nvim *nvim)
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

   _nvim_runtime_load(nvim, eina_strbuf_string_get(buf));
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

static unsigned int
_version_fragment_decode(const msgpack_object *version)
{
  /* A version shall be a positive integer that shall be contained within an
   * 'unsigned int' */
  if (EINA_UNLIKELY(version->type != MSGPACK_OBJECT_POSITIVE_INTEGER))
  {
    ERR("Version argument is expected to be a positive integer.");
    return 0u; /* Fallback to zero... */
  }
  assert(version->via.i64 >= 0);
  if (EINA_UNLIKELY(version->via.i64 > UINT_MAX))
  {
    ERR("Version is greater than %u, which is forbidden", UINT_MAX);
    return UINT_MAX; /* Truncating */
  }
  return (unsigned int)version->via.i64;
}


static inline Eina_Bool
_msgpack_streq(const msgpack_object_str *str, const char *with, size_t len)
{
  return 0 == strncmp(str->ptr, with, MIN(str->size, len));
}
#define _MSGPACK_STREQ(MsgPackStr, StaticStr) \
  _msgpack_streq(MsgPackStr, "" StaticStr "", sizeof(StaticStr) - 1u)

static void
_version_decode(s_nvim *nvim, const msgpack_object *args)
{
  /* A version shall be a dictionary containing the following parameters:
   *
   * {
   *   "major": X,
   *   "minor": Y,
   *   "patch": Z
   *   "api_level": A,
   *   "api_compatible": B,
   *   "api_prerelease": T/F,
   * }
   *
   * For now, we are only interested by 'major', 'minor' and 'patch'.
   */
  if (EINA_UNLIKELY(args->type != MSGPACK_OBJECT_MAP))
  {
    ERR("A dictionary was expected. Got type 0x%x", args->type);
    return;
  }
  const msgpack_object_map *const map = &(args->via.map);
  for (uint32_t i = 0u; i < map->size; i++)
  {
    const msgpack_object_kv *const kv = &(map->ptr[i]);
    if (EINA_UNLIKELY(kv->key.type != MSGPACK_OBJECT_STR))
    {
      ERR("Dictionary key is expected to be of type string");
      continue;
    }
    const msgpack_object_str *const key = &(kv->key.via.str);
    if (_MSGPACK_STREQ(key, "major"))
    { nvim->version.major = _version_fragment_decode(&(kv->val)); }
    else if (_MSGPACK_STREQ(key, "minor"))
    { nvim->version.minor = _version_fragment_decode(&(kv->val)); }
    else if (_MSGPACK_STREQ(key, "patch"))
    { nvim->version.patch = _version_fragment_decode(&(kv->val)); }
  }
}

static void
_ui_options_decode(s_nvim *nvim, const msgpack_object *args)
{
  /* The ui_options object is a list like what is written below:
   *
   *   [
   *     "rgb",
   *     "ext_cmdline",
   *     "ext_popupmenu",
   *     "ext_tabline",
   *     "ext_wildmenu",
   *     "ext_linegrid",
   *     "ext_hlstate"
   *   ]
   *
   */
  if (EINA_UNLIKELY(args->type != MSGPACK_OBJECT_ARRAY))
  {
    ERR("An array was expected. Got type 0x%x", args->type);
    return;
  }
  const msgpack_object_array *const arr = &(args->via.array);
  for (uint32_t i = 0u; i < arr->size; i++)
  {
    const msgpack_object *const o = &(arr->ptr[i]);
    const msgpack_object_str *const opt = EOVIM_MSGPACK_STRING_OBJ_EXTRACT(o, fail);

    nvim->features.linegrid |= (_MSGPACK_STREQ(opt, "ext_linegrid"));
    nvim->features.multigrid |= (_MSGPACK_STREQ(opt, "ext_multigrid"));
    nvim->features.cmdline |= (_MSGPACK_STREQ(opt, "ext_cmdline"));
    nvim->features.wildmenu |= (_MSGPACK_STREQ(opt, "ext_wildmenu"));
  }

  return;
fail:
  ERR("Failed to decode ui_options API");
}

static void
_api_decode_cb(s_nvim *nvim, void *data EINA_UNUSED, const msgpack_object *result)
{
  /* We expect two arguments:
   * 1) the channel ID. we don't care.
   * 2) a dictionary containing meta information - that's what we want
   */
  if (EINA_UNLIKELY(result->type != MSGPACK_OBJECT_ARRAY))
  {
    ERR("An array is expected. Got type 0x%x", result->type);
    return;
  }
  const msgpack_object_array *const args = &(result->via.array);
  if (EINA_UNLIKELY(args->size != 2u))
  {
    ERR("An array of two arguments is expected. Got %"PRIu32, args->size);
    return;
  }
  if (EINA_UNLIKELY(args->ptr[1].type != MSGPACK_OBJECT_MAP))
  {
    ERR("The second argument is expected to be a map.");
    return;
  }
  const msgpack_object_map *const map = &(args->ptr[1].via.map);

  /* Now that we have the map containing the API information, go through it to
   * extract what we need. Currently, we are only interested in the "version"
   * attribute */
  for (uint32_t i = 0u; i < map->size; i++)
  {
    const msgpack_object_kv *const kv = &(map->ptr[i]);
    if (EINA_UNLIKELY(kv->key.type != MSGPACK_OBJECT_STR))
    {
      ERR("Key is expected to be of type string. Got type 0x%x", kv->key.type);
      continue;
    }
    const msgpack_object_str *const key = &(kv->key.via.str);

    /* Decode the "version" arguments */
    if (_MSGPACK_STREQ(key, "version"))
    { _version_decode(nvim, &(kv->val)); }
    /* Decode the "ui_options" arguments */
    else if (_MSGPACK_STREQ(key, "ui_options"))
    { _ui_options_decode(nvim, &(kv->val)); }
  }

  /****************************************************************************
  * Now that we have decoded the API information, use them!
  *****************************************************************************/

  INF("Running Neovim version %u.%u.%u",
      nvim->version.major, nvim->version.minor, nvim->version.patch);
  if (EINA_UNLIKELY((nvim->version.major == 0) && (nvim->version.minor < 2)))
  {
    gui_die(&nvim->gui,
            "You are running neovim %u.%u.%u, which is unsupported. "
            "Please consider upgrading Neovim.",
            nvim->version.major, nvim->version.minor, nvim->version.patch);
  }
  /* We are now sure that we are running at least 0.2.0. *********************/

  /* We want linegrid */
  if (! nvim->features.linegrid)
  {
    gui_die(&nvim->gui,
      "Eovim only supports rendering with 'ext_linegrid', which you version of "
      "neovim (%u.%u.%u) does not provide.",
      nvim->version.major, nvim->version.minor, nvim->version.patch);
  }

  if (nvim->features.cmdline)
  { nvim_api_ui_ext_set(nvim, "ext_cmdline", nvim->config->ext_cmdline); }
  if (nvim->features.wildmenu)
  { nvim_api_ui_ext_set(nvim, "ext_wildmenu", nvim->config->ext_cmdline); }
  // TODO - uncomment when new UI will be implemented
  //if (nvim->features.linegrid)
  //{ nvim_api_ui_ext_set(nvim, "ext_linegrid", EINA_TRUE); }
  //if (nvim->features.multigrid)
  //{ nvim_api_ui_ext_set(nvim, "ext_multigrid", EINA_TRUE); }

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

static Eina_Bool _nvim_event_handlers_add(s_nvim *nvim)
{
   struct {
      const int event;
      const Ecore_Event_Handler_Cb callback;
   } const ctor[NVIM_HANDLERS_LAST] = {
      [NVIM_HANDLER_ADD] = {
         .event = ECORE_EXE_EVENT_ADD,
         .callback = _nvim_added_cb,
      },
      [NVIM_HANDLER_DEL] = {
         .event = ECORE_EXE_EVENT_DEL,
         .callback = _nvim_deleted_cb,
      },
      [NVIM_HANDLER_DATA] = {
         .event = ECORE_EXE_EVENT_DATA,
         .callback = _nvim_received_data_cb,
      },
      [NVIM_HANDLER_ERROR] = {
         .event = ECORE_EXE_EVENT_ERROR,
         .callback = _nvim_received_error_cb,
      },
   };
   int i;

   /* Create the handlers for all incoming events for spawned nvim instances */
   for (i = 0; i < NVIM_HANDLERS_LAST; i++)
     {
        nvim->event_handlers[i] =
           ecore_event_handler_add(ctor[i].event, ctor[i].callback, nvim);
        if (EINA_UNLIKELY(! nvim->event_handlers[i]))
          {
             CRI("Failed to create handler for event 0x%x", ctor[i].event);
             goto fail;
          }
     }

   return EINA_TRUE;

fail:
   for (i--; i >= 0; i--)
     ecore_event_handler_del(nvim->event_handlers[i]);
   return EINA_FALSE;
}

static void _nvim_event_handlers_del(s_nvim *nvim)
{
   for (int i = 0; i < NVIM_HANDLERS_LAST; i++)
     ecore_event_handler_del(nvim->event_handlers[i]);
}


/*============================================================================*
 *                                 Public API                                 *
 *============================================================================*/

uint32_t
nvim_next_uid_get(s_nvim *nvim)
{
   /* Overflow is not an error */
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
   ok &= eina_strbuf_append(cmdline, " --embed");

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

   /* Configure the event handlers */
   if (EINA_UNLIKELY(! _nvim_event_handlers_add(nvim)))
     {
        CRI("Failed to setup event handlers");
        goto del_mem;
     }

   /* We will enable mouse handling by default. We do not receive the
    * information from neovim unless we change mode. This is annoying. */
   nvim->mouse_enabled = EINA_TRUE;

   nvim->decode = eina_ustrbuf_new();
   if (EINA_UNLIKELY(! nvim->decode))
     {
        CRI("Failed to create unicode string buffer");
        goto del_events;
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

   /* Initialize the virtual interface to safe values (non-NULL pointers) */
   _virtual_interface_init(nvim);

   /* Add a callback to the vimenter request */
   nvim_request_add("vimenter", _vimenter_request_cb);

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
        goto del_config;
     }
   DBG("Running %s", eina_strbuf_string_get(cmdline));
   eina_strbuf_free(cmdline);

   /* FIXME These are sooo fragile. Rework that!!! */
   _nvim_builtin_runtime_load(nvim);
   nvim_api_var_integer_set(nvim, "eovim_running", 1);

   /* Create the GUI window */
   if (EINA_UNLIKELY(! gui_add(&nvim->gui, nvim)))
     {
        CRI("Failed to set up the graphical user interface");
        goto del_process;
     }
   if (opts->maximized)
      gui_maximized_set(&nvim->gui, opts->maximized);
   else if (opts->fullscreen)
      gui_fullscreen_set(&nvim->gui, opts->fullscreen);
   else
      gui_resize(&nvim->gui, opts->geometry.w, opts->geometry.h);

   return nvim;

del_process:
   ecore_exe_kill(nvim->exe);
del_config:
   config_free(nvim->config);
del_ustrbuf:
   eina_ustrbuf_free(nvim->decode);
del_events:
   _nvim_event_handlers_del(nvim);
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
        _nvim_event_handlers_del(nvim);
        msgpack_sbuffer_destroy(&nvim->sbuffer);
        msgpack_unpacker_destroy(&nvim->unpacker);
        config_free(nvim->config);
        eina_ustrbuf_free(nvim->decode);
        free(nvim);
     }
}

Eina_Bool nvim_flush(s_nvim *nvim)
{
   /* Send the data present in the msgpack buffer */
   const Eina_Bool ok =
     ecore_exe_send(nvim->exe, nvim->sbuffer.data, (int)nvim->sbuffer.size);

   /* Now that the data is gone (hopefully), clear the buffer */
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to send %zu bytes to neovim", nvim->sbuffer.size);
        msgpack_sbuffer_clear(&nvim->sbuffer);
        return EINA_FALSE;
     }
   DBG("Sent %zu bytes to neovim", nvim->sbuffer.size);
   msgpack_sbuffer_clear(&nvim->sbuffer);
   return EINA_TRUE;
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
