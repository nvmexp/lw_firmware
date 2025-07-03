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

// LWE (nkuo): LW internal headers which are referenced by the code added in this file
#include <aes128.h>
#include "dev_fuse.h"
#include "sec2_objsec2.h"

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT* should not be const." )
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_BUFFER_PARAM_25033, "Out params can't be const" )

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
#define HMAC_GENERATION_RETRY_MAXIMUM 10

// LWE (nkuo): TKs are generated on demand in production code. So we don't need these globals to be compiled in.
#ifdef SINGLE_METHOD_ID_REPLAY
static OEM_TEE_TK_HISTORY g_oTEETKHistory PR_ATTR_DATA_OVLY(_g_oTEETKHistory) = { 0 };
DRM_EXPORT_VAR DRM_VOID *g_pTEETKHistory  PR_ATTR_DATA_OVLY(_g_pTEETKHistory) = &g_oTEETKHistory;
#endif

// LWE (nkuo): We need to call this function in DRM_TEE_STUB_HandleMethodRequest() in SINGLE_METHOD_ID_REPLAY mode, so can't be declared as static
#ifdef SINGLE_METHOD_ID_REPLAY
/*
** This is a simple TK (TEE-key) history initialization. One must choose to use another mechanism, at least for initializing the current TK.
*/
DRM_RESULT DRM_CALL _InitializeTKeys( DRM_VOID )
{
    DRM_RESULT drRet   = DRM_SUCCESS;  /* Do not use dr to ensure no jump macros are used. */
    DRM_RESULT drLeave = DRM_SUCCESS;

    static const DRM_BYTE g_rgbCTK_TEST[DRM_AES_KEYSIZE_128] PR_ATTR_DATA_OVLY(_g_rgbCTK_TEST) = {
        0x8B, 0x22, 0x2F, 0xFD, 0x1E, 0x76, 0x19, 0x56,
        0x59, 0xCF, 0x27, 0x03, 0x89, 0x8C, 0x42, 0x7F
    };
    OEM_TEE_TK_HISTORY  *pTEETKHistory = NULL;

    {
        /* Never use jump macros while holding the critical section. */
        if( DRM_SUCCEEDED( drRet = OEM_TEE_BASE_CRITSEC_Enter() ) )
        {
            pTEETKHistory = DRM_REINTERPRET_CAST( OEM_TEE_TK_HISTORY, g_pTEETKHistory );

            if( pTEETKHistory != NULL )
            {
                if( pTEETKHistory->coKeys == 0 )
                {
                    ChkVOID( OEM_TEE_CRYPTO_BASE_GetStaticKeyHistoryKey( &pTEETKHistory->rgoKeys[ 0 ] ) );

                    drRet = OEM_TEE_CRYPTO_AES128_SetKey( NULL, &pTEETKHistory->rgoKeys[ 0 ], g_rgbCTK_TEST );
                    if( DRM_FAILED( drRet ) )
                    {
                        ChkVOID( OEM_SELWRE_ZERO_MEMORY( pTEETKHistory, sizeof( *pTEETKHistory ) ) );
                    }
                    else
                    {
                        pTEETKHistory->coKeys = 1;
                    }
                }
                else
                {
                    /* no-op: Key history is already initialized */
                }
            }
            else
            {
                DRMASSERT( FALSE );
                drRet = DRM_E_LOGICERR;
            }

            drLeave = OEM_TEE_BASE_CRITSEC_Leave();
        }
    }

    return DRM_FAILED( drRet ) ? drRet : drLeave;
}
#endif

/*
** This function assumes the PLAINTEXT_P256 object contains two 128-bit AES keys where the 2nd key is set.
** It generates the 1st 128-bit AES key such that the PLAINTEXT_P256 is ECC encryptable
*/
static DRM_RESULT DRM_CALL _GenerateKeyForECCEncryptableKeyPair(
    __inout                                            OEM_TEE_CONTEXT        *f_pContext,
    __inout                                            PLAINTEXT_P256         *f_pPlainText )
{
    DRM_RESULT dr           = DRM_SUCCESS;
    DRM_DWORD  dwRetryCount = 0;

    ChkArg( f_pPlainText != NULL );

    /*
    ** Randomly generate an ECC P256 encryptable pair of AES keys
    ** There is ~.01% chance that this will fail (the entropy may fail to give a legit value), hence the loop.
    ** Any other error should fall out of the loop and be caught by the ChkDR below.
    ** See OEM_ECC_GenerateHMACKey_P256 declaration for more info on this failure case.
    */

    for( dwRetryCount = 0; dwRetryCount < HMAC_GENERATION_RETRY_MAXIMUM; dwRetryCount++ )
    {
        dr = OEM_TEE_CRYPTO_ECC256_GenerateHMACKey( f_pContext, f_pPlainText );

        if( dr != DRM_E_P256_HMAC_KEYGEN_FAILURE )
        {
            break;
        }
    }

    ChkDR( dr );

ErrorExit:
    return dr;
}

