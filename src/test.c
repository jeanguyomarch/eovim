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

#include "eovim/test.h"
#include "eovim/nvim.h"
#include "eovim/log.h"
#include <Ecore.h>

#ifdef EOVIM_TEST_MODE
static Ecore_Event_Handler *_sigusr = NULL;
static Eina_List *_nvim_instances = NULL;

static Eina_Bool
_sigusr_cb(void *data EINA_UNUSED,
           int type EINA_UNUSED,
           void *info EINA_UNUSED)
{
   s_nvim *nvim;
   Eina_List *list_it;
   EINA_LIST_FOREACH(_nvim_instances, list_it, nvim)
     {
        termview_textgrid_dump(nvim->gui.termview, stdout);
        fflush(stdout);
     }

   return ECORE_CALLBACK_PASS_ON;
}
#endif

Eina_Bool
test_init(void)
{
#ifdef EOVIM_TEST_MODE
   _sigusr = ecore_event_handler_add(ECORE_EVENT_SIGNAL_USER, _sigusr_cb, NULL);
   if (EINA_UNLIKELY(! _sigusr))
     {
        CRI("Failed to create signal handler for SIGUSRx");
        return EINA_FALSE;
     }
#endif

   return EINA_TRUE;
}

void
test_shutdown(void)
{
#ifdef EOVIM_TEST_MODE
   ecore_event_handler_del(_sigusr);
#endif
}

void
test_nvim_register(const s_nvim *nvim)
{
#ifdef EOVIM_TEST_MODE
   _nvim_instances = eina_list_append(_nvim_instances, nvim);
#else
   (void) nvim;
#endif
}

void
test_nvim_unregister(const s_nvim *nvim)
{
#ifdef EOVIM_TEST_MODE
   _nvim_instances = eina_list_remove(_nvim_instances, nvim);
#else
   (void) nvim;
#endif
}
