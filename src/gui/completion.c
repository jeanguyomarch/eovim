/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/log.h>
#include <eovim/gui.h>
#include <eovim/main.h>

#include "gui_private.h"

struct completion {
	struct popupmenu pop;
	Evas_Object *edje;

	/* Position at which the completion popup must be displayed */
	unsigned int col;
	unsigned int row;

	int has_kind;
	int max_len;
};
static_assert(offsetof(struct completion, pop) == 0, "popupmenu must be the first element");

static Elm_Genlist_Item_Class *_completion_itc = NULL;

/*
 * The popupmenu is ALWAYS the first field of the completion menu. So the address of the
 * popupmenu is the same than the completion widget.
 * This is strictly valid C, that does not violate strict aliasing.
 */
#define COMPLETION_GET(PopUp) ((struct completion *)PopUp)

/* XXX Not sure if that's the best structure, given that some strings are small... */
struct completion_item {
	struct completion *completion;
	const char *word;
	const char *kind;
	const char *menu;
	const char *info;
	/* Buffer containing the memory pointed to by word, kind, menu, info */
	char mem[];
};

static struct completion_item *completion_item_new(struct completion *const cmpl,
						   const char *const word, const uint32_t word_size,
						   const char *const kind, const uint32_t kind_size,
						   const char *const menu, const uint32_t menu_size,
						   const char *const info, const uint32_t info_size)
{
	/*
	 * word_ptr + kind_ptr + menu_ptr + info_ptr +
	 * [word + \0] + [kind + \0] + [menu + \0] + [info] + \0]
	 */
	const size_t buf_size =
		sizeof(struct completion_item) + word_size + kind_size + menu_size + info_size + 4u;

	struct completion_item *const item = malloc(buf_size);
	if (EINA_UNLIKELY(!item)) {
		CRI("Failed to allocate memory (%zu bytes)", buf_size);
		return NULL;
	}
	item->completion = cmpl;
	char *buf = item->mem;

	item->word = buf;
	memcpy(buf, word, word_size);
	buf[word_size] = '\0';
	buf += (word_size + 1u);

	item->kind = buf;
	memcpy(buf, kind, kind_size);
	buf[kind_size] = '\0';
	buf += (kind_size + 1u);

	item->menu = buf;
	memcpy(buf, menu, menu_size);
	buf[menu_size] = '\0';
	buf += (menu_size + 1u);

	item->info = buf;
	memcpy(buf, info, info_size);
	buf[info_size] = '\0';

	return item;
}

static void completion_item_del(void *const data, Evas_Object *const obj EINA_UNUSED)
{
	struct completion_item *const item = data;
	free(item);
}

static Evas_Object *completion_resuable_content_get(void *const data, Evas_Object *const obj,
						    const char *const part EINA_UNUSED,
						    Evas_Object *old)
{
	struct completion_item *const item = data;
	struct completion *const cmpl = item->completion;
	const struct popupmenu *const pop = &cmpl->pop;

	old = popupmenu_item_use(pop, obj, old);

	/* Setup the contents of the textblock style */
	Eina_Strbuf *const buf = cmpl->pop.sbuf;
	eina_strbuf_reset(buf);
	const char base[] = "<left_margin=4><ellipsis=1.0>";
	eina_strbuf_append_length(buf, base, sizeof(base) - 1);

	/********************************************************************************
	 * The goal is to display, on a best-effort basis:
	 *   KIND WORD MENU
	 *
	 * ,---------------------------------,
	 * | [m] my_method (menu details...) |
	 * '---------------------------------'
	 *
	 * Where 'kind' is styled by the vim runtime, and menu details use different font
	 * weight and style.
	 * The word is displayed with a very light glow.
	 */
	if (cmpl->has_kind) {
		if (item->kind[0] != '\0') {
			const unsigned int style = gui_style_hash(item->kind);
			eina_strbuf_append_printf(
				buf, "<kind_default><kind_%u>%s</kind_%u></kind_default> ", style,
				item->kind, style);
		} else
			eina_strbuf_append_length(buf, "  ", 2);
	}
	eina_strbuf_append_printf(buf, "<style=glow glow_color=#%06" PRIx32 "10>%s</>",
				  pop->gui->default_fg.value & 0xffffff, item->word);

	if (item->menu[0] != '\0') {
		eina_strbuf_append_printf(buf, " <font_style=italic font_weight=thin>%s</>",
					  item->menu);
	}

	evas_object_textblock_text_markup_set(old, eina_strbuf_string_get(buf));
	return old;
}

void gui_completion_append(struct gui *const gui, const char *const word, const uint32_t word_size,
			   const char *const kind, const uint32_t kind_size, const char *const menu,
			   const uint32_t menu_size, const char *const info,
			   const uint32_t info_size)
{
	struct completion *const cmpl = gui->completion;
	struct completion_item *const item = completion_item_new(
		cmpl, word, word_size, kind, kind_size, menu, menu_size, info, info_size);
	if (EINA_UNLIKELY(!item))
		return;

	/* Retrieve the amount of codepoints used in the completion item. This will give
	 * a *rough* estimation of the width of the completion... If in the end it is too big,
	 * we will truncate with an ellipsis. I know this is fundamentally incorrect, because
	 * we should compute the actual width, but this is quite complex to do, and this
	 * heuristic will cover most of the cases (none I will ever face... I guess) */
	const int len = eina_unicode_utf8_get_len(item->word) +
			eina_unicode_utf8_get_len(item->menu) +
			eina_unicode_utf8_get_len(item->kind);

	cmpl->max_len = MAX(cmpl->max_len, len);
	cmpl->has_kind |= (kind_size != 0);
	popupmenu_append(&cmpl->pop, item);
}

