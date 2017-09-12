/*
 * Copyright (c) 2017 Jean Guyomarc'h
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "eovim/config.h"
#include "eovim/log.h"
#include <Eet.h>

#define CONFIG_VERSION 0

static Eet_Data_Descriptor *_edd_color = NULL;
static Eet_Data_Descriptor *_edd = NULL;

#define EDD_COLOR_REGISTER(Field) \
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_color, s_config_color, \
                                 # Field, Field, EET_T_UCHAR)

#define EDD_BASIC_ADD(Field, Type) \
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd, s_config, # Field, Field, Type)
#define EDD_COLOR_ADD(Field) \
   EET_DATA_DESCRIPTOR_ADD_SUB(_edd, s_config, # Field, Field, _edd_color)

Eina_Bool
config_init(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, s_config);
   _edd = eet_data_descriptor_stream_new(&eddc);

   /* ========================================================================
    * s_config_color serialization
    * ===================================================================== */
   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, s_config_color);
   _edd_color = eet_data_descriptor_stream_new(&eddc);

   EDD_COLOR_REGISTER(r);
   EDD_COLOR_REGISTER(g);
   EDD_COLOR_REGISTER(b);
   EDD_COLOR_REGISTER(a);

   /* ========================================================================
    * s_config serialization
    * ===================================================================== */

   EDD_BASIC_ADD(version, EET_T_UINT);
   EDD_COLOR_ADD(fg_color);

   return EINA_TRUE;
}

void
config_shutdown(void)
{
   eet_data_descriptor_free(_edd_color);
   eet_data_descriptor_free(_edd);
}

void
config_fg_color_set(s_config *config,
                    int r, int g, int b, int a)
{
   config->fg_color->r = (uint8_t)r;
   config->fg_color->g = (uint8_t)g;
   config->fg_color->b = (uint8_t)b;
   config->fg_color->a = (uint8_t)a;
}

s_config *
config_new(void)
{
   s_config *const config = calloc(1, sizeof(s_config));
   if (EINA_UNLIKELY(! config))
     {
        CRI("Failed to allocate memory");
        return NULL;
     }

   config->version = CONFIG_VERSION;
   config->fg_color = malloc(sizeof(s_config_color));
   config_fg_color_set(config, 0xff, 0xff, 0xff, 0xff);

   return config;
}

void
config_free(s_config *config)
{
   free(config->fg_color);
   free(config);
}
