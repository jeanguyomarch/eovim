/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_OPTIONS_H__
#define __EOVIM_OPTIONS_H__

#include "eovim/types.h"

typedef struct
{
   s_geometry geometry;

   const char *nvim_prog;
   const char *theme;

   Eina_Bool fullscreen;
   Eina_Bool maximized; /**< Eovim will run in a maximized window */
   Eina_Bool forbidden;
} s_options;

typedef enum
{
   OPTIONS_RESULT_ERROR,
   OPTIONS_RESULT_QUIT,
   OPTIONS_RESULT_CONTINUE,
} e_options_result;

e_options_result options_parse(int argc, const char *argv[], s_options *opts);
void options_defaults_set(s_options *opts);

#endif /* ! __EOVIM_OPTIONS_H__ */
