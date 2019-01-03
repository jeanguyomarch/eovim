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

#ifndef __EOVIM_API_H__
#define __EOVIM_API_H__

#include "eovim/types.h"
#include <Eina.h>
#include <msgpack.h>

Eina_Bool nvim_api_ui_attach(s_nvim *nvim, unsigned int width, unsigned int height);
Eina_Bool nvim_api_get_api_info(s_nvim *nvim, f_nvim_api_cb cb, void *data);
Eina_Bool nvim_api_ui_try_resize(s_nvim *nvim, unsigned int width, unsigned height);
Eina_Bool nvim_api_ui_ext_cmdline_set(s_nvim *nvim, Eina_Bool externalize);
Eina_Bool nvim_api_ui_ext_wildmenu_set(s_nvim *nvim, Eina_Bool externalize);
Eina_Bool nvim_api_input(s_nvim *nvim, const char *input, size_t input_size);

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

Eina_List *nvim_api_request_find(const s_nvim *nvim, uint32_t req_id);
void nvim_api_request_free(s_nvim *nvim, Eina_List *req_item);
void nvim_api_request_call(s_nvim *nvim, const Eina_List *req_item, const msgpack_object *result);
Eina_Bool nvim_api_var_integer_set(s_nvim *nvim, const char *name, int value);

Eina_Bool nvim_api_init(void);
void nvim_api_shutdown(void);

#endif /* ! __EOVIM_API_H__ */
