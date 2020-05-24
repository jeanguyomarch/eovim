/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

typedef Eina_Bool (*f_mode_decode)(const msgpack_object *, struct mode *);

static Eina_Hash *_modes_params = NULL;

static Eina_Bool _arg_cursor_shape_get(const msgpack_object *obj, e_cursor_shape *arg);
static Eina_Bool _arg_mouse_shape_get(const msgpack_object *obj, int *arg);


/* There are many parameters to mode... so let's use an X-macro to generate most
 * of the code needed to parse and decode them.
 *
 *  Keyword             Decode Function         Field Name
 */
#define PARAMETERS(X)                                                  \
  X(cell_percentage,    arg_uint_get,          cell_percentage)        \
  X(blinkon,            arg_uint_get,          blinkon)                \
  X(blinkoff,           arg_uint_get,          blinkoff)               \
  X(blinkwait,          arg_uint_get,          blinkwait)              \
  X(name,               arg_stringshare_get,   name)                   \
  X(short_name,         arg_stringshare_get,   short_name)             \
  X(cursor_shape,       _arg_cursor_shape_get, cursor_shape)           \
  X(mouse_shape,        _arg_mouse_shape_get,  mouse_shape)            \
  X(attr_id,            arg_t_int_get,         attr_id)                \
  X(attr_id_lm,         arg_t_int_get,         attr_id_lm)             \
  X(hl_id,              arg_uint_get,          hl_id)                  \
  X(id_lm,              arg_uint_get,          hl_lm)                  \
  /**/

#define GEN_DECODERS(Kw, DecodeFunc, FieldName) \
  static Eina_Bool \
  _param_ ## Kw ## _cb(const msgpack_object *const obj, struct mode *const mode) \
  { return DecodeFunc(obj, &mode->FieldName); }
PARAMETERS(GEN_DECODERS)
#undef GEN_DECODERS


static Eina_Bool
_arg_cursor_shape_get(const msgpack_object *const obj, e_cursor_shape *const arg)
{
  const msgpack_object_str *const str = EOVIM_MSGPACK_STRING_OBJ_EXTRACT(obj, fail);
  if (! strncmp(str->ptr, "block", str->size))
  { *arg = CURSOR_SHAPE_BLOCK; }
  else if (! strncmp(str->ptr, "horizontal", str->size))
  { *arg = CURSOR_SHAPE_HORIZONTAL; }
  else if (! strncmp(str->ptr, "vertical", str->size))
  { *arg = CURSOR_SHAPE_VERTICAL; }
  else
  {
    ERR("Unknown cursor shape. Falling back to block");
    *arg = CURSOR_SHAPE_BLOCK;
  }
  return EINA_TRUE;
fail:
  return EINA_FALSE;
}

static Eina_Bool
_arg_mouse_shape_get(const msgpack_object *const obj EINA_UNUSED, int *const arg EINA_UNUSED)
{
  return EINA_TRUE; /* DO NOTHING */
}


/*****************************************************************************/

static Eina_Bool
_mode_info_set(s_nvim *const nvim, const msgpack_object_array *const params)
{
  Eina_Bool ret = EINA_TRUE;

  /* First arg: boolean */
  Eina_Bool cursor_style_enabled;
  GET_ARG(params, 0, bool, &cursor_style_enabled);
  if (EINA_UNLIKELY(! cursor_style_enabled))
  { ERR("We don't honor cursor_style_enabled"); } /* <-- TODO */


  /* Second arg: an array that contains maps of information */
  ARRAY_OF_ARGS_EXTRACT(params, kw_params);

  /* Go through all the arguments. They are expected to be maps */
  for (uint32_t i = 0; i < kw_params->size; i++)
  {
    const msgpack_object_map *const map =
      EOVIM_MSGPACK_MAP_EXTRACT(&kw_params->ptr[i], continue);
    const msgpack_object *o_key, *o_val;
    unsigned int it;

    /* Create the structure that will contain the mode informtion */
    struct mode *const mode = nvim_mode_new();
    if (EINA_UNLIKELY(! mode)) { goto fail; }

    EOVIM_MSGPACK_MAP_ITER(map, it, o_key, o_val)
    {
      Eina_Stringshare *const key = EOVIM_MSGPACK_STRING_EXTRACT(o_key, fail);
      const f_mode_decode func = eina_hash_find(_modes_params, key);
      if (EINA_UNLIKELY(! func))
      { WRN("Unhandled attribute '%s'", key); }
      else
      { ret &= func(o_val, mode); }
      eina_stringshare_del(key);
    }

    /* Replace the mode */
    void *const old = eina_hash_set(nvim->modes, mode->name, mode);
    nvim_mode_free(old);
  }

  return ret;
fail:
  return EINA_FALSE;
}


Eina_Bool
nvim_event_mode_info_set(s_nvim *nvim,
                         const msgpack_object_array *args)
{
  Eina_Bool ret = EINA_TRUE;
  for (unsigned int j = 1u; j < args->size; j++)
  {
    const msgpack_object *const obj = &(args->ptr[j]);
    CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);
    const msgpack_object_array *const params = &(obj->via.array);
    CHECK_ARGS_COUNT(params, >=, 2);
    ret &= _mode_info_set(nvim, params);
  }
  return ret;
}

Eina_Bool
nvim_event_mode_change(s_nvim *const nvim,
                       const msgpack_object_array *const args)
{
  for (unsigned int i = 1; i < args->size; i++)
  {
    const msgpack_object *const obj = &(args->ptr[i]);
    CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, EINA_FALSE);

    ARRAY_OF_ARGS_EXTRACT(args, params);
    CHECK_ARGS_COUNT(params, >=, 2);

    Eina_Stringshare *name;
    GET_ARG(params, 0, stringshare, &name);
    const s_mode *const mode = eina_hash_find(nvim->modes, name);
    gui_mode_update(&nvim->gui, mode);
    eina_stringshare_del(name);
  }
  return EINA_TRUE;
}

/*****************************************************************************/

Eina_Bool
mode_init(void)
{
  _modes_params = eina_hash_stringshared_new(NULL);
  if (EINA_UNLIKELY(! _modes_params))
  {
    CRI("Failed to create hash table");
    goto fail;
  }

  struct param
  {
    const char *const name;
    const f_mode_decode func;
  };

  const struct param params[] = {
#define GEN_PARAM(kw, _, __) \
    { .name = #kw, .func = &_param_ ## kw ## _cb },
    PARAMETERS(GEN_PARAM)
#undef GEN_PARAM
  };

  for (size_t i = 0; i < EINA_C_ARRAY_LENGTH(params); i++)
  {
    Eina_Stringshare *const str = eina_stringshare_add(params[i].name);
    if (EINA_UNLIKELY(! str))
    {
      CRI("Failed to register stringshare");
      goto fail;
    }

    if (EINA_UNLIKELY(! eina_hash_add(_modes_params, str, params[i].func)))
    {
      CRI("Failed to add item in hash");
      eina_stringshare_del(str);
      goto fail;
    }
  }
  return EINA_TRUE;
fail:
  return EINA_FALSE;
}

void
mode_shutdown(void)
{
  eina_hash_free(_modes_params);
}
