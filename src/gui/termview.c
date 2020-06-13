/* This file is part of Eovim, which is under the MIT License ****************/

#include "eovim/termview.h"
#include "eovim/log.h"
#include "eovim/gui.h"
#include "eovim/main.h"
#include "eovim/keymap.h"
#include "eovim/nvim_helper.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim.h"

#include "gui_private.h"

#include <Edje.h>
#include <Ecore_Input.h>

enum { THEME_MSG_BLINK_SET = 0,
       THEME_MSG_COLOR_SET = 1,
       THEME_MSG_MAY_BLINK_SET = 2,
};

static Evas_Smart *_smart = NULL;
static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

/* This is the invisible separator. A zero-width space character that
 * allows to split ligatures without changing underlying VISUAL REPRESENTATION
 * of the text. */
static const char INVISIBLE_SEP[] = "\xe2\x81\xa3";

struct termview;
typedef void (*f_cursor_calc)(struct termview *, int, int, int, int);

static void _relayout(struct termview *sd);

struct cell {
	char utf8[8]; /* NOT NUL-terminated */
	uint32_t bytes;
	uint32_t style_id;
};

struct termview {
	Evas_Object_Smart_Clipped_Data __clipped_data; /* Required by Evas */
	Evas_Object *layout;
	Evas_Object *object;

	struct nvim *nvim;
	Evas_Object *textblock;
	Ecore_Event_Handler *key_down_handler;
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
		Evas_Object *ui; /**< Edje object */
		Evas_Textblock_Cursor *cur;
		unsigned int x;
		unsigned int y;

		unsigned int next_x;
		unsigned int next_y;

		/* This is set to true when the cursor has written a invisible
		 * space. It should be at (x,y) */
		Eina_Bool sep_written;

		/* Writing position */
		f_cursor_calc calc;
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

	const struct mode *mode;

	Eina_List *seq_compose;

	Eina_Hash *styles;
	struct {
		Eina_Strbuf *text;

		Evas_Textblock_Style *object;
		union color default_fg;
		union color default_bg;
		union color default_sp;

		Eina_Stringshare *font_name;
		unsigned int font_size;
		unsigned int line_gap;
	} style;

	Eina_Rectangle geometry;
	Eina_Bool pending_style_update;
	Eina_Bool mode_changed;

	/***************************************************************************
	 * The resize...
	 *
	 * That's something I found surprisingly very difficult to handle properly.
	 * The problem is that resize can arise from two different event sources
	 *  1) a style change (i.e. font) must cause the window to fit the termview
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
	 * we request a dimension change in neovim. This calls termview_matrix_set()
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

static struct termview_style *_termview_style_new(void)
{
	return calloc(1, sizeof(struct termview_style));
}

static void _termview_style_free(struct termview_style *const style)
{
	free(style);
}

static Eina_Bool _kind_style_foreach(const Eina_Hash *const hash EINA_UNUSED, const void *const key,
				     void *const data, void *const fdata)
{
	const char *const style = data;
	const char *const kind = key;
	struct termview *const sd = fdata;
	eina_strbuf_append_printf(sd->style.text, " kind_%s='+ %s'", kind, style);
	return EINA_TRUE;
}

static Eina_Bool _style_foreach(const Eina_Hash *const hash EINA_UNUSED, const void *const key,
				void *const data, void *const fdata)
{
	const struct termview_style *const style = data;
	const int64_t style_id = *((const int64_t *)key);
	struct termview *const sd = fdata;
	Eina_Strbuf *const buf = sd->style.text;

	eina_strbuf_append_printf(buf, " X%" PRIx64 "='+", style_id);

	if (style->reverse) {
		const uint32_t fg = (style->bg_color.value == COLOR_DEFAULT) ?
					    sd->style.default_bg.value :
					    style->bg_color.value;
		const uint32_t bg = (style->fg_color.value == COLOR_DEFAULT) ?
					    sd->style.default_fg.value :
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

	if (style->underline) {
		eina_strbuf_append_printf(buf,
					  " underline=on underline_type=dashed"
					  " underline_color=#%06" PRIx32,
					  style->sp_color.value & 0xFFFFFF);
	} else if (style->undercurl) {
		eina_strbuf_append_printf(buf,
					  " underline=dashed underline_type=dashed"
					  " underline_dash_color=#%06" PRIx32
					  " underline_dash_width=4 underline_dash_gap=2",
					  style->sp_color.value & 0xFFFFFF);
	}
	eina_strbuf_append_char(buf, '\'');

	return EINA_TRUE;
}

static void _termview_style_update(struct termview *const sd)
{
	Eina_Strbuf *const buf = sd->style.text;
	struct gui *const gui = &sd->nvim->gui;
	eina_strbuf_reset(buf);

	sd->pending_style_update = EINA_FALSE;
	eina_strbuf_append_printf(buf, "DEFAULT='font=\\'%s\\' font_size=%u color=#%06x wrap=none",
				  sd->style.font_name, sd->style.font_size,
				  sd->style.default_fg.value & 0xFFFFFF);
	if (sd->style.line_gap != 0u) {
		eina_strbuf_append_printf(buf, " linegap=%u", sd->style.line_gap);
	}
	eina_strbuf_append_char(buf, '\'');

	eina_hash_foreach(sd->styles, &_style_foreach, sd);
	eina_hash_foreach(gui->nvim->kind_styles, &_kind_style_foreach, sd);

	//DBG("Style update: %s\n", eina_strbuf_string_get(buf));
	evas_textblock_style_set(sd->style.object, eina_strbuf_string_get(buf));

	/* The height of a "cell" may vary depending on the font, linegap, etc. */
	evas_textblock_cursor_line_geometry_get(sd->cursors[0], NULL, NULL, NULL,
						(int *)&sd->cell_h);

