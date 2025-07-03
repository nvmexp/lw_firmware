/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteecrypto.h>
#include <oemteeinternal.h>
#include <oemcommon.h>
#include <drmmathsafe.h>
#include <drmlastinclude.h>

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT* should not be const." )
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_BUFFER_PARAM_25033, "Out params can't be const" )

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
** If your hardware supports a security boundary between code running
** inside and outside the TEE, the key allocated by this function MUST
** be memory to which only the TEE has access.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function allocates memory that is only accessible inside the TEE.
**
** Operations Performed:
**
**  1. Allocate the given amount of memory and return it.
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
** f_cbSize:            (in)     The amount of memory to allocate.
** f_ppv:               (out)    The allocated memory.
**                               This will be freed by OEM_TEE_BASE_SelwreMemFree.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SelwreMemAlloc(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                            DRM_DWORD                   f_cbSize,
    __deref_out_bcount(f_cbSize)                    DRM_VOID                  **f_ppv )
{
    DRM_RESULT              dr            = DRM_SUCCESS;
    OEM_TEE_MEM_ALLOC_INFO *pAllocInfo    = NULL;
    DRM_DWORD               cbActualSize; /* Initialized by DRM_DWordAdd */

    DRMASSERT( f_ppv  != NULL );
    DRMASSERT( *f_ppv == NULL );    /* Catch leaks */
    DRMASSERT( f_cbSize > 0 );

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkDR( DRM_DWordAdd( sizeof(*pAllocInfo), f_cbSize, &cbActualSize ) );

    // LWE (kwilson) - update to use LW allocator
    ChkMem(pAllocInfo = (OEM_TEE_MEM_ALLOC_INFO *)pvPortMallocFreeable(cbActualSize));

    pAllocInfo->dwAllocatedSize = cbActualSize;

    *f_ppv = ( DRM_VOID * )( ( DRM_BYTE * )pAllocInfo + sizeof(*pAllocInfo) );

    /* Note: Do not zero-initialize, as this may be key-material. */
ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
** If your hardware supports a security boundary between code running inside
** and outside the TEE, this function must protect against freeing maliciously-
** specified memory, e.g. where the memory being freed is inaccessible or invalid.
** Otherwise, You do not need to replace this function implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function frees memory allocated by OEM_TEE_BASE_SelwreMemAlloc.
**
** Operations Performed:
**
**  1. Zero the given memory.
**  2. Free the given memory.
**  3. Set *f_ppv = NULL.
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
** f_ppv:               (in/out) On input, *f_ppv is the memory to free.
**                               On output, *f_ppv is NULL.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_BASE_SelwreMemFree(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __inout                                         DRM_VOID                  **f_ppv )
{
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_ppv != NULL );
    if( *f_ppv != NULL )
    {
        OEM_TEE_MEM_ALLOC_INFO *pAllocInfo = NULL;
        DRM_DWORD           dwSize     = 0;

        pAllocInfo = ( OEM_TEE_MEM_ALLOC_INFO * )( ( DRM_BYTE * )(*f_ppv) - sizeof(*pAllocInfo) );

        dwSize = pAllocInfo->dwAllocatedSize;

        OEM_SELWRE_MEMSET( ( DRM_VOID * )pAllocInfo, 0, dwSize );

        ChkVOID( SAFE_OEM_FREE( pAllocInfo ) );

        *f_ppv = NULL;
    }
}
#endif
EXIT_PK_NAMESPACE_CODE;

PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 */
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

