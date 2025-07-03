/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMBCERTFORMATPARSER_C
#include <oembroker.h>
#include <drmbytemanip.h>
#include <drmmathsafe.h>
#include <drmbcertformatparser.h>
#include <drmutf.h>
#include <drmprofile.h>
#include <drmlastinclude.h>

PRAGMA_STRICT_GS_PUSH_ON;

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
#define DoExpirationCheck(ft) ( ( (ft).dwLowDateTime != 0 ) || ( (ft).dwHighDateTime != 0 ) )

static DRM_GLOBAL_CONST PUBKEY_P256 g_ECC256MSPlayReadyRootIssuerPubKeyBCERT PR_ATTR_DATA_OVLY(_g_ECC256MSPlayReadyRootIssuerPubKeyBCERT) = { DRM_ECC256_MS_PLAYREADY_ROOT_ISSUER_PUBKEY };

#define DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION2( pChailwer, pCert ) DRM_DO {   \
    *(pChailwer) = (pCert)->SelwrityVersion2;                                       \
} DRM_WHILE_FALSE


#define DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION( pChailwer, pCert ) DRM_DO {    \
    (pChailwer)->fValid            = TRUE;                                          \
    (pChailwer)->dwPlatformID      = (pCert)->SelwrityVersion.dwPlatformID;         \
    (pChailwer)->dwSelwrityVersion = (pCert)->SelwrityVersion.dwSelwrityVersion;    \
} DRM_WHILE_FALSE

/*****************************************************************************
** Function:    _VerifyChildSelwrityVersionMatchesChain
**
** Synopsis:    Ensures that the Security Version information in the passed in
**              certificate if available is consistent with
**              the Chain Security Version information.
**
** Arguments: [f_poCert]                 : The Cert to be checked.
**            [f_poChainSelwrityVersion] : The Security Version to check against.
**
******************************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _VerifyChildSelwrityVersionMatchesChain(
    __in_ecount(1)     const DRM_BCERTFORMAT_CERT                       *f_poCert,
    __in               const DRM_BCERTFORMAT_SELWRITY_VERSION2          *f_poChainSelwrityVersion )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRMASSERT( f_poCert                         != NULL );
    DRMASSERT( f_poChainSelwrityVersion         != NULL );
    DRMASSERT( f_poChainSelwrityVersion->fValid );

    /*
    ** Only fail if the SelwrityVersion in the Cert is Valid and doesn't match.
    ** Shouldn't fail if the SelwrityVersion information is absent.
    ** Note that many legacy certificates did not have security versions which
    ** matched throughout the chain, so we skip the match check on them.
    ** All other certificates (for both current and future platform IDs)
    ** must have matching security versions.
    */
    if( f_poCert->SelwrityVersion2.fValid )
    {
        ChkBOOL( f_poCert->SelwrityVersion2.dwPlatformID == f_poChainSelwrityVersion->dwPlatformID, DRM_E_BCERT_ILWALID_PLATFORM_IDENTIFIER );

        switch( f_poCert->SelwrityVersion2.dwPlatformID )
        {
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_OSX:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WINDOWS_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WM_7:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_IOS_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_XBOX_PPC:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_X86:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_ANDROID_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_1_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_1_X86:
            /* Legacy certificates: no-op */
            break;
        default:
            ChkBOOL( f_poCert->SelwrityVersion2.dwSelwrityVersion == f_poChainSelwrityVersion->dwSelwrityVersion, DRM_E_BCERT_ILWALID_SELWRITY_VERSION );
            break;
        }
    }
    else if( f_poCert->SelwrityVersion.fValid )
    {
        ChkBOOL( f_poCert->SelwrityVersion.dwPlatformID == f_poChainSelwrityVersion->dwPlatformID, DRM_E_BCERT_ILWALID_PLATFORM_IDENTIFIER );

        switch( f_poCert->SelwrityVersion.dwPlatformID )
        {
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_OSX:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WINDOWS_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WM_7:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_IOS_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_XBOX_PPC:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_X86:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_ANDROID_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_1_ARM:
        case DRM_BCERT_SELWRITY_VERSION_PLATFORM_WP8_1_X86:
            /* Legacy certificates: no-op */
            break;
        default:
            ChkBOOL( f_poCert->SelwrityVersion.dwSelwrityVersion == f_poChainSelwrityVersion->dwSelwrityVersion, DRM_E_BCERT_ILWALID_SELWRITY_VERSION );
            break;
        }
    }

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    _DRM_BCERTFORMAT_VerifyAdjacentCerts
**
** Synopsis:    Verifies that certificates in a chain are correctly linked:
**              1. Issuer key value is the same as a key entry in the next certificate
**                 that issued previous one.
**              2. Security level of the next cert is not lower
**                 then that of the current cert.
**              3. The parent certificate has the appropriate issuer rights to issue
**                 the child certificate of a particular type with particular key usages and features.
**              4. The platform ID is consistent throughout the chain.
**
** Arguments: [f_pParserCtx]   : The BCERT format parser context.
**            [f_poChain]      : The certificate chain.
**            [f_poChildCert]  : pointer to structure that contains information
**                               from the child certificate, cannot be NULL
**            [f_poParentCert] : pointer to structure with similar information
**                               from the parent certificate, cannot be NULL
**                               so this function will never be called for a chain with one cert.
**            [f_poChainSelwrityVersion]
**                             : The current security version for the chain.  This value should
**                               initially be set to the first parent certificate's security version.
**
** Notes:     This function should be called ONLY when iterating a chain from the leaf most
**            certificate to the root certificate.
**
******************************************************************************/
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_BCERTFORMAT_VerifyAdjacentCerts(
    __inout                  DRM_BCERTFORMAT_PARSER_CONTEXT_BASE        *f_pParserCtx,
    __in_ecount(1)     const DRM_BCERTFORMAT_CERT                       *f_poChildCert,
    __in_ecount(1)     const DRM_BCERTFORMAT_CERT                       *f_poParentCert,
    __inout                  DRM_BCERTFORMAT_SELWRITY_VERSION2          *f_poChainSelwrityVersion )
{
    DRM_RESULT                dr                        = DRM_SUCCESS;
    DRM_DWORD                 iCount                    = 0;
    DRM_DWORD                 dwParentKeyUsageMask      = 0;
    DRM_DWORD                 dwChildKeyUsageMask       = 0;
    DRM_BOOL                  fSupportSelwreClock       = FALSE;
    DRM_BOOL                  fSupportAntiRollbackClock = FALSE;
    DRM_BCERTFORMAT_KEY_TYPE *pKeyType                  = NULL;
    DRM_DWORD                 idx                       = 0;

    ChkArg( f_poChildCert != NULL );
    ChkArg( f_poParentCert != NULL );
    ChkArg( f_poChainSelwrityVersion != NULL );

    /*
    ** Parent cert type should be issuer because it has child certificate
    */
    DRM_BCERTFORMAT_CHKVERIFICATIONERR(
        f_pParserCtx,
        f_poParentCert->BasicInformation.dwType == DRM_BCERT_CERTTYPE_ISSUER,
        DRM_E_BCERT_ILWALID_CERT_TYPE );

    /*
    ** Security level of the parent cert is higher or equal to its child cert's.
    */
    DRM_BCERTFORMAT_CHKVERIFICATIONERR(
        f_pParserCtx,
        f_poChildCert->BasicInformation.dwSelwrityLevel <= f_poParentCert->BasicInformation.dwSelwrityLevel,
        DRM_E_BCERT_ILWALID_SELWRITY_LEVEL );

    /*
    ** Make sure the chain security version is consistent throughout the chain
    */
    if( !f_poChainSelwrityVersion->fValid )
    {
        DRM_BOOL fParentHasSelwrityVersion = FALSE;

        /*
        ** Update the chain security version with the parent version if available.
        */
        if( f_poParentCert->SelwrityVersion2.fValid )
        {
            DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION2( f_poChainSelwrityVersion, f_poParentCert );
            fParentHasSelwrityVersion = TRUE;
        }
        else if( f_poParentCert->SelwrityVersion.fValid )
        {
            DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION( f_poChainSelwrityVersion, f_poParentCert );
            fParentHasSelwrityVersion = TRUE;
        }

        if( fParentHasSelwrityVersion )
        {
            /*
            ** If Parent Cert has Security version,
            ** Child Cert MUST match the values if the cert has valid SelwrityVersion info..
            */
            ChkDR( _VerifyChildSelwrityVersionMatchesChain( f_poChildCert, f_poChainSelwrityVersion ) );
        }
    }

    /*
    ** Do not use "else if" here because the preceding block can modify f_poChainSelwrityVersion->fValid.
    ** If the Chain Security version is still not set by the parent, try to get it from the child Cert.
    */
    if( !f_poChainSelwrityVersion->fValid )
    {
        if( f_poChildCert->SelwrityVersion2.fValid )
        {
            DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION2( f_poChainSelwrityVersion, f_poChildCert );
        }
        else if( f_poChildCert->SelwrityVersion.fValid )
        {
            DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION( f_poChainSelwrityVersion, f_poChildCert );
        }
    }

    /*
    ** Issuer key for the current cert is the same as one of the key values in the child cert
    */
    dr = DRM_E_BCERT_ISSUERKEY_KEYINFO_MISMATCH;

    pKeyType = f_poParentCert->KeyInformation.pHead;

    for ( idx = 0; idx < f_poParentCert->KeyInformation.cEntries; idx++ )
    {
        AssertChkBOOL( XBBA_TO_CB( f_poChildCert->SignatureInformation.xbbaIssuerKey ) >= DRM_BCERT_MAX_PUBKEY_VALUE_LENGTH );
        AssertChkBOOL( XBBA_TO_CB( pKeyType[ idx ].xbbaKeyValue ) >= DRM_BCERT_MAX_PUBKEY_VALUE_LENGTH );

        if ( OEM_SELWRE_ARE_EQUAL(
                XBBA_TO_PB( f_poChildCert->SignatureInformation.xbbaIssuerKey ),
                XBBA_TO_PB( pKeyType[idx].xbbaKeyValue ),
                DRM_BCERT_MAX_PUBKEY_VALUE_LENGTH ) )
        {
            dr = DRM_SUCCESS;
            break;
        }
    }
    DRM_BCERTFORMAT_CHKVERIFICATIONERR( f_pParserCtx, DRM_SUCCEEDED( dr ), dr );
    dr = DRM_SUCCESS;

    /*
    ** Collect the key usages of the parent certificate for all keys in the certificate
    */
    for ( idx = 0; idx < f_poParentCert->KeyInformation.cEntries; idx++ )
    {
        DRM_DWORD        iUsage       = 0;
        const DRM_DWORD* dwlUsages    = XB_DWORD_LIST_TO_PDWORD( pKeyType[idx].dwlKeyUsages );

        for( iUsage = 0; iUsage < pKeyType[idx].dwlKeyUsages.cDWORDs; iUsage++ )
        {
            dwParentKeyUsageMask |= BCERT_KEYUSAGE_BIT( dwlUsages[iUsage] );
        }
    }

    /*
    ** Check that the current certificate has appropriate Issuer rights
    ** to issue a child certificate of particular type.
    */
    if ( !(dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT( DRM_BCERT_KEYUSAGE_ISSUER_ALL ) ) ) /* Issuer-All can issue anything */
    {
        switch( f_poChildCert->BasicInformation.dwType )
        {
            case DRM_BCERT_CERTTYPE_DOMAIN:
                /*
                ** Issuer-Domain can issue certificates of Type Domain.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_DOMAIN),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_DEVICE:
                /*
                ** Issuer-Device can issue certificate of Type Device.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_DEVICE),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_PC:
                /*
                ** Issuer-Indiv can issue certificate of Type PC.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_INDIV),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_SILVERLIGHT:
                /*
                ** Issuer-SilverLight can issue certificate of Type SilverLight.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_SILVERLIGHT),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_APPLICATION:
                /*
                ** Issuer-Application can issue certificate of Type Application.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_APPLICATION),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_METERING:
                /*
                ** Issuer-Metering can issue certificate of Type Metering.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_METERING),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_KEYFILESIGNER:
                /*
                ** Issuer-SignKeyFile can issue certificate of Type KeyFileSigner
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_SIGN_KEYFILE),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_SERVER:
                /*
                ** Issuer-Server can issue certificate of Type Server
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_SERVER),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_SELWRETIMESERVER:
                /*
                ** Issuer-SelwreTimeServer can issue certificate of Type SelwreTimeServer
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_SELWRETIMESERVER),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_RPROVMODELAUTH:
                /*
                ** Issuer-RProvModelAuth can issue certificate of Type RProvModelAuth
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT( DRM_BCERT_KEYUSAGE_ISSUER_RPROVMODELAUTH ),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_CRL_SIGNER:
                /*
                ** Issuer-CLR can issue certificate of Type CRL Signer
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_CRL), DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_ISSUER:
                /*
                ** Issuer cert is issued by another issuer cert, and Issuer-related key usages should match.
                */
                pKeyType = f_poChildCert->KeyInformation.pHead;
                for( idx = 0; idx < f_poChildCert->KeyInformation.cEntries; idx++ )
                {
                    DRM_DWORD        iUsage       = 0;
                    const DRM_DWORD* dwlUsages    = XB_DWORD_LIST_TO_PDWORD( pKeyType[idx].dwlKeyUsages );

                    for( iUsage = 0; iUsage < pKeyType[idx].dwlKeyUsages.cDWORDs; iUsage++ )
                    {
                        dwChildKeyUsageMask |= BCERT_KEYUSAGE_BIT( dwlUsages[iUsage] );
                    }
                }

                /*
                ** Check key usages in a child cert:
                ** the parent cert is not Issuer-All (already checked outside switch)
                ** and must have all issuer-... key usages that child cert has
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    DRM_SUCCEEDED( DRM_BCERTFORMAT_VerifyChildUsage( dwChildKeyUsageMask, dwParentKeyUsageMask ) ),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
            case DRM_BCERT_CERTTYPE_LICENSESIGNER:
                /*
                ** Issuer-SignLicense can issue certificate of Type LicenseSigner
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_SIGN_LICENSE),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;

            default:
                /*
                ** Service cert type. If it has a parent cert it can only be Issuer_All cert, so fail
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    f_poChildCert->BasicInformation.dwType == DRM_BCERT_CERTTYPE_SERVICE,
                    DRM_E_BCERT_ILWALID_CERT_TYPE );

                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    f_poChildCert->BasicInformation.dwType != DRM_BCERT_CERTTYPE_SERVICE, /* Always false, added to get around OACR warning */
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
        }
    }

    /*
    ** Some features in a child cert can only be issued by a parent cert with specific key usages.
    */
    for ( iCount = 0; iCount < f_poChildCert->FeatureInformation.dwlFeatures.cDWORDs; iCount++ )
    {
        const DRM_DWORD *dwlFeatures = XB_DWORD_LIST_TO_PDWORD( f_poChildCert->FeatureInformation.dwlFeatures );

        switch( dwlFeatures[ iCount ] )
        {
            case DRM_BCERT_FEATURE_TRANSMITTER:         __fallthrough;
            case DRM_BCERT_FEATURE_RECEIVER:            __fallthrough;
            case DRM_BCERT_FEATURE_SHARED_CERTIFICATE:  __fallthrough;
                /*
                ** Transmitter, Receiver, SharedCertificate can be issued by Issuer-Link.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_LINK),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                break;
           case DRM_BCERT_FEATURE_SELWRE_CLOCK:
                /*
                ** SelwreClock can be issued by Issuer-Device.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_DEVICE),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                fSupportSelwreClock = TRUE;
                break;
            case DRM_BCERT_FEATURE_ANTIROLLBACK_CLOCK:
                /*
                ** Anti-Rollback Clock can be issued by Issuer-Device.
                */
                DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                    f_pParserCtx,
                    dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT(DRM_BCERT_KEYUSAGE_ISSUER_DEVICE),
                    DRM_E_BCERT_ILWALID_KEY_USAGE );
                fSupportAntiRollbackClock = TRUE;
                break;
            default:
                break;
        }
    }

    /*
    ** Either SelwreClock or Anti-Rollback Clock can be issued in one certificate, never both.
    */
    DRM_BCERTFORMAT_CHKVERIFICATIONERR(
        f_pParserCtx,
        !( fSupportSelwreClock && fSupportAntiRollbackClock ),
        DRM_E_BCERT_ILWALID_KEY_USAGE );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    _VerifySignature
