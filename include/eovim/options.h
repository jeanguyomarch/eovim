/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_OPTIONS_H__
#define __EOVIM_OPTIONS_H__

#include "eovim/types.h"

struct options {
	struct geometry geometry;

	const char *nvim_prog;
	const char *theme;

	Eina_Bool fullscreen;
	Eina_Bool maximized; /**< Eovim will run in a maximized window */
	Eina_Bool forbidden;
};

enum options_result {
	OPTIONS_RESULT_ERROR,
	OPTIONS_RESULT_QUIT,
	OPTIONS_RESULT_CONTINUE,
};

enum options_result options_parse(int argc, const char *argv[], struct options *opts);
void options_defaults_set(struct options *opts);

#endif /* ! __EOVIM_OPTIONS_H__ */
