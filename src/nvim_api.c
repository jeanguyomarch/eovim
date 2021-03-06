/* This file is part of Eovim, which is under the MIT License ****************/

#include "eovim/types.h"
#include "eovim/log.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim_event.h"
#include "eovim/nvim.h"

struct request {
	EINA_INLIST;
	struct {
		f_nvim_api_cb func;
		void *data;
	} cb;
	uint32_t uid;
};

/* Mempool to allocate the requests */
static Eina_Mempool *_mempool;

static struct request *_request_new(struct nvim *nvim, const char *rpc_name, size_t rpc_name_len)
{
	struct request *const req = eina_mempool_calloc(_mempool, sizeof(struct request));
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to allocate memory for a request object");
		return NULL;
	}

	req->uid = nvim_next_uid_get(nvim);
	DBG("Preparing request '%s' with id %" PRIu32, rpc_name, req->uid);

	/* The buffer MUST be empty before preparing another request. If this is not
    * the case, something went very wrong! Discard the buffer and keep going */
	if (EINA_UNLIKELY(nvim->sbuffer.size != 0u)) {
		ERR("The buffer is not empty. I've messed up somewhere");
		msgpack_sbuffer_clear(&nvim->sbuffer);
	}

	/* Keep the request around */
	nvim->requests = eina_inlist_append(nvim->requests, EINA_INLIST_GET(req));

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

static Eina_Bool _request_send(struct nvim *nvim, struct request *req)
{
	/* Finally, send that to the slave neovim process */
	if (EINA_UNLIKELY(!nvim_flush(nvim))) {
		nvim_api_request_free(nvim, req);
		return EINA_FALSE;
	}
	return EINA_TRUE;
}

struct request *nvim_api_request_find(const struct nvim *nvim, uint32_t req_id)
{
	struct request *req;

	EINA_INLIST_FOREACH (nvim->requests, req)
		if (req->uid == req_id)
			return req;
	return NULL;
}

void nvim_api_request_free(struct nvim *nvim, struct request *req)
{
	nvim->requests = eina_inlist_remove(nvim->requests, EINA_INLIST_GET(req));
	eina_mempool_free(_mempool, req);
}

void nvim_api_request_call(struct nvim *nvim, const struct request *req,
			   const msgpack_object *result)
{
	if (req->cb.func) {
		req->cb.func(nvim, req->cb.data, result);
	}
}

Eina_Bool nvim_api_ui_attach(struct nvim *nvim, unsigned int width, unsigned int height,
			     f_nvim_api_cb func, void *func_data)
{
	const char api[] = "nvim_ui_attach";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to create request");
		return EINA_FALSE;
	}
	req->cb.func = func;
	req->cb.data = func_data;

	msgpack_packer *const pk = &nvim->packer;
	msgpack_pack_array(pk, 3);
	msgpack_pack_int64(pk, width);
	msgpack_pack_int64(pk, height);

	/* Pack the only option to be always enabled */
	const char *const keys[] = {
		"rgb",
		"ext_linegrid",
		"ext_hlstate",
	};

	msgpack_pack_map(pk, EINA_C_ARRAY_LENGTH(keys));
	for (size_t i = 0; i < EINA_C_ARRAY_LENGTH(keys); i++) {
		const size_t len = strlen(keys[i]);
		msgpack_pack_str(pk, len);
		msgpack_pack_str_body(pk, keys[i], len);
		msgpack_pack_true(pk);
	}

	return _request_send(nvim, req);
}

Eina_Bool nvim_api_get_api_info(struct nvim *nvim, f_nvim_api_cb cb, void *data)
{
	const char api[] = "nvim_get_api_info";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to create request");
		return EINA_FALSE;
	}
	req->cb.func = cb;
	req->cb.data = data;

	msgpack_packer *const pk = &nvim->packer;
	msgpack_pack_array(pk, 0);

	return _request_send(nvim, req);
}

