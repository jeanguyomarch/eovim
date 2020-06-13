/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_TERMVIEW_H__
#define __EOVIM_TERMVIEW_H__

#include "eovim/types.h"
#include <Evas.h>

struct termview_style {
	union color fg_color;
	union color bg_color;
	union color sp_color;
	Eina_Bool reverse;
	Eina_Bool italic;
	Eina_Bool bold;
	Eina_Bool underline;
	Eina_Bool undercurl;
	Eina_Bool strikethrough;
};

Eina_Bool termview_init(void);
void termview_shutdown(void);
Evas_Object *termview_add(Evas_Object *parent, struct nvim *nvim);
void termview_matrix_set(Evas_Object *obj, unsigned int cols, unsigned int rows);
void termview_cell_size_get(const Evas_Object *obj, unsigned int *w, unsigned int *h);
void termview_size_get(const Evas_Object *obj, unsigned int *cols, unsigned int *rows);
void termview_clear(Evas_Object *obj);
void termview_cursor_goto(Evas_Object *obj, unsigned int to_x, unsigned int to_y);
void termview_cell_geometry_get(const Evas_Object *obj, unsigned int cell_x, unsigned int cell_y,
				int *px, int *py, int *pw, int *ph);

void termview_cursor_mode_set(Evas_Object *obj, const struct mode *mode);
void termview_cursor_visibility_set(Evas_Object *obj, Eina_Bool visible);

struct termview_style *termview_style_get(Evas_Object *obj, t_int style_id);

void termview_scroll(Evas_Object *obj, int top, int bot, int left, int right, int rows);

void termview_default_colors_set(Evas_Object *obj, union color fg, union color bg, union color sp);

void termview_font_set(Evas_Object *obj, Eina_Stringshare *font_name, unsigned int font_size);

void termview_line_edit(Evas_Object *obj, unsigned int row, unsigned int col, const char *text,
			size_t text_len, t_int style_id, size_t repeat);

void termview_flush(Evas_Object *obj);
void termview_linespace_set(Evas_Object *obj, unsigned int linespace);
void termview_redraw_end(Evas_Object *obj);

#endif /* ! __EOVIM_TERMVIEW_H__ */
