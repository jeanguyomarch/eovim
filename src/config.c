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

#define CONFIG_VERSION 1

static Eet_Data_Descriptor *_edd = NULL;
static const char _key[] = "eovim/config";

#define EDD_BASIC_ADD(Field, Type) \
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd, s_config, # Field, Field, Type)

Eina_Bool
config_init(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, s_config);
   _edd = eet_data_descriptor_stream_new(&eddc);

   /* ========================================================================
    * s_config serialization
    * ===================================================================== */

   EDD_BASIC_ADD(version, EET_T_UINT);
   EDD_BASIC_ADD(font_size, EET_T_UINT);
   EDD_BASIC_ADD(font_name, EET_T_STRING);
   EDD_BASIC_ADD(mute_bell, EET_T_UCHAR);

   return EINA_TRUE;
}

void
config_shutdown(void)
{
   eet_data_descriptor_free(_edd);
}

void
config_font_size_set(s_config *config,
                     unsigned int font_size)
{
   config->font_size = font_size;
}

void
config_font_name_set(s_config *config,
                     Eina_Stringshare *font_name)
{
   eina_stringshare_replace(&config->font_name, font_name);
}

void
config_bell_mute_set(s_config *config,
                     Eina_Bool mute)
{
   config->mute_bell = !!mute;
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
   config->font_name = eina_stringshare_add("Mono");
   config->font_size = 12;
   config->mute_bell = EINA_FALSE;

   return config;
}

static char *
_config_path_get(const char *filename)
{
   if (filename)
     {
        /* If we pass an existing config path, get a copy of it */
        return strdup(filename);
     }
   else
     {
        /* Compose the path to the config file */
        const char *const home = efreet_config_home_get();
        Eina_Strbuf *const buf = eina_strbuf_new();
        if (EINA_UNLIKELY(! buf))
          {
             CRI("Failed to create stringbuffer");
             return NULL;
          }
        eina_strbuf_append_printf(buf, "%s/eovim/config", home);
        ecore_file_mkpath(eina_strbuf_string_get(buf));
        eina_strbuf_append(buf, "/eovim.cfg");

        /* Now that we have the final string, we get the storage string and
         * discard the string buffer */
        char *const path = eina_strbuf_string_steal(buf);
        eina_strbuf_free(buf);

        return path;
     }
}

s_config *
config_load(const char *file)
{
   s_config *cfg = NULL;
   char *const path = _config_path_get(file);
   if (EINA_UNLIKELY(! path)) return NULL;

   if (ecore_file_exists(path))
     {
        /* If the file does exist, we will read from it */
        Eet_File *const ef = eet_open(path, EET_FILE_MODE_READ);
        if (EINA_UNLIKELY(! ef))
          {
             CRI("Failed to open file '%s'", path);
             goto end;
          }

        cfg = eet_data_read(ef, _edd, _key);
        eet_close(ef);
        if (EINA_UNLIKELY(! cfg))
          {
             CRI("Configuration is corrupted.");
             goto new_config; /* Try to create a default config */
          }
        cfg->font_name = eina_stringshare_add(cfg->font_name);
        cfg->path = path;
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
        cfg->path = path;
     }

end:
   return cfg;
}

void
config_save(s_config *config)
{
   /* Open and save the config file */
   Eet_File *const ef = eet_open(config->path, EET_FILE_MODE_WRITE);
   if (EINA_UNLIKELY(! ef))
     {
        CRI("Failed to open file '%s'", config->path);
        return;
     }
   const int ok = eet_data_write(ef, _edd, _key, config, 1);
   eet_close(ef);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to write the configuration");
        return;
     }
}

void
config_free(s_config *config)
{
   eina_stringshare_del(config->font_name);
   free(config->path);
   free(config);
}