**
** Synopsis:    Default signature verification callback using the provided
**              issuer public key.
**
** Arguments:   [f_pOemTeeContext   ] : The OEM TEE context.
**              [f_wSignatureType   ] : The signature mechanism type.
**              [f_cbData           ] : The size of the signed data.
**              [f_pbData           ] : The signed data buffer.
**              [f_cbIssuerPublicKey] : The size of the issuer key used
**                                      to validate the signature.
**              [f_pbIssuerPublicKey] : The issuer key used to validate
**                                      the signature.
**              [f_cbSignature      ] : The size of the signature.
**              [f_pbSignature      ] : The signature buffer.
**
******************************************************************************/
static DRM_RESULT DRM_CALL _VerifySignature(
    __inout_opt                                 OEM_TEE_CONTEXT     *f_pOemTeeContext,
    __in                                        DRM_WORD             f_wSignatureType,
    __in                                        DRM_DWORD            f_cbData,
    __in_bcount( f_cbData )               const DRM_BYTE            *f_pbData,
    __in                                        DRM_DWORD            f_cbIssuerPublicKey,
    __in_bcount( f_cbIssuerPublicKey )    const DRM_BYTE            *f_pbIssuerPublicKey,
    __in                                        DRM_DWORD            f_cbSignature,
    __in_bcount( f_cbSignature )          const DRM_BYTE            *f_pbSignature )
{
    DRM_RESULT          dr         = DRM_SUCCESS;
    SIGNATURE_P256      oSig;
    PUBKEY_P256         oPubKey;

    ChkArg( f_pbData            != NULL );
    ChkArg( f_cbData             > 0    );
    ChkArg( f_pbSignature       != NULL );
    ChkArg( f_pbIssuerPublicKey != NULL );

    ChkArg( f_wSignatureType    == DRM_BCERT_SIGNATURE_TYPE_P256      );
    ChkArg( f_cbSignature       == ECDSA_P256_SIGNATURE_SIZE_IN_BYTES );
    ChkArg( f_cbIssuerPublicKey == sizeof(PUBKEY_P256)                );

    ChkVOID( OEM_SELWRE_MEMCPY( oSig.m_rgbSignature, f_pbSignature      , ECDSA_P256_SIGNATURE_SIZE_IN_BYTES ) );
    ChkVOID( OEM_SELWRE_MEMCPY( oPubKey.m_rgbPubkey, f_pbIssuerPublicKey, sizeof(PUBKEY_P256)                ) );

    /*
    ** Verify signature
    */
    ChkDR( Oem_Broker_ECDSA_P256_Verify(
        f_pOemTeeContext,
        NULL,
        f_pbData,
        f_cbData,
        &oPubKey,
        &oSig ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_VerifySignature(
    __in                            const DRM_BCERTFORMAT_CERT                  *f_pCertificate,
    __inout_opt                           OEM_TEE_CONTEXT                       *f_pOemTeeContext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pCertificate->SignatureInformation.fValid );
    ChkArg( NULL != XBBA_TO_PB( f_pCertificate->xbbaRawData ) );
    ChkBOOL( f_pCertificate->HeaderData.dwSignedLength < XBBA_TO_CB( f_pCertificate->xbbaRawData ), DRM_E_BCERT_ILWALID_SIGNEDCERT_LENGTH );
    ChkBOOL( NULL != XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaIssuerKey ), DRM_E_BCERT_ISSUERKEY_KEYINFO_MISMATCH );

    ChkDR( _VerifySignature(
        f_pOemTeeContext,
        f_pCertificate->SignatureInformation.wSignatureType,
        f_pCertificate->HeaderData.dwSignedLength,
        XBBA_TO_PB( f_pCertificate->xbbaRawData ),
        XBBA_TO_CB( f_pCertificate->SignatureInformation.xbbaIssuerKey ),
        XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaIssuerKey ),
        XBBA_TO_CB( f_pCertificate->SignatureInformation.xbbaSignature ),
        XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaSignature ) ) );