/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation that is
** specific to your PlayReady port. (You must also generate or build-in a new AES key
** to use as the CTK and protect it as a Device Asset per PlayReady Compliance and
** Robustness Rules. It must be unique per device.)
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function initializes an OEM_TEE_CONTEXT.
** It also ensures that the CTK/CTKID and all older TKs/TKIDs (if any)
** are all initialized such that the following OEM functions can be called successfully.
** OEM_TEE_BASE_GetCTKID
** OEM_TEE_BASE_GetTKByID
**
** Note: The default PK implementation does not initialize any older TKs/TKIDs
** (no key history) and initializes the CTK/CTKID to fixed TEST values which MUST
** be replaced by you.  The CTK and all older TKs must be robustly protected
** in an OEM-specific manner.
**
** The CTK must be unique PER DEVICE.  Two devices must not have the same CTK
** even if they are devices which are the exact same make and model.
** The CTK must NOT change without an update to the TEE software.
**
** Note: Refer to comments on the declaration of the OEM_TEE_CONTEXT structure
** before using its members.  There are important security and functionality
** concerns that you need to be aware of before you use place data in this structure.
**
** Operations Performed:
**
**  1. Initialize the CTK/CTKID and all older TKs/TKIDs (if any).
**  2. Initialize the OEM_TEE_CONTEXT.
**
** Arguments:
**
** f_pContext:           (in/out) On output this will be a newly allocated OEM_TEE_CONTEXT.
**                                This will be freed by OEM_TEE_BASE_FreeTEEContext.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AllocTEEContext(
    __inout                 OEM_TEE_CONTEXT              *f_pContext )
{
    DRM_RESULT   dr                     = DRM_SUCCESS;

    ChkArg( f_pContext != NULL );
    DRMASSERT( f_pContext->pbUnprotectedOEMData == NULL );  /* Catch leaks */

// LWE (nkuo): TKs are generated on demand in production code. So we don't need to call following function.
#ifdef SINGLE_METHOD_ID_REPLAY
    ChkDR( _InitializeTKeys() );
#endif

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation.
** However, this function MUST have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function uninitializes an OEM_TEE_CONTEXT.
** Note: This function does NOT (and MUST not) uninitialize the CTK and CTKID.
**
** Operations Performed:
**
**  1. Uninitialize the OEM_TEE_CONTEXT.
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
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_BASE_FreeTEEContext(
    __inout_opt           OEM_TEE_CONTEXT              *f_pContextAllowNULL )
{
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    if( f_pContextAllowNULL != NULL )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContextAllowNULL, (DRM_VOID **)&f_pContextAllowNULL->pbUnprotectedOEMData ) );

        f_pContextAllowNULL->cbUnprotectedOEMData    = 0;
        f_pContextAllowNULL->pbUnprotectedOEMData    = NULL;
    }
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However,
** this function MUST have a valid implementation for your PlayReady
** port.
**
** If your hardware supports a security boundary between code running
** inside and outside the TEE, the key allocated by this function MUST
** be memory to which only the TEE has access.
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
** f_pKey:              (out)    The allocated AES 128 Key.
**                               This will be freed with OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AllocKeyAES128(
    __inout_opt                                           OEM_TEE_CONTEXT        *f_pContextAllowNULL,
    __out                                                 OEM_TEE_KEY            *f_pKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_pKey != NULL );

    ChkDR( OEM_TEE_CRYPTO_AES128_AllocKey( f_pContextAllowNULL, f_pKey ) );     /* Do not zero-initialize key material */

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However,
** this function MUST have a valid implementation for your PlayReady
** port.
**
** If your hardware supports a security boundary between code running
** inside and outside the TEE, the key allocated by this function MUST
** be memory to which only the TEE has access.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function allocates an uninitialized ECC 256 Key.
**
** Operations Performed:
**
**  1. Allocate an uninitialized ECC 256 Key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pKey:               (out)    The allocated ECC 256 Key.
**                                This will be freed with OEM_TEE_BASE_FreeKeyECC256.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AllocKeyECC256(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __out                                                 OEM_TEE_KEY                *f_pKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pKey != NULL );

    ChkDR( OEM_TEE_CRYPTO_ECC256_AllocKey( f_pContext, f_pKey ) );     /* Do not zero-initialize key material */

ErrorExit:
    return dr;
}


