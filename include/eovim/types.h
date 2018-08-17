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
typedef struct prefs s_prefs;
typedef struct completion s_completion;
typedef struct geometry s_geometry;
typedef Eina_Bool (*f_event_cb)(s_nvim *nvim, const msgpack_object_array *args);

typedef enum
{
   CURSOR_SHAPE_BLOCK           = 0,
   CURSOR_SHAPE_HORIZONTAL      = 1,
   CURSOR_SHAPE_VERTICAL        = 2,
} e_cursor_shape;

struct completion
{
   Eina_Stringshare *word;
   Eina_Stringshare *kind;
   Eina_Stringshare *menu;
   Eina_Stringshare *info;
};

struct geometry
{
   unsigned int w;
   unsigned int h;
};

struct version
{
   unsigned int major;
   unsigned int minor;
   unsigned int patch;
   char extra[32];
};

struct mode
{
   Eina_Stringshare *name; /**< Name of the mode */
   e_cursor_shape cursor_shape; /**< Shape of the cursor */
   unsigned int cell_percentage;
      /**< Percentage of the cell that the cursor occupies */
   unsigned int blinkon; /**< Delay during which the cursor is displayed */
   unsigned int blinkoff; /**< Delay during which the cursor is hidden */
   unsigned int blinkwait; /**< Delay for transitionning ON <-> OFF */
   unsigned int hl_id;
   unsigned int id_lm;
   char short_name[1];
     /**< Short name... I don't remember what this is... but it is a string
      * embedded at the end of this struct (not just a single char) */
};

#endif /* ! __EOVIM_TYPES_H__ */
