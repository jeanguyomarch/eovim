/* This file is part of Eovim, which is under the MIT License ****************/

#include "eovim/types.h"
#include "eovim/nvim.h"
#include "eovim/nvim_completion.h"
#include "eovim/nvim_event.h"
#include "eovim/msgpack_helper.h"
#include "eovim/gui.h"
#include "event/event.h"

/* Lookup table of stringshared keywords. At init time, it points to const
 * strings. After init, all entries will point to the associated stringshares
 */
Eina_Stringshare *nvim_event_keywords[__KW_LAST] =
{
  [KW_FOREGROUND] = "foreground",
  [KW_BACKGROUND] = "background",
  [KW_SPECIAL] = "special",
  [KW_REVERSE] = "reverse",
  [KW_ITALIC] = "italic",
  [KW_BOLD] = "bold",
  [KW_UNDERLINE] = "underline",
  [KW_UNDERCURL] = "undercurl",
  [KW_CURSOR_SHAPE] = "cursor_shape",
  [KW_CELL_PERCENTAGE] = "cell_percentage",
  [KW_BLINKWAIT] = "blinkwait",
  [KW_BLINKON] = "blinkon",
  [KW_BLINKOFF] = "blinkoff",
  [KW_HL_ID] = "hl_id",
  [KW_ID_LM] = "id_lm",
  [KW_NAME] = "name",
  [KW_SHORT_NAME] = "short_name",
  [KW_MOUSE_SHAPE] = "mouse_shape",
  [KW_MODE_NORMAL] = "normal",
  [KW_ARABICSHAPE] = "arabicshape",
  [KW_AMBIWIDTH] = "ambiwidth",
  [KW_EMOJI] = "emoji",
  [KW_GUIFONT] = "guifont",
  [KW_GUIFONTSET] = "guifontset",
  [KW_GUIFONTWIDE] = "guifontwide",
  [KW_LINESPACE] = "linespace",
  [KW_SHOWTABLINE] = "showtabline",
  [KW_TERMGUICOLORS] = "termguicolors",
  [KW_EXT_POPUPMENU] = "ext_popupmenu",
  [KW_EXT_TABLINE] = "ext_tabline",
  [KW_EXT_CMDLINE] = "ext_cmdline",
  [KW_EXT_WILDMENU] = "ext_wildmenu",
  [KW_EXT_LINEGRID] = "ext_linegrid",
  [KW_EXT_HLSTATE] = "ext_hlstate",
};

typedef struct
{
   Eina_Stringshare *name; /**< Name of the method */
   Eina_Hash *callbacks; /**< Table of callbacks associated with the method */
} s_method;

typedef enum
{
   E_METHOD_REDRAW, /**< The "redraw" method */
   E_METHOD_EOVIM, /**< The "eovim" method */

   __E_METHOD_LAST
} e_method;

/* Array of callbacks for each method. It is NOT a hash table as we will support
 * (for now) very little number of methods (around two). It is much faster to
 * search sequentially among arrays of few cells than a map of two. */
static s_method _methods[__E_METHOD_LAST];


