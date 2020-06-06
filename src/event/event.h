/* This file is part of Eovim, which is under the MIT License ****************/

/**
 * @file event.h
 *
 * This is an internal header that is shared among the event source
 * files, but cannot (should not) be accessed from other modules.
 */
#ifndef EOVIM_EVENT_H__
#define EOVIM_EVENT_H__

#include "eovim/types.h"
#include "eovim/nvim.h"
#include "eovim/nvim_event.h"
#include "eovim/msgpack_helper.h"
#include "eovim/log.h"

typedef enum {
	/* Highlight keywords */
	KW_FOREGROUND,
	KW_BACKGROUND,
	KW_SPECIAL,
	KW_REVERSE,
	KW_ITALIC,
	KW_BOLD,
	KW_UNDERLINE,
	KW_UNDERCURL,

	/* Mode info set keywords */
	KW_CURSOR_SHAPE,
	KW_CELL_PERCENTAGE,
	KW_BLINKWAIT,
	KW_BLINKON,
	KW_BLINKOFF,
	KW_HL_ID,
	KW_ID_LM,
	KW_NAME,
	KW_SHORT_NAME,
	KW_MOUSE_SHAPE,

	/* 'option_set' keywords */
	KW_ARABICSHAPE,
	KW_AMBIWIDTH,
	KW_EMOJI,
	KW_GUIFONT,
	KW_GUIFONTSET,
	KW_GUIFONTWIDE,
	KW_LINESPACE,
	KW_SHOWTABLINE,
	KW_TERMGUICOLORS,
	KW_EXT_POPUPMENU,
	KW_EXT_TABLINE,
	KW_EXT_CMDLINE,
	KW_EXT_WILDMENU,
	KW_EXT_LINEGRID,
	KW_EXT_HLSTATE,

	KW_MODE_NORMAL,

	__KW_LAST,

	/* Aliases */
	KW_HIGHLIGHT_START = KW_FOREGROUND,
	KW_HIGHLIGHT_END = KW_UNDERCURL,
	KW_MODE_INFO_START = KW_CURSOR_SHAPE,
	KW_MODE_INFO_END = KW_MOUSE_SHAPE,
	KW_MODES_START = KW_MODE_NORMAL,
	KW_MODES_END = KW_MODE_NORMAL,
} e_kw;

/**
 * Global (shall be read-only after eovim's init. Contains an global of
 * stringshared data that is used to do very fast string comparison.
 */
extern Eina_Stringshare *nvim_event_keywords[__KW_LAST];

/**
 * RO-accessor to nvim_event_keywors.
 */
#define KW(Name) nvim_event_keywords[(Name)]

/*
 * When checking the count of args, we have to subtract 1 from the total
 * args count of the object, as the first argument is the command name
 * itself.
 */
#define CHECK_BASE_ARGS_COUNT(Args, Op, Count)                                                     \
	if (EINA_UNLIKELY(!((Args)->size - 1 Op(Count)))) {                                        \
		CRI("Invalid argument count. (%u %s %u) is false", (Args)->size - 1, #Op,          \
		    (Count));                                                                      \
		return EINA_FALSE;                                                                 \
	}

#define CHECK_ARGS_COUNT(Args, Op, Count)                                                          \
	if (EINA_UNLIKELY(!((Args)->size Op(Count)))) {                                            \
		CRI("Invalid argument count. (%u %s %u) is false", (Args)->size, #Op, (Count));    \
		return EINA_FALSE;                                                                 \
	}

#define GET_ARG(Args, Index, Type, Get)                                                            \
	do {                                                                                       \
		if (EINA_UNLIKELY((Index) >= (Args)->size)) {                                      \
			ERR("Out of bounds index");                                                \
			return EINA_FALSE;                                                         \
		}                                                                                  \
		if (EINA_UNLIKELY(!arg_##Type##_get(&((Args)->ptr[Index]), (Get)))) {              \
			return EINA_FALSE;                                                         \
		}                                                                                  \
	} while (0)

#define ARRAY_OF_ARGS_EXTRACT(Args, Ret)                                                           \
	const msgpack_object_array *const Ret = array_of_args_extract((Args), 1);                  \
	if (EINA_UNLIKELY(!Ret)) {                                                                 \
		return EINA_FALSE;                                                                 \
	}

#define CHECK_TYPE(Obj, Type, ...)                                                                 \
	if (EINA_UNLIKELY((Obj)->type != Type)) {                                                  \
		CRI("Expected type 0x%x. Got 0x%x", Type, (Obj)->type);                            \
		return __VA_ARGS__;                                                                \
	}

const msgpack_object_array *array_of_args_extract(const msgpack_object_array *args,
						  size_t at_index);
Eina_Bool arg_t_int_get(const msgpack_object *obj, t_int *arg);
Eina_Bool arg_uint_get(const msgpack_object *obj, unsigned int *arg);
Eina_Bool arg_color_get(const msgpack_object *obj, union color *arg);
Eina_Bool arg_stringshare_get(const msgpack_object *obj, Eina_Stringshare **arg);
Eina_Bool arg_bool_get(const msgpack_object *obj, Eina_Bool *arg);

/*****************************************************************************/

Eina_Bool nvim_event_cmdline_show(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_pos(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_special_char(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_hide(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_block_show(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_block_append(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_block_hide(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_wildmenu_show(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_wildmenu_hide(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_wildmenu_select(struct nvim *nvim, const msgpack_object_array *args);

/*****************************************************************************/

Eina_Bool mode_init(void);
void mode_shutdown(void);
Eina_Bool nvim_event_mode_info_set(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_mode_change(struct nvim *nvim, const msgpack_object_array *args);

/*****************************************************************************/

Eina_Bool option_set_init(void);
void option_set_shutdown(void);
Eina_Bool nvim_event_option_set(struct nvim *nvim, const msgpack_object_array *args);

/*****************************************************************************/

Eina_Bool event_linegrid_init(void);
void event_linegrid_shutdown(void);
Eina_Bool nvim_event_default_colors_set(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_hl_attr_define(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_hl_group_set(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_grid_resize(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_grid_clear(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_grid_cursor_goto(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_grid_line(struct nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_grid_scroll(struct nvim *nvim, const msgpack_object_array *args);

/*****************************************************************************/

Eina_Bool nvim_event_eovim_reload(struct nvim *nvim, const msgpack_object_array *args);

#endif /* ! EOVIM_EVENT_H__ */
