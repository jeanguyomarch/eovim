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

#include "envim/keymap.h"
#include "envim/log.h"

typedef struct
{
   const char *const in;
   const char *const out;
} s_keymap;


#define KM(In, Out) { .in = In, .out = Out }
#define KM_STD(In) KM(In, "<" In ">")

static const s_keymap _map[] =
{
   KM_STD("Up"),
   KM_STD("Down"),
   KM_STD("Left"),
   KM_STD("Right"),
   KM_STD("F2"),
   KM_STD("F3"),
   KM_STD("F4"),
   KM_STD("F5"),
   KM_STD("F6"),
   KM_STD("F7"),
   KM_STD("F8"),
   KM_STD("F9"),
   KM_STD("F10"),
   KM_STD("F11"),
   KM_STD("F12"),
};

static Eina_Hash *_keymap = NULL;

Eina_Bool
keymap_init(void)
{
   /* First, create the hash table that wil hold the keymap */
   _keymap = eina_hash_string_superfast_new(EINA_FREE_CB(eina_stringshare_del));
   if (EINA_UNLIKELY(! _keymap))
     {
        CRI("Failed to create hash table");
        goto fail;
     }

   /* Fill the hash table with keys as litteral strings and values as
    * stringshares */
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(_map); i++)
     {
        const s_keymap *const km = &(_map[i]);
        Eina_Stringshare *const val = eina_stringshare_add(km->out);
        if (EINA_UNLIKELY(! val))
          {
             CRI("Failed to create stringshare from string '%s'", km->out);
             goto fail;
          }
        if (EINA_UNLIKELY(! eina_hash_add(_keymap, km->in, val)))
          {
             CRI("Failed to add '%s' => '%s", km->in, val);
             eina_stringshare_del(val);
             goto fail;
          }
     }

   return EINA_TRUE;

fail:
   if (_keymap) eina_hash_free(_keymap);
   return EINA_FALSE;
}

void
keymap_shutdown(void)
{
   eina_hash_free(_keymap);
}

Eina_Stringshare *
keymap_get(const char *input)
{
   return eina_hash_find(_keymap, input);
}
