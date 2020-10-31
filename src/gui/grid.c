/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/log.h>
#include <eovim/gui.h>
#include <eovim/main.h>
#include <eovim/nvim_helper.h>
#include <eovim/nvim_api.h>
#include <eovim/nvim.h>

#include "gui_private.h"

#include <Edje.h>

/* This is the invisible separator. A zero-width space character that
 * allows to split ligatures without changing underlying VISUAL REPRESENTATION
 * of the text.
 *
 * It is the unicode U+2063 (http://www.unicode-symbol.com/u/2063.html) that is
 * preferred, as this is the "invisible separator".
 * Note however that EFL before 1.24 have a bug (?) that causes the textblock
 * rendering to be completely broken when this character is encountered.
 * Surprisingly, it does not complain for U+2065 (http://www.unicode-symbol.com/u/2065.html),
 * which is an invalid codepoint! However, this makes it behave exactly as EFL >= 1.24,
 * so we will go for that...
 */
#ifdef EFL_VERSION_1_24
static const char INVISIBLE_SEP[] = "\xe2\x81\xa3";
#else
static const char INVISIBLE_SEP[] = "\xe2\x81\xa5";
#endif

struct cell {
	char utf8[8]; /* NOT NUL-terminated */
	uint32_t bytes;
	uint32_t style_id;
};

struct grid {
	Evas_Object *layout;

	struct nvim *nvim;
	Evas_Object *textblock;
	Eina_Strbuf *line;
	struct cell **cells;
	Evas_Textblock_Cursor **cursors;
	Evas_Textblock_Cursor *tmp;

	/* This per-row set of booleans is used to control which line has been
	 * modified and needs to be re-rendered in the textblock.
	 *
	 * XXX It could/should be a bitmap to reduce space usage? XXX
	 * XXX It could also contain information about columns??
	 */
	Eina_Bool *line_has_changed;

	/* This textgrid exists to determine very easily the size of the a cell
	 * after a font change. Otherwise, we have to go through a callback hell
	 * to TRY to determine the line geometry of a textblock. I didn't manage
	 * to get a nice result... */
	Evas_Object *sizing_textgrid;

	struct {
		Evas_Textblock_Cursor *cur;
		unsigned int x;
		unsigned int y;

		unsigned int next_x;
		unsigned int next_y;

		/* This is set to true when the cursor has written a invisible
		 * space. It should be at (x,y) */
		Eina_Bool sep_written;
	} cursor;

	unsigned int cell_w;
	unsigned int cell_h;
	unsigned int rows;
	unsigned int cols;

	struct {
		/* When mouse drag starts, we store in here the button that was pressed
		 * when dragging was initiated. Since there is no button 0, we use 0 as a
		 * value telling that there is no dragging */
		int btn;
		unsigned int prev_cx; /**< Previous X position */
		unsigned int prev_cy; /**< Previous Y position */
	} mouse_drag;

	Eina_Rectangle geometry;
	Eina_Bool mode_changed;

	/***************************************************************************
	 * The resize...
	 *
	 * That's something I found surprisingly very difficult to handle properly.
	 * The problem is that resize can arise from two different event sources
	 *  1) a style change (i.e. font) must cause the window to fit the grid
	 *  2) the user resizes the window
	 *
	 * So, we must handle with the same "_smart_resize":
	 * a - When the window is resized, we want the textblock to fit the entire
	 * space; so a window resize must always resize the textblock.
	 * b - when the window is resized by the user nvim_api_ui_try_resize() is
	 * to be called, to change the dimension of neovim.
	 * c - when the style change, we request a window resize
	 *
	 * This may cause loops. For example, when the user resizes the window,
	 * we request a dimension change in neovim. This calls grid_matrix_set()
	 * and a call to _relayout(). Relayout changes this window size...
	 *
	 * The EFL do not provide (to the best of my knowledge) means to detect
	 * a "resize,start" and "resize,end" event. There is just "resize".
	 * So, the idea is to detect when we are processing a resize (neovim)
	 * or not. Hence the counter in_resize. When it reaches zero, it means
	 * that a relayout may occur.
	 */
	int in_resize;
	Eina_Bool may_send_relayout;
};

