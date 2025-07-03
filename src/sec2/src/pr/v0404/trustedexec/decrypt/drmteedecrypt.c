/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmteesupported.h>
#include <drmteexb.h>
#include <drmteecache.h>
#include <oembyteorder.h>
#include <drmteedecryptinternal.h>
#include <drmteelicprepinternal.h>
#include <drmteeselwrestop2internal.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_DECRYPT_IsDECRYPTSupported( DRM_VOID )
{
    return TRUE;
}

/*
** Global PlayReady policy decision: Any license with the Play Right is acceptable to use for Miracast.
*/
#define DRM_TEE_IMPL_DECRYPT_ENABLE_FIXED_PLAY_ENABLERS( __dwPlayEnablers ) DRM_DO { ( __dwPlayEnablers ) |= DRM_TEE_PLAY_ENABLER_TYPE_HDCP_MIRACAST; } DRM_WHILE_FALSE

DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerUnknownOutput, 0x786627D8, 0xC2A6, 0x44BE, 0x8F, 0x88, 0x08, 0xAE, 0x25, 0x5B, 0x01, 0xA7 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerUnknownOutputConstrained, 0xB621D91F, 0xEDCC, 0x4035, 0x8D, 0x4B, 0xDC, 0x71, 0x76, 0x0D, 0x43, 0xE9 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerDTCP, 0xD685030B, 0x0F4F, 0x43A6, 0xBB, 0xAD, 0x35, 0x6F, 0x1E, 0xA0, 0x04, 0x9A );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerHelix, 0x002F9772, 0x38A0, 0x43E5, 0x9F, 0x79, 0x0F, 0x63, 0x61, 0xDC, 0xC6, 0x2A );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerHDCPMiracast, 0xA340C256, 0x0941, 0x4D4C, 0xAD, 0x1D, 0x0B, 0x67, 0x35, 0xC0, 0xCB, 0x24 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerHDCPWiVu, 0x1B4542E3, 0xB5CF, 0x4C99, 0xB3, 0xBA, 0x82, 0x9A, 0xF4, 0x6C, 0x92, 0xF8 );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerAirPlay, 0x5ABF0F0D, 0xDC29, 0x4B82, 0x99, 0x82, 0xFD, 0x8E, 0x57, 0x52, 0x5B, 0xFC );
DRM_DEFINE_LOCAL_GUID( g_guidPlayEnablerFrameServer, 0xAE092501, 0xA9E3, 0x46f6, 0xAF, 0xBE, 0x62, 0x85, 0x77, 0xDC, 0xDF, 0x55 );

//PRAGMA_DIAG_OFF(attributes, "Turning warning off for GCC because it thinks these functions might not be able to be inlined for free builds..")
static DRM_RESULT DRM_CALL _DecryptScalableLicenseKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_XMRFORMAT                *f_pLeaf,
    __in                  const DRM_XMRFORMAT                *f_pRoot,
    __in_ecount(2)        const DRM_TEE_KEY                  *f_pEscrowedKeys,
    __out_ecount(2)             DRM_TEE_KEY                  *f_pLeafKeys );
static DRM_RESULT DRM_CALL _DecryptScalableLicenseKey(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_XMRFORMAT                *f_pLeaf,
    __in                  const DRM_XMRFORMAT                *f_pRoot,
    __in_ecount( 2 )      const DRM_TEE_KEY                  *f_pEscrowedKeys,
    __out_ecount( 2 )           DRM_TEE_KEY                  *f_pLeafKeys )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRMASSERT( f_pLeaf != NULL );
    DRMASSERT( f_pRoot != NULL );

    /* Ensure that this is a scalable chained license */
    ChkArg( XMRFORMAT_IS_UPLINKX_VALID( f_pLeaf ) );
    ChkArg( XMRFORMAT_IS_AUX_KEY_VALID( f_pRoot ) );

    ChkDR( DRM_TEE_IMPL_LICPREP_DecryptScalableLicenseKeyFromKeyMaterial(
        f_pContext,
        &f_pLeaf->OuterContainer.KeyMaterialContainer,
        &f_pRoot->OuterContainer.KeyMaterialContainer,
        f_pEscrowedKeys,
        f_pLeafKeys ) );

ErrorExit:
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

    ChkDR( DRM_TEE_IMPL_BASE_ParseLicenseAlloc( f_pContext, cbLeaf, f_pLicenses->pb, f_pLeaf ) );

    if( f_pLicenses->cb > cbLeaf )
    {
        /* Remaining data must be root license */
        ChkDR( DRM_DWordSub( f_pLicenses->cb, cbLeaf, &cbRoot ) );
        ChkDR( DRM_TEE_IMPL_BASE_ParseLicenseAlloc( f_pContext, cbRoot, f_pLicenses->pb + cbLeaf, f_pRoot ) );  /* Can't overflow: already verified that f_pLicenses->cb > cbLeaf */
    }

ErrorExit:
    return dr;
}