	gui_wildmenu_style_set(gui->wildmenu, sd->style.object, sd->cell_w, sd->cell_h);
	gui_completion_style_set(gui->completion, sd->style.object, sd->cell_w, sd->cell_h);

	_relayout(sd);
}

static void _keys_send(struct termview *sd, const char *keys, unsigned int size)
{
	nvim_api_input(sd->nvim, keys, size);
	if (sd->nvim->gui.theme.react_to_key_presses)
		edje_object_signal_emit(sd->cursor.ui, "key,down", "eovim");
}

static inline Eina_Bool _composing_is(const struct termview *sd)
{
	/* Composition is pending if the seq_compose list is not empty */
	return (sd->seq_compose == NULL) ? EINA_FALSE : EINA_TRUE;
}

static inline void _composition_reset(struct termview *sd)
{
	/* Discard all elements within the list */
	Eina_Stringshare *str;
	EINA_LIST_FREE(sd->seq_compose, str)
	eina_stringshare_del(str);
}

static inline void _composition_add(struct termview *sd, const Ecore_Event_Key *const key)
{
	/* Add the key as a stringshare in the seq list. Hence, starting the
    * composition */
	Eina_Stringshare *const shr = eina_stringshare_add(key->key);
	sd->seq_compose = eina_list_append(sd->seq_compose, shr);
}

/*
 * This function returns EINA_TRUE when the key was handled within this
 * functional unit. When it returns EINA_FALSE, the caller should handle the
 * key itself.
 */
static Eina_Bool _compose(struct termview *const sd, const Ecore_Event_Key *const key)
{
	if (_composing_is(sd)) {
		char *res = NULL;

		/* When composition is enabled, we want to skip modifiers, and only feed
        * non-modified keys to the composition engine */
		if (key->modifiers != 0u)
			return EINA_TRUE;

		/* Add the current key to the composition list, and compute */
		_composition_add(sd, key);
		const Ecore_Compose_State state = ecore_compose_get(sd->seq_compose, &res);
		if (state == ECORE_COMPOSE_DONE) {
			/* We composed! Write the composed key! */
			_composition_reset(sd);
			if (EINA_LIKELY(NULL != res)) {
				_keys_send(sd, res, (unsigned int)strlen(res));
				free(res);
				return EINA_TRUE;
			}
		} else if (state == ECORE_COMPOSE_NONE) {
			/* The composition yield nothing. Reset */
			_composition_reset(sd);
		}
	} else /* Not composing yet */
	{
		/* Add the key to the composition engine */
		_composition_add(sd, key);
		const Ecore_Compose_State state = ecore_compose_get(sd->seq_compose, NULL);
		if (state != ECORE_COMPOSE_MIDDLE) {
			/* Nope, this does not allow composition */
			_composition_reset(sd);
		} else {
			return EINA_TRUE;
		} /* Composing.... */
	}

	/* Delegate the key to the caller */
	return EINA_FALSE;
}

