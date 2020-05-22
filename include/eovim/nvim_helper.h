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

#ifndef __EOVIM_NVIM_HELPER_H__
#define __EOVIM_NVIM_HELPER_H__

#include "eovim/types.h"

typedef struct
{
   struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
      Eina_Bool used;
   } bg, fg;
} s_hl_group;

typedef void (*f_highlight_group_decode)(s_nvim *nvim, const s_hl_group *hl_group);

void
nvim_helper_highlight_group_decode(s_nvim *nvim,
                                   unsigned int group,
                                   f_highlight_group_decode func);

void
nvim_helper_highlight_group_decode_noop(s_nvim *nvim,
                                        unsigned int group,
                                        f_highlight_group_decode func);


Eina_Bool nvim_helper_autocmd_do(s_nvim *nvim, const char *event, f_nvim_api_cb func, void *func_data);

#endif /* ! __EOVIM_NVIM_HELPER_H__ */
