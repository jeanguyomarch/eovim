/*
 * Copyright (c) 2018-2019 Jean Guyomarc'h
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

typedef enum
{
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
#define CHECK_BASE_ARGS_COUNT(Args, Op, Count) \
   if (EINA_UNLIKELY(! ((Args)->size - 1 Op (Count)))) { \
      CRI("Invalid argument count. (%u %s %u) is false", \
          (Args)->size - 1, # Op, (Count)); \
      return EINA_FALSE; \
   }

#define CHECK_ARGS_COUNT(Args, Op, Count) \
   if (EINA_UNLIKELY(! ((Args)->size Op (Count)))) { \
      CRI("Invalid argument count. (%u %s %u) is false", \
          (Args)->size, # Op, (Count)); \
      return EINA_FALSE; \
   }

#define GET_ARG(Args, Index, Type, Get) \
   if (EINA_UNLIKELY(! _arg_ ## Type ## _get((Args), (Index), (Get)))) { \
      return EINA_FALSE; \
   }

#define ARRAY_OF_ARGS_EXTRACT(Args, Ret) \
   const msgpack_object_array *const Ret = _array_of_args_extract(Args); \
   if (EINA_UNLIKELY(! Ret)) { return EINA_FALSE; }

#define CHECK_TYPE(Obj, Type, ...) \
   if (EINA_UNLIKELY((Obj)->type != Type)) { \
      CRI("Expected type 0x%x. Got 0x%x", Type, (Obj)->type); \
      return __VA_ARGS__; \
   }


static inline const msgpack_object_array *
_array_of_args_extract(const msgpack_object_array *args)
{
   const msgpack_object *const obj = &(args->ptr[1]);
   CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, NULL);
   return &(obj->via.array);
}

static inline Eina_Bool
_arg_t_int_get(const msgpack_object_array *args,
               unsigned int index,
               t_int *arg)
{
   const msgpack_object *const obj = &(args->ptr[index]);
   if (EINA_UNLIKELY((obj->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&
                     (obj->type != MSGPACK_OBJECT_NEGATIVE_INTEGER)))
     {
        CRI("Expected an integer type for argument %u. Got 0x%x",
            index, obj->type);
        return EINA_FALSE;
     }
   *arg = obj->via.i64;
   return EINA_TRUE;
}

static inline Eina_Bool
_arg_stringshare_get(const msgpack_object_array *args,
                     unsigned int index,
                     Eina_Stringshare **arg)
{
   const msgpack_object *const obj = &(args->ptr[index]);
   CHECK_TYPE(obj, MSGPACK_OBJECT_STR, EINA_FALSE);
   const msgpack_object_str *const str = &(obj->via.str);
   *arg = eina_stringshare_add_length(str->ptr, str->size);
   if (EINA_UNLIKELY(! *arg))
     {
        CRI("Failed to create stringshare");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static inline Eina_Bool
_arg_bool_get(const msgpack_object_array *args,
              unsigned int index,
              Eina_Bool *arg)
{
   const msgpack_object *const obj = &(args->ptr[index]);
   CHECK_TYPE(obj, MSGPACK_OBJECT_BOOLEAN, EINA_FALSE);
   *arg = obj->via.boolean;
   return EINA_TRUE;
}


/*****************************************************************************/

Eina_Bool nvim_event_cmdline_show(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_pos(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_special_char(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_hide(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_block_show(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_block_append(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_cmdline_block_hide(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_wildmenu_show(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_wildmenu_hide(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_wildmenu_select(s_nvim *nvim, const msgpack_object_array *args);

/*****************************************************************************/

Eina_Bool mode_init(void);
void mode_shutdown(void);
Eina_Bool nvim_event_mode_info_set(s_nvim *nvim, const msgpack_object_array *args);
Eina_Bool nvim_event_mode_change(s_nvim *nvim, const msgpack_object_array *args);

/*****************************************************************************/

Eina_Bool option_set_init(void);
void option_set_shutdown(void);
Eina_Bool nvim_event_option_set(s_nvim *nvim, const msgpack_object_array *args);

/*****************************************************************************/

Eina_Bool nvim_event_eovim_reload(s_nvim *nvim, const msgpack_object_array *args);

#endif /* ! EOVIM_EVENT_H__ */