#define _FreeLicenseChain( __pOemTeeCtx, __pLeaf, __pRoot )  DRM_DO {               \
    ChkVOID( DRM_TEE_IMPL_BASE_ParsedLicenseFree( (__pOemTeeCtx), (__pLeaf) ) );    \
    ChkVOID( DRM_TEE_IMPL_BASE_ParsedLicenseFree( (__pOemTeeCtx), (__pRoot) ) );    \
} DRM_WHILE_FALSE

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
    __in_ecount( 2 )       const DRM_TEE_KEY            *f_pEscrowedKeys,
    __inout_ecount( 2 )          DRM_TEE_KEY            *f_pLeafKeys );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _ValidateLicenseChain(
    __inout                      DRM_TEE_CONTEXT        *f_pContext,
    __in                   const DRM_TEE_BYTE_BLOB      *f_pChecksum,
    __in                         DRM_BOOL                f_fEnforceRIVs,
    __in                         DRM_DWORD               f_dwRKBRiv,
    __in                         DRM_TEE_XMR_LICENSE    *f_pLeaf,
    __in                         DRM_TEE_XMR_LICENSE    *f_pRoot,
    __in_ecount( 2 )       const DRM_TEE_KEY            *f_pEscrowedKeys,
    __inout_ecount( 2 )          DRM_TEE_KEY            *f_pLeafKeys )
{
    DRM_RESULT       dr             = DRM_SUCCESS;
    OEM_TEE_CONTEXT *pOemTeeCtx     = NULL;
    DRM_BOOL         fDerivedLeaf   = FALSE;

    DRMASSERT( f_pChecksum != NULL );
    DRMASSERT( f_pLeaf != NULL );
    DRMASSERT( f_pRoot != NULL );
    DRMASSERT( f_pEscrowedKeys != NULL );
    DRMASSERT( f_pLeafKeys != NULL );
    DRMASSERT( f_pContext != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    if( f_pRoot->fValid )
    {
        DRMASSERT( f_pLeafKeys[ 0 ].eType == DRM_TEE_KEY_TYPE_PR_CI );
        DRMASSERT( f_pLeafKeys[ 1 ].eType == DRM_TEE_KEY_TYPE_PR_CK );

        /* Verify root license signature */
        ChkDR( DRM_TEE_IMPL_BASE_CheckLicenseSignature( f_pContext, &f_pRoot->oLicense, f_pEscrowedKeys ) );

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
            ChkDR( OEM_TEE_DECRYPT_VerifyLicenseChecksum(
                pOemTeeCtx,
                &f_pLeafKeys[ 1 ].oKey, /* CK is always second */
                f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.guidKeyID.rgb,
                XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkX.xbbaChecksum ),
                XBBA_TO_CB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkX.xbbaChecksum ) ) );
        }
        else
        {
            /* Decrypt leaf license key with root content key: we already verified that the license has a content key */
            DRMASSERT( XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ) != NULL );
            __analysis_assume( XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ) != NULL );
            ChkDR( OEM_TEE_DECRYPT_DecryptContentKeysWithLicenseKey(
                pOemTeeCtx,
                &f_pEscrowedKeys[ 1 ].oKey, /* CK is always second */
                XBBA_TO_CB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ),
                XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ),
                &f_pLeafKeys[ 0 ].oKey,     /* CI is always first */
                &f_pLeafKeys[ 1 ].oKey ) ); /* CK is always second */

            /* Verify chained checksum */
            ChkDR( OEM_TEE_DECRYPT_VerifyLicenseChecksum(
                pOemTeeCtx,
                &f_pEscrowedKeys[ 1 ].oKey, /* CK is always second */
                f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkKID.idUplinkKID.rgb,
                XBBA_TO_PB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkKID.xbbaChainedChecksum ),
                XBBA_TO_CB( f_pLeaf->oLicense.OuterContainer.KeyMaterialContainer.UplinkKID.xbbaChainedChecksum ) ) );
        }
    }

    /* Verify leaf license signature */
    ChkDR( DRM_TEE_IMPL_BASE_CheckLicenseSignature( f_pContext, &f_pLeaf->oLicense, f_pRoot->fValid ? f_pLeafKeys : f_pEscrowedKeys ) );

    if( !fDerivedLeaf && f_pChecksum->cb > 0 )
    {
        /* Verify content header checksum */
        ChkDR( OEM_TEE_DECRYPT_VerifyLicenseChecksum(
            pOemTeeCtx,
            f_pRoot->fValid ? &f_pLeafKeys[ 1 ].oKey : &f_pEscrowedKeys[ 1 ].oKey, /* CK is always second */
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
    if( XMRFORMAT_IS_PLAY_VALID( &f_pLeaf->oLicense ) )
    {
        ChkDR( _ValidateExtensibleRestrictionObjects( f_pLeaf->oLicense.OuterContainer.PlaybackPolicyContainer.pUnknownObjects ) );
        ChkDR( _ValidateExtensibleRestrictionContainers( &f_pLeaf->oLicense.OuterContainer.PlaybackPolicyContainer.UnknownContainer ) );
    }

    if( f_pRoot->fValid && XMRFORMAT_IS_PLAY_VALID( &f_pRoot->oLicense ) )
    {
        ChkDR( _ValidateExtensibleRestrictionObjects( f_pRoot->oLicense.OuterContainer.PlaybackPolicyContainer.pUnknownObjects ) );
        ChkDR( _ValidateExtensibleRestrictionContainers( &f_pRoot->oLicense.OuterContainer.PlaybackPolicyContainer.UnknownContainer ) );
    }

    /*
    ** Validate there are no MUST-UNDERSTAND extensible restrictions for global license policy
    */
    ChkDR( _ValidateExtensibleRestrictionObjects( f_pLeaf->oLicense.OuterContainer.GlobalPolicyContainer.pUnknownObjects ) );
    ChkDR( _ValidateExtensibleRestrictionContainers( &f_pLeaf->oLicense.OuterContainer.UnknownContainer ) );

    if( f_pRoot->fValid )
    {
        ChkDR( _ValidateExtensibleRestrictionObjects( f_pRoot->oLicense.OuterContainer.GlobalPolicyContainer.pUnknownObjects ) );
        ChkDR( _ValidateExtensibleRestrictionContainers( &f_pRoot->oLicense.OuterContainer.UnknownContainer ) );
    }

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

    switch( f_pLicense->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType )
    {
    case XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CTR:
        ChkBOOL( DRM_TEE_IMPL_AES128CTR_IsAES128CTRSupported(), DRM_E_ILWALID_LICENSE );
        break;
    case XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CBC:
        ChkBOOL( DRM_TEE_IMPL_AES128CBC_IsAES128CBCSupported(), DRM_E_ILWALID_LICENSE );
        break;
    default:
        ChkDR( DRM_E_ILWALID_LICENSE );
        break;
    }

ErrorExit:
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ValidateDecryptionMode(
    __inout               OEM_TEE_CONTEXT              *f_pOemTeeCtx,
    __in                  DRM_DWORD                     f_dwRequestedDecryptionMode,
    __in                  DRM_WORD                      f_wSelwrityLevel );
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ValidateDecryptionMode(
    __inout               OEM_TEE_CONTEXT              *f_pOemTeeCtx,
    __in                  DRM_DWORD                     f_dwRequestedDecryptionMode,
    __in                  DRM_WORD                      f_wSelwrityLevel )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkBOOL( DRM_TEE_IMPL_SAMPLEPROT_IsDecryptionModeSupported( f_pOemTeeCtx, f_dwRequestedDecryptionMode ), DRM_E_RIGHTS_NOT_AVAILABLE );

    if( f_dwRequestedDecryptionMode == OEM_TEE_DECRYPTION_MODE_NOT_SELWRE )
    {
        ChkBOOL( f_wSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000, DRM_E_RIGHTS_NOT_AVAILABLE );
    }
    else if( f_dwRequestedDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        ChkBOOL( f_wSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000, DRM_E_RIGHTS_NOT_AVAILABLE );
    }
    else if( f_dwRequestedDecryptionMode != OEM_TEE_DECRYPTION_MODE_HANDLE )
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

        /* Because we've verified leafSL <= rootSL, this ensures that we are using DRM_MAX( leafSL, rootSL ) */
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
    else if( f_pLeaf->fValid && XMRFORMAT_IS_OPL_VALID( &f_pLeaf->oLicense ) )
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

static DRM_GLOBAL_CONST void * s_rpvComparisonGuids[]  PR_ATTR_DATA_OVLY(_s_rpvComparisonGuids) =
{
    &g_guidPlayEnablerUnknownOutput,
    &g_guidPlayEnablerUnknownOutputConstrained,
    &g_guidPlayEnablerDTCP,
    &g_guidPlayEnablerHelix,
    &g_guidPlayEnablerHDCPMiracast,
    &g_guidPlayEnablerHDCPWiVu,
    &g_guidPlayEnablerAirPlay,
    &g_guidPlayEnablerFrameServer,
};
static DRM_GLOBAL_CONST DRM_DWORD s_rdwTypeMasks[] PR_ATTR_DATA_OVLY(_s_rdwTypeMasks) =
{
    DRM_TEE_PLAY_ENABLER_TYPE_UNKNOWN_OUTPUT,
    DRM_TEE_PLAY_ENABLER_TYPE_UNKNOWN_OUTPUT_CONSTRAINED,
    DRM_TEE_PLAY_ENABLER_TYPE_DTCP,
    DRM_TEE_PLAY_ENABLER_TYPE_HELIX,
    DRM_TEE_PLAY_ENABLER_TYPE_HDCP_MIRACAST,
    DRM_TEE_PLAY_ENABLER_TYPE_HDCP_WIVU,
    DRM_TEE_PLAY_ENABLER_TYPE_AIRPLAY,
    DRM_TEE_PLAY_ENABLER_TYPE_FRAME_SERVER,
};
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
        if( pContainer->wType == DRM_XMRFORMAT_ID_PLAY_ENABLER_CONTAINER )
        {
            for( pObject = pContainer->pObject; pObject != NULL; pObject = pObject->pNext )
            {
                if( pObject->wType == DRM_XMRFORMAT_ID_PLAY_ENABLER_OBJECT )
                {
                    const void *pComparison = pObject->pbBuffer + pObject->ibData;      /* Can't overflow: guaranteed by XMR parser */
                    DRM_DWORD idx;

                    DRMCASSERT( DRM_NO_OF( s_rdwTypeMasks ) == DRM_NO_OF( s_rpvComparisonGuids ) );
                    ChkBOOL( pObject->cbData == sizeof(DRM_GUID), DRM_E_ILWALID_LICENSE );
                    for (idx = 0; idx < DRM_NO_OF( s_rdwTypeMasks ); idx++ )
                    {
                        if( OEM_SELWRE_ARE_EQUAL( pComparison, s_rpvComparisonGuids[idx], sizeof(DRM_GUID) ) )
                        {
                            dwPlayEnablers |= s_rdwTypeMasks[idx];
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

    /*
    ** Enable fixed play enablers (when parsing policy from license).
    ** This ensures that the OEM blob created by BuildExternalPolicyCryptoInfo
    ** knows that Miracast is enabled in case they parse / use that OEM blob
    ** in external code outside the PlayReady TEE's parsing using
    ** the OEM function ParseAndVerifyExternalPolicyCryptoInfo.
    */
    ChkVOID( DRM_TEE_IMPL_DECRYPT_ENABLE_FIXED_PLAY_ENABLERS( dwPlayEnablers ) );

    *f_pdwPlayEnablers = dwPlayEnablers;

ErrorExit:
    return dr;
}

#define DRM_LICENSE_AND_RESTRICTION_INDEX_TO_RESTRICTION_PTR( __pLicense, __iRestriction, __pRestriction ) DRM_DO {                                 \
    switch( __iRestriction )                                                                                                                        \
    {                                                                                                                                               \
    case 0: ( __pRestriction ) = ( __pLicense )->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitAnalogVideoProtection.pOPL; break;         \
    case 1: ( __pRestriction ) = ( __pLicense )->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalAudioProtection.pOPL; break;        \
    case 2: ( __pRestriction ) = ( __pLicense )->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalVideoProtection.pOPL; break;        \
    }                                                                                                                                               \
} DRM_WHILE_FALSE

static DRM_NO_INLINE DRM_RESULT DRM_CALL _DetermineLicenseRestrictionsSize(
    __in  const DRM_TEE_XMR_LICENSE  *f_pLeafLicense,
    __in        DRM_DWORD             f_cRestrictions,
    __out       DRM_DWORD            *f_pcbRestrictions )
{
    DRM_RESULT dr             = DRM_SUCCESS;
    DRM_DWORD  cbRestrictions = 0;

    if( f_cRestrictions > 0 )
    {
        /* Total restrictions size will be the size of each restriction structure including its configuration data */
        DRM_DWORD cRestrictionsCounted = 0;
        DRM_DWORD iRestriction         = 0;

        /* We can start with the size of all the structures without their configuration data rather than adding one at a time */
        ChkDR( DRM_DWordMult( f_cRestrictions, sizeof( DRM_TEE_POLICY_RESTRICTION ), &cbRestrictions ) );

        for( iRestriction = 0; iRestriction < 3; iRestriction++ )
        {
            const DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION *pRestriction   = NULL;
            const DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION *pConfiguration = NULL;

            DRM_LICENSE_AND_RESTRICTION_INDEX_TO_RESTRICTION_PTR( f_pLeafLicense, iRestriction, pRestriction );

            for( pConfiguration = pRestriction; pConfiguration != NULL; pConfiguration = pConfiguration->pNext )
            {
                ChkBOOL( cRestrictionsCounted <= f_cRestrictions, DRM_E_BUFFER_BOUNDS_EXCEEDED );

                /* Add in the next restriction's configuration data size */
                ChkDR( DRM_DWordAddSame( &cbRestrictions, XBBA_TO_CB( pConfiguration->xbbaConfig ) ) );
                cRestrictionsCounted++;
            }
        }

        ChkBOOL( cRestrictionsCounted == f_cRestrictions, DRM_E_BUFFER_BOUNDS_EXCEEDED );
    }

    *f_pcbRestrictions = cbRestrictions;

ErrorExit:
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicenseRestrictions(
    __in     const DRM_TEE_XMR_LICENSE        *f_pLeafLicense,
    __in           DRM_DWORD                   f_cbRestrictions,
    __inout        DRM_TEE_POLICY_INFO        *f_pPolicy )
{
    DRM_RESULT dr = DRM_SUCCESS;

    if( f_pPolicy->cRestrictions > 0 )
    {
        DRM_DWORD  cRestrictionsCounted    = 0;
        DRM_BYTE  *pbNextPolicyRestriction = NULL;
        DRM_DWORD  iRestriction            = 0;

#if DRM_DBG
        /* Used to verify we didn't overflow what we allocated */
        DRM_BYTE *pbPolicyRestrictionMax = NULL;
#endif /* DRM_DBG */

        /* The first restriction is located at the end of pPolicy */
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)f_pPolicy, sizeof( *f_pPolicy ), (DRM_SIZE_T*)&pbNextPolicyRestriction ) );

#if DRM_DBG
        /* Used to verify we allocated exactly the right amount of space */
        ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pbNextPolicyRestriction, f_cbRestrictions, (DRM_SIZE_T*)&pbPolicyRestrictionMax ) );
#endif /* DRM_DBG */

        for( iRestriction = 0; iRestriction < 3; iRestriction++ )
        {
            const DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION *pRestriction   = NULL;
            const DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION *pConfiguration = NULL;

            DRM_LICENSE_AND_RESTRICTION_INDEX_TO_RESTRICTION_PTR( f_pLeafLicense, iRestriction, pRestriction );

            for( pConfiguration = pRestriction; pConfiguration != NULL; pConfiguration = pConfiguration->pNext )
            {
                DRM_TEE_POLICY_RESTRICTION  oTeeRestriction = { 0 };

                /*
                ** Loop ilwariant: At start of each loop, pbNextPolicyRestriction points to the offset into pPolicy where
                ** next to copy a DRM_TEE_POLICY_RESTRICTION structure followed by its corresponding configuration data
                */
                DRM_DWORD cbRestrictionConfiguration = XBBA_TO_CB( pConfiguration->xbbaConfig );

                ChkBOOL( cRestrictionsCounted <= f_pPolicy->cRestrictions, DRM_E_BUFFER_BOUNDS_EXCEEDED );

                /* Total size of the restriction equals base size plus any configuration data */
                ChkDR( DRM_DWordAdd( sizeof( oTeeRestriction ), cbRestrictionConfiguration, &oTeeRestriction.cbThis ) );

                switch( iRestriction )
                {
                case 0: oTeeRestriction.wRestrictionCategory = (DRM_WORD)DRM_XMRFORMAT_ID_EXPLICIT_ANALOG_VIDEO_OUTPUT_PROTECTION_CONTAINER; break;
                case 1: oTeeRestriction.wRestrictionCategory = (DRM_WORD)DRM_XMRFORMAT_ID_EXPLICIT_DIGITAL_AUDIO_OUTPUT_PROTECTION_CONTAINER; break;
                case 2: oTeeRestriction.wRestrictionCategory = (DRM_WORD)DRM_XMRFORMAT_ID_EXPLICIT_DIGITAL_VIDEO_OUTPUT_PROTECTION_CONTAINER; break;
                }

                oTeeRestriction.idRestrictionType = pConfiguration->idOPL;
                oTeeRestriction.cbRestrictionConfiguration = cbRestrictionConfiguration;

                ChkVOID( OEM_TEE_MEMCPY( pbNextPolicyRestriction, &oTeeRestriction, sizeof( oTeeRestriction ) ) );

                /* Move past this restriction's fixed-size data */
                ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pbNextPolicyRestriction, sizeof( DRM_TEE_POLICY_RESTRICTION ), (DRM_SIZE_T*)&pbNextPolicyRestriction ) );

                /* Config data (if any) is located immediately after the fixed-size data */
                if( cbRestrictionConfiguration > 0 )
                {
                    ChkVOID( OEM_TEE_MEMCPY( pbNextPolicyRestriction, XBBA_TO_PB( pConfiguration->xbbaConfig ), cbRestrictionConfiguration ) );
                    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pbNextPolicyRestriction, cbRestrictionConfiguration, (DRM_SIZE_T*)&pbNextPolicyRestriction ) );
                }

                cRestrictionsCounted++;
            }
        }

        /* Verify we used the exact same amount of space as we allocated */
        DRMASSERT( pbNextPolicyRestriction == pbPolicyRestrictionMax );
    }