void gui_completion_reset(struct gui *const gui)
{
	popupmenu_clear(&gui->completion->pop);
	gui->completion->has_kind = 0;
	gui->completion->max_len = 0;
}

static void completion_hide(struct popupmenu *const pop)
{
	struct completion *const cmpl = COMPLETION_GET(pop);
	evas_object_hide(cmpl->edje);
	edje_object_signal_emit(cmpl->edje, "eovim,completion,hide", "eovim");
}

static void completion_resize(struct popupmenu *const pop)
{
	struct completion *const cmpl = COMPLETION_GET(pop);
	if (pop->item_height <= 0)
		return;
	struct gui *const gui = pop->gui;

	/* Determine the maximum dimension of the completion menu */
	int win_width, win_height;
	evas_object_geometry_get(pop->gui->win, NULL, NULL, &win_width, &win_height);
	const int max_width = win_width - 16;
	const int max_height = (int)((float)win_height * 0.45f); /* 45% of total height */

	/* We will display at most 8 items */
	const int max_items = MIN(8, (int)pop->items_count);
	int height = pop->item_height * max_items + 2;
	if (height > max_height)
		height = max_height;

	/* Retrieve the geometry of the cell at which the completion must be displayed, and
	 * the overall size of the grid */
	int cx, cy, cw, ch;
	struct grid *const first_grid = eina_list_data_get(gui->grids); // XXX
	grid_cell_geometry_get(first_grid, cmpl->col, cmpl->row, &cx, &cy, &cw, &ch);
	unsigned int cols, rows;
	grid_size_get(first_grid, &cols, &rows);

	int xpos, ypos;

	xpos = cx;
	if (xpos < 0)
		xpos = 0;

	/* Determine whether the completion shall appear below or above the cursor */
	if (cmpl->row <= rows / 2)
		ypos = cy + ch + 2;
	else
		ypos = cy - height - 8;

	const int chars = cmpl->max_len + 1 + (cmpl->has_kind ? 2 : 0);
	int width = (chars + 4) * pop->cell_width; /* Add more chars for space */
	if (xpos + width > max_width)
		width = max_width - xpos;

	evas_object_size_hint_min_set(pop->spacer, width, height);
	evas_object_size_hint_max_set(pop->spacer, width, height);
	evas_object_move(cmpl->edje, xpos, ypos);
}

void gui_completion_show(struct gui *const gui, const unsigned int col, const unsigned int row)
{
	struct completion *const cmpl = gui->completion;
	struct popupmenu *const pop = &cmpl->pop;
	gui->active_popup = pop;

	cmpl->col = col;
	cmpl->row = row;

	evas_object_show(pop->table);
	evas_object_show(pop->genlist);
	evas_object_show(cmpl->edje);
	edje_object_signal_emit(cmpl->edje, "eovim,completion,show", "eovim");

	/* Trigger the resize (which will also place the popup */
	pop->iface->resize(pop);
}

static const struct popupmenu_interface completion_iface = {
	.hide = &completion_hide,
	.resize = &completion_resize,
};

struct completion *gui_completion_add(struct gui *const gui)
{
	struct completion *const cmpl = calloc(1, sizeof(*cmpl));
	if (EINA_UNLIKELY(!cmpl)) {
		CRI("Failed to allocate memory for completion popup");
		return NULL;
	}
	struct popupmenu *const pop = &cmpl->pop;
	const char *const edje_file = main_edje_file_get();

	popupmenu_setup(pop, gui, &completion_iface, _completion_itc);

	cmpl->edje = edje_object_add(gui->evas);
	edje_object_file_set(cmpl->edje, edje_file, "eovim/completion");
	evas_object_smart_member_add(cmpl->edje, gui->layout);
	edje_object_part_swallow(cmpl->edje, "eovim.completion", pop->table);
	return cmpl;
}

void gui_completion_del(struct completion *const cmpl)
{
	popupmenu_del(&cmpl->pop);
	free(cmpl);
}

void gui_completion_style_set(struct completion *const cmpl,
			      const Evas_Textblock_Style *const style, const unsigned int cell_w,
			      const unsigned int cell_h)
{
	popupmenu_style_changed(&cmpl->pop, style, cell_w, cell_h);
}

Eina_Bool gui_completion_init(void)
{
	_completion_itc = elm_genlist_item_class_new();
	if (EINA_UNLIKELY(!_completion_itc)) {
		CRI("Failed to create genlist item class");
		return EINA_FALSE;
	}
	_completion_itc->item_style = "full";
	_completion_itc->func.reusable_content_get = &completion_resuable_content_get;
	_completion_itc->func.del = &completion_item_del;
	return EINA_TRUE;
}

void gui_completion_shutdown(void)
{
	elm_genlist_item_class_free(_completion_itc);
}
