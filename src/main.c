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

#include "Envim.h"

int _envim_log_domain = -1;

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

   MODULE(api),
   MODULE(nvim),
   MODULE(request),

#undef MODULE
};

static void
_init_func(void *data)
{
   s_nvim *const nvim = data;
   nvim_rpc(nvim, REQUEST_GET_API_INFO);
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED,
         char **argv EINA_UNUSED)
{
   int return_code = EXIT_FAILURE;

   /* First step: initialize the logging framework */
   _envim_log_domain = eina_log_domain_register("envim", EINA_COLOR_RED);
   if (EINA_UNLIKELY(_envim_log_domain < 0))
     {
        EINA_LOG_CRIT("Failed to create log domain");
        goto end;
     }

   /*
    * Initialize all the different modules that compose Envim.
    */
   const s_module *const mod_last = &(_modules[EINA_C_ARRAY_LENGTH(_modules) - 1]);
   const s_module *mod_it;
   for (mod_it = _modules; mod_it <= mod_last; mod_it++)
     {
        printf("init %s\n", mod_it->name);
       if (EINA_UNLIKELY(mod_it->init() != EINA_TRUE))
         {
            CRI("Failed to initialize module '%s'", mod_it->name);
            goto modules_shutdown;
         }
     }

   s_nvim *const nvim = nvim_new();

   ecore_job_add(_init_func, nvim);

   /* Run the main loop */
   elm_run();

   nvim_free(nvim);


   /* Everything seemed to have run fine :) */
   return_code = EXIT_SUCCESS;

modules_shutdown:
   for (--mod_it; mod_it >= _modules; mod_it--)
     mod_it->shutdown();
   eina_log_domain_unregister(_envim_log_domain);
end:
   return return_code;
}
ELM_MAIN()
