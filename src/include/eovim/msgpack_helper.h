/*
 * Copyright (c) 2017 Jean Guyomarc'h
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

#ifndef __EOVIM_MSGPACK_HELPER_H__
#define __EOVIM_MSGPACK_HELPER_H__

#include <Eina.h>
#include <msgpack.h>

/**
 * Retrive a map from a msgpack object \a Obj. If \a Obj is not of type map,
 * then this macro jumps to the label \a Fail.
 */
#define EOVIM_MSGPACK_MAP_EXTRACT(Obj, Fail) \
   ({                                                                   \
    if (EINA_UNLIKELY((Obj)->type != MSGPACK_OBJECT_MAP)) {             \
         CRI("A map was expected, but we got 0x%x", (Obj)->type);       \
         goto Fail; }                                                   \
    &((Obj)->via.map);                                                  \
   })

/**
 * Iterate over a msgpack map \a Map, using \a It as an size type that is
 * incremented by one at each new element in the map. At each iteration, the
 * msgpack objects \a Key and \a Val will be set to a key-value pair contained
 * within \a Map.
 */
#define EOVIM_MSGPACK_MAP_ITER(Map, It, Key, Val)                       \
   for ((It) = 0,                                                       \
        (Key) = &((Map)->ptr[0].key),                                   \
        (Val) = &((Map)->ptr[0].val);                                   \
        (It) < (Map)->size;                                             \
        (It)++,                                                         \
        (Key) = &((Map)->ptr[(It)].key),                                \
        (Val) = &((Map)->ptr[(It)].val))

/**
 * Retrive an Eina_Stringshare from a msgpack object \a Obj. If \a Obj is not
 * of type STR or BIN, then this macro jumps to the label \a Fail.
 */
#define EOVIM_MSGPACK_STRING_EXTRACT(Obj, Fail)                         \
   ({                                                                   \
   if (EINA_UNLIKELY(((Obj)->type != MSGPACK_OBJECT_STR) &&             \
                     ((Obj)->type != MSGPACK_OBJECT_BIN))) {            \
      CRI("A string type (STR/BIN) was expected, but we got 0x%x", (Obj)->type); \
      goto Fail; }                                                      \
   eina_stringshare_add_length((Obj)->via.str.ptr, (Obj)->via.str.size); \
   })


/**
 * Retrive an array from a msgpack object \a Obj. If \a Obj is not of type
 * array, then this macro jumps to the label \a Fail.
 */
#define EOVIM_MSGPACK_ARRAY_EXTRACT(Obj, Fail)                          \
   ({                                                                   \
    if (EINA_UNLIKELY((Obj)->type != MSGPACK_OBJECT_ARRAY)) {           \
      CRI("An array is expected, but we got 0x%x", (Obj)->type);        \
      goto Fail; }                                                      \
    &((Obj)->via.array);                                                \
   })


#endif /* ! __EOVIM_MSGPACK_HELPER_H__ */
