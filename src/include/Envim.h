/*
 * Copyright (c) 2017 Jean Guyomarc'h
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

#ifndef __ENVIM_H__
#define __ENVIM_H__

#include <msgpack.h>
#include <Elementary.h>

typedef struct
{
   unsigned int major;
   unsigned int minor;
   unsigned int patch;
} s_version;

typedef struct
{
   s_version version;
} s_api;

typedef struct
{
   s_api api;

   Ecore_Exe *exe;
   Evas_Object *win;
   Eina_List *requests;

   msgpack_zone mempool;
   msgpack_sbuffer sbuffer;
   msgpack_packer packer;
   uint64_t request_id;
} s_nvim;

typedef enum
{
   REQUEST_NONE = 0,
   REQUEST_GET_API_INFO,

   __REQUEST_LAST /* Sentinel */
} e_request;

typedef enum
{
   API_KEY_INVALID = 0,
   API_KEY_VERSION,
   API_KEY_FUNCTIONS,
   API_KEY_TYPES,
   API_KEY_UI_EVENTS,

   __API_KEY_LAST /* Sentinel */
} e_api_key;

typedef enum
{
   ARG_TYPE_INT64,
   ARG_TYPE_STRING,
   ARG_TYPE_BOOL,
   ARG_TYPE_FLOAT64,
   ARG_TYPE_ARRAY,
   ARG_TYPE_MAP,
} e_arg_type;

typedef struct
{
   uint64_t uid;
   e_request type;
} s_request;

typedef void (*f_request_handler)(s_nvim *nvim, const s_request *req, const msgpack_object_array *args);

typedef struct
{
   Eina_Stringshare *method;
   const f_request_handler handler;
   const unsigned int in_args_count;
   const unsigned int out_min_args;
   const unsigned int out_max_args;
   const e_arg_type in_args_types[];
} s_request_info;





extern int _envim_log_domain;

#define DBG(...) EINA_LOG_DOM_DBG(_envim_log_domain, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_envim_log_domain, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_envim_log_domain, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_envim_log_domain, __VA_ARGS__)
#define CRI(...) EINA_LOG_DOM_CRIT(_envim_log_domain, __VA_ARGS__)

Eina_Bool nvim_init(void);
void nvim_shutdown(void);
s_nvim *nvim_new(void);
void nvim_free(s_nvim *nvim);

Eina_Bool request_init(void);
void request_shutdown(void);
s_request *request_new(uint64_t req_uid, e_request req_type);
void request_free(s_request *req);
const s_request_info *request_info_get(const s_request *req);
Eina_Bool nvim_rpc(s_nvim *nvim, e_request request_type, ...);

Eina_Bool api_init(void);
void api_shutdown(void);
e_api_key api_key_for_string_get(Eina_Stringshare *key);
Eina_Bool api_version_set(const msgpack_object *obj, s_version *version);

#endif /* ! __ENVIM_H__ */
