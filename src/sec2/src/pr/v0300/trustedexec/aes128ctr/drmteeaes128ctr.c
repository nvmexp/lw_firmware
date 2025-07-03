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
#include <drmteexb.h>
#include <drmteekbcryptodata.h>
#include <drmteecache.h>
#include <drmerr.h>
#include <oemtee.h>
#include <oemparsers.h>
#include <oembyteorder.h>
#include <drmxmrformatparser.h>
#include <drmderivedkey.h>
#include <drmbcertconstants.h>
#include <drmteeaes128ctrinternal.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_AES128CTR_IMPL_IsAES128CTRSupported( DRM_VOID )
{
    return TRUE;
}

DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerUnknownOutput, 0x786627D8, 0xC2A6, 0x44BE, 0x8F, 0x88, 0x08, 0xAE, 0x25, 0x5B, 0x01, 0xA7 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerUnknownOutputConstrained, 0xB621D91F, 0xEDCC, 0x4035, 0x8D, 0x4B, 0xDC, 0x71, 0x76, 0x0D, 0x43, 0xE9 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerDTCP, 0xD685030B, 0x0F4F, 0x43A6, 0xBB, 0xAD, 0x35, 0x6F, 0x1E, 0xA0, 0x04, 0x9A );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerHelix, 0x002F9772, 0x38A0, 0x43E5, 0x9F, 0x79, 0x0F, 0x63, 0x61, 0xDC, 0xC6, 0x2A );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerHDCPMiracast, 0xA340C256, 0x0941, 0x4D4C, 0xAD, 0x1D, 0x0B, 0x67, 0x35, 0xC0, 0xCB, 0x24 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerHDCPWiVu, 0x1B4542E3, 0xB5CF, 0x4C99, 0xB3, 0xBA, 0x82, 0x9A, 0xF4, 0x6C, 0x92, 0xF8 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerAirPlay, 0x5ABF0F0D, 0xDC29, 0x4B82, 0x99, 0x82, 0xFD, 0x8E, 0x57, 0x52, 0x5B, 0xFC );

static DRM_BYTE g_rgbMagicConstantLeft[DRM_AES_KEYSIZE_128] PR_ATTR_DATA_OVLY(_g_rgbMagicConstantLeft) = {
    0xf0, 0x61, 0x4d, 0xb6, 0x0f, 0xca, 0x3f, 0xbb,
    0x0f, 0x0d, 0x20, 0xf5, 0x61, 0xad, 0xec, 0x81 };

static DRM_BYTE g_rgbMagicConstantRight[DRM_AES_KEYSIZE_128] PR_ATTR_DATA_OVLY(_g_rgbMagicConstantRight) = {
    0x3e, 0xf4, 0x88, 0xf3, 0x37, 0xe2, 0xda, 0xc6,
    0x62, 0x14, 0x9a, 0xdb, 0x82, 0x7e, 0x97, 0x6d };

static DRM_BYTE g_rgbMagicConstantZero[DRM_AES_KEYSIZE_128] PR_ATTR_DATA_OVLY(_g_rgbMagicConstantZero) = {
    0x7e, 0xe9, 0xed, 0x4a, 0xf7, 0x73, 0x22, 0x4f,
    0x00, 0xb8, 0xea, 0x7e, 0xfb, 0x02, 0x7c, 0xbb };

typedef DRM_OBFUS_FIXED_ALIGN struct __tagDRM_TEE_AUX_KEY_ENTRY
{
    DRM_DWORD    dwLocation;
    DRM_TEE_KEY  oKey;
} DRM_TEE_AUX_KEY_ENTRY;

#define DRM_TEE_AUX_KEY_ENTRY_EMPTY { 0, DRM_TEE_KEY_EMPTY }

#define KEY_STACK_SIZE 2
PRAGMA_DIAG_OFF(attributes, "Turning warning off for GCC because it thinks these functions might not be able to be inlined for free builds..")
static DRM_RESULT _TryDeriveKey(
    __inout                             OEM_TEE_CONTEXT       *f_pContext,
    __in                          const DRM_DWORD              f_dwLocation,
    __inout_ecount( KEY_STACK_SIZE )    DRM_TEE_AUX_KEY_ENTRY *f_poKeyStack,
    __inout                             DRM_DWORD             *f_pdwLwrrentKeyIdx );
static DRM_RESULT _TryDeriveKey(
    __inout                             OEM_TEE_CONTEXT       *f_pContext,
    __in                          const DRM_DWORD              f_dwLocation,
    __inout_ecount( KEY_STACK_SIZE )    DRM_TEE_AUX_KEY_ENTRY *f_poKeyStack,
    __inout                             DRM_DWORD             *f_pdwLwrrentKeyIdx )
{
    DRM_RESULT   dr = DRM_SUCCESS;
    DRM_DWORD    dwSBMParent = 0;
    DRM_BOOL     fDeriveRight = FALSE;
    OEM_TEE_KEY *pScalableKey = NULL;
    OEM_TEE_KEY *pDerivedScalableKey = NULL;
    DRM_DWORD    dwDepth = 0;

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_poKeyStack != NULL );
    DRMASSERT( f_pdwLwrrentKeyIdx != NULL );
    DRMASSERT( *f_pdwLwrrentKeyIdx == 0 );

    while( f_poKeyStack[*f_pdwLwrrentKeyIdx ].dwLocation != f_dwLocation )
    {
        dwSBMParent = SBM( f_poKeyStack[*f_pdwLwrrentKeyIdx ].dwLocation );

        fDeriveRight = (f_dwLocation & dwSBMParent) != 0;
        pScalableKey = &f_poKeyStack[*f_pdwLwrrentKeyIdx ].oKey.oKey;
        pDerivedScalableKey = &f_poKeyStack[ *f_pdwLwrrentKeyIdx ^ 1 ].oKey.oKey;

        ChkDR( OEM_TEE_AES128CTR_DeriveScalableKeyWithAES128Key( f_pContext, pScalableKey, (fDeriveRight ? g_rgbMagicConstantRight : g_rgbMagicConstantLeft), pDerivedScalableKey ) );

        f_poKeyStack[*f_pdwLwrrentKeyIdx^1].dwLocation =
            (((f_poKeyStack[*f_pdwLwrrentKeyIdx].dwLocation & (~dwSBMParent)) |
            (f_dwLocation & dwSBMParent)) |
            (dwSBMParent >> 1));

        (*f_pdwLwrrentKeyIdx) ^= 1;

        dwDepth++;
        ChkBOOL( dwDepth < MAX_KEY_STACK, DRM_E_UNABLE_TO_RESOLVE_LOCATION_TREE );
    }

ErrorExit:
    return dr;
}

static DRM_RESULT _FindNextAuxEntry(
    __in                            DRM_DWORD                      f_dwLocation,
    __in                      const DRM_DWORD                      f_cAuxKeys,
    __in_ecount( f_cAuxKeys ) const DRM_XMRFORMAT_AUX_KEY_ENTRY   *f_prgAuxKeys,
    __inout                         DRM_DWORD                     *f_pdwAuxKeyIndex );
static DRM_RESULT _FindNextAuxEntry(
    __in                            DRM_DWORD                      f_dwLocation,
    __in                      const DRM_DWORD                      f_cAuxKeys,
    __in_ecount( f_cAuxKeys ) const DRM_XMRFORMAT_AUX_KEY_ENTRY   *f_prgAuxKeys,
    __inout                         DRM_DWORD                     *f_pdwAuxKeyIndex )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BOOL   fDone = FALSE;
    DRM_LONG   nLeft, nRight, nMiddle, nStart;  /* Initialized before usage below */
    DRM_DWORD  dwLocationToTry;                 /* Initialized before usage below */

    DRMASSERT( f_prgAuxKeys != NULL );
    DRMASSERT( f_pdwAuxKeyIndex != NULL );

    if( f_cAuxKeys == 0
        || *f_pdwAuxKeyIndex >= f_cAuxKeys )
    {
        dr = DRM_S_FALSE;
    }
    else
    {
        nStart = nLeft = (DRM_LONG)*f_pdwAuxKeyIndex;

        /* Can't underflow because we get here only if f_cAuxKeys != 0 */
        nRight = (DRM_LONG)(f_cAuxKeys - 1);

        while( !fDone )
        {
            nMiddle = (nLeft + nRight) / 2;

            NETWORKBYTES_TO_DWORD( dwLocationToTry, &f_prgAuxKeys[nMiddle].dwLocation, 0 );
            if( IsAncestor( dwLocationToTry, f_dwLocation ) )
            {
                *f_pdwAuxKeyIndex = (DRM_DWORD)nMiddle;
                fDone = TRUE;   /* Found the index */
            }
            else
            {
                if( dwLocationToTry > f_dwLocation )
                {
                    nRight = nMiddle - 1;
                }
                else
                {
                    nLeft = nMiddle + 1;
                }

                if( nLeft >= (DRM_LONG)f_cAuxKeys
                    || nRight <  nStart
                    || nLeft  >  nRight )
                {
                    dr = DRM_S_FALSE;
                    fDone = TRUE;   /* Index not be found */
                }
            }
        }
    }

    return dr;
}

static DRM_RESULT _CalcOneDerivedKey(
    __inout                                   OEM_TEE_CONTEXT                   *f_pContext,
    __in                                const OEM_TEE_KEY                       *f_pContentKeyPrime,
    __in                                      DRM_DWORD                          f_cAuxKeys,
    __in_ecount( f_cAuxKeys )           const DRM_XMRFORMAT_AUX_KEY_ENTRY       *f_prgAuxKeys,
    __in                                const DRM_XMRFORMAT_UPLINKX             *f_poUplinkX,
    __inout                                   OEM_TEE_KEY                       *f_pUplinkXKey,
    __in                                const DRM_DWORD                          f_dwUplinkXIdx );
static DRM_RESULT _CalcOneDerivedKey(
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
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( f_pContext, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &rgoKeyStack[ iKey ].oKey ) );
    }

    ChkArg( f_dwUplinkXIdx < f_poUplinkX->cEntries );

    pLocEntries = XB_DWORD_LIST_TO_PDWORD( f_poUplinkX->dwlLocations );
    AssertChkArg( pLocEntries != NULL );
    dwLocation = pLocEntries[f_dwUplinkXIdx];

    while( !fFound )
    {
        const DRM_XMRFORMAT_AUX_KEY_ENTRY *pAuxKeyEntry;

        ChkDR( _FindNextAuxEntry( dwLocation, f_cAuxKeys, f_prgAuxKeys, &dwAuxKeyIndex ) );
        if( dr == DRM_S_FALSE )
        {
            /* No match. */
            TRACE( ("Fail to derive key for location: %d", dwLocation) );
            ChkDR( DRM_E_UNABLE_TO_RESOLVE_LOCATION_TREE );
        }
        pAuxKeyEntry = &f_prgAuxKeys[dwAuxKeyIndex];
        dwAuxKeyIndex++;
        NETWORKBYTES_TO_DWORD( rgoKeyStack[0].dwLocation, &pAuxKeyEntry->dwLocation, 0 );

        /* only for this condition we can derive the necessary key */
        if( IsAncestor( rgoKeyStack[0].dwLocation, dwLocation ) )
        {
            ChkDR( OEM_TEE_AES128CTR_DeriveScalableKeyWithAES128Key(
                f_pContext,
                f_pContentKeyPrime,
                pAuxKeyEntry->rgbAuxKey,
                &rgoKeyStack[0].oKey.oKey ) );

            /* two element rotating stack is initialized with a key now, top is set to second element */
            ChkDR( _TryDeriveKey( f_pContext, dwLocation, rgoKeyStack, &dwKeyIdx ) );
            ChkDR( OEM_TEE_AES128CTR_UpdateUplinkXKey( f_pContext, &rgoKeyStack[dwKeyIdx].oKey.oKey, f_pUplinkXKey ) );
            fFound = TRUE;
        }
    }

ErrorExit:
    for( iKey = 0; iKey < DRM_NO_OF( rgoKeyStack ); iKey++ )
    {
        ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &rgoKeyStack[iKey].oKey.oKey ) );
    }
    return dr;
}

static DRM_RESULT _CallwlateKeys_Derived(
    __inout             OEM_TEE_CONTEXT                 *f_pContext,
    __in          const OEM_TEE_KEY                     *f_pRootContentKey,
    __in          const DRM_XMRFORMAT_AUX_KEY           *f_poAuxKeys,
    __in          const DRM_XMRFORMAT_UPLINKX           *f_poUplinkX,
    __inout             OEM_TEE_KEY                     *f_pUplinkXKey );
