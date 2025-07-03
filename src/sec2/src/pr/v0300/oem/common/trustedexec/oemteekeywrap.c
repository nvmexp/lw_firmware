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
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_BUFFER_PARAM_25033, "Out params can't be const" )

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
GCC_ATTRIB_NO_STACK_PROTECT()
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT should not be const.")
static DRM_RESULT DRM_CALL _WrapAES128Key(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const OEM_TEE_KEY                *f_pWrappingKey,
    __in                                      const OEM_TEE_KEY                *f_pKeyToBeWrapped,
    __inout                                         DRM_DWORD                  *f_pibWrappedKeyBuffer,
    __inout_opt                                     DRM_BYTE                   *f_pbWrappedKeyBuffer )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pWrappingKey        != NULL );
    DRMASSERT( f_pibWrappedKeyBuffer != NULL );

    if( f_pbWrappedKeyBuffer != NULL )
    {
        OEM_WRAPPED_KEY_AES_128   oWrappedKey;   /* Initialized in RANDOM_GetBytes and MEMCPY calls */
        OEM_UNWRAPPED_KEY_AES_128 oUnwrappedKey; /* Initialized in WrapKeyAES128 */
        const DRM_BYTE           *pbKey;
        
        DRMASSERT( f_pKeyToBeWrapped != NULL );

        DRMCASSERT( sizeof(oUnwrappedKey) > OEM_AESKEYWRAP_RANDOMLEN_IN_BYTES );
        ChkDR( OEM_TEE_CRYPTO_RANDOM_GetBytes( 
            f_pContext,
            (DRM_BYTE*)&oUnwrappedKey + sizeof(oUnwrappedKey) - OEM_AESKEYWRAP_RANDOMLEN_IN_BYTES,
            OEM_AESKEYWRAP_RANDOMLEN_IN_BYTES ) );

        pbKey = OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES(f_pKeyToBeWrapped);
        ChkVOID( OEM_TEE_MEMCPY( &oUnwrappedKey, pbKey, DRM_AES_KEYSIZE_128 ) );

        dr = OEM_TEE_CRYPTO_AESKEYWRAP_WrapKeyAES128( f_pContext, f_pWrappingKey, &oUnwrappedKey, &oWrappedKey );

        ChkVOID( OEM_TEE_MEMCPY( &(f_pbWrappedKeyBuffer)[*f_pibWrappedKeyBuffer], &oWrappedKey, sizeof(oWrappedKey) ) );
        ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oWrappedKey, sizeof(oWrappedKey ) ) );
        ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oUnwrappedKey, sizeof(oUnwrappedKey ) ) );

        ChkDR( dr );
    }

    *f_pibWrappedKeyBuffer += sizeof(OEM_WRAPPED_KEY_AES_128);

ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_RESULT DRM_CALL _UnwrapAES128Key(
    __inout_opt                                     OEM_TEE_CONTEXT     *f_pContextAllowNULL,
    __in                                      const OEM_TEE_KEY         *f_pWrappingKey,
    __inout                                         DRM_DWORD           *f_pcbWrappedKeyBuffer,
    __inout                                         DRM_DWORD           *f_pibWrappedKeyBuffer,
    __in                                      const DRM_BYTE            *f_pbWrappedKeyBuffer,
    __out                                           OEM_TEE_KEY         *f_pPreallocatedUnwrappedKey )
{
    DRM_RESULT                  dr = DRM_SUCCESS;
    OEM_WRAPPED_KEY_AES_128     oWrappedKey;   /* Do not zero-initialize key material */
    OEM_UNWRAPPED_KEY_AES_128   oUnwrappedKey; /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    DRMASSERT( f_pWrappingKey               != NULL );
    DRMASSERT( f_pcbWrappedKeyBuffer        != NULL );
    DRMASSERT( f_pibWrappedKeyBuffer        != NULL );
    DRMASSERT( f_pbWrappedKeyBuffer         != NULL );
    DRMASSERT( f_pPreallocatedUnwrappedKey  != NULL );

    ChkBOOL( *f_pcbWrappedKeyBuffer >= sizeof(OEM_WRAPPED_KEY_AES_128), DRM_E_TEE_ILWALID_KEY_DATA );
    ChkVOID( OEM_TEE_MEMCPY( &oWrappedKey, &f_pbWrappedKeyBuffer[*f_pibWrappedKeyBuffer], sizeof(oWrappedKey) ) );

    ChkDR( OEM_TEE_CRYPTO_AESKEYWRAP_UnwrapKeyAES128( f_pContextAllowNULL, f_pWrappingKey, &oWrappedKey, &oUnwrappedKey ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContextAllowNULL, f_pPreallocatedUnwrappedKey, (const DRM_BYTE*)&oUnwrappedKey ) );

    *f_pcbWrappedKeyBuffer -= sizeof(OEM_WRAPPED_KEY_AES_128);
    *f_pibWrappedKeyBuffer += sizeof(OEM_WRAPPED_KEY_AES_128);

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function wraps the given Key and 8 bytes of entropy with the given TK
** for the purpose of persisting the key to disk.  It uses the AES 128 
** Codebook for the KEK. It uses the AES Key Wrap Specification given in 
** the following NIST document (published 16 November 2001).
**
** http://csrc.nist.gov/groups/ST/toolkit/dolwments/kms/key-wrap.pdf
**
** Note: The caller makes no assumptions about the size of a wrapped key.
**
** Operations Performed:
**
**  1. If a buffer was given, wrap the given key using the given key using AES Key Wrap
**     and write the wrapped value to the given buffer at the given offset.
**  2. Update the given offset with the size of the wrapped key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pWrappingKey:          (in)     The key with which to wrap the given key.
** f_pKeyToBeWrapped:       (in)     The key to be wrapped.
** f_pibWrappedKeyBuffer:   (in/out) On input, the offset to which to write the
**                                   given key (if a buffer was specified).
**                                   On output, the input value plus the size
**                                   of the wrapped key.
** f_pbWrappedKeyBuffer:    (in/out) The buffer to which to write the given key.
**                                   This parameter may be NULL if only the offset
**                                   update for the wrapped key size is required.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_WrapAES128KeyForPersistedStorage(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const OEM_TEE_KEY                *f_pWrappingKey,
    __in                                      const OEM_TEE_KEY                *f_pKeyToBeWrapped,
    __inout                                         DRM_DWORD                  *f_pibWrappedKeyBuffer,
    __inout_opt                                     DRM_BYTE                   *f_pbWrappedKeyBuffer )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDR( _WrapAES128Key( f_pContext, f_pWrappingKey, f_pKeyToBeWrapped, f_pibWrappedKeyBuffer, f_pbWrappedKeyBuffer ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function wraps the given Key and 8 bytes of entropy with the given TK
** for the purpose of persisting the key to disk.  It uses the AES 128 
** Codebook for the KEK. It uses the AES Key Wrap Specification given in 
** the following NIST document (published 16 November 2001).
**
** http://csrc.nist.gov/groups/ST/toolkit/dolwments/kms/key-wrap.pdf
**
** Note: The caller makes no assumptions about the size of a wrapped key.
**
** Operations Performed:
**
**  1. If a buffer was given, wrap the given key using the given wrapping key
**     using AES Key Wrap and write the wrapped value to the given buffer at
**     the given offset.
**  2. Update the given offset with the size of the wrapped key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pWrappingKey:          (in)     The key with which to wrap the given key.
** f_pKeyToBeWrapped:       (in)     The key to be wrapped.
** f_pibWrappedKeyBuffer:   (in/out) On input, the offset to which to write the
**                                   given key (if a buffer was specified).
**                                   On output, the input value plus the size
**                                   of the wrapped key.
** f_pbWrappedKeyBuffer:    (in/out) The buffer to which to write the given key.
**                                   This parameter may be NULL if only the offset
**                                   update for the wrapped key size is required.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT should not be const.")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_WrapECC256KeyForPersistedStorage(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const OEM_TEE_KEY                *f_pWrappingKey,
    __in                                      const OEM_TEE_KEY                *f_pKeyToBeWrapped,
    __inout                                         DRM_DWORD                  *f_pibWrappedKeyBuffer,
    __inout_opt                                     DRM_BYTE                   *f_pbWrappedKeyBuffer )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pWrappingKey        != NULL );
    DRMASSERT( f_pibWrappedKeyBuffer != NULL );

    if( f_pbWrappedKeyBuffer != NULL )
    {
        OEM_WRAPPED_KEY_ECC_256   oWrappedKey;   /* Initialized in WrapKeyECC256 */
        OEM_UNWRAPPED_KEY_ECC_256 oUnwrappedKey; /* Initialized in RANDOM_GetBytes and MEMCPY calls */
        const PRIVKEY_P256       *poPrivkey;
        
        DRMASSERT( f_pKeyToBeWrapped != NULL );

        poPrivkey = OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256(f_pKeyToBeWrapped);

        ChkDR( OEM_TEE_CRYPTO_RANDOM_GetBytes( 
            f_pContext,
            (DRM_BYTE*)&oUnwrappedKey + sizeof(oUnwrappedKey) - OEM_AESKEYWRAP_RANDOMLEN_IN_BYTES, 
            OEM_AESKEYWRAP_RANDOMLEN_IN_BYTES ) );

        ChkVOID( OEM_TEE_MEMCPY( &oUnwrappedKey, poPrivkey, sizeof(*poPrivkey) ) );
        
        dr = OEM_TEE_CRYPTO_AESKEYWRAP_WrapKeyECC256( f_pContext, f_pWrappingKey, &oUnwrappedKey, &oWrappedKey );

        ChkVOID( OEM_TEE_MEMCPY( &(f_pbWrappedKeyBuffer)[*f_pibWrappedKeyBuffer], &oWrappedKey, sizeof(oWrappedKey) ) );
        ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oWrappedKey, sizeof(oWrappedKey) ) );
        ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oUnwrappedKey, sizeof(oUnwrappedKey) ) );

        ChkDR( dr );
    }
    *f_pibWrappedKeyBuffer += sizeof(OEM_WRAPPED_KEY_ECC_256);

ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
** Any reimplementation MUST ensure that the wrapped key can NOT be maintained
** beyond a call to OEM_TEE_BASE_FreeTEEContext or system restart and still have
** useful meaning as a key at that time.
**
** Synopsis:
**
** This function wraps the given Key and 8 bytes of entropy with the given TK 
** for the purpose of maintaining the key in opaque form as it temporarily 
** leaves the TEE.
**
** Note: The default PK implementation uses an algorithm similar to
** OEM_TEE_BASE_WrapKeyForPersistedStorage but including the session ID
** as a salt to ensure transience.  You may use a completely different
** mechanism (such as an opaque handle to a key cached inside the TEE)
** as long as that mechanism is transient as described previously.
**
** Note: The caller makes no assumptions about the size of a wrapped key.
**
** Operations Performed:
**
**  1. If a buffer was given, wrap the given key using the given wrapping key
**     using AES Key Wrap and write the wrapped value to the given buffer at
**     the given offset.
**  2. Update the given offset with the size of the wrapped key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pWrappingKey:          (in)     The key with which to wrap the given key.
** f_pKeyToBeWrapped:       (in)     The key to be wrapped.
** f_pibWrappedKeyBuffer:   (in/out) On input, the offset to which to write the
**                                   given key (if a buffer was specified).
**                                   On output, the input value plus the size
**                                   of the wrapped key.
** f_pbWrappedKeyBuffer:    (in/out) The buffer to which to write the given key.
**                                   This parameter may be NULL if only the offset
**                                   update for the wrapped key size is required.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_WrapAES128KeyForTransientStorage(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const OEM_TEE_KEY                *f_pWrappingKey,
    __in                                      const OEM_TEE_KEY                *f_pKeyToBeWrapped,
    __inout                                         DRM_DWORD                  *f_pibWrappedKeyBuffer,
    __inout_opt                                     DRM_BYTE                   *f_pbWrappedKeyBuffer )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDR( _WrapAES128Key( f_pContext, f_pWrappingKey, f_pKeyToBeWrapped, f_pibWrappedKeyBuffer, f_pbWrappedKeyBuffer ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function reverses the operation performed by OEM_TEE_BASE_WrapAES128KeyForPersistedStorage.
** Refer to that function for details.
**
** Note: The caller makes no assumptions about the size of a wrapped key.
**
** Operations Performed:
**
**  1. Unwrap a key of the given type using the given key from the given buffer at the given offset.
**  2. Subtract the size of the wrapped key from the given size.
**  3. Add the size of the wrapped key to the given offset.
**  4. Return the unwrapped key.
**
** Arguments:
**
** f_pContext:                  (in/out) The TEE context returned from
**                                       OEM_TEE_BASE_AllocTEEContext.
** f_pWrappingKey:              (in)     The key with which to unwrap the given key.
** f_pcbWrappedKeyBuffer:       (in/out) On input, the size of the buffer remaining
**                                       beyond the given offset.
**                                       On output, the size of the buffer remaining
**                                       beyond the updated offset.
** f_pibWrappedKeyBuffer:       (in/out) On input, the offset in the given buffer
**                                       from which to read the wrapped key.
**                                       On output, the offset in the given buffer
**                                       immediately after the wrapped key.
** f_pbWrappedKeyBuffer:        (in)     The buffer from which to read the wrapped key.
** f_pPreallocatedUnwrappedKey: (in/out) The unwrapped key.
**                                       The OEM_TEE_KEY must be pre-allocated
**                                       by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                       and freed after use by the caller using
**                                       OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_UnwrapAES128KeyFromPersistedStorage(
    __inout                                  OEM_TEE_CONTEXT            *f_pContext,
    __in                               const OEM_TEE_KEY                *f_pWrappingKey,
    __inout                                  DRM_DWORD                  *f_pcbWrappedKeyBuffer,
    __inout                                  DRM_DWORD                  *f_pibWrappedKeyBuffer,
    __in                               const DRM_BYTE                   *f_pbWrappedKeyBuffer,
    __inout                                  OEM_TEE_KEY                *f_pPreallocatedUnwrappedKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkDR( _UnwrapAES128Key( f_pContext, f_pWrappingKey, f_pcbWrappedKeyBuffer, f_pibWrappedKeyBuffer, f_pbWrappedKeyBuffer, f_pPreallocatedUnwrappedKey ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function reverses the operation performed by OEM_TEE_BASE_WrapECC256KeyFromPersistedStorage.
** Refer to that function for details.
**
** Note: The caller makes no assumptions about the size of a wrapped key.
**
** Operations Performed:
**
**  1. Unwrap a key of the given type using the given key from the given buffer at the given offset.
**  2. Subtract the size of the wrapped key from the given size.
**  3. Add the size of the wrapped key to the given offset.
**  4. Return the unwrapped key.
**
** Arguments:
**
** f_pContext:                  (in/out) The TEE context returned from
**                                       OEM_TEE_BASE_AllocTEEContext.
** f_pWrappingKey:              (in)     The key with which to unwrap the given key.
** f_pcbWrappedKeyBuffer:       (in/out) On input, the size of the buffer remaining
**                                       beyond the given offset.
**                                       On output, the size of the buffer remaining
**                                       beyond the updated offset.
** f_pibWrappedKeyBuffer:       (in/out) On input, the offset in the given buffer
**                                       from which to read the wrapped key.
**                                       On output, the offset in the given buffer
**                                       immediately after the wrapped key.
** f_pbWrappedKeyBuffer:        (in)     The buffer from which to read the wrapped key.
** f_pPreallocatedUnwrappedKey: (out)    The unwrapped key.
**                                       The OEM_TEE_KEY must be pre-allocated
**                                       by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                       and freed after use by the caller using
**                                       OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT should not be const.")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_UnwrapECC256KeyFromPersistedStorage(
    __inout                                  OEM_TEE_CONTEXT            *f_pContext,
    __in                               const OEM_TEE_KEY                *f_pWrappingKey,
    __inout                                  DRM_DWORD                  *f_pcbWrappedKeyBuffer,
    __inout                                  DRM_DWORD                  *f_pibWrappedKeyBuffer,
    __in                               const DRM_BYTE                   *f_pbWrappedKeyBuffer,
    __inout                                  OEM_TEE_KEY                *f_pPreallocatedUnwrappedKey )
{
    DRM_RESULT                  dr = DRM_SUCCESS;

    OEM_WRAPPED_KEY_ECC_256     oWrappedKey;      /* Do not zero-initialize key material */
    OEM_UNWRAPPED_KEY_ECC_256   oUnwrappedKey;    /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pWrappingKey               != NULL );
    DRMASSERT( f_pcbWrappedKeyBuffer        != NULL );
    DRMASSERT( f_pibWrappedKeyBuffer        != NULL );
    DRMASSERT( f_pbWrappedKeyBuffer         != NULL );
    DRMASSERT( f_pPreallocatedUnwrappedKey  != NULL );

    ChkBOOL( *f_pcbWrappedKeyBuffer >= sizeof(OEM_WRAPPED_KEY_ECC_256), DRM_E_TEE_ILWALID_KEY_DATA );
    ChkVOID( OEM_TEE_MEMCPY( &oWrappedKey, &f_pbWrappedKeyBuffer[*f_pibWrappedKeyBuffer], sizeof(oWrappedKey) ) );

    ChkDR( OEM_TEE_CRYPTO_AESKEYWRAP_UnwrapKeyECC256( f_pContext, f_pWrappingKey, &oWrappedKey, &oUnwrappedKey ) );

    ChkVOID( OEM_TEE_MEMCPY( 
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256( f_pPreallocatedUnwrappedKey ), 
        &oUnwrappedKey,
        sizeof(PRIVKEY_P256) ) );

    *f_pcbWrappedKeyBuffer -= sizeof(OEM_WRAPPED_KEY_ECC_256);
    *f_pibWrappedKeyBuffer += sizeof(OEM_WRAPPED_KEY_ECC_256);

ErrorExit:
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oWrappedKey, sizeof(oWrappedKey ) ) );
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oUnwrappedKey, sizeof(oUnwrappedKey ) ) );

    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST
