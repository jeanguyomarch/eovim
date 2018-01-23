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

   /* At this point, we have received a string, that starts with a \n for some
    * neovim version. I don't know, that's just the way it is. I don't want it,
    * so I exclude it for later processing.
    * There are different possible size values:
    *   - 1 (if no background color was set, and there is a leading \n)
    *   - 0 (if no background color was set, and there is no leading \n)
    *   - 8 (if there is a background color and a leading \n)
    *   - 7 (if there is a background color and no leading \n)
    */
   const msgpack_object_str *const obj = &(result->via.str);
   const char *msg = obj->ptr;
   switch (obj->size)
     {
      case 1:
         if (obj->ptr[0] != '\n') goto error;
         /* Fall through */
      case 0:
         /* Do nothing, stop here */
         return;

         /* In some versions of Neovim, an additional initial \n may be passed
          * in the response. In such case, strip it. */
      case 8:
         if (obj->ptr[0] != '\n') goto error;
         else msg = &(obj->ptr[1]);
         /* Fall through */
      case 7:
         sscanf(msg, "#%02hhx%02hhx%02hhx",
                &hl_group.bg.r, &hl_group.bg.g, &hl_group.bg.b);
         hl_group.bg.used = EINA_TRUE;
         break;

error:default:
        ERR("The message's size: %u, is inexpected. Hexdump below:", obj->size);
        for (unsigned int i = 0; i < obj->size; i++)
           fprintf(stderr, " %02x", obj->ptr[i]);
        fprintf(stderr, "\n");
        return;
     }

   /* Send the hl group to the callback function */
   const f_highlight_group_decode func = (const f_highlight_group_decode)(data);
   func(nvim, &hl_group);
}


void
nvim_helper_highlight_group_decode_noop(s_nvim *nvim EINA_UNUSED,
                                        unsigned int group EINA_UNUSED,
                                        f_highlight_group_decode func EINA_UNUSED)
{
   /* Not supported by Neovim, so nothing to do */
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

static void
_version_decode(s_nvim *nvim,
                void *data,
                const msgpack_object *result)
{
   /* Make sure we got a string object from Neovim */
   if (EINA_UNLIKELY(result->type != MSGPACK_OBJECT_STR))
     {
        ERR("A string is expected. Got type 0%x", result->type);
        return;
     }
   const msgpack_object_str *const str = &(result->via.str);

   s_version version;
   memset(&version, 0, sizeof(version));

   /* Yes, this is evil, but I really don't want to crash later because the
    * string is not NUL-terminated */
   ((char *)(str->ptr))[str->size - 1] = '\0';

   /* A neovim version starts with 'NVIM vX.Y.Z[-patch]' I want to find where
    * 'NVIM v' is, so I can attack X.Y.Z[-patch] */
   const char start[] = "NVIM v";
   const char *const ptr = strstr(str->ptr, start);
   const char *const v = ptr + sizeof(start) - 1;

   int bytes;
   const int ret =
      sscanf(v, "%u.%u.%u%n",
             &version.major, &version.minor, &version.patch, &bytes);
   if (EINA_UNLIKELY(ret != 3))
     {
        ERR("Failed to successfully parse version. Results may be unexpected.");
        /* We went full retard, but we can keep going */
     }
   else
     {
        /* Parsing successful. Yay! Find out the extra version if present.
         * It is if the next character to be read afer maj.min.patch is '-'. */
        if (v[bytes] == '-')
          sscanf(&v[bytes + 1], "%32s", version.extra);
     }

   /* Send the hl group to the callback function */
   const f_version_decode func = (const f_version_decode)(data);
   func(nvim, &version);
}

void
nvim_helper_version_decode(s_nvim *nvim,
                           f_version_decode func)
{
   EINA_SAFETY_ON_NULL_RETURN(func);

   const char vim_cmd[] = ":silent! version";
   nvim_api_command_output(nvim, vim_cmd, sizeof(vim_cmd) - 1,
                           _version_decode, func);
}

void
nvim_helper_autocmd_do(s_nvim *nvim,
                       const char *event)
{
   /* Compose the vim command that will trigger an autocmd for the User event,
    * with our custom args */
   char buf[512];
   const int bytes = snprintf(buf, sizeof(buf), ":doautocmd User %s", event);
   if (EINA_UNLIKELY(bytes < 0))
     {
        CRI("Failed to write data in stack buffer");
        return;
     }

   /* Make neovim execute this */
   const Eina_Bool ok = nvim_api_command(nvim, buf, (unsigned int)bytes);
   if (EINA_UNLIKELY(! ok))
     ERR("Failed to execute autocmd via: '%s'", buf);

}
