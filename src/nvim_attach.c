/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/nvim_api.h>
#include <eovim/types.h>
#include <eovim/nvim.h>
#include <eovim/nvim_api.h>
#include <eovim/nvim_event.h>
#include <eovim/nvim_helper.h>
#include <eovim/nvim_request.h>
#include <eovim/msgpack_helper.h>
#include <eovim/log.h>
#include <eovim/main.h>

static unsigned int _version_fragment_decode(const msgpack_object *version)
{
	/* A version shall be a positive integer that shall be contained within an
   * 'unsigned int' */
	if (EINA_UNLIKELY(version->type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
		ERR("Version argument is expected to be a positive integer.");
		return 0u; /* Fallback to zero... */
	}
	assert(version->via.i64 >= 0);
	if (EINA_UNLIKELY(version->via.i64 > UINT_MAX)) {
		ERR("Version is greater than %u, which is forbidden", UINT_MAX);
		return UINT_MAX; /* Truncating */
	}
	return (unsigned int)version->via.i64;
}

static void _version_decode(struct nvim *nvim, const msgpack_object *args)
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
	if (EINA_UNLIKELY(args->type != MSGPACK_OBJECT_MAP)) {
		ERR("A dictionary was expected. Got type 0x%x", args->type);
		return;
	}
	const msgpack_object_map *const map = &(args->via.map);
	for (uint32_t i = 0u; i < map->size; i++) {
		const msgpack_object_kv *const kv = &(map->ptr[i]);
		if (EINA_UNLIKELY(kv->key.type != MSGPACK_OBJECT_STR)) {
			ERR("Dictionary key is expected to be of type string");
			continue;
		}
		const msgpack_object_str *const key = &(kv->key.via.str);
		if (_MSGPACK_STREQ(key, "major")) {
			nvim->version.major = _version_fragment_decode(&(kv->val));
		} else if (_MSGPACK_STREQ(key, "minor")) {
			nvim->version.minor = _version_fragment_decode(&(kv->val));
		} else if (_MSGPACK_STREQ(key, "patch")) {
			nvim->version.patch = _version_fragment_decode(&(kv->val));
		}
	}
}

static void _ui_options_decode(struct nvim *nvim, const msgpack_object *args)
{
	/* The ui_options object is a list like what is written below:
	 *
	 *   [
	 *     "rgb",
	 *     "ext_cmdline",
	 *     "ext_popupmenu",
	 *     "ext_tabline",
	 *     "ext_linegrid",
	 *     "ext_hlstate"
	 *   ]
	 *
	 */
	if (EINA_UNLIKELY(args->type != MSGPACK_OBJECT_ARRAY)) {
		ERR("An array was expected. Got type 0x%x", args->type);
		return;
	}
	const msgpack_object_array *const arr = &(args->via.array);
	for (uint32_t i = 0u; i < arr->size; i++) {
		const msgpack_object *const o = &(arr->ptr[i]);
		const msgpack_object_str *const opt = MPACK_STRING_OBJ_EXTRACT(o, goto fail);

		nvim->features.linegrid |= (_MSGPACK_STREQ(opt, "ext_linegrid"));
		nvim->features.multigrid |= (_MSGPACK_STREQ(opt, "ext_multigrid"));
		nvim->features.cmdline |= (_MSGPACK_STREQ(opt, "ext_cmdline"));
		nvim->features.tabline |= (_MSGPACK_STREQ(opt, "ext_tabline"));
		nvim->features.popupmenu |= (_MSGPACK_STREQ(opt, "ext_popupmenu"));
	}

	if (EINA_UNLIKELY(!nvim->features.linegrid)) {
		gui_die(&nvim->gui,
			"You are running neovim %u.%u.%u, which does not provide support "
			"for the 'ext_linegrid' feature. Please upgrade neovim.",
			nvim->version.major, nvim->version.minor, nvim->version.patch);
	}

	return;
fail:
	ERR("Failed to decode ui_options API");
}

/******************************************************************************
 *                                  - 5 -
 *
 * The UI is now attached. The init.vim has been sourced. We will start by
 * fetching configuration variables, that will impact the theme and external
 * UI features.
 *
 * This is a bit tricky, though... Indeed neovim has just sent a BLOCKING
 * request. That is: nothing will be displayed to the user until we answer the
 * request! So we must send our response back to neovim before doing anything.
 *****************************************************************************/
static Eina_Bool _ui_attached_cb(struct nvim *const nvim,
				 const msgpack_object_array *const args EINA_UNUSED,
				 msgpack_packer *const pk, const uint32_t req_id)
{
	/* The "vimenter" request will not happen again. Delete */
	nvim_request_del("vimenter");

	/* Load the user configuration */
	nvim_helper_config_reload(nvim);

	/* Now, generate the response: everything went fine */
	msgpack_pack_array(pk, 4);
	msgpack_pack_int(pk, 1);
	msgpack_pack_uint32(pk, req_id);
	msgpack_pack_nil(pk); /* Error */
	msgpack_pack_nil(pk); /* Result */
	nvim_flush(nvim);

	/* Okay, start running the GUI! */
	gui_ready_set(&nvim->gui);

	/* Notify the user that we are ready to roll */
	nvim_helper_autocmd_do(nvim, "EovimReady", NULL, NULL);
	return EINA_TRUE;
}

/******************************************************************************
 *                                  - 4 -
 *
 * This is called after the VimEnter autocmd has been registered.
 * We will now really attach to nvim. It is the vimenter request that will
 * trigger the _ui_attached_cb handler
 *****************************************************************************/
