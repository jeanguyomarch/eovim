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
#include <Ecore_File.h>
#include <Efreet.h>

#define CONFIG_VERSION 0

static Eet_Data_Descriptor *_edd_color = NULL;
static Eet_Data_Descriptor *_edd = NULL;
static const char _key[] = "eovim/config";

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
   EDD_COLOR_ADD(bg_color);
   EDD_BASIC_ADD(use_bg_color, EET_T_UCHAR);

   return EINA_TRUE;
}

void
config_shutdown(void)
{
   eet_data_descriptor_free(_edd_color);
   eet_data_descriptor_free(_edd);
}

void
config_bg_color_set(s_config *config,
                    int r, int g, int b, int a)
{
   config->bg_color->r = (uint8_t)r;
   config->bg_color->g = (uint8_t)g;
   config->bg_color->b = (uint8_t)b;
   config->bg_color->a = (uint8_t)a;
}

void
config_use_bg_color_set(s_config *config,
                        Eina_Bool use)
{
   config->use_bg_color = !!use;
}

static s_config *
_config_new(void)
{
   s_config *const config = calloc(1, sizeof(s_config));
   if (EINA_UNLIKELY(! config))
     {
        CRI("Failed to allocate memory");
        return NULL;
     }

   config->version = CONFIG_VERSION;
   config->bg_color = malloc(sizeof(s_config_color));
   config_bg_color_set(config, 0, 0, 0, 255);
   config->use_bg_color = EINA_FALSE;

   return config;
}

static Eina_Strbuf *
_config_path_get(void)
{
  /* Compose the path to the config file */
   const char *const home = efreet_config_home_get();
   Eina_Strbuf *const path = eina_strbuf_new();
   if (EINA_UNLIKELY(! path))
     {
        CRI("Failed to create stringbuffer");
        return NULL;
     }
   eina_strbuf_append_printf(path, "%s/eovim/config", home);
   ecore_file_mkpath(eina_strbuf_string_get(path));
   eina_strbuf_append(path, "/eovim.cfg");

   return path;
}

s_config *
config_load(void)
{
   s_config *cfg = NULL;
   Eina_Strbuf *const path = _config_path_get();
   if (EINA_UNLIKELY(! path)) return NULL;

   const char *const cfg_file = eina_strbuf_string_get(path);
   if (ecore_file_exists(cfg_file))
     {
        /* If the file does exist, we will read from it */
        Eet_File *const ef = eet_open(cfg_file, EET_FILE_MODE_READ);
        if (EINA_UNLIKELY(! ef))
          {
             CRI("Failed to open file '%s'", cfg_file);
             goto end;
          }

        cfg = eet_data_read(ef, _edd, _key);
        eet_close(ef);
        if (EINA_UNLIKELY(! cfg))
          {
             CRI("Configuration is corrupted.");
             goto new_config; /* Try to create a default config */
          }

     }
   else
     {
new_config: /* Yes, f!ck you too */
        /* The file does not exist, we will create a default one */
        cfg = _config_new();
        if (EINA_UNLIKELY(! cfg))
          {
             CRI("Failed to create a default configuration");
             goto end;
          }
     }

end:
   eina_strbuf_free(path);
   return cfg;
}

void
config_save(s_config *config)
{
   Eina_Strbuf *const path = _config_path_get();
   if (EINA_UNLIKELY(! path)) return;

   /* Open and save the config file */
   const char *const cfg_file = eina_strbuf_string_get(path);
   Eet_File *const ef = eet_open(cfg_file, EET_FILE_MODE_WRITE);
   if (EINA_UNLIKELY(! ef))
     {
        CRI("Failed to open file '%s'", cfg_file);
        goto end;
     }
   const int ok = eet_data_write(ef, _edd, _key, config, 1);
   eet_close(ef);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to write the configuration");
        goto end;
     }
end:
   eina_strbuf_free(path);
}

void
config_free(s_config *config)
{
   free(config->bg_color);
   free(config);
}