static DRM_RESULT _CallwlateKeys_Derived(
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

    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( f_pContext, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &oContentKeyPrime ) );

    ChkDR( OEM_TEE_AES128CTR_CallwlateContentKeyPrimeWithAES128Key( f_pContext, f_pRootContentKey, g_rgbMagicConstantZero, &oContentKeyPrime.oKey ) );

    for( iEntry = 0; iEntry < f_poUplinkX->cEntries; iEntry++ )
    {
        ChkDR( _CalcOneDerivedKey( f_pContext, &oContentKeyPrime.oKey, f_poAuxKeys->cAuxKeys, pAuxKeys, f_poUplinkX, f_pUplinkXKey, iEntry ) );
    }

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( f_pContext, &oContentKeyPrime.oKey ) );
    return dr;
}
#endif
#if defined (SEC_COMPILE)
static DRM_RESULT _DecryptScalableLicenseKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_XMRFORMAT                *f_pLeaf,
    __in                  const DRM_XMRFORMAT                *f_pRoot,
    __in_ecount(2)        const DRM_TEE_KEY                  *f_pEscrowedKeys,
    __out_ecount(2)             DRM_TEE_KEY                  *f_pLeafKeys );
static DRM_RESULT _DecryptScalableLicenseKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_XMRFORMAT                *f_pLeaf,
    __in                  const DRM_XMRFORMAT                *f_pRoot,
    __in_ecount( 2 )      const DRM_TEE_KEY                  *f_pEscrowedKeys,
    __out_ecount( 2 )           DRM_TEE_KEY                  *f_pLeafKeys )
{
    DRM_RESULT                  dr                                      = DRM_SUCCESS;
    DRM_TEE_KEY                 oUplinkXKey                             = DRM_TEE_KEY_EMPTY;
    DRM_TEE_KEY                 oSecondaryKey                           = DRM_TEE_KEY_EMPTY;
    DRM_BYTE                    rgbSecondaryKey[DRM_AES_KEYSIZE_128]; /* Do not zero-initialize key material */
    OEM_TEE_CONTEXT            *pOemTeeCtx                              = NULL;

    const DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER   *pXmrContainerKeyLeaf = NULL;
    const DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER   *pXmrContainerKeyRoot = NULL;

    DRMASSERT( f_pContext      != NULL );
    DRMASSERT( f_pLeaf         != NULL );
    DRMASSERT( f_pRoot         != NULL );
    DRMASSERT( f_pEscrowedKeys != NULL );
    DRMASSERT( f_pLeafKeys     != NULL );

    DRMASSERT( f_pLeafKeys[ 0 ].eType == DRM_TEE_KEY_TYPE_PR_CI );
    DRMASSERT( f_pLeafKeys[ 1 ].eType == DRM_TEE_KEY_TYPE_PR_CK );

    /* Ensure that this is a scalable chained license */
    ChkArg( XMRFORMAT_IS_UPLINKX_VALID( f_pLeaf ) );
    ChkArg( XMRFORMAT_IS_AUX_KEY_VALID( f_pRoot ) );

    pOemTeeCtx = &f_pContext->oContext;

    pXmrContainerKeyLeaf = &f_pLeaf->OuterContainer.KeyMaterialContainer;
    pXmrContainerKeyRoot = &f_pRoot->OuterContainer.KeyMaterialContainer;

    /*
    ** The Content Key XMR object has the encrypted secondary key at offset
    ** ECC_P256_CIPHERTEXT_SIZE_IN_BYTES of the EncryptedKeyBuffer field.
    */
    DRMASSERT( pXmrContainerKeyRoot->ContentKey.wKeyEncryptionCipherType == XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256_WITH_KZ );
    ChkBOOL( pXmrContainerKeyRoot->ContentKey.xbbaEncryptedKey.cbData == ECC_P256_CIPHERTEXT_SIZE_IN_BYTES + DRM_AES_KEYSIZE_128, DRM_E_ILWALID_LICENSE );

    /* Initialize the uplinkx key with zeros */
    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &oUplinkXKey ) );
    ChkDR( OEM_TEE_AES128CTR_InitUplinkXKey( pOemTeeCtx, &oUplinkXKey.oKey ) );

    /* Derive the key, oUplinkXKey, based on the locations specified in the uplinkx */
    ChkDR( _CallwlateKeys_Derived( pOemTeeCtx, &f_pEscrowedKeys[ 1 ].oKey, &pXmrContainerKeyRoot->AuxKeys, &pXmrContainerKeyLeaf->UplinkX, &oUplinkXKey.oKey ) );  /* CK is always second */

    {
        DRM_BYTE *pbSecondaryKey = NULL;

        /* Get the secondary key */
        ChkDR( DRM_DWordPtrAdd( (DRM_DWORD_PTR)XBBA_TO_PB( pXmrContainerKeyRoot->ContentKey.xbbaEncryptedKey ), ECC_P256_CIPHERTEXT_SIZE_IN_BYTES, (DRM_DWORD_PTR*)&pbSecondaryKey ) );
        ChkVOID( OEM_TEE_MEMCPY(
            rgbSecondaryKey,
            pbSecondaryKey,
            DRM_AES_KEYSIZE_128 ) );
    }


    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_DERIVATION, &oSecondaryKey ) );
    ChkDR( OEM_TEE_AES128CTR_DeriveScalableKeyWithAES128Key( pOemTeeCtx, &f_pEscrowedKeys[ 1 ].oKey, rgbSecondaryKey, &oSecondaryKey.oKey ) );

    /* Decrypt leaf license's content key */
    ChkBOOL( pXmrContainerKeyLeaf->ContentKey.xbbaEncryptedKey.cbData == 2 * DRM_AES_KEYSIZE_128, DRM_E_ILWALID_LICENSE );
    ChkDR( OEM_TEE_AES128CTR_DecryptContentKeysWithDerivedKeys(
        pOemTeeCtx,
        &oSecondaryKey.oKey,
        &oUplinkXKey.oKey,
        XBBA_TO_PB( pXmrContainerKeyLeaf->ContentKey.xbbaEncryptedKey ),
        &f_pLeafKeys[ 0 ].oKey,
        &f_pLeafKeys[ 1 ].oKey ) );
    DRMASSERT( f_pLeafKeys[0].eType == DRM_TEE_KEY_TYPE_PR_CI );
    DRMASSERT( f_pLeafKeys[1].eType == DRM_TEE_KEY_TYPE_PR_CK );

ErrorExit:

    if( pOemTeeCtx != NULL )
    {
        ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oUplinkXKey.oKey ) );
        ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oSecondaryKey.oKey ) );
    }

    ChkVOID( OEM_TEE_ZERO_MEMORY( rgbSecondaryKey, sizeof( rgbSecondaryKey ) ) );

    return dr;
}
PRAGMA_DIAG_ON(attributes)

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicenseChain(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __out                 DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __out                 DRM_TEE_XMR_LICENSE          *f_pRoot );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicenseChain(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __out                 DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __out                 DRM_TEE_XMR_LICENSE          *f_pRoot )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    DRM_DWORD        cbLeaf     = 0;
    DRM_DWORD        cbRoot     = 0;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pLicenses != NULL );
    DRMASSERT( f_pLeaf     != NULL );
    DRMASSERT( f_pRoot     != NULL );
    DRMASSERT( f_pContext  != NULL );

    /* Callwlate length of leaf license */
    ChkBOOL( f_pLicenses->cb > XMR_HEADER_LENGTH + 2 * sizeof(DRM_WORD) + sizeof(DRM_DWORD), DRM_E_ILWALID_LICENSE );
    NETWORKBYTES_TO_DWORD( cbLeaf, f_pLicenses->pb, XMR_HEADER_LENGTH + 2 * sizeof(DRM_WORD) );
    ChkDR( DRM_DWordAddSame( &cbLeaf, XMR_HEADER_LENGTH ) );

    ChkBOOL( f_pLicenses->cb >= cbLeaf, DRM_E_ILWALID_LICENSE );

    ChkDR( DRM_TEE_BASE_IMPL_ParseLicense( f_pContext, cbLeaf, f_pLicenses->pb, f_pLeaf ) );

    if( f_pLicenses->cb > cbLeaf )
    {
        /* Remaining data must be root license */
        ChkDR( DRM_DWordSub( f_pLicenses->cb, cbLeaf, &cbRoot ) );
        ChkDR( DRM_TEE_BASE_IMPL_ParseLicense( f_pContext, cbRoot, f_pLicenses->pb + cbLeaf, f_pRoot ) );
    }

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _ValidateExtensibleRestrictionObjects(
    __in_ecount_opt( 1 )    const DRM_XB_UNKNOWN_OBJECT     *f_pUnkObject )
{
    DRM_RESULT                   dr   = DRM_SUCCESS;
    const DRM_XB_UNKNOWN_OBJECT *pUnk = f_pUnkObject;

    while( pUnk != NULL )
    {
        if( pUnk->fValid )
        {
            ChkBOOL( 0 == (pUnk->wFlags & XMR_FLAGS_MUST_UNDERSTAND), DRM_E_EXTENDED_RESTRICTION_NOT_UNDERSTOOD );
        }

        pUnk = pUnk->pNext;
    }

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _ValidateExtensibleRestrictionContainers(
    __in_ecount_opt( 1 )    const DRM_XB_UNKNOWN_CONTAINER  *f_pUnkCont )
{
    DRM_RESULT                      dr       = DRM_SUCCESS;
    const DRM_XB_UNKNOWN_CONTAINER *pLwrCont = f_pUnkCont;

    while( pLwrCont != NULL )
    {
        if( pLwrCont->fValid )
        {
            ChkDR( _ValidateExtensibleRestrictionObjects( ( const DRM_XB_UNKNOWN_OBJECT * )pLwrCont->pObject ) );
            ChkDR( _ValidateExtensibleRestrictionContainers( pLwrCont->pUnkChildcontainer ) );
        }

        pLwrCont = pLwrCont->pNext;
    }

ErrorExit:
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ValidateLicenseChain(
    __inout                      DRM_TEE_CONTEXT        *f_pContext,
    __in                   const DRM_TEE_BYTE_BLOB      *f_pChecksum,
    __in                         DRM_BOOL                f_fEnforceRIVs,
    __in                         DRM_DWORD               f_dwRKBRiv,
    __in                         DRM_TEE_XMR_LICENSE    *f_pLeaf,
    __in                         DRM_TEE_XMR_LICENSE    *f_pRoot,
    __in_ecount( 2 )             DRM_TEE_KEY            *f_pEscrowedKeys,
    __inout_ecount( 2 )          DRM_TEE_KEY            *f_pLeafKeys );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _ValidateLicenseChain(
    __inout                      DRM_TEE_CONTEXT        *f_pContext,
    __in                   const DRM_TEE_BYTE_BLOB      *f_pChecksum,
    __in                         DRM_BOOL                f_fEnforceRIVs,
    __in                         DRM_DWORD               f_dwRKBRiv,
    __in                         DRM_TEE_XMR_LICENSE    *f_pLeaf,
    __in                         DRM_TEE_XMR_LICENSE    *f_pRoot,
    __in_ecount( 2 )             DRM_TEE_KEY            *f_pEscrowedKeys,
    __inout_ecount( 2 )          DRM_TEE_KEY            *f_pLeafKeys )
{
    DRM_RESULT       dr             = DRM_SUCCESS;
    OEM_TEE_CONTEXT *pOemTeeCtx     = NULL;
    DRM_BOOL         fDerivedLeaf   = FALSE;

    DRMASSERT( f_pChecksum     != NULL );
    DRMASSERT( f_pLeaf         != NULL );
    DRMASSERT( f_pRoot         != NULL );
    DRMASSERT( f_pEscrowedKeys != NULL );
    DRMASSERT( f_pLeafKeys     != NULL );
    DRMASSERT( f_pContext      != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    if( f_pRoot->fValid )
    {
        DRMASSERT( f_pLeafKeys[ 0 ].eType == DRM_TEE_KEY_TYPE_PR_CI );
        DRMASSERT( f_pLeafKeys[ 1 ].eType == DRM_TEE_KEY_TYPE_PR_CK );

        /* Verify root license signature */
        ChkDR( DRM_TEE_BASE_IMPL_CheckLicenseSignature( f_pContext, &f_pRoot->oLicense, f_pEscrowedKeys ) );

        fDerivedLeaf = XMRFORMAT_IS_UPLINKX_VALID( &f_pLeaf->oLicense );
        ChkBOOL( fDerivedLeaf == XMRFORMAT_IS_AUX_KEY_VALID( &f_pRoot->oLicense ), DRM_E_ILWALID_LICENSE );

        if( fDerivedLeaf )
        {
            ChkDR( _DecryptScalableLicenseKey(
                f_pContext,
                &f_pLeaf->oLicense,
                &f_pRoot->oLicense,
                f_pEscrowedKeys,
                f_pLeafKeys ) );

            /* Verify chained checksum */
            ChkDR( OEM_TEE_AES128CTR_VerifyLicenseChecksum(
                pOemTeeCtx,
                &f_pLeafKeys[1].oKey, /* CK is always second */
                f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.guidKeyID.rgb,
                XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkX.xbbaChecksum ),
                XBBA_TO_CB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkX.xbbaChecksum ) ) );
        }
        else
        {
            /* Decrypt leaf license key with root content key - we already verified that the license has a content key */
            DRMASSERT( XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ) != NULL );
            __analysis_assume( XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ) != NULL );
            ChkDR( OEM_TEE_AES128CTR_DecryptContentKeysWithLicenseKey(
                pOemTeeCtx,
                &f_pEscrowedKeys[1].oKey, /* CK is always second */
                XBBA_TO_CB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ),
                XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ),
                &f_pLeafKeys[0].oKey,     /* CI is always first */
                &f_pLeafKeys[1].oKey ) ); /* CK is always second */

            /* Verify chained checksum */
            ChkDR( OEM_TEE_AES128CTR_VerifyLicenseChecksum(
                pOemTeeCtx,
                &f_pEscrowedKeys[1].oKey, /* CK is always second */
                f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkKID.idUplinkKID.rgb,
                XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkKID.xbbaChainedChecksum ),
                XBBA_TO_CB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkKID.xbbaChainedChecksum ) ) );
        }
    }

    /* Verify leaf license signature */
    ChkDR( DRM_TEE_BASE_IMPL_CheckLicenseSignature( f_pContext, &f_pLeaf->oLicense, f_pRoot->fValid ? f_pLeafKeys : f_pEscrowedKeys ) );

    if( !fDerivedLeaf && f_pChecksum->cb > 0 )
    {
        /* Verify content header checksum */
        ChkDR( OEM_TEE_AES128CTR_VerifyLicenseChecksum(
            pOemTeeCtx,
            f_pRoot->fValid ? &f_pLeafKeys[1].oKey : &f_pEscrowedKeys[1].oKey, /* CK is always second */
            f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.guidKeyID.rgb,
            f_pChecksum->pb,
            f_pChecksum->cb ) );
    }

    /*
    ** The high level OS will also check the license's RIV and in the case where SL > 2000 we disallow sample protection.
    */
    if( f_fEnforceRIVs )
    {
        ChkBOOL( f_pRoot->oLicense.OuterContainer.GlobalPolicyContainer.RevocationInfoVersion.dwValue <= f_dwRKBRiv, DRM_E_ILWALID_LICENSE );
        ChkBOOL( f_pLeaf->oLicense.OuterContainer.GlobalPolicyContainer.RevocationInfoVersion.dwValue <= f_dwRKBRiv, DRM_E_ILWALID_LICENSE );
    }

    /*
    ** Validate there are no MUST-UNDERSTAND extensible restrictions for playback
    */
    if( XMRFORMAT_IS_PLAY_VALID( &f_pLeaf->oLicense ))
    {
        ChkDR( _ValidateExtensibleRestrictionObjects( f_pLeaf->oLicense.OuterContainer.PlaybackPolicyContainer.pUnknownObjects ) );
        ChkDR( _ValidateExtensibleRestrictionContainers( &f_pLeaf->oLicense.OuterContainer.PlaybackPolicyContainer.UnknownContainer ) );
    }

    /*
    ** Validate there are no MUST-UNDERSTAND extensible restrictions for global license policy
    */
    ChkDR( _ValidateExtensibleRestrictionObjects( f_pLeaf->oLicense.OuterContainer.GlobalPolicyContainer.pUnknownObjects ) );
    ChkDR( _ValidateExtensibleRestrictionContainers( &f_pLeaf->oLicense.OuterContainer.UnknownContainer ) );


