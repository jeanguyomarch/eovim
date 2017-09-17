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

#ifndef __EOVIM_TYPES_H__
#define __EOVIM_TYPES_H__

#include <Eina.h>
#include <msgpack.h>
#include <stdint.h>

typedef int64_t t_int;
typedef struct version s_version;
typedef struct request s_request;
typedef struct nvim s_nvim;
typedef struct config s_config;
typedef struct mode s_mode;
typedef struct position s_position;
typedef struct gui s_gui;
typedef struct completion s_completion;
typedef Eina_Bool (*f_event_cb)(s_nvim *nvim, const msgpack_object_array *args);

typedef enum
{
   CURSOR_SHAPE_BLOCK,
   CURSOR_SHAPE_HORIZONTAL,
   CURSOR_SHAPE_VERTICAL,
} e_cursor_shape;

struct completion
{
   Eina_Stringshare *word;
   Eina_Stringshare *kind;
   Eina_Stringshare *menu;
   Eina_Stringshare *info;
};

struct version
{
   unsigned int major;
   unsigned int minor;
   unsigned int patch;
};

Eina_Bool types_init(void);
void types_shutdown(void);

#endif /* ! __EOVIM_TYPES_H__ */