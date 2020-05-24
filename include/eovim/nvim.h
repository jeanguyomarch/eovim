/* This file is part of Eovim, which is under the MIT License ****************/

#ifndef __EOVIM_NVIM_H__
#define __EOVIM_NVIM_H__

#include "eovim/types.h"
#include "eovim/options.h"
#include "eovim/nvim_helper.h"
#include "eovim/gui.h"

#include <Eina.h>
#include <Ecore.h>
#include <msgpack.h>

#define NVIM_VERSION_MAJOR(Nvim) ((Nvim)->version.major)
#define NVIM_VERSION_MINOR(Nvim) ((Nvim)->version.minor)
#define NVIM_VERSION_PATCH(Nvim) ((Nvim)->version.patch)

#define NVIM_VERSION_EQ(Nvim, Major, Minor, Patch) \
   (NVIM_VERSION_MAJOR(Nvim) == (Major) && \
    NVIM_VERSION_MINOR(Nvim) == (Minor) && \
    NVIM_VERSION_PATCH(Nvim) == (Patch))

/* Used for indexing event_handlers */
typedef enum
{
   NVIM_HANDLER_ADD   = 0,
   NVIM_HANDLER_DEL   = 1,
   NVIM_HANDLER_DATA  = 2,
   NVIM_HANDLER_ERROR = 3,
   NVIM_HANDLERS_LAST /* Sentinel */
} e_nvim_handler;

struct nvim
{
   s_gui gui;
   s_version version; /**< The neovim's version */
   uint64_t channel;
   s_config *config;
   const s_options *opts;

   Ecore_Exe *exe;

   Ecore_Event_Handler *event_handlers[NVIM_HANDLERS_LAST];
   Eina_Inlist *requests;

   msgpack_unpacker unpacker;

   /* The following msgpack structures must be handled on the main loop only */
   msgpack_sbuffer sbuffer;
   msgpack_packer packer;
   uint32_t request_id;

   Eina_Hash *modes;
   Eina_Bool mouse_enabled;

   struct {
     Eina_Bool linegrid;
     Eina_Bool multigrid;
     Eina_Bool cmdline;
     Eina_Bool wildmenu;
     Eina_Bool tabline;
     Eina_Bool popupmenu;
   } features;
};


s_nvim *nvim_new(const s_options *opts, const char *const args[]);
void nvim_free(s_nvim *nvim);
uint32_t nvim_next_uid_get(s_nvim *nvim);
void nvim_mouse_enabled_set(s_nvim *nvim, Eina_Bool enable);
Eina_Bool nvim_mouse_enabled_get(const s_nvim *nvim);
void nvim_attach(s_nvim *nvim);

/**
 * Flush the msgpack buffer to the neovim instance, by writing to its standard
 * input
 *
 * @param[in] nvim The neovim handle
 * @return EINA_TRUE on success, EINA_FALSE on failure.
 */
Eina_Bool nvim_flush(s_nvim *nvim);

struct mode *nvim_mode_new(void);
void nvim_mode_free(struct mode *mode);

#endif /* ! __EOVIM_NVIM_H__ */
