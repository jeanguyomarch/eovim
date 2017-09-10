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

#include "envim/mode.h"

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
