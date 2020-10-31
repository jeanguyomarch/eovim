/* This file is part of Eovim, which is under the MIT License ****************/

#include <eovim/log.h>
#include <eovim/gui.h>
#include <eovim/main.h>
#include <eovim/nvim_helper.h>
#include <eovim/nvim_api.h>
#include <eovim/nvim.h>

#include "gui_private.h"

#include <Ecore_Input.h>


struct keyin {
	Ecore_Event_Handler *key_down_handler;
	Eina_List *seq_compose;
	/** A map associating EflKeyName -> VimKeyName (both const char*) */
	Eina_Hash *keymap;
	struct nvim *nvim;
	struct gui *gui;
};

/* This is just a shortcut to define a keymap that is self-associating */
#define X_IDENTITY(KeyName) X(KeyName, KeyName)

/* This is a compile-time list of key mapping from EFL to ViM. These are remarkable
 * keys that must be encoded by ViM in a textual way (e.g. F1 is <F1>).
 *
 * Every argument has the form:
 *      X(Key Name, Vim Name)
 */
#define KEYMAP(X) \
	X("Up", "Up")\
	X("Down", "Down")\
	X("Left", "Left")\
	X("Right", "Right")\
	X("F1", "F1")\
	X("F2", "F2")\
	X("F3", "F3")\
	X("F4", "F4")\
	X("F5", "F5")\
	X("F6", "F6")\
	X("F7", "F7")\
	X("F8", "F8")\
	X("F9", "F9")\
	X("F10", "F10")\
	X("F11", "F11")\
	X("F12", "F12")\
	X("F13", "F13")\
	X("F14", "F14")\
	X("F15", "F15")\
	X("F16", "F16")\
	X("F17", "F17")\
	X("F18", "F18")\
	X("F19", "F19")\
	X("F20", "F20")\
	X("F21", "F21")\
	X("F22", "F22")\
	X("F23", "F23")\
	X("F24", "F24")\
	X("F25", "F25")\
	X("F26", "F26")\
	X("F27", "F27")\
	X("F28", "F28")\
	X("F29", "F29")\
	X("F30", "F30")\
	X("F31", "F31")\
	X("F32", "F32")\
	X("F33", "F33")\
	X("F34", "F34")\
	X("F34", "F34")\
	X("F35", "F35")\
	X("F37", "F37")\
	X("Home", "Home")\
	X("End", "End")\
	X("Tab", "Tab")\
	X("BackSpace", "BS")\
	X("less", "lt")\
	X("Prior", "PageUp")\
	X("Next", "PageDown")\
	X("Delete", "Del")\
	X("space", "Space")\
	X("ISO_Left_Tab", "Tab")\
	X("backslash", "Bslash")\
	/**/


static void _keys_send(struct keyin *const kin, const char *keys, unsigned int size)
{
	nvim_api_input(kin->nvim, keys, size);
	gui_cursor_key_pressed(kin->gui);
}

static inline bool _composing_is(const struct keyin *const kin)
{
	/* Composition is pending if the seq_compose list is not empty */
	return kin->seq_compose == NULL;
}

static inline void _composition_reset(struct keyin *const kin)
{
	/* Discard all elements within the list */
	Eina_Stringshare *str;
	EINA_LIST_FREE(kin->seq_compose, str)
		eina_stringshare_del(str);
}

static inline void _composition_add(struct keyin *const kin, const Ecore_Event_Key *const key)
{
	/* Add the key as a stringshare in the seq list. Hence, starting the
	 * composition */
	Eina_Stringshare *const shr = eina_stringshare_add(key->key);
	kin->seq_compose = eina_list_append(kin->seq_compose, shr);
}

/*
 * This function returns EINA_TRUE when the key was handled within this
 * functional unit. When it returns EINA_FALSE, the caller should handle the
 * key itself.
 */