** have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function reverses the operation performed by OEM_TEE_BASE_WrapKeyForTransientStorage.
** Refer to that function for details.
**
** Note: The caller makes no assumptions about the size of a wrapped key.
**
** Operations Performed:
**
**  1. Unwrap a key of the given type using the given key from the given buffer at the given offset.
**  2. Subtract the size of the wrapped key from the given size.
**  3. Add the size of the wrapped key to the given offset.
**  4. Return the unwrapped key.
**
** Arguments:
**
** f_pContextAllowNULL:         (in/out) The TEE context returned from
**                                       OEM_TEE_BASE_AllocTEEContext.
**                                       This function may receive NULL.
**                                       This function may receive an
**                                       OEM_TEE_CONTEXT where
**                                       cbUnprotectedOEMData == 0 and
**                                       pbUnprotectedOEMData == NULL.
** f_pWrappingKey:              (in)     The key with which to unwrap the given key.
** f_pcbWrappedKeyBuffer:       (in/out) On input, the size of the buffer
**                                       remaining beyond the given offset.
**                                       On output, the size of the buffer
**                                       remaining beyond the updated offset.
** f_pibWrappedKeyBuffer:       (in/out) On input, the offset in the given buffer
**                                       from which to read the wrapped key.
**                                       On output, the offset in the given buffer
**                                       immediately after the wrapped key.
** f_pbWrappedKeyBuffer:        (in)     The buffer from which to read the wrapped key.
** f_pPreallocatedUnwrappedKey: (in/out) The unwrapped key which has already been allocated by the
**                                       caller using OEM_TEE_BASE_AllocKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_UnwrapAES128KeyFromTransientStorage(
    __inout_opt                                     OEM_TEE_CONTEXT     *f_pContextAllowNULL,
    __in                                      const OEM_TEE_KEY         *f_pWrappingKey,
    __inout                                         DRM_DWORD           *f_pcbWrappedKeyBuffer,
    __inout                                         DRM_DWORD           *f_pibWrappedKeyBuffer,
    __in                                      const DRM_BYTE            *f_pbWrappedKeyBuffer,
    __inout                                         OEM_TEE_KEY         *f_pPreallocatedUnwrappedKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkDR( _UnwrapAES128Key( f_pContextAllowNULL, f_pWrappingKey, f_pcbWrappedKeyBuffer, f_pibWrappedKeyBuffer, f_pbWrappedKeyBuffer, f_pPreallocatedUnwrappedKey ) );

ErrorExit:
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 */

