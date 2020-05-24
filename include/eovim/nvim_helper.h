/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_NVIM_HELPER_H__
#define __EOVIM_NVIM_HELPER_H__

#include "eovim/types.h"

Eina_Bool nvim_helper_autocmd_do(s_nvim *nvim, const char *event, f_nvim_api_cb func, void *func_data);

Eina_Bool nvim_helper_config_reload(s_nvim *nvim);

#endif /* ! __EOVIM_NVIM_HELPER_H__ */
