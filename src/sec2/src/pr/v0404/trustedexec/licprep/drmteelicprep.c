/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmteesupported.h>
#include <drmteebase.h>
#include <drmteekbcryptodata.h>
#include <drmteexb.h>
#include <drmxmrformatparser.h>
#include <drmmathsafe.h>
#include <drmnonceverify.h>
#include <oemtee.h>
#include <drmrivcrlparser.h>
#include <oemaeskeywrap.h>
#include <drmnoncestore.h>
#include <drmderivedkey.h>
#include <oembyteorder.h>
#include <drmteelicprepinternal.h>
#include <drmteeselwrestop2internal.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_LICPREP_IsLICPREPSupported( DRM_VOID )
{
    return TRUE;
}

/*
** Maximum expected number of licenses allowed to be created during a single session assuming key rotation.
*/
#define DRM_MAX_LICENSE_COUNT_FOR_NONCES ((DRM_DWORD)(1024))

static DRM_RESULT DRM_CALL _ParseAndVerifyInMemoryLicenseNonce(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pNKB,
    __in            const DRM_ID                       *f_pLID,
    __out                 DRM_ID                       *f_pidNonce )
{
    DRM_RESULT  dr       = DRM_SUCCESS;

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pNKB     != NULL );
    DRMASSERT( f_pLID     != NULL );
    DRMASSERT( f_pidNonce != NULL );

    ChkBOOL( IsBlobAssigned( f_pNKB ), DRM_E_NONCE_STORE_TOKEN_NOT_FOUND );

    /* Parse the given NKB and verify its signature. */
    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyNKB(
        f_pContext,
        f_pNKB,
        f_pidNonce ) );

    /* Check that the license is properly bound to the nonce */
    ChkDR( DRM_NONCE_VerifyNonce( f_pidNonce, f_pLID, DRM_MAX_LICENSE_COUNT_FOR_NONCES ) );

ErrorExit:
    return dr;
}

static DRM_GLOBAL_CONST DRM_BYTE g_rgbMagicConstantLeft[ DRM_AES_KEYSIZE_128 ] PR_ATTR_DATA_OVLY(_g_rgbMagicConstantLeft) = {
    0xf0, 0x61, 0x4d, 0xb6, 0x0f, 0xca, 0x3f, 0xbb,
    0x0f, 0x0d, 0x20, 0xf5, 0x61, 0xad, 0xec, 0x81 };

static DRM_GLOBAL_CONST DRM_BYTE g_rgbMagicConstantRight[ DRM_AES_KEYSIZE_128 ] PR_ATTR_DATA_OVLY(_g_rgbMagicConstantRight) = {
    0x3e, 0xf4, 0x88, 0xf3, 0x37, 0xe2, 0xda, 0xc6,
    0x62, 0x14, 0x9a, 0xdb, 0x82, 0x7e, 0x97, 0x6d };

static DRM_GLOBAL_CONST DRM_BYTE g_rgbMagicConstantZero[ DRM_AES_KEYSIZE_128 ] PR_ATTR_DATA_OVLY(_g_rgbMagicConstantZero) = {
    0x7e, 0xe9, 0xed, 0x4a, 0xf7, 0x73, 0x22, 0x4f,
    0x00, 0xb8, 0xea, 0x7e, 0xfb, 0x02, 0x7c, 0xbb };

typedef DRM_OBFUS_FIXED_ALIGN struct __tagDRM_TEE_AUX_KEY_ENTRY
{
    DRM_DWORD    dwLocation;
    DRM_TEE_KEY  oKey;
} DRM_TEE_AUX_KEY_ENTRY;

#define DRM_TEE_AUX_KEY_ENTRY_EMPTY { 0, DRM_TEE_KEY_EMPTY }

#define KEY_STACK_SIZE 2

PRAGMA_DIAG_OFF( attributes, "Turning warning off for GCC because it thinks these functions might not be able to be inlined for free builds.." )
static DRM_RESULT DRM_CALL _TryDeriveKey(
    __inout                             OEM_TEE_CONTEXT       *f_pContext,
    __in                          const DRM_DWORD              f_dwLocation,
    __inout_ecount( KEY_STACK_SIZE )    DRM_TEE_AUX_KEY_ENTRY *f_poKeyStack,
    __inout                             DRM_DWORD             *f_pdwLwrrentKeyIdx );
static DRM_RESULT DRM_CALL _TryDeriveKey(
    __inout                             OEM_TEE_CONTEXT       *f_pContext,
    __in                          const DRM_DWORD              f_dwLocation,
    __inout_ecount( KEY_STACK_SIZE )    DRM_TEE_AUX_KEY_ENTRY *f_poKeyStack,
    __inout                             DRM_DWORD             *f_pdwLwrrentKeyIdx )
{
    DRM_RESULT         dr                  = DRM_SUCCESS;
    DRM_DWORD          dwSBMParent         = 0;
    DRM_BOOL           fDeriveRight        = FALSE;
    const OEM_TEE_KEY *pScalableKey        = NULL;
    OEM_TEE_KEY       *pDerivedScalableKey = NULL;
    DRM_DWORD          dwDepth             = 0;

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_poKeyStack != NULL );
    DRMASSERT( f_pdwLwrrentKeyIdx != NULL );
    AssertChkArg( *f_pdwLwrrentKeyIdx == 0 );

    while( f_poKeyStack[ *f_pdwLwrrentKeyIdx ].dwLocation != f_dwLocation )
    {
        dwSBMParent = SBM( f_poKeyStack[ *f_pdwLwrrentKeyIdx ].dwLocation );

        fDeriveRight = ( f_dwLocation & dwSBMParent ) != 0;
        pScalableKey = &f_poKeyStack[ *f_pdwLwrrentKeyIdx ].oKey.oKey;
        pDerivedScalableKey = &f_poKeyStack[ *f_pdwLwrrentKeyIdx ^ 1 ].oKey.oKey;

        ChkDR( OEM_TEE_DECRYPT_DeriveScalableKeyWithAES128Key( f_pContext, pScalableKey, ( fDeriveRight ? g_rgbMagicConstantRight : g_rgbMagicConstantLeft ), pDerivedScalableKey ) );

        f_poKeyStack[ *f_pdwLwrrentKeyIdx ^ 1 ].dwLocation =
            ( ( ( f_poKeyStack[ *f_pdwLwrrentKeyIdx ].dwLocation & ( ~dwSBMParent ) ) |
            ( f_dwLocation & dwSBMParent ) ) |
                ( dwSBMParent >> 1 ) );

        ( *f_pdwLwrrentKeyIdx ) ^= 1;

        dwDepth++;
        ChkBOOL( dwDepth < MAX_KEY_STACK, DRM_E_UNABLE_TO_RESOLVE_LOCATION_TREE );

        AssertChkBOOL( *f_pdwLwrrentKeyIdx < KEY_STACK_SIZE );
    }

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _FindNextAuxEntry(
    __in                            DRM_DWORD                      f_dwLocation,
    __in                      const DRM_DWORD                      f_cAuxKeys,
    __in_ecount( f_cAuxKeys ) const DRM_XMRFORMAT_AUX_KEY_ENTRY   *f_prgAuxKeys,
    __inout                         DRM_DWORD                     *f_pdwAuxKeyIndex );
