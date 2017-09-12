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

#include "envim/types.h"

#include "nvim_api.h"

#include "envim/config.h"
#include "envim/mode.h"
#include "envim/gui.h"
#include "envim/termview.h"
#include "envim/log.h"
#include "envim/main.h"

struct request
{
   uint32_t uid;
   const void *then_callback;
   f_request_error error_callback;
   void *callback_data;
   e_request type;
};


struct nvim
{
   s_gui gui;
   s_config *config;

   Ecore_Exe *exe;
   Eina_List *requests;
   Eina_Hash *modes;

   struct {
      Eina_Stringshare *name;
      unsigned int index;
   } mode;

   msgpack_unpacker unpacker;
   msgpack_sbuffer sbuffer;
   msgpack_packer packer;
   uint32_t request_id;

   Eina_Bool mouse_enabled;
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



/*============================================================================*
 *                                  Nvim API                                  *
 *============================================================================*/


Eina_Bool nvim_init(void);
void nvim_shutdown(void);
s_nvim *nvim_new(const char *program, unsigned int argc, const char *const argv[]);
void nvim_free(s_nvim *nvim);
uint32_t nvim_get_next_uid(s_nvim *nvim);
Eina_Bool nvim_api_response_dispatch(s_nvim *nvim, const s_request *req, const msgpack_object_array *args);
Eina_Bool nvim_mode_add(s_nvim *nvim, s_mode *mode);
s_mode *nvim_named_mode_get(s_nvim *nvim, Eina_Stringshare *name);
void nvim_mode_set(s_nvim *nvim, Eina_Stringshare *name, unsigned int index);
void nvim_mouse_enabled_set(s_nvim *nvim, Eina_Bool enable);
Eina_Bool nvim_mouse_enabled_get(const s_nvim *nvim);


/*============================================================================*
 *                                 Request API                                *
 *============================================================================*/

Eina_Bool request_init(void);
void request_shutdown(void);
s_request *request_new(uint32_t req_uid, e_request req_type, const void *then_cb, f_request_error error_cb, void *cb_data);
void request_free(s_request *req);


/*============================================================================*
 *                                  Types API                                 *
 *============================================================================*/

Eina_Bool types_init(void);
void types_shutdown(void);

/*============================================================================*
 *                              Packing Functions                             *
 *============================================================================*/

void pack_object(msgpack_packer *pk, const Eina_Value *value);
void pack_non_implemented(msgpack_packer *pk, const void *obj);
void pack_boolean(msgpack_packer *pk, Eina_Bool boolean);
void pack_stringshare(msgpack_packer *pk, Eina_Stringshare *str);
void pack_position(msgpack_packer *pk, s_position pos);
void pack_list_of_strings(msgpack_packer *pk, const Eina_List *list_str);
void pack_dictionary(msgpack_packer *pk, const Eina_Hash *dict);

Eina_Bool pack_boolean_get(const msgpack_object_array *args);
s_position pack_position_get(const msgpack_object_array *args);
t_int pack_int_get(const msgpack_object_array *args);
Eina_Stringshare *pack_stringshare_get(const msgpack_object_array *args);
Eina_Value *pack_object_get(const msgpack_object_array *args);
Eina_List *pack_strings_get(const msgpack_object_array *args);
Eina_List *pack_array_get(const msgpack_object_array *args);
Eina_Value *pack_single_object_get(const msgpack_object *obj);
Eina_Stringshare *pack_single_stringshare_get(const msgpack_object *obj);
void *pack_non_implemented_get(const msgpack_object_array *args);

extern const Eina_Value_Type *ENVIM_VALUE_TYPE_BOOL;
extern const Eina_Value_Type *ENVIM_VALUE_TYPE_NESTED;


#endif /* ! __ENVIM_H__ */
