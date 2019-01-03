/*
 * Copyright (c) 2019 Jean Guyomarc'h
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

#include "event.h"

/** Dictionary that associates a callback handler to each option name */
static Eina_Hash *_options = NULL;

/**
 * Type of functions that shall handle options modifiers.
 *
 * @param[in] nvim Global nvim instance context
 * @param[in] value Generic msgpack value provided to the option
 * @return EINA_TRUE on success, EINA_FALSE otherwise
 */
typedef Eina_Bool (*f_opt_set)(s_nvim *nvim, const msgpack_object *value);

/*****************************************************************************/

static Eina_Bool
_arabicshape_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_ambiwidth_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_emoji_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_guifont_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_guifontset_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_guifontwide_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_linespace_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_showtabline_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_termguicolors_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_ext_popupmenu_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_ext_tabline_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_ext_cmdline_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_ext_wildmenu_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_ext_linegrid_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

static Eina_Bool
_ext_hlstate_set(s_nvim *nvim EINA_UNUSED, const msgpack_object *value EINA_UNUSED)
{
   /* XXX unimplemented for now */
   return EINA_TRUE;
}

/*****************************************************************************/

Eina_Bool
nvim_event_option_set(s_nvim *nvim,
                      const msgpack_object_array *args)
{
   /* We expect the arguments that come after the very first parameter (name of
    * the event, so 'option_set' in this case) to be a set of pairs:
    *
    * [
    *   'option_set',
    *   [
    *     [keyword-name (type: string), value (type: any)],
    *     ...
    *   ]
    * ]
    */
   CHECK_BASE_ARGS_COUNT(args, >=, 1u);
   for (uint32_t i = 1u; i < args->size; i++)
     {
        /* Get a pair (option name + option value) */
        const msgpack_object_array *const opt =
           EOVIM_MSGPACK_ARRAY_EXTRACT(&args->ptr[i], fail);
        CHECK_ARGS_COUNT(opt, ==, 2u);

        /* Get the name of the option as a stringshare */
        Eina_Stringshare *const key =
           EOVIM_MSGPACK_STRING_EXTRACT(&opt->ptr[0], fail);

        /* Find the handler for the option */
        const f_opt_set func = eina_hash_find(_options, key);
        if (EINA_UNLIKELY(! func))
          {
             ERR("Unknown 'option_set' keyword argument '%s'", key);
             eina_stringshare_del(key);
             continue; /* Try the next keyword-argument */
          }

        /* Execute the handler of the option */
        const Eina_Bool ok = func(nvim, &opt->ptr[1]);
        if (EINA_UNLIKELY(! ok))
          {
             ERR("Failed to process option with keyword '%s'", key);
             eina_stringshare_del(key);
             return EINA_FALSE;
          }
        eina_stringshare_del(key);
     }

   return EINA_TRUE;
fail:
   return EINA_FALSE;
}

typedef struct
{
   Eina_Stringshare *const opt; /**< Name of the option */
   const f_opt_set func; /**< Callback */
} s_opt;

#define OPT(Keyword, Func) { .opt = KW(Keyword), .func = (Func) }

static Eina_Bool
_option_add(const s_opt *opt)
{
  const Eina_Bool ok = eina_hash_direct_add(_options, opt->opt, opt->func);
  if (EINA_UNLIKELY(! ok))
  { ERR("Failed to register option '%s' in options table", opt->opt); }
  return ok;
}

Eina_Bool
option_set_init(void)
{
   _options = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(! _options))
     {
        CRI("Failed to create hash table");
        return EINA_FALSE;
     }

   const s_opt opts[] = {
      OPT(KW_ARABICSHAPE, _arabicshape_set),
      OPT(KW_AMBIWIDTH, _ambiwidth_set),
      OPT(KW_EMOJI, _emoji_set),
      OPT(KW_GUIFONT, _guifont_set),
      OPT(KW_GUIFONTSET, _guifontset_set),
      OPT(KW_GUIFONTWIDE, _guifontwide_set),
      OPT(KW_LINESPACE, _linespace_set),
      OPT(KW_SHOWTABLINE, _showtabline_set),
      OPT(KW_TERMGUICOLORS, _termguicolors_set),
      OPT(KW_EXT_POPUPMENU, _ext_popupmenu_set),
      OPT(KW_EXT_TABLINE, _ext_tabline_set),
      OPT(KW_EXT_CMDLINE, _ext_cmdline_set),
      OPT(KW_EXT_WILDMENU, _ext_wildmenu_set),
      OPT(KW_EXT_LINEGRID, _ext_linegrid_set),
      OPT(KW_EXT_HLSTATE, _ext_hlstate_set),
   };
   for (size_t i = 0u; i < EINA_C_ARRAY_LENGTH(opts); i++)
     if (EINA_UNLIKELY(! _option_add(&opts[i])))
       goto fail;

   return EINA_TRUE;
fail:
   eina_hash_free(_options);
   return EINA_FALSE;
}

void
option_set_shutdown(void)
{
   eina_hash_free(_options);
}
