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

#define TABPAGE_MAGIC 0x3210edff
#define WINDOW_MAGIC  0x83291fea
#define BUFFER_MAGIC  0xeab732d0
#define NVIM_MAGIC    0xffe32412

#define MAGIC_CHECK(X, MAGIC, ...)                                             \
   do {                                                                        \
      if (! EINA_MAGIC_CHECK(X, MAGIC)) {                                      \
         EINA_MAGIC_FAIL(X, MAGIC);                                            \
         return __VA_ARGS__;                                                   \
      }                                                                        \
   } while (0)

#define MAGIC_FREE(X)                                                          \
   do {                                                                        \
      EINA_MAGIC_SET(X, EINA_MAGIC_NONE);                                      \
      free(X);                                                                 \
   } while (0)


#define TABPAGE_MAGIC_CHECK(X, ...) MAGIC_CHECK(X, TABPAGE_MAGIC, __VA_ARGS__)
#define WINDOW_MAGIC_CHECK(X, ...) MAGIC_CHECK(X, WINDOW_MAGIC, __VA_ARGS__)
#define BUFFER_MAGIC_CHECK(X, ...) MAGIC_CHECK(X, BUFFER_MAGIC, __VA_ARGS__)

typedef struct version s_version;
typedef struct request s_request;
typedef struct nvim s_nvim;
typedef struct position s_position;
typedef struct window s_window;
typedef struct tabpage s_tabpage;
typedef struct buffer s_buffer;
typedef struct object s_object;
typedef struct gui s_gui;
typedef int64_t t_int;
typedef void (*f_request_error)(const s_nvim *nvim, const s_request *req, void *data);

#define T_INT_INVALID ((t_int)-1)

#include "nvim_api.h"

struct object
{
   int64_t id;
};

struct request
{
   uint32_t uid;
   const void *then_callback;
   f_request_error error_callback;
   void *callback_data;
   e_request type;
};

struct gui
{
   Evas_Object *win;
   Evas_Object *layout;
};

#define NVIM_GUI_CONTAINER_GET(Gui) (s_nvim *)((Gui) - offsetof(s_nvim, gui))

struct nvim
{
   s_gui gui;

   Ecore_Exe *exe;
   Eina_List *requests;
   Eina_Hash *tabpages;
   Eina_Hash *windows;
   Eina_Hash *buffers;

   msgpack_unpacker unpacker;
   msgpack_sbuffer sbuffer;
   msgpack_packer packer;
   uint32_t request_id;

   EINA_MAGIC;
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
   EINA_INLIST;

   t_int id;
   s_tabpage *parent;
   Evas_Object *layout;
   Evas_Object *textgrid;

   EINA_MAGIC;
};

struct tabpage
{
   t_int id;
   Evas_Object *layout;
   Eina_Inlist *windows;
   EINA_MAGIC;
};

struct buffer
{
   EINA_INLIST;

   t_int id;
   EINA_MAGIC;
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


/*============================================================================*
 *                                  Main API                                  *
 *============================================================================*/

Eina_Bool main_in_tree_is(void);
const char *main_edje_file_get(void);


/*============================================================================*
 *                                   GUI API                                  *
 *============================================================================*/

Eina_Bool gui_init(void);
void gui_shutdown(void);
Eina_Bool gui_add(s_gui *gui);
void gui_del(s_gui *gui);
Evas_Object *gui_layout_item_add(const s_gui *gui, const char *group);
Evas_Object *gui_tabpage_add(s_gui *gui);
Eina_Bool gui_window_add(s_gui *gui, s_window *win);


/*============================================================================*
 *                                  Nvim API                                  *
 *============================================================================*/


Eina_Bool nvim_init(void);
void nvim_shutdown(void);
s_nvim *nvim_new(const char *program, unsigned int argc, const char *const argv[]);
void nvim_free(s_nvim *nvim);
uint32_t nvim_get_next_uid(s_nvim *nvim);
Eina_Bool nvim_api_response_dispatch(s_nvim *nvim, const s_request *req, const msgpack_object_array *args);
void nvim_reload_tabpages(s_nvim *nvim, Eina_List *tabpages);
void nvim_win_size_set(s_nvim *nvim, s_window *win, unsigned int width, unsigned int height);


/*============================================================================*
 *                                 Request API                                *
 *============================================================================*/

Eina_Bool request_init(void);
void request_shutdown(void);
s_request *request_new(uint32_t req_uid, e_request req_type, const void *then_cb, f_request_error error_cb, void *cb_data);
void request_free(s_request *req);


/*============================================================================*
 *                                 Tabpage API                                *
 *============================================================================*/

Eina_Bool tabpage_init(void);
void tabpage_shutdown(void);
s_tabpage *tabpage_new(t_int id, s_gui *gui);
void tabpage_free(s_tabpage *tab);
void tabpage_window_add(s_tabpage *tab, s_window *win);
void tabpage_window_del(s_tabpage *tab, s_window *win);


/*============================================================================*
 *                                 Window API                                 *
 *============================================================================*/

Eina_Bool window_init(void);
void window_shutdown(void);
s_window *window_new(t_int id, s_tabpage *parent, s_gui *gui);
void window_free(s_window *win);


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
t_int pack_object_get(const msgpack_object_array *args);
t_int pack_window_get(const msgpack_object_array *args);
t_int pack_buffer_get(const msgpack_object_array *args);
t_int pack_tabpage_get(const msgpack_object_array *args);
Eina_List *pack_tabpages_get(const msgpack_object_array *args);
Eina_List *pack_windows_get(const msgpack_object_array *args);
Eina_List *pack_buffers_get(const msgpack_object_array *args);
Eina_List *pack_strings_get(const msgpack_object_array *args);
void *pack_non_implemented_get(const msgpack_object_array *args);


#endif /* ! __ENVIM_H__ */
