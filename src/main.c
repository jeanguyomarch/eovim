/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/keymap.h>
#include <eovim/nvim.h>
#include <eovim/nvim_api.h>
#include <eovim/version.h>
#include <eovim/nvim_request.h>
#include <eovim/nvim_event.h>
#include <eovim/termview.h>
#include <eovim/main.h>
#include <eovim/log.h>

#include <Ecore_Getopt.h>

int _eovim_log_domain = -1;

static Eina_Bool _in_tree = EINA_FALSE;
static Eina_Strbuf *_edje_file = NULL;

struct module {
	const char *const name;
	Eina_Bool (*const init)(void);
	void (*const shutdown)(void);
};

static const struct module _modules[] = {
#define MODULE(name_)                                                                              \
	{                                                                                          \
		.name = #name_, .init = &name_##_init, .shutdown = &name_##_shutdown               \
	}

	MODULE(keymap),	      MODULE(nvim_api),	      MODULE(nvim_request), MODULE(nvim_event),
	MODULE(gui_wildmenu), MODULE(gui_completion), MODULE(termview),

#undef MODULE
};

static const Ecore_Getopt options_desc = {
	"eovim",
	"%prog [options] [file...]",
	EOVIM_VERSION,
	"(c) 2017-2020 Jean Guyomarc'h and others",
	"MIT",
	"The Enlightened Neovim - a GUI client for Neovim written with the EFL\n\n"
	"Unknown options will be directly forwarded to neovim.",
	EINA_FALSE, /* Not strict */
	{ ECORE_GETOPT_STORE_STR('\0', "nvim", "Path to the Neovim program"),
	  ECORE_GETOPT_STORE_STR('t', "theme", "Path to the Edje theme"),
	  ECORE_GETOPT_STORE_TRUE('M', "maximized", "Start eovim in a maximized window"),
	  ECORE_GETOPT_STORE_TRUE('F', "fullscreen", "Start eovim in a fullscreen window"),
	  ECORE_GETOPT_CALLBACK_ARGS(
		  'g', "geometry",
		  "Set the initial dimensions of the window (e.g. 120x40 for a 120x40 cells window)",
		  "COLUMNSxROWS", &ecore_getopt_callback_size_parse, NULL),
	  ECORE_GETOPT_VERSION('V', "version"), ECORE_GETOPT_COPYRIGHT('C', "copyright"),
	  ECORE_GETOPT_LICENSE('L', "license"), ECORE_GETOPT_HELP('h', "help"),
	  ECORE_GETOPT_SENTINEL }
};

static Eina_Bool _edje_file_init(const char *theme)
{
	EINA_SAFETY_ON_NULL_RETURN_VAL(theme, EINA_FALSE);

	const char *const dir = main_in_tree_is() ? BUILD_DATA_DIR : elm_app_data_dir_get();

	_edje_file = eina_strbuf_new();
	if (EINA_UNLIKELY(!_edje_file)) {
		CRI("Failed to create Strbuf");
		return EINA_FALSE;
	}
	eina_strbuf_append_printf(_edje_file, "%s/themes/%s.edj", dir, theme);
	return EINA_TRUE;
}

Eina_Bool main_in_tree_is(void)
{
	return _in_tree;
}

const char *main_edje_file_get(void)
{
	return eina_strbuf_string_get(_edje_file);
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
static void __attribute__((constructor)) __constructor(void)
{
	setenv("EINA_LOG_BACKTRACE", "-1", 0);
	eina_log_domain_level_set("efreet_cache", 0);
#ifdef NDEBUG
	eina_log_domain_level_set("eina_safety", 0);
#endif
}

EAPI_MAIN int elm_main(int argc, char **argv);
EAPI_MAIN int elm_main(int argc, char **argv)
{
	struct options opts = {
		.geometry = { 0, 0, 120, 40 },
		.nvim = "nvim",
		.theme = "default",
		.fullscreen = EINA_FALSE,
		.maximized = EINA_FALSE,
	};
	Eina_Bool quit = EINA_FALSE;
	Ecore_Getopt_Value values[] = { ECORE_GETOPT_VALUE_STR(opts.nvim),
					ECORE_GETOPT_VALUE_STR(opts.theme),
					ECORE_GETOPT_VALUE_BOOL(opts.maximized),
					ECORE_GETOPT_VALUE_BOOL(opts.fullscreen),
					ECORE_GETOPT_VALUE_PTR_CAST(opts.geometry),
					ECORE_GETOPT_VALUE_BOOL(quit),
					ECORE_GETOPT_VALUE_BOOL(quit),
					ECORE_GETOPT_VALUE_BOOL(quit),
					ECORE_GETOPT_VALUE_BOOL(quit),
					ECORE_GETOPT_VALUE_NONE };

	int return_code = EXIT_FAILURE;

	/* First step: initialize the logging framework */
	_eovim_log_domain = eina_log_domain_register("eovim", EINA_COLOR_RED);
	if (EINA_UNLIKELY(_eovim_log_domain < 0)) {
		EINA_LOG_CRIT("Failed to create log domain");
		goto end;
	}

	const int args = ecore_getopt_parse(&options_desc, values, argc, argv);
	if (args < 0) {
		CRI("Failed to parser command-line options");
		goto log_unregister;
	}
	if (quit) {
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

	if (EINA_UNLIKELY(!_edje_file_init(opts.theme))) {
		CRI("Failed to compose edje file path");
		goto log_unregister;
	}

	/*
	 * Initialize all the different modules that compose Eovim.
	 */
	const struct module *const mod_last = &(_modules[EINA_C_ARRAY_LENGTH(_modules) - 1]);
	const struct module *mod_it;
	for (mod_it = _modules; mod_it <= mod_last; mod_it++) {
		if (EINA_UNLIKELY(mod_it->init() != EINA_TRUE)) {
			CRI("Failed to initialize module '%s'", mod_it->name);
			goto modules_shutdown;
		}
	}

	/*=========================================================================
    * Create the Neovim handler
    *========================================================================*/
	struct nvim *const nvim = nvim_new(&opts, (const char *const *)argv + args);
	if (EINA_UNLIKELY(!nvim)) {
		CRI("Failed to create a NeoVim instance");
		goto modules_shutdown;
	}

	/*=========================================================================
    * Start the main loop
    *========================================================================*/
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
