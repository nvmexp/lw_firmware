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

typedef struct __OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY
{
    DRM_BOOL   fInUse;
    DRM_DWORD  cbMemory;
    DRM_BYTE  *pbMemory;
} OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY;

#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_PC
#define OEM_TEE_HANDLE_MEMORY_TABLE_SIZE 1000
#else   /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_PC */
#define OEM_TEE_HANDLE_MEMORY_TABLE_SIZE 2
#endif  /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_PC */

#if defined(SEC_COMPILE)
/*
** The following table is only needed if you are using handle-backed memory and will use the
** default OEM handle-backed memory implementation.
*/
static OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY s_rgoHandleMemoryTable[ OEM_TEE_HANDLE_MEMORY_TABLE_SIZE ] PR_ATTR_DATA_OVLY(_s_rgoHandleMemoryTable)= {{ 0 }};
#endif

#ifdef NONE
/*
** This function is used only for handle-backed memory.  It is responsible for finding
** and reserving a slot in the handle-backed memory table.   The value of the handle
** will be set to the index of the reserved free slot (if available).  If no slot is
** available, DRM_E_NOT_FOUND will be returned.  This function protects the handle
** table with a crititical section.
*/
static DRM_RESULT DRM_CALL _OEM_TEE_BASE_SelwreMemHandleReserveSlot(
    __inout     OEM_TEE_MEMORY_HANDLE               f_hMemHandle,
    __deref_out OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY **f_ppEmptySlot )
{
    DRM_RESULT drRet   = DRM_SUCCESS;  /* Do not use dr to ensure no jump macros are used. */
    DRM_RESULT drLeave = DRM_SUCCESS;
    DRM_DWORD  idx;     /* Loop variable */

    DRMASSERT( f_ppEmptySlot != NULL );

    *f_ppEmptySlot = NULL;

    /* Never use jump macros while holding the critical section. */
    if( DRM_SUCCEEDED( drRet = OEM_TEE_BASE_CRITSEC_Enter() ) )
    {
        DRM_BOOL fFound = FALSE;
        for( idx = 0; idx < DRM_NO_OF( s_rgoHandleMemoryTable ) && !fFound; idx++ )
        {
            if( !s_rgoHandleMemoryTable[ idx ].fInUse )
            {
                *f_ppEmptySlot = &s_rgoHandleMemoryTable[ idx ];
                ( *f_ppEmptySlot )->fInUse = TRUE;
                *(DRM_DWORD *)f_hMemHandle = idx;
                fFound = TRUE;
            }
        }
        if( !fFound )
        {
            drRet = DRM_E_NOT_FOUND;
        }

        drLeave = OEM_TEE_BASE_CRITSEC_Leave();
    }

    return DRM_FAILED( drRet ) ? drRet : drLeave;
}

/*
** This function is used only for handle-backed memory.  It gets the
** handle-backed memory slot for the provided handle.  If the slot at
** the provided handle is not valid, this function will return
** DRM_E_NOT_FOUND.  This function is not protected by a critical
** section.  It is intended that the caller protects this call and
** any use of the returned slot value.
*/
static DRM_RESULT DRM_CALL _OEM_TEE_BASE_SelwreMemHandleGetSlot(
    __in        OEM_TEE_MEMORY_HANDLE               f_hMemHandle,
    __deref_out OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY **f_ppSlot )
{
    DRM_RESULT dr           = DRM_E_NOT_FOUND;
    DRM_DWORD  dwSlotIndex; /* Initialized before use */

    ChkArg( f_ppSlot != NULL );
    ChkArg( f_hMemHandle != OEM_TEE_MEMORY_HANDLE_ILWALID );

    dwSlotIndex = *( const DRM_DWORD * )f_hMemHandle;

    *f_ppSlot = NULL;

    if( dwSlotIndex < DRM_NO_OF(s_rgoHandleMemoryTable) )
    {
        if( s_rgoHandleMemoryTable[ dwSlotIndex ].fInUse )
        {
            *f_ppSlot = &s_rgoHandleMemoryTable[ dwSlotIndex ];
            dr = DRM_SUCCESS;
        }
    }

ErrorExit:
    return dr;
}
#endif