static DRM_RESULT DRM_CALL _FindNextAuxEntry(
    __in                            DRM_DWORD                      f_dwLocation,
    __in                      const DRM_DWORD                      f_cAuxKeys,
    __in_ecount( f_cAuxKeys ) const DRM_XMRFORMAT_AUX_KEY_ENTRY   *f_prgAuxKeys,
    __inout                         DRM_DWORD                     *f_pdwAuxKeyIndex )
{
    FIND_AUX_KEY_ENTRY_IMPL( DRM_S_FALSE );
}

static DRM_RESULT DRM_CALL _CalcOneDerivedKey(
    __inout                                   OEM_TEE_CONTEXT                   *f_pContext,
    __in                                const OEM_TEE_KEY                       *f_pContentKeyPrime,
    __in                                      DRM_DWORD                          f_cAuxKeys,
    __in_ecount( f_cAuxKeys )           const DRM_XMRFORMAT_AUX_KEY_ENTRY       *f_prgAuxKeys,
    __in                                const DRM_XMRFORMAT_UPLINKX             *f_poUplinkX,
    __inout                                   OEM_TEE_KEY                       *f_pUplinkXKey,
    __in                                const DRM_DWORD                          f_dwUplinkXIdx );
static DRM_RESULT DRM_CALL _CalcOneDerivedKey(
    __inout                                   OEM_TEE_CONTEXT                   *f_pContext,
    __in                                const OEM_TEE_KEY                       *f_pContentKeyPrime,
    __in                                      DRM_DWORD                          f_cAuxKeys,
    __in_ecount( f_cAuxKeys )           const DRM_XMRFORMAT_AUX_KEY_ENTRY       *f_prgAuxKeys,
    __in                                const DRM_XMRFORMAT_UPLINKX             *f_poUplinkX,
    __inout                                   OEM_TEE_KEY                       *f_pUplinkXKey,
    __in                                const DRM_DWORD                          f_dwUplinkXIdx )
{
    DRM_RESULT              dr = DRM_SUCCESS;
    DRM_DWORD               iKey = 0;
    DRM_DWORD               dwKeyIdx = 0;
    DRM_DWORD               dwAuxKeyIndex = 0;
    DRM_BOOL                fFound = FALSE;
    DRM_DWORD               dwLocation = 0;
    const DRM_DWORD        *pLocEntries = NULL;
    DRM_TEE_AUX_KEY_ENTRY   rgoKeyStack[] = { DRM_TEE_AUX_KEY_ENTRY_EMPTY, DRM_TEE_AUX_KEY_ENTRY_EMPTY };

    DRMCASSERT( DRM_NO_OF( rgoKeyStack ) == KEY_STACK_SIZE );

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pContentKeyPrime != NULL );
    DRMASSERT( f_prgAuxKeys != NULL );
    DRMASSERT( f_poUplinkX != NULL );
    DRMASSERT( f_pUplinkXKey != NULL );

    /* Initialize the OEM_TEE_KEYs in the key stack */
    for( iKey = 0; iKey < DRM_NO_OF( rgoKeyStack ); iKey++ )
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( f_pContext, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &rgoKeyStack[ iKey ].oKey ) );
    }

    ChkArg( f_dwUplinkXIdx < f_poUplinkX->cEntries );

    pLocEntries = XB_DWORD_LIST_TO_PDWORD( f_poUplinkX->dwlLocations );
    AssertChkBOOL( pLocEntries != NULL );
    dwLocation = pLocEntries[ f_dwUplinkXIdx ];

    while( !fFound )
    {
        const DRM_XMRFORMAT_AUX_KEY_ENTRY *pAuxKeyEntry;

        ChkDR( _FindNextAuxEntry( dwLocation, f_cAuxKeys, f_prgAuxKeys, &dwAuxKeyIndex ) );
        if( dr == DRM_S_FALSE )
        {
            /* No match. */
            DRM_DBG_TRACE( ( "Fail to derive key for location: %d", dwLocation ) );
            ChkDR( DRM_E_UNABLE_TO_RESOLVE_LOCATION_TREE );
        }
        pAuxKeyEntry = &f_prgAuxKeys[ dwAuxKeyIndex ];
        dwAuxKeyIndex++;
        NETWORKBYTES_TO_DWORD( rgoKeyStack[ 0 ].dwLocation, &pAuxKeyEntry->dwLocation, 0 );

        /* only for this condition we can derive the necessary key */
        if( IsAncestor( rgoKeyStack[ 0 ].dwLocation, dwLocation ) )
        {
            ChkDR( OEM_TEE_DECRYPT_DeriveScalableKeyWithAES128Key(
                f_pContext,
                f_pContentKeyPrime,
                pAuxKeyEntry->rgbAuxKey,
                &rgoKeyStack[ 0 ].oKey.oKey ) );

            /* two element rotating stack is initialized with a key now, top is set to second element */
            ChkDR( _TryDeriveKey( f_pContext, dwLocation, rgoKeyStack, &dwKeyIdx ) );
            ChkDR( OEM_TEE_DECRYPT_UpdateUplinkXKey( f_pContext, &rgoKeyStack[ dwKeyIdx ].oKey.oKey, f_pUplinkXKey ) );
            fFound = TRUE;
        }
    }

