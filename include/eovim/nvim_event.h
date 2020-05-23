/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_EVENT_H__
#define __EOVIM_EVENT_H__

#include "eovim/types.h"
#include <msgpack.h>

Eina_Bool nvim_event_dispatch(s_nvim *nvim, Eina_Stringshare *method_name,
                              Eina_Stringshare *command, const msgpack_object_array *args);
Eina_Bool nvim_event_init(void);
void nvim_event_shutdown(void);

#endif /* ! __EOVIM_EVENT_H__ */