static void _coords_to_cell(const struct grid *sd, int px, int py, unsigned int *cell_x,
			    unsigned int *cell_y)
{
	int ox, oy; /* Textblock origin */
	int ow, oh; /* Textblock size */

	evas_object_geometry_get(sd->textblock, &ox, &oy, &ow, &oh);

	/* Clamp cell_x in [0 ; cols[ */
	if (px < ox) {
		*cell_x = 0;
	} else if (px - ox >= ow) {
		*cell_x = sd->cols - 1;
	} else {
		*cell_x = (unsigned int)((px - ox) / (int)sd->cell_w);
	}

	/* Clamp cell_y in [0 ; rows[ */
	if (py < oy) {
		*cell_y = 0;
	} else if (py - oy >= oh) {
		*cell_y = sd->rows - 1;
	} else {
		*cell_y = (unsigned int)((py - oy) / (int)sd->cell_h);
	}
}

static const char *_mouse_button_to_string(int button)
{
	switch (button) {
	case 3:
		return "Right";
	case 2:
		return "Middle";
	case 1: /* Fall through */
	default:
		return "Left";
	}
}

static void _mouse_event(struct grid *sd, const char *event, unsigned int cx, unsigned int cy,
			 int btn)
{
	char input[64];

	/* If mouse is NOT enabled, we don't handle mouse events */
	if (!nvim_mouse_enabled_get(sd->nvim)) {
		return;
	}

	/* Determine which button we pressed */
	const char *const button = _mouse_button_to_string(btn);

	/* Convert the mouse input as an input format. */
	const int bytes = snprintf(input, sizeof(input), "<%s%s><%u,%u>", button, event, cx, cy);

	nvim_api_input(sd->nvim, input, (unsigned int)bytes);
}

static void _grid_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				    void *event)
{
	struct grid *const sd = data;

	/* If there is no mouse drag, nothing to do! */
	if (!sd->mouse_drag.btn) {
		return;
	}

	const Evas_Event_Mouse_Move *const ev = event;
	unsigned int cx, cy;

	_coords_to_cell(sd, ev->cur.canvas.x, ev->cur.canvas.y, &cx, &cy);

	/* Did we move? If not, stop right here */
	if ((cx == sd->mouse_drag.prev_cx) && (cy == sd->mouse_drag.prev_cy))
		return;

	/* At this point, we have actually moved the mouse while holding a mouse
	 * button, hence dragging. Send the event then update the current mouse
	 * position. */
	_mouse_event(sd, "Drag", cx, cy, sd->mouse_drag.btn);

	sd->mouse_drag.prev_cx = cx;
	sd->mouse_drag.prev_cy = cy;
}

static void _grid_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				  void *event)
{
	struct grid *const sd = data;
	const Evas_Event_Mouse_Up *const ev = event;
	unsigned int cx, cy;

	_coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
	_mouse_event(sd, "Release", cx, cy, ev->button);
	sd->mouse_drag.btn = 0; /* Disable mouse dragging */
}

static void _grid_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				    void *event)
{
	struct grid *const sd = data;
	const Evas_Event_Mouse_Down *const ev = event;
	unsigned int cx, cy;

	_coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);

	/* When pressing down the mouse, we just registered the first values thay
	 * may be used for dragging with the mouse. */
	sd->mouse_drag.prev_cx = cx;
	sd->mouse_drag.prev_cy = cy;

	_mouse_event(sd, "Mouse", cx, cy, ev->button);
	sd->mouse_drag.btn = ev->button; /* Enable mouse dragging */
}

static void _grid_mouse_wheel_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				     void *event)
{
	struct grid *const sd = data;
	const Evas_Event_Mouse_Wheel *const ev = event;

	/* If mouse is NOT enabled, we don't handle mouse events */
	if (!nvim_mouse_enabled_get(sd->nvim)) {
		return;
	}

	const char *const dir = (ev->z < 0) ? "Up" : "Down";

	char input[64];
	unsigned int cx, cy;
	_coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
	const int bytes = snprintf(input, sizeof(input), "<ScrollWheel%s><%u,%u>", dir, cx, cy);
	nvim_api_input(sd->nvim, input, (unsigned int)bytes);
}