ErrorExit:
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ValidateLicenseEncryptionType(
    __in                  DRM_TEE_XMR_LICENSE          *f_pLicense );
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ValidateLicenseEncryptionType(
    __in                  DRM_TEE_XMR_LICENSE          *f_pLicense )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkBOOL( f_pLicense->fValid && XMRFORMAT_IS_CONTENT_KEY_VALID( &f_pLicense->oLicense ), DRM_E_ILWALID_LICENSE );
    ChkBOOL( f_pLicense->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CTR, DRM_E_ILWALID_LICENSE );

ErrorExit:
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ValidateDecryptionMode(
    __in                  DRM_DWORD                     f_dwRequestedDecryptionMode,
    __in                  DRM_WORD                      f_wSelwrityLevel );
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ValidateDecryptionMode(
    __in                  DRM_DWORD                     f_dwRequestedDecryptionMode,
    __in                  DRM_WORD                      f_wSelwrityLevel )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkBOOL( DRM_TEE_SAMPLEPROT_IMPL_IsDecryptionModeSupported( f_dwRequestedDecryptionMode ), DRM_E_RIGHTS_NOT_AVAILABLE );

    if( f_dwRequestedDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE )
    {
        ChkBOOL( f_wSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000, DRM_E_RIGHTS_NOT_AVAILABLE );
    }
    else if( f_dwRequestedDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        ChkBOOL( f_wSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000, DRM_E_RIGHTS_NOT_AVAILABLE );
    }
    else if( f_dwRequestedDecryptionMode != OEM_TEE_AES128CTR_DECRYPTION_MODE_HANDLE )
    {
        ChkBOOL( f_wSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000, DRM_E_RIGHTS_NOT_AVAILABLE );
    }

ErrorExit:
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ParseLicenseChainSelwrityLevel(
    __in                  DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in                  DRM_TEE_XMR_LICENSE          *f_pRoot,
    __in_opt        const DRM_DWORD                    *f_pdwSPKBSelwrityLevel,
    __out                 DRM_WORD                     *f_pwSelwrityLevel );
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ParseLicenseChainSelwrityLevel(
    __in                  DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in                  DRM_TEE_XMR_LICENSE          *f_pRoot,
    __in_opt        const DRM_DWORD                    *f_pdwSPKBSelwrityLevel,
    __out                 DRM_WORD                     *f_pwSelwrityLevel )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRMASSERT( f_pLeaf           != NULL );
    DRMASSERT( f_pRoot           != NULL );
    DRMASSERT( f_pwSelwrityLevel != NULL );

    ChkBOOL( f_pLeaf->fValid && XMRFORMAT_IS_SELWRITY_LEVEL_VALID( &f_pLeaf->oLicense ), DRM_E_ILWALID_LICENSE );

    *f_pwSelwrityLevel = f_pLeaf->oLicense.OuterContainer.GlobalPolicyContainer.SelwrityLevel.wMinimumSelwrityLevel;

    if( f_pRoot->fValid )
    {
        ChkBOOL( XMRFORMAT_IS_SELWRITY_LEVEL_VALID( &f_pRoot->oLicense ), DRM_E_ILWALID_LICENSE );

        ChkBOOL(
            f_pLeaf->oLicense.OuterContainer.GlobalPolicyContainer.SelwrityLevel.wMinimumSelwrityLevel
         <= f_pRoot->oLicense.OuterContainer.GlobalPolicyContainer.SelwrityLevel.wMinimumSelwrityLevel, DRM_E_ILWALID_LICENSE );

        /* Because we've verified leafSL <= rootSL, this ensures that we are using max( leafSL, rootSL ) */
        *f_pwSelwrityLevel = f_pRoot->oLicense.OuterContainer.GlobalPolicyContainer.SelwrityLevel.wMinimumSelwrityLevel;
    }

    if( f_pdwSPKBSelwrityLevel != NULL )
    {
        ChkBOOL( *f_pwSelwrityLevel <= *f_pdwSPKBSelwrityLevel, DRM_E_ILWALID_LICENSE );
    }

ErrorExit:
    return dr;
}

static DRM_FRE_INLINE DRM_VOID DRM_CALL _ParseLicenseChainExpiration(
    __in            const DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in            const DRM_TEE_XMR_LICENSE          *f_pRoot,
    __out                 DRM_DWORD                    *f_pdwBeginDate,
    __out                 DRM_DWORD                    *f_pdwEndDate );
static DRM_FRE_INLINE DRM_VOID DRM_CALL _ParseLicenseChainExpiration(
    __in            const DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in            const DRM_TEE_XMR_LICENSE          *f_pRoot,
    __out                 DRM_DWORD                    *f_pdwBeginDate,
    __out                 DRM_DWORD                    *f_pdwEndDate )
{
    DRM_DWORD  dwLeafBeginDate = 0;
    DRM_DWORD  dwRootBeginDate = 0;
    DRM_DWORD  dwLeafEndDate   = XMR_UNLIMITED;
    DRM_DWORD  dwRootEndDate   = XMR_UNLIMITED;

    DRMASSERT( f_pLeaf        != NULL );
    DRMASSERT( f_pRoot        != NULL );
    DRMASSERT( f_pdwBeginDate != NULL );
    DRMASSERT( f_pdwEndDate   != NULL );

    if( f_pLeaf->fValid && XMRFORMAT_IS_EXPIRATION_VALID( &f_pLeaf->oLicense ) )
    {
        dwLeafBeginDate = f_pLeaf->oLicense.OuterContainer.GlobalPolicyContainer.Expiration.dwBeginDate;
        dwLeafEndDate   = f_pLeaf->oLicense.OuterContainer.GlobalPolicyContainer.Expiration.dwEndDate  ;
    }

    if( f_pRoot->fValid && XMRFORMAT_IS_EXPIRATION_VALID( &f_pRoot->oLicense ) )
    {
        dwRootBeginDate = f_pRoot->oLicense.OuterContainer.GlobalPolicyContainer.Expiration.dwBeginDate;
        dwRootEndDate   = f_pRoot->oLicense.OuterContainer.GlobalPolicyContainer.Expiration.dwEndDate  ;
    }

    *f_pdwBeginDate = DRM_MAX( dwLeafBeginDate, dwRootBeginDate );
    *f_pdwEndDate   = DRM_MIN( dwLeafEndDate  , dwRootEndDate   );
}

static DRM_FRE_INLINE DRM_VOID DRM_CALL _ParseLicenseChainRealTimeExpirationPresent(
    __in            const DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in            const DRM_TEE_XMR_LICENSE          *f_pRoot,
    __out                 DRM_BOOL                     *f_pfRealTimeExpirationPresent );
static DRM_FRE_INLINE DRM_VOID DRM_CALL _ParseLicenseChainRealTimeExpirationPresent(
    __in            const DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in            const DRM_TEE_XMR_LICENSE          *f_pRoot,
    __out                 DRM_BOOL                     *f_pfRealTimeExpirationPresent )
{
    DRMASSERT( f_pLeaf                       != NULL );
    DRMASSERT( f_pRoot                       != NULL );
    DRMASSERT( f_pfRealTimeExpirationPresent != NULL );

    *f_pfRealTimeExpirationPresent =
        ( f_pLeaf->fValid && XMRFORMAT_IS_REAL_TIME_EXPIRATION_VALID( &f_pLeaf->oLicense ) )
     || ( f_pRoot->fValid && XMRFORMAT_IS_REAL_TIME_EXPIRATION_VALID( &f_pRoot->oLicense ) );
}

static DRM_FRE_INLINE DRM_VOID DRM_CALL _ParseLicenseChainOutputProtectionLevels(
    __in                                    const DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in                                    const DRM_TEE_XMR_LICENSE          *f_pRoot,
    __out_ecount( OUTPUT_PROTECTION__COUNT )      DRM_WORD                     *f_pwOutputProtectionLevels );