ErrorExit:
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _AllocateLicensePolicyInfoAndParseLicenseRestrictions(
    __inout                                     OEM_TEE_CONTEXT             *f_pOemTeeCtx,
    __in                                  const DRM_TEE_XMR_LICENSE         *f_pLeafLicense,
    __deref_out                                 DRM_TEE_POLICY_INFO        **f_ppPolicy )
{
    DRM_RESULT           dr             = DRM_SUCCESS;
    DRM_TEE_POLICY_INFO *pPolicy        = NULL;
    DRM_DWORD            cbPolicy       = sizeof( *pPolicy );
    DRM_DWORD            cRestrictions  = 0;
    DRM_DWORD            cbRestrictions = 0;

    ChkBOOL( f_pLeafLicense->fValid && XMRFORMAT_IS_PLAY_VALID( &f_pLeafLicense->oLicense ), DRM_E_ILWALID_LICENSE );

    /* First, determine the size of the restrictions, if any, and add them to the overall policy size */

    /* Restriction count == ExplicitAnalogVideoProtection + ExplicitDigitalAudioProtection + ExplicitDigitalVideoProtection */
    ChkDR( DRM_DWordAdd(
        f_pLeafLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitAnalogVideoProtection.cEntries,
        f_pLeafLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalAudioProtection.cEntries,
        &cRestrictions ) );
    ChkDR( DRM_DWordAddSame(
        &cRestrictions,
        f_pLeafLicense->oLicense.OuterContainer.PlaybackPolicyContainer.ExplicitDigitalVideoProtection.cEntries ) );

    ChkDR( _DetermineLicenseRestrictionsSize( f_pLeafLicense, cRestrictions, &cbRestrictions ) );

    /* Now we have the total size of all restrictions, so add that in to the total policy size */
    ChkDR( DRM_DWordAddSame( &cbPolicy, cbRestrictions ) );

    /* Next, allocate the policy structure and fill in its size and the restriction count */
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pOemTeeCtx, cbPolicy, (DRM_VOID**)&pPolicy ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pPolicy, cbPolicy ) );
    pPolicy->cbThis        = cbPolicy;
    pPolicy->cRestrictions = cRestrictions;

    /* Finally, parse the actual restrictions.  Other members of pPolicy are parsed later (outside this function) */
    ChkDR( _ParseLicenseRestrictions( f_pLeafLicense, cbRestrictions, pPolicy ) );

    *f_ppPolicy = pPolicy;
    pPolicy = NULL;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeCtx, (DRM_VOID**)&pPolicy ) );
    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _ParseLicensePolicies(
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

    if( f_poRoot->fValid )
    {
        /* Chained licenses must not have SelwreStop2 information */
        ChkBOOL( !XMRFORMAT_IS_SELWRESTOP2_VALID( &f_poRoot->oLicense ), DRM_E_ILWALID_LICENSE );
        ChkBOOL( !XMRFORMAT_IS_SELWRESTOP2_VALID( &f_poLeaf->oLicense ), DRM_E_ILWALID_LICENSE );
    }

    f_poPolicy->fSelwreStop2 = XMRFORMAT_IS_SELWRESTOP2_VALID( &f_poLeaf->oLicense );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_EnforcePolicy(
    __inout                     OEM_TEE_CONTEXT                      *f_pOemTeeContext,
    __in                        DRM_TEE_POLICY_INFO_CALLING_API       f_ePolicyCallingAPI,
    __in                  const DRM_TEE_POLICY_INFO                  *f_pPolicy )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDR( DRM_TEE_IMPL_SELWRESTOP2_VerifyDecryptionNotStopped( f_pOemTeeContext, f_pPolicy ) );

    ChkDRAllowENOTIMPL( OEM_TEE_DECRYPT_EnforcePolicy( f_pOemTeeContext, f_ePolicyCallingAPI, f_pPolicy ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*
** Synopsis:
**
** This function takes a complete license chain (simple or leaf/root) which
** can decrypt content and allows the OEM to construct an
** OEM-specific policy blob containing the same policy that the CDKB will
** include when DRM_TEE_DECRYPT_PrepareToDecrypt is called.
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
**                               The leaf-most license must use supported key types for CI/CK.
**                               Each license up the chain (if any) should
**                               then sequentially be next in the byte array (if any).
**                               Since each license has an embedded license size, the
**                               PRITEE can easily parse this blob into individual licenses.
** f_dwDecryptionMode:  (in)     The mode requested by the caller for decryption.
** f_pOEMPolicyInfo:    (out)    An OEM specific policy data blob.
**                               This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_DECRYPT_PreparePolicyInfo(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __in                        DRM_DWORD                     f_dwDecryptionMode,
    __out                       DRM_TEE_BYTE_BLOB            *f_pOEMPolicyInfo )
{
    DRM_RESULT            dr                           = DRM_SUCCESS;
    DRM_TEE_XMR_LICENSE  *pLeaf                        = NULL;
    DRM_TEE_XMR_LICENSE  *pRoot                        = NULL;
    DRM_DWORD             cbOEMPolicyInfo              = 0;
    DRM_BYTE             *pbOEMPolicyInfo              = NULL;
    DRM_TEE_BYTE_BLOB     oOEMPolicyInfo               = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT      *pOemTeeCtx                   = NULL;
    DRM_TEE_POLICY_INFO  *pPolicy                      = NULL;

    DRMASSERT( f_pContext       != NULL );
    DRMASSERT( f_pLicenses      != NULL );
    DRMASSERT( f_pOEMPolicyInfo != NULL );

    DRMASSERT( IsBlobAssigned( f_pLicenses ) );

    pOemTeeCtx = &f_pContext->oContext;

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pOEMPolicyInfo, sizeof(*f_pOEMPolicyInfo) ) );

    DRMCASSERT( DRM_MAX_LICENSE_CHAIN_DEPTH == 2 );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof( *pLeaf ), (DRM_VOID**)&pLeaf ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pLeaf, sizeof( *pLeaf ) ) );
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof( *pRoot ), (DRM_VOID**)&pRoot ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pRoot, sizeof( *pRoot ) ) );

    ChkDR( _ParseLicenseChain( f_pContext, f_pLicenses, pLeaf, pRoot ) );
    ChkDR( _AllocateLicensePolicyInfoAndParseLicenseRestrictions( pOemTeeCtx, pLeaf, &pPolicy ) );

    pPolicy->dwDecryptionMode = f_dwDecryptionMode;
    ChkDR( _ParseLicenseChainSelwrityLevel( pLeaf, pRoot, NULL, &pPolicy->wSelwrityLevel ) );

    ChkDR( _ParseLicensePolicies( pLeaf, pRoot, pPolicy ) );
    pPolicy->eSymmetricEncryptionType = (XMR_SYMMETRIC_ENCRYPTION_TYPE)pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType;

    dr = OEM_TEE_DECRYPT_BuildExternalPolicyCryptoInfo(
        pOemTeeCtx,
        &f_pContext->idSession,
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        pPolicy,
        &cbOEMPolicyInfo,
        &pbOEMPolicyInfo );

    if( dr == DRM_E_NOTIMPL )
    {
        dr = DRM_SUCCESS;
    }
    else
    {
        ChkDR( dr );
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership( f_pContext, cbOEMPolicyInfo, &pbOEMPolicyInfo, &oOEMPolicyInfo ) );
        ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pOEMPolicyInfo, &oOEMPolicyInfo ) );
    }

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oOEMPolicyInfo ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbOEMPolicyInfo ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pPolicy ) );
    ChkVOID( _FreeLicenseChain( pOemTeeCtx, pLeaf, pRoot ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pLeaf ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pRoot ) );
    return dr;
}
#endif

