/*
 * Copyright (c) 2019 Jean Guyomarc'h
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

#ifndef EOVIM_NVIM_REQUEST_H__
#define EOVIM_NVIM_REQUEST_H__

#include "eovim/types.h"

/**
 * Callback signature used when replying to a request.
 *
 * @param[in] nvim The neovim handle
 * @param[in] args Array of arguments from the request
 * @param[in,out] pk Msgpack packer to be used to write the error and the
 *   result of the request. See msgpack-rpc.
 * @return EINA_TRUE on success, EINA_FALSE on failure
 *
 * @note This function is responsible for calling nvim_flush().
 */
typedef Eina_Bool (*f_nvim_request_cb)(s_nvim *nvim, const msgpack_object_array *args,
                                       msgpack_packer *pk);

Eina_Bool nvim_request_init(void);
void nvim_request_shutdown(void);

Eina_Bool nvim_request_add(const char *request_name, f_nvim_request_cb func);
void nvim_request_del(const char *request_name);

Eina_Bool
nvim_request_process(s_nvim *nvim, Eina_Stringshare *request,
                     const msgpack_object_array *args, uint32_t req_id);

#endif /* ! EOVIM_NVIM_REQUEST_H__ */
