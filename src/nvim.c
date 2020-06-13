/* This file is part of Eovim, which is under the MIT License ****************/

#include "eovim/nvim_api.h"
#include "eovim/types.h"
#include "eovim/nvim.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim_event.h"
#include "eovim/nvim_request.h"
#include "eovim/nvim_helper.h"
#include "eovim/msgpack_helper.h"
#include "eovim/log.h"
#include "eovim/main.h"

/*============================================================================*
 *                                 Private API                                *
 *============================================================================*/

static Eina_Bool _handle_request(struct nvim *nvim, const msgpack_object_array *args)
{
	/* Retrieve the request identifier ****************************************/
	if (EINA_UNLIKELY(args->ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
		ERR("Second argument in request is expected to be an integer");
		return EINA_FALSE;
	}
	const uint64_t long_req_id = args->ptr[1].via.u64;
	if (EINA_UNLIKELY(long_req_id > UINT32_MAX)) {
		ERR("Request ID '%" PRIu64 " is too big", long_req_id);
		return EINA_FALSE;
	}
	const uint32_t req_id = (uint32_t)long_req_id;

	/* Retrieve the request arguments *****************************************/
	if (EINA_UNLIKELY(args->ptr[3].type != MSGPACK_OBJECT_ARRAY)) {
		ERR("Fourth argument in request is expected to be an array");
		return EINA_FALSE;
	}
	const msgpack_object_array *const req_args = &(args->ptr[3].via.array);

	/* Retrieve the request name **********************************************/
	if (EINA_UNLIKELY(args->ptr[2].type != MSGPACK_OBJECT_STR)) {
		ERR("Third argument in request is expected to be a string");
		return EINA_FALSE;
	}
	const msgpack_object_str *const str = &(args->ptr[2].via.str);
	Eina_Stringshare *const request = eina_stringshare_add_length(str->ptr, str->size);
	if (EINA_UNLIKELY(!request)) {
		ERR("Failed to create stringshare");
		return EINA_FALSE;
	}

	const Eina_Bool ok = nvim_request_process(nvim, request, req_args, req_id);
	eina_stringshare_del(request);
	return ok;
}

static Eina_Bool _handle_request_response(struct nvim *nvim, const msgpack_object_array *args)
{
	/* 2nd arg should be an integer */
	if (EINA_UNLIKELY(args->ptr[1].type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
		ERR("Second argument in response is expected to be an integer");
		goto fail;
	}

	/* When this variable is se to EINA_TRUE, we won't process the response.
    * This is here so we can go through the error message without crashing too
    * early. */
	Eina_Bool do_not_process = EINA_FALSE;

	/* Get the request from the pending requests list. */
	const uint32_t req_id = (uint32_t)args->ptr[1].via.u64;
	struct request *const req = nvim_api_request_find(nvim, req_id);
	if (EINA_UNLIKELY(!req)) {
		/* We probably should wait for the error message to be fetched back.
         * So we start by throwing an error, then we tell that the response
         * shall not be processed. */
		CRI("Uh... received a response to request %" PRIu32 ", but it was not "
		    "registered. Something wrong happend somewhere!",
		    req_id);
		do_not_process = EINA_TRUE;
	} else
		DBG("Received response to request %" PRIu32, req_id);

	/* If 3rd arg is an array, this is an error message. */
	const msgpack_object_type err_type = args->ptr[2].type;
	if (err_type == MSGPACK_OBJECT_ARRAY) {
		const msgpack_object_array *const err_args = &(args->ptr[2].via.array);
		if (EINA_UNLIKELY(err_args->size != 2)) {
			ERR("Error response is supposed to have two arguments");
			goto fail_req;
		}
		if (EINA_UNLIKELY(err_args->ptr[1].type != MSGPACK_OBJECT_STR)) {
			ERR("Error response is supposed to contain a string");
			goto fail_req;
		}
		const msgpack_object_str *const e = &(err_args->ptr[1].via.str);
		Eina_Stringshare *const err = eina_stringshare_add_length(e->ptr, e->size);
		if (EINA_UNLIKELY(!err)) {
			CRI("Failed to create stringshare");
			goto fail_req;
		}

		CRI("Neovim reported an error: %s", err);
		eina_stringshare_del(err);
		goto fail_req;
	} else if (err_type != MSGPACK_OBJECT_NIL) {
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

static Eina_Stringshare *_stringshare_extract(const msgpack_object *obj)
{
	if (obj->type == MSGPACK_OBJECT_STR) {
		const msgpack_object_str *const str = &(obj->via.str);
		return eina_stringshare_add_length(str->ptr, str->size);
	} else if (obj->type == MSGPACK_OBJECT_BIN) {
		const msgpack_object_bin *const bin = &(obj->via.bin);
		return eina_stringshare_add_length(bin->ptr, bin->size);
	} else {
		ERR("Second argument in notification is expected to be a string "
		    "(or BIN string), but it is of type 0x%x",
		    obj->type);
		return NULL;
	}
}

static Eina_Bool _handle_notification(struct nvim *nvim, const msgpack_object_array *args)
{
	/*
    * 2nd argument must be a string (or bin string).
    * It contains the METHOD to be called for the notification.
    * We decore the string as a stringshare, to feed it to our table of
    * methods.
    */
	Eina_Stringshare *const method = _stringshare_extract(&(args->ptr[1]));
	if (EINA_UNLIKELY(!method)) {
		CRI("Failed to create stringshare from Neovim method");
		goto fail;
	}
	DBG("Received notification '%s'", method);

	/*
    * 3rd argument must be an array of objects
    */
	if (EINA_UNLIKELY(args->ptr[2].type != MSGPACK_OBJECT_ARRAY)) {
		ERR("Third argument in notification is expected to be an array");
		goto del_method;
	}
	const msgpack_object_array *const args_arr = &(args->ptr[2].via.array);

	/* Find the method handler */
	const struct method *const meth = nvim_event_method_find(method);
	if (EINA_UNLIKELY(!meth)) {
		goto del_method;
	}

	/*
    * Go through the notification's commands. There are formatted of the form
    * [ command_name, Args... ]
    * So we expect arguments to be arrays of at least one element.
    * command_name must be a string!
    */
	for (unsigned int i = 0; i < args_arr->size; i++) {
		const msgpack_object *const arg = &(args_arr->ptr[i]);
		if (EINA_UNLIKELY(arg->type != MSGPACK_OBJECT_ARRAY)) {
			CRI("Expected argument of type array. Got 0x%x.", arg->type);
			continue; /* Try next element */
		}
		const msgpack_object_array *const cmd = &(arg->via.array);
		if (EINA_UNLIKELY(cmd->size < 1)) {
			CRI("Expected at least one argument. Got zero.");
			continue; /* Try next element */
		}
		Eina_Stringshare *const command = _stringshare_extract(&(cmd->ptr[0]));
		if (EINA_UNLIKELY(!command)) {
			CRI("Failed to create stringshare from command object");
			continue; /* Try next element */
		}
		const Eina_Bool ok = nvim_event_method_dispatch(nvim, meth, command, cmd);
		if (EINA_UNLIKELY((!ok) &&
				  (eina_log_domain_level_get("eovim") >= EINA_LOG_LEVEL_WARN))) {
			WRN("Command '%s' failed with input object:", command);
			fprintf(stderr, " -=> ");
			msgpack_object_print(stderr, *arg);
			fprintf(stderr, "\n");
		}
		eina_stringshare_del(command);
	}

	/* Notify we are done processing the batch of functions for this method */
	nvim_event_method_batch_end(nvim, meth);

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

static Eina_Bool _nvim_added_cb(void *const data, const int type EINA_UNUSED, void *const event)
{
	/* EFL versions 1.21 (and maybe 1.20 as well ??) have a bug. When coming out
    * of sleep/hibernation, a spurious event was sent, causing
    * ECORE_EXE_EVENT_ADD to be triggered with a NULL event, which is supposed
    * to be always set. This test prevents this spurious event to crash eovim */
	if (EINA_LIKELY(event != NULL)) {
		const Ecore_Exe_Event_Add *const info = event;

		/* Hey, did you know that Edje actually launches a process... hello
      * efreed! This has been going on for years... Now make sure we only
      * talk to neovim, and nobody else */
		const char *const tag = ecore_exe_tag_get(info->exe);
		if (tag && (0 == strcmp(tag, "neovim"))) {
			INF("Nvim process with PID %i was created", ecore_exe_pid_get(info->exe));
			nvim_attach(data);
		}
	}

	return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool _nvim_deleted_cb(void *data, int type EINA_UNUSED, void *event)
{
	const Ecore_Exe_Event_Del *const info = event;
	struct nvim *const nvim = data;
	const int pid = ecore_exe_pid_get(info->exe);

	/* We consider that neovim crashed if it receives an uncaught signal */
	if (info->signalled) {
		ERR("Process with PID %i died of uncaught signal %i", pid, info->exit_signal);
		gui_die(&nvim->gui,
			"The Neovim process %i died. Eovim cannot continue its execution", pid);
	} else {
		INF("Process with PID %i terminated with exit code %i", pid, info->exit_code);
		gui_del(&nvim->gui);
	}
	return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool _nvim_received_data_cb(void *data, int type EINA_UNUSED, void *event)
{
	/* See https://github.com/msgpack-rpc/msgpack-rpc/blob/master/spec.md */
	const Ecore_Exe_Event_Data *const info = event;
	struct nvim *const nvim = data;
	msgpack_unpacker *const unpacker = &nvim->unpacker;
	const size_t recv_size = (size_t)info->size;

	DBG("Incoming data from PID %u (size %zu)", ecore_exe_pid_get(info->exe), recv_size);

	/*
    * We have received something from NeoVim. We now must deserialize this.
    */
	msgpack_unpacked result;

	/* Resize the unpacking buffer if need be */
	if (msgpack_unpacker_buffer_capacity(unpacker) < recv_size) {
		const bool ret = msgpack_unpacker_reserve_buffer(unpacker, recv_size);
		if (!ret) {
			ERR("Memory reallocation of %zu bytes failed", recv_size);
			goto end;
		}
	}
	/* This seems to be required, but that's plain inefficiency */
	memcpy(msgpack_unpacker_buffer(unpacker), info->data, recv_size);
	msgpack_unpacker_buffer_consumed(unpacker, recv_size);

	msgpack_unpacked_init(&result);
	for (;;) {
		const msgpack_unpack_return ret = msgpack_unpacker_next(unpacker, &result);
		if (ret == MSGPACK_UNPACK_CONTINUE) {
			break;
		} else if (EINA_UNLIKELY(ret != MSGPACK_UNPACK_SUCCESS)) {
			ERR("Error while unpacking data from neovim (0x%x)", ret);
			goto end_unpack;
		}
		const msgpack_object *const obj = &(result.data);

#if 0 /* Uncomment to roughly dump the received messages */
        msgpack_object_print(stderr, *obj);
        fprintf(stderr, "\n--------\n");
#endif

		if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_ARRAY)) {
			ERR("Unexpected msgpack type 0x%x", obj->type);
			goto end_unpack;
		}

		const msgpack_object_array *const args = &(obj->via.array);
		const unsigned int response_args_count = 4u;
		const unsigned int notif_args_count = 3u;
		if ((args->size != response_args_count) && (args->size != notif_args_count)) {
			ERR("Unexpected count of arguments: %u.", args->size);
			goto end_unpack;
		}

		if (EINA_UNLIKELY(args->ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
			ERR("First argument in response is expected to be an integer");
			goto end_unpack;
		}
		switch (args->ptr[0].via.u64) {
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
			ERR("Invalid message identifier %" PRIu64, args->ptr[0].via.u64);
			goto end_unpack;
		}
	} /* End of message unpacking */

end_unpack:
	msgpack_unpacked_destroy(&result);
end:
	return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool _nvim_received_error_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
	const Ecore_Exe_Event_Data *const info = event;
	const char *const msg = info->data;
	ERR("Error: %s", msg);
	return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool _nvim_event_handlers_add(struct nvim *nvim)
{
	struct {
		const int event;
		const Ecore_Event_Handler_Cb callback;
	} const ctor[] = {
		{
			.event = ECORE_EXE_EVENT_ADD,
			.callback = _nvim_added_cb,
		},
		{
			.event = ECORE_EXE_EVENT_DEL,
			.callback = _nvim_deleted_cb,
		},
		{
			.event = ECORE_EXE_EVENT_DATA,
			.callback = _nvim_received_data_cb,
		},
		{
			.event = ECORE_EXE_EVENT_ERROR,
			.callback = _nvim_received_error_cb,
		},
	};
	static_assert(EINA_C_ARRAY_LENGTH(ctor) == EINA_C_ARRAY_LENGTH(nvim->event_handlers),
		      "Please fix your code!");
	size_t i = 0;

	/* Create the handlers for all incoming events for spawned nvim instances */
	for (; i < EINA_C_ARRAY_LENGTH(ctor); i++) {
		nvim->event_handlers[i] =
			ecore_event_handler_add(ctor[i].event, ctor[i].callback, nvim);
		if (EINA_UNLIKELY(!nvim->event_handlers[i])) {
			CRI("Failed to create handler for event 0x%x", ctor[i].event);
			goto fail;
		}
	}

	return EINA_TRUE;

fail:
	for (--i; i != SIZE_MAX; i--)
		ecore_event_handler_del(nvim->event_handlers[i]);
	return EINA_FALSE;
}

static void _nvim_event_handlers_del(struct nvim *nvim)
{
	for (size_t i = 0; i < EINA_C_ARRAY_LENGTH(nvim->event_handlers); i++)
		ecore_event_handler_del(nvim->event_handlers[i]);
}

/*============================================================================*
 *                                 Public API                                 *
 *============================================================================*/

uint32_t nvim_next_uid_get(struct nvim *nvim)
{
	/* Overflow is not an error */
	return nvim->request_id++;
}

struct nvim *nvim_new(const struct options *opts, const char *const args[])
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(opts, NULL);

	Eina_Bool ok;

	/* Forge the command-line for the nvim program. We manually enforce
    * --embed and --headless, because we are the gui client, and forward all
    * the options to the command-line. We are using an Eina_Strbuf to make no
    * assumption on the size of the command-line.
    */
	Eina_Strbuf *const cmdline = eina_strbuf_new();
	if (EINA_UNLIKELY(!cmdline)) {
		CRI("Failed to create strbuf");
		goto fail;
	}
	ok = eina_strbuf_append_printf(cmdline, "\"%s\"", opts->nvim_prog);
	ok &= eina_strbuf_append(cmdline, " --embed");

	for (const char *arg = *args; arg != NULL; arg = *(++args)) {
		ok &= eina_strbuf_append_printf(cmdline, " \"%s\"", arg);
	}
	if (EINA_UNLIKELY(!ok)) {
		CRI("Failed to correctly format the command line");
		goto del_strbuf;
	}

	/* First, create the nvim data */
	struct nvim *const nvim = calloc(1, sizeof(struct nvim));
	if (!nvim) {
		CRI("Failed to create nvim structure");
		goto del_strbuf;
	}
	nvim->opts = opts;

	/* Configure the event handlers */
	if (EINA_UNLIKELY(!_nvim_event_handlers_add(nvim))) {
		CRI("Failed to setup event handlers");
		goto del_mem;
	}

	/* We will enable mouse handling by default. We do not receive the
    * information from neovim unless we change mode. This is annoying. */
	nvim->mouse_enabled = EINA_TRUE;

	/* Initialze msgpack for RPC */
	msgpack_sbuffer_init(&nvim->sbuffer);
	msgpack_packer_init(&nvim->packer, &nvim->sbuffer, msgpack_sbuffer_write);
	msgpack_unpacker_init(&nvim->unpacker, 2048);

	nvim->modes = eina_hash_stringshared_new(EINA_FREE_CB(&nvim_mode_free));
	if (EINA_UNLIKELY(!nvim->modes)) {
		CRI("Failed to create hash map");
		goto del_events;
	}

	nvim->kind_styles = eina_hash_string_small_new(EINA_FREE_CB(&free));
	if (EINA_UNLIKELY(!nvim->kind_styles)) {
		CRI("Failed to create hash map");
		goto del_modes;
	}

	/* Create the neovim process */
	nvim->exe = ecore_exe_pipe_run(eina_strbuf_string_get(cmdline),
				       ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_WRITE |
					       ECORE_EXE_PIPE_ERROR | ECORE_EXE_TERM_WITH_PARENT,
				       nvim);
	if (EINA_UNLIKELY(!nvim->exe)) {
		CRI("Failed to execute nvim instance");
		goto del_kind_styles;
	}
	ecore_exe_tag_set(nvim->exe, "neovim");
	DBG("Running %s", eina_strbuf_string_get(cmdline));
	eina_strbuf_free(cmdline);

	/* Create the GUI window */
	if (EINA_UNLIKELY(!gui_add(&nvim->gui, nvim))) {
		CRI("Failed to set up the graphical user interface");
		goto del_process;
	}

	return nvim;

del_process:
	ecore_exe_kill(nvim->exe);
del_kind_styles:
	eina_hash_free(nvim->kind_styles);
del_modes:
	eina_hash_free(nvim->modes);
del_events:
	_nvim_event_handlers_del(nvim);
del_mem:
	free(nvim);
del_strbuf:
	eina_strbuf_free(cmdline);
fail:
	return NULL;
}

void nvim_free(struct nvim *const nvim)
{
	if (nvim) {
		_nvim_event_handlers_del(nvim);
		msgpack_sbuffer_destroy(&nvim->sbuffer);
		msgpack_unpacker_destroy(&nvim->unpacker);
		eina_hash_free(nvim->kind_styles);
		eina_hash_free(nvim->modes);
		free(nvim);
	}
}

Eina_Bool nvim_flush(struct nvim *nvim)
{
	/* Send the data present in the msgpack buffer */
	const Eina_Bool ok = ecore_exe_send(nvim->exe, nvim->sbuffer.data, (int)nvim->sbuffer.size);

	/* Now that the data is gone (hopefully), clear the buffer */
	if (EINA_UNLIKELY(!ok)) {
		CRI("Failed to send %zu bytes to neovim", nvim->sbuffer.size);
		msgpack_sbuffer_clear(&nvim->sbuffer);
		return EINA_FALSE;
	}
	DBG("Sent %zu bytes to neovim", nvim->sbuffer.size);
	msgpack_sbuffer_clear(&nvim->sbuffer);
	return EINA_TRUE;
}

void nvim_mouse_enabled_set(struct nvim *nvim, Eina_Bool enable)
{
	nvim->mouse_enabled = !!enable;
}

Eina_Bool nvim_mouse_enabled_get(const struct nvim *nvim)
{
	return nvim->mouse_enabled;
}

struct mode *nvim_mode_new(void)
{
	struct mode *const mode = calloc(1, sizeof(*mode));
	if (EINA_UNLIKELY(!mode)) {
		CRI("Failed to allocate memory of size %zu B", sizeof(*mode));
	}
	return mode;
}

void nvim_mode_free(struct mode *const mode)
{
	if (mode) {
		if (mode->name) {
			eina_stringshare_del(mode->name);
		}
		if (mode->short_name) {
			eina_stringshare_del(mode->short_name);
		}
		free(mode);
	}
}