static void _cursor_calc_block(struct termview *const sd, const int x, const int y, const int w,
			       const int h)
{
	/* Place the cursor at (x,y) and set its size to a cell */
	evas_object_move(sd->cursor.ui, x, y);
	evas_object_resize(sd->cursor.ui, w, h);
}

static void _cursor_calc_vertical(struct termview *const sd, const int x, const int y, int w,
				  const int h)
{
	/* Place the cursor at (x,y) and set its width to mode->cell_percentage
   * of a cell's width */
	evas_object_move(sd->cursor.ui, x, y);
	w = (w * (int)sd->mode->cell_percentage) / 100;
	if (w <= 0)
		w = 1; /* We don't want an invisible cursor */
	evas_object_resize(sd->cursor.ui, w, h);
}

static void _cursor_calc_horizontal(struct termview *const sd, const int x, const int y,
				    const int w, const int h)
{
	/* Place the cursor at the bottom of (x,y) and set its height to
   * mode->cell_percentage of a cell's height */

	int h2 = (h * (int)sd->mode->cell_percentage) / 100;
	if (h2 <= 0)
		h2 = 1; /* We don't want an invisible cursor */

	evas_object_move(sd->cursor.ui, x, y + h - h2);
	evas_object_resize(sd->cursor.ui, w, h2);
}

