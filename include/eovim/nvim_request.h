/* This file is part of Eovim, which is under the MIT License ****************/

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
 * This function is responsible for generating in \p pk the response message
 * Pack the message: it is an array of four (4) items:
 *  - the rpc type:
 *    - 1 is a request response
 *  - the unique identifier of the request
 *  - the error return
 *  - the result return
 * See: https://github.com/msgpack-rpc/msgpack-rpc/blob/master/spec.md
 *
 * Then, call nvim_flush()
 */
typedef Eina_Bool (*f_nvim_request_cb)(s_nvim *nvim, const msgpack_object_array *args,
                                       msgpack_packer *pk, uint32_t req_id);

Eina_Bool nvim_request_init(void);
void nvim_request_shutdown(void);

Eina_Bool nvim_request_add(const char *request_name, f_nvim_request_cb func);
void nvim_request_del(const char *request_name);

Eina_Bool
nvim_request_process(s_nvim *nvim, Eina_Stringshare *request,
                     const msgpack_object_array *args, uint32_t req_id);

#endif /* ! EOVIM_NVIM_REQUEST_H__ */
