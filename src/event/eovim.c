/* This file is part of Eovim, which is under the MIT License ****************/

#include "event.h"

Eina_Bool nvim_event_eovim_reload(struct nvim *const nvim,
				  const msgpack_object_array *const args EINA_UNUSED)
{
	return nvim_helper_config_reload(nvim);
}
