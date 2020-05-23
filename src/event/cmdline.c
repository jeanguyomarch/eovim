/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

Eina_Bool
nvim_event_cmdline_show(s_nvim *nvim,
                        const msgpack_object_array *args)
{
   Eina_Bool ret = EINA_FALSE;
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 6);

   /*
    * The arguments of cmdline_show are:
    *
    * [0]: content
    * [1]: cursor position (int)
    * [2]: first character (string)
    * [3]: prompt (string)
    * [4]: indentation (int)
    * [5]: level (int)
    */
   const msgpack_object_array *const content =
      EOVIM_MSGPACK_ARRAY_EXTRACT(&params->ptr[0], fail);
   const int64_t pos =
      EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[1], fail);
   Eina_Stringshare *const firstc =
      EOVIM_MSGPACK_STRING_EXTRACT(&params->ptr[2], fail);
   Eina_Stringshare *const prompt =
      EOVIM_MSGPACK_STRING_EXTRACT(&params->ptr[3], del_firstc);
   const int64_t indent =
      EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[4], del_prompt);
   //const int64_t level =
   //   EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[5], del_prompt);

   /* Create the string buffer, which will hold the content of the cmdline */
   Eina_Strbuf *const buf = eina_strbuf_new();
   if (EINA_UNLIKELY(! buf))
     {
        CRI("Failed to allocate memory");
        return EINA_FALSE;
     }

   /* Add to the content of the command-line the number of spaces the text
    * should be indented of */
   for (size_t i = 0; i < (size_t)indent; i++)
     eina_strbuf_append_char(buf, ' ');

   for (unsigned int i = 0; i < content->size; i++)
     {
        const msgpack_object_array *const cont =
           EOVIM_MSGPACK_ARRAY_EXTRACT(&content->ptr[i], del_buf);

        /* The map will contain highlight attributes */
        //const msgpack_object_map *const map =
        //   EOVIM_MSGPACK_MAP_EXTRACT(&cont->ptr[0], del_buf);

        /* Extract the content of the command-line */
        const msgpack_object *const cont_o = &(cont->ptr[1]);
        EOVIM_MSGPACK_STRING_CHECK(cont_o, del_buf);
        const msgpack_object_str *const str = &(cont_o->via.str);
        eina_strbuf_append_length(buf, str->ptr, str->size);
     }

   gui_cmdline_show(&nvim->gui, eina_strbuf_string_get(buf), prompt, firstc);

   /* Set the cursor position within the command-line */
   gui_cmdline_cursor_pos_set(&nvim->gui, (size_t)pos);

   ret = EINA_TRUE;
del_buf:
   eina_strbuf_free(buf);
del_prompt:
   eina_stringshare_del(prompt);
del_firstc:
   eina_stringshare_del(firstc);
fail:
   return ret;
}

Eina_Bool
nvim_event_cmdline_pos(s_nvim *nvim,
                       const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 2);

   /* First argument if the position, second is the level. Level is not
    * handled for now.
    */
   const int64_t pos =
      EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[0], fail);
   //const int64_t level =
   //   EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[1], fail);

   gui_cmdline_cursor_pos_set(&nvim->gui, (size_t)pos);
   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

Eina_Bool
nvim_event_cmdline_special_char(s_nvim *nvim EINA_UNUSED,
                                const msgpack_object_array *args EINA_UNUSED)
{
   CRI("unimplemented");
   return EINA_TRUE;
}

Eina_Bool
nvim_event_cmdline_hide(s_nvim *nvim,
                        const msgpack_object_array *args EINA_UNUSED)
{
   gui_cmdline_hide(&nvim->gui);
   return EINA_TRUE;
}

Eina_Bool
nvim_event_cmdline_block_show(s_nvim *nvim EINA_UNUSED,
                              const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Blocks in cmdline is currently not supported. Sorry.");
   return EINA_TRUE;
}

Eina_Bool
nvim_event_cmdline_block_append(s_nvim *nvim EINA_UNUSED,
                                const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Blocks in cmdline is currently not supported. Sorry.");
   return EINA_TRUE;
}

Eina_Bool
nvim_event_cmdline_block_hide(s_nvim *nvim EINA_UNUSED,
                              const msgpack_object_array *args EINA_UNUSED)
{
   CRI("Blocks in cmdline is currently not supported. Sorry.");
   return EINA_TRUE;
}

Eina_Bool
nvim_event_wildmenu_show(s_nvim *nvim,
                         const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);
   s_gui *const gui = &nvim->gui;

   /* Go through all the items to be added to the wildmenu, and populate the
    * UI interface */
   const msgpack_object_array *const content =
      EOVIM_MSGPACK_ARRAY_EXTRACT(&params->ptr[0], fail);
   for (unsigned int i = 0; i < content->size; i++)
     {
        Eina_Stringshare *const item =
           EOVIM_MSGPACK_STRING_EXTRACT(&(content->ptr[i]), fail);
        gui_wildmenu_append(gui, item);
     }
   gui_wildmenu_show(gui);

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

Eina_Bool
nvim_event_wildmenu_hide(s_nvim *nvim,
                         const msgpack_object_array *args EINA_UNUSED)
{
   gui_wildmenu_clear(&nvim->gui);
   return EINA_TRUE;
}

Eina_Bool
nvim_event_wildmenu_select(s_nvim *nvim,
                           const msgpack_object_array *args)
{
   CHECK_BASE_ARGS_COUNT(args, ==, 1);
   ARRAY_OF_ARGS_EXTRACT(args, params);
   CHECK_ARGS_COUNT(params, ==, 1);

   const int64_t index = EOVIM_MSGPACK_INT64_EXTRACT(&params->ptr[0], fail);
   gui_wildmenu_select(&nvim->gui, index);
   return EINA_TRUE;
fail:
   return EINA_FALSE;
}