static void _coords_to_cell(const struct termview *sd, int px, int py, unsigned int *cell_x,
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

static void _mouse_event(struct termview *sd, const char *event, unsigned int cx, unsigned int cy,
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

static void _termview_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				    void *event)
{
	struct termview *const sd = data;

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

static void _termview_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				  void *event)
{
	struct termview *const sd = data;
	const Evas_Event_Mouse_Up *const ev = event;
	unsigned int cx, cy;

	_coords_to_cell(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);
	_mouse_event(sd, "Release", cx, cy, ev->button);
	sd->mouse_drag.btn = 0; /* Disable mouse dragging */
}

static void _termview_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				    void *event)
{
	struct termview *const sd = data;
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

static void _termview_mouse_wheel_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				     void *event)
{
	struct termview *const sd = data;
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

/**
 * If \p cond is true add the modifier \p mod to the cell of \p buf at the index
 * pointed by \p size.  A dash '-' is then appended.
 * The index pointed by \p size is then incremented by two to update the caller's
 * value.
 *
 * \return
 * If the operation would overflow (the index pointed by \p size is greater than
 * 61) this function does nothing and returns EINA_FALSE.
 * Otherwise, this function returns EINA_TRUE, indicating the absence of error.
 */
static Eina_Bool _add_modifier(char buf[64], const int cond, const char mod, int *const size)
{
	if (cond) {
		if (EINA_UNLIKELY(*size + 2 >= 64)) {
			ERR("Buffer overflow! Not sending key to neovim");
			return EINA_FALSE;
		}

		buf[(*size) + 0] = mod;
		buf[(*size) + 1] = '-';
		(*size) += 2;
	}
	return EINA_TRUE;
}

static Eina_Bool _termview_key_down_cb(void *const data, const int type EINA_UNUSED,
				       void *const event)
{
	struct termview *const sd = data;
	const Ecore_Event_Key *const ev = event;
	int send_size = 0;
	const struct keymap *const keymap = keymap_get(ev->key);
	char buf[64];
	const char *send;
	const char caps[] = "Caps_Lock";
	struct gui *const gui = &(sd->nvim->gui);

#if 0
   printf("key      : %s\n", ev->key);
   printf("keyname  : %s\n", ev->keyname);
   printf("string   : %s\n", ev->string);
   printf("compose  : %s\n", ev->compose);
   printf("modifiers: 0x%x\n", ev->modifiers);
   printf("------------------------\n");
#endif

	/* Did we press the Caps_Lock key? We can either have enabled or disabled
    * caps lock. */
	if (!strcmp(ev->key, caps)) /* Caps lock is pressed */
	{
		if (ev->modifiers & ECORE_EVENT_LOCK_CAPS) /* DISABLE */
		{
			/* If we press caps lock with prior caps locks, it means we
              * just DISABLED the caps lock */
			gui_caps_lock_dismiss(gui);
		} else /* ENABLE */
		{
			/* If we press caps lock without prior caps locks, it means we
              * just ENABLED the caps lock */
			gui_caps_lock_alert(gui);
		}
	}

	/* If the key produces nothing. Stop */
	if ((ev->string == NULL) && (keymap == NULL))
		return ECORE_CALLBACK_PASS_ON;

	/* Try the composition. When this function returns EINA_TRUE, it already
    * worked out, nothing more to do. */
	if (_compose(sd, ev))
		return ECORE_CALLBACK_PASS_ON;

	const Eina_Bool ctrl = ev->modifiers & ECORE_EVENT_MODIFIER_CTRL;
	const Eina_Bool super = ev->modifiers & ECORE_EVENT_MODIFIER_WIN;
	const Eina_Bool alt = ev->modifiers & ECORE_EVENT_MODIFIER_ALT;
	const Eina_Bool shift = ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT;

	/* Register modifiers. Ctrl and shift are special: we enable composition
    * only if the key is present in the keymap (it is a special key). We
    * disregard shift alone, because it would just mean "uppercase" if alone;
    * however when combined with the keymap, we will compose! */
	const Eina_Bool compose = ctrl || super || alt || (shift && keymap);

	if (compose) {
		const char *const key = keymap ? keymap->name : ev->key;
		if (!key) {
			return ECORE_CALLBACK_PASS_ON;
		}

		/* Compose a string containing the textual representation of the
         * special keys to be sent to neovim.
         * Search :help META for details.
         *
         * We first compose the first part with the modifiers. E.g. <C-S-
         */
		buf[0] = '<';
		send_size = 1;
		if (!_add_modifier(buf, ctrl, 'C', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}
		if (!_add_modifier(buf, shift, 'S', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}
		if (!_add_modifier(buf, super, 'D', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}
		if (!_add_modifier(buf, alt, 'A', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}

		/* Add the real key after the modifier, and close the bracket */
		assert(send_size < (int)sizeof(buf));
		const size_t len = sizeof(buf) - (size_t)send_size;
		const int ret = snprintf(buf + send_size, len, "%s>", key);
		if (EINA_UNLIKELY(ret < 2)) {
			ERR("Failed to compose key.");
			return ECORE_CALLBACK_PASS_ON;
		}
		send_size += ret;
		send = buf;
	} else if (keymap) {
		send_size = snprintf(buf, sizeof(buf), "<%s>", keymap->name);
		if (EINA_UNLIKELY(send_size < 3)) {
			ERR("Failed to write key to string");
			return ECORE_CALLBACK_PASS_ON;
		}
		send = buf;
	} else {
		assert(ev->string != NULL);
		send = ev->string;
		send_size = (int)strlen(send);
	}

	/* If a key is availabe pass it to neovim and update the ui */
	if (EINA_LIKELY(send_size > 0))
		_keys_send(sd, send, (unsigned int)send_size);
	else
		DBG("Unhandled key '%s'", ev->key);
	return ECORE_CALLBACK_PASS_ON;
}

static void _termview_focus_in_cb(void *data, Evas *evas, Evas_Object *obj EINA_UNUSED,
				  void *event EINA_UNUSED)
{
	Edje_Message_Int msg = { .val = 1 }; /* may_blink := TRUE */
	struct termview *const sd = data;
	struct gui *const gui = &(sd->nvim->gui);

	/* When entering back on the Eovim window, the user may have pressed
    * Caps_Lock outside of Eovim's context. So we have to make sure when
    * entering Eovim again, that we are on the same page with the input
    * events. */
	const Evas_Lock *const lock = evas_key_lock_get(evas);
	if (evas_key_lock_is_set(lock, "Caps_Lock"))
		gui_caps_lock_alert(gui);
	else
		gui_caps_lock_dismiss(gui);

	edje_object_message_send(sd->cursor.ui, EDJE_MESSAGE_INT, THEME_MSG_MAY_BLINK_SET, &msg);
	edje_object_signal_emit(sd->cursor.ui, "focus,in", "eovim");
	if (sd->mode && sd->mode->blinkon != 0) {
		edje_object_signal_emit(sd->cursor.ui, "eovim,blink,start", "eovim");
	}

	gui_wildmenu_style_set(gui->wildmenu, sd->style.object, sd->cell_w, sd->cell_h);
	gui_completion_style_set(gui->completion, sd->style.object, sd->cell_w, sd->cell_h);
}

static void _termview_focus_out_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
				   void *event EINA_UNUSED)
{
	Edje_Message_Int msg = { .val = 0 }; /* may_blink := FALSE */

	struct termview *const sd = data;
	edje_object_message_send(sd->cursor.ui, EDJE_MESSAGE_INT, THEME_MSG_MAY_BLINK_SET, &msg);
	edje_object_signal_emit(sd->cursor.ui, "eovim,blink,stop", "eovim");
	edje_object_signal_emit(sd->cursor.ui, "focus,out", "eovim");
}

static void _relayout(struct termview *const sd)
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
		evas_object_geometry_get(sd->object, &geo->x, &geo->y, NULL, NULL);

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
		EINA_ITERATOR_FOREACH (it, rect) {
			eina_rectangle_union(&frame, rect);
		}
		eina_iterator_free(it);

		geo->w = (int)(sd->cell_w * sd->cols);
		geo->h = frame.h;

		if (sd->may_send_relayout) {
			evas_object_smart_callback_call(sd->object, "relayout", geo);
		}
	}
}

static void _smart_add(Evas_Object *obj)
{
	struct termview *const sd = calloc(1, sizeof(struct termview));
	if (EINA_UNLIKELY(!sd)) {
		CRI("Failed to allocate termview structure");
		return;
	}

	sd->line = eina_strbuf_new();

	/* At startup, first thing we will do is resize. This is caused by the call to
    * nvim_attach() */
	sd->in_resize = 1;

	/* Create the smart object */
	evas_object_smart_data_set(obj, sd);
	_parent_sc.add(obj);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_IN, _termview_focus_in_cb, sd);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_FOCUS_OUT, _termview_focus_out_cb, sd);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_MOVE, _termview_mouse_move_cb, sd);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_DOWN, _termview_mouse_down_cb, sd);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_UP, _termview_mouse_up_cb, sd);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_WHEEL, _termview_mouse_wheel_cb,
				       sd);

	/* We use the ECORE event instead of the evas smart callback because
    * the key modifiers are much more convenient as Ecore_Event_Key events
    * than Evas_Event_Key_Down.
    * This is not a problem, because the termview is the only widget in
    * the Evas that actually requires keyboard use */
	sd->key_down_handler =
		ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, &_termview_key_down_cb, sd);

	sd->styles = eina_hash_int64_new(EINA_FREE_CB(_termview_style_free));

	Evas *const evas = evas_object_evas_get(obj);
	Evas_Object *o;

	sd->style.object = evas_textblock_style_new();
	sd->style.text = eina_strbuf_new();

	/* A 1x1 cell matrix to retrieve the font size. Always invisible */
	sd->sizing_textgrid = o = evas_object_textgrid_add(evas);
	evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_textgrid_size_set(o, 1, 1);

	sd->textblock = o = evas_object_textblock_add(evas);
	evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_smart_member_add(o, obj);
	evas_object_textblock_style_set(o, sd->style.object);
	evas_object_show(sd->textblock);
	sd->tmp = evas_object_textblock_cursor_new(sd->textblock);

	/* Cursor setup */
	sd->cursor.ui = o = edje_object_add(evas);
	sd->cursor.calc = _cursor_calc_block; /* Cursor will be a block by default */
	edje_object_file_set(o, main_edje_file_get(), "eovim/cursor");
	evas_object_pass_events_set(o, EINA_TRUE);
	evas_object_propagate_events_set(o, EINA_FALSE);
	evas_object_smart_member_add(o, obj);
	evas_object_show(o);
	sd->cursor.cur = evas_object_textblock_cursor_new(sd->textblock);
}

