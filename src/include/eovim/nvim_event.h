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

#ifndef __EOVIM_EVENT_H__
#define __EOVIM_EVENT_H__

#include "eovim/types.h"
#include <msgpack.h>

Eina_Bool nvim_event_resize(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_clear(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_eol_clear(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cursor_goto(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_mode_info_set(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_update_menu(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_busy_start(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_busy_stop(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_mouse_on(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_mouse_off(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_mode_change(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_set_scroll_region(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_scroll(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_highlight_set(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_put(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_bell(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_visual_bell(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_update_fg(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_update_bg(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_update_sp(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_suspend(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_set_title(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_set_icon(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_popupmenu_show(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_popupmenu_hide(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_popupmenu_select(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_tabline_update(s_nvim *nvim, const msgpack_object_array *args);

Eina_Bool nvim_event_dispatch(s_nvim *nvim, Eina_Stringshare *command, const msgpack_object_array *args);
Eina_Bool nvim_event_init(void);
void nvim_event_shutdown(void);

#endif /* ! __EOVIM_EVENT_H__ */
