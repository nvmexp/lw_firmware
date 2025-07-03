/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
// LWE (nkuo): TKs are generated on demand in production code. So we don't need following global/function to be compiled in.
#ifdef SINGLE_METHOD_ID_REPLAY

/*
** Statically allocated Current TEE Key
*/
static OEM_TEE_KEY_AES s_oCTKRaw PR_ATTR_DATA_OVLY(_s_oCTKRaw) = { { 0 } };

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** This function is used in all PlayReady scenarios.
**
** This function implementation returns a reference to a static OEM_TEE_KEY
** which is used as the location to place the oldest TK as part of initializing
** the TKs during OEM_TEE_BASE_AllocTEEContext.
**
** You are welcome to replace or remove this function entirely based on how
** you implement OEM_TEE_BASE_AllocTEEContext.
**
** Synopsis:
**
** This function returns an uninitialized OEM_TEE_KEY to be used as the oldest TK.
** Since the default implementation only has the CTK, this static OEM_TEE_KEY
** is also equal to the CTK (uninitialized).  The caller initializes the key.
**
** Operations Performed:
**
**  1. Return the uninitialized CTK.
**
** Arguments:
**
** f_pKey: (in/out) The uninitialized CTK.
**                  The OEM_TEE_KEY is NOT pre-allocated by the caller
**                  and is never freed.  It is statically allocated.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_BASE_GetStaticKeyHistoryKey(
    __inout_ecount( 1 )                                   OEM_TEE_KEY                  *f_pKey )
{
    DRMASSERT( f_pKey != NULL );
    f_pKey->pKey = &s_oCTKRaw;
}
#endif
EXIT_PK_NAMESPACE_CODE;