static void _grid_resize_cb(void *const data, Evas *const e EINA_UNUSED, Evas_Object *const obj, void *const event EINA_UNUSED)
{
	struct grid *const sd = data;
	if (!sd->cell_w || !sd->cell_h)
		return;

	Evas_Coord w, h;
	assert(obj == sd->textblock);
	evas_object_geometry_get(obj, NULL, NULL, &w, &h);
	const unsigned int cols = (unsigned int)w / sd->cell_w;
	const unsigned int rows = (unsigned int)h / sd->cell_h;

	evas_object_resize(sd->textblock, w, h);
	if (cols && rows && ((cols != sd->cols) || (rows != sd->rows))) {
		sd->in_resize++;
		nvim_api_ui_try_resize(sd->nvim, cols, rows);
	}
}

void grid_size_get(const struct grid *const sd, unsigned int *cols, unsigned int *rows)
{
	if (cols)
		*cols = sd->cols;
	if (rows)
		*rows = sd->rows;
}

void grid_matrix_set(struct grid *const sd, const unsigned int cols, const unsigned int rows)
{
	EINA_SAFETY_ON_TRUE_RETURN((cols == 0) || (rows == 0));

	/* Prevent useless resize */
	if ((sd->cols == cols) && (sd->rows == rows))
		return;

	/* We maintain the grid of cells as an Iliffe vector. Make sure we properly
	 * resize it without losing allocated memory */
	if (sd->cells) {
		free(sd->cells[0]);
	}
	sd->cells = realloc(sd->cells, rows * sizeof(struct cell *));
	sd->cells[0] = malloc(rows * cols * sizeof(struct cell));
	for (unsigned int i = 1; i < rows; i++) {
		sd->cells[i] = sd->cells[i - 1] + cols;
	}

	/* Every cell contains a single whitespace */
	for (unsigned int i = 0; i < rows; i++) {
		for (unsigned int j = 0; j < cols; j++) {
			struct cell *const c = &sd->cells[i][j];
			c->utf8[0] = ' ';
			c->bytes = 1;
			c->style_id = 0;
		}
	}

	/* We maintain a table of cursors, one by line. */
	if ((sd->cursors) && (rows < sd->rows)) {
		for (unsigned int i = rows; i < sd->rows; i++) {
			evas_textblock_cursor_free(sd->cursors[i]);
		}
	}
	sd->cursors = realloc(sd->cursors, rows * sizeof(Evas_Textblock_Cursor *));
	for (unsigned int i = sd->rows; i < rows; i++) {
		sd->cursors[i] = evas_object_textblock_cursor_new(sd->textblock);
	}

	/* Make sure our set of changed line has the right size. We don't care
	 * about its values, as we call grid_clear() just after */
	sd->line_has_changed = realloc(sd->line_has_changed, sizeof(Eina_Bool) * rows);

	sd->cols = cols;
	sd->rows = rows;
	grid_clear(sd);

	sd->in_resize--;
	sd->may_send_relayout = sd->in_resize == 0;
}

void grid_clear(struct grid *const sd)
{
	EINA_SAFETY_ON_FALSE_RETURN(sd->cols != 0 && sd->rows != 0);

	/* Delete everything written in the textblock */
	evas_object_textblock_clear(sd->textblock);
	sd->cursor.sep_written = EINA_FALSE;

	/* Write ones everywhere. All lines do change. */
	memset(sd->line_has_changed, 0xff, sizeof(Eina_Bool) * sd->rows);

	/* We add paragraph separators (<ps>) for each line. This allows a much
	 * faster textblock lookup. We add an extra space before to avoid internal
	 * textblock errors (is this a bug?) */
	for (unsigned int i = 0u; i < sd->rows; i++)
		evas_object_textblock_text_markup_prepend(sd->cursors[0], " </ps>");

	/* One cursor per paragraph */
	evas_textblock_cursor_paragraph_first(sd->cursors[0]);
	for (unsigned int i = 1u; i < sd->rows; i++) {
		evas_textblock_cursor_copy(sd->cursors[i - 1], sd->cursors[i]);
		evas_textblock_cursor_paragraph_next(sd->cursors[i]);
	}
}