ErrorExit:
    for( iKey = 0; iKey < DRM_NO_OF( rgoKeyStack ); iKey++ )
    {
        ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &rgoKeyStack[ iKey ].oKey.oKey ) );
    }
    return dr;
}

static DRM_RESULT DRM_CALL _CallwlateKeys_Derived(
    __inout             OEM_TEE_CONTEXT                 *f_pContext,
    __in          const OEM_TEE_KEY                     *f_pRootContentKey,
    __in          const DRM_XMRFORMAT_AUX_KEY           *f_poAuxKeys,
    __in          const DRM_XMRFORMAT_UPLINKX           *f_poUplinkX,
    __inout             OEM_TEE_KEY                     *f_pUplinkXKey );
static DRM_RESULT DRM_CALL _CallwlateKeys_Derived(
    __inout             OEM_TEE_CONTEXT                 *f_pContext,
    __in          const OEM_TEE_KEY                     *f_pRootContentKey,
    __in          const DRM_XMRFORMAT_AUX_KEY           *f_poAuxKeys,
    __in          const DRM_XMRFORMAT_UPLINKX           *f_poUplinkX,
    __inout             OEM_TEE_KEY                     *f_pUplinkXKey )
{
    DRM_RESULT                         dr = DRM_SUCCESS;
    DRM_DWORD                          iEntry = 0;
    DRM_TEE_KEY                        oContentKeyPrime = DRM_TEE_KEY_EMPTY;
    const DRM_XMRFORMAT_AUX_KEY_ENTRY *pAuxKeys = NULL;

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pRootContentKey != NULL );
    DRMASSERT( f_poAuxKeys != NULL );
    DRMASSERT( f_poUplinkX != NULL );
    DRMASSERT( f_pUplinkXKey != NULL );

    ChkBOOL( f_poAuxKeys->cAuxKeys > 0, DRM_E_UNABLE_TO_RESOLVE_LOCATION_TREE );
    ChkBOOL( f_poUplinkX->cEntries > 0, DRM_E_UNABLE_TO_RESOLVE_LOCATION_TREE );

    pAuxKeys = (const DRM_XMRFORMAT_AUX_KEY_ENTRY *)XBBA_TO_PB( f_poAuxKeys->xbbaAuxKeys );
    ChkBOOL( pAuxKeys != NULL, DRM_E_UNABLE_TO_RESOLVE_LOCATION_TREE );

    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( f_pContext, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &oContentKeyPrime ) );

    ChkDR( OEM_TEE_DECRYPT_CallwlateContentKeyPrimeWithAES128Key( f_pContext, f_pRootContentKey, g_rgbMagicConstantZero, &oContentKeyPrime.oKey ) );

    for( iEntry = 0; iEntry < f_poUplinkX->cEntries; iEntry++ )
    {
        ChkDR( _CalcOneDerivedKey( f_pContext, &oContentKeyPrime.oKey, f_poAuxKeys->cAuxKeys, pAuxKeys, f_poUplinkX, f_pUplinkXKey, iEntry ) );
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &oContentKeyPrime.oKey ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_LICPREP_DecryptScalableLicenseKeyFromKeyMaterial(
    __inout                     DRM_TEE_CONTEXT                         *f_pContext,
    __in                  const DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER    *f_pXmrContainerKeyLeaf,
    __in                  const DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER    *f_pXmrContainerKeyRoot,
    __in_ecount( 2 )      const DRM_TEE_KEY                             *f_pEscrowedKeys,
    __out_ecount( 2 )           DRM_TEE_KEY                             *f_pLeafKeys )
{
    DRM_RESULT                  dr                                      = DRM_SUCCESS;
    DRM_TEE_KEY                 oUplinkXKey                             = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY                 oSecondaryKey                           = DRM_TEE_KEY_EMPTY;
    DRM_BYTE                    rgbSecondaryKey[ DRM_AES_KEYSIZE_128 ]; /* Do not zero-initialize key material */
    OEM_TEE_CONTEXT            *pOemTeeCtx                              = NULL;

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pXmrContainerKeyLeaf != NULL );
    DRMASSERT( f_pXmrContainerKeyRoot != NULL );
    DRMASSERT( f_pEscrowedKeys != NULL );
    DRMASSERT( f_pLeafKeys != NULL );

    DRMASSERT( f_pLeafKeys[ 0 ].eType == DRM_TEE_KEY_TYPE_PR_CI );
    DRMASSERT( f_pLeafKeys[ 1 ].eType == DRM_TEE_KEY_TYPE_PR_CK );

    pOemTeeCtx = &f_pContext->oContext;

    /*
    ** The Content Key XMR object has the encrypted secondary key at offset
    ** ECC_P256_CIPHERTEXT_SIZE_IN_BYTES of the EncryptedKeyBuffer field.
    */
    DRMASSERT( f_pXmrContainerKeyRoot->ContentKey.wKeyEncryptionCipherType == XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256_WITH_KZ );
    ChkBOOL( XBBA_TO_CB( f_pXmrContainerKeyRoot->ContentKey.xbbaEncryptedKey ) == ECC_P256_CIPHERTEXT_SIZE_IN_BYTES + DRM_AES_KEYSIZE_128, DRM_E_ILWALID_LICENSE );

    /* Initialize the uplinkx key with zeros */
    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &oUplinkXKey ) );
    ChkDR( OEM_TEE_DECRYPT_InitUplinkXKey( pOemTeeCtx, &oUplinkXKey.oKey ) );

    /* Derive the key, oUplinkXKey, based on the locations specified in the uplinkx */
    ChkDR( _CallwlateKeys_Derived( pOemTeeCtx, &f_pEscrowedKeys[ 1 ].oKey, &f_pXmrContainerKeyRoot->AuxKeys, &f_pXmrContainerKeyLeaf->UplinkX, &oUplinkXKey.oKey ) );  /* CK is always second */

    {
        DRM_BYTE *pbSecondaryKey = NULL;

        /* Get the secondary key */
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)XBBA_TO_PB( f_pXmrContainerKeyRoot->ContentKey.xbbaEncryptedKey ), ECC_P256_CIPHERTEXT_SIZE_IN_BYTES, (DRM_SIZE_T*)&pbSecondaryKey ) );
        ChkVOID( OEM_TEE_MEMCPY(
            rgbSecondaryKey,
            pbSecondaryKey,
            DRM_AES_KEYSIZE_128 ) );
    }

    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &oSecondaryKey ) );
    ChkDR( OEM_TEE_DECRYPT_DeriveScalableKeyWithAES128Key( pOemTeeCtx, &f_pEscrowedKeys[ 1 ].oKey, rgbSecondaryKey, &oSecondaryKey.oKey ) );

    /* Decrypt leaf license's content key */
    ChkBOOL( XBBA_TO_CB( f_pXmrContainerKeyLeaf->ContentKey.xbbaEncryptedKey ) == 2 * DRM_AES_KEYSIZE_128, DRM_E_ILWALID_LICENSE );
    ChkDR( OEM_TEE_DECRYPT_DecryptContentKeysWithDerivedKeys(
        pOemTeeCtx,
        &oSecondaryKey.oKey,
        &oUplinkXKey.oKey,
        XBBA_TO_PB( f_pXmrContainerKeyLeaf->ContentKey.xbbaEncryptedKey ),
        &f_pLeafKeys[ 0 ].oKey,
        &f_pLeafKeys[ 1 ].oKey ) );
    DRMASSERT( f_pLeafKeys[ 0 ].eType == DRM_TEE_KEY_TYPE_PR_CI );
    DRMASSERT( f_pLeafKeys[ 1 ].eType == DRM_TEE_KEY_TYPE_PR_CK );