static Eina_Bool
nvim_event_resize(s_nvim *nvim,
                  const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   t_int rows, columns;
   GET_ARG(params, 0, t_int, &columns);
   GET_ARG(params, 1, t_int, &rows);

   gui_resized_confirm(&nvim->gui, (unsigned int)columns, (unsigned int)rows);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_clear(s_nvim *nvim,
                 const msgpack_object_array *args EINA_UNUSED)
{
   gui_clear(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_flush(
  s_nvim *const nvim EINA_UNUSED,
  const msgpack_object_array *const args EINA_UNUSED)
{
  /* We just ignore the flush event. It tells the UI that neovim is done
   * drwaing the screen, but The EFL are alrady responsible of rendering what
   * changed. */
  return EINA_TRUE;
}

static Eina_Bool
nvim_event_eol_clear(s_nvim *nvim,
                     const msgpack_object_array *args EINA_UNUSED)
{
   gui_eol_clear(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_cursor_goto(s_nvim *nvim,
                       const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   t_int row, column;
   GET_ARG(params, 0, t_int, &row);
   GET_ARG(params, 1, t_int, &column);

   gui_cursor_goto(&nvim->gui, (unsigned int)column, (unsigned int)row);

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_update_menu(s_nvim *nvim EINA_UNUSED,
                       const msgpack_object_array *args EINA_UNUSED)
{
   DBG("Unimplemented");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_busy_start(s_nvim *nvim,
                      const msgpack_object_array *args EINA_UNUSED)
{
   gui_busy_set(&nvim->gui, EINA_TRUE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_busy_stop(s_nvim *nvim,
                     const msgpack_object_array *args EINA_UNUSED)
{
   gui_busy_set(&nvim->gui, EINA_FALSE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_mouse_on(s_nvim *nvim,
                    const msgpack_object_array *args EINA_UNUSED)
{
   nvim_mouse_enabled_set(nvim, EINA_TRUE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_mouse_off(s_nvim *nvim,
                     const msgpack_object_array *args EINA_UNUSED)
{
   nvim_mouse_enabled_set(nvim, EINA_FALSE);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_set_scroll_region(s_nvim *nvim,
                             const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, >=, 1);

   /* XXX It appears that under certain circumstantes, set_scroll_region can
    * receive multiple arguments:
    *
    *   ["set_scroll_region", [0, 39, 0, 118], [0, 39, 0, 118]]
    *
    * This is not part of the specification, but happens anyway. So be ready to
    * handle that case.
    */
   for (uint32_t i = 1u; i < args->size; i++)
   {
     const msgpack_object *const obj = &(args->ptr[i]);
     CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
     const msgpack_object_array *const params = &(obj->via.array);
     CHECK_ARGS_COUNT(params, ==, 4);

     t_int top, bot, left, right;
     GET_ARG(params, 0, t_int, &top);
     GET_ARG(params, 1, t_int, &bot);
     GET_ARG(params, 2, t_int, &left);
     GET_ARG(params, 3, t_int, &right);

     gui_scroll_region_set(&nvim->gui, (int)top, (int)bot, (int)left, (int)right);
   }

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_scroll(s_nvim *nvim,
                  const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, >=, 1);
   /*
    * It appears that in some cases (when the sh*tty command popup opens,
    * because a message is open in another castle), the scroll command changes,
    * and we get ["scroll", [N], [N]] instead of ["scroll", [N]].  I'm not sure
    * if this is supposed to happen, but that's what happens!  So we should be
    * ready to handle X arguments to "scroll" instead of just one
    */
   for (unsigned int i = 1; i < args->size; i++) /* 1 to exclude "scroll" */
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);

        t_int scroll;
        GET_ARG(arr, 0, t_int, &scroll);
        gui_scroll(&nvim->gui, (int)scroll);
     }

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_highlight_set(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, >=, 1);

   /* Array that holds pointer to values within hash tables.
    * We must ensure this array has been zeroes-out since we will rely on
    * NULL pointers to check whether a value was provided or not */
   const msgpack_object *objs[KW_HIGHLIGHT_END - KW_HIGHLIGHT_START + 1];
   memset(objs, 0, sizeof(objs));

   /*
    * highlight arguments are arrays containing maps.
    */
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);
        const msgpack_object *const arr_arg = &(arr->ptr[0]);
        CHECK_TYPE(arr_arg, MSGPACK_OBJECT_MAP, EINA_FALSE);

        /*
         * Okay, we now have a map. We go through all its key-value pairs
         * and match the keys with the expected keys. If we have a match,
         * we grab a pointer to the value, and pass on to the next pair.
         */
        const msgpack_object_map *const map = &(arr_arg->via.map);
        for (unsigned int j = 0; j < map->size; j++)
          {
             const msgpack_object_kv *const kv = &(map->ptr[j]);
             const msgpack_object *const key_obj = &(kv->key);
             CHECK_TYPE(key_obj, MSGPACK_OBJECT_STR, EINA_FALSE);
             const msgpack_object_str *const key = &(key_obj->via.str);
             Eina_Stringshare *const shr_key = eina_stringshare_add_length(
                key->ptr, key->size
             );
             if (EINA_UNLIKELY(! shr_key))
               {
                  CRI("Failed to create stringshare");
                  return EINA_FALSE;
               }

             /*
              * Keys matching. I think this is pretty efficient, as we are
              * comparing stringshares! So key matching is just a pointer
              * comparison. There are not much keys to be matched against
              * (< 10), so linear walk is good enough -- probably faster than
              * hashing.
              */
             for (unsigned int k = KW_HIGHLIGHT_START, x = 0; k <= KW_HIGHLIGHT_END; k++, x++)
               {
                  if (shr_key == KW(k))
                    {
                       objs[x] = &(kv->val);
                       break;
                    }
               }

             eina_stringshare_del(shr_key);
          }
     }

   s_termview_style style = {
      .fg_color = -1,
      .bg_color = -1,
      .sp_color = -1,
      .reverse = EINA_FALSE,
      .italic = EINA_FALSE,
      .bold = EINA_FALSE,
      .underline = EINA_FALSE,
      .undercurl = EINA_FALSE,
   };
   const msgpack_object *o;

   /*
    * At this point, we have collected everything we could. We check for
    * types to ensure that we will correctly use the memory.
    */

#define _GET_INT(Kw, Set)                                                     \
   if ((o = objs[Kw])) {                                                      \
        if (EINA_UNLIKELY((o->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&     \
                          (o->type != MSGPACK_OBJECT_NEGATIVE_INTEGER)))      \
          CRI("Expected an integer type. Got 0x%x", o->type);                 \
        else Set = o->via.i64;                                                \
   }

#define _GET_BOOL(Kw, Set)                                                    \
   if ((o = objs[Kw])) {                                                      \
        if (EINA_UNLIKELY(o->type != MSGPACK_OBJECT_BOOLEAN))                 \
          CRI("Expected an integer type. Got 0x%x", o->type);                 \
        else Set = o->via.boolean;                                            \
   }

   _GET_INT(KW_FOREGROUND, style.fg_color);
   _GET_INT(KW_BACKGROUND, style.bg_color);
   _GET_INT(KW_SPECIAL, style.sp_color);
   _GET_BOOL(KW_REVERSE, style.reverse);
   _GET_BOOL(KW_ITALIC, style.italic);
   _GET_BOOL(KW_BOLD, style.bold);
   _GET_BOOL(KW_UNDERLINE, style.underline);
   _GET_BOOL(KW_UNDERCURL, style.undercurl);

#undef _GET_INT
#undef _GET_BOOL

   gui_style_set(&nvim->gui, &style);

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_put(s_nvim *nvim,
               const msgpack_object_array *args)
{
   /*
    * The 'put' command is much more complicated than what it should be.
    * We are suppose to receive a string, but we actually do receive arrays
    * of strings.
    */
   CHECK_BASE_ARGS_COUNT(args, >=, 1);

   /* Reset the put decode buffer */
   eina_ustrbuf_reset(nvim->decode);

   /*
    * Go through all the arguments. Each argument is an array that contains
    * exactly one string. We concatenate all these strings together in one
    * static buffer (this is always called from the main thread, so no
    * concurrency shit).
    */
   for (unsigned int i = 1; i < args->size; i++)
     {
        const msgpack_object *const obj = &(args->ptr[i]);
        CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const arr = &(obj->via.array);
        CHECK_ARGS_COUNT(arr, ==, 1);
        const msgpack_object *const arr_arg = &(arr->ptr[0]);
        CHECK_TYPE(arr_arg, MSGPACK_OBJECT_STR, EINA_FALSE);

        const msgpack_object_str *const str = &(arr_arg->via.str);
        int index = 0;
        const Eina_Unicode cp = eina_unicode_utf8_next_get(str->ptr, &index);
        if (EINA_UNLIKELY(cp == 0))
          {
             ERR("Failed to decode utf-8 string. Skipping.");
             continue;
          }
        eina_ustrbuf_append_char(nvim->decode, cp);
     }

   /* Finally, pass the string and its length to the gui */
   gui_put(&nvim->gui,
           eina_ustrbuf_string_get(nvim->decode),
           (unsigned int)eina_ustrbuf_length_get(nvim->decode));

   return EINA_TRUE;
}

static Eina_Bool
nvim_event_bell(s_nvim *nvim,
                const msgpack_object_array *args EINA_UNUSED)
{
   gui_bell_ring(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_visual_bell(s_nvim *nvim EINA_UNUSED,
                       const msgpack_object_array *args EINA_UNUSED)
{
   DBG("Unimplemented");
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_update_fg(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int color;
   GET_ARG(params, 0, t_int, &color);
   gui_update_fg(&nvim->gui, color);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_update_bg(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int color;
   GET_ARG(params, 0, t_int, &color);
   gui_update_bg(&nvim->gui, color);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_update_sp(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int color;
   GET_ARG(params, 0, t_int, &color);
   gui_update_sp(&nvim->gui, color);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_suspend(s_nvim *nvim EINA_UNUSED,
                   const msgpack_object_array *args EINA_UNUSED)
{
   /* Nothing to do */
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_set_title(s_nvim *nvim,
                     const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   Eina_Stringshare *const title =
      EOVIM_MSGPACK_STRING_EXTRACT(&params->ptr[0], fail);
   gui_title_set(&nvim->gui, title);
   eina_stringshare_del(title);

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static Eina_Bool
nvim_event_set_icon(s_nvim *nvim EINA_UNUSED,
                    const msgpack_object_array *args EINA_UNUSED)
{
   /* Do nothing. Seems it can be safely ignored. */
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_popupmenu_show(s_nvim *nvim,
                          const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, >=, 4);

   const msgpack_object *const data_obj = &(params->ptr[0]);
   CHECK_TYPE(data_obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
   const msgpack_object_array *const data = &(data_obj->via.array);
   s_gui *const gui = &nvim->gui;

   t_int selected, row, col;
   GET_ARG(params, 1, t_int, &selected);
   GET_ARG(params, 2, t_int, &row);
   GET_ARG(params, 3, t_int, &col);

   gui_completion_prepare(gui, data->size);

   size_t max_len_word = 0u;
   size_t max_len_menu = 0u;
   for (unsigned int i = 0u; i < data->size; i++)
     {
        CHECK_TYPE(&data->ptr[i], MSGPACK_OBJECT_ARRAY, EINA_FALSE);
        const msgpack_object_array *const completion = &(data->ptr[i].via.array);
        CHECK_ARGS_COUNT(completion, ==, 4);

        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[0], fail); /* word */
        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[1], fail); /* kind */
        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[2], fail); /* menu */
        EOVIM_MSGPACK_STRING_CHECK(&completion->ptr[3], fail); /* info */

        s_completion *const compl = nvim_completion_new(
           completion->ptr[0].via.str.ptr, completion->ptr[0].via.str.size,
           completion->ptr[1].via.str.ptr, completion->ptr[1].via.str.size,
           completion->ptr[2].via.str.ptr, completion->ptr[2].via.str.size,
           completion->ptr[3].via.str.ptr, completion->ptr[3].via.str.size
        );
        if (EINA_LIKELY(compl != NULL))
          {
             /* FIXME - This is obviously wrong... the count of bytes does not
              * equal to the number of characters...  */
             max_len_word = MAX(max_len_word, completion->ptr[0].via.str.size);
             max_len_menu = MAX(max_len_menu, completion->ptr[2].via.str.size);

             gui_completion_add(gui, compl);
          }
     }

   gui_completion_show(gui, max_len_word, max_len_menu, (int)selected,
                       (unsigned int)col, (unsigned int)row);

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

static Eina_Bool
nvim_event_popupmenu_hide(s_nvim *nvim,
                          const msgpack_object_array *args EINA_UNUSED)
{
   gui_completion_hide(&nvim->gui);
   gui_completion_clear(&nvim->gui);
   return EINA_TRUE;
}

static Eina_Bool
nvim_event_popupmenu_select(s_nvim *nvim,
                            const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   t_int selected;
   GET_ARG(params, 0, t_int, &selected);

   gui_completion_selected_set(&nvim->gui, (int)selected);

   return EINA_TRUE;
}

static Eina_Bool
_tab_index_get(const msgpack_object *obj,
               uint8_t *index)
{
   /* It shall be of EXT type */
   CHECK_TYPE(obj, MSGPACK_OBJECT_EXT, EINA_FALSE);

   /* EXT type shall have subtype 2 (INT >= 0) */
   const msgpack_object_ext *const o = &(obj->via.ext);
   CHECK_TYPE(o, MSGPACK_OBJECT_POSITIVE_INTEGER, EINA_FALSE);

   /* I only handle indexes of size 1 byte */
   if (EINA_UNLIKELY(o->size != 1))
     {
        CRI("Oops. I received a integer of %"PRIu32" bytes. I don't know "
            "how to handle that!", o->size);
        return EINA_FALSE;
     }

   /* Get the index, on one byte */
   *index = (uint8_t)(o->ptr[0]);
   return EINA_TRUE;
}

#define TAB_INDEX_GET(Obj)                                              \
   ({ uint8_t tab_index___;                                             \
      if (EINA_UNLIKELY(! _tab_index_get(Obj, &tab_index___)))          \
      { return EINA_FALSE; }                                            \
      tab_index___;                                                     \
   })


static Eina_Bool
nvim_event_tabline_update(s_nvim *nvim,
                          const msgpack_object_array *args)
{
   /*
    * The tabline update message accepts an array of two arguments
    * - the active tab of type EXT:2 (POSITIVE INT).
    * - the complete list of tabs, with a key "tab" which is the index
    *   and a key "name" which is the title of the tab
    */
    //   [{"tab"=>(ext: 2)"\x02", "name"=>"b"}, {"tab"=>(ext: 2)"\x01", "name"=>"a"}]
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   s_gui *const gui = &nvim->gui;

   /* Get the currently active tab */
   const msgpack_object *const o_sel = &(params->ptr[0]);
   const uint8_t current = TAB_INDEX_GET(o_sel);

   ARRAY_OF_ARGS_EXTRACT(params, tabs);

   gui_tabs_reset(gui);

   /* If we have no tabs or just one, we consider we have no tab at all,
    * and update the UI accordingly. And we stop here. */
   if (tabs->size <= 1)
     {
        gui_tabs_hide(gui);
        return EINA_TRUE;
     }

   gui_tabs_show(gui);

   for (unsigned int i = 0; i < tabs->size; i++)
     {
        const msgpack_object *const o_tab = &tabs->ptr[i];
        const msgpack_object_map *const map =
           EOVIM_MSGPACK_MAP_EXTRACT(o_tab, fail);
        const msgpack_object *o_key, *o_val;
        unsigned int it;

        uint8_t tab_id = UINT8_MAX;
        Eina_Stringshare *tab_name = NULL;

        EOVIM_MSGPACK_MAP_ITER(map, it, o_key, o_val)
          {
             const msgpack_object_str *const key =
                EOVIM_MSGPACK_STRING_OBJ_EXTRACT(o_key, fail);
             if (! strncmp(key->ptr, "tab", key->size))
               tab_id = TAB_INDEX_GET(o_val);
             else if (! strncmp(key->ptr, "name", key->size))
               tab_name = EOVIM_MSGPACK_STRING_EXTRACT(o_val, fail);
             else
               ERR("Invalid key name");
          }

        if ((! tab_name) || (tab_id == UINT8_MAX))
          {
             ERR("Failed to extract tab information");
             continue;
          }
        gui_tabs_add(gui, tab_name, tab_id,
                     (tab_id == current) ? EINA_TRUE : EINA_FALSE);
     }

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}


Eina_Bool
nvim_event_dispatch(s_nvim *nvim,
                    Eina_Stringshare *method_name,
                    Eina_Stringshare *command,
                    const msgpack_object_array *args)
{
   /* Go sequentially through the list of methods we know about, so we can
    * find out the callbacks table for that method, to try to find what matches
    * 'command'. */
   for (size_t i = 0u; i < EINA_C_ARRAY_LENGTH(_methods); i++)
     {
        const s_method *const method = &(_methods[i]);
        if (method->name == method_name) /* Fond the method */
          {
             /* Grab the callback for the command. If we could find none,
              * that's an error. Otherwise we call it. In both cases, the
              * execution of the function will be terminated. */
             const f_event_cb cb = eina_hash_find(method->callbacks, command);
             if (EINA_UNLIKELY(! cb))
               {
                  WRN("Failed to get callback for command '%s' of method '%s'",
                      command, method_name);
                  return EINA_FALSE;
               }
             else
               return cb(nvim, args);
          }
     }

   /* At this point, we didn't find the method. */
   WRN("Unknown method '%s'", method_name);
   return EINA_FALSE;
}


typedef struct
{
   const char *const name; /**< Name of the event */
   const unsigned int size; /**< Size of @p name */
   const f_event_cb func; /**< Callback function */
} s_method_ctor;

#define CB_CTOR(Name, Func) \
{ .name = (Name), .size = sizeof(Name) - 1, .func = (Func) }

static Eina_Bool
_command_add(Eina_Hash *table,
             const char *name,
             unsigned int name_len,
             f_event_cb func)
{
   /* Create the key to be inserted in the table */
   Eina_Stringshare *const cmd = eina_stringshare_add_length(
      name, name_len
   );
   if (EINA_UNLIKELY(! cmd))
     {
        CRI("Failed to create stringshare from '%s', size %u",
            name, name_len);
        goto fail;
     }

   /* Add the value 'func' to the key 'cmd' in the table */
   if (EINA_UNLIKELY(! eina_hash_add(table, cmd, func)))
     {
        CRI("Failed to register callback %p for command '%s'",
            func, cmd);
        goto add_fail;
     }

   return EINA_TRUE;
add_fail:
   eina_stringshare_del(cmd);
fail:
   return EINA_FALSE;
}

static Eina_Hash *
_method_callbacks_table_build(const s_method_ctor *ctors,
                              unsigned int ctors_count)
{
   /* Generate a hash table that will contain the callbacks to be called for
    * each event sent by neovim. */
   Eina_Hash *const cb = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(! cb))
     {
        CRI("Failed to create hash table to hold callbacks");
        goto fail;
     }

   /* Add the events in the callbacks table. */
   for (unsigned int i = 0; i < ctors_count; i++)
     {
        const Eina_Bool ok =
           _command_add(cb, ctors[i].name, ctors[i].size, ctors[i].func);
        if (EINA_UNLIKELY(! ok))
          {
             CRI("Failed to register command '%s'", ctors[i].name);
             goto hash_free;
          }
     }

   return cb;
hash_free:
   eina_hash_free(cb);
fail:
   return NULL;
}

static void
_method_free(s_method *method)
{
   /* This function is always used on statically-allocated containers.
    * As such, free() should not be called on 'method' */
   if (method->name) eina_stringshare_del(method->name);
   if (method->callbacks) eina_hash_free(method->callbacks);
}

static Eina_Bool
_method_redraw_init(e_method method_id)
{
   s_method *const method = &(_methods[method_id]);
   const s_method_ctor ctors[] = {
      CB_CTOR("resize", nvim_event_resize),
      CB_CTOR("clear", nvim_event_clear),
      CB_CTOR("eol_clear", nvim_event_eol_clear),
      CB_CTOR("cursor_goto", nvim_event_cursor_goto),
      CB_CTOR("mode_info_set", nvim_event_mode_info_set),
      CB_CTOR("update_menu", nvim_event_update_menu),
      CB_CTOR("busy_start", nvim_event_busy_start),
      CB_CTOR("busy_stop", nvim_event_busy_stop),
      CB_CTOR("mouse_on", nvim_event_mouse_on),
      CB_CTOR("mouse_off", nvim_event_mouse_off),
      CB_CTOR("mode_change", nvim_event_mode_change),
      CB_CTOR("set_scroll_region", nvim_event_set_scroll_region),
      CB_CTOR("scroll", nvim_event_scroll),
      CB_CTOR("highlight_set", nvim_event_highlight_set),
      CB_CTOR("put", nvim_event_put),
      CB_CTOR("bell", nvim_event_bell),
      CB_CTOR("visual_bell", nvim_event_visual_bell),
      CB_CTOR("update_fg", nvim_event_update_fg),
      CB_CTOR("update_bg", nvim_event_update_bg),
      CB_CTOR("update_sp", nvim_event_update_sp),
      CB_CTOR("suspend", nvim_event_suspend),
      CB_CTOR("set_title", nvim_event_set_title),
      CB_CTOR("set_icon", nvim_event_set_icon),
      CB_CTOR("popupmenu_show", nvim_event_popupmenu_show),
      CB_CTOR("popupmenu_hide", nvim_event_popupmenu_hide),
      CB_CTOR("popupmenu_select", nvim_event_popupmenu_select),
      CB_CTOR("tabline_update", nvim_event_tabline_update),
      CB_CTOR("cmdline_show", nvim_event_cmdline_show),
      CB_CTOR("cmdline_pos", nvim_event_cmdline_pos),
      CB_CTOR("cmdline_special_char", nvim_event_cmdline_special_char),
      CB_CTOR("cmdline_hide", nvim_event_cmdline_hide),
      CB_CTOR("cmdline_block_show", nvim_event_cmdline_block_show),
      CB_CTOR("cmdline_block_append", nvim_event_cmdline_block_append),
      CB_CTOR("cmdline_block_hide", nvim_event_cmdline_block_hide),
      CB_CTOR("wildmenu_show", nvim_event_wildmenu_show),
      CB_CTOR("wildmenu_hide", nvim_event_wildmenu_hide),
      CB_CTOR("wildmenu_select", nvim_event_wildmenu_select),
      CB_CTOR("option_set", nvim_event_option_set),
      CB_CTOR("flush", nvim_event_flush),
   };

   /* Register the name of the method as a stringshare */
   const char name[] = "redraw";
   method->name = eina_stringshare_add_length(name, sizeof(name) - 1);
   if (EINA_UNLIKELY(! method->name))
     {
        CRI("Failed to create stringshare from string '%s'", name);
        goto fail;
     }

   /* Build the callbacks table of the method */
   method->callbacks =
      _method_callbacks_table_build(ctors, EINA_C_ARRAY_LENGTH(ctors));
   if (EINA_UNLIKELY(! method->callbacks))
     {
        CRI("Failed to create table of callbacks");
        goto free_name;
     }

   return EINA_TRUE;
free_name:
   eina_stringshare_del(method->name);
fail:
   return EINA_FALSE;
}

static Eina_Bool
_method_eovim_init(e_method method_id)
{
   s_method *const method = &(_methods[method_id]);

   const s_method_ctor ctors[] = {
     CB_CTOR("reload", nvim_event_eovim_reload),
   };

   /* Register the name of the method as a stringshare */
   const char name[] = "eovim";
   method->name = eina_stringshare_add_length(name, sizeof(name) - 1);
   if (EINA_UNLIKELY(! method->name))
     {
        CRI("Failed to create stringshare from string '%s'", name);
        goto fail;
     }

   /* Create an empty the callbacks table of the method */
   method->callbacks =
      _method_callbacks_table_build(ctors, EINA_C_ARRAY_LENGTH(ctors));
   if (EINA_UNLIKELY(! method->callbacks))
     {
        CRI("Failed to create table of callbacks");
        goto free_name;
     }

   return EINA_TRUE;
free_name:
   eina_stringshare_del(method->name);
fail:
   return EINA_FALSE;
}

Eina_Bool
nvim_event_init(void)
{
   int i;
   for (i = 0; i < __KW_LAST; i++)
     {
        nvim_event_keywords[i] = eina_stringshare_add(nvim_event_keywords[i]);
        if (EINA_UNLIKELY(! nvim_event_keywords[i]))
          {
             CRI("Failed to create stringshare");
             goto fail;
          }
     }

   /* Initialize the "redraw" method */
   if (EINA_UNLIKELY(! _method_redraw_init(E_METHOD_REDRAW)))
     {
        CRI("Failed to setup the redraw method");
        goto del_methods;
     }

   /* Initialize the "eovim" method */
   if (EINA_UNLIKELY(! _method_eovim_init(E_METHOD_EOVIM)))
     {
        CRI("Failed to setup the eovim method");
        goto del_methods;
     }

   /* Initialize the internals of 'mode_info_set' */
   if (EINA_UNLIKELY(! mode_init()))
     {
        CRI("Failed to initialize mode internals");
        goto del_methods;
     }

   /* Initialize the internals of option_set */
   if (EINA_UNLIKELY(! option_set_init()))
     {
        CRI("Failed to initialize 'option_set'");
        goto mode_deinit;
     }

   return EINA_TRUE;

mode_deinit:
   mode_shutdown();
del_methods:
   for (size_t j = 0u; j < EINA_C_ARRAY_LENGTH(_methods); j++)
     _method_free(&(_methods[j]));
fail:
   for (i--; i >= 0; i--)
     eina_stringshare_del(nvim_event_keywords[i]);
   return EINA_FALSE;
}

void
nvim_event_shutdown(void)
{
   mode_shutdown();
   option_set_shutdown();
   for (unsigned int i = 0; i < __KW_LAST; i++)
     eina_stringshare_del(nvim_event_keywords[i]);
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_methods); i++)
     _method_free(&(_methods[i]));
}
