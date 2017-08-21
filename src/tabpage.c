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

#include "Envim.h"

static const char TABPAGE_MAGIC_STR[] = "Tabpage";

Eina_Bool
tabpage_init(void)
{
   eina_magic_string_static_set(TABPAGE_MAGIC, TABPAGE_MAGIC_STR);
   return EINA_TRUE;
}

void
tabpage_shutdown(void)
{
   /* Nothing to do */
}

s_tabpage *
tabpage_new(t_int id,
            s_gui *gui)
{
   s_tabpage *const tab = calloc(1, sizeof(s_tabpage));
   if (EINA_UNLIKELY(! tab))
     {
        CRI("Failed to allocate memory");
        return NULL;
     }

   tab->layout = gui_tabpage_add(gui);
   if (EINA_UNLIKELY(! tab->layout)) goto fail;
   tab->id = id;
   EINA_MAGIC_SET(tab, TABPAGE_MAGIC);
   return tab;

fail:
   free(tab);
   return NULL;
}

void
tabpage_free(s_tabpage *tab)
{
   if (tab)
     {
        TABPAGE_MAGIC_CHECK(tab);
        MAGIC_FREE(tab);
     }
}

void
tabpage_window_add(s_tabpage *tab,
                   s_window *win)
{
   TABPAGE_MAGIC_CHECK(tab);
   WINDOW_MAGIC_CHECK(win);

   tab->windows = eina_inlist_append(tab->windows, EINA_INLIST_GET(win));
}

void
tabpage_window_del(s_tabpage *tab,
                   s_window *win)
{
   TABPAGE_MAGIC_CHECK(tab);
   WINDOW_MAGIC_CHECK(win);

}
