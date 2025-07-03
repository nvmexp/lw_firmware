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
#ifdef NONE
/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of
** Oem_Random_GetBytes.
**
** Arguments:
**
** f_pOemTeeContext:       (in)  This parameter should be NULL for normal world invocation.
** f_pOEMContext:          (in)  OEM specified context
** f_pbData:               (out) Buffer to hold the random bytes
** f_cbData:               (in)  Count of bytes to generate and fill in pbData
*/
DRM_API DRM_RESULT DRM_CALL Oem_Broker_Random_GetBytes(
    __inout_opt                                DRM_VOID               *f_pOemTeeContext,
    __in_opt                                   DRM_VOID               *f_pOEMContext,
    __out_bcount(f_cbData)                     DRM_BYTE               *f_pbData,
    __in                                       DRM_DWORD               f_cbData )
{
    DRM_RESULT dr = DRM_SUCCESS;

    UNREFERENCED_PARAMETER( f_pOemTeeContext );

    ChkDR( Oem_Random_GetBytes(
        f_pOEMContext,
        f_pbData,
        f_cbData ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of
** the heap memory allocation function.
**
** Arguments:
**
** f_cbSize:    (in)    The amount of memory to allocate.
** f_ppv:       (out)   The allocated memory.
**                      This will be freed by Oem_Broker_MemFree.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_MemAlloc(
    __in                                    DRM_DWORD                 f_cbSize,
    __deref_out_bcount( f_cbSize )          DRM_VOID                **f_ppv )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_ppv    != NULL );
    ChkArg( f_cbSize >  0    );

    ChkMem( *f_ppv = Oem_MemAlloc( f_cbSize ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of
** the heap memory free function.
**
** Arguments:
**
** f_ppv:   (in/out)    On input, *f_ppv is the memory to free.
**                      On output, *f_ppv is NULL.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_MemFree(
    __inout                                  DRM_VOID                **f_ppv )
{
    if( f_ppv != NULL )
    {
        SAFE_OEM_FREE( *f_ppv );
    }
}

/*
** Synopsis:
**
** This function brokers a call to the normal world implementation of
** the Oem_Clock_GetSystemTimeAsFileTime function.
**
** Arguments:
**
** f_pOemTeeContext:          (in)     This parameter should be NULL for normal world invocation.
** f_pFileTime:               (out)    Pointer to DRMFILETIME structure to get the time
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Clock_GetSystemTimeAsFileTime(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __out                                      DRMFILETIME            *f_pFileTime )
{
    DRM_RESULT dr = DRM_SUCCESS;
    UNREFERENCED_PARAMETER( f_pOemTeeContextAllowNULL );
    ChkVOID( Oem_Clock_GetSystemTimeAsFileTime( NULL, f_pFileTime ) );
    return dr;
}
#endif
#elif DRM_COMPILE_FOR_SELWRE_WORLD  /* DRM_COMPILE_FOR_NORMAL_WORLD */
#if defined(SEC_COMPILE)
/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** Oem_Random_GetBytes.
**
** Arguments:
**
** f_pOemTeeContextAllowNULL: (in/out) The TEE context returned from
**                                     OEM_TEE_BASE_AllocTEEContext.
**                                     This function may receive NULL.
**                                     This function may receive an
**                                     OEM_TEE_CONTEXT where
**                                     cbUnprotectedOEMData == 0 and
**                                     pbUnprotectedOEMData == NULL.
** f_pOEMContext:             (in)     This parameter is requried to be null
** f_pbData:                  (out)    Buffer to hold the random bytes
** f_cbData:                  (in)     Count of bytes to generate and fill in pbData
*/
DRM_API DRM_RESULT DRM_CALL Oem_Broker_Random_GetBytes(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __in_opt                                   DRM_VOID               *f_pOEMContext,
    __out_bcount( f_cbData )                     DRM_BYTE               *f_pbData,
    __in                                       DRM_DWORD               f_cbData )
{
    DRM_RESULT dr = DRM_SUCCESS;

    UNREFERENCED_PARAMETER( f_pOEMContext );

    ChkDR( OEM_TEE_BASE_GenerateRandomBytes( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), f_cbData, f_pbData ) );

ErrorExit:
    return dr;
}
#endif

#if defined (SEC_COMPILE)
/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** the heap memory allocation function.
**
** Arguments:
**
** f_cbSize:    (in)    The amount of memory to allocate.
** f_ppv:       (out)   The allocated memory.
**                      This will be freed by Oem_Broker_MemFree.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_MemAlloc(
    __in                                    DRM_DWORD                 f_cbSize,
    __deref_out_bcount( f_cbSize )          DRM_VOID                **f_ppv )
{
    return OEM_TEE_BASE_SelwreMemAlloc( NULL, f_cbSize, f_ppv );
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** the heap memory free function.
**
** Arguments:
**
** f_ppv:   (in/out)    On input, *f_ppv is the memory to free.
**                      On output, *f_ppv is NULL.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Broker_MemFree(
    __inout                                  DRM_VOID                **f_ppv )
{
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, f_ppv ) );
}

/*
** Synopsis:
**
** This function brokers a call to the TEE implementation of
** the Oem_Clock_GetSystemTimeAsFileTime function.
**
** Arguments:
**
** f_pOemTeeContextAllowNULL: (in/out) The TEE context returned from
**                                     OEM_TEE_BASE_AllocTEEContext.
**                                     This function may receive NULL.
**                                     This function may receive an
**                                     OEM_TEE_CONTEXT where
**                                     cbUnprotectedOEMData == 0 and
**                                     pbUnprotectedOEMData == NULL.
** f_pFileTime:               (out)    Pointer to DRMFILETIME structure to get the time
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Broker_Clock_GetSystemTimeAsFileTime(
    __inout_opt                                DRM_VOID               *f_pOemTeeContextAllowNULL,
    __out                                      DRMFILETIME            *f_pFileTime )
{
    return OEM_TEE_CLOCK_GetTimestamp( DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContextAllowNULL ), f_pFileTime );
}
#endif

#else

#error Both DRM_COMPILE_FOR_NORMAL_WORLD and DRM_COMPILE_FOR_SELWRE_WORLD are set to 0.

#endif /* DRM_COMPILE_FOR_SELWRE_WORLD || DRM_COMPILE_FOR_NORMAL_WORLD */

PREFAST_POP;
PREFAST_POP;

EXIT_PK_NAMESPACE_CODE;
