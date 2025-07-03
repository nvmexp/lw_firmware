/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>
#include <aes128.h>


ENTER_PK_NAMESPACE_CODE;

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** If your hardware supports a security boundary between code running inside
** and outside the TEE, the key allocated by this function MUST be memory to
** which only the TEE has access.
** Otherwise, You do not need to replace this function implementation. 
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function allocates an uninitialized AES 128 Key.
**
** Operations Performed:
**
**  1. Allocate an uninitialized AES 128 Key.
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
** f_pKey:                 (out) The allocated AES 128 Key.
**                               This will be freed with OEM_TEE_CRYPTO_AES128_FreeKey.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_AllocKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __out                                                 OEM_TEE_KEY                  *f_pKey )
{
    DRMASSERT( f_pKey != NULL );
    return OEM_TEE_BASE_SelwreMemAlloc( f_pContextAllowNULL, sizeof(OEM_TEE_KEY_AES), (DRM_VOID**)&f_pKey->pKey ); /* Do not zero-initialize key material */
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** If your hardware supports a security boundary between code running inside
** and outside the TEE, this function MUST protect against freeing a maliciously-
** specified key, e.g. where the memory being freed is inaccessible or invalid.
** Otherwise, You do not need to replace this function implementation. 
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function frees a key allocated by OEM_TEE_CRYPTO_AES128_AllocKey.
**
** Operations Performed:
**
**  1. Zero and free the given key.
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
** f_pKey:              (in/out) The key to be freed.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_AES128_FreeKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout                                               OEM_TEE_KEY                  *f_pKey )
{
    DRMASSERT( f_pKey != NULL );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContextAllowNULL, &f_pKey->pKey ) );
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( f_pKey, sizeof(*f_pKey) ) );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function initializes a given pre-allocated AES 128 Key with the given 128 bytes which represent the raw key.
**
** Operations Performed:
**
**  1. Initialize the given key with the given bytes.
**
** Arguments:
**
** f_pContextAllowNULL:    (in/out) The TEE context returned from
**                                  OEM_TEE_BASE_AllocTEEContext.
**                                  This function may receive NULL.
**                                  This function may receive an
**                                  OEM_TEE_CONTEXT where
**                                  cbUnprotectedOEMData == 0 and
**                                  pbUnprotectedOEMData == NULL.
** f_pPreallocatedDestKey: (in/out) The key to be initialized.
**                                  The OEM_TEE_KEY MUST be pre-allocated
**                                  by the caller using OEM_TEE_CRYPTO_AES128_AllocKey
**                                  and freed after use by the caller using
**                                  OEM_TEE_CRYPTO_AES128_FreeKey.
** f_rgbKey:                   (in) The 128 bytes which represent the raw key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_SetKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __inout_ecount( 1 )                                   OEM_TEE_KEY                  *f_pPreallocatedDestKey,
    __in_bcount( DRM_AES_KEYSIZE_128 )              const DRM_BYTE                      f_rgbKey[ DRM_AES_KEYSIZE_128 ] )
{
    OEM_TEE_MEMCPY( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( f_pPreallocatedDestKey ), f_rgbKey, DRM_AES_KEYSIZE_128 );
#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)
    /*
    ** LWE: (vyadav) Only do this if we're using SW implementation of AES. If we
    ** use HW implementation then we don't need to generate the round keys in SW.
    */
    return Oem_Aes_SetKey( f_rgbKey, OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY( f_pPreallocatedDestKey ) );
#else
    return DRM_SUCCESS;
#endif
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function retrieves the 128 bytes which represent the raw key from the given initialized key.
**
** Operations Performed:
**
**  1. Return the raw key bytes.
**
** Arguments:
**
** f_pContextAllowNULL:    (in/out) The TEE context returned from
**                                  OEM_TEE_BASE_AllocTEEContext.
**                                  This function may receive NULL.
**                                  This function may receive an
**                                  OEM_TEE_CONTEXT where
**                                  cbUnprotectedOEMData == 0 and
**                                  pbUnprotectedOEMData == NULL.
** f_pKey:                     (in) The key for which to get the raw bytes.
** f_rgbKey:                  (out) The 128 bytes which represent the raw key.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_CRYPTO_AES128_GetKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in                                            const OEM_TEE_KEY                  *f_pKey,
    __out_bcount( DRM_AES_KEYSIZE_128 )                   DRM_BYTE                      f_rgbKey[DRM_AES_KEYSIZE_128] )
{
    DRMASSERT( f_pKey->pKey != NULL );
    OEM_TEE_MEMCPY( f_rgbKey, ( (OEM_TEE_KEY_AES*)(f_pKey->pKey) )->rgbRawKey, DRM_AES_KEYSIZE_128 );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function initializes an uninitialized AES 128 Key using the same key as another
** initialized AES 128 Key (i.e. it copies from a source key to a destination key).
**
** Operations Performed:
**
**  1. Copy the given source key to the given destination key.
**
** Arguments:
**
** f_pContextAllowNULL:    (in/out) The TEE context returned from
**                                  OEM_TEE_BASE_AllocTEEContext.
**                                  This function may receive NULL.
**                                  This function may receive an
**                                  OEM_TEE_CONTEXT where
**                                  cbUnprotectedOEMData == 0 and
**                                  pbUnprotectedOEMData == NULL.
** f_pPreallocatedDestKey: (in/out) The copied key.
**                                  The OEM_TEE_KEY MUST be pre-allocated
**                                  by the caller using OEM_TEE_CRYPTO_AES128_AllocKey
**                                  and freed after use by the caller using
**                                  OEM_TEE_CRYPTO_AES128_FreeKey.
** f_pSourceKey:               (in) The key being copied.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_CopyKey(
    __inout_opt                                         OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __inout                                             OEM_TEE_KEY                *f_pPreallocatedDestKey,
    __in                                          const OEM_TEE_KEY                *f_pSourceKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRMASSERT( f_pPreallocatedDestKey != NULL );
    DRMASSERT( f_pSourceKey           != NULL );

    OEM_TEE_MEMCPY(
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( f_pPreallocatedDestKey ),
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( f_pSourceKey ),
        DRM_AES_KEYSIZE_128 );

#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)
    /*
    ** LWE: (vyadav) Only do this if we're using SW implementation of AES. If we
    ** use HW implementation then we don't need to generate the round keys in SW.
    */
    ChkDR( Oem_Aes_SetKey(
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( f_pPreallocatedDestKey ),
        OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY( f_pPreallocatedDestKey ) ) );

ErrorExit:
#endif

    return dr;
}

/*********************************************************************************************
** Function:  _EncryptDecryptOneBlock_HS_LS
**
** Synopsis:  This helper function help determine whether an AES request from the caller needs to
**            be done via HW secret at HS mode (when KDF mode is explicitly requested) or via SW secret
**            at LS mode (when KDF mode is not requested), then it will call the corresponding HS or LS
**            code based on its decision.
**
** Arguments:
**            [f_pKey]        : The key to be used for encryption/descryption (not used when KDF mode is requested)
**            [f_rgbData]     : Buffer storing the data for encryption/descryption and returning the encrypted/decrypted data
**            [f_fCryptoMode] : Specifies if encryption or decryption is requested
**
** Returns:   DRM_SUCCESS on success
**********************************************************************************************/
GCC_ATTRIB_NO_STACK_PROTECT()
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL _EncryptDecryptOneBlock_HS_LS(
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )                    DRM_BYTE                      f_rgbData[ DRM_AES_BLOCKLEN ],
    __in                                            const DRM_DWORD                     f_fCryptoMode )
{
    DRM_RESULT dr = DRM_SUCCESS;

    if (!FLD_TEST_DRF(_PR_AES128, _KDF, _MODE, _NONE, f_fCryptoMode))
    {
        // Prepare the required arguments for HS exelwtion
        AES_KDF_ARGS aesKdfArgs;

        aesKdfArgs.f_pbBuffer    = f_rgbData;
        aesKdfArgs.f_fCryptoMode = f_fCryptoMode;

        // Perform the KDF operation at HS mode
        ChkDR( OEM_TEE_HsCommonEntryWrapper(PR_HS_AES_KDF, OVL_INDEX_IMEM(libMemHs), (DRM_VOID *)&aesKdfArgs ) );
    }
    else
    {
        // f_pKey can not be NULL when KDF mode is not requested (SW secret is used)
        ChkPtrAndReturn(f_pKey, DRM_E_ILWALIDARG);

        // Encrypt/Decrypt at LS when SW secret is requested
        dr = Oem_Aes_EncryptDecryptOneBlock_LWIDIA_LS( f_rgbData, f_rgbData, OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( f_pKey ), f_fCryptoMode );
    }

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function AES 128 encrypts the given block of data in-place using the given AES 128 key.
**
** Operations Performed:
**
**  1. Encrypt the given block using the given key.
**
** Arguments:
**
** f_pKey:            (in) The key to use for encryption.
** f_rgbData:     (in/out) The data to encrypt in-place.
** f_fCryptoMode:     (in) Specifies the crypto mode and whether the SW or HW secret will be used
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_EncryptOneBlock(
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )                    DRM_BYTE                      f_rgbData[ DRM_AES_BLOCKLEN ],
    __in                                            const DRM_DWORD                     f_fCryptoMode )
{

#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)
    return Oem_Aes_EncryptOne( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY( f_pKey ), f_rgbData );
#else
    return _EncryptDecryptOneBlock_HS_LS(f_pKey, f_rgbData, f_fCryptoMode);
#endif
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port if
** this function is still called by other OEM_TEE functions once your port
** is complete.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function AES 128 decrypts the given block of data in-place using the given AES 128 key.
**
** Operations Performed:
**
**  1. Decrypt the given block using the given key.
**
** Arguments:
**
** f_pKey:             (in) The key to use for decryption.
** f_rgbData:      (in/out) The data to decrypt in-place.
** f_fCryptoMode:      (in) Specifies the crypto mode and whether the SW or HW secret will be used
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_AES128_DecryptOneBlock(
    __in_ecount( 1 )                                const OEM_TEE_KEY                  *f_pKey,
    __inout_bcount( DRM_AES_BLOCKLEN )                    DRM_BYTE                      f_rgbData[ DRM_AES_BLOCKLEN ],
    __in                                            const DRM_DWORD                     f_fCryptoMode )
{
#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)
    return Oem_Aes_DecryptOne( OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY( f_pKey ), f_rgbData );
#else
    return _EncryptDecryptOneBlock_HS_LS(f_pKey, f_rgbData, f_fCryptoMode);
#endif
}
#endif

EXIT_PK_NAMESPACE_CODE;