void grid_line_edit(struct grid *const sd, const unsigned int row, const unsigned int col,
			const char *text, size_t text_len, const t_int style_id,
			const size_t repeat)
{
	struct cell *const cells_row = sd->cells[row];
	for (size_t i = 0; i < repeat; i++) {
		struct cell *const c = &cells_row[col + i];
		if (text_len == 1) {
			switch (text[0]) {
			case '<':
				text = "&lt;";
				text_len = 4;
				break;

			case '>':
				text = "&gt;";
				text_len = 4;
				break;

			case '&':
				text = "&amp;";
				text_len = 5;
				break;

			case '"':
				text = "&quot;";
				text_len = 6;
				break;

			case '\'':
				text = "&apos;";
				text_len = 6;
				break;
			}
		}
		assert(text_len <= sizeof(c->utf8));
		memcpy(c->utf8, text, text_len);
		c->bytes = (uint32_t)text_len;
		c->style_id = (uint32_t)style_id;
	}
	sd->line_has_changed[row] = EINA_TRUE;
}

void grid_flush(struct grid *const sd)
{
	Eina_Strbuf *const line = sd->line;

	if (sd->pending_style_update)
		grid_style_update(sd);

	for (unsigned int i = 0u; i < sd->rows; i++) {
		if (!sd->line_has_changed[i])
			continue;
		const struct cell *const row = sd->cells[i];

		if (sd->cursor.y == i)
			sd->cursor.sep_written = EINA_FALSE;

		uint32_t last_style = 0;
		for (unsigned int col = 0u; col < sd->cols; col++) {
			const struct cell *const c = &row[col];

			if (c->style_id != last_style) {
				if (last_style != 0) {
					eina_strbuf_append_printf(line, "</X%" PRIx32 ">",
								  last_style);
				}
				if (c->style_id != 0) {
					eina_strbuf_append_printf(line, "<X%" PRIx32 ">",
								  c->style_id);
				}
			}

			eina_strbuf_append_length(line, c->utf8, c->bytes);
			last_style = c->style_id;
		}
		if (last_style != 0)
			eina_strbuf_append_printf(line, "</X%" PRIx32 ">", last_style);

		Evas_Textblock_Cursor *const start = sd->cursors[i];
		Evas_Textblock_Cursor *const end = sd->tmp;
		evas_textblock_cursor_copy(start, end);
		evas_textblock_cursor_paragraph_char_first(start);
		evas_textblock_cursor_paragraph_char_last(end);

		evas_textblock_cursor_range_delete(start, end);
		evas_object_textblock_text_markup_prepend(end, eina_strbuf_string_get(line));
		eina_strbuf_reset(line);
	}
	memset(sd->line_has_changed, 0, sizeof(Eina_Bool) * sd->rows);
}

/**
 * THis function is called when we are done processing a batch of the "redraw"
 * method. This is a good time to update the cursor position. We cannot do it
 * when we receive cursor_goto, because the flush method has not yet been
 * called, which means that we cannot manipulate nor query the textblock!
 */
void grid_redraw_end(struct grid *const sd)
{
	const unsigned int to_x = sd->cursor.next_x;
	const unsigned int to_y = sd->cursor.next_y;

	/* Avoid useless computations */
	if ((to_x == sd->cursor.x) && (to_y == sd->cursor.y) && (!sd->mode_changed))
		return;

	/* Before moving the cursor, we delete the character JUST BEFORE the cursor.
	 * It is the invisible separator, we want it removed before the cursor
	 * goes away */
	if (sd->cursor.sep_written) {
		/* This is the situation:
		 *
		 * ,-- cursor.x
		 * |
		 * v
		 * +---+---+---+---+
		 * |   | < |   | = |
		 * +---+---+---+---+
		 *   ^       ^
		 *   |       '--- delete this
		 *   '--- delete this
		 */
		evas_textblock_cursor_char_delete(sd->cursor.cur);
		evas_textblock_cursor_char_next(sd->cursor.cur);
		evas_textblock_cursor_char_delete(sd->cursor.cur);
	}

	/* This is the situation:
	 *
	 * ,-- to_x
	 * |
	 * v
	 * +---+---+
	 * | < | = |
	 * +---+---+
	 * ^   ^
	 * |   '--- place a whitespace
	 * '-- place a whitespace
	 */

	/* Move the cursor to position (to_x + 1, to_y). Note the to_x+1, very
	 * important! It is used to insert a whitespace */
	evas_textblock_cursor_copy(sd->cursors[to_y], sd->cursor.cur);
	evas_textblock_cursor_paragraph_char_first(sd->cursor.cur);
	for (unsigned int i = 0u; i <= to_x; i++)
		evas_textblock_cursor_char_next(sd->cursor.cur);

	/* Insert the invisible separator at to_x+1 and to_x */
	if (sd->nvim->gui.theme.cursor_cuts_ligatures) {
		evas_textblock_cursor_text_append(sd->cursor.cur, INVISIBLE_SEP);
		evas_textblock_cursor_char_prev(sd->cursor.cur);
		evas_textblock_cursor_text_append(sd->cursor.cur, INVISIBLE_SEP);
		sd->cursor.sep_written = EINA_TRUE;
	} else
		evas_textblock_cursor_char_prev(sd->cursor.cur);

	int ox, oy;
	evas_object_geometry_get(sd->textblock, &ox, &oy, NULL, NULL);

	int y = 0, h = 0;
	evas_textblock_cursor_char_geometry_get(sd->cursor.cur, NULL, &y, NULL, &h);
	if (!gui_cmdline_enabled_get(&sd->nvim->gui))
		gui_cursor_calc(&sd->nvim->gui, (int)(to_x * sd->cell_w) + ox, y + oy,
				(int)sd->cell_w, h);

	/* Update the cursor's current position */
	sd->cursor.x = to_x;
	sd->cursor.y = to_y;
	sd->mode_changed = EINA_FALSE;
}