static void _smart_del(Evas_Object *obj)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	evas_textblock_style_free(sd->style.object);
	eina_strbuf_free(sd->style.text);
	evas_object_del(sd->cursor.ui);
	eina_strbuf_free(sd->line);
	if (sd->cells) {
		free(sd->cells[0]);
		free(sd->cells);
	}
	free(sd->line_has_changed);
	for (unsigned int i = 0u; i < sd->rows; i++) {
		evas_textblock_cursor_free(sd->cursors[i]);
	}
	free(sd->cursors);
	ecore_event_handler_del(sd->key_down_handler);
	_composition_reset(sd);
}

static void _smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
	struct termview *const sd = evas_object_smart_data_get(obj);

	if (!sd->cell_w || !sd->cell_h) {
		return;
	}

	const unsigned int cols = (unsigned int)w / sd->cell_w;
	const unsigned int rows = (unsigned int)h / sd->cell_h;

	evas_object_resize(sd->textblock, w, h);
	if (cols && rows && ((cols != sd->cols) || (rows != sd->rows))) {
		sd->in_resize++;
		nvim_api_ui_try_resize(sd->nvim, cols, rows);
	}
}

Eina_Bool termview_init(void)
{
	static Evas_Smart_Class sc;

	static const Evas_Smart_Cb_Description smart_callbacks[] = { { "relayout", "" },
								     { NULL, NULL } };

	evas_object_smart_clipped_smart_set(&_parent_sc);
	sc = _parent_sc;
	sc.name = "termview";
	sc.version = EVAS_SMART_CLASS_VERSION;
	sc.add = _smart_add;
	sc.del = _smart_del;
	sc.resize = _smart_resize;
	sc.callbacks = smart_callbacks, _smart = evas_smart_class_new(&sc);

	return EINA_TRUE;
}

