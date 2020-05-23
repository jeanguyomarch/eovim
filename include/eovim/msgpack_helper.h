/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_MSGPACK_HELPER_H__
#define __EOVIM_MSGPACK_HELPER_H__

#include <Eina.h>
#include <msgpack.h>

/**
 * Retrive a map from a msgpack object @a Obj. If @a Obj is not of type map,
 * then this macro jumps to the label @a Fail.
 */
#define EOVIM_MSGPACK_MAP_EXTRACT(Obj, Fail) \
   ({                                                                   \
    if (EINA_UNLIKELY((Obj)->type != MSGPACK_OBJECT_MAP)) {             \
         CRI("A map was expected, but we got 0x%x", (Obj)->type);       \
         goto Fail; }                                                   \
    &((Obj)->via.map);                                                  \
   })

/**
 * Iterate over a msgpack map @a Map, using @a It as an size type that is
 * incremented by one at each new element in the map. At each iteration, the
 * msgpack objects @a Key and @a Val will be set to a key-value pair contained
 * within @a Map.
 */
#define EOVIM_MSGPACK_MAP_ITER(Map, It, Key, Val)                       \
   for ((It) = 0,                                                       \
        (Key) = &((Map)->ptr[0].key),                                   \
        (Val) = &((Map)->ptr[0].val);                                   \
        (It) < (Map)->size;                                             \
        (It)++,                                                         \
        (Key) = &((Map)->ptr[(It)].key),                                \
        (Val) = &((Map)->ptr[(It)].val))

#define EOVIM_MSGPACK_ARRAY_ITER(Arr, It, Obj)                          \
   for ((It) = 0,                                                       \
        (Obj) = &((Arr)->ptr[0]);                                       \
        (It) < (Arr)->size;                                             \
        (It)++,                                                         \
        (Obj) = &(Arr->ptr[(It)]))


#define EOVIM_MSGPACK_STRING_CHECK(Obj, Fail)                           \
   if (EINA_UNLIKELY(((Obj)->type != MSGPACK_OBJECT_STR) &&             \
                     ((Obj)->type != MSGPACK_OBJECT_BIN))) {            \
      CRI("A string type (STR/BIN) was expected, but we got 0x%x", (Obj)->type); \
      goto Fail; }                                                      \

/**
 * Retrive an Eina_Stringshare from a msgpack object @a Obj. If @a Obj is not
 * of type STR or BIN, then this macro jumps to the label @a Fail.
 */
#define EOVIM_MSGPACK_STRING_EXTRACT(Obj, Fail)                         \
   ({                                                                   \
    EOVIM_MSGPACK_STRING_CHECK(Obj, Fail);                              \
    eina_stringshare_add_length((Obj)->via.str.ptr, (Obj)->via.str.size); \
   })

#define EOVIM_MSGPACK_STRING_OBJ_EXTRACT(Obj, Fail)                     \
   ({                                                                   \
    EOVIM_MSGPACK_STRING_CHECK(Obj, Fail);                              \
    &((Obj)->via.str);                                                  \
   })


/**
 * Retrive an array from a msgpack object @a Obj. If @a Obj is not of type
 * array, then this macro jumps to the label @a Fail.
 */
#define EOVIM_MSGPACK_ARRAY_EXTRACT(Obj, Fail)                          \
   ({                                                                   \
    if (EINA_UNLIKELY((Obj)->type != MSGPACK_OBJECT_ARRAY)) {           \
      CRI("An array is expected, but we got 0x%x", (Obj)->type);        \
      goto Fail; }                                                      \
    &((Obj)->via.array);                                                \
   })

/**
 * Retrive a 64-bits integer from a msgpack object @a Obj. If @a Obj is not of
 * type integer, then this macro jumps to the label @a Fail.
 */
#define EOVIM_MSGPACK_INT64_EXTRACT(Obj, Fail)                          \
   ({                                                                   \
    if (EINA_UNLIKELY(((Obj)->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&  \
                      ((Obj)->type != MSGPACK_OBJECT_NEGATIVE_INTEGER))) { \
      CRI("An integer is expected, but we got 0x%x", (Obj)->type);      \
      goto Fail; }                                                      \
    (Obj)->via.i64;                                                     \
   })

#endif /* ! __EOVIM_MSGPACK_HELPER_H__ */