ErrorExit:
    return dr;
}

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _InitializeParserContextWithDefaults(
    __in                DRM_BOOL                        f_fCheckSignature,
    __inout_ecount( 1 ) DRM_BCERTFORMAT_PARSER_CONTEXT *f_pParserCtx )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRMASSERT( f_pParserCtx != NULL );
    ChkDR( DRM_BCERTFORMAT_InitializeParserContext(
        f_fCheckSignature,
        FALSE,                                      /* f_fDontFailOnMissingExtData      */
        0,                                          /* f_cRequiredKeyUsages             */
        NULL,                                       /* f_pdwRequiredKeyUsages           */
        0,                                          /* f_cResults                       */
        NULL,                                       /* f_pResults                       */
        f_pParserCtx ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_OverrideRootPublicKey
**
** Synopsis:    Overrides the root public key used for validation when parsing or verifying
**              a certificate chain.  The default root public key is the PlayReady ROOT CA
**              public key defined at the top of this file (g_ECC256MSPlayReadyRootIssuerPubKeyBCERT).
**
** Arguments:
**      [f_pParserCtx]                     : The BCERT format parser context.
**      [f_pRootPubKey]                    : The expected root public key for the certificate chain.
**                                           If the root certificate in the chain does contain this
**                                           public key, parsing will fail.  Passing NULL will skip
**                                           root public key validation.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_OverrideRootPublicKey(
    __inout_ecount( 1 )                     DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __in_ecount_opt( 1 )              const PUBKEY_P256                           *f_pRootPubKey )
{
    DRM_RESULT                           dr   = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT_FULL *pCtx = DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( f_pParserCtx );

    ChkArg( f_pParserCtx != NULL );

    if( !pCtx->oBaseCtx.fInitialized )
    {
        ChkDR( _InitializeParserContextWithDefaults( TRUE /* f_fCheckSignature */, f_pParserCtx ) );
    }
    else
    {
        AssertChkBOOL( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( &pCtx->oBaseCtx ) );
    }

    if( f_pRootPubKey != NULL )
    {
        pCtx->oRootPubKey    = *f_pRootPubKey;
        pCtx->fRootPubKeySet = TRUE;
    }
    else
    {
        pCtx->fRootPubKeySet = FALSE;
    }

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_InitializeParserContext
**
** Synopsis:    Initializes BCERT parser context.  The parameters of this function are used when
**              validating the certificate chain.  This function is optional and must be called
**              prior to DRM_BCERTFORMAT_ParseCertificateChain, otherwise, default values will
**              be used for the validation properties.
**
** Arguments:
**      [f_fCheckSignature]                : Determines if the signature should be validated.
**      [f_fDontFailOnMissingExtData]      : If TRUE, no validation of the extended certificate
**                                           data will occur even if the certificate flag says
**                                           their is extended data.  If FALSE, parsing will fail
**                                           if the certificate flags say there should be extended
**                                           data and there is not.
**      [f_cRequiredKeyUsages]             : The count of required DWORD key usages in parameter
**                                           f_pdwRequiredKeyUsages.
**      [f_pdwRequiredKeyUsages]           : Optional. An array of DWORD key usage values that are
**                                           required by the certificate chain.  If the set of usages
**                                           for the entire chain does not include each of these usages,
**                                           parsing will fail.
**      [f_cResults]                       : The number of non-critical failed results to store before
**                                           aborting the parsing operation.  If this value is set to
**                                           zero, then any error will stop parsing immediately.
**      [f_pResults]                       : Optional. The array of verification results that will be
**                                           updated with non-critical failures during the certificate
**                                           parsing.  Any non-critical failures will still fail parsing.
**                                           This just allows minor errors to accumulate before exiting
**                                           parsing.
**      [f_pParserCtx]                     : The BCERT format parser context.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_InitializeParserContext(
    __in                                            DRM_BOOL                               f_fCheckSignature,
    __in                                            DRM_BOOL                               f_fDontFailOnMissingExtData,
    __in                                            DRM_DWORD                              f_cRequiredKeyUsages,
    __in_ecount_opt( f_cRequiredKeyUsages )   const DRM_DWORD                             *f_pdwRequiredKeyUsages,
    __in                                            DRM_DWORD                              f_cResults,
    __in_ecount_opt( f_cResults )                   DRM_BCERTFORMAT_VERIFICATIONRESULT    *f_pResults,
    __inout_ecount( 1 )                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx )
{
    DRM_RESULT                           dr       = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT_FULL *pCtx     = DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( f_pParserCtx );

    ChkArg( f_pParserCtx != NULL );
    ChkArg( ( f_cResults == 0 ) == ( f_pResults == NULL ) );

    ZEROMEM( f_pParserCtx, sizeof(*pCtx) );

    if( f_cRequiredKeyUsages > 0 )
    {
        DRM_DWORD iCount;

        ChkArg( f_pdwRequiredKeyUsages != NULL );
        for ( iCount = 0; iCount < f_cRequiredKeyUsages; iCount++ )
        {
            pCtx->dwRequiredKeyUsageMask |= BCERT_KEYUSAGE_BIT( f_pdwRequiredKeyUsages[iCount] ); /* collect required key usages */
        }
    }

    pCtx->fCheckSignature           = f_fCheckSignature;
    pCtx->fDontFailOnMissingExtData = f_fDontFailOnMissingExtData;
    pCtx->iResult                   = 0;
    pCtx->cResults                  = f_cResults;
    pCtx->pResults                  = f_pResults;
    pCtx->oBaseCtx.eContextType     = DRM_BCERTFORMAT_CONTEXT_TYPE_FULL;
    pCtx->oBaseCtx.fInitialized     = TRUE;
    pCtx->oRootPubKey               = g_ECC256MSPlayReadyRootIssuerPubKeyBCERT;
    pCtx->fRootPubKeySet            = TRUE;

ErrorExit:
    return dr;
}
#endif

#if defined (SEC_COMPILE)
/*****************************************************************************
** Function:    _DRM_BCERTFORMAT_ParseCertficate
**
** Synopsis:    Function that parses one certificate in a chain.
**              For performance reasons it does not verify the certificate signature.  Signatures
**              are verified during the chain parsing.
**
** Arguments:   [f_pParserCtx]                  : The BCERT format parser context.
**              [f_cbCertificate]               : The size of the certificate.
**              [f_pbCertificate]               : The serialized certificate data.
**              [f_pcbParsed]                   : Output parameter, size of the parsed output (in bytes)
**              [f_pdwKeyUsageSet]              : Output parameter, the mask of all key usages in the
**                                                certificate chain.
**              [f_pCertificate]                : Output parameter, pointer to the parsed certificate
**              [f_fForceSkipSignatureCheck]    : If TRUE, skip signature check regardless of other parameters.
**                                                Otherwise, perform signature check based on other parameters.
**
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_BCERTFORMAT_ParseCertificate(
    __inout                                 DRM_BCERTFORMAT_PARSER_CONTEXT_INTERNAL *f_pParserCtx,
    __in                                    DRM_DWORD                                f_cbCertificate,
    __in_bcount( f_cbCertificate )    const DRM_BYTE                                *f_pbCertificate,
    __out_opt                               DRM_DWORD                               *f_pcbParsed,
    __out_opt                               DRM_DWORD                               *f_pdwKeyUsageSet,
    __out                                   DRM_BCERTFORMAT_CERT                    *f_pCertificate,
    __in                                    DRM_BOOL                                 f_fForceSkipSignatureCheck )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT                           dr            = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT_BASE *pCtx          = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_PARSER_CONTEXT_BASE, f_pParserCtx );
    DRM_DWORD                            dwVersion     = 0;
    DRM_DWORD                            idx           = 0;
    DRM_DWORD                            cbParsed      = 0;
    DRM_DWORD                            dwKeyUsageSet = 0;

    ChkArg( f_pParserCtx    != NULL );
    ChkArg( f_pbCertificate != NULL );
    ChkArg( f_cbCertificate  > 0    );
    ChkArg( f_pCertificate  != NULL );
    ChkArg( pCtx->fInitialized );

    ZEROMEM( f_pCertificate, sizeof( *f_pCertificate ) );

    ChkDR( DRM_XB_UnpackBinary(
        f_pbCertificate,
        f_cbCertificate,
        pCtx->pStack,
        s_DRM_BCERTFORMAT_CERT_FormatDescription,
        DRM_NO_OF( s_DRM_BCERTFORMAT_CERT_FormatDescription ),
        &dwVersion,
        &cbParsed,
        f_pCertificate ) );

    /*
    ** Update key usage set.
    */
    ChkBOOL( f_pCertificate->KeyInformation.cEntries <= DRM_BCERT_MAX_KEY_USAGES, DRM_E_BCERT_TOO_MANY_PUBLIC_KEYS );
    for( idx = 0; idx < f_pCertificate->KeyInformation.cEntries; idx++ )
    {
        DRM_DWORD iUsage = 0;
        DRM_BCERTFORMAT_KEY_TYPE *pKeyType = &f_pCertificate->KeyInformation.pHead[ idx ];
        const DRM_DWORD          *pdwUsage = XB_DWORD_LIST_TO_PDWORD( pKeyType->dwlKeyUsages );

        ChkBOOL( pKeyType->dwlKeyUsages.cDWORDs > 0, DRM_E_BCERT_KEY_USAGES_NOT_SPECIFIED );

        for( iUsage = 0; iUsage < pKeyType->dwlKeyUsages.cDWORDs; iUsage++ )
        {
            pKeyType->dwUsageSet |= BCERT_KEYUSAGE_BIT( pdwUsage[ iUsage ] );
        }

        dwKeyUsageSet |= pKeyType->dwUsageSet;
    }

    /*
    ** Update the certificate feature set.
    */
    f_pCertificate->FeatureInformation.dwFeatureSet = 0;
    {
        const DRM_DWORD *pdwFeatures = XB_DWORD_LIST_TO_PDWORD( f_pCertificate->FeatureInformation.dwlFeatures );

        if( pdwFeatures != NULL )
        {
            for( idx = 0; idx < f_pCertificate->FeatureInformation.dwlFeatures.cDWORDs; idx++ )
            {
                f_pCertificate->FeatureInformation.dwFeatureSet |= BCERT_FEATURE_BIT( pdwFeatures[ idx ] );
            }
        }
    }

    /* Caller can forcibly skip signature checking regardless of other parameters */
    if( !f_fForceSkipSignatureCheck )
    {
        /* Check the signature if either we are using the base context or the full context indicates we should check the signature. */
        if( !DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) || DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->fCheckSignature )
        {
            ChkArg( f_pCertificate->SignatureInformation.fValid );
            ChkBOOL( f_pCertificate->HeaderData.dwSignedLength < f_cbCertificate, DRM_E_BCERT_ILWALID_SIGNEDCERT_LENGTH );
            ChkBOOL( NULL != XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaIssuerKey ), DRM_E_BCERT_ISSUERKEY_KEYINFO_MISMATCH );

            ChkDR( _VerifySignature(
                pCtx->pOemTeeCtx,
                f_pCertificate->SignatureInformation.wSignatureType,
                f_pCertificate->HeaderData.dwSignedLength,
                f_pbCertificate,
                XBBA_TO_CB( f_pCertificate->SignatureInformation.xbbaIssuerKey ),
                XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaIssuerKey ),
                XBBA_TO_CB( f_pCertificate->SignatureInformation.xbbaSignature ),
                XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaSignature ) ) );
        }
    }

    /*
    ** Check if the certificate is expired - optional
    */
    if( DoExpirationCheck( pCtx->ftLwrrentTime ) )
    {
        DRM_UINT64  ui64LwrrentTime;
        DRM_UINT64  uiCertTime;
        DRMFILETIME ftCertTime;

        FILETIME_TO_UI64( pCtx->ftLwrrentTime, ui64LwrrentTime );
        DRM_CREATE_FILE_TIME( f_pCertificate->BasicInformation.dwExpirationDate, ftCertTime );
        FILETIME_TO_UI64( ftCertTime, uiCertTime );

        DRM_BCERTFORMAT_CHKVERIFICATIONERR( pCtx, !DRM_UI64Les( uiCertTime, ui64LwrrentTime ), DRM_E_BCERT_BASICINFO_CERT_EXPIRED );
    }

    /*
    ** Validate domain URL string is null terminated
    */
    if( f_pCertificate->DomainInformation.fValid && f_pCertificate->DomainInformation.xbbaDomainUrl.fValid )
    {
        DRM_RESULT drTmp = DRM_STR_UTF8_IsUTF8NullTerminatedData(
            XBBA_TO_PB( f_pCertificate->DomainInformation.xbbaDomainUrl ),
            XBBA_TO_CB( f_pCertificate->DomainInformation.xbbaDomainUrl ) );
        DRM_BCERTFORMAT_CHKVERIFICATIONERR( pCtx, DRM_SUCCEEDED( drTmp ), drTmp );
    }

    /*
    ** Validate metering URL string is null terminated
    */
    if( f_pCertificate->MeteringInformation.fValid && f_pCertificate->MeteringInformation.xbbaMeteringUrl.fValid )
    {
        DRM_RESULT drTmp = DRM_STR_UTF8_IsUTF8NullTerminatedData(
            XBBA_TO_PB( f_pCertificate->MeteringInformation.xbbaMeteringUrl ),
            XBBA_TO_CB( f_pCertificate->MeteringInformation.xbbaMeteringUrl ) );
        DRM_BCERTFORMAT_CHKVERIFICATIONERR( pCtx, DRM_SUCCEEDED( drTmp ), drTmp );
    }

    /*
    ** Validate manufacturer strings are null terminated
    */
    if( f_pCertificate->ManufacturerInformation.fValid )
    {
        DRM_RESULT drTmp;

        if( f_pCertificate->ManufacturerInformation.xbbaManufacturerName.fValid && XBBA_TO_CB( f_pCertificate->ManufacturerInformation.xbbaManufacturerName ) > 0 )
        {
            drTmp = DRM_STR_UTF8_IsUTF8NullTerminatedData(
                XBBA_TO_PB( f_pCertificate->ManufacturerInformation.xbbaManufacturerName ),
                XBBA_TO_CB( f_pCertificate->ManufacturerInformation.xbbaManufacturerName ) );
            DRM_BCERTFORMAT_CHKVERIFICATIONERR( pCtx, DRM_SUCCEEDED( drTmp ), drTmp );
        }
        if( f_pCertificate->ManufacturerInformation.xbbaModelName.fValid && XBBA_TO_CB( f_pCertificate->ManufacturerInformation.xbbaModelName ) > 0 )
        {
            drTmp = DRM_STR_UTF8_IsUTF8NullTerminatedData(
                XBBA_TO_PB( f_pCertificate->ManufacturerInformation.xbbaModelName ),
                XBBA_TO_CB( f_pCertificate->ManufacturerInformation.xbbaModelName ) );
            DRM_BCERTFORMAT_CHKVERIFICATIONERR( pCtx, DRM_SUCCEEDED( drTmp ), drTmp );
        }
        if( f_pCertificate->ManufacturerInformation.xbbaModelNumber.fValid && XBBA_TO_CB( f_pCertificate->ManufacturerInformation.xbbaModelNumber ) > 0 )
        {
            drTmp = DRM_STR_UTF8_IsUTF8NullTerminatedData(
                XBBA_TO_PB( f_pCertificate->ManufacturerInformation.xbbaModelNumber ),
                XBBA_TO_CB( f_pCertificate->ManufacturerInformation.xbbaModelNumber ) );
            DRM_BCERTFORMAT_CHKVERIFICATIONERR( pCtx, DRM_SUCCEEDED( drTmp ), drTmp );
        }
    }

    /*
    ** Validate extended data record signature
    */
    if( ( f_pCertificate->BasicInformation.dwFlags & DRM_BCERT_FLAGS_EXTDATA_PRESENT ) == DRM_BCERT_FLAGS_EXTDATA_PRESENT )
    {
        if( f_pCertificate->ExDataContainer.fValid )
        {
            if( f_pCertificate->ExDataContainer.ExDataSignatureInformation.fValid )
            {
                DRM_BYTE *pbSignData = XBBA_TO_PB( f_pCertificate->ExDataContainer.xbbaRawData );
                DRM_DWORD cbSignData = XBBA_TO_CB( f_pCertificate->ExDataContainer.xbbaRawData );

                ChkBOOL( pbSignData != NULL, DRM_E_BCERT_ILWALID_EXTDATARECORD );

                ChkDR( DRM_DWordSubSame( &cbSignData, ECDSA_P256_SIGNATURE_SIZE_IN_BYTES ) );
                ChkDR( DRM_DWordSubSame(
                    &cbSignData,
                    DRM_BCERT_OBJECT_HEADER_LEN + sizeof( DRM_WORD ) + sizeof( DRM_WORD ) ) );  /* object header, signature type, signature size */

                /* Skip container header */
                ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pbSignData, DRM_BCERT_OBJECT_HEADER_LEN, (DRM_SIZE_T *)&pbSignData ) );
                ChkDR( DRM_DWordSubSame( &cbSignData, DRM_BCERT_OBJECT_HEADER_LEN ) );
                ChkBOOL( cbSignData < f_cbCertificate, DRM_E_BCERT_ILWALID_EXTDATARECORD );

                ChkBOOL( NULL != XBBA_TO_PB( f_pCertificate->ExDataSigKeyInfo.xbbKeyValue ), DRM_E_BCERT_ILWALID_EXTDATARECORD );

                ChkDR( _VerifySignature(
                    pCtx->pOemTeeCtx,
                    f_pCertificate->ExDataContainer.ExDataSignatureInformation.wSignatureType,
                    cbSignData,
                    pbSignData,
                    XBBA_TO_CB( f_pCertificate->ExDataSigKeyInfo.xbbKeyValue ),
                    XBBA_TO_PB( f_pCertificate->ExDataSigKeyInfo.xbbKeyValue ),
                    XBBA_TO_CB( f_pCertificate->ExDataContainer.ExDataSignatureInformation.xbbaSignature ),
                    XBBA_TO_PB( f_pCertificate->ExDataContainer.ExDataSignatureInformation.xbbaSignature ) ) );
            }
        }
        else
        {
            /*
            ** This is a "partially built" certificate, record DRM_E_BCERT_HWIDINFO_IS_MISSING code.
            ** The caller will callwlate where the next cert begins.
            ** The fDontFailOnMissingExtData field is only avaliable for the full context (non-TEE calls), so
            ** all calls with the base context must have the HWIDINFO field set.
            */
            ChkBOOL( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL(pCtx) && DRM_BCERTFORMAT_PARSER_CTX_TO_FULL(pCtx)->fDontFailOnMissingExtData, DRM_E_BCERT_HWIDINFO_IS_MISSING );
        }
    }


    if( f_pdwKeyUsageSet != NULL )
    {
        *f_pdwKeyUsageSet = dwKeyUsageSet;
    }

    if( f_pcbParsed != NULL )
    {
        *f_pcbParsed = cbParsed;
    }

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_GetCertificate
**
** Synopsis:    Function that parses one certificate in a chain.
**              For performance reasons it does not verify the certificate signature.
**              Signature verification is performed during the parsing of the certificate
**              chain.
**
** Arguments:   [f_pParserCtx]          : The BCERT format parser context.
**              [f_pCertificateHeader]  : The certificate header for the certificate to be parsed.
**                                        The certificate header a member variable of the certificate
**                                        chain object DRM_BCERTFORMAT_CHAIN.  This is populated by
**                                        the call to DRM_BCERTFORMAT_ParseCertificateChain.
**              [f_pcbParsed]           : Optional output parameter, size of the parsed output (in bytes)
**              [f_pCert]               : Optional output parameter, pointer to the parsed certificate
**                                        If the caller is internal to the certificate parsing code, it
**                                        may pass NULL and directly use the cache in f_pParserCtx instead.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetCertificate(
    __inout_ecount( 1 )                     DRM_BCERTFORMAT_PARSER_CONTEXT          *f_pParserCtx,
    __in                              const DRM_BCERTFORMAT_CERT_HEADER             *f_pCertificateHeader,
    __out_ecount_opt( 1 )                   DRM_DWORD                               *f_pcbParsed,
    __out_ecount_opt( 1 )                   DRM_BCERTFORMAT_CERT                    *f_pCert )
{
    DRM_RESULT                           dr   = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT_FULL *pCtx = DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( f_pParserCtx );

    ChkArg( f_pParserCtx         != NULL );
    ChkArg( f_pCertificateHeader != NULL );
    ChkArg( pCtx->oBaseCtx.fInitialized );

    AssertChkBOOL( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( &pCtx->oBaseCtx ) );

    ChkArg( XBBA_TO_PB( f_pCertificateHeader->xbbaRawData ) != NULL );
    ChkArg( XBBA_TO_CB( f_pCertificateHeader->xbbaRawData ) != 0    );

    AssertChkBOOL( f_pCertificateHeader->dwIndex < DRM_NO_OF( pCtx->rgoCertCache ) );

    if( f_pcbParsed != NULL )
    {
        *f_pcbParsed = 0;
    }

    if( !pCtx->rgoCertCache[ f_pCertificateHeader->dwIndex ].fValid )
    {
        ChkDR( _DRM_BCERTFORMAT_ParseCertificate(
            f_pParserCtx,
            XBBA_TO_CB( f_pCertificateHeader->xbbaRawData ),
            XBBA_TO_PB( f_pCertificateHeader->xbbaRawData ),
            f_pcbParsed,
            NULL,
            &pCtx->rgoCertCache[ f_pCertificateHeader->dwIndex ],
            FALSE ) );
    }

    if( f_pCert != NULL )
    {
        MEMCPY( f_pCert, &pCtx->rgoCertCache[ f_pCertificateHeader->dwIndex ], sizeof( *f_pCert ) );
    }

ErrorExit:
    return dr;
}