void termview_shutdown(void)
{
	evas_smart_free(_smart);
}

Evas_Object *termview_add(Evas_Object *parent, struct nvim *nvim)
{
	Evas *const e = evas_object_evas_get(parent);
	Evas_Object *const obj = evas_object_smart_add(e, _smart);
	struct termview *const sd = evas_object_smart_data_get(obj);
	sd->object = obj;
	sd->nvim = nvim;
	sd->layout = parent;
	return obj;
}

void termview_cell_size_get(const Evas_Object *obj, unsigned int *w, unsigned int *h)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	if (w)
		*w = sd->cell_w;
	if (h)
		*h = sd->cell_h;
}

void termview_size_get(const Evas_Object *obj, unsigned int *cols, unsigned int *rows)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	if (cols)
		*cols = sd->cols;
	if (rows)
		*rows = sd->rows;
}

void termview_matrix_set(Evas_Object *const obj, const unsigned int cols, const unsigned int rows)
{
	EINA_SAFETY_ON_TRUE_RETURN((cols == 0) || (rows == 0));
	struct termview *const sd = evas_object_smart_data_get(obj);

	/* Prevent useless resize */
	if ((sd->cols == cols) && (sd->rows == rows)) {
		return;
	}

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
   * about its values, as we call termview_clear() just after */
	sd->line_has_changed = realloc(sd->line_has_changed, sizeof(Eina_Bool) * rows);

	sd->cols = cols;
	sd->rows = rows;
	termview_clear(obj);

	sd->in_resize--;
	sd->may_send_relayout = sd->in_resize == 0;
}

void termview_clear(Evas_Object *const obj)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	EINA_SAFETY_ON_FALSE_RETURN(sd->cols != 0 && sd->rows != 0);

	/* Delete everything written in the textblock */
	evas_object_textblock_clear(sd->textblock);
	sd->cursor.sep_written = EINA_FALSE;

	/* Write ones everywhere. All lines do change. */
	memset(sd->line_has_changed, 0xff, sizeof(Eina_Bool) * sd->rows);

	/* We add paragraph separators (<ps>) for each line. This allows a much
   * faster textblock lookup. We add an extra space before to avoid internal
   * textblock errors (is this a bug?) */
	for (unsigned int i = 0u; i < sd->rows; i++) {
		evas_object_textblock_text_markup_prepend(sd->cursors[0], " </ps>");
	}

	/* One cursor per paragraph */
	evas_textblock_cursor_paragraph_first(sd->cursors[0]);
	for (unsigned int i = 1u; i < sd->rows; i++) {
		evas_textblock_cursor_copy(sd->cursors[i - 1], sd->cursors[i]);
		evas_textblock_cursor_paragraph_next(sd->cursors[i]);
	}
}