static void _vimenter_registered_cb(struct nvim *const nvim, void *const data EINA_UNUSED,
				    const msgpack_object *const result EINA_UNUSED)
{
	const struct geometry *const geo = &nvim->opts->geometry;
	nvim_api_ui_attach(nvim, geo->w, geo->h, NULL, NULL);
}

/******************************************************************************
 *                                  - 3 -
 *
 * This is called after we send our custom vim runtime. We will now register
 * a vimenter autocmd.
 *****************************************************************************/
static void _eovim_runtime_loaded_cb(struct nvim *const nvim, void *const data,
				     const msgpack_object *const result EINA_UNUSED)
{
	/* We re-use the strbuf previously used to compose the command that allows the
	 * registration of the VimEnter autocmd */
	Eina_Strbuf *const buf = data;

	eina_strbuf_reset(buf);
	eina_strbuf_append_printf(
		buf, "autocmd VimEnter * call rpcrequest(%" PRIu64 ", 'vimenter')", nvim->channel);

	/* Create the vimenter request handler, so we can be notified after
	 * nvim_ui_attached has been processed */
	nvim_request_add("vimenter", _ui_attached_cb);

	/* Request the registration of the vimenter autocmd */
	nvim_api_command(nvim, eina_strbuf_string_get(buf),
			 (unsigned int)eina_strbuf_length_get(buf), &_vimenter_registered_cb, NULL);

	eina_strbuf_free(buf);
}

/******************************************************************************
 *                                  - 2 -
 *
 * This is called after we have collected neovim's capabilities. We now send
 * our own vim runtime to neovim, before init.vim is sourced
 *****************************************************************************/
static void _nvim_runtime_load(struct nvim *const nvim)
{
	Eina_Strbuf *const buf = eina_strbuf_new();
	if (EINA_UNLIKELY(!buf)) {
		CRI("Failed to allocate string buffer");
		return;
	}
	eina_strbuf_append(buf, ":source ");

	/* Compose the path to the runtime file */
	const char *const dir = (main_in_tree_is()) ? SOURCE_DATA_DIR : elm_app_data_dir_get();
	eina_strbuf_append_printf(buf, "%s/vim/runtime.vim", dir);

	/* Send it to neovim */
	nvim_api_command(nvim, eina_strbuf_string_get(buf),
			 (unsigned int)eina_strbuf_length_get(buf), &_eovim_runtime_loaded_cb, buf);
}

/******************************************************************************
 *                                  - 1 -
 *
 * This is called when neovim sends us its capabilities
 *****************************************************************************/
static void _api_decode_cb(struct nvim *nvim, void *data EINA_UNUSED, const msgpack_object *result)
{
	/* We expect two arguments:
	 * 1) the channel ID.
	 * 2) a dictionary containing meta information - that's what we want
	 */
	if (EINA_UNLIKELY(result->type != MSGPACK_OBJECT_ARRAY)) {
		ERR("An array is expected. Got type 0x%x", result->type);
		return;
	}
	const msgpack_object_array *const args = &(result->via.array);
	if (EINA_UNLIKELY(args->size != 2u)) {
		ERR("An array of two arguments is expected. Got %" PRIu32, args->size);
		return;
	}
	if (EINA_UNLIKELY(args->ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
		ERR("The first argument is expected to be a positive integer");
		return;
	}
	nvim->channel = args->ptr[0].via.u64;
	if (EINA_UNLIKELY(args->ptr[1].type != MSGPACK_OBJECT_MAP)) {
		ERR("The second argument is expected to be a map.");
		return;
	}
	const msgpack_object_map *const map = &(args->ptr[1].via.map);

	/* Now that we have the map containing the API information, go through it to
	 * extract what we need. Currently, we are only interested in the "version"
	 * attribute */
	for (uint32_t i = 0u; i < map->size; i++) {
		const msgpack_object_kv *const kv = &(map->ptr[i]);
		if (EINA_UNLIKELY(kv->key.type != MSGPACK_OBJECT_STR)) {
			ERR("Key is expected to be of type string. Got type 0x%x", kv->key.type);
			continue;
		}
		const msgpack_object_str *const key = &(kv->key.via.str);

		/* Decode the "version" arguments */
		if (_MSGPACK_STREQ(key, "version")) {
			_version_decode(nvim, &(kv->val));
		}
		/* Decode the "ui_options" arguments */
		else if (_MSGPACK_STREQ(key, "ui_options")) {
			_ui_options_decode(nvim, &(kv->val));
		}
	}

	/****************************************************************************
	 * Now that we have decoded the API information, use them!
	 *****************************************************************************/
	INF("Running Neovim version %u.%u.%u", nvim->version.major, nvim->version.minor,
	    nvim->version.patch);

	/****************************************************************************
	 * Now that we are done with neovim's capabilities, time to load our initial
	 * vimscript runtime, before the init.vim is sourced ***********************/
	_nvim_runtime_load(nvim);
}

/******************************************************************************
 * This is the entry point that describes how eovim attaches to neovim. Because
 * we are in an asynchronous world now (two processes talk to each other in a
 * non-blocking manner), callback chains are everywhere!
 *
 * We follow the ui-startup procedure (see :help ui-startup). Note that we
 * need to query the neovim API to determine what neovim can do (or cannot do).
 * We also want to query some variable set from vimscript, to use or not some
 * externalized UI features.
 *
 *****************************************************************************/
void nvim_attach(struct nvim *const nvim)
{
	/* We first start be querying neovim's capabilities */
	nvim_api_get_api_info(nvim, _api_decode_cb, NULL);
}