static DRM_FRE_INLINE DRM_VOID DRM_CALL _ParseLicenseChainOutputProtectionLevels(
    __in                                    const DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in                                    const DRM_TEE_XMR_LICENSE          *f_pRoot,
    __out_ecount( OUTPUT_PROTECTION__COUNT )      DRM_WORD                     *f_pwOutputProtectionLevels )
{
    DRMASSERT( f_pLeaf                    != NULL );
    DRMASSERT( f_pRoot                    != NULL );
    DRMASSERT( f_pwOutputProtectionLevels != NULL );

    if( f_pRoot->fValid && XMRFORMAT_IS_OPL_VALID( &f_pRoot->oLicense ) )
    {
        const DRM_XMRFORMAT_MINIMUM_OUTPUT_PROTECTION_LEVELS *pOPLs = &f_pRoot->oLicense.OuterContainer.PlaybackPolicyContainer.MinimumOutputProtectionLevel;

        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__COMPRESSED_DIGITAL_VIDEO_INDEX  ] = pOPLs->wCompressedDigitalVideo;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] = pOPLs->wUncompressedDigitalVideo;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__ANALOG_VIDEO_INDEX              ] = pOPLs->wAnalogVideo;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__COMPRESSED_DIGITAL_AUDIO_INDEX  ] = pOPLs->wCompressedDigitalAudio;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_AUDIO_INDEX] = pOPLs->wUncompressedDigitalAudio;
    }
    else if ( f_pLeaf->fValid && XMRFORMAT_IS_OPL_VALID( &f_pLeaf->oLicense ) )
    {
        const DRM_XMRFORMAT_MINIMUM_OUTPUT_PROTECTION_LEVELS *pOPLs = &f_pLeaf->oLicense.OuterContainer.PlaybackPolicyContainer.MinimumOutputProtectionLevel;

        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__COMPRESSED_DIGITAL_VIDEO_INDEX  ] = pOPLs->wCompressedDigitalVideo;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] = pOPLs->wUncompressedDigitalVideo;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__ANALOG_VIDEO_INDEX              ] = pOPLs->wAnalogVideo;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__COMPRESSED_DIGITAL_AUDIO_INDEX  ] = pOPLs->wCompressedDigitalAudio;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_AUDIO_INDEX] = pOPLs->wUncompressedDigitalAudio;
    }
    else
    {
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__COMPRESSED_DIGITAL_VIDEO_INDEX  ] = 100;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] = 100;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__ANALOG_VIDEO_INDEX              ] = 100;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__COMPRESSED_DIGITAL_AUDIO_INDEX  ] = 100;
        f_pwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_AUDIO_INDEX] = 100;
    }
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicensePlayEnablers(
    __in                              const DRM_TEE_XMR_LICENSE   *f_pLicense,
    __out_ecount( 1 )                       DRM_DWORD             *f_pdwPlayEnablers );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicensePlayEnablers(
    __in                              const DRM_TEE_XMR_LICENSE   *f_pLicense,
    __out_ecount( 1 )                       DRM_DWORD             *f_pdwPlayEnablers )
{
    DRM_RESULT                                     dr              = DRM_SUCCESS;
    DRM_DWORD                                      dwPlayEnablers  = 0;
    /* initialized as weak references later */
    const DRM_XMRFORMAT_PLAYBACK_POLICY_CONTAINER *pPlayback;
    const DRM_XB_UNKNOWN_CONTAINER                *pContainer;
    const DRM_XB_UNKNOWN_OBJECT                   *pObject;

    DRMASSERT( f_pLicense        != NULL );
    DRMASSERT( f_pdwPlayEnablers != NULL );

    pPlayback = &f_pLicense->oLicense.OuterContainer.PlaybackPolicyContainer;

    for( pContainer = &pPlayback->UnknownContainer; pContainer != NULL; pContainer = pContainer->pNext )
    {
        if( pContainer->wType == XMR_OBJECT_TYPE_PLAY_ENABLER_CONTAINER )
        {
            for( pObject = pContainer->pObject; pObject != NULL; pObject = pObject->pNext )
            {
                if( pObject->wType == XMR_OBJECT_TYPE_PLAY_ENABLER_OBJECT )
                {
                    const void *pComparison = pObject->pbBuffer + pObject->ibData;
                    static const void * rpvComparisonGuids[] PR_ATTR_DATA_OVLY(_rpvComparisonGuids) =
                    {
                        &g_guidPlayEnablerUnknownOutput,
                        &g_guidPlayEnablerUnknownOutputConstrained,
                        &g_guidPlayEnablerDTCP,
                        &g_guidPlayEnablerHelix,
                        &g_guidPlayEnablerHDCPMiracast,
                        &g_guidPlayEnablerHDCPWiVu,
                        &g_guidPlayEnablerAirPlay,
                    };
                    static const DRM_DWORD rdwTypeMasks[] PR_ATTR_DATA_OVLY(_rdwTypeMasks) =
                    {
                        DRM_TEE_PLAY_ENABLER_TYPE_UNKNOWN_OUTPUT,
                        DRM_TEE_PLAY_ENABLER_TYPE_UNKNOWN_OUTPUT_CONSTRAINED,
                        DRM_TEE_PLAY_ENABLER_TYPE_DTCP,
                        DRM_TEE_PLAY_ENABLER_TYPE_HELIX,
                        DRM_TEE_PLAY_ENABLER_TYPE_HDCP_MIRACAST,
                        DRM_TEE_PLAY_ENABLER_TYPE_HDCP_WIVU,
                        DRM_TEE_PLAY_ENABLER_TYPE_AIRPLAY,
                    };
                    DRM_DWORD idx;

                    DRMCASSERT( DRM_NO_OF( rdwTypeMasks ) == DRM_NO_OF( rpvComparisonGuids ) );
                    ChkBOOL( pObject->cbData == sizeof(DRM_GUID), DRM_E_ILWALID_LICENSE );
                    for (idx = 0; idx < DRM_NO_OF( rdwTypeMasks ); idx++ )
                    {
                        if( OEM_SELWRE_ARE_EQUAL( pComparison, rpvComparisonGuids[idx], sizeof(DRM_GUID) ) )
                        {
                            dwPlayEnablers |= rdwTypeMasks[idx];
                            break;
                        }
                    }
                }
            }
        }
    }

    /* Unknown output is not allowed if constrained resolution unknown output is present */
    if( ( dwPlayEnablers & DRM_TEE_PLAY_ENABLER_TYPE_UNKNOWN_OUTPUT_CONSTRAINED ) != 0 )
    {
        dwPlayEnablers &= ~DRM_TEE_PLAY_ENABLER_TYPE_UNKNOWN_OUTPUT;
    }

    *f_pdwPlayEnablers = dwPlayEnablers;

ErrorExit:
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicenseRestrictions(
    __inout                                     DRM_TEE_CONTEXT             *f_pContext,
    __in                                  const DRM_TEE_XMR_LICENSE         *f_pLicense,
    __out_ecount( 1 )                           DRM_DWORD                   *f_pcRestrictions,
    __deref_out_ecount_opt( *f_pcRestrictions ) DRM_TEE_POLICY_RESTRICTION **f_ppRestrictions );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicenseRestrictions(
    __inout                                     DRM_TEE_CONTEXT             *f_pContext,
    __in                                  const DRM_TEE_XMR_LICENSE         *f_pLicense,
    __out_ecount( 1 )                           DRM_DWORD                   *f_pcRestrictions,
    __deref_out_ecount_opt( *f_pcRestrictions ) DRM_TEE_POLICY_RESTRICTION **f_ppRestrictions )
{
    DRM_RESULT                                dr             = DRM_SUCCESS;
    DRM_DWORD                                 iRestriction   = 0;
    DRM_DWORD                                 cRestrictions  = 0;
    DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION *pRestriction   = NULL;
    DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION *pConfiguration = NULL;
    OEM_TEE_CONTEXT                          *pOemTeeCtx     = NULL;
    DRM_TEE_POLICY_RESTRICTION               *pRestrictions  = NULL;

    DRMASSERT( f_pContext       != NULL );
    DRMASSERT( f_pLicense       != NULL );
    DRMASSERT( f_pcRestrictions != NULL );
    DRMASSERT( f_ppRestrictions != NULL );

    ChkBOOL( f_pLicense->fValid && XMRFORMAT_IS_PLAY_VALID( &f_pLicense->oLicense ), DRM_E_ILWALID_LICENSE );

    pOemTeeCtx = &f_pContext->oContext;

    /* f_pcRestrictions = ExplicitAnalogVideoProtection + ExplicitDigitalAudioProtection + ExplicitDigitalVideoProtection */
    ChkDR( DRM_DWordAdd(
        f_pLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitAnalogVideoProtection.cEntries,
        f_pLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalAudioProtection.cEntries,
        f_pcRestrictions ) );
    ChkDR( DRM_DWordAddSame(
        f_pcRestrictions,
        f_pLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalVideoProtection.cEntries ) );

    if( *f_pcRestrictions > 0 )
    {
        DRM_DWORD cbRestrictions = 0;
        ChkDR( DRM_DWordMult( *f_pcRestrictions, sizeof(DRM_TEE_POLICY_RESTRICTION), &cbRestrictions ) );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbRestrictions, (DRM_VOID**)&pRestrictions ) );

        for( iRestriction = 0; iRestriction < 3; iRestriction++ )
        {
            switch( iRestriction )
            {
            case 0: pRestriction = f_pLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitAnalogVideoProtection.pOPL ; break;
            case 1: pRestriction = f_pLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalAudioProtection.pOPL; break;
            case 2: pRestriction = f_pLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalVideoProtection.pOPL; break;
            }

            for( pConfiguration = pRestriction; pConfiguration != NULL; pConfiguration = pConfiguration->pNext )
            {
                ChkBOOL( cRestrictions <= *f_pcRestrictions, DRM_E_BUFFER_BOUNDS_EXCEEDED );

                switch( iRestriction )
                {
                case 0: pRestrictions[cRestrictions].wRestrictionCategory = (DRM_WORD)XMR_OBJECT_TYPE_EXPLICIT_ANALOG_VIDEO_OUTPUT_PROTECTION_CONTAINER ; break;
                case 1: pRestrictions[cRestrictions].wRestrictionCategory = (DRM_WORD)XMR_OBJECT_TYPE_EXPLICIT_DIGITAL_AUDIO_OUTPUT_PROTECTION_CONTAINER; break;
                case 2: pRestrictions[cRestrictions].wRestrictionCategory = (DRM_WORD)XMR_OBJECT_TYPE_EXPLICIT_DIGITAL_VIDEO_OUTPUT_PROTECTION_CONTAINER; break;
                }

                pRestrictions[cRestrictions].idRestrictionType          = pConfiguration->idOPL;
                pRestrictions[cRestrictions].cbRestrictionConfiguration = XBBA_TO_CB( pConfiguration->xbbaConfig );
                pRestrictions[cRestrictions].pbRestrictionConfiguration = XBBA_TO_PB( pConfiguration->xbbaConfig );
                cRestrictions++;
            }
        }
    }

    DRMASSERT( *f_pcRestrictions == cRestrictions );

    *f_ppRestrictions = pRestrictions;
    pRestrictions     = NULL;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, ( DRM_VOID ** )&pRestrictions ) );

    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicensePolicies(
    __inout               DRM_TEE_CONTEXT     *f_pContext,
    __in            const DRM_TEE_XMR_LICENSE *f_poLeaf,
    __in            const DRM_TEE_XMR_LICENSE *f_poRoot,
    __inout               DRM_TEE_POLICY_INFO *f_poPolicy )
{
    DRM_RESULT dr = DRM_SUCCESS;

    f_poPolicy->oKID = f_poLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.guidKeyID;
    f_poPolicy->oLID = f_poLeaf->oLicense.HeaderData.LID;
    ChkVOID( _ParseLicenseChainExpiration( f_poLeaf, f_poRoot, &f_poPolicy->dwBeginDate, &f_poPolicy->dwEndDate ) );
    ChkVOID( _ParseLicenseChainRealTimeExpirationPresent( f_poLeaf, f_poRoot, &f_poPolicy->fRealTimeExpirationPresent ) );
    ChkVOID( _ParseLicenseChainOutputProtectionLevels( f_poLeaf, f_poRoot, f_poPolicy->rgwOutputProtectionLevels ) );
    ChkDR( _ParseLicensePlayEnablers( f_poLeaf, &f_poPolicy->dwPlayEnablers ) );

    ChkDR( _ParseLicenseRestrictions(
        f_pContext,
        f_poLeaf,
        &f_poPolicy->cRestrictions,
        &f_poPolicy->pRestrictions ) );

ErrorExit:
    return dr;
}


