/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_API_H__
#define __EOVIM_API_H__

#include "eovim/types.h"
#include <Eina.h>
#include <msgpack.h>

Eina_Bool nvim_api_ui_attach(s_nvim *nvim, unsigned int width, unsigned int height,
                             f_nvim_api_cb func, void *func_data);
Eina_Bool nvim_api_get_api_info(s_nvim *nvim, f_nvim_api_cb cb, void *data);
Eina_Bool nvim_api_ui_try_resize(s_nvim *nvim, unsigned int width, unsigned height);
Eina_Bool nvim_api_ui_ext_set(s_nvim *nvim, const char *key, Eina_Bool enabled);
Eina_Bool nvim_api_input(s_nvim *nvim, const char *input, size_t input_size);
Eina_Bool nvim_api_get_var(s_nvim *nvim, const char *var, f_nvim_api_cb func, void *func_data);

Eina_Bool nvim_api_eval(s_nvim *nvim, const char *input, size_t input_size,
                        f_nvim_api_cb func, void *func_data);
Eina_Bool
nvim_api_command_output(s_nvim *nvim,
                        const char *input, size_t input_size,
                        f_nvim_api_cb func, void *func_data);

Eina_Bool
nvim_api_command(s_nvim *nvim,
                 const char *input, size_t input_size,
                 f_nvim_api_cb func, void *func_data);

s_request *nvim_api_request_find(const s_nvim *nvim, uint32_t req_id);
void nvim_api_request_free(s_nvim *nvim, s_request *req);
void nvim_api_request_call(s_nvim *nvim, const s_request *req, const msgpack_object *result);

Eina_Bool nvim_api_init(void);
void nvim_api_shutdown(void);

#endif /* ! __EOVIM_API_H__ */
