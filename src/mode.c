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

#include "eovim/mode.h"

static Eina_Stringshare *_shapes[] =
{
   [CURSOR_SHAPE_BLOCK] = "block",
   [CURSOR_SHAPE_HORIZONTAL] = "horizontal",
   [CURSOR_SHAPE_VERTICAL] = "vertical",
};

s_mode *
mode_new(Eina_Stringshare *name,
         const char *short_name,
         unsigned int short_size)
{
   s_mode *const mode = calloc(1, sizeof(s_mode) + short_size);
   if (EINA_UNLIKELY(! mode))
     {
        CRI("Failed to allocate memory");
        return NULL;
     }

   mode->name = eina_stringshare_ref(name);
   memcpy(mode->short_name, short_name, short_size);
   mode->short_name[short_size] = '\0';

   return mode;
}

void
mode_free(s_mode *mode)
{
   eina_stringshare_del(mode->name);
   free(mode);
}

Eina_Bool
mode_init(void)
{
   int i = 0;

   /* Create stringshares from the static strings. This allows much much
    * faster string comparison later on */
   for (; i < (int)EINA_C_ARRAY_LENGTH(_shapes); i++)
     {
       _shapes[i] = eina_stringshare_add(_shapes[i]);
       if (EINA_UNLIKELY(! _shapes[i]))
         {
            CRI("Failed to create stringshare");
            goto fail;
         }
     }
   return EINA_TRUE;

fail:
   for (--i; i >= 0; --i)
     eina_stringshare_del(_shapes[i]);
   return EINA_FALSE;
}

void
mode_shutdown(void)
{
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_shapes); i++)
     eina_stringshare_del(_shapes[i]);
}

e_cursor_shape
mode_cursor_shape_convert(Eina_Stringshare *shape_name)
{
   /* Try to match every possible shape. On match, return the index in the
    * table, which is the e_cursor_shape value. */
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_shapes); i++)
     if (shape_name == _shapes[i]) { return i; }

   /* Nothing found?! Fallback to block */
   ERR("Failed to find a cursor shape for '%s'", shape_name);
   return CURSOR_SHAPE_BLOCK;
}