static DRM_FRE_INLINE DRM_VOID DRM_CALL _FreeLicenseChain(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in                  DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in                  DRM_TEE_XMR_LICENSE          *f_pRoot );
static DRM_FRE_INLINE DRM_VOID DRM_CALL _FreeLicenseChain(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in                  DRM_TEE_XMR_LICENSE          *f_pLeaf,
    __in                  DRM_TEE_XMR_LICENSE          *f_pRoot )
{
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pLeaf    != NULL );
    DRMASSERT( f_pRoot    != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&f_pLeaf->pbStack ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&f_pRoot->pbStack ) );
}
#endif
#ifdef NONE
/*
** Synopsis:
**
** This function takes a complete license chain (simple or leaf/root) which
** can decrypt content with AES128CTR and allows the OEM to construct an
** OEM-specific policy blob containing the same policy that the CDKB will
** include when DRM_TEE_AES128CTR_PrepareToDecrypt is called.
**
** This function is not called by any Drm_* APIs.
**
** Operations Performed:
**
**  1. Parse the given XMR license(s).
**  2. Call an OEM function to create the OEM-specific policy blob.
**
** Arguments:
**
** f_pContext:          (in/out) The TEE context returned from
**                               DRM_TEE_BASE_AllocTEEContext.
** f_pLicenses:         (in)     The raw binary XMR license chain.
**                               The leaf-most license must be first in the byte array.
**                               The leaf-most license must use AES128CTR keys for CI/CK.
**                               Each license up the chain (if any) should
**                               then sequentially be next in the byte array (if any).
**                               Since each license has an embedded license size, the
**                               PRITEE can easily parse this blob into individual licenses.
** f_dwDecryptionMode:  (in)     The mode requested by the caller for decryption.
** f_pOEMPolicyInfo:    (out)    An OEM specific policy data blob.
**                               This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_PreparePolicyInfo(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __in                        DRM_DWORD                     f_dwDecryptionMode,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOEMPolicyInfo )
{
    DRM_RESULT            dr                           = DRM_SUCCESS;
    DRM_RESULT            drTmp                        = DRM_SUCCESS;
    DRM_TEE_XMR_LICENSE   oLeaf                        = {0};
    DRM_TEE_XMR_LICENSE   oRoot                        = {0};
    DRM_DWORD             cbOEMPolicyInfo              = 0;
    DRM_BYTE             *pbOEMPolicyInfo              = NULL;
    DRM_TEE_BYTE_BLOB     oOEMPolicyInfo               = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT      *pOemTeeCtx                   = NULL;
    DRM_TEE_POLICY_INFO   oPolicy                      = {0};

    DRMASSERT( f_pContext       != NULL );
    DRMASSERT( f_pLicenses      != NULL );
    DRMASSERT( f_pOEMPolicyInfo != NULL );

    DRMASSERT( IsBlobAssigned( f_pLicenses ) );

    pOemTeeCtx = &f_pContext->oContext;

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pOEMPolicyInfo, sizeof(*f_pOEMPolicyInfo) ) );

    oPolicy.dwDecryptionMode = f_dwDecryptionMode;

    DRMCASSERT( DRM_MAX_LICENSE_CHAIN_DEPTH == 2 );
    ChkDR( _ParseLicenseChain( f_pContext, f_pLicenses, &oLeaf, &oRoot ) );

    ChkDR( _ParseLicenseChainSelwrityLevel( &oLeaf, &oRoot, NULL, &oPolicy.wSelwrityLevel ) );

    ChkDR( _ParseLicensePolicies( f_pContext, &oLeaf, &oRoot, &oPolicy ) );

    dr = OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo(
        pOemTeeCtx,
        &f_pContext->idSession,
        NULL,
        NULL,
        0,
        NULL,
        &oPolicy,
        &cbOEMPolicyInfo,
        &pbOEMPolicyInfo );

    if( dr == DRM_E_NOTIMPL )
    {
        dr = DRM_SUCCESS;
    }
    else
    {
        ChkDR( dr );
        ChkDR( DRM_TEE_BASE_IMPL_AllocBlobAndTakeOwnership( f_pContext, cbOEMPolicyInfo, &pbOEMPolicyInfo, &oOEMPolicyInfo ) );
        ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pOEMPolicyInfo, &oOEMPolicyInfo ) );
    }

ErrorExit:
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_BASE_FreeBlob(f_pContext, &oOEMPolicyInfo);
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbOEMPolicyInfo ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&oPolicy.pRestrictions ) );
    ChkVOID( _FreeLicenseChain( f_pContext, &oLeaf, &oRoot ) );
    return dr;
}
#endif
#if defined (SEC_COMPILE)
/*
** Synopsis:
**
** This function takes a complete license chain (simple or leaf/root) which
** can decrypt content with AES128CTR and is bound to a given LKB.
** It returns sufficient data for content decryption to occur.
**
** This function is called inside Drm_Reader_Bind.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Parse the given PlayReady Private Key Blob (PPKB) and verify its signature.
**  3. Parse the given License Key Blob (LKB) and verify its signature.
**  4. Parse the given XMR license(s).
**  5. If two licenses were given (i.e. a license chain):
**     Verify the given root license's signature using its CI (from LKB).
**     Decrypt the given leaf license's CI/CK using the given root license's CK.
**     Verify the chained license checksum.
**  6. Verify the signature on the given leaf-most license using its CI
**     (either from the given LKB or decrypted using root's CK).
**  7. If a checksum from a content header was given, verify it.
**  8. If the OEM supports a clock inside the TEE, validate that all given
**     licenses have passed their begin date and not passed their end date.
**  9. Verify that the given leaf-most license uses AES 128 CTR.
** 10. If two licenses were given (i.e. a license chain):
**     Verify that the given security level for leaf <= root.
** 11. Call an OEM function to ensure that the OEM either supports the requested
**     decryption mode or changes the decryption mode to the MOST secure
**     one it supports.
** 12. If the mode is now OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE (zero),
**     verify that the given leaf-most license's security level <= 2000.
** 13. Parse any additional data from the license required to build a
**     Content Decryption Key Blob (CDKB).
** 14. Build and sign a CDKB.
**     This will include several pieces of data from the license(s).
** 15. Return the new CDKB and decryption mode.
**
** Arguments:
**
** f_pContext:          (in/out) The TEE context returned from
**                               DRM_TEE_BASE_AllocTEEContext.
** f_pLKB:              (in)     The License Key Blob (LKB) for the root-most license
**                               in the given license chain.
** f_pLicenses:         (in)     The raw binary XMR license chain.
**                               The leaf-most license must be first in the byte array.
**                               The leaf-most license must use AES128CTR keys for CI/CK.
**                               Each license up the chain (if any) should
**                               then sequentially be next in the byte array (if any).
**                               Since each license has an embedded license size, the
**                               PRITEE can easily parse this blob into individual licenses.
** f_pRKB:              (in)     The revocation key blob (RKB).
** f_pSPKB:             (in)     The sample protection key blob (SPKB).
** f_pChecksum:         (in)     The checksum from the content header, if any.
** f_pdwDecryptionMode: (in/out) On input, the mode requested by the caller
**                               for decryption.
**                               On output, the mode used for decryption
**                               which also determines the format of the CCD
**                               that will be returned by
**                               DRM_TEE_AES128CTR_DecryptContent.
**
**                               The value of OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE
**                               (zero) means that content is returned in the clear,
**                               in which case this function will fail with
**                               DRM_E_RIGHTS_NOT_AVAILABLE if the content is not <= SL2000.
**                               Any other value is OEM-specific.
**                               The caller may request a mode which is not supported
**                               in which case the OEM will remap the value to the mode
**                               which is the MOST secure version that is supported.
**                               If the MOST secure version that is supported is
**                               OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE, then
**                               this function will fail with
**                               DRM_E_RIGHTS_NOT_AVAILABLE if the content is not <= SL2000.
** f_pCDKB:             (out)    The Content Decryption Key Blob (CDKB) used to decrypt
**                               content with a KID that matches the leaf-most license in
**                               the specified license chain.
**                               This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_PrepareToDecrypt(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pSPKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pChecksum,
    __inout                     DRM_DWORD                    *f_pdwDecryptionMode,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCDKB )
{
    DRM_RESULT            dr                           = DRM_SUCCESS;
    DRM_TEE_XMR_LICENSE   oLeaf                        = {0};
    DRM_TEE_XMR_LICENSE   oRoot                        = {0};
    DRM_DWORD             cEscrowedKeys                = 0;
    DRM_TEE_KEY          *pEscrowedKeys                = NULL;
    DRM_TEE_KEY           rgoLeafKeys[2]               = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    OEM_TEE_CONTEXT      *pOemTeeCtx                   = NULL;
    DRM_BOOL              fSelfContained               = FALSE;
    DRM_TEE_POLICY_INFO   oPolicy;
    DRM_TEE_RKB_CONTEXT   oRKBCtx;                     /* Initialized in DRM_TEE_KB_ParseAndVerifyRKB */
    DRM_DWORD             cSPKeys                      = 0;
    DRM_TEE_KEY          *pSPKeys                      = NULL;
    DRM_DWORD             dwSPKBSelwrityLevel          = 0;
    DRM_DWORD             dwSPKBRIV                    = 0;
    DRM_DWORD             dwRKBRIV                     = 0;
    const DRM_DWORD      *pdwSPKBSelwrityLevel         = NULL;
    DRM_TEE_KEY           rgoCICKSPK[3]                = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    DRM_RevocationEntry   rgbCertDigests[DRM_BCERT_MAX_CERTS_PER_CHAIN] = {{ 0 }};

    // LWE (nkuo) - Initialize this parameter here instead at declaration time to avoid compile error "missing braces around initializer"
    ChkVOID( OEM_TEE_ZERO_MEMORY( &oPolicy, sizeof(DRM_TEE_POLICY_INFO) ) );

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext          != NULL );
    DRMASSERT( f_pLicenses         != NULL );
    DRMASSERT( f_pLKB              != NULL );
    DRMASSERT( f_pChecksum         != NULL );
    DRMASSERT( f_pdwDecryptionMode != NULL );
    DRMASSERT( f_pCDKB             != NULL );
    DRMASSERT( f_pRKB              != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    oPolicy.dwDecryptionMode = *f_pdwDecryptionMode;

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pCDKB, sizeof(*f_pCDKB) ) );

    /*
    ** The OEM may upgrade (inselwre->secure), downgrade (secure->inselwre), or modify (secure->secure) the
    ** decryption mode based on what it supports to allow an application to be written cross-platform as long
    ** as it can handle the returned decryption mode (and the hardware can as well).
    ** It's ok to upgrade or downgrade as long as we only allow >SL2000 to use something OTHER than non-secure decryption mode.
    ** Upgrade/downgrade must happen __BEFORE__ we validate the decryption mode via the call to _ValidateDecryptionMode
    ** in order to ensure that >SL2000 content does not use non-secure decryption mode.
    */
    ChkVOID( OEM_TEE_AES128CTR_RemapRequestedDecryptionModeToSupportedDecryptionMode( pOemTeeCtx, &oPolicy.dwDecryptionMode ) );

    if( oPolicy.dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        DRM_DWORD cCertDigests = DRM_BCERT_MAX_CERTS_PER_CHAIN;

        ChkArg( IsBlobAssigned( f_pSPKB ) );
        ChkDR( DRM_TEE_KB_ParseAndVerifySPKB(
            f_pContext,
            f_pSPKB,
            &dwSPKBSelwrityLevel,
            &cCertDigests,
            rgbCertDigests,
            &dwSPKBRIV,
            &cSPKeys,
            &pSPKeys) );

        AssertChkArg( dwSPKBSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000 );
        pdwSPKBSelwrityLevel = &dwSPKBSelwrityLevel;

        ChkBOOL( IsBlobAssigned( f_pRKB ), DRM_E_RIV_TOO_SMALL );
        ChkDR( DRM_TEE_KB_ParseAndVerifyRKB( f_pContext, f_pRKB, &oRKBCtx, cCertDigests, (const DRM_RevocationEntry*)rgbCertDigests ) );

        ChkBOOL( dwSPKBRIV <= oRKBCtx.dwRIV, DRM_E_ILWALID_REVOCATION_LIST );

        dwRKBRIV = oRKBCtx.dwRIV;
    }
    else
    {
        ChkArg( !IsBlobAssigned( f_pSPKB ) );
    }

    DRMCASSERT( DRM_MAX_LICENSE_CHAIN_DEPTH == 2 );
    ChkDR( _ParseLicenseChain( f_pContext, f_pLicenses, &oLeaf, &oRoot ) );

    ChkDR( _ValidateLicenseEncryptionType( &oLeaf ) );

    /* we need pEscrowedKeys only now */
    ChkDR( DRM_TEE_KB_ParseAndVerifyLKB( f_pContext, f_pLKB, &cEscrowedKeys, &pEscrowedKeys ) );

    if( oRoot.fValid )
    {
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CI, &rgoLeafKeys[ 0 ] ) );
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &rgoLeafKeys[ 1 ] ) );
    }

    ChkDR( _ValidateLicenseChain(
        f_pContext,
        f_pChecksum,
        oPolicy.dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION,
        dwRKBRIV,
        &oLeaf,
        &oRoot,
        pEscrowedKeys,
        rgoLeafKeys ) );

    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CI, &rgoCICKSPK[ 0 ] ) );
    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &rgoCICKSPK[ 1 ] ) );
    ChkDR( OEM_TEE_BASE_CopyKeyAES128(  pOemTeeCtx, &rgoCICKSPK[0].oKey, oRoot.fValid ? &rgoLeafKeys[0].oKey : &pEscrowedKeys[0].oKey ) );
    ChkDR( OEM_TEE_BASE_CopyKeyAES128(  pOemTeeCtx, &rgoCICKSPK[1].oKey, oRoot.fValid ? &rgoLeafKeys[1].oKey : &pEscrowedKeys[1].oKey ) );

    /* Keys no longer needed, free them */
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[0].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[1].oKey ) );
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray(f_pContext, cEscrowedKeys, &pEscrowedKeys ) );

    ChkDR( DRM_TEE_BASE_IMPL_ValidateLicenseExpiration( f_pContext, &oLeaf.oLicense, TRUE, TRUE ) );

    if( oRoot.fValid )
    {
        ChkDR( DRM_TEE_BASE_IMPL_ValidateLicenseExpiration( f_pContext, &oRoot.oLicense, TRUE, TRUE ) );
    }

    ChkDR( _ParseLicenseChainSelwrityLevel( &oLeaf, &oRoot, pdwSPKBSelwrityLevel, &oPolicy.wSelwrityLevel ) );
    ChkDR( _ValidateDecryptionMode( oPolicy.dwDecryptionMode, oPolicy.wSelwrityLevel ) );

    ChkDR( _ParseLicensePolicies( f_pContext, &oLeaf, &oRoot, &oPolicy ) );

    ChkDRAllowENOTIMPL( OEM_TEE_AES128CTR_EnforcePolicy( pOemTeeCtx, DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_PREPARETODECRYPT, &oPolicy ) );
    if( oPolicy.dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_SAMPLEPROT, &rgoCICKSPK[ 2 ] ) );
        ChkDR( OEM_TEE_BASE_CopyKeyAES128(  pOemTeeCtx, &rgoCICKSPK[2].oKey, &pSPKeys[0].oKey ) );
    }

    ChkDR( DRM_TEE_KB_BuildCDKB(
        f_pContext,
        fSelfContained,
        IsBlobAssigned( f_pSPKB ) ? (DRM_DWORD)3 : (DRM_DWORD)2,
        rgoCICKSPK,
        &oPolicy,
        f_pCDKB ) );

    *f_pdwDecryptionMode = oPolicy.dwDecryptionMode;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cSPKeys, &pSPKeys ) );
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cEscrowedKeys, &pEscrowedKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&oPolicy.pRestrictions ) );
    ChkVOID( _FreeLicenseChain( f_pContext, &oLeaf, &oRoot ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[0].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[1].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoCICKSPK[0].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoCICKSPK[1].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoCICKSPK[2].oKey ) );
    return dr;
}

