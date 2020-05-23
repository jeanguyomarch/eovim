/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef EOVIM_NVIM_COMPLETION_H__
#define EOVIM_NVIM_COMPLETION_H__

#include <Eina.h>
#include <eovim/types.h>

s_completion *
nvim_completion_new(const char *word, size_t word_size,
                    const char *kind, size_t kind_size,
                    const char *menu, size_t menu_size,
                    const char *info, size_t info_size);
void nvim_completion_free(s_completion *compl);

#endif /* ! EOVIM_NVIM_COMPLETION_H__ */
