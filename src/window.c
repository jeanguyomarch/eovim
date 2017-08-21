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
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANwinILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "Envim.h"

static const char WINDOW_MAGIC_STR[] = "Window";

Eina_Bool
window_init(void)
{
   eina_magic_string_static_set(WINDOW_MAGIC, WINDOW_MAGIC_STR);
   return EINA_TRUE;
}

void
window_shutdown(void)
{
   /* Nothing to do */
}

s_window *
window_new(t_int id, s_tabpage *parent, s_gui *gui)
{
   s_window *const win = calloc(1, sizeof(s_window));
   if (EINA_UNLIKELY(! win))
     {
        CRI("Failed to allocate memory");
        return NULL;
     }

   win->id = id;
   win->parent = parent;
   if (EINA_UNLIKELY(! gui_window_add(gui, win)))
     {
        CRI("Failed to set the graphical window");
        goto fail;
     }

   EINA_MAGIC_SET(win, WINDOW_MAGIC);
   tabpage_window_add(parent, win);
   return win;

fail:
   free(win);
   return NULL;
}

void
window_free(s_window *win)
{
   if (win)
     {
        WINDOW_MAGIC_CHECK(win);
        MAGIC_FREE(win);
     }
}
