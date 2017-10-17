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
                    void *data,
                    const msgpack_object *result)
{
   if (EINA_UNLIKELY(result->type != MSGPACK_OBJECT_STR))
     {
        ERR("A string is expected. Got type 0%x", result->type);
        return;
     }

   /* We will pass this data to the callback */
   s_hl_group hl_group;
   memset(&hl_group, 0, sizeof(hl_group));

   /* At this point, we have received a string, that starts with a \n. I don't
    * know, that's just the way it is. I don't want it, so I exclude it for
    * later processing.
    * There are two possible size values:
    *   - 1 (if no background color was set)
    *   - 8 (if there is a background color)
    */
   const msgpack_object_str *const obj = &(result->via.str);
   if (obj->size == 1)
     {
        /* Do nothing, stop here */
        return;
     }
   else if (obj->size == 8)
     {
        sscanf(obj->ptr, "\n#%02hhx%02hhx%02hhx",
               &hl_group.bg.r, &hl_group.bg.g, &hl_group.bg.b);
        hl_group.bg.used = EINA_TRUE;
     }
   else
     {
        ERR("The message's size: %u, is inexpected", obj->size);
        return;
     }

   /* Send the hl group to the callback function */
   const f_highlight_group_decode func = (const f_highlight_group_decode)(data);
   func(nvim, &hl_group);
}

void
nvim_helper_highlight_group_decode(s_nvim *nvim,
                                   unsigned int group,
                                   f_highlight_group_decode func)
{
   EINA_SAFETY_ON_NULL_RETURN(func);

   /* The result will yield a string containing the background color as an
    * hexadecimal value (e.g. #ffaa00) */
   char vim_cmd[512];
   const int bytes = snprintf(
      vim_cmd, sizeof(vim_cmd),
      ":silent! echo synIDattr(%u,\"bg#\")", group
   );
   nvim_api_command_output(nvim, vim_cmd, (unsigned int)bytes,
                           _hl_group_color_get, func);
}
}