#if defined (SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT DRM_CALL _AddFixedPolicyAndBuildCDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __in                                                  DRM_DWORD                   f_cKeys,
    __in_ecount( f_cKeys )                          const DRM_TEE_KEY                *f_pKeys,
    __in_opt                                        const DRM_TEE_KEY                *f_pSelwreStop2Key,
    __in_opt                                        const DRM_ID                     *f_pidSelwreStopSession,
    __in_ecount( 1 )                                      DRM_TEE_POLICY_INFO        *f_pPolicy,
    __out                                                 DRM_TEE_BYTE_BLOB          *f_pCDKB )
{
    DRM_RESULT dr = DRM_SUCCESS;

    /*
    ** Enable fixed play enablers (when building CDKB).
    ** This ensures all parsers will always see it as enabled.
    */
    ChkVOID( DRM_TEE_IMPL_DECRYPT_ENABLE_FIXED_PLAY_ENABLERS( f_pPolicy->dwPlayEnablers ) );

    ChkDR( DRM_TEE_IMPL_KB_BuildCDKB(
        f_pContext,
        f_fSelfContained,
        f_cKeys,
        f_pKeys,
        f_pSelwreStop2Key,
        f_pidSelwreStopSession,
        f_pPolicy,
        f_pCDKB ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function takes a complete license chain (simple or leaf/root) which
** can decrypt content and is bound to a given LKB.
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
**  9. Verify that the given leaf-most license uses a supported decryption algorithm.
** 10. If two licenses were given (i.e. a license chain):
**     Verify that the given security level for leaf <= root.
** 11. Call an OEM function to ensure that the OEM either supports the requested
**     decryption mode or changes the decryption mode to the MOST secure
**     one it supports.
** 12. If the mode is now OEM_TEE_DECRYPTION_MODE_NOT_SELWRE (zero),
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
**                               The leaf-most license must use supported key types for CI/CK.
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
**                               DRM_TEE_*_Decrypt* functions.
**
**                               The value of OEM_TEE_DECRYPTION_MODE_NOT_SELWRE
**                               (zero) means that content is returned in the clear,
**                               in which case this function will fail with
**                               DRM_E_RIGHTS_NOT_AVAILABLE if the content is not <= SL2000.
**                               Any other value is OEM-specific.
**                               The caller may request a mode which is not supported
**                               in which case the OEM will remap the value to the mode
**                               which is the MOST secure version that is supported.
**                               If the MOST secure version that is supported is
**                               OEM_TEE_DECRYPTION_MODE_NOT_SELWRE, then
**                               this function will fail with
**                               DRM_E_RIGHTS_NOT_AVAILABLE if the content is not <= SL2000.
** f_pCDKB:             (out)    The Content Decryption Key Blob (CDKB) used to decrypt
**                               content with a KID that matches the leaf-most license in
**                               the specified license chain.
**                               This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_DECRYPT_PrepareToDecrypt(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLKB,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pLicenses,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pRKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pSPKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pChecksum,
    __inout                     DRM_DWORD                    *f_pdwDecryptionMode,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCDKB )
{
    DRM_RESULT            dr                                = DRM_SUCCESS;
    DRM_TEE_XMR_LICENSE  *pLeaf                             = NULL;
    DRM_TEE_XMR_LICENSE  *pRoot                             = NULL;
    DRM_DWORD             cEscrowedKeys                     = 0;
    DRM_TEE_KEY          *pEscrowedKeys                     = NULL;
    DRM_TEE_KEY           rgoLeafKeys[ 2 ]                  = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    OEM_TEE_CONTEXT      *pOemTeeCtx                        = NULL;
    DRM_BOOL              fSelfContained                    = FALSE;
    DRM_DWORD             dwDecryptionMode                  = 0;
    DRM_TEE_POLICY_INFO  *pPolicy                           = NULL;
    DRM_TEE_RKB_CONTEXT   oRKBCtx;                          /* Initialized in DRM_TEE_IMPL_KB_ParseAndVerifyRKB */
    DRM_DWORD             cSPKeys                           = 0;
    DRM_TEE_KEY          *pSPKeys                           = NULL;
    DRM_DWORD             dwSPKBSelwrityLevel               = 0;
    DRM_DWORD             dwSPKBRIV                         = 0;
    DRM_DWORD             dwRKBRIV                          = 0;
    const DRM_DWORD      *pdwSPKBSelwrityLevel              = NULL;
    DRM_TEE_KEY           rgoCICKSPK[ 3 ]                   = { DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY, DRM_TEE_KEY_EMPTY };
    DRM_TEE_KEY           rgoSelwreStop2Key[ 1 ]            = { DRM_TEE_KEY_EMPTY };
    DRM_RevocationEntry   rgbCertDigests[ DRM_BCERT_MAX_CERTS_PER_CHAIN ];      /* OEM_TEE_ZERO_MEMORY */
    DRM_ID                idSelwreStopSession               = DRM_ID_EMPTY;

    ChkVOID( OEM_TEE_ZERO_MEMORY( rgbCertDigests, sizeof( rgbCertDigests ) ) );

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pLicenses != NULL );
    DRMASSERT( f_pLKB != NULL );
    DRMASSERT( f_pChecksum != NULL );
    DRMASSERT( f_pdwDecryptionMode != NULL );
    DRMASSERT( f_pCDKB != NULL );
    DRMASSERT( f_pRKB != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    dwDecryptionMode = *f_pdwDecryptionMode;

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pCDKB, sizeof( *f_pCDKB ) ) );

    /*
    ** The OEM may upgrade (inselwre->secure), downgrade (secure->inselwre), or modify (secure->secure) the
    ** decryption mode based on what it supports to allow an application to be written cross-platform as long
    ** as it can handle the returned decryption mode (and the hardware can as well).
    ** It's ok to upgrade or downgrade as long as we only allow >SL2000 to use something OTHER than non-secure decryption mode.
    ** Upgrade/downgrade must happen __BEFORE__ we validate the decryption mode via the call to _ValidateDecryptionMode
    ** in order to ensure that >SL2000 content does not use non-secure decryption mode.
    */
    ChkVOID( OEM_TEE_DECRYPT_RemapRequestedDecryptionModeToSupportedDecryptionMode( pOemTeeCtx, &dwDecryptionMode ) );

    if( dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        DRM_DWORD cCertDigests = DRM_BCERT_MAX_CERTS_PER_CHAIN;

        ChkArg( IsBlobAssigned( f_pSPKB ) );
        ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifySPKB(
            f_pContext,
            f_pSPKB,
            &dwSPKBSelwrityLevel,
            &cCertDigests,
            rgbCertDigests,
            &dwSPKBRIV,
            &cSPKeys,
            &pSPKeys ) );

        AssertChkBOOL( dwSPKBSelwrityLevel <= DRM_BCERT_SELWRITYLEVEL_2000 );
        pdwSPKBSelwrityLevel = &dwSPKBSelwrityLevel;

        ChkBOOL( IsBlobAssigned( f_pRKB ), DRM_E_RIV_TOO_SMALL );
        ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyRKB( f_pContext, f_pRKB, &oRKBCtx, cCertDigests, (const DRM_RevocationEntry*)rgbCertDigests ) );

        ChkBOOL( dwSPKBRIV <= oRKBCtx.dwRIV, DRM_E_ILWALID_REVOCATION_LIST );

        dwRKBRIV = oRKBCtx.dwRIV;
    }
    else
    {
        ChkArg( !IsBlobAssigned( f_pSPKB ) );
    }

    DRMCASSERT( DRM_MAX_LICENSE_CHAIN_DEPTH == 2 );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof( *pLeaf ), (DRM_VOID**)&pLeaf ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pLeaf, sizeof( *pLeaf ) ) );
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, sizeof( *pRoot ), (DRM_VOID**)&pRoot ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pRoot, sizeof( *pRoot ) ) );

    ChkDR( _ParseLicenseChain( f_pContext, f_pLicenses, pLeaf, pRoot ) );
    ChkDR( _AllocateLicensePolicyInfoAndParseLicenseRestrictions( pOemTeeCtx, pLeaf, &pPolicy ) );

    pPolicy->dwDecryptionMode = dwDecryptionMode;

    ChkDR( _ValidateLicenseEncryptionType( pLeaf ) );
    pPolicy->eSymmetricEncryptionType = (XMR_SYMMETRIC_ENCRYPTION_TYPE)pLeaf->oLicense.OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType;

    /* we need pEscrowedKeys only now */
    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyLKB( f_pContext, f_pLKB, &idSelwreStopSession, &cEscrowedKeys, &pEscrowedKeys ) );
    AssertChkBOOL( cEscrowedKeys >= 2 );

    if( pRoot->fValid )
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CI, &rgoLeafKeys[ 0 ] ) );
        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &rgoLeafKeys[ 1 ] ) );
    }

    ChkDR( _ValidateLicenseChain(
        f_pContext,
        f_pChecksum,
        pPolicy->dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION,
        dwRKBRIV,
        pLeaf,
        pRoot,
        pEscrowedKeys,
        rgoLeafKeys ) );

    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CI, &rgoCICKSPK[ 0 ] ) );
    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &rgoCICKSPK[ 1 ] ) );
    ChkDR( OEM_TEE_BASE_CopyKeyAES128( pOemTeeCtx, &rgoCICKSPK[ 0 ].oKey, pRoot->fValid ? &rgoLeafKeys[ 0 ].oKey : &pEscrowedKeys[ 0 ].oKey ) );
    ChkDR( OEM_TEE_BASE_CopyKeyAES128( pOemTeeCtx, &rgoCICKSPK[ 1 ].oKey, pRoot->fValid ? &rgoLeafKeys[ 1 ].oKey : &pEscrowedKeys[ 1 ].oKey ) );

    /* Keys no longer needed, free them */
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[ 0 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[ 1 ].oKey ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cEscrowedKeys, &pEscrowedKeys ) );

    ChkDR( DRM_TEE_IMPL_BASE_ValidateLicenseExpiration( f_pContext, &pLeaf->oLicense, TRUE, TRUE ) );

    if( pRoot->fValid )
    {
        ChkDR( DRM_TEE_IMPL_BASE_ValidateLicenseExpiration( f_pContext, &pRoot->oLicense, TRUE, TRUE ) );
    }

    ChkDR( _ParseLicenseChainSelwrityLevel( pLeaf, pRoot, pdwSPKBSelwrityLevel, &pPolicy->wSelwrityLevel ) );
    ChkDR( _ValidateDecryptionMode( pOemTeeCtx, pPolicy->dwDecryptionMode, pPolicy->wSelwrityLevel ) );

    ChkDR( _ParseLicensePolicies( pLeaf, pRoot, pPolicy ) );

    if( pPolicy->fSelwreStop2 )
    {
        DRM_BYTE rgbSelwreStop2Key[ DRM_AES_BLOCKLEN ];
        DRM_XMRFORMAT_SELWRESTOP2 *pXmrSelwreStop2 = &pLeaf->oLicense.OuterContainer.KeyMaterialContainer.SelwreStop2;

        /* Verify data */
        ChkBOOL( XMRFORMAT_IS_CANNOT_PERSIST_LICENSE( &pLeaf->oLicense ), DRM_E_ILWALID_LICENSE );
        ChkBOOL( XMRFORMAT_IS_SELWRESTOP_VALID( &pLeaf->oLicense ), DRM_E_ILWALID_LICENSE );
        ChkBOOL( pXmrSelwreStop2->wSymmetricKeyEncryptionType == XMR_SYMMETRIC_KEY_ENCRYPTION_TYPE_AES_128_ECB, DRM_E_ILWALID_LICENSE );
        ChkBOOL( pXmrSelwreStop2->wSignatureAlgorithm == XMR_SELWRE_STOP_2_SIGNATURE_ALGORITHM_ECC256_SHA256_AES128CBC_IVKIDSESSIONID_PLAINOLDSIG, DRM_E_ILWALID_LICENSE );
        ChkBOOL( XBBA_HAS_DATA( pXmrSelwreStop2->xbbaEncryptedKey ), DRM_E_ILWALID_LICENSE );
        ChkBOOL( XBBA_TO_CB( pXmrSelwreStop2->xbbaEncryptedKey ) == sizeof( rgbSelwreStop2Key ), DRM_E_ILWALID_LICENSE );

        /* Decrypt the Secure Stop 2 Key with CI */
        ChkVOID( OEM_TEE_MEMCPY( rgbSelwreStop2Key, XBBA_TO_PB( pXmrSelwreStop2->xbbaEncryptedKey ), sizeof( rgbSelwreStop2Key ) ) );
        ChkDR( OEM_TEE_BASE_AES128ECB_DecryptData( pOemTeeCtx, &rgoCICKSPK[ 0 ].oKey, sizeof( rgbSelwreStop2Key ), rgbSelwreStop2Key ) );

        /* Colwert the decrypted key's plaintext into a DRM_TEE_KEY */
        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_AES128_SELWRESTOP2, &rgoSelwreStop2Key[ 0 ] ) );
        ChkDR( OEM_TEE_BASE_AES128_SetKey( pOemTeeCtx, &rgoSelwreStop2Key[ 0 ].oKey, rgbSelwreStop2Key ) );
    }

    ChkDR( DRM_TEE_IMPL_DECRYPT_EnforcePolicy( pOemTeeCtx, DRM_TEE_POLICY_INFO_CALLING_API_DECRYPT_PREPARETODECRYPT, pPolicy ) );

    if( pPolicy->dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_SAMPLEPROT, &rgoCICKSPK[ 2 ] ) );
        ChkDR( OEM_TEE_BASE_CopyKeyAES128( pOemTeeCtx, &rgoCICKSPK[ 2 ].oKey, &pSPKeys[ 0 ].oKey ) );
    }

    ChkDR( _AddFixedPolicyAndBuildCDKB(
        f_pContext,
        fSelfContained,
        IsBlobAssigned( f_pSPKB ) ? (DRM_DWORD)3 : (DRM_DWORD)2,
        rgoCICKSPK,
        rgoSelwreStop2Key,
        &idSelwreStopSession,
        pPolicy,
        f_pCDKB ) );

    *f_pdwDecryptionMode = pPolicy->dwDecryptionMode;

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cSPKeys, &pSPKeys ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cEscrowedKeys, &pEscrowedKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pPolicy ) );
    ChkVOID( _FreeLicenseChain( pOemTeeCtx, pLeaf, pRoot ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pLeaf ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pRoot ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[ 0 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoLeafKeys[ 1 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoCICKSPK[ 0 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoCICKSPK[ 1 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoCICKSPK[ 2 ].oKey ) );
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &rgoSelwreStop2Key[ 0 ].oKey ) );
    return dr;
}

/*
** Synopsis:
**
** This function takes CDKB and create a OEM KeyBlob.
**
** This function is usually called after DRM_TEE_DECRYPT_PrepareToDecrypt.
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
**                               This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_DECRYPT_CreateOEMBlobFromCDKB(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
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
    DRM_TEE_POLICY_INFO *pPolicy                       = NULL;

    /* Arguments are checked in the DRM TEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pCDKB != NULL );
    DRMASSERT( f_pOEMKeyInfo != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_TEE_IMPL_DECRYPT_ParseAndVerifyCDKB(
        f_pContext,
        f_pCDKB,
        fSelfContained,
        &cCDKeys,
        &pCDKeys,
        NULL,                   /* No need for SelwreStop2Key here */
        NULL,                   /* No need for idSelwreStopSession here */
        &pPolicy ) );

    AssertChkArg( cCDKeys >= 2 );
    dr = OEM_TEE_DECRYPT_BuildExternalPolicyCryptoInfo(
        pOemTeeCtx,
        &f_pContext->idSession,
        f_pCDKB,
        &pCDKeys[ 1 ].oKey,
        cCDKeys == 3 ? &pCDKeys[ 2 ].oKey : NULL,
        f_pOEMInitData == NULL ? 0 : f_pOEMInitData->cb,
        f_pOEMInitData == NULL ? NULL : f_pOEMInitData->pb,
        pPolicy,
        &cbOEMKeyInfo,
        &pbOEMKeyInfo );

    if( dr == DRM_E_NOTIMPL )
    {
        dr = DRM_SUCCESS;
        DRMASSERT( pbOEMKeyInfo == NULL );

        fSelfContained = TRUE;

        ChkDR( _AddFixedPolicyAndBuildCDKB(
            f_pContext,
            fSelfContained,
            cCDKeys,
            pCDKeys,
            NULL,               /* No need for SelwreStop2Key here */
            NULL,               /* No need for idSelwreStopSession here */
            pPolicy,
            &oOEMKeyInfo ) );
    }
    else
    {
        ChkDR( dr );
        if( cbOEMKeyInfo > 0 )
        {
            ChkDR( DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership( f_pContext, cbOEMKeyInfo, &pbOEMKeyInfo, &oOEMKeyInfo ) );
        }
    }

    /* transfer ownership */
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pOEMKeyInfo, &oOEMKeyInfo ) );

