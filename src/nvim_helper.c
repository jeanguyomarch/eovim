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

#include "eovim/nvim.h"
#include "eovim/nvim_helper.h"
#include "eovim/nvim_api.h"
#include "eovim/log.h"
#include <msgpack.h>


static void
_hl_group_color_get(s_nvim *nvim,
                    void *data EINA_UNUSED,
                    const msgpack_object *result)
{
   /*
    * We expect data of the form:
    *  [ String, String ]
    * where each String is written: as "#......" (24-bits hexadecimal colors)
    */
   if (EINA_UNLIKELY(result->type != MSGPACK_OBJECT_ARRAY))
     {
        ERR("An array is expected. Got type 0%x", result->type);
        return;
     }
   const msgpack_object_array *const arr = &(result->via.array);
   if (EINA_UNLIKELY(arr->size != 2))
     {
        ERR("An array of two elements is expected. Got %u elements",
            arr->size);
        return;
     }
   const msgpack_object *const fg_obj = &(arr->ptr[0]);
   const msgpack_object *const bg_obj = &(arr->ptr[1]);
   if (EINA_UNLIKELY((fg_obj->type != MSGPACK_OBJECT_STR) ||
                     (bg_obj->type != MSGPACK_OBJECT_STR)))
     {
        ERR("Elements should be both of type string");
        return;
     }
   const msgpack_object_str *const fg = &(fg_obj->via.str);
   const msgpack_object_str *const bg = &(bg_obj->via.str);

   /* Check that they both are of the expected size */
   const unsigned int expected_size = sizeof("#123456") - 1;
   if (EINA_UNLIKELY((fg->size != expected_size) ||
                     (bg->size != expected_size)))
     {
        ERR("Elements should both have a size of %u", expected_size);
        return;
     }

   /* Parse each string of type "#......" (e.g. #ff2893) */
   s_hl_group hl_group;
   sscanf(fg->ptr, "#%02hhx%02hhx%02hhx",
          &hl_group.fg.r, &hl_group.fg.g, &hl_group.fg.b);
   sscanf(bg->ptr, "#%02hhx%02hhx%02hhx",
          &hl_group.bg.r, &hl_group.bg.g, &hl_group.bg.b);

   const f_highlight_group_decode func = (const f_highlight_group_decode)(data);
   func(nvim, &hl_group);
}

void
nvim_helper_highlight_group_decode(s_nvim *nvim,
                                   unsigned int group,
                                   f_highlight_group_decode func)
{
   EINA_SAFETY_ON_NULL_RETURN(func);

   /* The result will yield a list of two strings containing the colors
    * of the foreground and background of the given highlight group */
   char vim_cmd[512];
   const int bytes = snprintf(
      vim_cmd, sizeof(vim_cmd),
      "[synIDattr(%u,\"fg#\"),synIDattr(%u,\"bg#\")]",
      group, group
   );
   nvim_api_eval(nvim, vim_cmd, (unsigned int)bytes,
                 _hl_group_color_get, func);
}