void termview_line_edit(Evas_Object *const obj, const unsigned int row, const unsigned int col,
			const char *text, size_t text_len, const t_int style_id,
			const size_t repeat)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
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

void termview_flush(Evas_Object *const obj)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	Eina_Strbuf *const line = sd->line;

	if (sd->pending_style_update) {
		_termview_style_update(sd);
	}

	for (unsigned int i = 0u; i < sd->rows; i++) {
		if (!sd->line_has_changed[i]) {
			continue;
		}
		const struct cell *const row = sd->cells[i];

		if (sd->cursor.y == i) {
			sd->cursor.sep_written = EINA_FALSE;
		}

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
		if (last_style != 0) {
			eina_strbuf_append_printf(line, "</X%" PRIx32 ">", last_style);
		}

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
void termview_redraw_end(Evas_Object *const obj)
{
	struct termview *const sd = evas_object_smart_data_get(obj);

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
	evas_textblock_cursor_text_append(sd->cursor.cur, INVISIBLE_SEP);
	evas_textblock_cursor_char_prev(sd->cursor.cur);
	evas_textblock_cursor_text_append(sd->cursor.cur, INVISIBLE_SEP);
	sd->cursor.sep_written = EINA_TRUE;

	int ox, oy;
	evas_object_geometry_get(sd->textblock, &ox, &oy, NULL, NULL);

	int y = 0, h = 0;
	evas_textblock_cursor_char_geometry_get(sd->cursor.cur, NULL, &y, NULL, &h);
	sd->cursor.calc(sd, (int)(to_x * sd->cell_w) + ox, y + oy, (int)sd->cell_w, h);

	/* Update the cursor's current position */
	sd->cursor.x = to_x;
	sd->cursor.y = to_y;
	sd->mode_changed = EINA_FALSE;
}

void termview_cursor_goto(Evas_Object *const obj, const unsigned int to_x, const unsigned int to_y)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	EINA_SAFETY_ON_FALSE_RETURN(to_y < sd->rows);
	EINA_SAFETY_ON_FALSE_RETURN(sd->cols != 0 && sd->rows != 0);

	sd->cursor.next_x = to_x;
	sd->cursor.next_y = to_y;
}

void termview_scroll(Evas_Object *const obj, const int top, const int bot, const int left,
		     const int right, const int rows)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
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

void termview_cell_geometry_get(const Evas_Object *const obj, const unsigned int cell_x,
				const unsigned int cell_y, int *const px, int *const py,
				int *const pw, int *const ph)
{
	const struct termview *const sd = evas_object_smart_data_get(obj);

	evas_textblock_cursor_copy(sd->cursors[cell_y], sd->tmp);
	evas_textblock_cursor_paragraph_char_first(sd->tmp);
	for (unsigned int i = 0u; i < cell_x; i++)
		evas_textblock_cursor_char_next(sd->tmp);

	evas_textblock_cursor_char_geometry_get(sd->tmp, px, py, pw, ph);
}

