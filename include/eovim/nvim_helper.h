/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_NVIM_HELPER_H__
#define __EOVIM_NVIM_HELPER_H__

#include "eovim/types.h"

typedef struct
{
   struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
      Eina_Bool used;
   } bg, fg;
} s_hl_group;

typedef void (*f_highlight_group_decode)(s_nvim *nvim, const s_hl_group *hl_group);

void
nvim_helper_highlight_group_decode(s_nvim *nvim,
                                   unsigned int group,
                                   f_highlight_group_decode func);

void
nvim_helper_highlight_group_decode_noop(s_nvim *nvim,
                                        unsigned int group,
                                        f_highlight_group_decode func);


Eina_Bool nvim_helper_autocmd_do(s_nvim *nvim, const char *event, f_nvim_api_cb func, void *func_data);

Eina_Bool nvim_helper_config_reload(s_nvim *nvim);

#endif /* ! __EOVIM_NVIM_HELPER_H__ */
