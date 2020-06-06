/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_EVENT_H__
#define __EOVIM_EVENT_H__

#include "eovim/types.h"
#include <msgpack.h>

struct method;

Eina_Bool nvim_event_init(void);
void nvim_event_shutdown(void);

const struct method *nvim_event_method_find(Eina_Stringshare *method_name);
Eina_Bool nvim_event_method_dispatch(struct nvim *nvim, const struct method *method,
				     Eina_Stringshare *command, const msgpack_object_array *args);

Eina_Bool nvim_event_method_batch_end(struct nvim *nvim, const struct method *method);

#endif /* ! __EOVIM_EVENT_H__ */
