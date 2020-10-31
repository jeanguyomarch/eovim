/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/gui.h>
#include <eovim/log.h>
#include <eovim/main.h>
#include "gui_private.h"

enum { THEME_MSG_BLINK_SET = 0,
       THEME_MSG_COLOR_SET = 1,
       THEME_MSG_MAY_BLINK_SET = 2,
};

struct cursor {
	Evas_Object *edje;
	struct gui *gui;
	const struct mode *mode;
};

void gui_cursor_calc(struct gui *const gui, const int x, const int y, const int w, const int h)
{
	struct cursor *const cur = gui->cursor;
	if (!cur->mode)
		return;

	switch (cur->mode->cursor_shape) {
	case CURSOR_SHAPE_HORIZONTAL: {
		/* Place the cursor at the bottom of (x,y) and set its height to
		 * mode->cell_percentage of a cell's height */
		int h2 = (h * (int)cur->mode->cell_percentage) / 100;
		if (h2 <= 0)
			h2 = 1; /* We don't want an invisible cursor */

		evas_object_move(cur->edje, x, y + h - h2);
		evas_object_resize(cur->edje, w, h2);
	} break;

	case CURSOR_SHAPE_VERTICAL: {
		/* Place the cursor at (x,y) and set its width to mode->cell_percentage
		 * of a cell's width */
		evas_object_move(cur->edje, x, y);
		int w2 = (w * (int)cur->mode->cell_percentage) / 100;
		if (w2 <= 0)
			w2 = 1; /* We don't want an invisible cursor */
		evas_object_resize(cur->edje, w2, h);
	} break;

	case CURSOR_SHAPE_BLOCK: /* Fallthrough */
	default:
		evas_object_move(cur->edje, x, y);
		evas_object_resize(cur->edje, w, h);
		break;
	}
}

void cursor_focus_set(struct cursor *const cur, const Eina_Bool focused)
{
	if (focused) {
		Edje_Message_Int msg = { .val = 1 }; /* may_blink := TRUE */
		edje_object_message_send(cur->edje, EDJE_MESSAGE_INT, THEME_MSG_MAY_BLINK_SET,
					 &msg);
		edje_object_signal_emit(cur->edje, "focus,in", "eovim");
		if (cur->mode && cur->mode->blinkon != 0)
			edje_object_signal_emit(cur->edje, "eovim,blink,start", "eovim");
	} else
		edje_object_signal_emit(cur->edje, "focus,out", "eovim");
}

void cursor_mode_set(struct cursor *const cur, const struct mode *const mode)
{
	/* Update the blink parameters *********************************************/
	Edje_Message_Float_Set *msg;
	msg = alloca(sizeof(*msg) + (sizeof(double) * 3));
	msg->count = 3;
	msg->val[0] = (double)mode->blinkwait / 1000.0;
	msg->val[1] = (double)mode->blinkon / 1000.0;
	msg->val[2] = (double)mode->blinkoff / 1000.0;

	/* If we requested the cursor to blink, make it blink */
	if (mode->blinkon != 0) {
		/* If the cursor was blinking, we stop the blinking */
		if (cur->mode && cur->mode->blinkon)
			edje_object_signal_emit(cur->edje, "eovim,blink,stop", "eovim");
		edje_object_message_send(cur->edje, EDJE_MESSAGE_FLOAT_SET, THEME_MSG_BLINK_SET,
					 msg);
		edje_object_signal_emit(cur->edje, "eovim,blink,start", "eovim");
	} else
		edje_object_signal_emit(cur->edje, "eovim,blink,stop", "eovim");

	cur->mode = mode;
}

void gui_cursor_key_pressed(struct gui *const gui)
{
	if (gui->theme.react_to_key_presses)
		edje_object_signal_emit(gui->cursor->edje, "key,down", "eovim");
}

void cursor_color_set(struct cursor *const cur, const union color color)
{
	if (color.value != COLOR_DEFAULT) {
		Edje_Message_Int_Set *msg;
		msg = alloca(sizeof(*msg) + (sizeof(int) * 3));
		msg->count = 3;
		msg->val[0] = color.r;
		msg->val[1] = color.g;
		msg->val[2] = color.b;
		edje_object_message_send(cur->edje, EDJE_MESSAGE_INT_SET, THEME_MSG_COLOR_SET, msg);
	}
}

void cursor_blink_disable(struct cursor *const cur)
{
	Edje_Message_Int msg = { .val = 0 }; /* may_blink := FALSE */
	edje_object_message_send(cur->edje, EDJE_MESSAGE_INT, THEME_MSG_MAY_BLINK_SET, &msg);
	edje_object_signal_emit(cur->edje, "eovim,blink,stop", "eovim");
}

struct cursor *cursor_add(struct gui *const gui)
{
	struct cursor *const cur = calloc(1, sizeof(*cur));
	if (EINA_UNLIKELY(!cur)) {
		CRI("Failed to allocate memory");
		return NULL;
	}

	cur->edje = edje_object_add(gui->evas);
	cur->gui = gui;

	edje_object_file_set(cur->edje, main_edje_file_get(), "eovim/cursor");
	evas_object_pass_events_set(cur->edje, EINA_TRUE);
	evas_object_propagate_events_set(cur->edje, EINA_FALSE);
	evas_object_smart_member_add(cur->edje, gui->layout);
	evas_object_show(cur->edje);

	return cur;
}

void cursor_del(struct cursor *const cur)
{
	free(cur);
}