void grid_cursor_goto(struct grid *const sd, const unsigned int to_x, const unsigned int to_y)
{
	EINA_SAFETY_ON_FALSE_RETURN(to_y < sd->rows);
	EINA_SAFETY_ON_FALSE_RETURN(sd->cols != 0 && sd->rows != 0);

	sd->cursor.next_x = to_x;
	sd->cursor.next_y = to_y;
}

void grid_scroll(struct grid *const sd, const int top, const int bot, const int left,
		     const int right, const int rows)
{
	EINA_SAFETY_ON_FALSE_RETURN(right > left);
	EINA_SAFETY_ON_FALSE_RETURN(top >= 0 && bot >= 0 && left >= 0);

	int start_line, end_line, step;
	if (rows > 0) {
		/* Here, we scroll text UPWARDS. Line N-1 is replaced by line N.
		 *
		 *
		 * top
		 *   '>+------------------+     +------------------+
		 *     | Line 0           |  ,> | Line 1           |
		 *     +------------------+ /   +------------------+
		 * (1) | Line 1           |' ,> | Line 2           |
		 *     +------------------+ /   +------------------+
		 * (2) | Line 2           |'    | xxxxxx           |
		 *  ,> +------------------+     +------------------+
		 * bot
		 *
		 * So we start at top+|rows|, because top replaces nothing.
		 *     top+rows will overwrite top.
		 *     top+rows+1 will overwrite top+rows+1
		 *     etc.
		 *
		 * One bot is reached (exclusive), we are done.
		 */
		start_line = top + rows;
		end_line = bot;
		step = +1;
	} else {
		/* Here, we scroll text DOWNWARDS. Line N+1 is replaced by line N.
		 *
		 *     +------------------+     +------------------+
		 * (2) | Line 0           |,    | xxxxxx           |
		 *     +------------------+ \   +------------------+
		 * (1) | Line 1           |, '> | Line 0           |
		 *     +------------------+ \   +------------------+
		 *     | Line 2           |  '> | Line 1           |
		 *     +------------------+     +------------------+
		 *
		 * We start at bot-1-|rows|
		 */
		assert(rows < 0); /* <--- Frienly reminder */
		start_line = bot - 1 + rows;
		end_line = top - 1; /* Make the range exclusive */
		step = -1;
	}

	/* We have four working cursors
	 * c1 and c2 delimit the line portion to be copied
	 * c3 and c4 delimit the line portion to be replaced
	 */
	for (int from_line = start_line; from_line != end_line; from_line += step) {
		const int to_line = from_line - rows;
		if ((unsigned int)to_line >= sd->rows) {
			continue;
		}

		const struct cell *const source_row = sd->cells[from_line];
		struct cell *const target_row = sd->cells[to_line];

		const size_t len = sizeof(struct cell) * (size_t)(right - left);
		memcpy(&target_row[left], &source_row[left], len);

		sd->line_has_changed[to_line] = EINA_TRUE;
	}
}