/*
** Synopsis:
**
** This function takes CDKB and create a OEM KeyBlob.
**
** This function is usually called after DRM_TEE_AES128CTR_PrepareToDecrypt.
**
** f_pContext:          (in/out) The TEE context returned from
**                               DRM_TEE_BASE_AllocTEEContext.
** f_pCDKB:             (in)     The Content Decryption Key Blob (CDKB) used to decrypt
**                               content with a KID that matches the leaf-most license in
**                               the specified license chain.
** f_pOEMInitData       (in)     An OEM specific initialization data blob used to
**                               aid in the creation of the f_pOEMKeyInfo output parameter
**                               specifically for offloaded decryption that doesn't use CDKB
** f_pOEMKeyInfo        (out)    An OEM specific key data blob used
**                               specifically for offloaded decryption that doesn't use CDKB.
**                               The format of this data is opaque to the PlayReady PK.
**                               This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_CreateOEMBlobFromCDKB(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMInitData,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo )
{
    DRM_RESULT           dr                            = DRM_SUCCESS;
    DRM_RESULT           drTmp                         = DRM_SUCCESS;
    DRM_DWORD            cCDKeys                       = 0;
    DRM_TEE_KEY         *pCDKeys                       = NULL;
    DRM_DWORD            cbOEMKeyInfo                  = 0;
    DRM_BYTE            *pbOEMKeyInfo                  = NULL;
    DRM_TEE_BYTE_BLOB    oOEMKeyInfo                   = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT     *pOemTeeCtx                    = NULL;
    DRM_BOOL             fSelfContained                = FALSE;
    DRM_TEE_POLICY_INFO  oPolicy;

    // LWE (nkuo) - Initialize this parameter here instead at declaration time to avoid compile error "missing braces around initializer"
    ChkVOID( OEM_TEE_ZERO_MEMORY( &oPolicy, sizeof(DRM_TEE_POLICY_INFO) ) );

    /* Arguments are checked in the DRM TEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pCDKB    != NULL );
    DRMASSERT( f_pOEMKeyInfo != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_TEE_KB_ParseAndVerifyCDKB(
        f_pContext,
        f_pCDKB,
        fSelfContained,
        &cCDKeys,
        &pCDKeys,
        &oPolicy ) );

    dr = OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo(
      pOemTeeCtx,
      &f_pContext->idSession,
      &pCDKeys[1].oKey,
      cCDKeys == 3 ? &pCDKeys[2].oKey : NULL,
      f_pOEMInitData == NULL ? 0    : f_pOEMInitData->cb,
      f_pOEMInitData == NULL ? NULL : f_pOEMInitData->pb,
      &oPolicy,
      &cbOEMKeyInfo,
      &pbOEMKeyInfo );

    if( dr == DRM_E_NOTIMPL )
    {
        dr = DRM_SUCCESS;
        DRMASSERT( pbOEMKeyInfo == NULL );

        fSelfContained = TRUE;

        ChkDR( DRM_TEE_KB_BuildCDKB(
            f_pContext,
            fSelfContained,
            cCDKeys,
            pCDKeys,
            &oPolicy,
            &oOEMKeyInfo ) );
    }
    else
    {
        ChkDR( dr );
        if( cbOEMKeyInfo > 0 )
        {
            ChkDR( DRM_TEE_BASE_IMPL_AllocBlobAndTakeOwnership( f_pContext, cbOEMKeyInfo, &pbOEMKeyInfo, &oOEMKeyInfo ) );
        }
    }

    /* transfer ownership */
    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pOEMKeyInfo, &oOEMKeyInfo ) );

ErrorExit:
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp =  DRM_TEE_BASE_FreeBlob( f_pContext, &oOEMKeyInfo );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbOEMKeyInfo ) );
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );
    ChkVOID( DRM_TEE_KB_FreeCDKBCryptoDataRestrictionArray(
        f_pContext,
        oPolicy.cRestrictions,
        oPolicy.pRestrictions ) );
    return dr;
}
#endif
#ifdef NONE
static DRM_NO_INLINE DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_IMPL_DecryptContentPolicyHelper(
    __inout_opt                 DRM_TEE_CONTEXT                      *f_pContext,
    __deref_out                 DRM_TEE_CONTEXT                     **f_ppContext,
    __inout                     DRM_TEE_CONTEXT                      *f_pTeeCtxReconstituted,
    __out                       DRM_BOOL                             *f_pfReconstituted,
    __in                        DRM_TEE_POLICY_INFO_CALLING_API       f_ePolicyCallingAPI,
    __in                  const DRM_TEE_BYTE_BLOB                    *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB                    *f_pOEMKeyInfo,
    __out                       DRM_DWORD                            *f_pcKeys,
    __deref_out                 DRM_TEE_KEY                         **f_ppKeys,
    __out                       DRM_DWORD                            *f_pdwDecryptionMode,
    __out                       DRM_TEE_POLICY_INFO                 **f_ppPolicy )
{
    DRM_RESULT               dr                           = DRM_SUCCESS;
    DRM_DWORD                cCDKeys                      = 0;
    DRM_TEE_KEY             *pCDKeys                      = NULL;
    DRM_TEE_POLICY_INFO     *pOemPolicy                   = NULL;
    OEM_TEE_CONTEXT         *pOemTeeCtx                   = NULL;
    DRM_TEE_POLICY_INFO     *pPolicyRef                   = NULL;

    DRMASSERT(  f_ppPolicy != NULL );
    DRMASSERT( *f_ppPolicy != NULL );

    if( f_pContext == NULL )
    {
        f_pContext = f_pTeeCtxReconstituted;
        *f_pfReconstituted = TRUE;
    }
    else
    {
        *f_pfReconstituted = FALSE;
    }

    ChkDRAllowENOTIMPL( DRM_TEE_AES128CTR_IMPL_ParseAndVerifyExternalPolicyCryptoInfo(
        f_pContext,
        f_pCDKB,
        f_pOEMKeyInfo,
        *f_pfReconstituted,
        &cCDKeys,
        &pCDKeys,
        &pOemPolicy,
        *f_pfReconstituted ? &f_pContext->idSession : NULL ) );

    if( cCDKeys != 0 )
    {
        pPolicyRef = pOemPolicy;
    }
    else
    {
        AssertChkArg( pCDKeys == NULL );
        AssertChkArg( pOemPolicy == NULL );
        pPolicyRef = *f_ppPolicy;
        ChkDR( DRM_TEE_KB_ParseAndVerifyCDKB(
            f_pContext,
            f_pCDKB,
            *f_pfReconstituted,
            &cCDKeys,
            &pCDKeys,
            pPolicyRef ) );
    }

    AssertChkArg( cCDKeys == 2 || cCDKeys == 3 );
    if( *f_pfReconstituted )
    {
        ChkDR( DRM_TEE_CACHE_ReferenceContext( f_pContext ) );
    }

    pOemTeeCtx = &f_pContext->oContext;

    ChkDRAllowENOTIMPL( OEM_TEE_AES128CTR_EnforcePolicy( pOemTeeCtx, f_ePolicyCallingAPI, pPolicyRef ) );

    /* Sample protection type must NOT be a handle type */
    DRMCASSERT( !OEM_TEE_AES128CTR_DECRYPTION_MODE_IS_HANDLE_TYPE( OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION ) );

    /* Sample protection type must have a sample protection key available */
    if( pPolicyRef->dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        ChkBOOL( cCDKeys == 3, DRM_E_TEE_ILWALID_KEY_DATA );
    }

    *f_pcKeys = cCDKeys;
    *f_ppKeys = pCDKeys;
    pCDKeys   = NULL;
    *f_pdwDecryptionMode = pPolicyRef->dwDecryptionMode;
    *f_ppContext = f_pContext;

    *f_ppPolicy = pPolicyRef;
    pPolicyRef = NULL;
    pOemPolicy = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pOemPolicy ) );
    return dr;
}

