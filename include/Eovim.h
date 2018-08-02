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

#ifndef EOVIM_PUBLIC_PLUGINS_API___EOVIM_H__
#define EOVIM_PUBLIC_PLUGINS_API___EOVIM_H__

#include <eovim/types.h>
#include <eovim/version.h>
#include <eovim/msgpack_helper.h>

/* EFL and msgpack are mandatory */
#include <Eina.h>
#include <Evas.h>
#include <msgpack.h>

#ifndef EAPI
# undef EAPI
#endif
#ifdef _WIN32
# define EAPI __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define EAPI __attribute__ ((visibility("default")))
#else
# define EAPI extern
#endif

/**
 * Retrieve the Window handler from a Neovim instance.
 *
 * @param[in] nvim A pointer to the neovim instance. May not be NULL.
 * @return The Window handler associated to @p nvim.
 */
EAPI Evas_Object *eovim_window_get(s_nvim *nvim)
   EINA_ARG_NONNULL(1) EINA_WARN_UNUSED_RESULT EINA_PURE;

/**
 * Retrieve the Termview handler from a Neovim instance. The termview in Eovim
 * is the widget that displays a text zone.
 *
 * @param[in] nvim A pointer to the neovim instance. May not be NULL.
 * @return The Termview handler associated to @p nvim.
 */
EAPI Evas_Object *eovim_termview_get(s_nvim *nvim)
   EINA_ARG_NONNULL(1) EINA_WARN_UNUSED_RESULT EINA_PURE;

/**
 * @def EOVIM_PLUGIN_SYMBOL(Sym)
 *
 * Exports the parameter @p Sym as being the symbol to be called when the
 * plugin is triggered during the execution of Neovim. This symbol MUST
 * have a signature of type #f_event_cb.
 */
#define EOVIM_PLUGIN_SYMBOL(Sym) \
   EXPORTAPI const f_event_cb __eovim_plugin_symbol = &(Sym)

#endif /* ! EOVIM_PUBLIC_PLUGINS_API___EOVIM_H__ */
