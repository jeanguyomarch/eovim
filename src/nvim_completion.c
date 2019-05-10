/*
 * Copyright (c) 2019 Jean Guyomarc'h
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

#include <eovim/nvim_completion.h>

s_completion *
nvim_completion_new(const char *word, size_t word_size,
                    const char *kind, size_t kind_size,
                    const char *menu, size_t menu_size,
                    const char *info, size_t info_size)
{
   /*
    * word_ptr + kind_ptr + menu_ptr + info_ptr +
    * [word + \0] + [kind + \0] + [menu + \0] + [info] + \0]
    */
   const size_t buf_size =
     sizeof(s_completion) + word_size + kind_size + menu_size + info_size + 3u;

   s_completion *const compl = malloc(buf_size);
   char *buf = compl->mem;

   compl->word = buf;
   memcpy(buf, word, word_size);
   buf[word_size] = '\0';
   buf += (word_size + 2u);

   compl->kind = buf;
   memcpy(buf, kind, kind_size);
   buf[kind_size] = '\0';
   buf += (kind_size + 2u);

   compl->menu = buf;
   memcpy(buf, menu, menu_size);
   buf[menu_size] = '\0';
   buf += (menu_size + 2u);

   compl->info = buf;
   memcpy(buf, info, info_size);
   buf[info_size] = '\0';

   return compl;
}

void
nvim_completion_free(s_completion *compl)
{
   free(compl);
}
