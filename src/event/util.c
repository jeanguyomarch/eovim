/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

const msgpack_object_array *array_of_args_extract(const msgpack_object_array *const args,
						  const size_t at_index)
{
	if (EINA_UNLIKELY(at_index >= args->size)) {
		ERR("Out of bounds index");
		return NULL;
	}
	const msgpack_object *const obj = &(args->ptr[at_index]);
	CHECK_TYPE(obj, MSGPACK_OBJECT_ARRAY, NULL);
	return &(obj->via.array);
}

Eina_Bool arg_t_int_get(const msgpack_object *const obj, t_int *const arg)
{
	if (EINA_UNLIKELY((obj->type != MSGPACK_OBJECT_POSITIVE_INTEGER) &&
			  (obj->type != MSGPACK_OBJECT_NEGATIVE_INTEGER))) {
		CRI("Expected an integer type for argument. Got 0x%x", obj->type);
		return EINA_FALSE;
	}
	*arg = obj->via.i64;
	return EINA_TRUE;
}

Eina_Bool arg_uint_get(const msgpack_object *const obj, unsigned int *const arg)
{
	if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
		ERR("Expected a positive integer type for argument. Got 0x%x", obj->type);
		return EINA_FALSE;
	}
	if (EINA_UNLIKELY(obj->via.u64 > UINT_MAX)) {
		ERR("Integer value too big for specified storage");
		return EINA_FALSE;
	}
	*arg = (unsigned int)obj->via.u64;
	return EINA_TRUE;
}

Eina_Bool arg_color_get(const msgpack_object *const obj, union color *const arg)
{
	if (EINA_UNLIKELY(obj->type != MSGPACK_OBJECT_POSITIVE_INTEGER)) {
		CRI("Expected an uint32_t type for argument. Got 0x%x", obj->type);
		return EINA_FALSE;
	}
	if (EINA_UNLIKELY(obj->via.u64 > UINT32_MAX)) {
		CRI("Value too big to fit uint32_t");
		return EINA_FALSE;
	}
	arg->value = (uint32_t)obj->via.u64;
	arg->a = 0xff;
	return EINA_TRUE;
}

Eina_Bool arg_stringshare_get(const msgpack_object *const obj, Eina_Stringshare **const arg)
{
	CHECK_TYPE(obj, MSGPACK_OBJECT_STR, EINA_FALSE);
	const msgpack_object_str *const str = &(obj->via.str);
	*arg = eina_stringshare_add_length(str->ptr, str->size);
	if (EINA_UNLIKELY(!*arg)) {
		CRI("Failed to create stringshare");
		return EINA_FALSE;
	}
	return EINA_TRUE;
}

Eina_Bool arg_bool_get(const msgpack_object *const obj, Eina_Bool *const arg)
{
	CHECK_TYPE(obj, MSGPACK_OBJECT_BOOLEAN, EINA_FALSE);
	*arg = obj->via.boolean;
	return EINA_TRUE;
}
