/*
 * contrib.h is a header that exports functions defined in contrib.c
 * This file contains code from third-parties projects, and as such, these
 * sources are not subject to Eovim's license.
 */

#ifndef __CONTRIB_CONTRIB_H__
#define __CONTRIB_CONTRIB_H__

#include <Eina.h>

int
contrib_parse_font_name(const char *full_name,
                        const char **name, const char **pretty_name);

Eina_Bool contrib_key_is_modifier(const char *key);

#endif /* ! __CONTRIB_CONTRIB_H__ */
