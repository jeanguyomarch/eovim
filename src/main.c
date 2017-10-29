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

#include "eovim/keymap.h"
#include "eovim/config.h"
#include "eovim/mode.h"
#include "eovim/nvim.h"
#include "eovim/nvim_api.h"
#include "eovim/nvim_event.h"
#include "eovim/termview.h"
#include "eovim/main.h"
#include "eovim/plugin.h"
#include "eovim/log.h"
#include "eovim/prefs.h"
#include "eovim/version.h"
#include <Ecore_Getopt.h>

int _eovim_log_domain = -1;

static Eina_Bool _in_tree = EINA_FALSE;
static Eina_Strbuf *_edje_file = NULL;

static Eina_Bool
_parse_geometry_cb(const Ecore_Getopt *parser EINA_UNUSED,
                   const Ecore_Getopt_Desc *desc EINA_UNUSED,
                   const char *str,
                   void *data EINA_UNUSED,
                   Ecore_Getopt_Value *storage)
{
   s_geometry *const geo = (s_geometry *)storage->ptrp;
   const int nb = sscanf(str, "%ux%u", &geo->w, &geo->h);
   if (nb != 2)
     {
        ERR("Failed to parse geometry. <UINT>x<UINT> is expected (e.g. 80x24)");
        return EINA_FALSE;
     }
   else if ((geo->w == 0) || (geo->h == 0))
     {
        ERR("Geometry cannot have a dimension of 0");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static const Ecore_Getopt _options =
{
   "eovim",
   "%prog [options] [files...]",
   EOVIM_VERSION,
   "2017 (c) Jean Guyomarc'h",
   "MIT",
   "An EFL GUI client for NeoVim",
   EINA_FALSE, /* Not strict: allows forwarding */
   {
      /* Neovim options remapped */
      ECORE_GETOPT_STORE_TRUE('b', "binary", "Run neovim in binary mode"),
      ECORE_GETOPT_STORE_TRUE('d', "diff", "Run neovim in diff mode"),
      ECORE_GETOPT_STORE_TRUE('R', "read-only", "Run neovim in read-only mode"),
      ECORE_GETOPT_STORE_TRUE('Z', "restricted", "Run neovim in restricted mode"),
      ECORE_GETOPT_STORE_TRUE('n', "no-swap", "Disable swap files use"),
      ECORE_GETOPT_STORE_STR('r', "recover", "Recover crashed session"),
      ECORE_GETOPT_STORE_STR('u', "nvimrc", "Override nvim.init with a custom file"),
      ECORE_GETOPT_STORE_TRUE('N', "no-plugins", "Don't load plugin scripts"),
      ECORE_GETOPT_CALLBACK_ARGS('g', "geometry",
         "Initial dimensions (in cells) of the window (e.g. 80x24)",
         "GEOMETRY", _parse_geometry_cb, NULL),

      ECORE_GETOPT_STORE_TRUE('o', NULL, "Open one horizontal window per file"),
      ECORE_GETOPT_STORE_UINT('\0', "hsplit", "Open N horizontal windows"),
      ECORE_GETOPT_STORE_TRUE('O', NULL, "Open one vertical window per file"),
      ECORE_GETOPT_STORE_UINT('\0', "vsplit", "Open N vertical windows"),
      ECORE_GETOPT_STORE_TRUE('p', NULL, "Open one tab page per file"),
      ECORE_GETOPT_STORE_UINT('\0', "tabs", "Open N tab pages"),

      /* Eovim options */
      ECORE_GETOPT_STORE_STR('\0', "config",
         "Provide an alternate user configuration"),
      ECORE_GETOPT_STORE_TRUE('F', "fullscreen", "Run Eovim in full screen"),
      ECORE_GETOPT_STORE_STR('t', "theme", "Name of the theme to be used"),
      ECORE_GETOPT_STORE_STR('\0', "nvim", "Path to the nvim program"),
      ECORE_GETOPT_HELP('h', "help"),
      ECORE_GETOPT_VERSION('V', "version"),
      ECORE_GETOPT_SENTINEL
   }
};

typedef struct
{
   const char *const name;
   Eina_Bool (*const init)(void);
   void (*const shutdown)(void);
} s_module;

static const s_module _modules[] =
{
#define MODULE(name_) \
   { .name = #name_, .init = name_ ## _init, .shutdown = name_ ## _shutdown }

   MODULE(config),
   MODULE(keymap),
   MODULE(mode),
   MODULE(nvim_api),
   MODULE(nvim_event),
   MODULE(plugin),
   MODULE(prefs),
   MODULE(gui),
   MODULE(termview),
   MODULE(nvim),

#undef MODULE
};

static Eina_Bool
_edje_file_init(const char *theme)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(theme, EINA_FALSE);

   const char *const dir = main_in_tree_is()
      ? BUILD_DATA_DIR 
      : elm_app_data_dir_get();

   _edje_file = eina_strbuf_new();
   if (EINA_UNLIKELY(! _edje_file))
     {
        CRI("Failed to create Strbuf");
        return EINA_FALSE;
     }
   eina_strbuf_append_printf(_edje_file, "%s/themes/%s.edj", dir, theme);
   return EINA_TRUE;
}

Eina_Bool
main_in_tree_is(void)
{
   return _in_tree;
}

const char *
main_edje_file_get(void)
{
   return eina_strbuf_string_get(_edje_file);
}

EAPI_MAIN int elm_main(int argc, char **argv);
EAPI_MAIN int
elm_main(int argc,
         char **argv)
{
   int return_code = EXIT_FAILURE;
   int args;
   Eina_Bool quit_option = EINA_FALSE;
   char *nvim_prog = "nvim";
   char *theme = "default";
   s_nvim_options opts;
   nvim_options_defaults_set(&opts);

   Ecore_Getopt_Value values[] = {
      /* Neovim options remapped */
      ECORE_GETOPT_VALUE_BOOL(opts.binary),
      ECORE_GETOPT_VALUE_BOOL(opts.diff),
      ECORE_GETOPT_VALUE_BOOL(opts.read_only),
      ECORE_GETOPT_VALUE_BOOL(opts.restricted),
      ECORE_GETOPT_VALUE_BOOL(opts.no_swap),
      ECORE_GETOPT_VALUE_STR(opts.recover),
      ECORE_GETOPT_VALUE_STR(opts.nvimrc),
      ECORE_GETOPT_VALUE_BOOL(opts.no_plugins),
      ECORE_GETOPT_VALUE_PTR_CAST(opts.geometry),

      ECORE_GETOPT_VALUE_BOOL(opts.hsplit.per_file),
      ECORE_GETOPT_VALUE_UINT(opts.hsplit.count),
      ECORE_GETOPT_VALUE_BOOL(opts.vsplit.per_file),
      ECORE_GETOPT_VALUE_UINT(opts.vsplit.count),
      ECORE_GETOPT_VALUE_BOOL(opts.tsplit.per_file),
      ECORE_GETOPT_VALUE_UINT(opts.tsplit.count),

      /* Eovim options */
      ECORE_GETOPT_VALUE_STR(opts.config_path),
      ECORE_GETOPT_VALUE_BOOL(opts.fullscreen),
      ECORE_GETOPT_VALUE_STR(theme),
      ECORE_GETOPT_VALUE_STR(nvim_prog),
      ECORE_GETOPT_VALUE_BOOL(quit_option),
      ECORE_GETOPT_VALUE_BOOL(quit_option),
   };

   /* First step: initialize the logging framework */
   _eovim_log_domain = eina_log_domain_register("eovim", EINA_COLOR_RED);
   if (EINA_UNLIKELY(_eovim_log_domain < 0))
     {
        EINA_LOG_CRIT("Failed to create log domain");
        goto end;
     }

   /* Do the getopts */
   args = ecore_getopt_parse(&_options, values, argc, argv);
   if (args < 0)
     {
        CRI("Failed to parse command-line arguments");
        goto log_unregister;
     }

   /* Did we require quitting? Quit! */
   if (quit_option)
     {
        return_code = EXIT_SUCCESS;
        goto log_unregister;
     }

   /*
    * App settings
    */
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_language_set("");
   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_lib_dir_set(PACKAGE_LIB_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(elm_main, "eovim", "themes/default.edj");

   const char *const env = getenv("EOVIM_IN_TREE");
   _in_tree = (env) ? !!atoi(env) : EINA_FALSE;

   if (EINA_UNLIKELY(! _edje_file_init(theme)))
     {
        CRI("Failed to compose edje file path");
        goto log_unregister;
     }

   /*
    * Initialize all the different modules that compose Eovim.
    */
   const s_module *const mod_last = &(_modules[EINA_C_ARRAY_LENGTH(_modules) - 1]);
   const s_module *mod_it;
   for (mod_it = _modules; mod_it <= mod_last; mod_it++)
     {
        if (EINA_UNLIKELY(mod_it->init() != EINA_TRUE))
          {
             CRI("Failed to initialize module '%s'", mod_it->name);
             goto modules_shutdown;
          }
     }
   /*
    * Create the GUI client
    */
   s_nvim *const nvim = nvim_new(&opts, nvim_prog,
                                 (unsigned int)(argc - args),
                                 (const char *const *)(&argv[args]));
   if (EINA_UNLIKELY(! nvim))
     {
        CRI("Failed to create a NeoVim instance");
        goto modules_shutdown;
     }

   /*
    * Run the main loop
    */
   elm_run();

   nvim_free(nvim);

   /* Everything seemed to have run fine :) */
   return_code = EXIT_SUCCESS;
modules_shutdown:
   for (--mod_it; mod_it >= _modules; mod_it--)
     mod_it->shutdown();
   eina_strbuf_free(_edje_file);
log_unregister:
   eina_log_domain_unregister(_eovim_log_domain);
end:
   return return_code;
}
ELM_MAIN()
