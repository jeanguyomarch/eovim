/* This file is part of Eovim, which is under the MIT License ****************/

#include "eovim/keymap.h"
#include "eovim/log.h"

struct kv_keymap {
	const char *const key;
	const struct keymap keymap;
};

#define KM(Key, Std)                                                                               \
	{                                                                                          \
		.key = Key, .keymap = {.size = sizeof(Std) - 1, .name = Std }                      \
	}

#define KM_IDENT(Key) KM(Key, Key)

static const struct kv_keymap _map[] = {
	KM_IDENT("Up"),
	KM_IDENT("Down"),
	KM_IDENT("Left"),
	KM_IDENT("Right"),
	KM_IDENT("F1"),
	KM_IDENT("F2"),
	KM_IDENT("F3"),
	KM_IDENT("F4"),
	KM_IDENT("F5"),
	KM_IDENT("F6"),
	KM_IDENT("F7"),
	KM_IDENT("F8"),
	KM_IDENT("F9"),
	KM_IDENT("F10"),
	KM_IDENT("F11"),
	KM_IDENT("F12"),
	KM_IDENT("F13"),
	KM_IDENT("F14"),
	KM_IDENT("F15"),
	KM_IDENT("F16"),
	KM_IDENT("F17"),
	KM_IDENT("F18"),
	KM_IDENT("F19"),
	KM_IDENT("F20"),
	KM_IDENT("F21"),
	KM_IDENT("F22"),
	KM_IDENT("F23"),
	KM_IDENT("F24"),
	KM_IDENT("F25"),
	KM_IDENT("F26"),
	KM_IDENT("F27"),
	KM_IDENT("F28"),
	KM_IDENT("F29"),
	KM_IDENT("F30"),
	KM_IDENT("F31"),
	KM_IDENT("F32"),
	KM_IDENT("F33"),
	KM_IDENT("F34"),
	KM_IDENT("F34"),
	KM_IDENT("F35"),
	KM_IDENT("F37"),
	KM_IDENT("Home"),
	KM_IDENT("End"),
	KM("BackSpace", "BS"),
	KM("less", "lt"),
	KM("Prior", "PageUp"),
	KM("Next", "PageDown"),
	KM("Delete", "Del"),
	KM("space", "Space"),
	KM_IDENT("Tab"),
	KM("ISO_Left_Tab", "Tab"),
	KM("backslash", "Bslash"),
};

static Eina_Hash *_keymap = NULL;

Eina_Bool keymap_init(void)
{
	/* First, create the hash table that wil hold the keymap */
	_keymap = eina_hash_string_superfast_new(NULL);
	if (EINA_UNLIKELY(!_keymap)) {
		CRI("Failed to create hash table");
		goto fail;
	}

	/* Fill the hash table with keys as litteral strings and values as
	 * stringshares */
	for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_map); i++) {
		const struct kv_keymap *const kv = &(_map[i]);
		if (EINA_UNLIKELY(!eina_hash_add(_keymap, kv->key, &kv->keymap))) {
			CRI("Failed to add keymap entry '%s'", kv->key);
			goto fail;
		}
	}

	return EINA_TRUE;

fail:
	if (_keymap)
		eina_hash_free(_keymap);
	return EINA_FALSE;
}

void keymap_shutdown(void)
{
	eina_hash_free(_keymap);
}

const struct keymap *keymap_get(const char *input)
{
	return eina_hash_find(_keymap, input);
}