ErrorExit:
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oOEMKeyInfo );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pbOEMKeyInfo ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pPolicy ) );
    return dr;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_DecryptContentPolicyHelper(
    __inout_opt                 DRM_TEE_CONTEXT                      *f_pContext,
    __deref_out                 DRM_TEE_CONTEXT                     **f_ppContext,
    __inout                     DRM_TEE_CONTEXT                      *f_pTeeCtxReconstituted,
    __out                       DRM_BOOL                             *f_pfReconstituted,
    __in                        DRM_TEE_POLICY_INFO_CALLING_API       f_ePolicyCallingAPI,
    __in                  const DRM_TEE_BYTE_BLOB                    *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB                    *f_pOEMKeyInfo,
    __out                       DRM_DWORD                            *f_pcKeys,
    __deref_out                 DRM_TEE_KEY                         **f_ppKeys,
    __deref_out                 DRM_TEE_POLICY_INFO                 **f_ppPolicy )
{
    DRM_RESULT               dr                           = DRM_SUCCESS;
    DRM_DWORD                cCDKeys                      = 0;
    DRM_TEE_KEY             *pCDKeys                      = NULL;
    OEM_TEE_CONTEXT         *pOemTeeCtx                   = NULL;
    DRM_TEE_POLICY_INFO     *pPolicy                      = NULL;

    DRMASSERT(  f_ppPolicy != NULL );
    DRMASSERT( *f_ppPolicy == NULL );

    if( f_pContext == NULL )
    {
        f_pContext = f_pTeeCtxReconstituted;
        *f_pfReconstituted = TRUE;
    }
    else
    {
        *f_pfReconstituted = FALSE;
    }

    ChkDRAllowENOTIMPL( DRM_TEE_IMPL_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo(
        f_pContext,
        f_pCDKB,
        f_pOEMKeyInfo,
        *f_pfReconstituted,
        &cCDKeys,
        &pCDKeys,
        &pPolicy,
        *f_pfReconstituted ? &f_pContext->idSession : NULL ) );

    if( cCDKeys == 0 )
    {
        AssertChkBOOL( pCDKeys == NULL );
        AssertChkBOOL( pPolicy == NULL );
        ChkDR( DRM_TEE_IMPL_DECRYPT_ParseAndVerifyCDKB(
            f_pContext,
            f_pCDKB,
            *f_pfReconstituted,
            &cCDKeys,
            &pCDKeys,
            NULL,           /* No need for SelwreStop2Key here */
            NULL,           /* No need for idSelwreStopSession here */
            &pPolicy ) );
    }

    AssertChkBOOL( cCDKeys == 2 || cCDKeys == 3 );
    if( *f_pfReconstituted )
    {
        ChkDR( DRM_TEE_IMPL_CACHE_ReferenceContext( f_pContext ) );
    }

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_TEE_IMPL_DECRYPT_EnforcePolicy( pOemTeeCtx, f_ePolicyCallingAPI, pPolicy ) );

    /* Sample protection type must NOT be a handle type */
    DRMCASSERT( !OEM_TEE_DECRYPTION_MODE_IS_HANDLE_TYPE( OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION ) );

    /* Sample protection type must have a sample protection key available */
    if( pPolicy->dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        ChkBOOL( cCDKeys == 3, DRM_E_TEE_ILWALID_KEY_DATA );
    }

    *f_pcKeys = cCDKeys;
    *f_ppKeys = pCDKeys;
    pCDKeys   = NULL;
    *f_ppContext = f_pContext;

    *f_ppPolicy = pPolicy;
    pPolicy = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pPolicy ) );
    return dr;
}
#endif