#if defined (SEC_COMPILE)
/*
** This function is used only for handle-backed memory.  It frees the
** handle-backed memory and releases the slot for later use.  This
** function is protected by a critical section.
**
** LWE (kwilson) - In Microsoft PK code, _OEM_TEE_BASE_SelwreMemHandleFreeSlot returns
**                 DRM_VOID. Lwpu had to change the return value to DRM_RESULT in order
**                 to support a return code from acquiring/releasing the critical
**                 section, which may not always succeed.
*/
static DRM_RESULT DRM_CALL _OEM_TEE_BASE_SelwreMemHandleFreeSlot(
    __inout OEM_TEE_CONTEXT         *f_pContextAllowNULL,
    __in    OEM_TEE_MEMORY_HANDLE    f_hMemHandle )
{
    DRM_RESULT dr = DRM_SUCCESS;

    if( f_hMemHandle != OEM_TEE_MEMORY_HANDLE_ILWALID )
    {
        DRM_DWORD dwSlotIndex = *( const DRM_DWORD * )f_hMemHandle;

        if( dwSlotIndex < DRM_NO_OF(s_rgoHandleMemoryTable) )
        {
            ChkDR( OEM_TEE_BASE_CRITSEC_Enter() );
            ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContextAllowNULL, (DRM_VOID**)&s_rgoHandleMemoryTable[ dwSlotIndex ].pbMemory ) );
            s_rgoHandleMemoryTable[ dwSlotIndex ].cbMemory = 0;
            s_rgoHandleMemoryTable[ dwSlotIndex ].fInUse   = FALSE;
            ChkDR( OEM_TEE_BASE_CRITSEC_Leave() );
        }
    }
ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port.  If your PlayReady port does not
** support handle-backed memory, this function MUST return DRM_E_NOTIMPL.
**
** If your hardware supports a security boundary between code running inside
** and outside the TEE, all memory allocated by this function MUST
** be memory to which only the TEE has access, and the handle returned
** MUST be able to cross the secure boundary without leaking any information
** to code running outside the TEE.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function allocates memory that is only accessible inside the TEE.
** Unlike OEM_TEE_BASE_SelwreMemAlloc, it does not return the memory directly
** in a form which can be written/read outside your code.  Instead, it returns
** an opaque handle which only your code inside the TEE can write to/read from.
** This handle MUST also be able to be passed across the secure boundary in its
** opaque form and be valid for its lifetime (i.e. until the function
** OEM_TEE_BASE_SelwreMemHandleFree is called).
**
** Therefore, if secure memory handles are NOT supported, this function MUST
** be changed to return DRM_E_NOTIMPL.
**
** If handles ARE supported, this function MUST be replaced with an implementation
** that does not allow the secure memory data to be accessible from the normal
** world.
**
** Operations Performed:
**
**  1. Allocate the given amount of memory.
**  2. Return an opaque handle to that memory and the size of the handle, as bytes,
**     across the secure boundary.
**
** Arguments:
**
** f_pContext:          (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
** f_dwDecryptionMode:  (in)     The decryption mode being used for the handle.
** f_cbSize:            (in/out) The amount of memory to allocate.
** f_pcbMemHandle:      (out)    The size of *f_pMemHandle to be copied into the inselwre world.
** f_pMemHandle:        (out)    An opaque handle to the allocated memory.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SelwreMemHandleAlloc(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                            DRM_DWORD                   f_dwDecryptionMode,
    __in                                            DRM_DWORD                   f_cbSize,
    __out                                           DRM_DWORD                  *f_pcbMemHandle,
    __out                                           OEM_TEE_MEMORY_HANDLE      *f_pMemHandle )
{
    /*
    ** The default PK uses an "opaque handle" which is just a pointer to the allocated data.
    ** The size of the allocated data is the requested size + sizeof(DRM_DWORD).  The DRM_DWORD
    ** is used to track the size of the handle-backed allocation.  This size value does not
    ** include the DRM_DWORD size.  In a real TEE implementation, the handle (which is a pointer
    ** value to the data) will not be accessible from the normal world.  For PK tests, this
    ** pointer value is used to validate decryption and to free the allocation.  This only works
    ** because the test run in the same process as the TEE code and therefore has access to the
    ** same memory space.
    */
    DRM_RESULT                         dr         = DRM_SUCCESS;
    OEM_TEE_MEMORY_HANDLE              hMemHandle = OEM_TEE_MEMORY_HANDLE_ILWALID;
    DRM_BYTE                          *pb         = NULL;
    OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY *pEntry     = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    ChkArg( f_cbSize > 0 );
    ChkArg( f_dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_HANDLE );
    ChkArg( f_pcbMemHandle != NULL );
    ChkArg( f_pMemHandle != NULL );

    *f_pcbMemHandle = 0;
    *f_pMemHandle   = OEM_TEE_MEMORY_HANDLE_ILWALID;

    /* Allocate the handle backed memory */
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, f_cbSize, (DRM_VOID**)&pb ) );
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( pb, f_cbSize ) );

    /* Allocate amd set the handle value */
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, sizeof(OEM_TEE_MEMORY_HANDLE), (DRM_VOID**)&hMemHandle ) );
    ChkDR( _OEM_TEE_BASE_SelwreMemHandleReserveSlot( hMemHandle, &pEntry ) );

    /* Transfer ownership */
    *f_pMemHandle    = hMemHandle;
    pEntry->pbMemory = pb;
    pEntry->cbMemory = f_cbSize;
    *f_pcbMemHandle  = sizeof(OEM_TEE_MEMORY_HANDLE);
    hMemHandle       = OEM_TEE_MEMORY_HANDLE_ILWALID;
    pb               = NULL;
    pEntry           = NULL;