ErrorExit:

    if( pOemTeeCtx != NULL )
    {
        ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oUplinkXKey.oKey ) );
        ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oSecondaryKey.oKey ) );
    }

    ChkVOID( OEM_TEE_ZERO_MEMORY( rgbSecondaryKey, sizeof( rgbSecondaryKey ) ) );

    return dr;
}
PRAGMA_DIAG_ON( attributes )

#if OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE > 0
typedef struct __tagDRM_TEE_LICPREP_OPTIMIZED_CONTENT_KEY2_CACHE_ENTRY
{
    DRM_DWORD   dwAge;
    DRM_ID      idCacheKey;
    DRM_BYTE    rgbCacheKey[ DRM_AES_BLOCKLEN ];
} DRM_TEE_LICPREP_OPTIMIZED_CONTENT_KEY2_CACHE_ENTRY;

typedef DRM_TEE_LICPREP_OPTIMIZED_CONTENT_KEY2_CACHE_ENTRY OPTCK2CE;

#define DRM_TEE_LICPREP_OPTIMIZED_CONTENT_KEY2_CACHE_ENTRY_EMPTY { 0, DRM_ID_EMPTY, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

DRM_TEE_LICPREP_OPTIMIZED_CONTENT_KEY2_CACHE_ENTRY rgbOptCK2Cache[ OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE ] = { DRM_TEE_LICPREP_OPTIMIZED_CONTENT_KEY2_CACHE_ENTRY_EMPTY };

