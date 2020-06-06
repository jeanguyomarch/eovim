/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_API_H__
#define __EOVIM_API_H__

#include "eovim/types.h"
#include <Eina.h>
#include <msgpack.h>

Eina_Bool nvim_api_ui_attach(struct nvim *nvim, unsigned int width, unsigned int height,
			     f_nvim_api_cb func, void *func_data);
Eina_Bool nvim_api_get_api_info(struct nvim *nvim, f_nvim_api_cb cb, void *data);
Eina_Bool nvim_api_ui_try_resize(struct nvim *nvim, unsigned int width, unsigned height);
Eina_Bool nvim_api_ui_ext_set(struct nvim *nvim, const char *key, Eina_Bool enabled);
Eina_Bool nvim_api_input(struct nvim *nvim, const char *input, size_t input_size);
Eina_Bool nvim_api_get_var(struct nvim *nvim, const char *var, f_nvim_api_cb func, void *func_data);

Eina_Bool nvim_api_eval(struct nvim *nvim, const char *input, size_t input_size, f_nvim_api_cb func,
			void *func_data);
Eina_Bool nvim_api_command_output(struct nvim *nvim, const char *input, size_t input_size,
				  f_nvim_api_cb func, void *func_data);

Eina_Bool nvim_api_command(struct nvim *nvim, const char *input, size_t input_size,
			   f_nvim_api_cb func, void *func_data);

struct request *nvim_api_request_find(const struct nvim *nvim, uint32_t req_id);
void nvim_api_request_free(struct nvim *nvim, struct request *req);
void nvim_api_request_call(struct nvim *nvim, const struct request *req,
			   const msgpack_object *result);

Eina_Bool nvim_api_init(void);
void nvim_api_shutdown(void);

#endif /* ! __EOVIM_API_H__ */
