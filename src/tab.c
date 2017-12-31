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

#include "eovim/types.h"
#include "eovim/log.h"

struct tab
{
   EINA_INLIST;
   Eina_Stringshare *name;
   unsigned int id;
};


s_tab *
tab_new(Eina_Stringshare *name,
        unsigned int id)
{
   s_tab *const tab = malloc(sizeof(s_tab));
   if (EINA_UNLIKELY(! tab))
     {
        CRI("Failed to allocate memory for tab object");
        return NULL;
     }

   tab->name = name;
   tab->id = id;

   return tab;
}

void
tab_chain_add(s_tab *tab,
              s_tab **parent)
{
///   tab->chain = *parent;
   *parent = tab;
}

void
tab_free(s_tab *tab)
{
   EINA_SAFETY_ON_NULL_RETURN(tab);
   eina_stringshare_del(tab->name);
   free(tab);
}

const s_tab *
tab_find(const s_tab *list,
         Eina_Stringshare *name,
         unsigned int id)
{
   while (list)
     {
        if ((list->name == name) && (list->id == id))
          return list;
        list = list->chain;
     }
   return NULL;
}