ErrorExit:
    ChkVOID( _OEM_TEE_BASE_SelwreMemHandleFreeSlot( f_pContext, hMemHandle ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pb ) );
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
#endif

#if defined(SEC_COMPILE)
/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port.  If your PlayReady port does not
** support handle-backed memory, this function should do nothing.
**
** If your hardware supports a security boundary between code running inside
** and outside the TEE, this function MUST protect against freeing maliciously-
** specified memory, e.g. where the memory being freed is inaccessible or invalid.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function frees memory allocated by OEM_TEE_BASE_SelwreMemHandleAlloc.
**
** Operations Performed:
**
**  1. Zero the given memory.
**  2. Free the given memory.
**  3. Set *f_pMemHandle = OEM_TEE_MEMORY_HANDLE_ILWALID.
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
** f_pMemHandle:        (in/out) On input, *f_pMemHandle is a
**                               handle to the memory to free.
**                               On output, *f_pMemHandle is
**                               OEM_TEE_MEMORY_HANDLE_ILWALID.
**
** LWE (kwilson) - In Microsoft PK code, OEM_TEE_BASE_SelwreMemHandleFree returns DRM_VOID.
**                 Lwpu had to change the return value to DRM_RESULT in order
**                 to support a return code from acquiring/releasing the critical
**                 section, which may not always succeed.
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SelwreMemHandleFree(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __inout                                         OEM_TEE_MEMORY_HANDLE      *f_pMemHandle )
{
    DRM_RESULT dr = DRM_SUCCESS;

    /* Note: See comments in OEM_TEE_BASE_SelwreMemHandleAlloc */
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_pMemHandle != NULL );

    if( *f_pMemHandle != OEM_TEE_MEMORY_HANDLE_ILWALID )
    {
        ChkDR(_OEM_TEE_BASE_SelwreMemHandleFreeSlot(f_pContextAllowNULL, *f_pMemHandle));
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContextAllowNULL, f_pMemHandle ) );
        *f_pMemHandle = OEM_TEE_MEMORY_HANDLE_ILWALID;
    }
ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
#endif