//
// LWE (nkuo) - rgdwKeyUsages and rgdwChainDataKeyIndex are const data which will be compiled as resident data. Thus they
// are moved outside the function so that we can assign section attribute to them so that they will be put into data overlay
// and we can save the precious DMEM space
//
DRM_GLOBAL_CONST DRM_DWORD s_rgdwKeyUsages[5] PR_ATTR_DATA_OVLY(_s_rgdwKeyUsages) = {
    DRM_BCERT_KEYUSAGE_SIGN,
    DRM_BCERT_KEYUSAGE_ENCRYPT_KEY,
    DRM_BCERT_KEYUSAGE_PRND_ENCRYPT_KEY_DEPRECATED,
    DRM_BCERT_KEYUSAGE_ENCRYPTKEY_SAMPLE_PROTECTION_AES128CTR,
    DRM_BCERT_KEYUSAGE_SIGN_CRL };

DRM_GLOBAL_CONST DRM_DWORD s_rgdwChainDataKeyIndex[5] PR_ATTR_DATA_OVLY(_s_rgdwChainDataKeyIndex) = {
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__ENCRYPT,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__PRND_ENCRYPT_DEPRECATED,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__SAMPLEPROT,
    DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN_CRL };

static DRM_RESULT DRM_CALL _GetChainDataFromCert(
    __in    const DRM_BCERTFORMAT_CERT              *f_pCert,
    __inout       DRM_BCERTFORMAT_PARSER_CHAIN_DATA *f_pChainData )
{
    DRM_RESULT                      dr       = DRM_SUCCESS;
    DRM_DWORD                       idxKey;  /* Loop variable */
    const DRM_BCERTFORMAT_KEY_TYPE *pKeyType = NULL;

    DRMASSERT( f_pCert != NULL );
    DRMASSERT( f_pChainData != NULL );

    f_pChainData->dwLeafFeatureSet              = f_pCert->FeatureInformation.dwFeatureSet;
    f_pChainData->dwLeafSelwrityLevel           = f_pCert->BasicInformation.dwSelwrityLevel;
    f_pChainData->oManufacturingName.cbString   = XBBA_TO_CB( f_pCert->ManufacturerInformation.xbbaManufacturerName );
    f_pChainData->oManufacturingName.pbString   = XBBA_TO_PB( f_pCert->ManufacturerInformation.xbbaManufacturerName );
    f_pChainData->oManufacturingModel.cbString  = XBBA_TO_CB( f_pCert->ManufacturerInformation.xbbaModelName );
    f_pChainData->oManufacturingModel.pbString  = XBBA_TO_PB( f_pCert->ManufacturerInformation.xbbaModelName );
    f_pChainData->oManufacturingNumber.cbString = XBBA_TO_CB( f_pCert->ManufacturerInformation.xbbaModelNumber );
    f_pChainData->oManufacturingNumber.pbString = XBBA_TO_PB( f_pCert->ManufacturerInformation.xbbaModelNumber );

    pKeyType = f_pCert->KeyInformation.pHead;

    DRMCASSERT( DRM_NO_OF( f_pChainData->rgoLeafPubKeys ) >= DRM_BCERTFORMAT_CHAIN_DATA_KEY__COUNT );

    for( idxKey = 0; idxKey < f_pCert->KeyInformation.cEntries; idxKey++ )
    {
        DRM_DWORD idxUsage; /* Loop variable */

        DRMCASSERT( DRM_NO_OF( s_rgdwKeyUsages ) == DRM_NO_OF( s_rgdwChainDataKeyIndex ) );

        for( idxUsage = 0; idxUsage < DRM_NO_OF( s_rgdwKeyUsages ); idxUsage++ )
        {
            if( DRM_BCERT_IS_KEYUSAGE_SUPPORTED( pKeyType[ idxKey ].dwUsageSet, s_rgdwKeyUsages[ idxUsage ] ) )
            {
                ChkBOOL( !DRM_BCERTFORMAT_PARSER_CHAIN_DATA_IS_KEY_VALID( f_pChainData->dwValidKeyMask, s_rgdwChainDataKeyIndex[ idxUsage ] ), DRM_E_BCERT_ILWALID_KEY_USAGE );
                ChkBOOL( sizeof( PUBKEY_P256 ) == XBBA_TO_CB( pKeyType[ idxKey ].xbbaKeyValue ), DRM_E_BCERT_ILWALID_KEY_LENGTH );
                ChkVOID( OEM_SELWRE_MEMCPY( &f_pChainData->rgoLeafPubKeys[ s_rgdwChainDataKeyIndex[ idxUsage ] ], XBBA_TO_PB( pKeyType[ idxKey ].xbbaKeyValue ), sizeof( PUBKEY_P256 ) ) );
                f_pChainData->dwValidKeyMask |= DRM_BCERTFORMAT_PARSER_CHAIN_DATA_KEY_BIT( s_rgdwChainDataKeyIndex[ idxUsage ] );
            }
        }
    }

ErrorExit:
    return dr;
}

