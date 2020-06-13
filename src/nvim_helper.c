/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/nvim.h>
#include <eovim/nvim_helper.h>
#include <eovim/msgpack_helper.h>
#include <eovim/nvim_api.h>
#include <eovim/log.h>
#include <msgpack.h>

Eina_Bool nvim_helper_autocmd_do(struct nvim *const nvim, const char *const event,
				 const f_nvim_api_cb func, void *const func_data)
{
	/* Compose the vim command that will trigger an autocmd for the User event,
    * with our custom args */
	char buf[512];
	const int bytes = snprintf(buf, sizeof(buf), ":doautocmd User %s", event);
	if (EINA_UNLIKELY(bytes < 0)) {
		CRI("Failed to write data in stack buffer");
		return EINA_FALSE;
	}

	/* Make neovim execute this */
	const Eina_Bool ok = nvim_api_command(nvim, buf, (size_t)bytes, func, func_data);
	if (EINA_UNLIKELY(!ok))
		ERR("Failed to execute autocmd via: '%s'", buf);
	return ok;
}

/** \return TRUE if \p res contains a strictly positive integer */
static inline Eina_Bool parse_config_boolean(const msgpack_object *const res)
{
	return (res->type == MSGPACK_OBJECT_POSITIVE_INTEGER) && (res->via.u64 != UINT64_C(0));
}

static void parse_theme_config(struct nvim *const nvim EINA_UNUSED, void *const data,
			       const msgpack_object *const result)
{
	Eina_Bool *const param = data;
	*param = parse_config_boolean(result);
}

static void parse_ext_config(struct nvim *const nvim, void *const data,
			     const msgpack_object *const result)
{
	const char *const key = data;
	const Eina_Bool param = parse_config_boolean(result);
	nvim_api_ui_ext_set(nvim, key, param);
}

static void parse_completion_styles(struct nvim *const nvim, void *const data EINA_UNUSED,
				    const msgpack_object *const result)
{
	const msgpack_object_map *const map = MPACK_MAP_EXTRACT(result, return );
	const msgpack_object *o_key, *o_val;
	unsigned int it;

	MPACK_MAP_ITER (map, it, o_key, o_val) {
		const msgpack_object_str *const k = MPACK_STRING_OBJ_EXTRACT(o_key, continue);
		const msgpack_object_str *const v = MPACK_STRING_OBJ_EXTRACT(o_val, continue);
		char *const key = strndup(k->ptr, k->size);
		if (EINA_UNLIKELY(!key)) {
			CRI("Failed to allocate memory");
			continue;
		}
		char *const val = strndup(v->ptr, v->size);
		if (EINA_UNLIKELY(!val)) {
			CRI("Failed to allocate memory");
			free(key);
			continue;
		}
		const Eina_Bool ok = eina_hash_direct_add(nvim->kind_styles, key, val);
		if (EINA_UNLIKELY(!ok)) {
			ERR("Failed to add key-value to hash map");
			free(key);
			free(val);
			continue;
		}
	}
}

Eina_Bool nvim_helper_config_reload(struct nvim *const nvim)
{
	struct gui *const gui = &nvim->gui;

	/* Retrive theme-oriented configuration */
	nvim_api_get_var(nvim, "eovim_theme_bell_enabled", &parse_theme_config,
			 &gui->theme.bell_enabled);
	nvim_api_get_var(nvim, "eovim_theme_react_to_key_presses", &parse_theme_config,
			 &gui->theme.react_to_key_presses);
	nvim_api_get_var(nvim, "eovim_theme_react_to_caps_lock", &parse_theme_config,
			 &gui->theme.react_to_caps_lock);

	nvim_api_get_var(nvim, "eovim_ext_tabline", &parse_ext_config, "ext_tabline");
	nvim_api_get_var(nvim, "eovim_ext_popupmenu", &parse_ext_config, "ext_popupmenu");
	nvim_api_get_var(nvim, "eovim_ext_cmdline", &parse_ext_config, "ext_cmdline");

	nvim_api_get_var(nvim, "eovim_theme_completion_styles", &parse_completion_styles, NULL);
	//nvim_api_get_var(nvim, "eovim_ext_multigrid",
	//  &parse_config_boolean, &gui->ext.multigrid);
	return EINA_TRUE;
}
