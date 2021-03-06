/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_KEYMAP_H__
#define __EOVIM_KEYMAP_H__

#include <Eina.h>

struct keymap {
	const unsigned int size;
	const char *const name;
};

Eina_Bool keymap_init(void);
void keymap_shutdown(void);
const struct keymap *keymap_get(const char *input);

#endif /* ! __EOVIM_KEYMAP_H__ */
