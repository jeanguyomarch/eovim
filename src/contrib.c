/*
 * Contrib contains code copy/pasted from other projects. The license of Eovim
 * does not apply to this file.
 */

#include "contrib/contrib.h"
#include <Eina.h>
Eina_Bool
contrib_key_is_modifier(const char *key)
{
   /*
    * This function is taken from the source code of terminology.
    * https://www.enlightenment.org/about-terminology
    */
#define STATIC_STR_EQUAL(STR) (!strncmp(key, STR, sizeof(STR) - 1))
   if (EINA_LIKELY(key != NULL) && (
       STATIC_STR_EQUAL("Shift") ||
       STATIC_STR_EQUAL("Control") ||
       STATIC_STR_EQUAL("Alt") ||
       STATIC_STR_EQUAL("AltGr") ||
       STATIC_STR_EQUAL("Meta") ||
       STATIC_STR_EQUAL("Super") ||
       STATIC_STR_EQUAL("Hyper") ||
       STATIC_STR_EQUAL("Scroll_Lock") ||
       STATIC_STR_EQUAL("Num_Lock") ||
       STATIC_STR_EQUAL("ISO_Level3_Shift") ||
       STATIC_STR_EQUAL("Caps_Lock")))
     return EINA_TRUE;
#undef STATIC_STR_EQUAL
   return EINA_FALSE;
}