#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_ParseAndVerifyCDKB(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                            const DRM_TEE_BYTE_BLOB          *f_pCDKB,
    __in                                                  DRM_BOOL                    f_fSelfContained,
    __out                                                 DRM_DWORD                  *f_pcKeys,
    __deref_out_ecount( *f_pcKeys )                       DRM_TEE_KEY               **f_ppKeys,
    __deref_opt_out_ecount_opt( 1 )                       DRM_TEE_KEY               **f_ppSelwreStop2Key,
    __out_opt                                             DRM_ID                     *f_pidSelwreStopSession,
    __deref_out                                           DRM_TEE_POLICY_INFO       **f_ppPolicy )
{
    DRM_RESULT dr = DRM_SUCCESS;

    /*
    ** This is the one place where DRM_TEE_IMPL_KB_ParseAndVerifyCDKBDoNotCallDirectly is called.
    ** The name is to indicate that other code should call DRM_TEE_IMPL_DECRYPT_ParseAndVerifyCDKB instead.
    */
    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyCDKBDoNotCallDirectly(
        f_pContext,
        f_pCDKB,
        f_fSelfContained,
        f_pcKeys,
        f_ppKeys,
        f_ppSelwreStop2Key,
        f_pidSelwreStopSession,
        f_ppPolicy ) );

    /*
    ** Enable fixed play enablers (when parsing CDKB).
    ** This ensures all callers will always see it as enabled.
    */
    ChkVOID( DRM_TEE_IMPL_DECRYPT_ENABLE_FIXED_PLAY_ENABLERS( ( *f_ppPolicy )->dwPlayEnablers ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*
** Synopsis:
**
** This function parses and validates the external policy cryptographic information created
** by the function OEM_TEE_DECRYPT_BuildExternalPolicyCryptoInfo and returns the content
** keys, session ID, and policy information associated with the external policy.
**
** For debug builds only, this function also validates the OEM key information provided by
** function OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo against the Content
** Decryption Key Blob (CDKB).
**
** This function is called inside:
**  DRM_TEE_*_Decrypt* functions
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
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo(
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
    DRM_ID               idSession;      /* Initialized in OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo */
    DRM_TEE_POLICY_INFO *pOemPolicy      = NULL;
    OEM_TEE_CONTEXT     *pOemTeeCtx      = NULL;
#if DRM_DBG
    DRM_TEE_POLICY_INFO *pPolicyFromCDKB = NULL;
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
    ChkDR( DRM_TEE_IMPL_BASE_AllocateKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_PR_CK, &pKeys[ 1 ] ) );
    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_SAMPLEPROT, &pKeys[ 2 ] ) );

    ChkDR( OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo(
        pOemTeeCtx,
        f_pOEMKeyInfo->cb,
        f_pOEMKeyInfo->pb,
        &pOemPolicy,
        &idSession,
        &pKeys[1].oKey,
        &pKeys[2].oKey ) );

    /*
    ** Enable fixed play enablers (when parsing policy from OEM crypto info).
    ** This ensures that an OEM bug that fails to set it will not block Miracast.
    */
    ChkVOID( DRM_TEE_IMPL_DECRYPT_ENABLE_FIXED_PLAY_ENABLERS( pOemPolicy->dwPlayEnablers ) );

    if( pOemPolicy->dwDecryptionMode != OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        /*
        ** The following code will not affect DRM_TEE_IMPL_BASE_FreeKeyArray since the
        ** sample protection key is the last item in the array and is released here. It
        ** will simple cause DRM_TEE_IMPL_BASE_FreeKeyArray to skip the last key (which
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
** the OEM's implementation of OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo is
** correct.
*/
#if DRM_DBG
    if( f_pCDKB != NULL && IsBlobAssigned( f_pCDKB ) )
    {
        ChkDR( DRM_TEE_IMPL_DECRYPT_ParseAndVerifyCDKB(
            f_fIsReconstituted ? &oTeeCtx : f_pContext,
            f_pCDKB,
            FALSE,
            &cCDKeysFromCDKB,
            &pCDKeysFromCDKB,
            NULL,                   /* No need for SelwreStop2Key here */
            NULL,                   /* No need for idSelwreStopSession here */
            &pPolicyFromCDKB ) );

        AssertChkBOOL( cCDKeysFromCDKB == 2 || cCDKeysFromCDKB == 3 );

        if( f_fIsReconstituted )
        {
            AssertChkBOOL( OEM_SELWRE_ARE_EQUAL( &f_pContext->idSession, &oTeeCtx.idSession, sizeof( DRM_ID ) ) );
        }

        /* Validate the policy information */
        AssertChkBOOL( pOemPolicy->cbThis == pPolicyFromCDKB->cbThis );
        AssertChkBOOL( OEM_SELWRE_ARE_EQUAL( pOemPolicy, pPolicyFromCDKB, pOemPolicy->cbThis ) );
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
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pOemPolicy ) );