void grid_cell_geometry_get(const struct grid *const sd, const unsigned int cell_x,
				const unsigned int cell_y, int *const px, int *const py,
				int *const pw, int *const ph)
{
	evas_textblock_cursor_copy(sd->cursors[cell_y], sd->tmp);
	evas_textblock_cursor_paragraph_char_first(sd->tmp);
	for (unsigned int i = 0u; i < cell_x; i++)
		evas_textblock_cursor_char_next(sd->tmp);

	evas_textblock_cursor_char_geometry_get(sd->tmp, px, py, pw, ph);
}

void grid_cursor_mode_set(struct grid *const sd, const struct mode *const mode)
{
	EINA_SAFETY_ON_NULL_RETURN(mode);

	struct gui *const gui = &sd->nvim->gui;
	cursor_mode_set(gui->cursor, mode);

	/* Update the cursor's color settings **************************************/
	struct grid_style *const style = eina_hash_find(sd->styles, &mode->attr_id);
	if (style != NULL)
		cursor_color_set(gui->cursor, style->fg_color);

	/* Register the new mode and update the cursor calculation function. */
	sd->mode_changed = EINA_TRUE;
}

void grid_cell_size_set(struct grid *const sd, const unsigned int cell_w, const unsigned int cell_h)
{
	sd->cell_w = cell_w;
	sd->cell_h = cell_h;
}

void grid_focus_in(struct grid *const sd)
{
	/* XXX This is a transition function */
	struct gui *const gui = &sd->nvim->gui;
	gui_wildmenu_style_set(gui->wildmenu, sd->style.object, sd->cell_w, sd->cell_h);
	gui_completion_style_set(gui->completion, sd->style.object, sd->cell_w, sd->cell_h);
}

Evas_Object *grid_textblock_get(const struct grid *const sd)
{
	return sd->textblock;
}

struct grid *grid_add(struct gui *const gui)
{
	struct grid *const sd = calloc(1, sizeof(*sd));
	if (EINA_UNLIKELY(! sd)) {
		CRI("Failed to allocate memory for grid");
		return NULL;
	}
	sd->nvim = gui->nvim;
	sd->layout = gui->layout;
	sd->line = eina_strbuf_new();

	/* At startup, first thing we will do is resize. This is caused by the call to
	 * nvim_attach() */
	sd->in_resize = 1;

	sd->styles = eina_hash_int64_new(EINA_FREE_CB(_grid_style_free));

	Evas_Object *o;

	sd->style.object = evas_textblock_style_new();
	sd->style.text = eina_strbuf_new();

	sd->textblock = o = evas_object_textblock_add(gui->evas);
	evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_textblock_style_set(o, sd->style.object);
	evas_object_show(sd->textblock);
	sd->tmp = evas_object_textblock_cursor_new(sd->textblock);

	/* Attach callbacks to the textblock */
	evas_object_event_callback_add(sd->textblock, EVAS_CALLBACK_MOUSE_MOVE, _grid_mouse_move_cb, sd);
	evas_object_event_callback_add(sd->textblock, EVAS_CALLBACK_MOUSE_DOWN, _grid_mouse_down_cb, sd);
	evas_object_event_callback_add(sd->textblock, EVAS_CALLBACK_MOUSE_UP, _grid_mouse_up_cb, sd);
	evas_object_event_callback_add(sd->textblock, EVAS_CALLBACK_MOUSE_WHEEL, _grid_mouse_wheel_cb, sd);
	evas_object_event_callback_add(sd->textblock, EVAS_CALLBACK_RESIZE, _grid_resize_cb, sd);

	/* Cursor setup */
	sd->cursor.cur = evas_object_textblock_cursor_new(sd->textblock);
	return sd;
}

void grid_del(struct grid *const sd)
{
	evas_textblock_style_free(sd->style.object);
	eina_strbuf_free(sd->style.text);
	eina_strbuf_free(sd->line);
	eina_hash_free(sd->styles);
	if (sd->cells) {
		free(sd->cells[0]);
		free(sd->cells);
	}
	free(sd->line_has_changed);
	for (unsigned int i = 0u; i < sd->rows; i++)
		evas_textblock_cursor_free(sd->cursors[i]);
	free(sd->cursors);
	free(sd);
}