static Eina_Bool _compose(struct keyin *const kin, const Ecore_Event_Key *const key)
{
	if (_composing_is(kin)) {
		char *res = NULL;

		/* We discard LOCK modifiers as they do not interest us. We only
                 * want to filter modifiers sucj as shift, ctrl, etc.
                 * Filtering LOCK-only keys actually result in completely blocking
                 * input during composition if something like "num lock" is ON. */
		const unsigned int modifiers =
			key->modifiers & (unsigned)(~ECORE_EVENT_LOCK_SCROLL) &
			(unsigned)(~ECORE_EVENT_LOCK_NUM) & (unsigned)(~ECORE_EVENT_LOCK_CAPS) &
			(unsigned)(~ECORE_EVENT_LOCK_SHIFT);

		/* When composition is enabled, we want to skip modifiers, and only feed
		 * non-modified keys to the composition engine. */
		if (modifiers != 0u)
			return EINA_TRUE;

		/* Add the current key to the composition list, and compute */
		_composition_add(kin, key);
		const Ecore_Compose_State state = ecore_compose_get(kin->seq_compose, &res);
		if (state == ECORE_COMPOSE_DONE) {
			/* We composed! Write the composed key! */
			_composition_reset(kin);
			if (EINA_LIKELY(NULL != res)) {
				_keys_send(kin, res, (unsigned int)strlen(res));
				free(res);
				return EINA_TRUE;
			}
		} else if (state == ECORE_COMPOSE_NONE) {
			/* The composition yield nothing. Reset */
			_composition_reset(kin);
		}
	} else /* Not composing yet */
	{
		/* Add the key to the composition engine */
		_composition_add(kin, key);
		const Ecore_Compose_State state = ecore_compose_get(kin->seq_compose, NULL);
		if (state != ECORE_COMPOSE_MIDDLE) {
			/* Nope, this does not allow composition */
			_composition_reset(kin);
		} else {
			return EINA_TRUE;
		} /* Composing.... */
	}

	/* Delegate the key to the caller */
	return EINA_FALSE;
}

/**
 * If \p cond is true add the modifier \p mod to the cell of \p buf at the index
 * pointed by \p size.  A dash '-' is then appended.
 * The index pointed by \p size is then incremented by two to update the caller's
 * value.
 *
 * \return
 * If the operation would overflow (the index pointed by \p size is greater than
 * 61) this function does nothing and returns EINA_FALSE.
 * Otherwise, this function returns EINA_TRUE, indicating the absence of error.
 */
static Eina_Bool _add_modifier(char buf[64], const int cond, const char mod, int *const size)
{
	if (cond) {
		if (EINA_UNLIKELY(*size + 2 >= 64)) {
			ERR("Buffer overflow! Not sending key to neovim");
			return EINA_FALSE;
		}

		buf[(*size) + 0] = mod;
		buf[(*size) + 1] = '-';
		(*size) += 2;
	}
	return EINA_TRUE;
}