static DRM_RESULT DRM_CALL _UseOptimizedContentKey2(
    __inout           OEM_TEE_CONTEXT                       *f_pOemTeeCtx,
    __in        const OEM_TEE_KEY                           *f_pPrivateEncryptionKeyWeakRef,
    __in        const DRM_XMRFORMAT_OPTIMIZED_CONTENT_KEY2  *f_pOptimizedContentKey2WeakRef,
    __inout           OEM_TEE_KEY                           *f_pFirstECCDecryptOutput,
    __inout           OEM_TEE_KEY                           *f_pSecondECCDecryptOutput,
    __out             DRM_BOOL                              *f_pfUsedOptimizedContentKey2 )
{
    DRM_RESULT       dr                                 = DRM_SUCCESS;
    DRM_TEE_KEY      rgoTemporaryKeys[ 3 ]              = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    DRM_BYTE         rgbTemp[ DRM_AES_BLOCKLEN * 2 ];   /* copied into */
    DRM_BYTE        *pbTemp0                            = rgbTemp;
    DRM_BYTE        *pbTemp1                            = &rgbTemp[ DRM_AES_BLOCKLEN ];
    DRM_DWORD        idx;                               /* loop var */
    OPTCK2CE        *pOldestEntry                       = NULL;
    OPTCK2CE        *pLwrrentEntry                      = NULL;
    const OPTCK2CE  *pNewestEntry                       = NULL;

    *f_pfUsedOptimizedContentKey2 = FALSE;

    if( !f_pOptimizedContentKey2WeakRef->fValid
     || f_pOptimizedContentKey2WeakRef->wCacheKeyEncryptionAlgorithm != XMR_OPTIMIZED_CONTENT_KEY2_CACHE_KEY_ENCRYPTION_ALGORITHM_ECC256_XOR_OMAC1
     || f_pOptimizedContentKey2WeakRef->wOriginalEccOutputEncryptionAlgorithm != XMR_OPTIMIZED_CONTENT_KEY2_ORIGINAL_ECC_OUTPUT_ENCRYPTION_ALGORITHM_ECB )
    {
        /* No object present or object has unrecognized algorithm.  Ignore. */
        goto ErrorExit;
    }

    /* Verify buffer sizes */
    ChkBOOL( XBBA_TO_CB( f_pOptimizedContentKey2WeakRef->xbbaCacheKey ) == ECC_P256_CIPHERTEXT_SIZE_IN_BYTES, DRM_E_ILWALID_LICENSE );
    ChkBOOL( XBBA_TO_CB( f_pOptimizedContentKey2WeakRef->xbbaOriginalEccOutput ) == ECC_P256_PLAINTEXT_SIZE_IN_BYTES, DRM_E_ILWALID_LICENSE );

    /* Search cache */
    pOldestEntry = &rgbOptCK2Cache[ 0 ];
    pNewestEntry = &rgbOptCK2Cache[ 0 ];
    for( idx = 0; idx < OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE; idx++ )
    {
        if( rgbOptCK2Cache[ idx ].dwAge < pOldestEntry->dwAge )
        {
            pOldestEntry = &rgbOptCK2Cache[ idx ];
        }
        if( rgbOptCK2Cache[ idx ].dwAge > pNewestEntry->dwAge )
        {
            pNewestEntry = &rgbOptCK2Cache[ idx ];
        }
        if( rgbOptCK2Cache[ idx ].dwAge != 0 && OEM_SELWRE_ARE_EQUAL( &rgbOptCK2Cache[ idx ].idCacheKey, &f_pOptimizedContentKey2WeakRef->idCacheKey, sizeof( DRM_ID ) ) )
        {
            /* Cache hit! */
            pLwrrentEntry = &rgbOptCK2Cache[ idx ];
        }
    }

    if( pLwrrentEntry == NULL )
    {
        /* Cache miss! */

        DRM_BYTE         rgbToHash0[ DRM_AES_BLOCKLEN ] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        DRM_BYTE         rgbToHash1[ DRM_AES_BLOCKLEN ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        CIPHERTEXT_P256  oOptCK2CipherText;             /* copied into */

        /* Per algorithm: ECC decrypt two keys, use them to each hash a different value, and XOR the results together - this yields the Cache Key */

        ChkVOID( OEM_TEE_MEMCPY( &oOptCK2CipherText, XBBA_TO_PB( f_pOptimizedContentKey2WeakRef->xbbaCacheKey ), ECC_P256_CIPHERTEXT_SIZE_IN_BYTES ) );

        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( f_pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &rgoTemporaryKeys[ 0 ] ) );
        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( f_pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &rgoTemporaryKeys[ 1 ] ) );

        ChkDR( OEM_TEE_BASE_ECC256_Decrypt_AES128Keys(
            f_pOemTeeCtx,
            f_pPrivateEncryptionKeyWeakRef,
            &oOptCK2CipherText,
            &rgoTemporaryKeys[ 0 ].oKey,
            &rgoTemporaryKeys[ 1 ].oKey ) );

        ChkDR( OEM_TEE_BASE_SignWithOMAC1(
            f_pOemTeeCtx,
            &rgoTemporaryKeys[ 0 ].oKey,
            sizeof( rgbToHash0 ),
            rgbToHash0,
            pbTemp0 ) );
        ChkDR( OEM_TEE_BASE_SignWithOMAC1(
            f_pOemTeeCtx,
            &rgoTemporaryKeys[ 1 ].oKey,
            sizeof( rgbToHash1 ),
            rgbToHash1,
            pbTemp1 ) );

        ChkVOID( DRM_XOR( pbTemp0, pbTemp1, DRM_AES_BLOCKLEN ) );

        /* Overwrite the oldest cache entry's data */

        pLwrrentEntry = pOldestEntry;
        ChkVOID( OEM_TEE_MEMCPY( &pLwrrentEntry->idCacheKey, &f_pOptimizedContentKey2WeakRef->idCacheKey, sizeof( pLwrrentEntry->idCacheKey ) ) );
        ChkVOID( OEM_TEE_MEMCPY( pLwrrentEntry->rgbCacheKey, pbTemp0, sizeof( pLwrrentEntry->rgbCacheKey ) ) );
    }

    /* pLwrrentEntry now has the correct Cache Key regardless of whether we had a cache hit or a cache miss.  It should therefore now have the newest age. */
    pLwrrentEntry->dwAge = pNewestEntry->dwAge + 1;

    /*
    ** Per algorithm: AES ECB decrypt with Cache Key
    ** This will yield the same output that standard ECC decrypt would have
    ** yielded on a machine where OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE == 0
    */
    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( f_pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &rgoTemporaryKeys[ 2 ] ) );
    ChkDR( OEM_TEE_BASE_AES128_SetKey( f_pOemTeeCtx, &rgoTemporaryKeys[ 2 ].oKey, pLwrrentEntry->rgbCacheKey ) );

    ChkVOID( OEM_TEE_MEMCPY( rgbTemp, XBBA_TO_PB( f_pOptimizedContentKey2WeakRef->xbbaOriginalEccOutput ), ECC_P256_PLAINTEXT_SIZE_IN_BYTES ) );

    ChkDR( OEM_TEE_BASE_AES128ECB_DecryptData( f_pOemTeeCtx, &rgoTemporaryKeys[ 2 ].oKey, sizeof( rgbTemp ), rgbTemp ) );

    /* Final output */
    ChkDR( OEM_TEE_BASE_AES128_SetKey( f_pOemTeeCtx, f_pFirstECCDecryptOutput, pbTemp0 ) );
    ChkDR( OEM_TEE_BASE_AES128_SetKey( f_pOemTeeCtx, f_pSecondECCDecryptOutput, pbTemp1 ) );

    *f_pfUsedOptimizedContentKey2 = TRUE;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pOemTeeCtx, &rgoTemporaryKeys[ 0 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pOemTeeCtx, &rgoTemporaryKeys[ 1 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pOemTeeCtx, &rgoTemporaryKeys[ 2 ].oKey ) );
    return dr;
}
#endif  /* OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE > 0 */