/*
** Synopsis:
**
** This function decrypts content using the decryption mode that was returned from
** DRM_TEE_AES128CTR_PrepareToDecrypt.
**
** This function is called inside:
**  Drm_Reader_DecryptLegacy
**  Drm_Reader_DecryptOpaque
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Retrieve the data required for decryption as follows.
**     a. If you have provided the OEM key information via implementing the
**        OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo, we will first attempt
**        to retrieve the decryption data from the OEM key info by calling
**        OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo.  If you
**        implement this function and successfully return the keys, session ID, and
**        policy, the CDKB will not be parsed and used.  This option can be used
**        if either the OEM key information can be stored in the TEE or the
**        decryption data can be cryptographically protected with better performance
**        by your own implementation.  If you choose this option and the data will
**        not remain in the TEE, you MUST make sure the keys are cryptographically
**        protected and the policy can be validated not to have changed (via
**        cryptographic signature).
**     b. If OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo returns
**        DRM_E_NOTIMPL or the key count returned is zero, the given Content
**        Decryption Key Blob (CDKB) will be parsed and its signature verified.
**  3. If the decryption mode returned from DRM_TEE_AES128CTR_PrepareToDecrypt
**     was equal to OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE,
**     decrypt the given content (using the given region mapping and IV)
**     into a clear buffer and return it via the Clear Content Data (CCD).
**     Otherwise, decrypt the given content (using the given region mapping and IV)
**     into an opaque buffer and return an it via the CCD.
**     The OEM-defined opaque buffer format is specified by the specific
**     OEM-defined decryption mode that was returned
**     (i.e. the OEM may support multiple decryption modes).
**
** Arguments:
**
** f_pContext:                 (in/out) The TEE context returned from
**                                      DRM_TEE_BASE_AllocTEEContext.
**                                      If this parameter is NULL, the CDKB must be
**                                      self-contained, i.e. it must include the
**                                      data required to reconstitute the DRM_TEE_CONTEXT.
** f_pCDKB:                    (in)     The Content Decryption Key Blob (CDKB) returned from
**                                      DRM_TEE_AES128CTR_PrepareToDecrypt.
** f_pEncryptedRegionMapping:  (in)     The encrypted-region mapping.
**
**                                      The number of DRM_DWORDs must be a multiple
**                                      of two.  Each DRM_DWORD pair represents
**                                      values indicating how many clear bytes
**                                      followed by how many encrypted bytes are
**                                      present in the data on output.
**                                      The sum of all of these DRM_DWORD pairs
**                                      must be equal to bytecount(clear content),
**                                      i.e. equal to f_pEncrypted->cb.
**
**                                      If the entire f_pEncrypted is encrypted, there will be
**                                      two DRM_DWORDs with values 0, bytecount(encrypted content).
**                                      If the entire f_pEncrypted is clear, there will be
**                                      two DRM_DWORDs with values bytecount(clear content), 0.
**
**                                      The caller must already be aware of what information
**                                      to pass to this function, as this information is
**                                      format-/codec-specific.
**
** f_ui64Initializatiolwector: (in)     The AES128CTR initialization vector.
** f_pEncrypted:               (in)     The encrypted bytes being decrypted.
** f_pCCD:                     (out)    The Clear Content Data (CCD) which represents the
**                                      clear data that was decrypted by this function.
**                                      This CCD is opaque unless the decryption mode
**                                      returned by DRM_TEE_AES128CTR_PrepareToDecrypt
**                                      was OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE.
**                                      This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_DecryptContent(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMapping,
    __in                        DRM_UINT64                    f_ui64Initializatiolwector,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD )
{
    DRM_RESULT               dr                     = DRM_SUCCESS;
    DRM_RESULT               drTmp                  = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB        oCCD                   = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD                cCDKeys                = 0;
    DRM_TEE_KEY             *pCDKeys                = NULL;
    DRM_TEE_CONTEXT          oTeeCtxReconstituted   = DRM_TEE_CONTEXT_EMPTY;
    DRM_BOOL                 fReconstituted         = FALSE;
    OEM_TEE_CONTEXT         *pOemTeeCtx             = NULL;
    OEM_TEE_MEMORY_HANDLE    hClear                 = OEM_TEE_MEMORY_HANDLE_ILWALID;
    DRM_TEE_POLICY_INFO      oPolicy                = { 0 };
    DRM_TEE_POLICY_INFO     *pPolicyRef             = &oPolicy;
    DRM_DWORD                dwDecryptionMode;      /* Initialized as out param from DRM_TEE_AES128CTR_IMPL_DecryptContentPolicyHelper */

    /* Arguments are checked in the DRM TEE proxy layer */
    DRMASSERT( f_pEncryptedRegionMapping != NULL );
    DRMASSERT( f_pEncrypted != NULL );
    DRMASSERT( f_pCCD != NULL );

    DRMASSERT( IsBlobAssigned( f_pEncrypted ) );

    /* f_pEncryptedRegionMapping must have a multiple of 2 DWORDs, each DWORD pair is a clear length followed by an encrypted length. */
    ChkArg( ( f_pEncryptedRegionMapping->cdwData & 1 ) == 0 );     /* Power of two: use & instead of % for better perf */

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pCCD, sizeof( *f_pCCD ) ) );

    ChkDR( DRM_TEE_AES128CTR_IMPL_DecryptContentPolicyHelper(
        f_pContext,
        &f_pContext,
        &oTeeCtxReconstituted,
        &fReconstituted,
        DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENT,
        f_pCDKB,
        f_pOEMKeyInfo,
        &cCDKeys,
        &pCDKeys,
        &dwDecryptionMode,
        &pPolicyRef ) );

    pOemTeeCtx = &f_pContext->oContext;

    if( OEM_TEE_AES128CTR_DECRYPTION_MODE_IS_HANDLE_TYPE( dwDecryptionMode ) )
    {
        DRM_DWORD cbhClear = 0;
        ChkDR( OEM_TEE_BASE_SelwreMemHandleAlloc( pOemTeeCtx, dwDecryptionMode, f_pEncrypted->cb, &cbhClear, &hClear ) );
        ChkDR( OEM_TEE_AES128CTR_DecryptContentIntoHandle(
            pOemTeeCtx,
            DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENT,
            pPolicyRef,
            &pCDKeys[ 1 ].oKey,
            f_pEncryptedRegionMapping->cdwData,
            f_pEncryptedRegionMapping->pdwData,
            f_ui64Initializatiolwector,
            f_pEncrypted->cb,
            f_pEncrypted->pb,
            hClear ) );

        /* Transfer handle into blob */
        ChkDR( DRM_TEE_BASE_IMPL_AllocBlobAndTakeOwnership( f_pContext, cbhClear, (DRM_BYTE**)&hClear, &oCCD ) );
        oCCD.eType = DRM_TEE_BYTE_BLOB_TYPE_CCD;
        oCCD.dwSubType = OEM_TEE_AES128CTR_DECRYPTION_MODE_HANDLE;
        hClear = OEM_TEE_MEMORY_HANDLE_ILWALID;
    }
    else
    {
        ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pEncrypted->cb, f_pEncrypted->pb, &oCCD ) );
        ChkDR( OEM_TEE_AES128CTR_DecryptContentIntoClear(
            pOemTeeCtx,
            DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENT,
            pPolicyRef,
            &pCDKeys[ 1 ].oKey,
            f_pEncryptedRegionMapping->cdwData,
            f_pEncryptedRegionMapping->pdwData,
            f_ui64Initializatiolwector,
            oCCD.cb,
            (DRM_BYTE*)oCCD.pb ) );
    }

    if( dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        /* 
        ** in case of sample protection, all input content must be encrypted; no sub sample mapping
        ** is supported/necessary
        */
        ChkArg( f_pEncryptedRegionMapping->cdwData == 2 
             && f_pEncryptedRegionMapping->pdwData[0] == 0 
             && f_pEncryptedRegionMapping->pdwData[1] == oCCD.cb );

        ChkDR( DRM_TEE_SAMPLEPROT_IMPL_ApplySampleProtection(
            pOemTeeCtx,
            &pCDKeys[ 2 ].oKey,
            f_ui64Initializatiolwector,
            oCCD.cb,
            (DRM_BYTE*)oCCD.pb ) );
    }

    /* Transfer ownership */
    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pCCD, &oCCD ) );

ErrorExit:
    ChkVOID( DRM_TEE_KB_FreeCDKBCryptoDataRestrictionArray( f_pContext, oPolicy.cRestrictions, oPolicy.pRestrictions ) );
    if( pPolicyRef != &oPolicy )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pPolicyRef ) );
    }

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    if( hClear != OEM_TEE_MEMORY_HANDLE_ILWALID )
    {
        drTmp = OEM_TEE_BASE_SelwreMemHandleFree( pOemTeeCtx, &hClear );
        if (dr == DRM_SUCCESS)
        {
            dr = drTmp;
        }
    }

    drTmp =  DRM_TEE_BASE_FreeBlob( f_pContext, &oCCD );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );

    if( fReconstituted )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&oTeeCtxReconstituted.oContext.pbUnprotectedOEMData ) );
    }

    return dr;
}

/*
** Synopsis:
**
** This function decrypts multiple sets of audio content using the decryption mode
** that was returned from DRM_TEE_AES128CTR_PrepareToDecrypt.
**
** This function is not called from any top-level PK functions.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Retrieve the data required for decryption as in DRM_TEE_AES128CTR_DecryptContent.
**  3. If the decryption mode returned from DRM_TEE_AES128CTR_PrepareToDecrypt
**     was equal to OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE,
**     decrypt the given content (using the given IVs)
**     into a clear buffer and return it via the Clear Content Data (CCD).
**     Otherwise, decrypt the given content (using the given IVs)
**     into an opaque buffer and return an it via the CCD.
**     The OEM-defined opaque buffer format is specified by the specific
**     OEM-defined decryption mode that was returned
**     (i.e. the OEM may support multiple decryption modes).
**     Note that multiple decryptions are performed with different IVs
**     on different subsets of the encrypted passed in.
**
** Arguments:
**
** f_pContext:                  (in/out) The TEE context returned from
**                                       DRM_TEE_BASE_AllocTEEContext.
**                                       If this parameter is NULL, the CDKB must be
**                                       self-contained, i.e. it must include the
**                                       data required to reconstitute the DRM_TEE_CONTEXT.
** f_pCDKB:                         (in) The Content Decryption Key Blob (CDKB) returned from
**                                       DRM_TEE_AES128CTR_PrepareToDecrypt.
** f_pInitializatiolwectors:        (in) The AES128CTR initialization vectors for each
**                                       subset of content indicated by f_pInitializatiolwectorSizes.
** f_pInitializatiolwectorSizes:    (in) Each DWORD in this list represents the number of bytes
**                                       that are encrypted with the corresponding IV in
**                                       f_pInitializatiolwectors.
**                                       The IV in f_pInitializatiolwectors[0] is used to
**                                       decrypt the first f_pInitializatiolwectorSizes[0] bytes,
**                                       The IV in f_pInitializatiolwectors[1] is used to
**                                       decrypt the *next* f_pInitializatiolwectorSizes[1] bytes,
**                                       and so forth.
** f_pEncrypted:                    (in) The encrypted bytes being decrypted.
**                                       Note that this represents the concatenation of multiple sets
**                                       of content with different IVs.
**                                       The number of entries in this array MUST match the
**                                       number of bytes in the sum over f_pInitializatiolwectorSizes.
** f_pCCD:                         (out) The Clear Content Data (CCD) which represents the
**                                       clear data that was decrypted by this function.
**                                       This CCD is opaque unless the decryption mode
**                                       returned by DRM_TEE_AES128CTR_PrepareToDecrypt
**                                       was OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE.
**                                       This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CTR_DecryptAudioContentMultiple(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_QWORDLIST            *f_pInitializatiolwectors,
    __in                  const DRM_TEE_DWORDLIST            *f_pInitializatiolwectorSizes,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD )
{
    DRM_RESULT               dr                     = DRM_SUCCESS;
    DRM_RESULT               drTmp                  = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB        oCCD                   = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD                cCDKeys                = 0;
    DRM_TEE_KEY             *pCDKeys                = NULL;
    DRM_TEE_CONTEXT          oTeeCtxReconstituted   = DRM_TEE_CONTEXT_EMPTY;
    DRM_BOOL                 fReconstituted         = FALSE;
    OEM_TEE_CONTEXT         *pOemTeeCtx             = NULL;
    DRM_TEE_POLICY_INFO      oPolicy                = { 0 };
    DRM_TEE_POLICY_INFO     *pPolicyRef             = &oPolicy;
    DRM_DWORD                dwDecryptionMode;      /* Initialized as out param from DRM_TEE_AES128CTR_IMPL_DecryptContentPolicyHelper */
    DRM_DWORD                iIV;                   /* Loop variable */
    DRM_BYTE                *pbNextEncrypted        = NULL;

    /* Arguments are checked in the DRM TEE proxy layer */
    DRMASSERT( f_pInitializatiolwectors != NULL );
    DRMASSERT( f_pInitializatiolwectorSizes != NULL );
    DRMASSERT( f_pEncrypted != NULL );
    DRMASSERT( f_pCCD != NULL );

    DRMASSERT( IsBlobAssigned( f_pEncrypted ) );

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pCCD, sizeof( *f_pCCD ) ) );

    /* Each vector must have a size, i.e. there's a strict 1:1 correspondence. */
    ChkArg( f_pInitializatiolwectors->cqwData == f_pInitializatiolwectorSizes->cdwData );

    /* All vector sizes must sum to the size of the encrypted buffer */
    {
        DRM_DWORD cbIVs = 0;

        for( iIV = 0; iIV < f_pInitializatiolwectorSizes->cdwData; iIV++ )
        {
            ChkArg( f_pInitializatiolwectorSizes->pdwData[ iIV ] > 0 );
            ChkDR( DRM_DWordAddSame( &cbIVs, f_pInitializatiolwectorSizes->pdwData[ iIV ] ) );
        }
        ChkArg( cbIVs == f_pEncrypted->cb );
    }

    ChkDR( DRM_TEE_AES128CTR_IMPL_DecryptContentPolicyHelper(
        f_pContext,
        &f_pContext,
        &oTeeCtxReconstituted,
        &fReconstituted,
        DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTAUDIOCONTENTMULTIPLE,
        f_pCDKB,
        f_pOEMKeyInfo,
        &cCDKeys,
        &pCDKeys,
        &dwDecryptionMode,
        &pPolicyRef ) );

    pOemTeeCtx = &f_pContext->oContext;

    /* Allocate the output buffer based on type */
    if( OEM_TEE_AES128CTR_DECRYPTION_MODE_IS_HANDLE_TYPE( dwDecryptionMode ) )
    {
        ChkDR( DRM_E_NOTIMPL );
    }
    else
    {
        ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pEncrypted->cb, f_pEncrypted->pb, &oCCD ) );
        pbNextEncrypted = (DRM_BYTE*)oCCD.pb;   /* cast away const */
    }

    for( iIV = 0; iIV < f_pInitializatiolwectors->cqwData; iIV++ )
    {
        DRM_UINT64   ui64NextIV             = f_pInitializatiolwectors->pqwData[ iIV ];
        DRM_DWORD    cbNextEncrypted        = f_pInitializatiolwectorSizes->pdwData[ iIV ];

        /* All audio bytes are always encrypted (i.e. clear byte count == 0 and encrypted byte count == all bytes) */
        DRM_DWORD    rgdwNextMapping[ 2 ];
        rgdwNextMapping[ 0 ] = 0;
        rgdwNextMapping[ 1 ] = f_pInitializatiolwectorSizes->pdwData[ iIV ];

        ChkDR( OEM_TEE_AES128CTR_DecryptContentIntoClear(
            pOemTeeCtx,
            DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTAUDIOCONTENTMULTIPLE,
            pPolicyRef,
            &pCDKeys[ 1 ].oKey,
            DRM_NO_OF( rgdwNextMapping ),
            rgdwNextMapping,
            ui64NextIV,
            cbNextEncrypted,
            pbNextEncrypted ) );

        if( dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
        {
            ChkDR( DRM_TEE_SAMPLEPROT_IMPL_ApplySampleProtection(
                pOemTeeCtx,
                &pCDKeys[ 2 ].oKey,
                ui64NextIV,
                cbNextEncrypted,
                pbNextEncrypted ) );
        }

        /* Move on to the next set of encrypted bytes */
        pbNextEncrypted += cbNextEncrypted;
    }

    /* Transfer ownership */
    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pCCD, &oCCD ) );

