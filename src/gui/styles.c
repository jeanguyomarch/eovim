/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/log.h>
#include <eovim/gui.h>
#include <eovim/main.h>
#include <eovim/nvim_helper.h>
#include <eovim/nvim_api.h>
#include <eovim/nvim.h>

#include "gui_private.h"


static void _relayout(struct grid *const sd)
{
	/* This function sends the "relayout" smart callback, but only when all the
	 * required information are available!! This may not be the case at init
	 * time, because the font and dimensions are provided to eovim with
	 * asynchronous callbacks (hello RPC).
	 * At some key functions, we want to notify the gui that the lyout changed,
	 * but cannot completely yet, because we are missing information. In that
	 * case, we mark the relayout as "pending". When a new information is
	 * available, this will be called again.
	 */
	if (EINA_LIKELY(sd->cols && sd->rows && sd->style.font_name)) {
		Eina_Rectangle *const geo = &sd->geometry;
		evas_object_geometry_get(sd->textblock, &geo->x, &geo->y, NULL, NULL);

		/* Height is a bit tricky, because it depends on the textblock itself.  So
		 * you can't just take the height of a row and multiply it by the number of
		 * rows. There will be some pixel differences...
		 *
		 * We most the last cursor to the last character, to make sure that we
		 * completely get the last line. We then calculate the exact height
		 * from a union of geometries.
		 *
		 * This is costly, but rarely performed.
		 */
		evas_textblock_cursor_paragraph_char_last(sd->cursors[sd->rows - 1]);
		Eina_Iterator *const it = evas_textblock_cursor_range_simple_geometry_get(
			sd->cursors[0], sd->cursors[sd->rows - 1]);
		Eina_Rectangle frame = EINA_RECTANGLE_INIT;
		Eina_Rectangle *rect;
		EINA_ITERATOR_FOREACH (it, rect)
			eina_rectangle_union(&frame, rect);
		eina_iterator_free(it);

		geo->w = (int)(sd->cell_w * sd->cols);
		geo->h = frame.h;

		if (sd->may_send_relayout) {
			Evas_Object *const win = sd->nvim->gui.win;
			evas_object_resize(win, geo->x + geo->w, geo->y + geo->h);
		}
	}
}


static struct style *_style_new(void)
{
	return calloc(1, sizeof(struct style));
}

static void _style_free(struct style *const style)
{
	free(style);
}

static Eina_Bool _kind_style_foreach(const Eina_Hash *const hash EINA_UNUSED, const void *const key,
				     void *const data, void *const fdata)
{
	const char *const style = data;
	const unsigned int kind_id = gui_style_hash(key);
	struct grid *const sd = fdata;
	eina_strbuf_append_printf(sd->style.text, " kind_%u='+ %s'", kind_id, style);
	return EINA_TRUE;
}