#if DRM_DBG
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cCDKeysFromCDKB, &pCDKeysFromCDKB ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pPolicyFromCDKB ) );
#endif /* DRM_DBG */

    return dr;
}
#endif

#ifdef NONE
typedef DRM_RESULT( DRM_CALL *DRM_pfnDecryptIntoClear )(
    __inout                                          OEM_TEE_CONTEXT                 *f_pContext,
    __in                                             DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                                       const DRM_TEE_POLICY_INFO             *f_pPolicy,
    __in                                       const OEM_TEE_KEY                     *f_pCK,
    __in                                             DRM_DWORD                        f_cEncryptedRegionMappings,
    __in_ecount( f_cEncryptedRegionMappings )  const DRM_DWORD                       *f_pdwEncryptedRegionMappings,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorHigh,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorLow,
    __in                                             DRM_DWORD                        f_cEncryptedRegionSkip,
    __in_ecount_opt( f_cEncryptedRegionSkip )  const DRM_DWORD                       *f_pcEncryptedRegionSkip,
    __in                                             DRM_DWORD                        f_cbEncryptedToClear,
    __inout_bcount( f_cbEncryptedToClear )           DRM_BYTE                        *f_pbEncryptedToClear,
    __in                                             DRM_DWORD                        f_ibProcessing,
    __out_opt                                        DRM_DWORD                       *f_pcbProcessed );