ErrorExit:
    ChkVOID( DRM_TEE_KB_FreeCDKBCryptoDataRestrictionArray( f_pContext, oPolicy.cRestrictions, oPolicy.pRestrictions ) );
    if( pPolicyRef != &oPolicy )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pPolicyRef ) );
    }
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_BASE_FreeBlob( f_pContext, &oCCD );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );

    if( fReconstituted )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&oTeeCtxReconstituted.oContext.pbUnprotectedOEMData ) );
    }

    return dr;
}

/*
** Synopsis:
**
** This function parses and validates the external policy cryptographic information created
** by the function OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo and returns the content
** keys, session ID, and policy information associated with the external policy.
**
** For debug builds only, this function also validates the OEM key information provided by
** function OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo against the Content
** Decryption Key Blob (CDKB).
**
** This function is called inside:
**  DRM_TEE_AES128CTR_DecryptContent
**  DRM_TEE_H264_PreProcessEncryptedData
**
** Operations Performed:
**
**  1. Parse and validate the OEM Key Info blob.
**  2. Validate the session ID for non-reconstituted sessions.
**  3. For debug builds only, validate the OEM content decryption data.
**     a. Parse and validate the CDKB.
**     b. Validate the provided session ID against the session ID stored in the CDKB.
**     c. Validate the provided policy information against the policy inside the CDKB.
**  4. Return the content keys, session ID and policy information.
**
** Arguments:
**
**  f_pContext:         (in/out)    The TEE context returned from DRM_TEE_BASE_AllocTEEContext.
**  f_pCDKB:            (in)        Optional. The Content Decryption Key Blob.
**  f_pOEMKeyInfo:      (in)        The OEM-specific Key Information blob.
**  f_fIsReconstituted: (in)        Indicates whether the TEE context (f_pContext) was reconstituted.
**  f_pcKeys:           (out)       The number of keys in parameter f_ppKeys.
**  f_ppKeys:           (out)       An array of the content decryption keys (integrity, encryption,
**                                  and sample protection - in order).  The caller expects at least
**                                  the first two keys to be in the array.  However, the first key
**                                  (content integrity) is not used, and therefore does not need to
**                                  be set.  It is only added to the array to maintain compatibility
**                                  with the CDKB key array.
**  f_ppPolicyInfo:     (out)       The policy information associated with the provided OEM key
**                                  info blob (f_pOEMKeyInfo).
**  f_pSessionID:       (out)       Optional. The session ID for the associated with the provided
**                                  OEM key info blob (f_pOEMKeyInfo).
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_TEE_AES128CTR_IMPL_ParseAndVerifyExternalPolicyCryptoInfo(
    __inout            DRM_TEE_CONTEXT          *f_pContext,
    __in_opt     const DRM_TEE_BYTE_BLOB        *f_pCDKB,
    __in         const DRM_TEE_BYTE_BLOB        *f_pOEMKeyInfo,
    __in               DRM_BOOL                  f_fIsReconstituted,
    __out              DRM_DWORD                *f_pcKeys,
    __deref_out        DRM_TEE_KEY             **f_ppKeys,
    __deref_out        DRM_TEE_POLICY_INFO     **f_ppPolicyInfo,
    __out_opt          DRM_ID                   *f_pSessionID )
{
    DRM_RESULT           dr              = DRM_SUCCESS;
    DRM_DWORD            cKeys           = 0;
    DRM_TEE_KEY         *pKeys           = NULL;
    DRM_ID               idSession;      /* Initialized in OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo */
    DRM_TEE_POLICY_INFO *pOemPolicy      = NULL;
    OEM_TEE_CONTEXT     *pOemTeeCtx      = NULL;
#if DRM_DBG
    DRM_TEE_POLICY_INFO  oPolicyFromCDKB = {0};
    DRM_DWORD            cCDKeysFromCDKB = 0;
    DRM_TEE_KEY         *pCDKeysFromCDKB = NULL;
    DRM_TEE_CONTEXT      oTeeCtx         = DRM_TEE_CONTEXT_EMPTY;
#endif /* DRM_DBG */

    ChkArg( f_pContext     != NULL );
    ChkArg( f_pcKeys       != NULL );
    ChkArg( f_ppKeys       != NULL );
    ChkArg( f_ppPolicyInfo != NULL );

    /* If the OEM Key info paramter is NULL or empty the caller will fall back to using CDKB if we return DRM_E_NOTIMPL. */
    ChkBOOL( f_pOEMKeyInfo != NULL          , DRM_E_NOTIMPL );
    ChkBOOL( IsBlobAssigned( f_pOEMKeyInfo ), DRM_E_NOTIMPL );

    *f_pcKeys       = 0;
    *f_ppKeys       = NULL;
    *f_ppPolicyInfo = NULL;

    pOemTeeCtx = &f_pContext->oContext;

    /* Allocate the maximum set of keys for content decryption (CI, CK, and sample protection) */
    cKeys = 3;
    ChkDR( DRM_TEE_BASE_IMPL_AllocateKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &pKeys[ 1 ] ) );
    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_SAMPLEPROT, &pKeys[ 2 ] ) );

    ChkDR( OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo(
        pOemTeeCtx,
        f_pOEMKeyInfo->cb,
        f_pOEMKeyInfo->pb,
        &pOemPolicy,
        &idSession,
        &pKeys[1].oKey,
        &pKeys[2].oKey ) );

    if( pOemPolicy->dwDecryptionMode != OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        /*
        ** The following code will not affect DRM_TEE_BASE_IMPL_FreeKeyArray since the
        ** sample protection key is the last item in the array and is released here. It
        ** will simple cause DRM_TEE_BASE_IMPL_FreeKeyArray to skip the last key (which
        ** is already cleaned up here).
        */
        cKeys = 2;
        ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &pKeys[2].oKey ) );
    }

    if( !f_fIsReconstituted )
    {
        ChkBOOL( OEM_SELWRE_ARE_EQUAL( &idSession, &f_pContext->idSession, sizeof(DRM_ID) ), DRM_E_TEE_ILWALID_KEY_DATA );
    }

/*
** For debug builds only, if we are provided a valid OEM Key Info blob we will validate it
** against the CDKB to verify consistency.  This will provide debug-only assurance that
** the OEM's implementation of OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo is
** correct.
*/
#if DRM_DBG
    if( f_pCDKB != NULL && IsBlobAssigned( f_pCDKB ) )
    {
        ChkDR( DRM_TEE_KB_ParseAndVerifyCDKB(
            f_fIsReconstituted ? &oTeeCtx : f_pContext,
            f_pCDKB,
            FALSE,
            &cCDKeysFromCDKB,
            &pCDKeysFromCDKB,
            &oPolicyFromCDKB ) );

        AssertChkArg( cCDKeysFromCDKB == 2 || cCDKeysFromCDKB == 3 );

        if( f_fIsReconstituted )
        {
            AssertChkArg( OEM_SELWRE_ARE_EQUAL( &f_pContext->idSession, &oTeeCtx.idSession, sizeof(DRM_ID) ) );
        }

        /* Validate the policy information */
        AssertChkArg( OEM_SELWRE_ARE_EQUAL( pOemPolicy, &oPolicyFromCDKB, sizeof( *pOemPolicy ) - sizeof( pOemPolicy->pRestrictions ) ) );
        AssertChkArg( pOemPolicy->cRestrictions == oPolicyFromCDKB.cRestrictions );

        {
            DRM_DWORD idxRestriction; /* Loop variable */

            for( idxRestriction = 0; idxRestriction < oPolicyFromCDKB.cRestrictions; idxRestriction++ )
            {
                const DRM_TEE_POLICY_RESTRICTION *pRestriction         = &pOemPolicy->pRestrictions[idxRestriction];
                const DRM_TEE_POLICY_RESTRICTION *pRestrictionFromCDKB = &oPolicyFromCDKB.pRestrictions[idxRestriction];

                AssertChkArg( OEM_SELWRE_ARE_EQUAL( pRestriction, pRestrictionFromCDKB, sizeof( DRM_TEE_POLICY_RESTRICTION ) - sizeof( pRestrictionFromCDKB->pbRestrictionConfiguration ) ) );
                AssertChkArg( pRestriction->cbRestrictionConfiguration == pRestrictionFromCDKB->cbRestrictionConfiguration );
                AssertChkArg( OEM_SELWRE_ARE_EQUAL( pRestriction->pbRestrictionConfiguration, pRestrictionFromCDKB->pbRestrictionConfiguration, pRestriction->cbRestrictionConfiguration ) );
            }
        }
    }
#endif /* DRM_DBG */

    *f_pcKeys = cKeys;
    *f_ppKeys = pKeys;
    cKeys     = 0;
    pKeys     = NULL;

    if( f_pSessionID != NULL )
    {
        *f_pSessionID = idSession;
    }
    *f_ppPolicyInfo = pOemPolicy;
    pOemPolicy      = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pOemPolicy ) );

#if DRM_DBG
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cCDKeysFromCDKB, &pCDKeysFromCDKB ) );
    ChkVOID( DRM_TEE_KB_FreeCDKBCryptoDataRestrictionArray(
        f_pContext,
        oPolicyFromCDKB.cRestrictions,
        oPolicyFromCDKB.pRestrictions ) );
#endif /* DRM_DBG */

    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;