#define DRM_BCERTFORMAT_ASSIGN_OR_ZERO( lhs, rhs ) DRM_DO {     \
    if( (lhs) != NULL )                                         \
    {                                                           \
        ChkVOID( ZEROMEM( lhs, sizeof(*(lhs)) ) );              \
    }                                                           \
    else                                                        \
    {                                                           \
        lhs = (rhs);                                            \
    }                                                           \
} DRM_WHILE_FALSE


static DRM_RESULT DRM_CALL _ValidateRootPublicKey(
    __inout         DRM_BCERTFORMAT_PARSER_CONTEXT_BASE *f_pCtx,
    __in      const DRM_BCERTFORMAT_CERT                *f_pCertificate )
{
    DRM_RESULT         dr          = DRM_SUCCESS;
    const PUBKEY_P256 *pRootPubKey = NULL;

    if( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( f_pCtx ) )
    {
        /*
        ** For the full context (normal world code path) the root public issuer signing key
        ** can be overriden or set to NULL indicating skipping this check.  Skipping the
        ** check is ONLY used for testing purposes.
        */
        if( DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( f_pCtx )->fRootPubKeySet )
        {
            pRootPubKey = &DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( f_pCtx )->oRootPubKey;
        }
    }
    else
    {
        pRootPubKey = &g_ECC256MSPlayReadyRootIssuerPubKeyBCERT;
    }

    if( pRootPubKey != NULL )
    {
        ChkBOOL( XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaIssuerKey ) != NULL, DRM_E_BCERT_ISSUERKEY_KEYINFO_MISMATCH );
        ChkBOOL( OEM_SELWRE_ARE_EQUAL( XBBA_TO_PB( f_pCertificate->SignatureInformation.xbbaIssuerKey ), pRootPubKey, sizeof( *pRootPubKey ) ), DRM_E_BCERT_ISSUERKEY_KEYINFO_MISMATCH );
    }

ErrorExit:
    return dr;
}

/*
** This function is the common code for parsing a certificate chain. The context can either be the
** DRM_BCERTFORMAT_PARSER_CONTEXT_BASE or DRM_BCERTFORMAT_PARSER_CONTEXT_FULL based on the caller.
** All TEE implementations will call with the base context and normal world calls will be with the
** full context.
*/
static DRM_RESULT DRM_CALL _ParseCertificateChain(
    __inout                               DRM_BCERTFORMAT_PARSER_CONTEXT_INTERNAL *f_pParserCtx,
    __inout_opt                           OEM_TEE_CONTEXT                         *f_pOemTeeContext,
    __in_opt                        const DRMFILETIME                             *f_pftLwrrentTime,
    __in                                  DRM_DWORD                                f_dwExpectedLeafCertType,
    __inout                               DRM_STACK_ALLOCATOR_CONTEXT             *f_pStack,
    __in                                  DRM_DWORD                                f_cbCertData,
    __in_bcount( f_cbCertData )     const DRM_BYTE                                *f_pbCertData,
    __inout_opt                           DRM_DWORD                               *f_pcbParsed,
    __out_opt                             DRM_BCERTFORMAT_CHAIN                   *f_pChain,
    __out_opt                             DRM_BCERTFORMAT_CERT                    *f_pLeafMostCert,
    __out_opt                             DRM_BCERTFORMAT_PARSER_CHAIN_DATA       *f_pChainData,
    __in                                  DRM_BOOL                                 f_fForceSkipSignatureCheck )
{
    DRM_RESULT                           dr             = DRM_SUCCESS;
    DRM_DWORD                            dwOffset       = 0;
    DRM_DWORD                            cbParsed       = 0;
    DRM_DWORD                            dwResultTemp   = 0;
    DRM_DWORD                            iCert          = 0;
    DRM_BCERTFORMAT_CERT_HEADER         *pCertHeaders   = NULL;
    DRM_DWORD                            dwKeyUsageSet  = 0;
    DRM_RESULT                           drOriginal     = DRM_SUCCESS;
    DRM_BCERTFORMAT_SELWRITY_VERSION2    oChainSecVer   = {0};
    DRM_BCERTFORMAT_CHAIN               *pChain         = NULL;
    DRM_BCERTFORMAT_CERT                *pCerts         = NULL;
    DRM_BCERTFORMAT_PARSER_CONTEXT_BASE *pCtx           = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_PARSER_CONTEXT_BASE, f_pParserCtx );

    ChkArg( f_pStack     != NULL );
    ChkArg( f_pParserCtx != NULL );
    ChkArg( f_pbCertData != NULL );

    DRMASSERT( pCtx->fInitialized );

    if( !DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) )
    {
        ChkDR( Oem_Broker_MemAlloc( 2 * sizeof(DRM_BCERTFORMAT_CERT), ( DRM_VOID ** )&pCerts ) );
    }

    if( f_pChain == NULL )
    {
        ChkDR( Oem_Broker_MemAlloc( sizeof(*pChain), ( DRM_VOID ** )&pChain ) );
    }

    DRM_BCERTFORMAT_ASSIGN_OR_ZERO( f_pChain       , pChain );
    DRM_BCERTFORMAT_ASSIGN_OR_ZERO( f_pLeafMostCert, NULL   );
    DRM_BCERTFORMAT_ASSIGN_OR_ZERO( f_pChainData   , NULL   );

    if( f_pftLwrrentTime != NULL )
    {
        pCtx->ftLwrrentTime = *f_pftLwrrentTime;
    }
    else
    {
        pCtx->ftLwrrentTime.dwHighDateTime = 0;
        pCtx->ftLwrrentTime.dwLowDateTime  = 0;
    }

    pCtx->dwExpectedLeafCertType = f_dwExpectedLeafCertType;
    pCtx->pOemTeeCtx             = f_pOemTeeContext;
    pCtx->pStack                 = f_pStack;

    dr = DRM_XB_UnpackHeader(
        f_pbCertData,
        f_cbCertData,
        pCtx->pStack,
        s_DRM_BCERTFORMAT_CHAIN_FormatDescription,
        DRM_NO_OF(s_DRM_BCERTFORMAT_CHAIN_FormatDescription),
        NULL,
        &cbParsed,
        NULL,
        NULL,
        f_pChain );

    /*
    ** If the header could not be unpacked then we may have a single certificate with the
    ** "CERT" header.  Therefore, we populate the chain information for a single certificate
    ** and attempt to parse the certificate data.
    */
    if( dr == DRM_E_XB_ILWALID_OBJECT )
    {
        f_pChain->Header.fValid            = TRUE;
        f_pChain->Header.cbChain           = f_cbCertData;
        f_pChain->Header.cCerts            = 1;
        f_pChain->Header.dwFlags           = 0;
        f_pChain->Header.dwVersion         = DRM_BCERTFORMAT_LWRRENT_VERSION;
        f_pChain->xbbaRawData.fValid       = TRUE;
        f_pChain->xbbaRawData.iData        = 0;
        f_pChain->xbbaRawData.cbData       = f_cbCertData;
        f_pChain->xbbaRawData.pbDataBuffer = ( DRM_BYTE * )f_pbCertData;

        drOriginal = DRM_E_BCERT_ILWALID_CHAIN_HEADER_TAG;
        dr = DRM_SUCCESS;
    }
    ChkDR( dr );

    ChkDR( DRM_DWordAddSame( &dwOffset, cbParsed ) );

    ChkDR( DRM_DWordMult( f_pChain->Header.cCerts, sizeof(*pCertHeaders), &dwResultTemp ) );
    ChkDR( DRM_STK_Alloc( pCtx->pStack, dwResultTemp, ( DRM_VOID ** )&pCertHeaders ) );
    ZEROMEM( pCertHeaders, dwResultTemp );

    f_pChain->Certificates.fValid       = TRUE;
    f_pChain->Certificates.pCertHeaders = pCertHeaders;
    f_pChain->Certificates.cEntries     = f_pChain->Header.cCerts;

    /*
    ** There should be 1-6 certificates in a chain
    */
    ChkBOOL( f_pChain->Header.cCerts > 0, DRM_E_BCERT_ILWALID_MAX_LICENSE_CHAIN_DEPTH );
    ChkBOOL( f_pChain->Header.cCerts <= DRM_BCERT_MAX_CERTS_PER_CHAIN, DRM_E_BCERT_ILWALID_MAX_LICENSE_CHAIN_DEPTH );

    /* default to no expiration */
    f_pChain->dwExpiration = 0xFFFFFFFF;

    /*
    ** Version is always 1
    */
    ChkBOOL( f_pChain->Header.dwVersion == DRM_BCERTFORMAT_LWRRENT_VERSION, DRM_E_BCERT_ILWALID_CHAIN_VERSION );

    for( iCert = 0; iCert < f_pChain->Header.cCerts; iCert++ )
    {
        DRM_DWORD                    dwCertKeyUsageSet = 0;
        OEM_SHA256_DIGEST            oDigest;          /* Initialized by OEM_SHA256_Finalize */
        DRM_DWORD                    dwExpiration;     /* Initialized with cert expiration value */
        DRM_BCERTFORMAT_CERT        *pParentCert       = NULL;
        const DRM_BCERTFORMAT_CERT  *pChildCert        = NULL;
        DRM_BOOL                     fValidCertHash    = FALSE;

        ChkBOOL( dwOffset < f_cbCertData, DRM_E_BUFFERTOOSMALL );

        if( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) )
        {
            pParentCert = &DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->rgoCertCache[ iCert ];
            pChildCert  = iCert > 0 ? &DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->rgoCertCache[ iCert - 1 ] : NULL;
        }
        else
        {
            /* Use either the leaf most cert parameter or the alternate between the first and second in the cert buffer. */
            pParentCert = iCert == 0 && f_pLeafMostCert != NULL ? f_pLeafMostCert : &pCerts[ iCert & 1 ];
            pChildCert  = iCert == 1 && f_pLeafMostCert != NULL ? f_pLeafMostCert : &pCerts[ ( iCert & 1 ) ^ 1 ];
        }

        ChkDR( _DRM_BCERTFORMAT_ParseCertificate(
            f_pParserCtx,
            f_cbCertData - dwOffset, /* Can't underflow: We check that dwOffset < f_cbCertData */
            f_pbCertData + dwOffset, /* Can't overflow: We check that dwOffset < f_cbCertData */
            &cbParsed,
            &dwCertKeyUsageSet,
            pParentCert,
            f_fForceSkipSignatureCheck ) );

        dwKeyUsageSet |= dwCertKeyUsageSet;

        dwExpiration = pParentCert->BasicInformation.dwExpirationDate;
        if( dwExpiration != 0 &&
            dwExpiration <  f_pChain->dwExpiration )
        {
            f_pChain->dwExpiration = dwExpiration;
        }

        /*
        ** Verify certificate digest is a SHA256 hashing the public key.
        */
        ChkDR( Oem_Broker_SHA256_CreateDigest(
            ( DRM_VOID * )f_pOemTeeContext,
            XBBA_TO_CB( pParentCert->KeyInformation.pHead[0].xbbaKeyValue ),
            XBBA_TO_PB( pParentCert->KeyInformation.pHead[0].xbbaKeyValue ),
            &oDigest ) );
        
        fValidCertHash = XBBA_TO_CB( pParentCert->BasicInformation.xbbaDigestValue ) == sizeof( oDigest )
            && XBBA_TO_PB( pParentCert->BasicInformation.xbbaDigestValue ) != NULL
            && 0 == MEMCMP( &oDigest, XBBA_TO_PB( pParentCert->BasicInformation.xbbaDigestValue ), sizeof( oDigest ) );

        if( iCert == 0 ) // Checking Leaf Cert
        {
            DRM_BCERTFORMAT_CHKVERIFICATIONERR(
                (DRM_BCERTFORMAT_PARSER_CONTEXT_BASE*) f_pParserCtx,
                fValidCertHash, 
                DRM_E_BCERT_ILWALID_DIGEST );
        }
        else
        {
            ChkBOOL( fValidCertHash, DRM_E_BCERT_ILWALID_DIGEST );
        }

        /* Add the current certificates digest to the chain data if provided */
        if( f_pChainData != NULL )
        {
            DRMCASSERT( DRM_NO_OF( f_pChainData->rgoDigests ) == DRM_BCERT_MAX_CERTS_PER_CHAIN );
            ChkVOID( OEM_SELWRE_MEMCPY( &f_pChainData->rgoDigests[iCert], &oDigest, sizeof(oDigest) ) );
            f_pChainData->cDigests++;   /* Can't overflow: cDigests <= DRM_BCERT_MAX_CERTS_PER_CHAIN */
        }

        if( iCert > 0 )
        {
            ChkDR( _DRM_BCERTFORMAT_VerifyAdjacentCerts( pCtx, pChildCert, pParentCert, &oChainSecVer ) );
        }
        else
        {
            /* Verify the leaf cert type */
            ChkBOOL( pCtx->dwExpectedLeafCertType == DRM_BCERT_CERTTYPE_UNKNOWN ||
                     pParentCert->BasicInformation.dwType == pCtx->dwExpectedLeafCertType, DRM_E_BCERT_ILWALID_CERT_TYPE );

            if( f_pChainData != NULL )
            {
                ChkDR( _GetChainDataFromCert( pParentCert, f_pChainData ) );
            }

            if( f_pChain->Header.cCerts == 1 )
            {
                /*
                ** If we're dealing with only one certificate, we still need to initialize the output
                ** chain security version normally populated by _DRM_BCERTFORMAT_VerifyAdjacentCerts.
                */
                if( pParentCert->SelwrityVersion2.fValid )
                {
                    DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION2( &oChainSecVer, pParentCert );
                }
                else if( pParentCert->SelwrityVersion.fValid )
                {
                    DRM_BCERTFORMAT_INIT_CHAIN_SELWRITY_VERSION( &oChainSecVer, pParentCert );
                }
            }
        }

        pCertHeaders[iCert].fValid      = TRUE;
        pCertHeaders[iCert].HeaderData  = pParentCert->HeaderData;
        pCertHeaders[iCert].dwOffset    = dwOffset;
        pCertHeaders[iCert].dwIndex     = iCert;
        pCertHeaders[iCert].xbbaRawData = pParentCert->xbbaRawData;

        ChkDR( DRM_DWordAddSame( &dwOffset, pCertHeaders[iCert].HeaderData.dwLength ) );

        /*
        ** Make sure the last certificate is signed with either the provided root public key
        ** or the default PlayReady public issuer key.
        */
        if( iCert == ( f_pChain->Header.cCerts - 1 ) )  /* Can't underflow: Wouldn't have entered loop unles cCerts > 0 */
        {
            ChkDR( _ValidateRootPublicKey( pCtx, pParentCert ) );
        }
    }

    /*
    ** See that all required key usages are present
    */
    ChkBOOL( !DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) ||
            ( ( dwKeyUsageSet & DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->dwRequiredKeyUsageMask )
                == DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->dwRequiredKeyUsageMask ), DRM_E_BCERT_REQUIRED_KEYUSAGE_MISSING );

    /*
    ** Get the leaf cert when using full context.  It is not needed for the base context because
    ** if the parameter is non-NULL, it will be used when parsing the first cert.  Full context
    ** parsing fills the certificate cache and then uses the cache to populate the leaf cert
    ** parameter.
    */
    if(    f_pChain->Header.cCerts >  0
        && f_pLeafMostCert         != NULL
        && DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) )
    {
        ChkDR( DRM_BCERTFORMAT_GetCertificate(
            ( DRM_BCERTFORMAT_PARSER_CONTEXT * )f_pParserCtx,
            &pCertHeaders[0],
            NULL,
            f_pLeafMostCert ) );
    }

    if( f_pcbParsed != NULL )
    {
        *f_pcbParsed = dwOffset;
    }

    /*
    ** Update the chain's security (robustness) version information
    */
    if( oChainSecVer.fValid )
    {
        f_pChain->dwPlatformID      = oChainSecVer.dwPlatformID;
        f_pChain->dwSelwrityVersion = oChainSecVer.dwSelwrityVersion;
    }
    else
    {
        f_pChain->dwPlatformID      = DRM_BCERT_SELWRITY_VERSION_PLATFORM_UNSPECIFIED;
        f_pChain->dwSelwrityVersion = DRM_BCERT_SELWRITY_VERSION_UNSPECIFIED;
    }

    if( f_pChainData != NULL )
    {
        f_pChainData->dwPlatformID      = f_pChain->dwPlatformID;
        f_pChainData->dwSelwrityVersion = f_pChain->dwSelwrityVersion;
    }

    if( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) && DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->iResult != 0 )
    {
        DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->cResults = DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->iResult;
        ChkDR( DRM_E_BCERT_VERIFICATION_ERRORS );
    }

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( ( DRM_VOID ** )&pCerts ) );
    ChkVOID( Oem_Broker_MemFree( ( DRM_VOID ** )&pChain  ) );

    if( dr == DRM_E_XB_ILWALID_OBJECT && DRM_FAILED( drOriginal ) )
    {
        dr = drOriginal;
    }
    return dr;
}
#endif