#ifdef NONE
/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port.  If your PlayReady port does not
** support handle-backed memory, this function MUST return zero.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function returns the number of bytes that were requested to be allocated
** by OEM_TEE_BASE_SelwreMemHandleAlloc when the memory was allocated.
**
** Operations Performed:
**
**  1. Return the number of allocated bytes.
**
** Arguments:
**
** f_pContext:          (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
** f_hMem:              (in)     The handle to memory for which
**                               the size is requested.
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
DRM_NO_INLINE DRM_API DRM_DWORD DRM_CALL OEM_TEE_BASE_SelwreMemHandleGetSize(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const OEM_TEE_MEMORY_HANDLE       f_hMem )
{
    DRM_DWORD                          dwSize = 0;
    OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY *pEntry = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_hMem != OEM_TEE_MEMORY_HANDLE_ILWALID );

    /* Never use jump macros while holding the critical section. */
    if( DRM_SUCCEEDED( OEM_TEE_BASE_CRITSEC_Enter() ) )
    {
        if( DRM_SUCCEEDED( _OEM_TEE_BASE_SelwreMemHandleGetSlot( f_hMem, &pEntry ) ) )
        {
            dwSize = pEntry->cbMemory;
        }
        if( DRM_FAILED( OEM_TEE_BASE_CRITSEC_Leave() ) )
        {
            dwSize = 0;
        }
    }

    return dwSize;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port.  If your PlayReady port does not
** support handle-backed memory, this function MUST return DRM_E_NOTIMPL.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function writes bytes to memory allocated by OEM_TEE_BASE_SelwreMemHandleAlloc
** at a given offset.
**
** Operations Performed:
**
**  1. Write the given bytes to the given handle-backed memory at the given offset.
**
** Arguments:
**
** f_pContext:          (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
** f_ibWrite:           (in)     The offset at which to write the bytes.
** f_cbWrite:           (in)     The number of bytes to write.
** f_pbWrite:           (in)     The bytes to write.
** f_hMem:              (in)     The handle to the memory to write to.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SelwreMemHandleWrite(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                            DRM_DWORD                   f_ibWrite,
    __in                                            DRM_DWORD                   f_cbWrite,
    __in_bcount( f_cbWrite )                  const DRM_BYTE                   *f_pbWrite,
    __inout                                         OEM_TEE_MEMORY_HANDLE       f_hMem )
{
    /* Note: See comments in OEM_TEE_BASE_SelwreMemHandleAlloc */
    DRM_RESULT                         dr     = DRM_SUCCESS;
    DRM_DWORD                          cb     = 0;
    DRM_BYTE                          *pb     = NULL;
    OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY *pEntry = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkArg( f_pbWrite != NULL );
    ChkArg( f_hMem != OEM_TEE_MEMORY_HANDLE_ILWALID );
    ChkDR( DRM_DWordAdd( f_ibWrite, f_cbWrite, &cb ) );

    {
        DRM_RESULT drRet   = DRM_SUCCESS;  /* Do not use dr to ensure no jump macros are used. */
        DRM_RESULT drLeave = DRM_SUCCESS;

        /* Never use jump macros while holding the critical section. */
        if( DRM_SUCCEEDED( drRet = OEM_TEE_BASE_CRITSEC_Enter() ) )
        {
            if( DRM_SUCCEEDED( drRet = _OEM_TEE_BASE_SelwreMemHandleGetSlot( f_hMem, &pEntry ) ) )
            {
                if( cb <= pEntry->cbMemory )
                {
                    pb = pEntry->pbMemory;
                    OEM_TEE_MEMCPY( &pb[ f_ibWrite ], f_pbWrite, f_cbWrite );
                }
                else
                {
                    drRet = DRM_E_ILWALIDARG;
                }
            }

            drLeave = OEM_TEE_BASE_CRITSEC_Leave();
        }
        ChkDR( drRet );
        ChkDR( drLeave );
    }

ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port.  If your PlayReady port does not
** support handle-backed memory, this function MUST return DRM_E_NOTIMPL.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function reads bytes from memory allocated by OEM_TEE_BASE_SelwreMemHandleAlloc
** at a given offset.
**
** Operations Performed:
**
**  1. Read the given bytes from the given handle-backed memory at the given offset.
**
** Arguments:
**
** f_pContext:          (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
** f_ibRead:            (in)     The offset at which to read the bytes.
** f_cbRead:            (in)     The number of bytes to read.
** f_pbRead:            (in)     The output buffer into which the bytes will read.
** f_hMem:              (in)     The handle to the memory to read from.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SelwreMemHandleRead(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                            DRM_DWORD                   f_ibRead,
    __in                                            DRM_DWORD                   f_cbRead,
    __out_bcount( f_cbRead )                        DRM_BYTE                   *f_pbRead,
    __in                                      const OEM_TEE_MEMORY_HANDLE       f_hMem )
{
    /* Note: See comments in OEM_TEE_BASE_SelwreMemHandleAlloc */
    DRM_RESULT                         dr     = DRM_SUCCESS;
    const DRM_BYTE                    *pb     = NULL;
    DRM_DWORD                          cb     = 0;
    OEM_TEE_HANDLE_MEMORY_TABLE_ENTRY *pEntry = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkArg( f_pbRead != NULL );
    ChkArg( f_hMem != OEM_TEE_MEMORY_HANDLE_ILWALID );
    ChkDR( DRM_DWordAdd( f_ibRead, f_cbRead, &cb ) );

    {
        DRM_RESULT drRet   = DRM_SUCCESS;  /* Do not use dr to ensure no jump macros are used. */
        DRM_RESULT drLeave = DRM_SUCCESS;

        /* Never use jump macros while holding the critical section. */
        if( DRM_SUCCEEDED( drRet = OEM_TEE_BASE_CRITSEC_Enter() ) )
        {
            if( DRM_SUCCEEDED( drRet = _OEM_TEE_BASE_SelwreMemHandleGetSlot( f_hMem, &pEntry ) ) )
            {
                if( cb <= pEntry->cbMemory )
                {
                    pb = pEntry->pbMemory;
                    OEM_TEE_MEMCPY( f_pbRead, &pb[ f_ibRead ], f_cbRead );
                }
                else
                {
                    drRet = DRM_E_ILWALIDARG;
                }
            }

            drLeave = OEM_TEE_BASE_CRITSEC_Leave();
        }
        ChkDR( drRet );
        ChkDR( drLeave );
    }

ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port.  If your PlayReady port does not
** support handle-backed memory, this function MUST return zero.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function returns the number of bytes required for a valid secure memory
** handle.
**
** Operations Performed:
**
**  1. Return the number of bytes required for a valid secure memory handle.
**
** Arguments:
**
** f_pContext:          (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
*/
DRM_NO_INLINE DRM_API DRM_DWORD DRM_CALL OEM_TEE_BASE_SelwreMemHandleGetHandleSize(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext )
{
    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    return sizeof(OEM_TEE_MEMORY_HANDLE);
}
#endif
EXIT_PK_NAMESPACE_CODE;

PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 */
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

