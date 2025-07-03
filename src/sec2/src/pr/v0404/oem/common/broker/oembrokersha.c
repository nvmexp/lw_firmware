/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmerr.h>
#include <oembroker.h>
#include <oemcommon.h>
#include <oemtee.h>

ENTER_PK_NAMESPACE_CODE;

#if !defined(DRM_COMPILE_FOR_NORMAL_WORLD) && !defined(DRM_COMPILE_FOR_SELWRE_WORLD)
#error Neither DRM_COMPILE_FOR_NORMAL_WORLD nor DRM_COMPILE_FOR_SELWRE_WORLD are defined.
#elif defined(DRM_COMPILE_FOR_NORMAL_WORLD) && DRM_COMPILE_FOR_NORMAL_WORLD != 1 && DRM_COMPILE_FOR_NORMAL_WORLD != 0
#error DRM_COMPILE_FOR_NORMAL_WORLD is set to neither 0 nor 1
#elif defined(DRM_COMPILE_FOR_SELWRE_WORLD) && DRM_COMPILE_FOR_SELWRE_WORLD != 1 && DRM_COMPILE_FOR_SELWRE_WORLD != 0
#error DRM_COMPILE_FOR_SELWRE_WORLD is set to neither 0 nor 1
#elif DRM_COMPILE_FOR_NORMAL_WORLD == DRM_COMPILE_FOR_SELWRE_WORLD
#error Both DRM_COMPILE_FOR_NORMAL_WORLD and DRM_COMPILE_FOR_SELWRE_WORLD are defined to the same value.  One must be defined to 1 by your profile.  The other must be defined to 0 by your profile.
#endif

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Ignore Nonconst Param - f_pOemTeeContext and f_pBigContext are mutually exclusive and used separately in different implementations of the broker functions." );
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_DANGEROUS_POINTERCAST_25024, "Ignore Dangerous Pointercast for colwersion from PRIVKEY_P256* to OEM_TEE_KEY*." );

#if DRM_COMPILE_FOR_NORMAL_WORLD
#if NONE
/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of the SHA256
** functions to create a digest from the provided data.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_SHA256_CreateDigest(
    __inout_opt                                     DRM_VOID                   *f_pOemTeeContextAllowNULL,
    __in                                            DRM_DWORD                   f_cbBuffer,
    __in_ecount( f_cbBuffer )                 const DRM_BYTE                    f_rgbBuffer[],
    __out                                           OEM_SHA256_DIGEST          *f_pDigest )
{
    DRM_RESULT         dr           = DRM_SUCCESS;
    OEM_SHA256_CONTEXT oSHAContext; /* Initialized by OEM_SHA256_Init     */

    UNREFERENCED_PARAMETER( f_pOemTeeContextAllowNULL );

    ChkDR( OEM_SHA256_Init    ( &oSHAContext ) );
    ChkDR( OEM_SHA256_Update  ( &oSHAContext, f_rgbBuffer, f_cbBuffer ) );
    ChkDR( OEM_SHA256_Finalize( &oSHAContext, f_pDigest ) );

ErrorExit:
    return dr;
}
#endif

#elif DRM_COMPILE_FOR_SELWRE_WORLD  /* DRM_COMPILE_FOR_NORMAL_WORLD */

#if defined (SEC_COMPILE)
/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of the SHA256
** functions to create a digest from the provided data.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_SHA256_CreateDigest(
    __inout_opt                                     DRM_VOID                   *f_pOemTeeContextAllowNULL,
    __in                                            DRM_DWORD                   f_cbBuffer,
    __in_ecount( f_cbBuffer )                 const DRM_BYTE                    f_rgbBuffer[],
    __out                                           OEM_SHA256_DIGEST          *f_pDigest )
{
    DRM_RESULT         dr           = DRM_SUCCESS;
    OEM_SHA256_CONTEXT oSHAContext; /* Initialized by OEM_SHA256_Init */
    OEM_TEE_CONTEXT   *pOemTeeCtx   = DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL );

    ChkDR( OEM_TEE_BASE_SHA256_Init( pOemTeeCtx, &oSHAContext ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update( pOemTeeCtx, &oSHAContext, f_cbBuffer, f_rgbBuffer ) );
    ChkDR( OEM_TEE_BASE_SHA256_Finalize( pOemTeeCtx, &oSHAContext, f_pDigest ) );

ErrorExit:
    return dr;
}
#endif

#else

#error Both DRM_COMPILE_FOR_NORMAL_WORLD and DRM_COMPILE_FOR_SELWRE_WORLD are set to 0.

#endif /* DRM_COMPILE_FOR_SELWRE_WORLD || DRM_COMPILE_FOR_NORMAL_WORLD */

PREFAST_POP;
PREFAST_POP;

EXIT_PK_NAMESPACE_CODE;

