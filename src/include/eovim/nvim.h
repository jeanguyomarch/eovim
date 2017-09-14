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

#ifndef __EOVIM_NVIM_H__
#define __EOVIM_NVIM_H__

#include "eovim/types.h"
#include "eovim/gui.h"

#include <Eina.h>
#include <Ecore.h>
#include <msgpack.h>

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


Eina_Bool nvim_init(void);
void nvim_shutdown(void);
s_nvim *nvim_new(const char *program, unsigned int argc, const char *const argv[]);
void nvim_free(s_nvim *nvim);
uint32_t nvim_next_uid_get(s_nvim *nvim);
Eina_Bool nvim_api_response_dispatch(s_nvim *nvim, const s_request *req, const msgpack_object_array *args);
Eina_Bool nvim_mode_add(s_nvim *nvim, s_mode *mode);
s_mode *nvim_named_mode_get(const s_nvim *nvim, Eina_Stringshare *name);
void nvim_mode_set(s_nvim *nvim, Eina_Stringshare *name, unsigned int index);
void nvim_mouse_enabled_set(s_nvim *nvim, Eina_Bool enable);
Eina_Bool nvim_mouse_enabled_get(const s_nvim *nvim);

#endif /* ! __EOVIM_NVIM_H__ */