static Eina_Bool _style_foreach(const Eina_Hash *const hash EINA_UNUSED, const void *const key,
				void *const data, void *const fdata)
{
	const struct style *const style = data;
	const int64_t style_id = *((const int64_t *)key);
	struct gui *const gui = fdata;
	Eina_Strbuf *const buf = gui->style.text;

	eina_strbuf_append_printf(buf, " X%" PRIx64 "='+", style_id);

	if (style->reverse) {
		const uint32_t fg = (style->bg_color.value == COLOR_DEFAULT) ?
					    gui->style.default_bg.value :
					    style->bg_color.value;
		const uint32_t bg = (style->fg_color.value == COLOR_DEFAULT) ?
					    gui->style.default_fg.value :
					    style->fg_color.value;
		eina_strbuf_append_printf(buf,
					  " color=#%06" PRIx32
					  " backing=on backing_color=#%06" PRIx32,
					  fg & 0xFFFFFF, bg & 0xFFFFFF);
	} else {
		if (style->fg_color.value != COLOR_DEFAULT) {
			eina_strbuf_append_printf(buf, " color=#%06" PRIx32,
						  style->fg_color.value & 0xFFFFFF);
		}
		if (style->bg_color.value != COLOR_DEFAULT) {
			eina_strbuf_append_printf(buf, " backing=on backing_color=#%06" PRIx32,
						  style->bg_color.value & 0xFFFFFF);
		}
	}
	if (style->italic) {
		eina_strbuf_append_printf(buf, " font_style=Italic");
	}
	if (style->bold) {
		eina_strbuf_append_printf(buf, " font_weight=Bold");
	}
	if (style->strikethrough) {
		eina_strbuf_append_printf(buf,
					  " strikethrough=on strikethrough_type=single"
					  " strikethrough_color=#%06" PRIx32,
					  style->fg_color.value & 0xFFFFFF);
	}

	const uint32_t sp = (style->sp_color.value == COLOR_DEFAULT) ? gui->style.default_sp.value :
								       style->sp_color.value;
	if (style->underline)
		eina_strbuf_append_printf(buf,
					  " underline=on"
					  " underline_color=#%06" PRIx32,
					  sp & 0xFFFFFF);
	else if (style->undercurl)
		eina_strbuf_append_printf(buf,
					  " underline=dashed underline_type=dashed"
					  " underline_dash_color=#%06" PRIx32
					  " underline_dash_width=4 underline_dash_gap=2",
					  sp & 0xFFFFFF);
	eina_strbuf_append_char(buf, '\'');

	return EINA_TRUE;
}


void gui_style_update(struct gui *const gui)
{
	Eina_Strbuf *const buf = gui->style.text;

	eina_strbuf_reset(buf);
	eina_strbuf_append_printf(buf, "DEFAULT='font=\\'%s\\' font_size=%u color=#%06x wrap=none",
				  gui->style.font_name, gui->style.font_size,
				  gui->style.default_fg.value & 0xFFFFFF);
	if (gui->style.line_gap != 0u) {
		eina_strbuf_append_printf(buf, " linegap=%u", gui->style.line_gap);
	}
	eina_strbuf_append_char(buf, '\'');

	eina_hash_foreach(gui->styles, &_style_foreach, gui);
	eina_hash_foreach(gui->nvim->kind_styles, &_kind_style_foreach, gui);

	//DBG("Style update: %s\n", eina_strbuf_string_get(buf));
	evas_textblock_style_set(gui->style.object, eina_strbuf_string_get(buf));

	/* The height of a "cell" may vary depending on the font, linegap, etc. */
	// TODO --> remove (callback)
	evas_textblock_cursor_line_geometry_get(gui->cursors[0], NULL, NULL, NULL,
						(int *)&sd->cell_h);

	// TODO
	gui_wildmenu_style_set(gui->wildmenu, gui->style.object, sd->cell_w, sd->cell_h);
	gui_completion_style_set(gui->completion, gui->style.object, sd->cell_w, sd->cell_h);

	if (sd->need_nvim_resize) {
		int w, h;
		evas_object_geometry_get(sd->textblock, NULL, NULL, &w, &h);
		const unsigned int cols = (unsigned)w / sd->cell_w;
		const unsigned int rows = (unsigned)h / sd->cell_h;
		if (cols && rows)
			nvim_api_ui_try_resize(sd->nvim, cols, rows);
	}

	_relayout(sd);

	sd->pending_style_update = EINA_FALSE;
	sd->need_nvim_resize = EINA_FALSE;
}

struct style *gui_style_get(struct gui *const gui, const t_int style_id)
{
	struct style *style = eina_hash_find(gui->style.styles, &style_id);
	if (style == NULL) {
		style = _style_new();
		if (EINA_UNLIKELY(!style))
			return NULL;
		const Eina_Bool added = eina_hash_add(gui->style.styles, &style_id, style);
		if (EINA_UNLIKELY(!added)) {
			ERR("Failed to add style to hash table");
			_style_free(style);
			return NULL;
		}
	}
	return style;
}
