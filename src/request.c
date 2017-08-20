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

static Eina_Mempool *_mempool = NULL;

Eina_Bool
request_init(void)
{
   _mempool = eina_mempool_add("chained_mempool", "s_request",
                               NULL, sizeof(s_request), 8);
   if (EINA_UNLIKELY(! _mempool))
     {
        CRI("Failed to initialize mempool");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

void
request_shutdown(void)
{
   eina_mempool_del(_mempool);
   _mempool = NULL;
}

s_request *
request_new(uint32_t req_uid,
            e_request req_type,
            const void *then_cb,
            f_request_error error_cb,
            void *cb_data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(then_cb, NULL);

   s_request *const req = eina_mempool_malloc(_mempool, sizeof(s_request));
   if (EINA_UNLIKELY(! req))
     {
        CRI("Failed to allocate memory for a request object");
        return NULL;
     }

   req->uid = req_uid;
   req->type = req_type;
   req->then_callback = then_cb;
   req->error_callback = error_cb;
   req->callback_data = cb_data;

   DBG("New request 0x%x with id %"PRIu32, req->type, req->uid);

   return req;
}

void
request_free(s_request *req)
{
   if (req)
     {
        eina_mempool_free(_mempool, req);
     }
}
