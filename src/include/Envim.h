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
#include "envim/request.h"
#include "envim/nvim.h"


/*============================================================================*
 *                              Logging Framework                             *
 *============================================================================*/



/*============================================================================*
 *                                  Nvim API                                  *
 *============================================================================*/



/*============================================================================*
 *                                 Request API                                *
 *============================================================================*/



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