#ifdef NONE
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_ParseCertificateChain
**
** Synopsis:    Parses and validates a full certificate chain.  The caller can optionally
**              call the function DRM_BCERTFORMAT_InitializeParserContext prior to this
**              call to set up the validation properties.
**
** Arguments:   [f_pParserCtx]     : The BCERT format parser context.
**              [f_pOemTeeContext] : Optional. The OEM TEE context.
**              [f_pftLwrrentTime] : Optional. The current time for which all certificates
**                                   expirations will be evaluated against.
**              [f_dwExpectedLeafCertType]
**                                 : The expected leaf certificate type.  If this value
**                                   is DRM_BCERT_CERTTYPE_UNKNOWN no validation will be
**                                   performed, otherwise, the leaf most certificate will
**                                   be validated against this type.
**              [f_pStack]         : The stack used to create data structures during
**                                   parsing.  This pointer will be stored in the
**                                   parser context (f_pParserCtx) for future use.
**              [f_cbCertData]     : The size of the data buffer (in bytes)
**              [f_pbCertData]     : The data buffer that is being parsed
**              [f_pcbParsed]      : Optional.  The number of bytes processed while parsing
**                                   the certificate chain.
**              [f_pChain]         : A pointer to a chain header structure.
**                                   Output parameter, cannot be NULL
**              [f_pLeafMostCert]  : Optional.  The leaf most certificate in the chain.
**
** Notes:  If you need to enumerate all certificates in a chain and read
**         their content into DRM_BCERTFORMAT_CERT structure, call this API to
**         1) to validate the certificate chain, 2) find out total number
**         of certs in it, 3) provide headers for each certificate.
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_ParseCertificateChain(
    __inout_ecount( 1 )                   DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                           OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in_ecount_opt( 1 )            const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                  DRM_DWORD                              f_dwExpectedLeafCertType,
    __inout                               DRM_STACK_ALLOCATOR_CONTEXT           *f_pStack,
    __in                                  DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )     const DRM_BYTE                              *f_pbCertData,
    __inout_ecount_opt( 1 )               DRM_DWORD                             *f_pcbParsed,
    __out_ecount( 1 )                     DRM_BCERTFORMAT_CHAIN                 *f_pChain,
    __out_ecount_opt( 1 )                 DRM_BCERTFORMAT_CERT                  *f_pLeafMostCert )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_BCERTFORMAT_PARSER_CONTEXT_FULL *pCtx = DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( f_pParserCtx );

    ChkArg( f_pStack     != NULL );
    ChkArg( f_pParserCtx != NULL );
    ChkArg( f_pbCertData != NULL );
    ChkArg( f_pChain     != NULL );

    if( !pCtx->oBaseCtx.fInitialized )
    {
        ChkDR( _InitializeParserContextWithDefaults( TRUE /* f_fCheckSignature */, f_pParserCtx ) );
    }
    else
    {
        AssertChkBOOL( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( &pCtx->oBaseCtx ) );
        ZEROMEM( pCtx->rgoCertCache, sizeof(pCtx->rgoCertCache) );
    }

    ChkDR( _ParseCertificateChain(
        f_pParserCtx,
        f_pOemTeeContext,
        f_pftLwrrentTime,
        f_dwExpectedLeafCertType,
        f_pStack,
        f_cbCertData,
        f_pbCertData,
        f_pcbParsed,
        f_pChain,
        f_pLeafMostCert,
        NULL,
        FALSE ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_VerifyCertificateChain
**
** Synopsis:    Parses and validates a full certificate chain.
**
** Arguments:   [f_pOemTeeContext]     : Optional. The OEM TEE context.
**              [f_pParserCtx]         : Optional. The BCERT format parser context used
**                                       that will be used during certificate validation.
**              [f_pftLwrrentTime]     : Optional. The current time for which all certificates
**                                       expirations will be evaluated against.
**              [f_dwCertType]         : The expected leaf certificate type.  Verification
**                                       will fail if this type is not DRM_BCERT_CERTTYPE_UNKNOWN
**                                       and does not match the leaf certificate type.
**              [f_dwMinSelwrityLevel] : The minimum security level for the leaf certificate.
**                                       Verification will fail if the security level for the
**                                       leaf certificate is less than this value.
**              [f_cbCertData]         : The size of the data buffer (in bytes).
**              [f_pbCertData]         : The data buffer that is being parsed.
**
** Notes:  If you need to enumerate all certificates in a chain and read
**         their content into DRM_BCERTFORMAT_CERT structure, call this API to
**         1) to validate the certificate chain, 2) find out total number
**         of certs in it, 3) provide headers for each certificate.
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_VerifyCertificateChain(
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __in_ecount_opt( 1 )              const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                    DRM_DWORD                              f_dwCertType,
    __in                                    DRM_DWORD                              f_dwMinSelwrityLevel,
    __in                                    DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )       const DRM_BYTE                              *f_pbCertData )
{
    DRM_RESULT                     dr          = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT oParserCtx; /* ZEROMEM */
    DRM_BCERTFORMAT_CHAIN          oChain      = {0};
    DRM_BCERTFORMAT_CERT           oCert       = {0};
    DRM_BYTE                      *pbStack     = NULL;
    const DRM_DWORD                cbStack     = DRM_BCERTFORMAT_STACK_SIZE;
    DRM_STACK_ALLOCATOR_CONTEXT    oStack;     /* Initialized by DRM_STK_Init */

    ChkArg( f_pbCertData != NULL );
    ChkArg( f_cbCertData  > 0    );

    if( f_pParserCtx == NULL )
    {
        ChkVOID( ZEROMEM( &oParserCtx, sizeof( oParserCtx ) ) );
        f_pParserCtx = &oParserCtx;
    }

    ChkDR( Oem_Broker_MemAlloc(  cbStack, ( DRM_VOID ** )&pbStack ) );
    ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );
    ChkDR( DRM_BCERTFORMAT_ParseCertificateChain(
        f_pParserCtx,
        f_pOemTeeContext,
        f_pftLwrrentTime,
        f_dwCertType,
        &oStack,
        f_cbCertData,
        f_pbCertData,
        NULL,
        &oChain,
        &oCert ) );

    ChkBOOL( f_dwMinSelwrityLevel <= oCert.BasicInformation.dwSelwrityLevel, DRM_E_DEVICE_SELWRITY_LEVEL_TOO_LOW );

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( ( DRM_VOID ** )&pbStack ) );
    return dr;
}


