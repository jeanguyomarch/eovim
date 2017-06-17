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

typedef struct version s_version;
typedef struct request s_request;
typedef struct nvim s_nvim;
typedef struct position s_position;
typedef struct window s_window;
typedef struct tabpage s_tabpage;
typedef struct buffer s_buffer;
typedef struct object s_object;
typedef int64_t t_int;

#include "nvim_api.h"

typedef enum
{
   PACK_EXT_BUFFER      = 0,
   PACK_EXT_WINDOW      = 1,
   PACK_EXT_TABPAGE     = 2,
} e_pack_ext;

struct object
{
   int64_t id;
};

struct request
{
   uint64_t uid;
   e_request type;
   const void *then_callback;
};

struct nvim
{
   Ecore_Exe *exe;
   Evas_Object *win;
   Eina_List *requests;
   Eina_Hash *tabpages;
   Eina_Hash *windows;
   Eina_Hash *buffers;

   msgpack_zone mempool;
   msgpack_sbuffer sbuffer;
   msgpack_packer packer;
   uint64_t request_id;
};

struct position
{
   int64_t x;
   int64_t y;
};

struct version
{
   unsigned int major;
   unsigned int minor;
   unsigned int patch;
};

struct window
{
   s_object obj;
};

struct tabpage
{
   s_object obj;
};

struct buffer
{
   s_object obj;
};


static inline s_position
position_make(int64_t x, int64_t y)
{
   const s_position pos = {
      .x = x,
      .y = y,
   };
   return pos;
}

/*============================================================================*
 *                              Logging Framework                             *
 *============================================================================*/

extern int _envim_log_domain;

#define DBG(...) EINA_LOG_DOM_DBG(_envim_log_domain, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_envim_log_domain, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_envim_log_domain, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_envim_log_domain, __VA_ARGS__)
#define CRI(...) EINA_LOG_DOM_CRIT(_envim_log_domain, __VA_ARGS__)

Eina_Bool nvim_init(void);
void nvim_shutdown(void);
s_nvim *nvim_new(const char *program, unsigned int argc, const char *const argv[]);
void nvim_free(s_nvim *nvim);
uint64_t nvim_get_next_uid(s_nvim *nvim);

s_request *request_new(uint64_t req_uid, e_request req_type, const void *then_cb);
void request_free(s_request *req);

Eina_Bool nvim_api_response_dispatch(s_nvim *nvim, const s_request *req, const msgpack_object_array *args);


/*============================================================================*
 *                              Packing Functions                             *
 *============================================================================*/

void pack_object(msgpack_packer *pk, const s_object *obj);
void pack_buffer(msgpack_packer *pk, const s_buffer *buf);
void pack_window(msgpack_packer *pk, const s_window *win);
void pack_tabpage(msgpack_packer *pk, const s_tabpage *tab);
void pack_non_implemented(msgpack_packer *pk, const void *obj);
void pack_boolean(msgpack_packer *pk, Eina_Bool boolean);
void pack_stringshare(msgpack_packer *pk, Eina_Stringshare *str);
void pack_position(msgpack_packer *pk, s_position pos);
void pack_list_of_windows(msgpack_packer *pk, const Eina_List *list_win);
void pack_list_of_buffers(msgpack_packer *pk, const Eina_List *list_buf);
void pack_list_of_tabpages(msgpack_packer *pk, const Eina_List *list_tab);
void pack_list_of_strings(msgpack_packer *pk, const Eina_List *list_str);

Eina_Bool pack_boolean_get(const msgpack_object_array *args);
s_position pack_position_get(const msgpack_object_array *args);
t_int pack_int_get(const msgpack_object_array *args);
Eina_Stringshare *pack_stringshare_get(const msgpack_object_array *args);
s_object *pack_object_get(const msgpack_object_array *args);
s_window *pack_window_get(const msgpack_object_array *args);
s_buffer *pack_buffer_get(const msgpack_object_array *args);
s_tabpage *pack_tabpage_get(const msgpack_object_array *args);
Eina_List *pack_tabpages_get(const msgpack_object_array *args);


void nvim_list_tabpages_cb(s_nvim *nvim, Eina_List *tabpages);

#endif /* ! __ENVIM_H__ */
