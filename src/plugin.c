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

#include "eovim/plugin.h"
#include "eovim/nvim_event.h"
#include "eovim/log.h"
#include "eovim/main.h"
#include <Elementary.h>

typedef enum
{
   E_LOAD_PATH_IN_TREE,
   E_LOAD_PATH_LOCAL,
   E_LOAD_PATH_SYSTEM,
   __E_LOAD_PATH_LAST
} e_load_path;

static char *_load_paths[__E_LOAD_PATH_LAST];
static Eina_Bool _enable_plugins = EINA_TRUE;

#define PLUGIN_ENABLED_OR_RETURN(...) \
   do { if (! _enable_plugins) { return __VA_ARGS__; } } while (0)

static char *
_paths_join(const char *dir,
            const char *name)
{
   /*
    * Join 'dir' and 'name' as path components in a single string
    */
   const size_t name_len = strlen(name);
   const size_t dir_len = strlen(dir);
   const size_t len = name_len + dir_len + 1; /* 1 is the / */
   char *const path = malloc(len + 1); /* Plus trailing \0 */
   if (EINA_UNLIKELY(! path))
     {
        CRI("Failed to allocate memory");
        return NULL;
     }
   memcpy(path, dir, dir_len);
   path[dir_len] = '/';
   memcpy(path + dir_len + 1, name, name_len);
   path[len] = '\0';
   return path;
}

static void
_list_plugins_cb(const char *name,
                 const char *dir,
                 void *data)
{
   Eina_Inlist **list = data;
   char *const path = _paths_join(dir, name);
   if (EINA_UNLIKELY(! path)) { return; }

   /* If the extension is not MODULE_EXT, ignore */
   const char *const ptr = strrchr(name, '.');
   if ((ptr == NULL) || (strcmp(ptr, MODULE_EXT) != 0))
     goto fail;

   /* Create a stringshare that holds the name of the plugin (filename stripped
    * from the extension) */
   Eina_Stringshare *const plug_name =
      eina_stringshare_add_length(name, (unsigned int)(ptr - name));
   if (EINA_UNLIKELY(! plug_name))
     {
        CRI("Failed to create stringshare");
        goto fail_del;
     }

   /* Go through the plugins already registered, to check if we can add this
    * plugin or if it has to be rejected */
     {
        s_plugin *plug_it;
        EINA_INLIST_FOREACH(*list, plug_it)
          {
             if (plug_it->name == plug_name)
               {
                  WRN("Plugin '%s' is already registered", plug_name);
                  goto fail_del;
               }
          }
     }

   /* Create a new container for the plugin and add it to the list */
   s_plugin *const plug = calloc(1, sizeof(s_plugin));
   if (EINA_UNLIKELY(! plug))
     {
        CRI("Failed to allocate memory while processing '%s'", dir);
        goto fail_free;
     }

   plug->name = plug_name;
   plug->module = eina_module_new(path);
   if (EINA_UNLIKELY(! plug->module))
     {
        CRI("Failed to create module from path '%s'", path);
        goto fail_free;
     }

   *list = eina_inlist_append(*list, EINA_INLIST_GET(plug));
   INF("Found plugin '%s' at path '%s'", plug->name, path);

   free(path);
   return;
fail_free:
   free(plug);
fail_del:
   eina_stringshare_del(plug_name);
fail:
   free(path);
}

Eina_Inlist *
plugin_list_new(void)
{
   Eina_Inlist *list = NULL;
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_load_paths); i++)
     if (_load_paths[i] != NULL)
       eina_file_dir_list(_load_paths[i], EINA_FALSE, _list_plugins_cb, &list);
   return list;
}

void
plugin_list_free(Eina_Inlist *list)
{
   s_plugin *plug;
   EINA_INLIST_FREE(list, plug)
     {
        list = eina_inlist_remove(list, EINA_INLIST_GET(plug));
        eina_stringshare_del(plug->name);
        eina_module_free(plug->module);
        free(plug);
     }
}