typedef DRM_RESULT( DRM_CALL *DRM_pfnDecryptIntoHandle )(
    __inout                                          OEM_TEE_CONTEXT                 *f_pContext,
    __in                                             DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                                       const DRM_TEE_POLICY_INFO             *f_pPolicy,
    __in                                       const OEM_TEE_KEY                     *f_pCK,
    __in                                             DRM_DWORD                        f_cEncryptedRegionMappings,
    __in_ecount( f_cEncryptedRegionMappings )  const DRM_DWORD                       *f_pdwEncryptedRegionMappings,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorHigh,
    __in                                             DRM_UINT64                       f_ui64InitializatiolwectorLow,
    __in                                             DRM_DWORD                        f_cEncryptedRegionSkip,
    __in_ecount_opt( f_cEncryptedRegionSkip )  const DRM_DWORD                       *f_pcEncryptedRegionSkip,
    __in                                             DRM_DWORD                        f_cbEncrypted,
    __in_bcount( f_cbEncrypted )               const DRM_BYTE                        *f_pbEncrypted,
    __inout                                          OEM_TEE_MEMORY_HANDLE            f_hClear,
    __in                                             DRM_DWORD                        f_ibProcessing,
    __out_opt                                        DRM_DWORD                       *f_pcbProcessed );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128_DecryptContentHelper(
    __inout_opt                 DRM_TEE_CONTEXT                 *f_pContext,
    __in                        DRM_TEE_POLICY_INFO_CALLING_API  f_ePolicyCallingAPI,
    __in                  const DRM_TEE_BYTE_BLOB               *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB               *f_pOEMKeyInfo,
    __in                  const DRM_TEE_QWORDLIST               *f_pEncryptedRegionInitializatiolwectorsHigh,
    __in_tee_opt          const DRM_TEE_QWORDLIST               *f_pEncryptedRegionInitializatiolwectorsLow,
    __in                  const DRM_TEE_DWORDLIST               *f_pEncryptedRegionCounts,
    __in                  const DRM_TEE_DWORDLIST               *f_pEncryptedRegionMappings,
    __in_tee_opt          const DRM_TEE_DWORDLIST               *f_pEncryptedRegionSkip,
    __in                  const DRM_TEE_BYTE_BLOB               *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB               *f_pCCD )
{
    DRM_RESULT               dr                     = DRM_SUCCESS;
    DRM_RESULT               drTmp                  = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB        oCCD                   = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD                cCDKeys                = 0;
    DRM_TEE_KEY             *pCDKeys                = NULL;
    DRM_TEE_CONTEXT          oTeeCtxReconstituted   = DRM_TEE_CONTEXT_EMPTY;
    DRM_BOOL                 fReconstituted         = FALSE;
    OEM_TEE_CONTEXT         *pOemTeeCtx             = NULL;
    DRM_TEE_POLICY_INFO     *pPolicy                = NULL;
    DRM_DWORD                iRegion;               /* loop variable */
    DRM_DWORD                iRegionMapping         = 0;
    OEM_TEE_MEMORY_HANDLE    hClear                 = OEM_TEE_MEMORY_HANDLE_ILWALID;
    DRM_DWORD                cbhClear               = 0;
    DRM_DWORD                ibDecrypting           = 0;
    DRM_BOOL                 fAESCBC                = FALSE;
    DRM_pfnDecryptIntoClear  pfnDecryptIntoClear    = NULL;
    DRM_pfnDecryptIntoHandle pfnDecryptIntoHandle   = NULL;

    /* Arguments are checked in the DRM TEE proxy layer */
    DRMASSERT( f_pEncryptedRegionInitializatiolwectorsHigh != NULL );
    DRMASSERT( f_pEncryptedRegionInitializatiolwectorsLow != NULL );
    DRMASSERT( f_pEncryptedRegionCounts != NULL );
    DRMASSERT( f_pEncryptedRegionMappings != NULL );
    DRMASSERT( f_pEncrypted != NULL );
    DRMASSERT( f_pCCD != NULL );

    DRMASSERT( IsBlobAssigned( f_pEncrypted ) );

    /* f_pEncryptedRegionMapping must have a multiple of 2 DWORDs, each DWORD pair is a clear length followed by an encrypted length. */
    ChkArg( ( f_pEncryptedRegionMappings->cdwData & 1 ) == 0 );     /* Power of two: use & instead of % for better perf */

    /* Each vector corresponds to a region, i.e. there's a strict 1:1 correspondence. */
    ChkArg( f_pEncryptedRegionInitializatiolwectorsHigh->cqwData == f_pEncryptedRegionCounts->cdwData );
    ChkArg( f_pEncryptedRegionInitializatiolwectorsLow->cqwData == f_pEncryptedRegionCounts->cdwData
         || f_pEncryptedRegionInitializatiolwectorsLow->cqwData == 0 );     /* 0 if 8 bytes IVs are being used */

    ChkArg( f_pEncryptedRegionSkip->cdwData == 0 || f_pEncryptedRegionSkip->cdwData == 2 );

    switch( f_ePolicyCallingAPI )
    {
    case DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_DECRYPTCONTENTMULTIPLE:
        fAESCBC = FALSE;
        pfnDecryptIntoClear = OEM_TEE_AES128CTR_DecryptContentIntoClear;
        pfnDecryptIntoHandle = OEM_TEE_AES128CTR_DecryptContentIntoHandle;
        break;
    case DRM_TEE_POLICY_INFO_CALLING_API_AES128CBC_DECRYPTCONTENTMULTIPLE:
        fAESCBC = TRUE;
        pfnDecryptIntoClear = OEM_TEE_AES128CBC_DecryptContentIntoClear;
        pfnDecryptIntoHandle = OEM_TEE_AES128CBC_DecryptContentIntoHandle;
        break;
    default:
        AssertChkArg( FALSE );
        break;
    }

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pCCD, sizeof( *f_pCCD ) ) );

    {
        DRM_DWORD iCount;  /* Loop variable */
        DRM_DWORD cMappings = 0;

        for( iCount = 0; iCount < f_pEncryptedRegionCounts->cdwData; iCount++ )
        {
            DRMASSERT( f_pEncryptedRegionCounts->pdwData[ iCount ] > 0 );
            ChkDR( DRM_DWordAddSame( &cMappings, f_pEncryptedRegionCounts->pdwData[ iCount ] ) );
        }
        ChkArg( cMappings == f_pEncryptedRegionMappings->cdwData >> 1 );
    }
    {
        DRM_DWORD iMapping;  /* Loop variable */
        DRM_DWORD cbMappings = 0;

        for( iMapping = 0; iMapping < f_pEncryptedRegionMappings->cdwData; iMapping++ )
        {
            ChkDR( DRM_DWordAddSame( &cbMappings, f_pEncryptedRegionMappings->pdwData[ iMapping ] ) );
            if( fAESCBC && ( iMapping & 1 ) && f_pEncryptedRegionSkip->cdwData == 0 )
            {
                /* In CBC1 (but NOT CBCS), encrypted regions (i.e. second entry in each region mapping pair) MUST be sized in whole blocks */
                DRMCASSERT( DRM_AES_BLOCKLEN == 16 );   /* Required for below to work without modification */
                ChkArg( ( f_pEncryptedRegionMappings->pdwData[ iMapping ] & 15 ) == 0 );
            }
        }
        ChkArg( cbMappings == f_pEncrypted->cb );
    }

    ChkDR( DRM_TEE_IMPL_DECRYPT_DecryptContentPolicyHelper(
        f_pContext,
        &f_pContext,
        &oTeeCtxReconstituted,
        &fReconstituted,
        f_ePolicyCallingAPI,
        f_pCDKB,
        f_pOEMKeyInfo,
        &cCDKeys,
        &pCDKeys,
        &pPolicy ) );

    if( fAESCBC )
    {
        ChkBOOL( pPolicy->eSymmetricEncryptionType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CBC, DRM_E_UNSUPPORTED_ALGORITHM );
    }
    else
    {
        ChkBOOL( pPolicy->eSymmetricEncryptionType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CTR, DRM_E_UNSUPPORTED_ALGORITHM );
    }

    pOemTeeCtx = &f_pContext->oContext;

    if( OEM_TEE_DECRYPTION_MODE_IS_HANDLE_TYPE( pPolicy->dwDecryptionMode ) )
    {
        ChkDR( OEM_TEE_BASE_SelwreMemHandleAlloc( pOemTeeCtx, pPolicy->dwDecryptionMode, f_pEncrypted->cb, &cbhClear, &hClear ) );
    }
    else
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pEncrypted->cb, f_pEncrypted->pb, &oCCD ) );
    }

    for( iRegion = 0; iRegion < f_pEncryptedRegionCounts->cdwData; iRegion++ )
    {
        DRM_UINT64 qwIVHigh = f_pEncryptedRegionInitializatiolwectorsHigh->pqwData[ iRegion ];
        DRM_UINT64 qwIVLow = f_pEncryptedRegionInitializatiolwectorsLow->cqwData == 0 ? DRM_UI64( 0 ) : f_pEncryptedRegionInitializatiolwectorsLow->pqwData[ iRegion ];
        DRM_DWORD  cbDecrypted = 0;

        if( OEM_TEE_DECRYPTION_MODE_IS_HANDLE_TYPE( pPolicy->dwDecryptionMode ) )
        {
            ChkDR( pfnDecryptIntoHandle(
                pOemTeeCtx,
                f_ePolicyCallingAPI,
                pPolicy,
                &pCDKeys[ 1 ].oKey,
                f_pEncryptedRegionCounts->pdwData[ iRegion ] << 1,          /* Number of encrypted region mappings for this region */
                &f_pEncryptedRegionMappings->pdwData[ iRegionMapping ],     /* Encrypted region mappings for this region */
                qwIVHigh,
                qwIVLow,
                f_pEncryptedRegionSkip->cdwData,
                f_pEncryptedRegionSkip->pdwData,
                f_pEncrypted->cb,
                f_pEncrypted->pb,
                hClear,
                ibDecrypting,
                &cbDecrypted ) );
        }
        else
        {
            ChkDR( pfnDecryptIntoClear(
                pOemTeeCtx,
                f_ePolicyCallingAPI,
                pPolicy,
                &pCDKeys[ 1 ].oKey,
                f_pEncryptedRegionCounts->pdwData[ iRegion ] << 1,          /* Number of encrypted region mapping entries for this region */
                &f_pEncryptedRegionMappings->pdwData[ iRegionMapping ],     /* Encrypted region mapping entries for this region */
                qwIVHigh,
                qwIVLow,
                f_pEncryptedRegionSkip->cdwData,
                f_pEncryptedRegionSkip->pdwData,
                oCCD.cb,
                (DRM_BYTE*)oCCD.pb,
                ibDecrypting,
                &cbDecrypted ) );

            if( pPolicy->dwDecryptionMode == OEM_TEE_DECRYPTION_MODE_SAMPLE_PROTECTION )
            {
                /* Sample protection always uses an 8 byte IV, i.e. the high 8 bytes */
                ChkDR( DRM_TEE_IMPL_SAMPLEPROT_ApplySampleProtection(
                    pOemTeeCtx,
                    &pCDKeys[ 2 ].oKey,
                    qwIVHigh,
                    cbDecrypted,
                    (DRM_BYTE*)&oCCD.pb[ ibDecrypting ] ) );
            }
        }
        iRegionMapping += f_pEncryptedRegionCounts->pdwData[ iRegion ] << 1;        /* Can't overflow: already checked that region mappings do not exceed buffer */
        ibDecrypting += cbDecrypted;                                                /* Can't overflow: guaranteed by function pointer call */
    }
    DRMASSERT( ibDecrypting == f_pEncrypted->cb );
    DRMASSERT( iRegionMapping == f_pEncryptedRegionMappings->cdwData );

    if( OEM_TEE_DECRYPTION_MODE_IS_HANDLE_TYPE( pPolicy->dwDecryptionMode ) )
    {
        /* Transfer handle into blob */
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlobAndTakeOwnership( f_pContext, cbhClear, (DRM_BYTE**)&hClear, &oCCD ) );
        oCCD.eType = DRM_TEE_BYTE_BLOB_TYPE_CCD;
        oCCD.dwSubType = OEM_TEE_DECRYPTION_MODE_HANDLE;
        hClear = OEM_TEE_MEMORY_HANDLE_ILWALID;
    }

    /* Transfer ownership */
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pCCD, &oCCD ) );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&pPolicy ) );
    
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oCCD );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    if( pCDKeys != NULL )
    {
        ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cCDKeys, &pCDKeys ) );
    }

    if( fReconstituted )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID **)&oTeeCtxReconstituted.oContext.pbUnprotectedOEMData ) );
    }

    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

