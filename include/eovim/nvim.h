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

#define NVIM_VERSION_EQ(Nvim, Major, Minor, Patch)                                                 \
	(NVIM_VERSION_MAJOR(Nvim) == (Major) && NVIM_VERSION_MINOR(Nvim) == (Minor) &&             \
	 NVIM_VERSION_PATCH(Nvim) == (Patch))

struct nvim {
	struct gui gui;
	struct version version; /**< The neovim's version */
	uint64_t channel;
	const struct options *opts;

	Ecore_Exe *exe;

	Ecore_Event_Handler *event_handlers[4];
	Eina_Inlist *requests;

	msgpack_unpacker unpacker;

	/* The following msgpack structures must be handled on the main loop only */
	msgpack_sbuffer sbuffer;
	msgpack_packer packer;
	uint32_t request_id;

	Eina_Hash *modes;

	Eina_Hash *hl_groups;

	/* Map of strings that associates to a kind identifier (used by completion) to
	 * a style string that is compatible with Evas_textblock */
	Eina_Hash *kind_styles;
	/* Map of strings that associates a cmdline prompt to
	 * a style string that is compatible with Evas_textblock */
	Eina_Hash *cmdline_styles;
	Eina_Bool mouse_enabled;

	struct {
		Eina_Bool linegrid;
		Eina_Bool multigrid;
		Eina_Bool cmdline;
		Eina_Bool tabline;
		Eina_Bool popupmenu;
	} features;
};

struct nvim *nvim_new(const struct options *opts, const char *const args[]);
void nvim_free(struct nvim *nvim);
uint32_t nvim_next_uid_get(struct nvim *nvim);
void nvim_mouse_enabled_set(struct nvim *nvim, Eina_Bool enable);
Eina_Bool nvim_mouse_enabled_get(const struct nvim *nvim);
void nvim_attach(struct nvim *nvim);

/**
 * Flush the msgpack buffer to the neovim instance, by writing to its standard
 * input
 *
 * @param[in] nvim The neovim handle
 * @return EINA_TRUE on success, EINA_FALSE on failure.
 */
Eina_Bool nvim_flush(struct nvim *nvim);

struct mode *nvim_mode_new(void);
void nvim_mode_free(struct mode *mode);

#endif /* ! __EOVIM_NVIM_H__ */
