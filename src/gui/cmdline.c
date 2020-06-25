/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/types.h>
#include <eovim/log.h>
#include <eovim/nvim.h>
#include <eovim/gui.h>
#include <eovim/termview.h>

#include "gui_private.h"

static const char CMDLINE_TEXT_PART[] = "eovim.cmdline:eovim.cmdline.text";
static const char CMDLINE_INFO_TEXT_PART[] = "eovim.cmdline_info:text";

struct cmdline {
	struct gui *gui;
	Eina_Strbuf *buf;
	Eina_Bool enabled;
};

static void style_apply(struct gui *const gui, const char *const part, const union color fg)
{
	Eina_Strbuf *const buf = gui->cmdline->buf;
	eina_strbuf_reset(buf);
	eina_strbuf_append_printf(buf, "DEFAULT='color=#%06" PRIx32 " font=\\'%s\\' font_size=%u'",
				  fg.value & 0xFFFFFF, gui->font.name, gui->font.size);
	edje_object_part_text_style_user_pop(gui->edje, part);
	edje_object_part_text_style_user_push(gui->edje, part, eina_strbuf_string_get(buf));
}

void gui_cmdline_show(struct gui *const gui, const char *const content,
		      Eina_Stringshare *const prompt, Eina_Stringshare *const firstc)
{
	EINA_SAFETY_ON_NULL_RETURN(firstc);
	struct nvim *const nvim = gui->nvim;

	const Eina_Bool use_prompt = (firstc[0] == '\0');
	const char *const prompt_signal =
		(use_prompt) ? "eovim,cmdline,prompt,custom" : "eovim,cmdline,prompt,builtin";

	Eina_Stringshare *hi_group = eina_hash_find(nvim->cmdline_styles, firstc);
	if (!hi_group) {
		WRN("No cmdline style for firstc '%s'", firstc);
		hi_group = eina_hash_find(nvim->cmdline_styles, "default");
		if (EINA_UNLIKELY(!hi_group)) {
			ERR("Failed to find 'default' hi_group");
			goto end;
		}
	}

	const struct termview_style *const style = eina_hash_find(nvim->hl_groups, hi_group);
	if (EINA_UNLIKELY(!style)) {
		ERR("Failed to find group for '%s'", hi_group);
		goto end;
	}

	style_apply(gui, CMDLINE_TEXT_PART, gui->default_fg);
	style_apply(gui, CMDLINE_INFO_TEXT_PART, style->fg_color);

	color_class_set("eovim.cmdline.info_bg", style->bg_color);

end:
	elm_layout_signal_emit(gui->layout, prompt_signal, "eovim");
	edje_object_part_text_unescaped_set(gui->edje, "eovim.cmdline:eovim.cmdline.text", content);
	edje_object_part_text_unescaped_set(gui->edje, "eovim.cmdline_info:text",
					    (use_prompt) ? prompt : firstc);

	/* Show the completion panel */
	if (!gui->cmdline->enabled) {
		elm_layout_signal_emit(gui->layout, "eovim,cmdline,show", "eovim");
		gui->cmdline->enabled = EINA_TRUE;
	}
}

static void cmdline_shown_cb(void *const data, Evas_Object *const obj EINA_UNUSED,
			     const char *const emission EINA_UNUSED,
			     const char *const source EINA_UNUSED)
{
	/* Once the command-line has been shown, place the cursor at the first position */
	const struct cmdline *const cmdline = data;
	gui_cmdline_cursor_pos_set(cmdline->gui, 0);
}

void gui_cmdline_hide(struct gui *const gui)
{
	elm_layout_signal_emit(gui->layout, "eovim,cmdline,hide", "eovim");
	gui->cmdline->enabled = EINA_FALSE;
}

Eina_Bool gui_cmdline_enabled_get(const struct gui *const gui)
{
	return gui->cmdline->enabled;
}

void gui_cmdline_cursor_pos_set(struct gui *const gui, const size_t pos)
{
	edje_object_part_text_cursor_pos_set(gui->edje, CMDLINE_TEXT_PART, EDJE_CURSOR_MAIN,
					     (int)pos);

	int ox, oy, cx, cy, cw, ch;
	edje_object_part_geometry_get(gui->edje, "eovim.cmdline", &ox, &oy, NULL, NULL);
	edje_object_part_text_cursor_geometry_get(gui->edje, CMDLINE_TEXT_PART, &cx, &cy, &cw, &ch);
	gui_cursor_calc(gui, ox + cx, oy + cy, cw, ch);
}

struct cmdline *cmdline_add(struct gui *const gui)
{
	struct cmdline *const cmd = calloc(1, sizeof(*cmd));
	if (EINA_UNLIKELY(!cmd)) {
		CRI("Failed to allocate memory");
		return NULL;
	}
	cmd->gui = gui;
	cmd->buf = eina_strbuf_new();
	elm_layout_signal_callback_add(gui->layout, "eovim,cmdline,shown", "eovim",
				       &cmdline_shown_cb, cmd);
	return cmd;
}

void cmdline_del(struct cmdline *const cmd)
{
	eina_strbuf_free(cmd->buf);
	free(cmd);
}