/*
** Synopsis:
**
** This function takes a license which is bound to the private device
** encryption key from the PlayReady leaf certificate Or is bound to
** the private domain encryption key from a domain certificate
** (which, in turn, is bound to the private device encryption
** key from the PlayReady leaf certificate) and symmetrically
** re-encrypts it to a TEE-specific key thus allowing the license to
** be used in the future without any asymmetric crypto operations.
**
** This function is called inside:
**  Drm_LicenseAcq_ProcessResponse
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Parse the given PlayReady Private Key Blob (PPKB) and verify its signature.
**  3. Parse the given XMR license while ignoring any XMR objects which are
**     unnecessary for the remainder of this function.
**  4. If the OEM supports a clock inside the TEE, validate that the license
**     has not passed its end date.
**  5. If the license is a leaf license, halt and return an empty LKB.
**  6. If a Domain key Blob (DKB) was given, verify the license is domain-bound.
**  7. If the license is in-memory only, verify that the license ID is bound to
**     a Nonce from the provided NKB.
**  8. If a DKB was given, parse the given DKB and verify its signature.
**  9. Decrypt the license content keys (CI/CK) with the private key
**     associated with the public key to which the license is bound
**     (either the domain private key in the given DKB or the private
**     encryption key in the given PPKB).
** 10. Verify the license signature using CI.
** 11. Build and sign a License Key Blob (LKB).
** 12. Return the new LKB.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                DRM_TEE_BASE_AllocTEEContext.
** f_pPPKB               (in)     The current PlayReady Private Key Blob (PPKB).
** f_pLicense:           (in)     The raw binary XMR license.
**                                If the license is domain-bound and domains are not
**                                supported, this function will return DRM_E_NOTIMPL.
** f_pDKB:               (in)     The Domain Key Blob (DKB) to which the XMR license
**                                is bound, if any
**                                (i.e. one of the output values from
**                                DRM_TEE_DOM_PackageKeys).
**                                This parameter must have cb==0 for a license
**                                which is not domain-bound.
** f_pNKB:               (in/out) The Nonces Key Blob (NKB) which holds a list of valid
**                                nonces bound to the current session. To be used to verify
**                                the acquired in-memory only license.
** f_ppLKB:              (out)    The License Key Blob (LKB) for the given license
**                                ready for storage to disk OR ready to be held
**                                in memory for decryption for licenses that
**                                cannot be persisted.
**                                This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_LICPREP_PackageKey(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in            const DRM_TEE_BYTE_BLOB            *f_pLicense,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pDKB,
    __in_tee_opt    const DRM_TEE_BYTE_BLOB            *f_pNKB,
    __out                 DRM_TEE_BYTE_BLOB            *f_pLKB )
{
    DRM_RESULT                   dr                     = DRM_SUCCESS;
    DRM_RESULT                   drTmp                  = DRM_SUCCESS;
    DRM_DWORD                    cKeys                  = 0;
    DRM_TEE_KEY                 *pKeys                  = NULL;
    DRM_TEE_XMR_LICENSE         *pXmrLicense            = NULL;
    DRM_BOOL                     fPersist               = FALSE;
    DRM_BOOL                     fScalable              = FALSE;
    DRM_TEE_KEY                  rgoContentKeys[ 2 ]    = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    DRM_TEE_KEY                 *pContentKeys           = NULL;
    DRM_DWORD                    cContentKeys           = 0;
    DRM_TEE_BYTE_BLOB            oLKB                   = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT             *pOemTeeCtx             = NULL;
    DRM_TEE_KEY                  oTKD                   = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY                 *pDomainKey             = NULL;
    DRM_TEE_KEY                  rgoEccOutputKeys[ 2 ]  = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    DRM_BOOL                     fSelwreStop2           = FALSE;
    DRM_ID                       idNonce                = DRM_ID_EMPTY;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pPPKB    != NULL );
    DRMASSERT( f_pLicense != NULL );
    DRMASSERT( f_pDKB     != NULL );
    DRMASSERT( f_pLKB     != NULL );

    DRMCASSERT( DRM_AES_BLOCKLEN == DRM_AES128OMAC1_SIZE_IN_BYTES );
    DRMCASSERT( DRM_AES_BLOCKLEN * 2 == ECC_P256_PLAINTEXT_SIZE_IN_BYTES );

    pOemTeeCtx = &f_pContext->oContext;

    /* Ensure that the Secure clock did not go out of sync */
    if( OEM_TEE_CLOCK_SelwreClockNeedsReSync( pOemTeeCtx ) )
    {
        ChkDR( DRM_E_TEE_CLOCK_DRIFTED );
    }

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pLKB, sizeof(*f_pLKB) ) );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof( *pXmrLicense ), (DRM_VOID**)&pXmrLicense ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pXmrLicense, sizeof( *pXmrLicense ) ) );

    ChkDR( DRM_TEE_IMPL_BASE_ParseLicenseAlloc( f_pContext, f_pLicense->cb, f_pLicense->pb, pXmrLicense ) );

    ChkBOOL( pXmrLicense->oLicense.fValid, DRM_E_ILWALID_LICENSE );

    ChkArg( XMRFORMAT_IS_DOMAIN_ID_VALID( &pXmrLicense->oLicense ) == IsBlobAssigned( f_pDKB ) );

    /*
    ** Note: We allow storage of a license with a begin date in the future as it will become valid at a later point in time.
    ** Thus, we explicitly only validate the license end date in this function.
    ** During Bind, we will validate both begin an end date inside the TEE.
    */
    ChkDR( DRM_TEE_IMPL_BASE_ValidateLicenseExpiration( f_pContext, &pXmrLicense->oLicense, FALSE, TRUE ) );

    fPersist = !XMRFORMAT_IS_CANNOT_PERSIST_LICENSE( &pXmrLicense->oLicense );
    fSelwreStop2 = XMRFORMAT_IS_SELWRESTOP2_VALID( &pXmrLicense->oLicense );

    if( fPersist )
    {
        ChkBOOL( !fSelwreStop2, DRM_E_ILWALID_LICENSE );
    }
    else
    {
        ChkDR( _ParseAndVerifyInMemoryLicenseNonce( f_pContext, f_pNKB, &pXmrLicense->oLicense.HeaderData.LID, &idNonce ) );
    }

    /* There is no need to generate an LKB for leaf licenses */
    if( !XMRFORMAT_IS_LEAF_LICENSE( &pXmrLicense->oLicense ) )
    {
        const DRM_BYTE                *pbCipherText                         = NULL;
        DRM_DWORD                      cbCipherText                         = 0;
        DRM_TEE_KEY_TYPE               eType                                = DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT;
        const DRM_TEE_KEY             *pPrivateEncryptionKeyWeakRef         = NULL;
        const DRM_XMRFORMAT_DOMAIN_ID *pdomainID                            = &pXmrLicense->oLicense.OuterContainer.GlobalPolicyContainer.DomainID;
        const DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER *pKeyMaterialContainer   = &pXmrLicense->oLicense.OuterContainer.KeyMaterialContainer;

        fScalable = XMRFORMAT_IS_AUX_KEY_VALID( &pXmrLicense->oLicense );

        ChkBOOL( XMRFORMAT_IS_CONTENT_KEY_VALID( &pXmrLicense->oLicense ), DRM_E_ILWALID_LICENSE );     /* Also guarantees that pKeyMaterialContainer is valid */

        pbCipherText = XBBA_TO_PB( pKeyMaterialContainer->ContentKey.xbbaEncryptedKey );
        AssertChkBOOL( pbCipherText != NULL );
        cbCipherText = XBBA_TO_CB( pKeyMaterialContainer->ContentKey.xbbaEncryptedKey );
        AssertChkBOOL( cbCipherText > 0 );

        if( pKeyMaterialContainer->ContentKey.wKeyEncryptionCipherType == XMR_ASYMMETRIC_ENCRYPTION_TYPE_TEE_TRANSIENT )
        {
            DRM_ID idSelwreStopSession = DRM_ID_EMPTY;
            ChkBOOL( DRM_TEE_IMPL_LICGEN_IsLICGENSupported(), DRM_E_ILWALID_LICENSE );

            ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, cbCipherText, pbCipherText, &oLKB ) );

            ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyLKB( f_pContext, &oLKB, &idSelwreStopSession, &cContentKeys, &pContentKeys ) );
            ChkBOOL( cContentKeys == 2, DRM_E_TEE_ILWALID_KEY_DATA );  /* CI/CK */

            ChkDR( DRM_TEE_IMPL_BASE_CheckLicenseSignature( f_pContext, &pXmrLicense->oLicense, pContentKeys ) );
        }
        else
        {
            CIPHERTEXT_P256  oCipherText; /* Initialized later on copy */
            DRM_BOOL         fViaSymmetric = FALSE;
            DRM_TEE_KEY     *pFirstECCDecryptOutput  = NULL;
            DRM_TEE_KEY     *pSecondECCDecryptOutput = NULL;

            switch( pKeyMaterialContainer->ContentKey.wKeyEncryptionCipherType )
            {
            case XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256:
                ChkBOOL( !fScalable, DRM_E_ILWALID_LICENSE );
                ChkBOOL( cbCipherText == sizeof( CIPHERTEXT_P256 ), DRM_E_ILWALID_LICENSE );
                break;
            case XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256_WITH_KZ:
                ChkBOOL( fScalable, DRM_E_ILWALID_LICENSE );
                ChkBOOL( cbCipherText == sizeof( CIPHERTEXT_P256 ) + DRM_AES_KEYSIZE_128, DRM_E_ILWALID_LICENSE );
                break;
            case XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256_VIA_SYMMETRIC:
                ChkBOOL( fScalable, DRM_E_ILWALID_LICENSE );
                ChkBOOL( cbCipherText == sizeof( CIPHERTEXT_P256 ) + 3 * DRM_AES_KEYSIZE_128, DRM_E_ILWALID_LICENSE );
                ChkBOOL( pKeyMaterialContainer->ContentKey.wSymmetricCipherType > 0, DRM_E_ILWALID_LICENSE );
                ChkBOOL( pKeyMaterialContainer->ContentKey.wSymmetricCipherType <= DRM_MAX_UNSIGNED_TYPE( DRM_BYTE ), DRM_E_ILWALID_LICENSE );
                fViaSymmetric = TRUE;
                break;
            default:
                ChkDR( DRM_E_ILWALID_LICENSE );
            }

            if( IsBlobAssigned( f_pDKB ) )
            {
                /*
                ** If a DKB was given, parse the given DKB and verify its signature.
                ** The PrivateEncryptionKey in this case should be the domain key
                */
                ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyDKB(
                   f_pContext,
                   f_pDKB,
                   pdomainID->dwRevision,
                   &pDomainKey ) );

                pPrivateEncryptionKeyWeakRef = pDomainKey;
            }
            else
            {
                ChkArg( IsBlobAssigned( f_pPPKB ) );
                ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, NULL ) );
                pPrivateEncryptionKeyWeakRef = DRM_TEE_IMPL_BASE_LocateKeyInPPKBWeakRef( &eType, cKeys, pKeys );
            }

            ChkVOID( OEM_TEE_MEMCPY( &oCipherText, pbCipherText, ECC_P256_CIPHERTEXT_SIZE_IN_BYTES ) );

            ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CI, &rgoContentKeys[ 0 ] ) );
            ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &rgoContentKeys[ 1 ] ) );

            if( fViaSymmetric )
            {
                /*
                ** Simple license using embedded scalable crypto.
                */
                ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CI, &rgoEccOutputKeys[ 0 ] ) );
                ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &rgoEccOutputKeys[ 1 ] ) );
                pFirstECCDecryptOutput = &rgoEccOutputKeys[ 0 ];
                pSecondECCDecryptOutput = &rgoEccOutputKeys[ 1 ];
            }
            else
            {
                pFirstECCDecryptOutput = &rgoContentKeys[ 0 ];
                pSecondECCDecryptOutput = &rgoContentKeys[ 1 ];
            }

            {
#if OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE > 0
                DRM_BOOL fUsedOptimizedContentKey2  = FALSE;

                ChkDR( _UseOptimizedContentKey2(
                    pOemTeeCtx,
                    &pPrivateEncryptionKeyWeakRef->oKey,
                    &pKeyMaterialContainer->OptimizedContentKey2,
                    &pFirstECCDecryptOutput->oKey,
                    &pSecondECCDecryptOutput->oKey,
                    &fUsedOptimizedContentKey2 ) );

                if( !fUsedOptimizedContentKey2 )
#endif  /* OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE > 0 */
                {
                    ChkDR( OEM_TEE_BASE_ECC256_Decrypt_AES128Keys(
                        pOemTeeCtx,
                        &pPrivateEncryptionKeyWeakRef->oKey,
                        &oCipherText,
                        &pFirstECCDecryptOutput->oKey,
                        &pSecondECCDecryptOutput->oKey ) );
                }
            }

            if( fScalable )
            {
                ChkDR( OEM_TEE_DECRYPT_UnshuffleScalableContentKeys( pOemTeeCtx, &pFirstECCDecryptOutput->oKey, &pSecondECCDecryptOutput->oKey ) );

                if( fViaSymmetric )
                {
                    /*
                    ** Simple license using embedded scalable crypto.
                    */

                    DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER oXmrContainerKeyEmbeddedLeaf = { 0 };
                    DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER oXmrContainerKeyEmbeddedRoot = { 0 };
                    static const DRM_DWORD rgdwRootLocations[] = { 1 };    /* Only one location exists */

                    /* The 'leaf' key info comes from the last 32 bytes of the key material buffer (2 encrypted AES keys) */
                    oXmrContainerKeyEmbeddedLeaf.fValid = TRUE;

                    oXmrContainerKeyEmbeddedLeaf.ContentKey.fValid = TRUE;
                    oXmrContainerKeyEmbeddedLeaf.ContentKey.xbbaEncryptedKey.fValid = TRUE;
                    oXmrContainerKeyEmbeddedLeaf.ContentKey.xbbaEncryptedKey.cbData = 2 * DRM_AES_KEYSIZE_128;
                    oXmrContainerKeyEmbeddedLeaf.ContentKey.xbbaEncryptedKey.pbDataBuffer = (DRM_BYTE*)&pbCipherText[ ECC_P256_CIPHERTEXT_SIZE_IN_BYTES + DRM_AES_KEYSIZE_128 ];

                    /*
                    ** The license doesn't have an uplinkX because it's a simple license.
                    ** However, the location data is well-known and constant.
                    */
                    oXmrContainerKeyEmbeddedLeaf.UplinkX.fValid = TRUE;
                    oXmrContainerKeyEmbeddedLeaf.UplinkX.cEntries = 1;
                    oXmrContainerKeyEmbeddedLeaf.UplinkX.dwlLocations.fValid = TRUE;
                    oXmrContainerKeyEmbeddedLeaf.UplinkX.dwlLocations.cDWORDs = 1;
                    oXmrContainerKeyEmbeddedLeaf.UplinkX.dwlLocations.pdwordBuffer = (DRM_BYTE*)rgdwRootLocations;

                    /*
                    ** The 'root' key info comes from the first 140 bytes of the key material buffer (1 ECC ciphertext and 1 encrypted AES key)
                    */
                    oXmrContainerKeyEmbeddedRoot.fValid = TRUE;

                    oXmrContainerKeyEmbeddedRoot.ContentKey.fValid = TRUE;
                    oXmrContainerKeyEmbeddedRoot.ContentKey.wKeyEncryptionCipherType = XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256_WITH_KZ;
                    oXmrContainerKeyEmbeddedRoot.ContentKey.xbbaEncryptedKey.fValid = TRUE;
                    oXmrContainerKeyEmbeddedRoot.ContentKey.xbbaEncryptedKey.cbData = ECC_P256_CIPHERTEXT_SIZE_IN_BYTES + DRM_AES_KEYSIZE_128;
                    oXmrContainerKeyEmbeddedRoot.ContentKey.xbbaEncryptedKey.pbDataBuffer = (DRM_BYTE*)pbCipherText;

                    /*
                    ** The license has the required aux key data.
                    ** This is a struct copy which copies pointers
                    ** This is safe because the callee does not modify oXmrContainerKeyEmbeddedRoot.
                    */
                    oXmrContainerKeyEmbeddedRoot.AuxKeys = pKeyMaterialContainer->AuxKeys;

                    /*
                    ** Now we can simply reuse the existing scalable license decryption
                    ** algorithm that's normally used for scalable license chains.
                    */
                    ChkDR( DRM_TEE_IMPL_LICPREP_DecryptScalableLicenseKeyFromKeyMaterial(
                        f_pContext,
                        &oXmrContainerKeyEmbeddedLeaf,
                        &oXmrContainerKeyEmbeddedRoot,
                        rgoEccOutputKeys,
                        rgoContentKeys ) );
                }
            }

            ChkDR( DRM_TEE_IMPL_BASE_CheckLicenseSignature( f_pContext, &pXmrLicense->oLicense, rgoContentKeys ) );

            ChkDR( DRM_TEE_IMPL_KB_BuildLKB( f_pContext, fPersist, &idNonce, rgoContentKeys, &oLKB ) );
        }
    }

    if( fSelwreStop2 )
    {
        ChkDR( DRM_TEE_IMPL_SELWRESTOP2_StartDecryption( pOemTeeCtx, &pXmrLicense->oLicense.HeaderData.LID, &idNonce ) );
    }

    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pLKB, &oLKB ) );

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cContentKeys, &pContentKeys ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, 1, &pDomainKey ) );

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oLKB );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( DRM_TEE_IMPL_BASE_ParsedLicenseFree( pOemTeeCtx, pXmrLicense ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pXmrLicense ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oTKD.oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoContentKeys[0].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoContentKeys[1].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoEccOutputKeys[ 0 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoEccOutputKeys[ 1 ].oKey ) );
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