static Eina_Bool _key_down_cb(void *const data, const int type EINA_UNUSED,
				       void *const event)
{
	struct keyin *const kin = data;
	const Ecore_Event_Key *const ev = event;
	int send_size = 0;
	const char *const vim_key = eina_hash_find(kin->keymap, ev->key);
	char buf[64];
	const char *send;
	const char caps[] = "Caps_Lock";

#if 0
   printf("key      : %s\n", ev->key);
   printf("keyname  : %s\n", ev->keyname);
   printf("string   : %s\n", ev->string);
   printf("compose  : %s\n", ev->compose);
   printf("modifiers: 0x%x\n", ev->modifiers);
   printf("------------------------\n");
#endif

	/* Did we press the Caps_Lock key? We can either have enabled or disabled
	 * caps lock. */
	if (!strcmp(ev->key, caps)) /* Caps lock is pressed */
	{
		if (ev->modifiers & ECORE_EVENT_LOCK_CAPS) /* DISABLE */
		{
			/* If we press caps lock with prior caps locks, it means we
			 * just DISABLED the caps lock */
			gui_caps_lock_dismiss(kin->gui);
		} else /* ENABLE */
		{
			/* If we press caps lock without prior caps locks, it means we
			 * just ENABLED the caps lock */
			gui_caps_lock_alert(kin->gui);
		}
	}

	/* If the key produces nothing. Stop */
	if ((ev->string == NULL) && (vim_key == NULL))
		return ECORE_CALLBACK_PASS_ON;

	/* Try the composition. When this function returns EINA_TRUE, it
	 * already worked out, nothing more to do. */
	if (_compose(kin, ev))
		return ECORE_CALLBACK_PASS_ON;

	const Eina_Bool ctrl = ev->modifiers & ECORE_EVENT_MODIFIER_CTRL;
	const Eina_Bool super = ev->modifiers & ECORE_EVENT_MODIFIER_WIN;
	const Eina_Bool alt = ev->modifiers & ECORE_EVENT_MODIFIER_ALT;
	const Eina_Bool shift = ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT;

	/* Register modifiers. Ctrl and shift are special: we enable composition
	 * only if the key is present in the keymap (it is a special key). We
	 * disregard shift alone, because it would just mean "uppercase" if alone;
	 * however when combined with the keymap, we will compose! */
	const Eina_Bool compose = ctrl || super || alt || (shift && vim_key);

	if (compose) {
		const char *const key = vim_key ? vim_key : ev->key;
		if (!key) {
			return ECORE_CALLBACK_PASS_ON;
		}

		/* Compose a string containing the textual representation of the
		 * special keys to be sent to neovim.
		 * Search :help META for details.
		 *
		 * We first compose the first part with the modifiers. E.g. <C-S-
		 */
		buf[0] = '<';
		send_size = 1;
		if (!_add_modifier(buf, ctrl, 'C', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}
		if (!_add_modifier(buf, shift, 'S', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}
		if (!_add_modifier(buf, super, 'D', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}
		if (!_add_modifier(buf, alt, 'A', &send_size)) {
			return ECORE_CALLBACK_PASS_ON;
		}

		/* Add the real key after the modifier, and close the bracket */
		assert(send_size < (int)sizeof(buf));
		const size_t len = sizeof(buf) - (size_t)send_size;
		const int ret = snprintf(buf + send_size, len, "%s>", key);
		if (EINA_UNLIKELY(ret < 2)) {
			ERR("Failed to compose key.");
			return ECORE_CALLBACK_PASS_ON;
		}
		send_size += ret;
		send = buf;
	} else if (vim_key) {
		send_size = snprintf(buf, sizeof(buf), "<%s>", vim_key);
		if (EINA_UNLIKELY(send_size < 3)) {
			ERR("Failed to write key to string");
			return ECORE_CALLBACK_PASS_ON;
		}
		send = buf;
	} else {
		assert(ev->string != NULL);
		send = ev->string;
		send_size = (int)strlen(send);
	}

	/* If a key is availabe pass it to neovim and update the ui */
	if (EINA_LIKELY(send_size > 0))
		_keys_send(kin, send, (unsigned int)send_size);
	else
		DBG("Unhandled key '%s'", ev->key);
	return ECORE_CALLBACK_PASS_ON;
}

struct keyin *keyin_add(struct gui *const gui)
{
	struct keyin *const kin = calloc(1, sizeof(*kin));
	if (EINA_UNLIKELY(! kin)) {
		CRI("Failed to allocate memory for keyin");
		return NULL;
	}
	kin->gui = gui;
	kin->nvim = gui->nvim;

	/* We use the ECORE event instead of the evas smart callback because
	 * the key modifiers are much more convenient as Ecore_Event_Key events
	 * than Evas_Event_Key_Down.
	 * This is not a problem, because the grid is the only widget in
	 * the Evas that actually requires keyboard use */
	kin->key_down_handler = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, &_key_down_cb, kin);
	if (EINA_UNLIKELY(!kin->key_down_handler)) {
		CRI("Failed to create event handler");
		goto fail_handler_create;
	}

	kin->keymap = eina_hash_string_superfast_new(NULL);
	if (EINA_UNLIKELY(!kin->keymap)) {
		CRI("Failed to create hash table");
		goto fail_hash_create;
	}

#define KEYMAP_INSERT(KeyName, VimName) \
	if (EINA_UNLIKELY(!eina_hash_add(kin->keymap, KeyName, VimName))) { \
		CRI("Failed to add keymap entry '%s' -> '%s'", KeyName, VimName); \
		goto fail_hash_populate; \
	}

	KEYMAP(KEYMAP_INSERT)
#undef KEYMAP_INSERT

	return kin;

fail_hash_populate:
	eina_hash_free(kin->keymap);
fail_hash_create:
	ecore_event_handler_del(kin->key_down_handler);
fail_handler_create:
	free(kin);
	return NULL;
}

void keyin_del(struct keyin *const kin)
{
	ecore_event_handler_del(kin->key_down_handler);
	_composition_reset(kin);
	eina_hash_free(kin->keymap);
	free(kin);
}