void termview_cursor_mode_set(Evas_Object *const obj, const struct mode *const mode)
{
	EINA_SAFETY_ON_NULL_RETURN(mode);
	EINA_SAFETY_ON_FALSE_RETURN((unsigned)mode->cursor_shape <= 2);

	/* Set sd->cursor_calc to the appropriate function that will calculate
   * the resizing and positionning of the cursor. We also keep track of
   * the mode. */
	struct termview *const sd = evas_object_smart_data_get(obj);
	const f_cursor_calc funcs[] = {
		[CURSOR_SHAPE_BLOCK] = &_cursor_calc_block,
		[CURSOR_SHAPE_HORIZONTAL] = &_cursor_calc_horizontal,
		[CURSOR_SHAPE_VERTICAL] = &_cursor_calc_vertical,
	};

	/* Update the blink parameters *********************************************/
	{
		Edje_Message_Float_Set *msg;
		msg = alloca(sizeof(*msg) + (sizeof(double) * 3));
		msg->count = 3;
		msg->val[0] = (double)mode->blinkwait / 1000.0;
		msg->val[1] = (double)mode->blinkon / 1000.0;
		msg->val[2] = (double)mode->blinkoff / 1000.0;

		/* If we requested the cursor to blink, make it blink */
		if (mode->blinkon != 0) {
			/* If the cursor was blinking, we stop the blinking */
			if (sd->mode && sd->mode->blinkon) {
				edje_object_signal_emit(sd->cursor.ui, "eovim,blink,stop", "eovim");
			}
			edje_object_message_send(sd->cursor.ui, EDJE_MESSAGE_FLOAT_SET,
						 THEME_MSG_BLINK_SET, msg);
			edje_object_signal_emit(sd->cursor.ui, "eovim,blink,start", "eovim");
		} else {
			edje_object_signal_emit(sd->cursor.ui, "eovim,blink,stop", "eovim");
		}
	}

	/* Update the cursor's color settings **************************************/
	struct termview_style *const style = eina_hash_find(sd->styles, &mode->attr_id);
	if (style != NULL && style->fg_color.value != COLOR_DEFAULT) {
		Edje_Message_Int_Set *msg;
		msg = alloca(sizeof(*msg) + (sizeof(int) * 3));
		msg->count = 3;
		msg->val[0] = style->fg_color.r;
		msg->val[1] = style->fg_color.g;
		msg->val[2] = style->fg_color.b;

		edje_object_message_send(sd->cursor.ui, EDJE_MESSAGE_INT_SET, THEME_MSG_COLOR_SET,
					 msg);
	}

	/* Register the new mode and update the cursor calculation function. */
	sd->mode = mode;
	sd->cursor.calc = funcs[mode->cursor_shape];
	sd->mode_changed = EINA_TRUE;
}

void termview_cursor_visibility_set(Evas_Object *obj, Eina_Bool visible)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	if (visible)
		evas_object_show(sd->cursor.ui);
	else
		evas_object_hide(sd->cursor.ui);
}

void termview_default_colors_set(Evas_Object *const obj, const union color fg, const union color bg,
				 const union color sp)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	const Eina_Bool changed = (sd->style.default_fg.value != fg.value) ||
				  (sd->style.default_bg.value != bg.value) ||
				  (sd->style.default_sp.value != sp.value);

	if (changed) {
		sd->style.default_fg = fg;
		sd->style.default_bg = bg;
		sd->style.default_sp = sp;
		sd->pending_style_update = EINA_TRUE;
	}
}

struct termview_style *termview_style_get(Evas_Object *const obj, const t_int style_id)
{
	struct termview *const sd = evas_object_smart_data_get(obj);

	/* TODO check errors */
	struct termview_style *style = eina_hash_find(sd->styles, &style_id);
	if (style == NULL) {
		style = _termview_style_new();
		if (EINA_UNLIKELY(!style))
			return NULL;
		const Eina_Bool added = eina_hash_add(sd->styles, &style_id, style);
		if (EINA_UNLIKELY(!added)) {
			ERR("Failed to add style to hash table");
			_termview_style_free(style);
			return NULL;
		}
	}

	/* If we requested a style, it is to modify it. So we implicitely
	 * request a style update that will occur at the next flush */
	sd->pending_style_update = EINA_TRUE;
	return style;
}

void termview_font_set(Evas_Object *const obj, Eina_Stringshare *const font_name,
		       const unsigned int font_size)
{
	struct termview *const sd = evas_object_smart_data_get(obj);

	eina_stringshare_replace(&sd->style.font_name, font_name);
	sd->style.font_size = font_size;

	evas_object_textgrid_font_set(sd->sizing_textgrid, sd->style.font_name,
				      (int)sd->style.font_size);
	evas_object_textgrid_cell_size_get(sd->sizing_textgrid, (int *)&sd->cell_w,
					   (int *)&sd->cell_h);
	sd->pending_style_update = EINA_TRUE;
}

void termview_linespace_set(Evas_Object *const obj, const unsigned int linespace)
{
	struct termview *const sd = evas_object_smart_data_get(obj);
	sd->style.line_gap = linespace;
	sd->pending_style_update = EINA_TRUE;
}
