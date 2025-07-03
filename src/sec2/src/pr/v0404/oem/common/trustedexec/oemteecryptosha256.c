/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemteecrypto.h>
#include <oemsha256.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function initializes the sha256 context structure.
**
** Operations Performed:
**
**  1. Initializes the sha256 context structure.
**
** Arguments:
**
** f_pContextAllowNULL: (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
**                               This function may receive NULL.
**                               This function may receive an
**                               OEM_TEE_CONTEXT where
**                               cbUnprotectedOEMData == 0 and
**                               pbUnprotectedOEMData == NULL.
** f_pShaContext:          (out) The SHA context to initialize.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_SHA256_Init(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __out_ecount( 1 )                                     OEM_SHA256_CONTEXT           *f_pShaContext )
{
    return OEM_SHA256_Init( f_pShaContext );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function inserts data to be hashed into an already-initialized sha256 context structure.
**
** Operations Performed:
**
**  1. Inserts data to be sha265 hashed.
**
** Arguments:
**
** f_pContextAllowNULL: (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
**                               This function may receive NULL.
**                               This function may receive an
**                               OEM_TEE_CONTEXT where
**                               cbUnprotectedOEMData == 0 and
**                               pbUnprotectedOEMData == NULL.
** f_pShaContext:        (in/out) The SHA context to insert data into.
** f_rgbBuffer:              (in) The data to insert.
** f_cbBuffer:               (in) The size of the data to insert.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_SHA256_Update(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __inout_ecount( 1 )                                   OEM_SHA256_CONTEXT           *f_pShaContext,
    __in_ecount( f_cbBuffer )                       const DRM_BYTE                      f_rgbBuffer[],
    __in                                                  DRM_DWORD                     f_cbBuffer )
{
    return OEM_SHA256_Update( f_pShaContext, f_rgbBuffer, f_cbBuffer );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port if this function is still called
** by other OEM_TEE functions once your port is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function finalizes all SHA256 operations on the data previously added via
** OEM_TEE_CRYPTO_SHA256_Update and returns the final digest value.
**
** Operations Performed:
**
**  1. Completes the SHA256 hash and returns its final digest value.
**
** Arguments:
**
** f_pContextAllowNULL: (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
**                               This function may receive NULL.
**                               This function may receive an
**                               OEM_TEE_CONTEXT where
**                               cbUnprotectedOEMData == 0 and
**                               pbUnprotectedOEMData == NULL.
** f_pShaContext:        (in/out) The SHA context to finalize.
** f_pDigest:               (out) The final digest value.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_SHA256_Finalize(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __inout_ecount( 1 )                                   OEM_SHA256_CONTEXT           *f_pShaContext,
    __out_ecount( 1 )                                     OEM_SHA256_DIGEST            *f_pDigest )
{
    return OEM_SHA256_Finalize( f_pShaContext, f_pDigest );
}
#endif
EXIT_PK_NAMESPACE_CODE;

