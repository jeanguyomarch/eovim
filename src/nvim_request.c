/* This file is part of Eovim, which is under the MIT License ****************/

#include "eovim/nvim_request.h"
#include "eovim/nvim.h"
#include "eovim/log.h"

static Eina_Hash *_nvim_requests;


/*============================================================================*
 *                                     API                                    *
 *============================================================================*/

Eina_Bool
nvim_request_add(const char *request_name, f_nvim_request_cb func)
{
   Eina_Stringshare *const name = eina_stringshare_add(request_name);
   const Eina_Bool ok = eina_hash_direct_add(_nvim_requests, name, func);
   if (EINA_UNLIKELY(! ok))
     {
        ERR("Failed to register request \"%s\"", request_name);
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

void
nvim_request_del(const char *request_name)
{
   Eina_Stringshare *const name = eina_stringshare_add(request_name);
   eina_hash_del(_nvim_requests, name, NULL);
   eina_stringshare_del(name);
}

Eina_Bool
nvim_request_init(void)
{
   _nvim_requests = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(! _nvim_requests))
     {
        CRI("Failed to create hash table");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

void
nvim_request_shutdown(void)
{
   assert(_nvim_requests != NULL);
   eina_hash_free(_nvim_requests);
   _nvim_requests = NULL;
}

Eina_Bool
nvim_request_process(s_nvim *nvim, Eina_Stringshare *request,
                     const msgpack_object_array *args, uint32_t req_id)
{
   /* This function shall only be used on the main loop. Otherwise, we cannot
    * use this packer */
   msgpack_packer *const pk = &nvim->packer;

   /* The buffer MUST be empty before preparing the response. If this is not
    * the case, something went very wrong! Discard the buffer and keep going */
   if (EINA_UNLIKELY(nvim->sbuffer.size != 0u))
     {
        ERR("The buffer is not empty. I've messed up somewhere");
        msgpack_sbuffer_clear(&nvim->sbuffer);
     }

   /*
    *
    * We start to reply with the two first elements. If we are not prepared to
    * handle this request, we will finish the message with an error and no
    * result. But if someone handles the request, it is up to the handler to
    * finish the message by setting both the error and result.
    */

   const f_nvim_request_cb func = eina_hash_find(_nvim_requests, request);
   if (EINA_UNLIKELY(! func))
     {
        WRN("No handler for request '%s'", request);
        const char error[] = "unknown request";

        /* See msgpack-rpc request response. Reply there is an error */
        msgpack_pack_array(pk, 4);
        msgpack_pack_int(pk, 1);
        msgpack_pack_uint32(pk, req_id);
        msgpack_pack_str(pk, sizeof(error) - 1u);
        msgpack_pack_str_body(pk, error, sizeof(error) - 1u);
        msgpack_pack_nil(pk);
        nvim_flush(nvim);
        return EINA_FALSE;
     }
   else
     {
        const Eina_Bool ok = func(nvim, args, pk, req_id);
        return ok;
     }
}