/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function
** MUST have a valid implementation for your PlayReady port.
**
** If the your hardware supports a security boundary between code running inside
** and outside the TEE, this function MUST protect against freeing a maliciously-
** specified key, e.g. where the memory being freed is inaccessible or invalid.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function frees a key allocated by OEM_TEE_BASE_AllocKeyAES128.
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
** f_pKey               (in/out) The key to be freed.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_BASE_FreeKeyAES128(
    __inout_opt                                           OEM_TEE_CONTEXT        *f_pContextAllowNULL,
    __inout                                               OEM_TEE_KEY            *f_pKey )
{
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    ChkVOID( OEM_TEE_CRYPTO_AES128_FreeKey( f_pContextAllowNULL, f_pKey ) );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However,
** this function MUST have a valid implementation for your PlayReady
** port.
**
** If your hardware supports a security boundary between code running inside
** and outside the TEE, this function MUST protect against freeing a maliciously-
** specified key, e.g. where the memory being freed is inaccessible or invalid.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function frees a key allocated by OEM_TEE_BASE_AllocKeyECC256.
**
** Operations Performed:
**
**  1. Zero and free the given key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pKey:               (in/out) The key to be freed.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL OEM_TEE_BASE_FreeKeyECC256(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __inout                                               OEM_TEE_KEY                *f_pKey )
{
    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    ChkVOID( OEM_TEE_CRYPTO_ECC256_FreeKey( f_pContext, f_pKey ) );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However,
** this function MUST have a valid implementation for your PlayReady
** port.
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
** f_pContext:                  (in/out) The TEE context returned from
**                                       OEM_TEE_BASE_AllocTEEContext.
** f_pPreallocatedDestKey:      (in/out) The copied key.
**                                       The OEM_TEE_KEY MUST be pre-allocated
**                                       by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                       and freed after use by the caller using
**                                       OEM_TEE_BASE_FreeKeyAES128.
** f_pSourceKey:                (in)     The key being copied.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_CopyKeyAES128(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __inout                                         OEM_TEE_KEY                *f_pPreallocatedDestKey,
    __in                                      const OEM_TEE_KEY                *f_pSourceKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pPreallocatedDestKey != NULL );
    DRMASSERT( f_pSourceKey           != NULL );

    ChkDR( OEM_TEE_CRYPTO_AES128_CopyKey( f_pContext, f_pPreallocatedDestKey, f_pSourceKey ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However,
** this function MUST have a valid implementation for your PlayReady
** port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function returns the ID of the CTK.
** If the CTK is the only TK, this function MUST return 0.
** Otherwise, this function MUST return the number of keys minus 1.
** Each time an update changes the CTK, the ID of the new CTK
** MUST be equal to the ID of the previous CTK + 1.
**
** Operations Performed:
**
**  1. Return the ID of the CTK.
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
** f_pidCTK:            (out)    The ID of the CTK.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GetCTKID(
    __inout_opt                                     OEM_TEE_CONTEXT         *f_pContextAllowNULL,
    __out                                           DRM_DWORD               *f_pdwidCTK )
{
    DRM_RESULT dr = DRM_SUCCESS;
#ifdef SINGLE_METHOD_ID_REPLAY
    /* Never use jump macros while holding the critical section. */
    //
    // LWE (kwilson) The critsec mutex token could be leaked by the macro ChkBOOL jumping to
    // ErrorExit without releasing the token.  Since this code is under SINGLE_METHOD_ID_REPLAY,
    // which indicates a test only environment for Lwpu and not a production environment,
    // we plan to visit this later (low priority).
    //
    ChkDR(OEM_TEE_BASE_CRITSEC_Enter());
    const OEM_TEE_TK_HISTORY *pTEETKHistory = DRM_REINTERPRET_CONST_CAST( const OEM_TEE_TK_HISTORY, g_pTEETKHistory );
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_pdwidCTK != NULL );

    ChkBOOL( pTEETKHistory->coKeys > 0, DRM_E_TEE_LAYER_UNINITIALIZED );
    *f_pdwidCTK = pTEETKHistory->coKeys - 1;
    ChkDR(OEM_TEE_BASE_CRITSEC_Leave());
#else
    // LWE (nkuo): LW's implementation in returning the CTK ID.
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkArg ( f_pdwidCTK != NULL );

    *f_pdwidCTK = DRM_TEE_LWRRENT_TK_ID;
#endif


ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However,
** this function MUST have a valid implementation for your PlayReady
** port.
**
** This function is used in all PlayReady scenarios. It is part of the
** key history implementation for the TKeys.
**
** Synopsis:
**
** This function returns a TK given an ID.
** If the ID is 0, the oldest TK MUST be returned.
**
** Operations Performed:
**
**  1. Find a TK that has the input ID.
**  2. Return it.
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
** f_dwid:              (in)     The ID of the TK.
** f_pTK:               (in/out) The returned key.
**                               The OEM_TEE_KEY MUST be pre-allocated
**                               by the caller using OEM_TEE_BASE_AllocKeyAES128
**                               and freed after use by the caller using
**                               OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GetTKByID(
    __inout_opt                                           OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                                  DRM_DWORD                   f_dwid,
    __inout                                               OEM_TEE_KEY                *f_pTK )
{
    DRM_RESULT dr = DRM_SUCCESS;
#ifdef SINGLE_METHOD_ID_REPLAY
    /* Never use jump macros while holding the critical section. */
    //
    // LWE (kwilson) The critsec mutex token could be leaked by the macro ChkBOOL or ChkDR jumping to
    // ErrorExit without releasing the token.  Since this code is under SINGLE_METHOD_ID_REPLAY,
    // which indicates a test only environment for Lwpu and not a production environment,
    // we plan to visit this later (low priority).
    //
    ChkDR(OEM_TEE_BASE_CRITSEC_Enter());

    const OEM_TEE_TK_HISTORY *pTEETKHistory = DRM_REINTERPRET_CONST_CAST( const OEM_TEE_TK_HISTORY, g_pTEETKHistory );

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkBOOL( pTEETKHistory->coKeys > 0, DRM_E_TEE_LAYER_UNINITIALIZED );

    AssertChkArg( f_dwid < pTEETKHistory->coKeys );

    ChkDR( OEM_TEE_CRYPTO_AES128_CopyKey( f_pContextAllowNULL, f_pTK , &pTEETKHistory->rgoKeys[f_dwid] ) );
    ChkDR( OEM_TEE_BASE_CRITSEC_Leave() );
#else
    // LWE (nkuo): LW's implementation of TK generation.
    DRM_BYTE  rgbKey[ DRM_AES_KEYSIZE_128 ];
    DRM_DWORD fCryptoMode = DRF_DEF(_PR_AES128, _KDF, _MODE, _GEN_TK) |
                            DRF_NUM(_PR_AES128, _TK,  _ID,   f_dwid);

    // It is not allowed to generate any future TK
    ChkArg( f_dwid <= DRM_TEE_LWRRENT_TK_ID );

    // Prepare the required arguments for HS exelwtion
    AES_KDF_ARGS aesKdfArgs;
    aesKdfArgs.f_pbBuffer    = rgbKey;
    aesKdfArgs.f_fCryptoMode = fCryptoMode;

    ChkDR(OEM_TEE_HsCommonEntryWrapper(PR_HS_AES_KDF, OVL_INDEX_IMEM(libMemHs), (DRM_VOID *)&aesKdfArgs));

    // Fill the encrypted key into f_pTK.
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContextAllowNULL, f_pTK, aesKdfArgs.f_pbBuffer ) );
#endif


ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** The derived output key MUST be indefinitely repeatedly regenerable when this function
** is called with the same parameters (over the lifetime of the device).
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function derives one AES 128 Key from another AES 128 Key.
** The value of the derived key depends on the key derivation IDs,
** i.e. one of the fixed Microsoft-Defined g_idKeyDerivation* values.
**
** The algorithm defined in the following document is used for key derivation.
** "Recommendation for Key Derivation Using Pseudorandom Functions", NIST Special Publication 800-108,
** http://csrc.nist.gov/publications/nistpubs/800-108/sp800-108.pdf
**
** Because our derived key is an AES128 key which is 128 bits long ((i.e. 'L'==128)
** and our psueo-random function (PRF) is Oem_Omac1_Sign has output size of 128 bits (i.e. 'h'==128),
** we always perform only one iteration since 'L' == 'h' and therefore Ceil(L/h) == 'n' == 1.
**
** We choose to use 8 bits (one byte) for the size of the binary representation of 'i'.
**
** Our 'Label' is the input DRM_ID (f_pidKeyDerivationID).
** Our 'Context' is another input DRM_ID (f_pidContext) or 16 bytes with value 0x00.
** We keep '0x00' for the separator and use 'L' == 128 for output of AES-128 keys.
**
** Operations Performed:
**
**  1. Use the given derivation key and given Key Derivation ID to derive an AES 128 Key
**     using the NIST algorithm as dislwssed above.
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
** f_pTKDeriver:        (in)     The TK to use for the key derivation.
** f_pidLabel:          (in)     The input data label used to derive the derived key.
** f_pidContext:        (in)     The input data context used to derive the derived key.
**                               If NULL, 16 bytes of 0x00 are used instead.
** f_pTKDerived:        (in/out) The derived key.
**                               The OEM_TEE_KEY MUST be pre-allocated
**                               by the caller using OEM_TEE_BASE_AllocKeyAES128
**                               and freed after use by the caller using
**                               OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_DeriveKey(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                      const OEM_TEE_KEY                *f_pTKDeriver,
    __in                                      const DRM_ID                     *f_pidLabel,
    __in_opt                                  const DRM_ID                     *f_pidContext,
    __inout                                         OEM_TEE_KEY                *f_pTKDerived )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[DRM_AES_KEYSIZE_128]; /* Initialized by OEM_TEE_CRYPTO_AES128_KDFCTR_r8_L128 */

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMCASSERT( DRM_AES_KEYSIZE_128 == DRM_AES_BLOCKLEN );
    DRMASSERT( f_pTKDerived != NULL );
    DRMASSERT( f_pidLabel   != NULL );

    ChkDR( OEM_TEE_CRYPTO_AES128_KDFCTR_r8_L128(
        f_pContextAllowNULL,
        f_pTKDeriver,
        f_pidLabel,
        f_pidContext,
        rgbKey ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContextAllowNULL, f_pTKDerived, rgbKey ) );

ErrorExit:
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( rgbKey, sizeof(rgbKey) ) );
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function initializes a given pre-allocated AES 128 Key with the
** given 128 bytes which represent the raw key.
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
** f_pKey:                     (in) The AES secret key being initialized.
** f_rgbKey:                   (in) The 128 bytes which represent the raw key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AES128_SetKey(
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in                                                  OEM_TEE_KEY                  *f_pKey,
    __in_bcount( DRM_AES_BLOCKLEN )                 const DRM_BYTE                      f_rgbKey[ DRM_AES_BLOCKLEN ] )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContextAllowNULL, f_pKey, f_rgbKey ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function verifies a signature over the given data with given key using AES 128 OMAC1 VERIFY.
**
** Operations Performed:
**
**  1. Verify the given AES128 OMAC1 signature over the given data using the given key.
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
** f_pKey:              (in)     The key to use to verify the signature.
** f_cbData             (in)     The size of the data to verify.
** f_pbData:            (in)     The buffer containing data to verify.
** f_cbSignature:       (in)     The size of the signature.
** f_pbSignature:       (in)     The buffer containing the signature to verify.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_VerifyOMAC1Signature(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                      const OEM_TEE_KEY                *f_pKey,
    __in                                            DRM_DWORD                   f_cbData,
    __in_bcount( f_cbData )                   const DRM_BYTE                   *f_pbData,
    __in                                            DRM_DWORD                   f_cbSignature,
    __in_bcount( f_cbSignature )              const DRM_BYTE                   *f_pbSignature)
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    ChkBOOL( DRM_AES_BLOCKLEN == f_cbSignature, DRM_E_ILWALID_SIGNATURE );

    ChkDR( OEM_TEE_CRYPTO_AES128_OMAC1_Verify(
        f_pContextAllowNULL,
        f_pKey,
        f_pbData,
        0,
        f_cbData,
        f_pbSignature,
        0 ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However,
** this function MUST have a valid implementation for your PlayReady
** port.
**
** Any reimplementation MUST exactly match the behavior of the default
** PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function signs data using an AES-128 key.
**
** Operations Performed:
**
**  1. Sign the given data using the given AES key.
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
** f_pKey:              (in)     Given key.
** f_cbDataToSign:      (in)     The size of the data to sign.
** f_pbDataToSign:      (in)     The data to sign.
** f_pbSignature:       (out)    The signature.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SignWithOMAC1(
    __inout_opt                                           OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                            const OEM_TEE_KEY                *f_pKey,
    __in                                                  DRM_DWORD                   f_cbDataToSign,
    __in_bcount( f_cbDataToSign )                   const DRM_BYTE                   *f_pbDataToSign,
    __out_bcount( DRM_AES128OMAC1_SIZE_IN_BYTES )         DRM_BYTE                   *f_pbSignature )
{
    DRM_RESULT           dr         = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_pbSignature != NULL );

    ChkDR( OEM_TEE_CRYPTO_AES128_OMAC1_Sign( f_pContextAllowNULL, f_pKey, f_pbDataToSign, 0, f_cbDataToSign, f_pbSignature ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function creates an ECC Private Key for signing TEE information in such a way 
** that the REE cannot by creating a new private key equal to the previous private key
** doubled.
**
** Operations Performed:
**
**  1. Fills the Signing Key with the original ECC Private Key * 2.
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
** f_pEccKey:               (in) The current ECC Signing Private Key.
** f_pSigningKey:           (in) The new key to be filled with f_pEccKey * 2.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_ECC256_GenerateTeeSigningPrivateKey(
    __inout_opt                                        OEM_TEE_CONTEXT              *f_pContextAllowNULL,
    __in_ecount( 1 )                             const OEM_TEE_KEY                  *f_pEccKey,
    __out_ecount( 1 )                                  OEM_TEE_KEY                  *f_pSigningKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkDR( OEM_TEE_CRYPTO_ECC256_GenerateTeeSigningPrivateKey(
        f_pContextAllowNULL,
        f_pEccKey,
        f_pSigningKey ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function signs a hash using the given ECC 256 private signing key.
**
** Operations Performed:
**
**  1. Sign the given hash using the given ECC 256 private signing key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pSigningKey:        (in)     The ECC 256 private signing key.
** f_pbHashToSign:       (in)     The data to sign.
** f_pSignature:         (out)    The signature.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_ECDSA_P256_SignHash(
    __inout                                             OEM_TEE_CONTEXT            *f_pContext,
    __in                                          const OEM_TEE_KEY                *f_pSigningKey,
    __in_bcount( ECC_P256_INTEGER_SIZE_IN_BYTES ) const DRM_BYTE                    f_rgbHashToSign[ECC_P256_INTEGER_SIZE_IN_BYTES],
    __out                                               SIGNATURE_P256             *f_pSignature )
{
    DRM_RESULT dr = DRM_SUCCESS;
    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkDR( OEM_TEE_CRYPTO_ECC256_ECDSA_SignHash(
        f_pContext,
        f_rgbHashToSign,
        f_pSigningKey,
        f_pSignature ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function verifies the signature of the given data given a public key.
**
** Operations Performed:
**
**  1. Verify the signature of the given data using the given ECC 256 public key.
**
** Arguments:
**
** f_pContextAllowNULL:  (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
**                                This function may receive NULL.
**                                This function may receive an
**                                OEM_TEE_CONTEXT where
**                                cbUnprotectedOEMData == 0 and
**                                pbUnprotectedOEMData == NULL.
** f_cbDataToVerify:     (in)     The size of the data to verify.
** f_pbDataToVerify:     (in)     The data to verify.
** f_pPubKey:            (in)     The public key.
** f_pSignature:         (in)     The signature.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_ECDSA_P256_Verify(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                            DRM_DWORD                   f_cbDataToVerify,
    __in_bcount( f_cbDataToVerify )           const DRM_BYTE                   *f_pbDataToVerify,
    __in                                      const PUBKEY_P256                *f_pPubKey,
    __in                                      const SIGNATURE_P256             *f_pSignature )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkDR( OEM_TEE_CRYPTO_ECC256_ECDSA_SHA256_Verify(
        f_pContextAllowNULL,
        f_pbDataToVerify,
        f_cbDataToVerify,
        f_pPubKey,
        f_pSignature ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** ECC encrypts at least one AES 128-bit key. The first AES key is optional and if not
** provided this function will generate data for the first key such that the plaintext
** maps to an EC point.
**
** Operations Performed:
**
**  1. Copy the key or keys (depending on the input) into a plain text buffer.
**  2. ECC P256 encrypt the plain text buffer into the provided output cipher text
**     buffer.
**  3. Zero out the plain text buffer.
**
** Arguments:
** f_pContext:             (in/out) The TEE context returned from
**                                  OEM_TEE_BASE_AllocTEEContext.
** f_pEncryptingECCPubKey: (in)     Public key used to encrypt the AES keys
** f_pKey1:                (in)     Optional: First AES 128 key to encrypt
** f_pKey2:                (in)     Second AES 128 key to encrypt
** f_pEncryptedKeys:       (out)    Ciphertext of the encrypted AES keys.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_ECC256_Encrypt_AES128Keys(
    __inout                       OEM_TEE_CONTEXT     *f_pContext,
    __in                    const PUBKEY_P256         *f_pEncryptingECCPubKey,
    __in_opt                const OEM_TEE_KEY         *f_pKey1,
    __in                    const OEM_TEE_KEY         *f_pKey2,
    __out                         CIPHERTEXT_P256     *f_pEncryptedKeys )
{
    DRM_RESULT           dr          = DRM_SUCCESS;
    PLAINTEXT_P256       oPlainText;    /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkArg( f_pContext             != NULL );
    ChkArg( f_pEncryptingECCPubKey != NULL );
    ChkArg( f_pKey2                != NULL );
    ChkArg( f_pEncryptedKeys       != NULL );

    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pKey2, &oPlainText.m_rgbPlaintext[ DRM_AES_KEYSIZE_128 ] ) );

    if( f_pKey1 != NULL )
    {
        ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pKey1, oPlainText.m_rgbPlaintext ) );
    }
    else
    {
        ChkDR( _GenerateKeyForECCEncryptableKeyPair( f_pContext, &oPlainText ) );
    }
    ChkDR( OEM_TEE_CRYPTO_ECC256_Encrypt( f_pContext, f_pEncryptingECCPubKey, &oPlainText, f_pEncryptedKeys ) );

ErrorExit:
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oPlainText, sizeof(oPlainText) ) );
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function decrypts two AES 128-bit keys. The first AES key output parameter may
** be NULL if it is known that the cipher text represents only one AES key.
**
** Operations Performed:
**
**  1. ECC P256 Decrypt the given cipher text into two AES keys using the given key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pPrivKey:              (in)     The private key to decrypt the cipher text.
** f_pCiphertext:           (in)     The cipher text representing the two encrypted AES keys.
** f_pKey1:                 (in/out) First AES key.
**                                   The OEM_TEE_KEY MUST be pre-allocated
**                                   by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                   and freed after use by the caller using
**                                   OEM_TEE_BASE_FreeKeyAES128.
** f_pKey2:                 (in/out) Second AES key.
**                                   The OEM_TEE_KEY MUST be pre-allocated
**                                   by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                   and freed after use by the caller using
**                                   OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_ECC256_Decrypt_AES128Keys(
    __inout                                          OEM_TEE_CONTEXT       *f_pContext,
    __in                                       const OEM_TEE_KEY           *f_pPrivKey,
    __in                                       const CIPHERTEXT_P256       *f_pCiphertext,
    __inout_opt                                      OEM_TEE_KEY           *f_pKey1,
    __inout                                          OEM_TEE_KEY           *f_pKey2 )
{
    DRM_RESULT             dr           = DRM_SUCCESS;
    DRM_BYTE               rgbKeys[DRM_AES_KEYSIZE_128*2]; /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkArg( f_pContext    != NULL );
    ChkArg( f_pPrivKey    != NULL );
    ChkArg( f_pCiphertext != NULL );
    ChkArg( f_pKey2       != NULL );

    /* Decrypt the Domain Session Keys using the Device Private Key */
    ChkDR( OEM_TEE_CRYPTO_ECC256_Decrypt(
        f_pContext,
        f_pPrivKey,
        f_pCiphertext,
        (PLAINTEXT_P256*)rgbKeys ) );

    /* Copy the decryption output into two AES Keys and setup the AES key objects */
    if( f_pKey1 != NULL )
    {
        ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pKey1, &rgbKeys[0] ) );
    }

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pKey2, &rgbKeys[DRM_AES_KEYSIZE_128] ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is only used by remote provisioning.
**
** Synopsis:
**
** Does AES CBC-Mode encryption on a buffer of data.
**
** Operations Performed:
**
**  1. Encrypt the given data with the provided AES key.
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
** f_pKey:              (in)     The AES secret key used to encrypt the buffer.
** f_rgbIV:             (inout)  The AES CBC IV.
** f_pbData:            (inout)  The data to be encrypted.
** f_cbData:            (in)     The size (in bytes) of the data to be encrypted.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AES128CBC_EncryptData(
    __inout_opt                                     OEM_TEE_CONTEXT      *f_pContextAllowNULL,
    __in                                      const OEM_TEE_KEY          *f_pKey,
    __in_bcount( DRM_AES_BLOCKLEN )           const DRM_BYTE              f_rgbIV[ DRM_AES_BLOCKLEN ],
    __in                                            DRM_DWORD             f_cbData,
    __inout_bcount( f_cbData )                      DRM_BYTE             *f_pbData )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkDR( OEM_TEE_CRYPTO_AES128_CbcEncryptData(
        f_pContextAllowNULL,
        f_pKey,
        f_pbData,
        f_cbData,
        f_rgbIV ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Does AES CBC-Mode decryption on a buffer of data.
**
** Operations Performed:
**
**  1. Decrypt the given data with the provided AES key.
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
** f_pKey:              (in)     The AES secret key used to decrypt the buffer.
** f_rgbIV:             (inout)  The AES CBC IV.
** f_pbData:            (inout)  The data to be decrypted.
** f_cbData:            (in)     The size (in bytes) of the data to be decrypted.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AES128CBC_DecryptData(
    __inout_opt                                     OEM_TEE_CONTEXT  *f_pContextAllowNULL,
    __in                                      const OEM_TEE_KEY      *f_pKey,
    __in_bcount( DRM_AES_BLOCKLEN )           const DRM_BYTE          f_rgbIV[ DRM_AES_BLOCKLEN ],
    __in                                            DRM_DWORD         f_cbData,
    __inout_bcount( f_cbData )                      DRM_BYTE         *f_pbData )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );

    ChkDR( OEM_TEE_CRYPTO_AES128_CbcDecryptData(
        f_pContextAllowNULL,
        f_pKey,
        f_pbData,
        f_cbData,
        f_rgbIV ) );

ErrorExit:
    return dr;
}

/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation that is specific
** to your PlayReady port.
**
** You MUST provide a cryptographically-secure random number generator per the PlayReady
** Compliance and Robustness Rules.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Operations Performed:
**
**  1. Generate a set of random bytes using a cryptographically-secure random number generator.
**
** Arguments:
**
** f_pContextAllowNULL:  (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
**                                This function may receive NULL.
**                                This function may receive an
**                                OEM_TEE_CONTEXT where
**                                cbUnprotectedOEMData == 0 and
**                                pbUnprotectedOEMData == NULL.
** f_cb:                 (in)     The number of bytes to generate.
** f_pb:                 (out)    The randomly generated bytes.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GenerateRandomBytes(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                            DRM_DWORD                   f_cb,
    __out_bcount( f_cb )                            DRM_BYTE                   *f_pb )
{
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    return OEM_TEE_CRYPTO_RANDOM_GetBytes( f_pContextAllowNULL, f_pb, f_cb );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** If you override this function, you MUST provide a cryptographically-secure random number
** generator per the PlayReady Compliance and Robustness Rules, but the default implementation
** of this function ilwokes OEM_TEE_BASE_GenerateRandomBytes which should already be implementing
** one.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Operations Performed:
**
**  1. Generate a random AES 128 key.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pKey:               (in/out) The randomly generated key.
**                                The OEM_TEE_KEY MUST be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GenerateRandomAES128Key(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __inout                                               OEM_TEE_KEY                *f_pKey )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbKey[DRM_AES_KEYSIZE_128];/* Initialized by OEM_TEE_BASE_GenerateRandomBytes */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pKey != NULL );

    ChkDR( OEM_TEE_BASE_GenerateRandomBytes( f_pContext, DRM_AES_KEYSIZE_128, rgbKey ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pKey, rgbKey ) );

ErrorExit:
    ChkVOID( OEM_SELWRE_ZERO_MEMORY( rgbKey, sizeof(rgbKey) ) );
    return dr;
}
#endif

#ifdef NONE
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** If you override this function, you MUST provide a cryptographically-secure random number
** generator per the PlayReady Compliance and Robustness Rules, but the default implementation
** of this function ilwokes OEM_TEE_BASE_GenerateRandomBytes which should already be implementing
** one.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Operations Performed:
**
** 1. Randomly generate two ecc-encryptable AES keys.
**
** Arguments:
**
** f_pContext:             (in/out) The TEE context returned from
**                                  OEM_TEE_BASE_AllocTEEContext.
** f_pKey1:                (in/out) The first randomly generated AES key.
**                                  The OEM_TEE_KEY MUST be pre-allocated
**                                  by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                  and freed after use by the caller using
**                                  OEM_TEE_BASE_FreeKeyAES128.
** f_pKey2:                (in/out) The second randomly generated AES key.
**                                  The OEM_TEE_KEY MUST be pre-allocated
**                                  by the caller using OEM_TEE_BASE_AllocKeyAES128
**                                  and freed after use by the caller using
**                                  OEM_TEE_BASE_FreeKeyAES128.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GenerateRandomAES128KeyPair(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __inout                                               OEM_TEE_KEY                *f_pKey1,
    __inout                                               OEM_TEE_KEY                *f_pKey2 )
{
    DRM_RESULT          dr           = DRM_SUCCESS;
    DRM_BYTE            rgbKeyPair[ECC_P256_PLAINTEXT_SIZE_IN_BYTES]; /* Do not zero-initialize key material */

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pKey1 != NULL );
    DRMASSERT( f_pKey2 != NULL );

    DRMCASSERT( sizeof( PLAINTEXT_P256 ) == DRM_AES_KEYSIZE_128_X2 );
    DRMCASSERT( sizeof( PLAINTEXT_P256 ) == ECC_P256_PLAINTEXT_SIZE_IN_BYTES );

    ChkDR( OEM_TEE_BASE_GenerateRandomBytes( f_pContext, ECC_P256_PLAINTEXT_SIZE_IN_BYTES, rgbKeyPair ) );
    ChkDR( _GenerateKeyForECCEncryptableKeyPair( f_pContext, (PLAINTEXT_P256*)rgbKeyPair ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pKey1, rgbKeyPair ) );
    ChkDR( OEM_TEE_CRYPTO_AES128_SetKey( f_pContext, f_pKey2, &rgbKeyPair[DRM_AES_KEYSIZE_128] ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** If you reimplement the encryption portion of this API, any reimplementation MUST exactly
** match the behavior of the default PK implementation.
**
** If you override this function, you MUST provide a cryptographically-secure random number
** generator per the PlayReady Compliance and Robustness Rules. However, the default
** implementation of this function ilwokes OEM_TEE_BASE_GenerateRandomBytes which should
** already be implementing one.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Operations Performed:
**
**  1. Randomly generate two AES keys.
**  2. AESECB encrypt the two AES keys with a given AES key and return the encrypted result.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pEncryptingAESKey:  (in)     The AES128 key used to encrypt the generated key pair.
** f_pKey1:              (in/out) The first randomly generated AES128 key.
** f_pKey2:              (in/out) The second randomly generated AES128 key.
** f_pEncryptedKeys      (out)    The AESECB encrypted key pair.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GenerateRandomAES128KeyPairAndAES128ECBEncrypt(
    __inout                                               OEM_TEE_CONTEXT    *f_pContext,
    __in                                            const OEM_TEE_KEY        *f_pEncryptingKey,
    __inout                                               OEM_TEE_KEY        *f_pKey1,
    __inout                                               OEM_TEE_KEY        *f_pKey2,
    __out_bcount( DRM_AES_KEYSIZE_128_X2 )                DRM_BYTE           *f_pEncryptedKeys )
{
    DRM_RESULT          dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext  );
    DRMASSERT( f_pKey1            != NULL );
    DRMASSERT( f_pKey2            != NULL );
    DRMASSERT( f_pEncryptingKey   != NULL );
    DRMASSERT( f_pEncryptedKeys   != NULL );

    ChkDR( OEM_TEE_BASE_GenerateRandomAES128Key( f_pContext, f_pKey1 ) );
    ChkDR( OEM_TEE_BASE_GenerateRandomAES128Key( f_pContext, f_pKey2 ) );

    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pKey1, f_pEncryptedKeys ) );
    ChkVOID( OEM_TEE_CRYPTO_AES128_GetKey( f_pContext, f_pKey2, &f_pEncryptedKeys[ DRM_AES_KEYSIZE_128 ] ) );

    ChkDR( OEM_TEE_CRYPTO_AES128_EcbEncryptData( f_pContext, f_pEncryptingKey, f_pEncryptedKeys, DRM_AES_KEYSIZE_128_X2 ) );

ErrorExit:
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/* This ID has to be different from the g_idKeyDerivation* ones mentioned in drmteebase.c*/
static DRM_GLOBAL_CONST DRM_ID     s_idUniqueIDLabel PR_ATTR_DATA_OVLY(_s_idUniqueIDLabel) = { { 0x97, 0xc6, 0x09, 0xe0, 0x62, 0x8b, 0x40, 0x6d, 0xa2, 0x15, 0xcd, 0xc1, 0x19, 0xf2, 0x68, 0x38} };
/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation that
** is specific to your PlayReady port if your PlayReady port defines its own unique
** device identifier.
**
** If the default implementation is used, you MUST make sure the oldest TK does
** not change. You MUST provide an identifier which is unique to each instance of a
** device per the PlayReady Compliance and Robustness Rules.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This API MUST return an ID which does not change over the LIFETIME of the device
** (even across software and/or hardware updates).  This API MUST NOT allow a 3rd
** party to tie this ID back to a given device by associating it with publically
** available information for the device.
**
** Operations Performed:
**
**  1. Return the device ID.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pID:                (out)    The device-specific unique ID.
**                                This ID MUST not change over the lifetime of
**                                the device (including across generally destructive
**                                operations such as factory reset, as applicable).
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GetDeviceUniqueID(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __out                                                 DRM_ID                     *f_pID )
{
    DRM_RESULT              dr                              = DRM_SUCCESS;
    OEM_TEE_KEY             oIDKey                          = OEM_TEE_KEY_EMPTY;
    OEM_TEE_KEY             oTKOldest                       = OEM_TEE_KEY_EMPTY;
    DRM_BYTE                rgbBlob[DRM_AES_BLOCKLEN]       = {0};

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pID != NULL );

    ChkDR( OEM_TEE_BASE_AllocKeyAES128( f_pContext, &oTKOldest ) );
    ChkDR( OEM_TEE_BASE_AllocKeyAES128( f_pContext, &oIDKey ) );

    /* Find the oldest/rootmost TK key. */
    ChkDR( OEM_TEE_BASE_GetTKByID( f_pContext, DRM_TEE_OLDEST_TK_ID, &oTKOldest ) );

    ChkDR( OEM_TEE_BASE_DeriveKey(
        f_pContext,
        &oTKOldest,
        &s_idUniqueIDLabel,
        NULL,
        &oIDKey ) );

    /* Encrypt all zeroes */
    ChkDR( OEM_TEE_BASE_AES128ECB_EncryptData(
        f_pContext,
        &oIDKey,
        sizeof(rgbBlob),
        rgbBlob ) );

    ChkVOID( OEM_TEE_MEMCPY( f_pID, rgbBlob, sizeof(*f_pID) ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &oTKOldest ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &oIDKey ) );
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function encrypts the given data with an AES key using AES ECB.
**
** Operations Performed:
**
**  1. AES ECB encrypt the data using the provided Key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) AES Key to encrypt the data.
** f_cbData:                    (in) Length of the data to encrypt.
** f_pbData:                (in/out) Buffer to hold the clear data and receive the encrypted data.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AES128ECB_EncryptData(
    __inout                                         OEM_TEE_CONTEXT             *f_pContext,
    __in                                      const OEM_TEE_KEY                 *f_pKey,
    __in                                            DRM_DWORD                    f_cbData,
    __inout_bcount( f_cbData )                      DRM_BYTE                    *f_pbData )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    ChkDR( OEM_TEE_CRYPTO_AES128_EcbEncryptData( f_pContext, f_pKey, f_pbData, f_cbData ) );

ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function decrypts the given data with an AES key using AES ECB.
**
** Operations Performed:
**
**  1. AES ECB decrypt the data using the provided Key.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pKey:                      (in) AES Key to decrypt the data.
** f_cbData:                    (in) Length of the data to decrypt.
** f_pbData:                (in/out) Buffer to hold the clear data and receive the decrypted data.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_AES128ECB_DecryptData(
    __inout                                         OEM_TEE_CONTEXT             *f_pContext,
    __in                                      const OEM_TEE_KEY                 *f_pKey,
    __in                                            DRM_DWORD                    f_cbData,
    __inout_bcount( f_cbData )                      DRM_BYTE                    *f_pbData )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    ChkDR( OEM_TEE_CRYPTO_AES128_EcbDecryptData( f_pContext, f_pKey, f_pbData, f_cbData ) );

ErrorExit:
    return dr;
}
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function initialize the sha256 context structure.
**
** Operations Performed:
**
** 1. Initialize the sha256 context structure.
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
** f_pShaContext:          (out) The sha 256 context to be initialized.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SHA256_Init(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __out_ecount( 1 )                               OEM_SHA256_CONTEXT         *f_pShaContext )
{
    DRM_RESULT  dr  = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_pShaContext != NULL );

    ChkDR( OEM_TEE_CRYPTO_SHA256_Init( f_pContextAllowNULL, f_pShaContext ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function inserts data to be hashed in the current sha 256 context.
**
** Operations Performed:
**
** 1. Insert data to be hashed in the current sha 256 context.
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
** f_pShaContext:       (in/out) Current running sha context.
** f_cbBuffer:              (in) Length of the f_rgbBuffer buffer.
** f_rgbBuffer:             (in) Buffer to perform sha256 on.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SHA256_Update(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __inout_ecount( 1 )                             OEM_SHA256_CONTEXT         *f_pShaContext,
    __in                                            DRM_DWORD                   f_cbBuffer,
    __in_ecount( f_cbBuffer )                 const DRM_BYTE                    f_rgbBuffer[] )
{
    DRM_RESULT  dr  = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_pShaContext != NULL );

    ChkDR( OEM_TEE_CRYPTO_SHA256_Update( f_pContextAllowNULL, f_pShaContext, f_rgbBuffer, f_cbBuffer ) );

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function performs all of the SHA256 operations on the perviously inputted data and
** returns the digest value.
**
** Operations Performed:
**
** 1. Finalize the SHA256 digest from the input data collected from calls to
**    OEM_TEE_BASE_SHA256_Update.
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
** f_pShaContext:       (in/out) Current running sha context.
** f_pDigest:              (out) Sha digest to be returned.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_SHA256_Finalize(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __inout_ecount( 1 )                             OEM_SHA256_CONTEXT         *f_pShaContext,
    __out_ecount( 1 )                               OEM_SHA256_DIGEST          *f_pDigest )
{
    DRM_RESULT  dr  = DRM_SUCCESS;

    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    DRMASSERT( f_pShaContext != NULL );
    DRMASSERT( f_pDigest     != NULL );

    ChkDR( OEM_TEE_CRYPTO_SHA256_Finalize( f_pContextAllowNULL, f_pShaContext, f_pDigest ) );

ErrorExit:
    return dr;
}

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation that is specific
** to your PlayReady port if the device supports Local (DRM_SUPPORT_LOCAL_PROVISIONING=1)
** or Remote (DRM_SUPPORT_REMOTE_PROVISIONING=1) Provisioning.  Otherwise, this function MUST
** return DRM_E_NOTIMPL.
**
** The leaf certificate private keys MUST be randomly generated with a cryptographically-secure
** random number generator per the PlayReady Compliance and Robustness Rules.
**
** This function is only used if the client supports Local or Remote Provisioning.
** Certain PlayReady clients may not need to implement this function.
**
** Synopsis:
**
** This function generates a random ECC 256 public/private keypair.
** It MUST use a cryptographically-secure random number generator.
**
** Operations Performed:
**
**  1. Generate a random ECC 256 public/private keypair.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pKey:               (in/out) The ECC256 private key.
**                                The OEM_TEE_KEY MUST be pre-allocated
**                                by the caller using OEM_TEE_BASE_AllocKeyECC256
**                                and freed after use by the caller using
**                                OEM_TEE_BASE_FreeKeyECC256.
** f_pPubKey:            (out)    The corresponding ECC256 public key.
**                                This will be placed in leaf device certificate.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_PROV_GenerateRandomECC256KeyPair(
    __inout                                               OEM_TEE_CONTEXT            *f_pContext,
    __inout                                               OEM_TEE_KEY                *f_pKey,
    __out                                                 PUBKEY_P256                *f_pPubKey )
{
    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pKey != NULL );
    return OEM_TEE_CRYPTO_ECC256_GenKeyPairPriv( f_pContext, f_pPubKey, f_pKey );
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** Any reimplementation MUST exactly match the behavior of the default PK implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function takes in data to sign using ECC256 ECDSA with SHA256 as the hashing algorithm.
**
** Operations Performed:
**
**  1. Sign the given data with the given private signing key.
**
** Arguments:
**
** f_pContextAllowNULL:     (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
**                                   This function may receive NULL.
**                                   This function may receive an
**                                   OEM_TEE_CONTEXT where
**                                   cbUnprotectedOEMData == 0 and
**                                   pbUnprotectedOEMData == NULL.
** f_cbToSign:              (in)     The size of the data to be signed.
** f_pbToSign:              (in)     The data to be signed.
** f_pECCSigningKey:        (in)     The private ECC256 signing key.
** f_pSignature:            (out)    The signature over the given data with the given signing
**                                   key.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_ECDSA_P256_SignData(
    __inout_opt                         OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                DRM_DWORD                   f_cbToSign,
    __in_bcount( f_cbToSign )     const DRM_BYTE                   *f_pbToSign,
    __in                          const OEM_TEE_KEY                *f_pECCSigningKey,
    __out                               SIGNATURE_P256             *f_pSignature )
{
    ASSERT_TEE_CONTEXT_ALLOW_NULL( f_pContextAllowNULL );
    return OEM_TEE_CRYPTO_ECC256_ECDSA_SHA256_Sign(
        f_pContextAllowNULL,
        f_pbToSign,
        f_cbToSign,
        f_pECCSigningKey,
        f_pSignature );
}
#endif
EXIT_PK_NAMESPACE_CODE;

PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 */
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