Eina_Bool nvim_api_ui_ext_set(struct nvim *const nvim, const char *const key, Eina_Bool enabled)
{
	const char api[] = "nvim_ui_set_option";

	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to create request");
		return EINA_FALSE;
	}
	const size_t len = strlen(key);
	msgpack_packer *const pk = &nvim->packer;
	msgpack_pack_array(pk, 2);
	msgpack_pack_str(pk, len);
	msgpack_pack_str_body(pk, key, len);
	if (enabled)
		msgpack_pack_true(pk);
	else
		msgpack_pack_false(pk);
	INF("Externalized UI option '%s' => %s", key, enabled ? "on" : "off");
	return _request_send(nvim, req);
}

Eina_Bool nvim_api_ui_try_resize(struct nvim *nvim, unsigned int width, unsigned height)
{
	EINA_SAFETY_ON_FALSE_RETURN_VAL(width > 0 && height > 0, EINA_FALSE);
	const char api[] = "nvim_ui_try_resize";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to create request");
		return EINA_FALSE;
	}

	msgpack_packer *const pk = &nvim->packer;
	msgpack_pack_array(pk, 2);
	msgpack_pack_int64(pk, width);
	msgpack_pack_int64(pk, height);

	return _request_send(nvim, req);
}

Eina_Bool nvim_api_eval(struct nvim *nvim, const char *input, size_t input_size, f_nvim_api_cb func,
			void *func_data)
{
	const char api[] = "nvim_eval";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
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

Eina_Bool nvim_api_command_output(struct nvim *nvim, const char *input, size_t input_size,
				  f_nvim_api_cb func, void *func_data)
{
	const char api[] = "nvim_command_output";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
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

Eina_Bool nvim_api_get_var(struct nvim *nvim, const char *var, f_nvim_api_cb func, void *func_data)
{
	const char api[] = "nvim_get_var";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to create request");
		return EINA_FALSE;
	}
	req->cb.func = func;
	req->cb.data = func_data;

	const size_t var_size = strlen(var);

	msgpack_packer *const pk = &nvim->packer;
	msgpack_pack_array(pk, 1);
	msgpack_pack_str(pk, var_size);
	msgpack_pack_str_body(pk, var, var_size);

	return _request_send(nvim, req);
}

Eina_Bool nvim_api_command(struct nvim *nvim, const char *input, size_t input_size,
			   f_nvim_api_cb func, void *func_data)
{
	const char api[] = "nvim_command";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to create request");
		return EINA_FALSE;
	}
	req->cb.func = func;
	req->cb.data = func_data;

	DBG("Running nvim command: %s", input);

	msgpack_packer *const pk = &nvim->packer;
	msgpack_pack_array(pk, 1);
	msgpack_pack_str(pk, input_size);
	msgpack_pack_str_body(pk, input, input_size);

	return _request_send(nvim, req);
}

Eina_Bool nvim_api_input(struct nvim *nvim, const char *input, size_t input_size)
{
	const char api[] = "nvim_input";
	struct request *const req = _request_new(nvim, api, sizeof(api) - 1);
	if (EINA_UNLIKELY(!req)) {
		CRI("Failed to create request");
		return EINA_FALSE;
	}

	msgpack_packer *const pk = &nvim->packer;
	msgpack_pack_array(pk, 1);
	msgpack_pack_str(pk, input_size);
	msgpack_pack_str_body(pk, input, input_size);

	return _request_send(nvim, req);
}

Eina_Bool nvim_api_init(void)
{
	_mempool = eina_mempool_add("chained_mempool", "struct request", NULL,
				    sizeof(struct request), 8);
	if (EINA_UNLIKELY(!_mempool)) {
		CRI("Failed to initialize chained mempool");
		return EINA_FALSE;
	}
	return EINA_TRUE;
}

void nvim_api_shutdown(void)
{
	eina_mempool_del(_mempool);
}