/***********************************************************************************************************
** Function:    DRM_BCERTFORMAT_GetPublicKeyByUsageFromChain
**
** Synopsis:    The function retrieves the public key of the cert with specified
**              key usage and index in a certificate chain.
**              On success returns a key value and an array of all key usages for the found key.
**              If key usage is set into DRM_BCERT_KEYUSAGE_UNKNOWN then the first public key will be returned.
**
** Arguments:   [f_pParserCtx]      : The BCERT format parser context.
**              [f_pChain]          : The certificate chain object created in DRM_BCERTFORMAT_ParseCertificateChain.
**              [f_dwKeyUsage]      : The requested key usage of public key.
**              [f_pPubkey]         : A pointer to a public key structure.
**              [f_pdwKeyUsageSet]  : key usage set of the returned key.
**                                    This parameter is optional and can be NULL.
**              [f_pCert]           : Optionally returns the requested certificate object.
**              [f_pdwCertKeyIndex] : Optionally returns the index of the requested key.
**
************************************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyByUsageFromChain(
    __inout_ecount( 1 )                 DRM_BCERTFORMAT_PARSER_CONTEXT  *f_pParserCtx,
    __in                          const DRM_BCERTFORMAT_CHAIN           *f_pChain,
    __in                                DRM_DWORD                        f_dwKeyUsage,
    __inout_ecount( 1 )                 PUBKEY_P256                     *f_pPubkey,
    __out_ecount_opt( 1 )               DRM_DWORD                       *f_pdwKeyUsageSet,
    __inout_ecount_opt( 1 )             DRM_BCERTFORMAT_CERT            *f_pCert,
    __out_ecount_opt( 1 )               DRM_DWORD                       *f_pdwCertKeyIndex )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT                                 dr                   = DRM_SUCCESS;
    DRM_DWORD                                  iKeyCounter          = 0;
    DRM_BOOL                                   fFoundKey            = FALSE;
    DRM_DWORD                                  iFoundKey            = 0;
    const DRM_BCERTFORMAT_CERT                *pKeyCert             = NULL;
    const DRM_BCERTFORMAT_KEY_INFO            *pKeyInfo             = NULL;
    DRM_DWORD                                  dwUsageSet           = 0;
    DRM_DWORD                                  dwBitMask            = (DRM_DWORD)BCERT_KEYUSAGE_BIT( f_dwKeyUsage );
    DRM_DWORD                                  iCert;               /* Loop variable */
    const DRM_BCERTFORMAT_PARSER_CONTEXT_FULL *pCtx                 = DRM_BCERTFORMAT_PARSER_CTX_TO_CONST_FULL( f_pParserCtx );

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_BCERT, PERF_FUNC_DRM_BCert_GetPublicKeyByUsage );

    ChkArg( f_pParserCtx != NULL );
    ChkArg( f_pChain     != NULL );
    ChkArg( f_pPubkey    != NULL );
    ChkArg( pCtx->oBaseCtx.fInitialized );

    AssertChkBOOL( DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( &pCtx->oBaseCtx ) );

    for( iCert = 0; iCert < f_pChain->Certificates.cEntries; iCert++ )
    {
        ChkDR( DRM_BCERTFORMAT_GetCertificate(
            f_pParserCtx,
            &f_pChain->Certificates.pCertHeaders[ iCert ],
            NULL,
            NULL ) );
        pKeyCert = &pCtx->rgoCertCache[ f_pChain->Certificates.pCertHeaders[ iCert ].dwIndex ];

        pKeyInfo = &pKeyCert->KeyInformation;

        if( f_dwKeyUsage != DRM_BCERT_KEYUSAGE_UNKNOWN )
        {
            /*
            ** Find the first key with requested key usage
            */
            for( iKeyCounter = 0; iKeyCounter < pKeyInfo->cEntries; iKeyCounter++ )
            {
                const DRM_BCERTFORMAT_KEY_TYPE *pKeyType = &pKeyInfo->pHead[ iKeyCounter ];

                if( ( dwBitMask & ( pKeyType->dwUsageSet ) ) != 0 )
                {
                    iFoundKey = iKeyCounter;
                    fFoundKey = TRUE;
                    dwUsageSet = pKeyType->dwUsageSet;
                    break;
                }
            }
        }
        else if( pKeyInfo->cEntries > 0 )
        {
            iFoundKey  = 0;
            fFoundKey  = TRUE;
            dwUsageSet = pKeyInfo->pHead[ 0 ].dwUsageSet;
            break;
        }

        if( fFoundKey )
        {
            break;
        }
    }

    /*
    ** If the loop is finished then the key with requested usage was not found
    */
    ChkBOOL( fFoundKey, DRM_E_BCERT_NO_PUBKEY_WITH_REQUESTED_KEYUSAGE );

    /*
    ** Copy key usage bitmaps.
    */
    if( f_pdwKeyUsageSet != NULL )
    {
        *f_pdwKeyUsageSet = dwUsageSet;
    }

    ChkBOOL( XBBA_TO_CB( pKeyInfo->pHead[ iFoundKey ].xbbaKeyValue ) >= sizeof( PUBKEY_P256 ), DRM_E_BCERT_PUBLIC_KEY_NOT_SPECIFIED );
    ChkBOOL( XBBA_TO_PB( pKeyInfo->pHead[ iFoundKey ].xbbaKeyValue ) != NULL                 , DRM_E_BCERT_PUBLIC_KEY_NOT_SPECIFIED );

    OEM_SELWRE_MEMCPY( f_pPubkey->m_rgbPubkey, XBBA_TO_PB( pKeyInfo->pHead[ iFoundKey ].xbbaKeyValue ), sizeof( PUBKEY_P256 ) );

    if( f_pdwCertKeyIndex != NULL )
    {
        *f_pdwCertKeyIndex = iFoundKey;
    }

    if( f_pCert != NULL )
    {
        MEMCPY( f_pCert, pKeyCert, sizeof( *f_pCert ) );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

/********************************************************************************************
** Function:    DRM_BCERTFORMAT_GetPublicKeyFromCert
**
** Synopsis:    The function retrieves the first public key (usually the public signing key)
**              of the specified cert in a certificate chain.
**
** Arguments:   [f_pCert]           : The certificate data.
**              [f_pPubkey]         : pointer to a public key structure
**
********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyFromCert(
    __in_ecount( 1 )    const DRM_BCERTFORMAT_CERT              *f_pCert,
    __inout_ecount( 1 )       PUBKEY_P256                       *f_pPubkey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_BCERT, PERF_FUNC_DRM_BCert_GetPublicKey );

    ChkArg( f_pCert   != NULL );
    ChkArg( f_pPubkey != NULL );
    ChkArg( f_pCert->KeyInformation.cEntries > 0 );

    ChkBOOL( XBBA_TO_CB( f_pCert->KeyInformation.pHead[ 0 ].xbbaKeyValue ) >= sizeof( PUBKEY_P256 ), DRM_E_BCERT_PUBLIC_KEY_NOT_SPECIFIED );
    ChkBOOL( XBBA_TO_PB( f_pCert->KeyInformation.pHead[ 0 ].xbbaKeyValue ) != NULL                 , DRM_E_BCERT_PUBLIC_KEY_NOT_SPECIFIED );
    OEM_SELWRE_MEMCPY( f_pPubkey->m_rgbPubkey, XBBA_TO_PB( f_pCert->KeyInformation.pHead[ 0 ].xbbaKeyValue ), sizeof( PUBKEY_P256 ) );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

/***********************************************************************************************************
** Function:    DRM_BCERTFORMAT_GetPublicKeyByUsage
**
** Synopsis:    The function retrieves the public key of the cert with specified
**              key usage and index in a certificate chain.
**              On success returns a key value and an array of all key usages for the found key.
**              If key usage is set into DRM_BCERT_KEYUSAGE_UNKNOWN then the first public key will be returned.
**
** Arguments:   [f_pParserCtx]      : Optional. The BCERT format parser context.
**              [f_pOemTeeContext]  : The OEM TEE context (if available).
**              [f_cbCertData]      : The size in bytes of the certificate chain.
**              [f_pbCertData]      : The certificate chain data.
**              [f_dwCertIndex]     : The index of the certificate in the chain for which to get the
**                                    public key.  Zero represents the leaf certificate.
**              [f_dwKeyUsage]      : The requested key usage of public key.
**              [f_pPubkey]         : A pointer to a public key structure.
**              [f_pdwKeyUsageSet]  : key usage set of the returned key.
**                                    This parameter is optional and can be NULL.
**              [f_pdwCertKeyIndex] : Optionally returns the index of the requested key.
**
************************************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyByUsage(
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in                              const DRM_DWORD                              f_cbCertData,
    __in_bcount(f_cbCertData)         const DRM_BYTE                              *f_pbCertData,
    __in                              const DRM_DWORD                              f_dwCertIndex,
    __in                              const DRM_DWORD                              f_dwKeyUsage,
    __out_ecount(1)                         PUBKEY_P256                           *f_pPubkey,
    __out_opt                               DRM_DWORD                             *f_pdwKeyUsageSet,
    __out_ecount_opt(1)                     DRM_DWORD                             *f_pdwCertKeyIndex )
{
    DRM_RESULT                     dr          = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT oParserCtx; /* ZEROMEM */
    DRM_BCERTFORMAT_CHAIN          oChain      = {0};
    DRM_BCERTFORMAT_CERT           oCert       = {0};
    DRM_BYTE                      *pbStack     = NULL;
    const DRM_DWORD                cbStack     = DRM_BCERTFORMAT_STACK_SIZE;
    DRM_STACK_ALLOCATOR_CONTEXT    oStack;     /* Initialized by DRM_STK_Init */

    if( f_pParserCtx == NULL )
    {
        ChkVOID( ZEROMEM( &oParserCtx, sizeof( oParserCtx ) ) );
        f_pParserCtx = &oParserCtx;
    }

    ChkDR( Oem_Broker_MemAlloc( cbStack, ( DRM_VOID ** )&pbStack ) );
    ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );
    ChkDR( DRM_BCERTFORMAT_ParseCertificateChain(
        f_pParserCtx,
        f_pOemTeeContext,
        NULL,
        DRM_BCERT_CERTTYPE_UNKNOWN,
        &oStack,
        f_cbCertData,
        f_pbCertData,
        NULL,
        &oChain,
        &oCert ) );

    if( f_dwCertIndex != 0 )
    {
        ChkArg( f_dwCertIndex < oChain.Certificates.cEntries );
        ChkDR( DRM_BCERTFORMAT_GetCertificate(
            f_pParserCtx,
            &oChain.Certificates.pCertHeaders[f_dwCertIndex],
            NULL,
            &oCert ) );
    }

    ChkDR( DRM_BCERTFORMAT_GetPublicKeyByUsageFromChain(
        f_pParserCtx,
        &oChain,
        f_dwKeyUsage,
        f_pPubkey,
        f_pdwKeyUsageSet,
        &oCert,
        f_pdwCertKeyIndex ) );

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( ( DRM_VOID ** )&pbStack ) );
    return dr;
}