Eina_Bool
plugin_load(s_plugin *plugin)
{
   /* Don't load plugins if plugins are disabled at run-time */
   PLUGIN_ENABLED_OR_RETURN(EINA_FALSE);

   if (plugin->loaded) { return EINA_TRUE; }

   /* Load the shared object */
   Eina_Bool ok = eina_module_load(plugin->module);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to load plugin '%s'", plugin->name);
        return EINA_FALSE;
     }

   /* Retrieve eovim's entry point. Careful, it will be a pointer to a function
    * pointer, and as such must be dereferenced prior using it as a callback. */
   const char *const symbol = "__eovim_plugin_symbol";
   f_event_cb *const fptr = eina_module_symbol_get(plugin->module, symbol);
   if (EINA_UNLIKELY((!fptr) && (!(*fptr))))
     {
        CRI("Failed to load symbol '%s' from plugin '%s'", symbol, plugin->name);
        goto unload;
     }
   plugin->callback = *fptr;

   /* Register it within Eovim's callbacks table */
   ok = nvim_event_plugin_register(plugin->name, plugin->callback);
   if (EINA_UNLIKELY(! ok))
     {
        CRI("Failed to register plugin '%s'", plugin->name);
        goto unload;
     }
   plugin->loaded = EINA_TRUE;

   return EINA_TRUE;
unload:
   eina_module_unload(plugin->module);
   return EINA_FALSE;
}

Eina_Bool
plugin_unload(s_plugin *plugin)
{
   PLUGIN_ENABLED_OR_RETURN(EINA_FALSE);
   if (plugin->loaded)
     {
        const Eina_Bool ok = eina_module_unload(plugin->module);
        if (EINA_UNLIKELY(! ok))
          CRI("Failed to unload plugin");
        else
          plugin->loaded = EINA_FALSE;
     }
   return plugin->loaded;
}

Eina_Bool
plugin_init(void)
{
   Eina_Strbuf *const buf = eina_strbuf_new();
   if (EINA_UNLIKELY(! buf))
     {
        CRI("Failed to create string buffer");
        return EINA_FALSE;
     }

   /* When we search for plugins in the source tree, duplicate the static
    * string */
   if (main_in_tree_is())
     _load_paths[E_LOAD_PATH_IN_TREE] = strdup(BUILD_PLUGINS_DIR);

   /* Add the the load paths the local load path, if it does exist */
   eina_strbuf_append_printf(buf, "%s/.local/lib%s/eovim",
                             eina_environment_home_get(), LIB_SUFFIX);
   if (ecore_file_is_dir(eina_strbuf_string_get(buf)))
     _load_paths[E_LOAD_PATH_LOCAL] = eina_strbuf_string_steal(buf);
   eina_strbuf_reset(buf);

   /* Add the system local path, if it does exist */
   eina_strbuf_append_printf(buf, "%s/eovim", elm_app_lib_dir_get());
   if (ecore_file_is_dir(eina_strbuf_string_get(buf)))
     _load_paths[E_LOAD_PATH_SYSTEM] = eina_strbuf_string_steal(buf);
   eina_strbuf_free(buf);

   return EINA_TRUE;
}

void
plugin_shutdown(void)
{
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_load_paths); i++)
     free(_load_paths[i]);
}

void
plugin_enabled_set(Eina_Bool enabled)
{
   _enable_plugins = !!enabled;
}

Eina_Bool
plugin_enabled_get(void)
{
   return _enable_plugins;
}

unsigned int
plugin_list_load(const Eina_Inlist *plugins,
                 const Eina_List *load_list)
{
   Eina_Stringshare *plug_enable;
   s_plugin *plug;
   Eina_List *l;
   unsigned int loaded = 0;

   EINA_LIST_FOREACH((Eina_List *)load_list, l, plug_enable)
     {
        EINA_INLIST_FOREACH(plugins, plug)
          {
             if (plug_enable == plug->name)
               {
                  const Eina_Bool ok = plugin_load(plug);
                  if (ok) loaded++;
               }
          }
     }
   return loaded;
}
