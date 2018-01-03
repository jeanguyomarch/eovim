/*
 * Copyright (c) 2017-2018 Jean Guyomarc'h
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
#include "eovim/options.h"

int _eovim_log_domain = -1;

static Eina_Bool _in_tree = EINA_FALSE;
static Eina_Strbuf *_edje_file = NULL;
static Eina_Inlist *_plugins = NULL;

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

Eina_Inlist *
main_plugins_get(void)
{
   return _plugins;
}

/* This function is a hack around a bug in the EFL backtrace bug.  If an ERR()
 * or CRI() is hit, for any reason, Eovim crash due to invalid memory handling
 * in eina_bt.
 *
 * Since Eovim is using Elementary, eina has already been initialized when
 * entering the elm_main(), and the backtrace level has already been set.  So
 * we use a constructor function (GNU extension) to set the environment with an
 * EINA_LOG_BACKTRACE that will disable all backtraces, unless it has been
 * previously specified by the user.
 */
static void __attribute__((constructor))
__constructor(void)
{
   const char bt_env[] = "EINA_LOG_BACKTRACE";
   if (! getenv(bt_env))
     setenv(bt_env, "-1", 1);
}

EAPI_MAIN int elm_main(int argc, char **argv);
EAPI_MAIN int
elm_main(int argc,
         char **argv)
{
   int return_code = EXIT_FAILURE;
   s_options opts;
   options_defaults_set(&opts);

   /* First step: initialize the logging framework */
   _eovim_log_domain = eina_log_domain_register("eovim", EINA_COLOR_RED);
   if (EINA_UNLIKELY(_eovim_log_domain < 0))
     {
        EINA_LOG_CRIT("Failed to create log domain");
        goto end;
     }

   /* Do the getopts */
   const e_options_result opts_res = options_parse(argc, (const char **)argv, &opts);
   switch (opts_res)
     {
      case OPTIONS_RESULT_QUIT:
         return_code = EXIT_SUCCESS;
         goto log_unregister;

      case OPTIONS_RESULT_ERROR:
         goto log_unregister;

      case OPTIONS_RESULT_CONTINUE:
         break;

      default:
         CRI("Wtf?! Enum value out of switch");
         goto log_unregister;
     }

#ifdef HAVE_PLUGINS
   /* If plugin-is are supported, we enable the plugins as long as the
    * --no-plugins option is NOT passed to eovim */
   plugin_enabled_set(! opts.no_plugins);
#else
   /* Plugins are not supported, disable them */
   plugin_enabled_set(EINA_FALSE);
#endif

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

   if (EINA_UNLIKELY(! _edje_file_init(opts.theme)))
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

   /*=========================================================================
    * Load the plugins
    *========================================================================*/
   _plugins = plugin_list_new();

   /*=========================================================================
    * Create the Neovim handler
    *========================================================================*/
   s_nvim *const nvim = nvim_new(&opts, (const char *const *)argv);
   if (EINA_UNLIKELY(! nvim))
     {
        CRI("Failed to create a NeoVim instance");
        goto plugins_shutdown;
     }

   /*=========================================================================
    * Start the main loop
    *========================================================================*/
   elm_run();

   nvim_free(nvim);

   /* Everything seemed to have run fine :) */
   return_code = EXIT_SUCCESS;
plugins_shutdown:
   plugin_list_free(_plugins);
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
