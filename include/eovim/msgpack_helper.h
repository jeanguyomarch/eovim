/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __MPACK_HELPER_H__
#define __MPACK_HELPER_H__

#include <Eina.h>
#include <msgpack.h>

/**
 * Retrive a map from a msgpack object @a Obj. If @a Obj is not of type map,
 * then this macro executes code @p OnFail
 */
#define MPACK_MAP_EXTRACT(Obj, OnFail)                                                             \
	({                                                                                         \
		if (EINA_UNLIKELY((Obj)->type != MSGPACK_OBJECT_MAP)) {                            \
			CRI("A map was expected, but we got 0x%x", (Obj)->type);                   \
			OnFail;                                                                    \
		}                                                                                  \
		&((Obj)->via.map);                                                                 \
	})

/**
 * Iterate over a msgpack map @a Map, using @a It as an size type that is
 * incremented by one at each new element in the map. At each iteration, the
 * msgpack objects @a Key and @a Val will be set to a key-value pair contained
 * within @a Map.
 */
#define MPACK_MAP_ITER(Map, It, Key, Val)                                                          \
	for ((It) = 0, (Key) = &((Map)->ptr[0].key), (Val) = &((Map)->ptr[0].val);                 \
	     (It) < (Map)->size;                                                                   \
	     (It)++, (Key) = &((Map)->ptr[(It)].key), (Val) = &((Map)->ptr[(It)].val))

#define MPACK_ARRAY_ITER(Arr, It, Obj)                                                             \
	for ((It) = 0, (Obj) = &((Arr)->ptr[0]); (It) < (Arr)->size;                               \
	     (It)++, (Obj) = &(Arr->ptr[(It)]))

#define MPACK_STRING_CHECK(Obj, OnFail)                                                            \
	if (EINA_UNLIKELY(((Obj)->type != MSGPACK_OBJECT_STR) &&                                   \
			  ((Obj)->type != MSGPACK_OBJECT_BIN))) {                                  \
		CRI("A string type (STR/BIN) was expected, but we got 0x%x", (Obj)->type);         \
		OnFail;                                                                            \
	}

/**
 * Retrive an Eina_Stringshare from a msgpack object @a Obj. If @a Obj is not
 * of type STR or BIN, then this macro jumps to the label @a Fail.
 */
#define MPACK_STRING_EXTRACT(Obj, OnFail)                                                          \
	({                                                                                         \
		MPACK_STRING_CHECK(Obj, OnFail);                                                   \
		eina_stringshare_add_length((Obj)->via.str.ptr, (Obj)->via.str.size);              \
	})

#define MPACK_STRING_OBJ_EXTRACT(Obj, OnFail)                                                      \
	({                                                                                         \
		MPACK_STRING_CHECK(Obj, OnFail);                                                   \
		&((Obj)->via.str);                                                                 \
	})

/**
 * Retrive an array from a msgpack object @a Obj. If @a Obj is not of type
 * array, then this macro jumps to the label @a Fail.
 */
#define MPACK_ARRAY_EXTRACT(Obj, OnFail)                                                           \
	({                                                                                         \
		if (EINA_UNLIKELY((Obj)->type != MSGPACK_OBJECT_ARRAY)) {                          \
			CRI("An array is expected, but we got 0x%x", (Obj)->type);                 \
			OnFail;                                                                    \
		}                                                                                  \
		&((Obj)->via.array);                                                               \
	})

/**
 * Retrive a 64-bits integer from a msgpack object @a Obj. If @a Obj is not of
 * type integer, then this macro jumps to the label @a Fail.
 */
#define MPACK_INT64_EXTRACT(Obj, OnFail)                                                           \
	({                                                                                         \
		if (EINA_UNLIKELY(((Obj)->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&              \
				  ((Obj)->type != MSGPACK_OBJECT_NEGATIVE_INTEGER))) {             \
			CRI("An integer is expected, but we got 0x%x", (Obj)->type);               \
			OnFail;                                                                    \
		}                                                                                  \
		(Obj)->via.i64;                                                                    \
	})

static inline Eina_Bool _msgpack_streq(const msgpack_object_str *const str, const char *const with,
				       const size_t len)
{
	return 0 == strncmp(str->ptr, with, MIN(str->size, len));
}
#define _MSGPACK_STREQ(MsgPackStr, StaticStr)                                                      \
	_msgpack_streq(MsgPackStr, "" StaticStr "", sizeof(StaticStr) - 1u)

#endif /* ! __MPACK_HELPER_H__ */
