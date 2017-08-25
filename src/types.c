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

static Eina_Value_Type _ENVIM_VALUE_TYPE_BOOL;
const Eina_Value_Type *ENVIM_VALUE_TYPE_BOOL = &_ENVIM_VALUE_TYPE_BOOL;

static Eina_Value_Type _ENVIM_VALUE_TYPE_NESTED;
const Eina_Value_Type *ENVIM_VALUE_TYPE_NESTED = &_ENVIM_VALUE_TYPE_NESTED;

Eina_Bool
types_init(void)
{
   /* ENVIM_VALUE_TYPE_BOOL is inherited from the Uchar type.
    * This is possible since Eina_Bool is actually typedef as unsigned char.
    * We just override the name.
    */
   memcpy(&_ENVIM_VALUE_TYPE_BOOL, EINA_VALUE_TYPE_UCHAR, sizeof(Eina_Value_Type));
   _ENVIM_VALUE_TYPE_BOOL.name = "Eina Bool";

   /* ENVIM_VALUE_TYPE_NESTED is inherited from the Uint64 type.
    * This is to store a pointer in an Eina Value
    */
   memcpy(&_ENVIM_VALUE_TYPE_NESTED, EINA_VALUE_TYPE_UINT64, sizeof(Eina_Value_Type));
   _ENVIM_VALUE_TYPE_NESTED.name = "Generic Pointer";
   _ENVIM_VALUE_TYPE_NESTED.value_size = sizeof(uintptr_t);

   return EINA_TRUE;
}

void
types_shutdown(void)
{
}
