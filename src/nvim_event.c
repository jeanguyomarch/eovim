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

void
nvim_event_resize(s_nvim *nvim,
                  t_int rows,
                  t_int columns)
{
   /*
    * Easy! We just have to resize the gui to make it fit the requested
    * amount of rows and columns.
    */
   gui_resize(&nvim->gui, rows, columns);
}

void
nvim_event_clear(s_nvim *nvim)
{
   /*
    * Easy! Clear the whole screen.
    */
   gui_clear(&nvim->gui);
}

void nvim_event_eol_clear(s_nvim *nvim) {}
void nvim_event_cursor_goto(s_nvim *nvim, t_int row, t_int col) {}
void nvim_event_mode_info_set(s_nvim *nvim, Eina_Bool enabled, Eina_List* cursor_styles) {}
void nvim_event_update_menu(s_nvim *nvim) {}
void nvim_event_busy_start(s_nvim *nvim) {}
void nvim_event_busy_stop(s_nvim *nvim) {}
void nvim_event_mouse_on(s_nvim *nvim) {}
void nvim_event_mouse_off(s_nvim *nvim) {}
void nvim_event_mode_change(s_nvim *nvim, Eina_Stringshare* mode, t_int mode_idx) {}
void nvim_event_set_scroll_region(s_nvim *nvim, t_int top, t_int bot, t_int left, t_int right) {}
void nvim_event_scroll(s_nvim *nvim, t_int count) {}
void nvim_event_highlight_set(s_nvim *nvim, Eina_Hash* attrs) {}
void nvim_event_put(s_nvim *nvim, Eina_Stringshare* str) {}
void nvim_event_bell(s_nvim *nvim) {}
void nvim_event_visual_bell(s_nvim *nvim) {}
void nvim_event_flush(s_nvim *nvim) {}

void
nvim_event_update_fg(s_nvim *nvim, t_int fg)
{
   WRN("fg: %"PRIi64, fg);
}
void nvim_event_update_bg(s_nvim *nvim, t_int bg) {}
void nvim_event_update_sp(s_nvim *nvim, t_int sp) {}
void nvim_event_suspend(s_nvim *nvim) {}
void nvim_event_set_title(s_nvim *nvim, Eina_Stringshare* title) {}
void nvim_event_set_icon(s_nvim *nvim, Eina_Stringshare* icon) {}
void nvim_event_popupmenu_show(s_nvim *nvim, Eina_List* items, t_int selected, t_int row, t_int col) {}
void nvim_event_popupmenu_hide(s_nvim *nvim) {}
void nvim_event_popupmenu_select(s_nvim *nvim, t_int selected) {}
void nvim_event_tabline_update(s_nvim *nvim, void* current, Eina_List* tabs) {}