/********************************************************************************************
** Function:    DRM_BCERTFORMAT_GetPublicKey
**
** Synopsis:    The function retrieves the first public key (usually the signing key) of the
**              leaf cert in a certificate chain.
**
** Arguments:   [f_pParserCtx]      : Optional. The BCERT format parser context.
**              [f_pOemTeeContext]  : The OEM TEE context (if available).
**              [f_cbCertData]      : The size in bytes of the certificate chain.
**              [f_pbCertData]      : The certificate chain data.
**              [f_pPubkey]         : Pointer to a public key structure
**
********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKey(
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in                              const DRM_DWORD                              f_cbCertData,
    __in_bcount(f_cbCertData)         const DRM_BYTE                              *f_pbCertData,
    __out_ecount(1)                         PUBKEY_P256                           *f_pPubkey )
{
    DRM_RESULT                     dr          = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT oParserCtx; /* ZEROMEM */
    DRM_BCERTFORMAT_CHAIN          oChain      = {0};
    DRM_BCERTFORMAT_CERT           oCert       = {0};
    DRM_BYTE                      *pbStack     = NULL;
    const DRM_DWORD                cbStack     = DRM_BCERTFORMAT_STACK_SIZE;
    DRM_STACK_ALLOCATOR_CONTEXT    oStack;     /* Initialized by DRM_STK_Init */

    if( f_pParserCtx == NULL )
    {
        ChkVOID( ZEROMEM( &oParserCtx, sizeof( oParserCtx ) ) );
        f_pParserCtx = &oParserCtx;

        ChkDR( _InitializeParserContextWithDefaults( FALSE /* f_fCheckSignature */, f_pParserCtx ) );
    }

    ChkDR( Oem_Broker_MemAlloc( cbStack, ( DRM_VOID ** )&pbStack ) );
    ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );
    ChkDR( DRM_BCERTFORMAT_ParseCertificateChain(
        f_pParserCtx,
        f_pOemTeeContext,
        NULL,
        DRM_BCERT_CERTTYPE_UNKNOWN,
        &oStack,
        f_cbCertData,
        f_pbCertData,
        NULL,
        &oChain,
        &oCert ) );

    ChkDR( DRM_BCERTFORMAT_GetPublicKeyFromCert(
        &oCert,
        f_pPubkey ) );

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( ( DRM_VOID ** )&pbStack ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetPublicKeyAndDomainRevision(
    _Inout_opt_                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    _Inout_opt_                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    _In_                              const DRM_DWORD                              f_cbCertData,
    _In_reads_( f_cbCertData )        const DRM_BYTE                              *f_pbCertData,
    _Out_writes_( 1 )                       PUBKEY_P256                           *f_pPubkey,
    _Out_writes_opt_( 1 )                   DRM_DWORD                             *f_pdwRevision )
{
    DRM_RESULT                     dr          = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT oParserCtx; /* ZEROMEM */
    DRM_BCERTFORMAT_CHAIN          oChain      = { 0 };
    DRM_BCERTFORMAT_CERT           oCert       = { 0 };
    DRM_BYTE                      *pbStack     = NULL;
    const DRM_DWORD                cbStack     = DRM_BCERTFORMAT_STACK_SIZE;
    DRM_STACK_ALLOCATOR_CONTEXT    oStack;     /* Initialized by DRM_STK_Init */
    DRM_DWORD                      dwExpectedLeafCertType = DRM_BCERT_CERTTYPE_UNKNOWN;

    if( f_pParserCtx == NULL )
    {
        ChkVOID( ZEROMEM( &oParserCtx, sizeof( oParserCtx ) ) );
        f_pParserCtx = &oParserCtx;

        ChkDR( _InitializeParserContextWithDefaults( FALSE /* f_fCheckSignature */, f_pParserCtx ) );
    }
    if( f_pdwRevision != NULL )
    {
        dwExpectedLeafCertType = DRM_BCERT_CERTTYPE_DOMAIN;
    }

    ChkDR( Oem_Broker_MemAlloc( cbStack, (DRM_VOID **)&pbStack ) );
    ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );
    ChkDR( DRM_BCERTFORMAT_ParseCertificateChain(
        f_pParserCtx,
        f_pOemTeeContext,
        NULL,
        dwExpectedLeafCertType,
        &oStack,
        f_cbCertData,
        f_pbCertData,
        NULL,
        &oChain,
        &oCert ) );

    ChkDR( DRM_BCERTFORMAT_GetPublicKeyFromCert(
        &oCert,
        f_pPubkey ) );

    if( f_pdwRevision != NULL )
    {
        ChkBOOL( oCert.DomainInformation.fValid, DRM_E_BCERT_ILWALID_CERT_TYPE );
        *f_pdwRevision = oCert.DomainInformation.dwRevision;
    }

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( (DRM_VOID **)&pbStack ) );
    return dr;
}

/********************************************************************************************
** Function:    DRM_BCERTFORMAT_GetSelwrityLevel
**
** Synopsis:    The function retrieves the security level of the
**              leaf cert in a certificate chain.
**
** Arguments:   [f_pParserCtx]       : Optional. The BCERT format parser context.
**              [f_pOemTeeContext]   : The OEM TEE context (if available).
**              [f_cbCertData]       : The size in bytes of the certificate chain.
**              [f_pbCertData]       : The certificate chain data.
**              [f_pdwSelwrityLevel] : Security level of the leaf certificate
**
********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_GetSelwrityLevel(
    __inout_opt                             DRM_BCERTFORMAT_PARSER_CONTEXT        *f_pParserCtx,
    __inout_opt                             OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in                              const DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )       const DRM_BYTE                              *f_pbCertData,
    __out_ecount( 1 )                       DRM_DWORD                             *f_pdwSelwrityLevel )
{
    DRM_RESULT                     dr          = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT oParserCtx; /* ZEROMEM */
    DRM_BCERTFORMAT_CHAIN          oChain      = { 0 };
    DRM_BCERTFORMAT_CERT           oCert       = { 0 };
    DRM_BYTE                      *pbStack     = NULL;
    const DRM_DWORD                cbStack     = DRM_BCERTFORMAT_STACK_SIZE;
    DRM_STACK_ALLOCATOR_CONTEXT    oStack;     /* Initialized by DRM_STK_Init */

    ChkArg( f_pdwSelwrityLevel != NULL );   /* Other params checked by DRM_BCERTFORMAT_ParseCertificateChain */

    if( f_pParserCtx == NULL )
    {
        ChkVOID( ZEROMEM( &oParserCtx, sizeof( oParserCtx ) ) );
        f_pParserCtx = &oParserCtx;

        ChkDR( _InitializeParserContextWithDefaults( FALSE /* f_fCheckSignature */, f_pParserCtx ) );
    }

    ChkDR( Oem_Broker_MemAlloc( cbStack, (DRM_VOID **)&pbStack ) );
    ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );
    ChkDR( DRM_BCERTFORMAT_ParseCertificateChain(
        f_pParserCtx,
        f_pOemTeeContext,
        NULL,
        DRM_BCERT_CERTTYPE_UNKNOWN,
        &oStack,
        f_cbCertData,
        f_pbCertData,
        NULL,
        &oChain,
        &oCert ) );

    AssertChkBOOL( oCert.fValid );
    AssertChkBOOL( oCert.BasicInformation.fValid );

    *f_pdwSelwrityLevel = oCert.BasicInformation.dwSelwrityLevel;

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( (DRM_VOID **)&pbStack ) );
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_VerifyChildUsage
**
** Synopsis:    Verify that the supplied child key type can be issued by a
**              parent key with the supplied usage mask
**
** Arguments:   [f_dwChildKeyUsageMask]  : The child key usage mask
**              [f_dwParentKeyUsage]     : The parent key usage mask
**
** Returns:     DRM_SUCCESS      - on success
**              DRM_E_BCERT_ILWALID_KEY_USAGE
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_VerifyChildUsage(
    __in const DRM_DWORD f_dwChildKeyUsageMask,
    __in const DRM_DWORD f_dwParentKeyUsageMask )
{
    DRM_RESULT dr = DRM_SUCCESS;

    /*
    ** Check that the current certificate has appropriate Issuer rights
    ** to issue a child certificate of particular type.
    */
    if ( !(f_dwParentKeyUsageMask & BCERT_KEYUSAGE_BIT( DRM_BCERT_KEYUSAGE_ISSUER_ALL ) ) ) /* Issuer-All can issue anything */
    {
        /*
        ** Cannot have Issuer-All child cert
        */
        ChkBOOL( !( f_dwChildKeyUsageMask & BCERT_KEYUSAGE_BIT( DRM_BCERT_KEYUSAGE_ISSUER_ALL ) ),
            DRM_E_BCERT_ILWALID_KEY_USAGE );

        /*
        ** Check key usages in a child cert:
        ** the parent cert is not Issuer-All and must have all issuer-... key usages that child cert has
        */
        ChkBOOL( ( ( f_dwChildKeyUsageMask  & BCERT_KEYUSAGE_PARENT_ISSUERS_MASK )
                 & ( f_dwParentKeyUsageMask & BCERT_KEYUSAGE_PARENT_ISSUERS_MASK ) )
                    == ( f_dwChildKeyUsageMask & BCERT_KEYUSAGE_PARENT_ISSUERS_MASK ),
                DRM_E_BCERT_ILWALID_KEY_USAGE );
    }

ErrorExit:
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_ParseCertificateChainData
**
** Synopsis:    Parses and validates a full certificate chain and returns a minimal
**              set of certificate chain and leaf cert data.
**
** Arguments:   [f_pOemTeeContext]              : Optional. The OEM TEE context.
**              [f_pftLwrrentTime]              : Optional. The current time for which all certificates
**                                                expirations will be evaluated against.
**              [f_dwExpectedLeafCertType]      : The expected leaf certificate type.  If this value
**                                                is DRM_BCERT_CERTTYPE_UNKNOWN no validation will be
**                                                performed, otherwise, the leaf most certificate will
**                                                be validated against this type.
**              [f_pStack]                      : The stack used to create data structures during
**                                                parsing.  This pointer will be stored in the
**                                                parser context (f_pParserCtx) for future use.
**              [f_cbCertData]                  : The size of the data buffer (in bytes)
**              [f_pbCertData]                  : The data buffer that is being parsed
**              [f_poChainData]                 : A pointer to a cert chain data structure.
**              [f_fForceSkipSignatureCheck]    : If TRUE, skip signature check regardless of other parameters.
**                                                Otherwise, perform signature check based on other parameters.
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_ParseCertificateChainData(
    __inout_opt                           OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in_opt                        const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                  DRM_DWORD                              f_dwExpectedLeafCertType,
    __inout                               DRM_STACK_ALLOCATOR_CONTEXT           *f_pStack,
    __in                                  DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )     const DRM_BYTE                              *f_pbCertData,
    __out                                 DRM_BCERTFORMAT_PARSER_CHAIN_DATA     *f_poChainData,
    __in                                  DRM_BOOL                               f_fForceSkipSignatureCheck )
{
    DRM_RESULT                          dr   = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT_BASE oCtx = DRM_BCERTFORMAT_PARSER_CONTEXT_BASE_EMPTY;

    ChkArg( f_cbCertData   > 0    );
    ChkArg( f_pbCertData  != NULL );
    ChkArg( f_pStack      != NULL );
    ChkArg( f_poChainData != NULL );

    oCtx.eContextType           = DRM_BCERTFORMAT_CONTEXT_TYPE_BASE;
    oCtx.fInitialized           = TRUE;
    oCtx.dwExpectedLeafCertType = f_dwExpectedLeafCertType;
    oCtx.pStack                 = f_pStack;
    oCtx.pOemTeeCtx             = f_pOemTeeContext;

    if( f_pftLwrrentTime != NULL )
    {
        oCtx.ftLwrrentTime = *f_pftLwrrentTime;
    }

    ChkDR( _ParseCertificateChain(
        &oCtx,
        f_pOemTeeContext,
        f_pftLwrrentTime,
        f_dwExpectedLeafCertType,
        f_pStack,
        f_cbCertData,
        f_pbCertData,
        NULL,
        NULL,
        NULL,
        f_poChainData,
        f_fForceSkipSignatureCheck ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API_VOID DRM_DWORD DRM_BCERTFORMAT_GetResultCount( __in DRM_BCERTFORMAT_PARSER_CONTEXT *f_pCertParserCtx )
{
    DRM_BCERTFORMAT_PARSER_CONTEXT_BASE *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_PARSER_CONTEXT_BASE, f_pCertParserCtx );
    if( f_pCertParserCtx != NULL && DRM_BCERTFORMAT_PARSER_CTX_IS_FULL( pCtx ) )
    {
        return DRM_BCERTFORMAT_PARSER_CTX_TO_FULL( pCtx )->cResults;
    }
    return 0;
}
#endif

EXIT_PK_NAMESPACE_CODE;

