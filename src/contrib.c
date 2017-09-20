/*
 * Contrib contains code copy/pasted from other projects. The license of Eovim
 * does not apply to this file.
 */

#include "contrib/contrib.h"
#include <Eina.h>

int
contrib_parse_font_name(const char *full_name,
                        const char **name,
                        const char **pretty_name)
{
   /*
    * This function is taken from the source code of terminology.
    * https://www.enlightenment.org/about-terminology
    */
   char buf[4096];
   size_t style_len = 0;
   size_t font_len = 0;
   char *style = NULL;
   char *s;
   unsigned int len;

   *pretty_name = NULL;
   *name = NULL;

   font_len = strlen(full_name);
   if (font_len >= sizeof(buf))
     return -1;
   style = strchr(full_name, ':');
   if (style)
     font_len = (size_t)(style - full_name);

   s = strchr(full_name, ',');
   if (s && style && s < style)
     font_len = s - full_name;

#define STYLE_STR ":style="
   if (style && strncmp(style, STYLE_STR, strlen(STYLE_STR)) == 0)
     {
        style += strlen(STYLE_STR);
        s = strchr(style, ',');
        style_len = (s == NULL) ? strlen(style) : (size_t)(s - style);
     }

   s = buf;
   memcpy(s, full_name, font_len);
   s += font_len;
   len = font_len;
   if (style)
     {
        memcpy(s, STYLE_STR, strlen(STYLE_STR));
        s += strlen(STYLE_STR);
        len += strlen(STYLE_STR);

        memcpy(s, style, style_len);
        s += style_len;
        len += style_len;
     }
   *s = '\0';
   *name = eina_stringshare_add_length(buf, len);
#undef STYLE_STR

   /* unescape the dashes */
   s = buf;
   len = 0;
   while ( (size_t)(s - buf) < sizeof(buf) &&
           font_len > 0 )
     {
        if (*full_name != '\\')
          {
             *s++ = *full_name;
          }
        full_name++;
        font_len--;
        len++;
     }
   /* copy style */
   if (style_len > 0 && ((sizeof(buf) - (s - buf)) > style_len + 3 ))
     {
        *s++ = ' ';
        *s++ = '(';
        memcpy(s, style, style_len);
        s += style_len;
        *s++ = ')';

        len += 3 + style_len;
     }
   *s = '\0';

   *pretty_name = eina_stringshare_add_length(buf, len);
   return 0;
}
