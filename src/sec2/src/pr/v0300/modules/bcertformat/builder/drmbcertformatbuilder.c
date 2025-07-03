/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMBCERTFORMATBUILDER_C
#include <oembroker.h>
#include <drmxbparser.h>
#include <drmbytemanip.h>
#include <drmmathsafe.h>
#include <drmbcertformatbuilder.h>
#include <drmprofile.h>
#include <drmutf.h>
#include <drmlastinclude.h>

PRAGMA_STRICT_GS_PUSH_ON;

ENTER_PK_NAMESPACE_CODE;
#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
static const DRM_ID g_DRM_BCERTFORMAT_EMPTY_ID = { { 0 } }; // LWE (nkuo) - changed due to compile error "missing braces around initializer"

/*****************************************************************************
** Function:    _DRM_BCERTFORMAT_CalcSignatureInfoSize
**
** Synopsis:    Callwlates the size of the Signature Information element.
**
** Arguments:   [f_wSignatureType]    : The type of signature signing the certificate
**              [f_cbIssuerKeyLength] : The issuer key length (in bytes)
**              [f_pcbSize]           : A pointer to a size variable to be updated
**
** Notes:       This routine will need to be modified if/when new signature types are supported.
**
******************************************************************************/
static DRM_RESULT _DRM_BCERTFORMAT_CalcSignatureInfoSize(
  __in           DRM_WORD      f_wSignatureType,
  __in           DRM_DWORD     f_cbIssuerKeyLength,
  __out          DRM_DWORD    *f_pcbSize )
{
    DRM_RESULT  dr               = DRM_SUCCESS;
    DRM_WORD    cbSignatureLength = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_BCERT, PERF_FUNC_calcSignatureInfoSize );

    ChkArg( f_pcbSize != NULL );

    /*
    ** The signature length (in bytes) is determined from the signature type
    */
    ChkBOOL( f_wSignatureType == DRM_BCERT_SIGNATURE_TYPE_P256, DRM_E_BCERT_ILWALID_SIGNATURE_TYPE );
    cbSignatureLength = sizeof( SIGNATURE_P256 );

    *f_pcbSize = DRM_BCERTFORMAT_BUILDER_CALLWLATE_SIGNATUREINFO_SIZE( cbSignatureLength, f_cbIssuerKeyLength );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}


/*****************************************************************************
** Function:    _DRM_BCERTFORMAT_CalcExtDataContainerSize
**
** Synopsis:    Callwlates the size of extended data container object
**              including everything in it.
**
** Arguments:   [f_pExtData]            : A pointer to all extended data input
**              [f_pcbContainerSize]    : A pointer to the total extended data
**                                        container size.
**
******************************************************************************/
static DRM_RESULT _DRM_BCERTFORMAT_CalcExtDataContainerSize(
  __in        const DRM_BCERTFORMAT_EXTENDED_DATA_CONTAINER     *f_pExtData,
  __out             DRM_DWORD                                   *f_pcbContainerSize )
{
    DRM_RESULT  dr                = DRM_SUCCESS;
    DRM_DWORD   cbBlobLength      = 0;
    DRM_WORD    cbSignatureLength = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_BCERT, PERF_FUNC_calcExtDataContainerSize );

    ChkArg( f_pExtData != NULL );
    ChkArg( f_pcbContainerSize != NULL );

    if ( f_pExtData->fValid )
    {
        /*
        ** Even we add extended data record to the cert later
        ** its length and type should be known now.
        */
        cbBlobLength = XBBA_TO_CB( f_pExtData->HwidRecord.xbbaData );

        ChkBOOL( f_pExtData->ExDataSignatureInformation.wSignatureType == DRM_BCERT_SIGNATURE_TYPE_P256, DRM_E_BCERT_ILWALID_SIGNATURE_TYPE );
        cbSignatureLength = sizeof( SIGNATURE_P256 );

        *f_pcbContainerSize = DRM_BCERTFORMAT_BUILDER_CALLWLATE_EXTDATA_CONTAINER_SIZE( cbBlobLength, cbSignatureLength );
        DRMASSERT( DRM_BCERTFORMAT_PAD_AMOUNT(*f_pcbContainerSize) == 0 );  /* callwlated size is always 32 bit aligned */
    }
    else
    {
        *f_pcbContainerSize = 0;
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif
#ifdef NONE
/*****************************************************************************
** Function:    _DRM_BCERTFORMAT_SetDefaultBuilderData
**
** Synopsis:    Fills in all the data that are common to all certificate types with default values.
**              Those data that do not have default values are parameters to this function.
**
** Arguments:   [f_pCtx]           : The BCERT format builder context object.
**              [f_dwCertType]     : The certificate type.
**              [f_pCertData]      : The certificate data specific to the provided certificate type.
**              [f_pCertificateID] : The certificate ID
**              [f_pClientID]      : Optional. The client ID associated with the certificate
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_BCERTFORMAT_SetDefaultBuilderData(
    __inout_ecount( 1 )           DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL  *f_pCtx,
    __in                          DRM_DWORD                                  f_dwCertType,
    __in                    const DRM_BCERTFORMAT_CERT_TYPE_DATA            *f_pCertData,
    __in                    const DRM_ID                                    *f_pCertificateID,
    __in_opt                const DRM_ID                                    *f_pClientID )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_XB_BUILDER_CONTEXT_INTERNAL *pXBCtx = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, &f_pCtx->oBuilderCtx );

    ChkArg( f_pCtx           != NULL );
    ChkArg( f_pCertData      != NULL );
    ChkBOOL( f_pCertificateID != NULL, DRM_E_BCERT_CERT_ID_NOT_SPECIFIED );

    ChkBOOL( 0 != MEMCMP( &g_DRM_BCERTFORMAT_EMPTY_ID, f_pCertificateID, sizeof(DRM_ID) ), DRM_E_BCERT_CERT_ID_NOT_SPECIFIED );

    f_pCtx->oNewCert.fValid                            = TRUE;
    f_pCtx->oNewCert.BasicInformation.fValid           = TRUE;
    f_pCtx->oNewCert.BasicInformation.certID           = *f_pCertificateID;
    f_pCtx->oNewCert.BasicInformation.dwType           = f_dwCertType;
    f_pCtx->oNewCert.BasicInformation.dwSelwrityLevel  = DRM_BCERT_SELWRITYLEVEL_2000;
    f_pCtx->oNewCert.BasicInformation.dwFlags          = DRM_BCERT_FLAGS_EMPTY;
    f_pCtx->oNewCert.BasicInformation.dwExpirationDate = DRM_BCERT_DEFAULT_EXPIRATION_DATE;

    f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.fValid = TRUE;
    f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.cbData = sizeof( DRM_SHA256_Digest );
    f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.iData  = 0;
    ChkDR( DRM_STK_Alloc( pXBCtx->pcontextStack, sizeof(DRM_SHA256_Digest), ( DRM_VOID ** )&f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.pbDataBuffer ) );

    if( f_pClientID != NULL )
    {
        f_pCtx->oNewCert.BasicInformation.ClientID = *f_pClientID;
    }

    ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_BASIC, &f_pCtx->oNewCert.BasicInformation ) );

    switch( f_dwCertType )
    {
        case DRM_BCERT_CERTTYPE_PC:
        {
            ChkBOOL( f_pCertData->PC.dwSelwrityVersion != 0, DRM_E_BCERT_ILWALID_SELWRITY_VERSION );

            f_pCtx->oNewCert.PCInfo.fValid            = TRUE;
            f_pCtx->oNewCert.PCInfo.dwSelwrityVersion = f_pCertData->PC.dwSelwrityVersion;
            ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_PC_INFO_ENTRY_TYPE, &f_pCtx->oNewCert.PCInfo ) );
            break;
        }

        case DRM_BCERT_CERTTYPE_SILVERLIGHT:
        {
            ChkBOOL( f_pCertData->Silverlight.dwPlatformIdentifier <= DRM_BCERT_SILVERLIGHT_PLATFORM_MAX, DRM_E_BCERT_ILWALID_PLATFORM_IDENTIFIER );
            ChkBOOL( f_pCertData->Silverlight.dwSelwrityVersion != 0, DRM_E_BCERT_ILWALID_SELWRITY_VERSION );

            f_pCtx->oNewCert.SilverlightInformation.fValid            = TRUE;
            f_pCtx->oNewCert.SilverlightInformation.dwPlatformID      = f_pCertData->Silverlight.dwPlatformIdentifier;
            f_pCtx->oNewCert.SilverlightInformation.dwSelwrityVersion = f_pCertData->Silverlight.dwSelwrityVersion;
            ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_SILVERLIGHT_INFO_ENTRY_TYPE, &f_pCtx->oNewCert.SilverlightInformation ) );
            break;
        }

        case DRM_BCERT_CERTTYPE_DEVICE:
        {
            f_pCtx->oNewCert.DeviceInformation.fValid                 = TRUE;
            f_pCtx->oNewCert.DeviceInformation.dwMaxLicenseSize       = f_pCertData->Device.cbMaxLicense;
            f_pCtx->oNewCert.DeviceInformation.dwMaxHeaderSize        = f_pCertData->Device.cbMaxHeader;
            f_pCtx->oNewCert.DeviceInformation.dwMaxLicenseChainDepth = f_pCertData->Device.dwMaxChainDepth;
            ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_DEVICE_INFO_ENTRY_TYPE, &f_pCtx->oNewCert.DeviceInformation ) );
            break;
        }

        case DRM_BCERT_CERTTYPE_DOMAIN:
        {
            ChkBOOL( 0 != MEMCMP( &g_DRM_BCERTFORMAT_EMPTY_ID, &f_pCertData->Domain.ServiceID, sizeof(DRM_ID) ), DRM_E_BCERT_SERVICE_ID_NOT_SPECIFIED );
            ChkBOOL( 0 != MEMCMP( &g_DRM_BCERTFORMAT_EMPTY_ID, &f_pCertData->Domain.AccountID, sizeof(DRM_ID) ), DRM_E_BCERT_ACCOUNT_ID_NOT_SPECIFIED );
            ChkBOOL( 0 != f_pCertData->Domain.cbDomainURL, DRM_E_BCERT_DOMAIN_URL_NOT_SPECIFIED );

            f_pCtx->oNewCert.DomainInformation.fValid                     = TRUE;
            f_pCtx->oNewCert.DomainInformation.ServiceID                  = f_pCertData->Domain.ServiceID;
            f_pCtx->oNewCert.DomainInformation.AccountID                  = f_pCertData->Domain.AccountID;
            f_pCtx->oNewCert.DomainInformation.dwRevision                 = f_pCertData->Domain.dwRevision;
            f_pCtx->oNewCert.DomainInformation.xbbaDomainUrl.fValid       = f_pCertData->Domain.cbDomainURL != 0;
            f_pCtx->oNewCert.DomainInformation.xbbaDomainUrl.iData        = 0;
            f_pCtx->oNewCert.DomainInformation.xbbaDomainUrl.cbData       = f_pCertData->Domain.cbDomainURL;
            f_pCtx->oNewCert.DomainInformation.xbbaDomainUrl.pbDataBuffer = ( DRM_BYTE * )f_pCertData->Domain.rgbDomainURL;
            ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_DOMAIN_INFO_ENTRY_TYPE, &f_pCtx->oNewCert.DomainInformation ) );
            break;
        }

        case DRM_BCERT_CERTTYPE_METERING:
        {
            ChkBOOL( 0 != MEMCMP( &g_DRM_BCERTFORMAT_EMPTY_ID, &f_pCertData->Metering.MeteringID, sizeof(DRM_ID) ), DRM_E_BCERT_METERING_ID_NOT_SPECIFIED );
            ChkBOOL( 0 != f_pCertData->Metering.cbMeteringURL, DRM_E_BCERT_METERING_URL_NOT_SPECIFIED );

            f_pCtx->oNewCert.MeteringInformation.fValid                       = TRUE;
            f_pCtx->oNewCert.MeteringInformation.MeteringID                   = f_pCertData->Metering.MeteringID;
            f_pCtx->oNewCert.MeteringInformation.xbbaMeteringUrl.fValid       = f_pCertData->Metering.cbMeteringURL != 0;
            f_pCtx->oNewCert.MeteringInformation.xbbaMeteringUrl.iData        = 0;
            f_pCtx->oNewCert.MeteringInformation.xbbaMeteringUrl.cbData       = f_pCertData->Metering.cbMeteringURL;
            f_pCtx->oNewCert.MeteringInformation.xbbaMeteringUrl.pbDataBuffer = ( DRM_BYTE * )f_pCertData->Metering.rgbMeteringURL;
            ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_METERING_INFO_ENTRY_TYPE, &f_pCtx->oNewCert.MeteringInformation ) );
            break;
        }

        case DRM_BCERT_CERTTYPE_SERVER:
        {
            ChkBOOL( 0 != f_pCertData->Server.dwWarningDays, DRM_E_BCERT_ILWALID_WARNING_DAYS );

            f_pCtx->oNewCert.ServerTypeInformation.fValid             = TRUE;
            f_pCtx->oNewCert.ServerTypeInformation.dwWarningStartDate = f_pCertData->Server.dwWarningDays;
            ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_SERVER_TYPE_INFO_ENTRY_TYPE, &f_pCtx->oNewCert.ServerTypeInformation ) );
            break;
        }

        case DRM_BCERT_CERTTYPE_CRL_SIGNER:
        case DRM_BCERT_CERTTYPE_ISSUER:
        case DRM_BCERT_CERTTYPE_SERVICE:
            /*
            ** Ensure that the Client ID is zero for Issuer, CRL Signer, and Service certificates
            */
            ChkBOOL( f_pClientID == NULL || MEMCMP( f_pClientID, &g_DRM_BCERTFORMAT_EMPTY_ID, sizeof(DRM_ID) ) == 0, DRM_E_ILWALIDARG );
            break;
        case DRM_BCERT_CERTTYPE_KEYFILESIGNER:
        case DRM_BCERT_CERTTYPE_LICENSESIGNER:
        case DRM_BCERT_CERTTYPE_APPLICATION:
            break;

        default:
            ChkDR( DRM_E_BCERT_ILWALID_CERT_TYPE );
            break;

    }

ErrorExit:
    return dr;
}
#endif

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
/*****************************************************************************
** Function:    _DRM_BCERTFORMAT_SetDefaultBuilderDeviceData
**
** Synopsis:    Fills in all the data for the device certificate with default values.
**              Those data that do not have default values are parameters to this function.
**
** Arguments:   [f_pCtx]           : The BCERT format builder context object.
**              [f_pCertData]      : The certificate data specific to the provided device certificate.
**              [f_pCertificateID] : The certificate ID
**              [f_pClientID]      : Optional. The client ID associated with the certificate
**
******************************************************************************/
static DRM_RESULT DRM_CALL _DRM_BCERTFORMAT_SetDefaultBuilderDeviceData(
    __inout_ecount( 1 )           DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL  *f_pCtx,
    __in                    const DRM_BCERTFORMAT_CERT_DEVICE_DATA          *f_pCertData,
    __in                    const DRM_ID                                    *f_pCertificateID,
    __in_opt                const DRM_ID                                    *f_pClientID )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_XB_BUILDER_CONTEXT_INTERNAL *pXBCtx = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, &f_pCtx->oBuilderCtx );

    ChkArg( f_pCtx           != NULL );
    ChkArg( f_pCertData      != NULL );
    ChkBOOL( f_pCertificateID != NULL, DRM_E_BCERT_CERT_ID_NOT_SPECIFIED );

    ChkBOOL( 0 != MEMCMP( &g_DRM_BCERTFORMAT_EMPTY_ID, f_pCertificateID, sizeof(DRM_ID) ), DRM_E_BCERT_CERT_ID_NOT_SPECIFIED );

    f_pCtx->oNewCert.fValid                            = TRUE;
    f_pCtx->oNewCert.BasicInformation.fValid           = TRUE;
    f_pCtx->oNewCert.BasicInformation.certID           = *f_pCertificateID;
    f_pCtx->oNewCert.BasicInformation.dwType           = DRM_BCERT_CERTTYPE_DEVICE;
    f_pCtx->oNewCert.BasicInformation.dwSelwrityLevel  = DRM_BCERT_SELWRITYLEVEL_2000;
    f_pCtx->oNewCert.BasicInformation.dwFlags          = DRM_BCERT_FLAGS_EMPTY;
    f_pCtx->oNewCert.BasicInformation.dwExpirationDate = DRM_BCERT_DEFAULT_EXPIRATION_DATE;

    f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.fValid = TRUE;
    f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.cbData = sizeof( DRM_SHA256_Digest );
    f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.iData  = 0;
    ChkDR( DRM_STK_Alloc( pXBCtx->pcontextStack, sizeof(DRM_SHA256_Digest), ( DRM_VOID ** )&f_pCtx->oNewCert.BasicInformation.xbbaDigestValue.pbDataBuffer ) );

    if( f_pClientID != NULL )
    {
        f_pCtx->oNewCert.BasicInformation.ClientID = *f_pClientID;
    }
    
    ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_BASIC, &f_pCtx->oNewCert.BasicInformation ) );
    f_pCtx->oNewCert.DeviceInformation.fValid                 = TRUE;
    f_pCtx->oNewCert.DeviceInformation.dwMaxLicenseSize       = f_pCertData->cbMaxLicense;
    f_pCtx->oNewCert.DeviceInformation.dwMaxHeaderSize        = f_pCertData->cbMaxHeader;
    f_pCtx->oNewCert.DeviceInformation.dwMaxLicenseChainDepth = f_pCertData->dwMaxChainDepth;
    ChkDR( DRM_XB_AddEntry( &f_pCtx->oBuilderCtx, DRM_BCERTFORMAT_DEVICE_INFO_ENTRY_TYPE, &f_pCtx->oNewCert.DeviceInformation ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetFeatures
**
** Synopsis:    Adds the provided certificate feature set to the certificate builder
**              context which will be applied to the resulting leaf most certificate
**              when DRM_BCERTFORMAT_FinishCertificateChain is called.
**
** Arguments:   [f_pBuilderCtx] : The BCERT format builder context object.
**              [f_cFeatures]   : The number of DWORD features in f_pdwFeatures.
**              [f_pdwFeatures] : An array of DWORD certificate feature values.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetFeatures(
    __inout_ecount( 1 )                 DRM_BCERTFORMAT_BUILDER_CONTEXT       *f_pBuilderCtx,
    __in                                DRM_DWORD                              f_cFeatures,
    __in_ecount( f_cFeatures )    const DRM_DWORD                             *f_pdwFeatures )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx != NULL );
    ChkArg( (f_pdwFeatures == NULL) == (f_cFeatures == 0) );

    pCtx->oNewCert.FeatureInformation.fValid                   = TRUE;
    pCtx->oNewCert.FeatureInformation.dwlFeatures.fValid       = TRUE;
    pCtx->oNewCert.FeatureInformation.dwlFeatures.iDwords      = 0;
    pCtx->oNewCert.FeatureInformation.dwlFeatures.cDWORDs      = f_cFeatures;
    pCtx->oNewCert.FeatureInformation.dwlFeatures.pdwordBuffer = ( DRM_BYTE * )f_pdwFeatures;

    ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_FEATURE, &pCtx->oNewCert.FeatureInformation ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetSelwrityLevel
**
** Synopsis:    Sets the security level for the certificate being created.
**
** Arguments:   [f_pBuilderCtx]     : The BCERT format builder context object.
**              [f_dwSelwrityLevel] : The certificate security level.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetSelwrityLevel(
    __inout_ecount( 1 )                 DRM_BCERTFORMAT_BUILDER_CONTEXT       *f_pBuilderCtx,
    __in                                DRM_DWORD                              f_dwSelwrityLevel )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx != NULL );
    pCtx->oNewCert.BasicInformation.fValid            = TRUE;
    pCtx->oNewCert.BasicInformation.dwSelwrityLevel   = f_dwSelwrityLevel;

    /* An XB entry for basic information is added by default, so do not try to add it again. */

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetBuilderFlags
**
** Synopsis:    Sets the chain header, basic information and manufacturer
**              flags.
**
** Arguments:   [f_pBuilderCtx]        : The BCERT format builder context object.
**              [f_dwChainHeaderFlags] : The chain header flags.
**              [f_dwBasicInfoFlags]   : The basic information flags.
**              [f_dwManufacturerFlags]: The manufacturer specific flags.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetBuilderFlags(
    __inout_ecount( 1 )                 DRM_BCERTFORMAT_BUILDER_CONTEXT       *f_pBuilderCtx,
    __in                                DRM_DWORD                              f_dwChainHeaderFlags,
    __in                                DRM_DWORD                              f_dwBasicInfoFlags,
    __in                                DRM_DWORD                              f_dwManufacturerFlags )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx != NULL );

    pCtx->oCertChain.Header.dwFlags                = f_dwChainHeaderFlags;
    pCtx->oNewCert.BasicInformation.dwFlags        = f_dwBasicInfoFlags | (pCtx->oNewCert.BasicInformation.dwFlags & DRM_BCERT_FLAGS_EXTDATA_PRESENT);
    pCtx->oNewCert.ManufacturerInformation.dwFlags = f_dwManufacturerFlags;

    /* An XB entry added by default, so do not try to add it again. */

ErrorExit:
    return dr;
}
#endif

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetSelwrityVersion
**
** Synopsis:    Sets the platform ID and the certificate security version value.
**
** Arguments:   [f_pBuilderCtx]       : The BCERT format builder context object.
**              [f_dwPlatformID]      : The platform ID for the security version.
**              [f_dwSelwrityVersion] : The value of the certificate security version.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetSelwrityVersion(
    __inout_ecount( 1 )                 DRM_BCERTFORMAT_BUILDER_CONTEXT       *f_pBuilderCtx,
    __in                                DRM_DWORD                              f_dwPlatformID,
    __in                                DRM_DWORD                              f_dwSelwrityVersion )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx != NULL );

    switch( pCtx->oNewCert.BasicInformation.dwType )
    {
        case DRM_BCERT_CERTTYPE_SILVERLIGHT:
            ChkBOOL( f_dwPlatformID      <= DRM_BCERT_SILVERLIGHT_PLATFORM_MAX, DRM_E_BCERT_ILWALID_PLATFORM_IDENTIFIER );
            __fallthrough;
        case DRM_BCERT_CERTTYPE_PC:
            ChkBOOL( f_dwSelwrityVersion != 0, DRM_E_BCERT_ILWALID_SELWRITY_VERSION );
            break;
        default:
            break;
    }

    pCtx->oNewCert.SelwrityVersion2.fValid            = TRUE;
    pCtx->oNewCert.SelwrityVersion2.dwPlatformID      = f_dwPlatformID;
    pCtx->oNewCert.SelwrityVersion2.dwSelwrityVersion = f_dwSelwrityVersion;

    ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_SELWRITY_VERSION_2, &pCtx->oNewCert.SelwrityVersion2 ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetExpirationDate
**
** Synopsis:    Sets the certificate's expiration date.
**
** Arguments:   [f_pBuilderCtx]      : The BCERT format builder context object.
**              [f_dwExpirationDate] : The expiration date.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetExpirationDate(
    __inout_ecount( 1 )                 DRM_BCERTFORMAT_BUILDER_CONTEXT     *f_pBuilderCtx,
    __in                                DRM_DWORD                            f_dwExpirationDate )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx != NULL );

    pCtx->oNewCert.BasicInformation.dwExpirationDate = f_dwExpirationDate;

    /* An XB entry for basic information is added by default, so do not try to add it again. */

ErrorExit:
    return dr;
}
#endif

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT _CopyEnforceUtf8(
    __in                            DRM_DWORD  f_cbIn,
    __in_bcount(f_cbIn)       const DRM_BYTE  *f_pbIn,
    __inout                         DRM_DWORD *f_pcbOut,
    __inout_bcount(*f_pcbOut)       DRM_BYTE  *f_pbOut,
    __out_ecount(1)                 DRM_XB_BYTEARRAY *f_pxbbaOut )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pbIn != NULL );
    ChkArg( f_cbIn > 0 );
    ChkArg( f_cbIn < *f_pcbOut );

    /* copy over what was provided */
    ChkVOID( MEMCPY( f_pbOut, f_pbIn, f_cbIn ) );

    /* in case not zero terminated, make sure is zero terminated */
    if ( f_pbOut[f_cbIn-1] != '\0' )
    {
        f_pbOut[f_cbIn] = '\0';
        f_cbIn++;
    }

    *f_pcbOut = f_cbIn;
    ChkDR( DRM_STR_UTF8_IsUTF8NullTerminatedData(
        f_pbOut,
        *f_pcbOut ) );

    f_pxbbaOut->fValid       = TRUE;
    f_pxbbaOut->iData        = 0;
    f_pxbbaOut->cbData       = *f_pcbOut;
    f_pxbbaOut->pbDataBuffer = f_pbOut;

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetManufacturerName
**
** Synopsis:    Sets manufacturer name string for the certificate.
**
** Arguments:   [f_pBuilderCtx]        : The BCERT format builder context object.
**              [f_cbManufacturerName] : The size in bytes of the manufacturer name to be
**                                       applied to the certificate.
**              [f_pbManufacturerName] : The manufacturer name string to be applied to the
**                                       certificate.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetManufacturerName(
    __inout_ecount( 1 )                             DRM_BCERTFORMAT_BUILDER_CONTEXT     *f_pBuilderCtx,
    __in                                            DRM_DWORD                            f_cbManufacturerName,
    __in_bcount( f_cbManufacturerName )       const DRM_BYTE                            *f_pbManufacturerName )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx   != NULL );

    pCtx->oManufactureStrings.ManufacturerName.cb = sizeof(pCtx->oManufactureStrings.ManufacturerName.rgb);

    ChkDR( _CopyEnforceUtf8(
        f_cbManufacturerName,
        f_pbManufacturerName,
        &pCtx->oManufactureStrings.ManufacturerName.cb,
        pCtx->oManufactureStrings.ManufacturerName.rgb,
        &pCtx->oNewCert.ManufacturerInformation.xbbaManufacturerName ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetModelName
**
** Synopsis:    Sets model name manufacturer string for the certificate.
**
** Arguments:   [f_pBuilderCtx]        : The BCERT format builder context object.
**              [f_cbModelName]        : The size in bytes of the model name to be applied
**                                       to the certificate.
**              [f_pbModelName]        : The model name string to be applied to the
**                                       certificate.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetModelName(
    __inout_ecount( 1 )                             DRM_BCERTFORMAT_BUILDER_CONTEXT     *f_pBuilderCtx,
    __in                                            DRM_DWORD                            f_cbModelName,
    __in_bcount( f_cbModelName )              const DRM_BYTE                            *f_pbModelName )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx   != NULL );

    pCtx->oManufactureStrings.ModelName.cb = sizeof(pCtx->oManufactureStrings.ModelName.rgb);

    ChkDR( _CopyEnforceUtf8(
        f_cbModelName,
        f_pbModelName,
        &pCtx->oManufactureStrings.ModelName.cb,
        pCtx->oManufactureStrings.ModelName.rgb,
        &pCtx->oNewCert.ManufacturerInformation.xbbaModelName ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetModelNumber
**
** Synopsis:    Sets the model number manufacturer string for the certificate.
**
** Arguments:   [f_pBuilderCtx]        : The BCERT format builder context object.
**              [f_cbModelNumber]      : The size in bytes of the model number to be
**                                       applied to the certificate.
**              [f_pbModelNumber]      : The model number string to be applied to the
**                                       certificate.
**
******************************************************************************/

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetModelNumber(
    __inout_ecount( 1 )                             DRM_BCERTFORMAT_BUILDER_CONTEXT     *f_pBuilderCtx,
    __in                                            DRM_DWORD                            f_cbModelNumber,
    __in_bcount( f_cbModelNumber )            const DRM_BYTE                            *f_pbModelNumber )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx   != NULL );

    pCtx->oManufactureStrings.ModelNumber.cb = sizeof(pCtx->oManufactureStrings.ModelNumber.rgb);

    ChkDR( _CopyEnforceUtf8(
        f_cbModelNumber,
        f_pbModelNumber,
        &pCtx->oManufactureStrings.ModelNumber.cb,
        pCtx->oManufactureStrings.ModelNumber.rgb,
        &pCtx->oNewCert.ManufacturerInformation.xbbaModelNumber ) );

ErrorExit:
    return dr;
}


/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetKeyInfo
**
** Synopsis:    Fills in all the data specific to the key info object with default values.
**              Those data that do not have default values are parameters to this function.
**              It is completely optional to call this function, it is provided as a colwenience.
**              As such, it does not duplicate any input data validity checking; input data
**              validity checks are in the code that builds the certificate chain.
**
** Arguments:   [f_pBuilderCtx] : The BCERT format builder context object.
**              [f_cKeys]       : Number of elements in f_pKeys array.
**              [f_pKeys]       : Pointer to the array of certificate keys.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetKeyInfo(
    __inout_ecount( 1 )             DRM_BCERTFORMAT_BUILDER_CONTEXT    *f_pBuilderCtx,
    __in                            DRM_DWORD                           f_cKeys,
    __in_ecount( f_cKeys )          DRM_BCERTFORMAT_BUILDER_KEYDATA    *f_pKeys )
{
    DRM_RESULT                                dr       = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx     = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );
    DRM_BCERTFORMAT_KEY_TYPE                 *pKeyInfo = NULL;

    ChkArg( f_pBuilderCtx != NULL );
    ChkArg( ( f_cKeys == 0 ) == ( f_pKeys == NULL ) );

    if( f_cKeys > 0 )
    {
        DRM_DWORD                        iCount;    /* Initialized in for loop      */
        DRM_DWORD                        cbKeyData; /* Initialized in DRM_DWordMult */
        DRM_XB_BUILDER_CONTEXT_INTERNAL *pXBCtx     = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, &pCtx->oBuilderCtx );

        ChkDR( DRM_DWordMult( f_cKeys, sizeof(DRM_BCERTFORMAT_KEY_TYPE), &cbKeyData ) );
        ChkDR( DRM_STK_Alloc( pXBCtx->pcontextStack, cbKeyData, ( DRM_VOID ** )&pKeyInfo ) );
        ZEROMEM( pKeyInfo, cbKeyData );

        ChkDR( DRM_DWordMult( f_cKeys, sizeof(DRM_BCERTFORMAT_BUILDER_KEYDATA), &cbKeyData ) );
        ChkDR( DRM_STK_Alloc( pXBCtx->pcontextStack, cbKeyData, ( DRM_VOID ** )&pCtx->pKeyData ) );
        ZEROMEM( pCtx->pKeyData, cbKeyData );

        for( iCount = 0; iCount < f_cKeys; iCount++ )
        {
            DRM_DWORD iUsage;

            ChkBOOL( f_pKeys[iCount].cKeyUsages > 0, DRM_E_BCERT_KEY_USAGES_NOT_SPECIFIED );

            /* Save local copy of the key */
            ChkArg( f_pKeys[iCount].cbKey <= sizeof(pCtx->pKeyData[iCount].rgbKey) );
            MEMCPY( pCtx->pKeyData[iCount].rgbKey, f_pKeys[iCount].rgbKey, f_pKeys[iCount].cbKey );

            /* Save local copy of the key usages */
            ChkArg( f_pKeys[iCount].cKeyUsages <= DRM_BCERT_MAX_KEY_USAGES );
            MEMCPY( pCtx->pKeyData[iCount].rgdwKeyUsages, f_pKeys[iCount].rgdwKeyUsages, f_pKeys[iCount].cKeyUsages * sizeof(DRM_DWORD) ); /* Cannot overflow because of previous count check */

            for( iUsage = 0; iUsage < f_pKeys[iCount].cKeyUsages; iUsage++ )
            {
                ChkBOOL( f_pKeys[iCount].rgdwKeyUsages[iUsage] > 0                            , DRM_E_BCERT_ILWALID_KEY_USAGE );
                ChkBOOL( f_pKeys[iCount].rgdwKeyUsages[iUsage] <= DRM_BCERT_KEYUSAGE_MAX_VALUE, DRM_E_BCERT_ILWALID_KEY_USAGE );
             }

            ChkDR( DRM_WordMult( f_pKeys[iCount].cbKey, 8, &pKeyInfo[iCount].wKeyLength ) ); /* key length is in bits */
            pKeyInfo[iCount].fValid                    = TRUE;
            pKeyInfo[iCount].wType                     = f_pKeys[iCount].wKeyType;
            pKeyInfo[iCount].dwFlags                   = 0;
            pKeyInfo[iCount].dwUsageSet                = 0;
            pKeyInfo[iCount].dwlKeyUsages.fValid       = TRUE;
            pKeyInfo[iCount].dwlKeyUsages.cDWORDs      = f_pKeys[iCount].cKeyUsages;
            pKeyInfo[iCount].dwlKeyUsages.pdwordBuffer = ( DRM_BYTE * )pCtx->pKeyData[iCount].rgdwKeyUsages;
            pKeyInfo[iCount].dwlKeyUsages.iDwords      = 0;
            pKeyInfo[iCount].xbbaKeyValue.fValid       = TRUE;
            pKeyInfo[iCount].xbbaKeyValue.cbData       = f_pKeys[iCount].cbKey;
            pKeyInfo[iCount].xbbaKeyValue.pbDataBuffer = pCtx->pKeyData[iCount].rgbKey;
            pKeyInfo[iCount].xbbaKeyValue.iData        = 0;
        }
    }

    pCtx->oNewCert.KeyInformation.fValid   = TRUE;
    pCtx->oNewCert.KeyInformation.cEntries = f_cKeys;
    pCtx->oNewCert.KeyInformation.pHead    = pKeyInfo;

    ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_KEY, &pCtx->oNewCert.KeyInformation ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetExtData
**
** Synopsis:    Sets the extended data record for the certificate.  This is generally only
**              used for PC certificates for adding a hardware identifier.
**
** Arguments:   [f_pBuilderCtx] : The BCERT format builder context object.
**              [f_cbExtData]   : Number of bytes in f_pbExtData.
**              [f_pbExtData]   : Pointer to the external certificate data.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetExtData(
    __inout_ecount( 1 )                             DRM_BCERTFORMAT_BUILDER_CONTEXT     *f_pBuilderCtx,
    __in                                            DRM_DWORD                            f_cbExtData,
    __in_bcount( f_cbExtData )                const DRM_BYTE                            *f_pbExtData )
{
    DRM_RESULT                                dr    = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx  = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx != NULL );

    ChkBOOL( f_pbExtData != NULL, DRM_E_BCERT_ILWALID_EXTDATARECORD );
    ChkBOOL( f_cbExtData != 0   , DRM_E_BCERT_ILWALID_EXTDATARECORD );

    pCtx->oNewCert.ExDataContainer.HwidRecord.fValid                = TRUE;
    pCtx->oNewCert.ExDataContainer.HwidRecord.xbbaData.fValid       = TRUE;
    pCtx->oNewCert.ExDataContainer.HwidRecord.xbbaData.iData        = 0;
    pCtx->oNewCert.ExDataContainer.HwidRecord.xbbaData.cbData       = f_cbExtData;
    pCtx->oNewCert.ExDataContainer.HwidRecord.xbbaData.pbDataBuffer = ( DRM_BYTE * )f_pbExtData;

    pCtx->oNewCert.BasicInformation.dwFlags |= DRM_BCERT_FLAGS_EXTDATA_PRESENT;

    ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_EXTDATA_HWID, &pCtx->oNewCert.ExDataContainer.HwidRecord ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SetExtDataKeyInfo
**
** Synopsis:    Sets the signing key information for the extended data record of the
**              certificate.
**
** Arguments:   [f_pBuilderCtx]         : The BCERT format builder context object.
**              [f_wKeyType]            : The signature key type.
**              [f_wKeyLen]             : The length in bits of the signature verification public key.
**              [f_dwFlags]             : The flags associated with the extended data signature.
**              [f_cbPrivateSigningKey] : The size in bytes of the signing private key.
**              [f_pbPrivateSigningKey] : The extended data signing private key.
**              [f_cbPublicSigningKey]  : The size in bytes of the public signing key.
**              [f_pbPublicSigningKey]  : The extended data signing public key.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SetExtDataKeyInfo(
    __inout_ecount( 1 )                             DRM_BCERTFORMAT_BUILDER_CONTEXT     *f_pBuilderCtx,
    __in                                            DRM_WORD                             f_wKeyType,
    __in                                            DRM_WORD                             f_wKeyLen,
    __in                                            DRM_DWORD                            f_dwFlags,
    __in                                            DRM_DWORD                            f_cbPrivateSigningKey,
    __in_bcount( f_cbPrivateSigningKey )      const DRM_BYTE                            *f_pbPrivateSigningKey,
    __in                                            DRM_DWORD                            f_cbPublicSigningKey,
    __in_bcount( f_cbPublicSigningKey )       const DRM_BYTE                            *f_pbPublicSigningKey )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx         != NULL );
    ChkArg( f_pbPrivateSigningKey != NULL );
    ChkArg( f_cbPrivateSigningKey  > 0    );
    ChkArg( f_pbPublicSigningKey  != NULL );
    ChkArg( f_cbPublicSigningKey   > 0    );

    pCtx->pbExtPrivKey = f_pbPrivateSigningKey;
    pCtx->cbExtPrivKey = f_cbPrivateSigningKey;

    pCtx->oNewCert.ExDataSigKeyInfo.fValid                   = TRUE;
    pCtx->oNewCert.ExDataSigKeyInfo.dwFlags                  = f_dwFlags;
    pCtx->oNewCert.ExDataSigKeyInfo.wKeyLen                  = f_wKeyLen;
    pCtx->oNewCert.ExDataSigKeyInfo.wType                    = f_wKeyType;
    pCtx->oNewCert.ExDataSigKeyInfo.xbbKeyValue.fValid       = TRUE;
    pCtx->oNewCert.ExDataSigKeyInfo.xbbKeyValue.iData        = 0;
    pCtx->oNewCert.ExDataSigKeyInfo.xbbKeyValue.cbData       = f_cbPublicSigningKey;
    pCtx->oNewCert.ExDataSigKeyInfo.xbbKeyValue.pbDataBuffer = ( DRM_BYTE * )f_pbPublicSigningKey;

    ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_EXTDATASIGNKEY, &pCtx->oNewCert.ExDataSigKeyInfo ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_LoadCertificateChain
**
** Synopsis:    Loads a certificate chain for editing.  The chain can be re-written
**              by calling DRM_BCERTFORMAT_SaveCertificateChain.  This function is used
**              creating a certificate in two phases; where the second phase adds the
**              extended data portion (HWID) to the certificate.
**
** Arguments:   [f_pOemTeeContext] : Optional. The OEM TEE context used when generating the
**                                   signature by calling Oem_Broker_ECDSA_P256_Sign.
**              [f_pStack]         : The stack memory buffer which will be used to
**                                   store XBinary structures during the certificate
**                                   building.
**              [f_cbCertChain]    : The size in bytes of the parent certificate chain.
**              [f_pbCertChain]    : A pointer to the serialized parent certificate chain.
**              [f_ppCertWeakRef]  : Will hold a weak reference pointer to the leaf most
**                                   certificate for editing.
**              [f_pBuilderCtx]    : The BCERT format builder context.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_LoadCertificateChain(
    __inout_opt                                 OEM_TEE_CONTEXT                         *f_pOemTeeContext,
    __inout                                     DRM_STACK_ALLOCATOR_CONTEXT             *f_pStack,
    __in                                        DRM_DWORD                                f_cbCertChain,
    __in_bcount( f_cbCertChain )          const DRM_BYTE                                *f_pbCertChain,
    __out_ecount( 1 )                           DRM_BCERTFORMAT_CERT                   **f_ppCertWeakRef,
    __out_ecount( 1 )                           DRM_BCERTFORMAT_BUILDER_CONTEXT         *f_pBuilderCtx,
    __out_ecount( 1 )                           DRM_BCERTFORMAT_PARSER_CONTEXT          *f_pParserCtx )
{
    DRM_RESULT                                dr   = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );

    ChkArg( f_pBuilderCtx   != NULL );
    ChkArg( f_pbCertChain   != NULL );
    ChkArg( f_cbCertChain    > 0    );
    ChkArg( f_ppCertWeakRef != NULL );
    ChkArg( f_pParserCtx    != NULL );

    ZEROMEM( &pCtx->oNewCert  , sizeof(pCtx->oNewCert  ) );
    ZEROMEM( &pCtx->oCertChain, sizeof(pCtx->oCertChain) );

    *f_ppCertWeakRef = NULL;

    ChkDR( DRM_XB_StartFormatFromStack(
        f_pStack,
        DRM_BCERTFORMAT_LWRRENT_VERSION,
        &pCtx->oBuilderCtx,
        s_DRM_BCERTFORMAT_CERT_FormatDescription ) );

    ChkDR( DRM_BCERTFORMAT_InitializeParserContext(
        TRUE,
        TRUE,
        0,
        NULL,
        0,
        NULL,
        f_pParserCtx ) );

    ChkDR( DRM_BCERTFORMAT_ParseCertificateChain(
        f_pParserCtx,
        f_pOemTeeContext,
        NULL,
        DRM_BCERT_CERTTYPE_UNKNOWN,
        f_pStack,
        f_cbCertChain,
        f_pbCertChain,
        NULL,
        &pCtx->oCertChain,
        &pCtx->oNewCert ) );

    ChkBOOL( pCtx->oCertChain.Header.dwVersion == DRM_BCERTFORMAT_LWRRENT_VERSION, DRM_E_BCERT_ILWALID_CERT_VERSION );

    /* Remove the leaf most cert from the chain since it is used as the new cert. */
    pCtx->oCertChain.Header.cCerts--;
    pCtx->oCertChain.Certificates.cEntries--;
    pCtx->oCertChain.Certificates.pCertHeaders++;

    /* Reset signatures and header data so that it will be re-written. */
    pCtx->oNewCert.HeaderData.fValid = FALSE;
    pCtx->oNewCert.SignatureInformation.fValid = FALSE;
    pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.fValid = FALSE;

    *f_ppCertWeakRef = &pCtx->oNewCert;

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_SaveCertificateChain
**
** Synopsis:    Saves a certificate chain that was previously opened for edit via
**              DRM_BCERTFORMAT_LoadCertificateChain.
**
** Arguments:   [f_pBuilderCtx]         : The BCERT format builder context.
**              [f_pOemTeeContext]      : Optional. The OEM TEE context used when generating the
**                                        signature by calling Oem_Broker_ECDSA_P256_Sign.
**              [f_pIssuerPrivateKey]   : The issuer private key.
**              [f_pIssuerPublicKey]    : The public key for the issuer of this certificate.
**              [f_pcbCertChain]        : On input, the size in bytes of the certificate chain buffer
**                                        parameter (f_pbCertificateChain).  On output, the size of
**                                        the serialized certificate chain.  If, f_pbCertificateChain
**                                        is NULL, the out value for this parameter will be the required
**                                        size in bytes for the serialized certificate chain.
**              [f_pbCertChain]         : The buffer that will hold the serialized certificate chain.
**                                        If this parameter is NULL, then f_pcbCertificateChain will
**                                        be set to the required size for the certificate chain.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_SaveCertificateChain(
    __inout                                     DRM_BCERTFORMAT_BUILDER_CONTEXT         *f_pBuilderCtx,
    __inout_opt                                 OEM_TEE_CONTEXT                         *f_pOemTeeContext,
    __in                                  const PRIVKEY_P256                            *f_pIssuerPrivateKey,
    __in                                  const PUBKEY_P256                             *f_pIssuerPublicKey,
    __out                                       DRM_DWORD                               *f_pcbCertChain,
    __out_bcount_opt( *f_pcbCertChain )         DRM_BYTE                                *f_pbCertChain )
{
    DRM_RESULT                                dr     = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx   = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );
    DRM_XB_BUILDER_CONTEXT                   *pXBCtx = NULL;

    ChkArg( f_pBuilderCtx       != NULL );
    ChkArg( f_pIssuerPrivateKey != NULL );
    ChkArg( f_pIssuerPublicKey  != NULL );
    ChkArg( f_pcbCertChain      != NULL );

    pXBCtx = ( DRM_XB_BUILDER_CONTEXT * )&pCtx->oBuilderCtx;

    /*
    ** Try to add all of the required or valid certificate objects. Allow DRM_E_XB_OBJECT_ALREADY_EXISTS errors because we
    ** may have already added the object.
    */
    ChkDRAllowXBObjectExists( DRM_XB_AddEntry( pXBCtx, XB_OBJECT_GLOBAL_HEADER, &pCtx->oNewCert ) );

    ChkDR( DRM_BCERTFORMAT_FinishCertificateChain(
        f_pBuilderCtx,
        f_pOemTeeContext,
        f_pIssuerPrivateKey,
        f_pIssuerPublicKey,
        f_pcbCertChain,
        f_pbCertChain,
        NULL ) );

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_StartCertificateChain
**
** Synopsis:    Starts the certificate building process and initializes the builder
**              context with the provided parameters.  If a parent certificate chain
**              is provided, it will be parsed and validated and the new certificate
**              will added to the beginning of the chain with the call to
**              DRM_BCERTFORMAT_FinishCertificateChain.
**
** Arguments:   [f_pStack]          : The stack memory buffer which will be used to
**                                    store XBinary structures during the certificate
**                                    generation.
**              [f_cbParentChain]   : The size in bytes of the parent certificate chain.
**              [f_pbParentChain]   : A pointer to the serialized parent certificate chain.
**              [f_dwCertType]      : The type of certificate to create.
**              [f_pCertData]       : The certificate data specific to the provided certificate
**                                    type (f_dwCertType).
**              [f_pCertificateID]  : The certificate identifier.
**              [f_pClientID]       : Optional. The client ID for the certificate.
**              [f_pcontextBuilder] : The BCERT format builder context.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_StartCertificateChain(
    __inout                                     DRM_STACK_ALLOCATOR_CONTEXT             *f_pStack,
    __in                                        DRM_DWORD                                f_cbParentChain,
    __in_bcount_opt( f_cbParentChain )    const DRM_BYTE                                *f_pbParentChain,
    __in                                        DRM_DWORD                                f_dwCertType,
    __in                                  const DRM_BCERTFORMAT_CERT_TYPE_DATA          *f_pCertData,
    __in                                  const DRM_ID                                  *f_pCertificateID,
    __in_opt                              const DRM_ID                                  *f_pClientID,
    __out_ecount( 1 )                           DRM_BCERTFORMAT_BUILDER_CONTEXT         *f_pcontextBuilder )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );

    ChkArg( f_pcontextBuilder != NULL );
    ChkArg( f_pCertificateID != NULL );

    ZEROMEM( &pCtx->oNewCert  , sizeof(pCtx->oNewCert  ) );
    ZEROMEM( &pCtx->oCertChain, sizeof(pCtx->oCertChain) );

    ChkDR( DRM_XB_StartFormatFromStack(
        f_pStack,
        DRM_BCERTFORMAT_LWRRENT_VERSION,
        &pCtx->oBuilderCtx,
        s_DRM_BCERTFORMAT_CERT_FormatDescription ) );

    ChkDR( _DRM_BCERTFORMAT_SetDefaultBuilderData( pCtx, f_dwCertType, f_pCertData, f_pCertificateID, f_pClientID ) );

    if( f_pbParentChain != NULL )
    {
        DRM_DWORD cbHeader    = f_cbParentChain;
        DRM_DWORD cbContainer = 0;
        DRM_DWORD dwVersion   = 0;
        DRM_XB_BUILDER_CONTEXT_INTERNAL *pXBCtx = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, &pCtx->oBuilderCtx );

        ChkDR( DRM_XB_UnpackHeader(
            f_pbParentChain,
            cbHeader,
            pXBCtx->pcontextStack,
            s_DRM_BCERTFORMAT_CHAIN_FormatDescription,
            DRM_NO_OF(s_DRM_BCERTFORMAT_CHAIN_FormatDescription),
            &dwVersion,
            &cbHeader,
            &cbContainer,
            NULL,
            &pCtx->oCertChain ) );

        ChkBOOL( dwVersion == DRM_BCERTFORMAT_LWRRENT_VERSION, DRM_E_BCERT_ILWALID_CERT_VERSION );
        ChkBOOL( pCtx->oCertChain.Header.cCerts < DRM_BCERT_MAX_CERTS_PER_CHAIN, DRM_E_BCERT_CHAIN_TOO_DEEP );
    }

ErrorExit:
    return dr;
}
#endif

#if !defined (SEC_USE_CERT_TEMPLATE) && defined (SEC_COMPILE)
/*****************************************************************************
** Function:    DRM_BCERTFORMAT_StartDeviceCertificateChain
**
** Synopsis:    Starts the device certificate building process and initializes the builder
**              context with the provided parameters.  If a parent certificate chain
**              is provided, it will be parsed and validated and the new device certificate
**              will added to the beginning of the chain with the call to
**              DRM_BCERTFORMAT_FinishCertificateChain.
**
** Arguments:   [f_pStack]          : The stack memory buffer which will be used to
**                                    store XBinary structures during the certificate
**                                    generation.
**              [f_cbParentChain]   : The size in bytes of the parent certificate chain.
**              [f_pbParentChain]   : A pointer to the serialized parent certificate chain.
**              [f_pCertData]       : The device certificate data specific to the provided
**                                    certificate type (f_dwCertType).
**              [f_pCertificateID]  : The certificate identifier.
**              [f_pClientID]       : Optional. The client ID for the certificate.
**              [f_pcontextBuilder] : The BCERT format builder context.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_StartDeviceCertificateChain(
    __inout                                     DRM_STACK_ALLOCATOR_CONTEXT             *f_pStack,
    __in                                        DRM_DWORD                                f_cbParentChain,
    __in_bcount_opt( f_cbParentChain )    const DRM_BYTE                                *f_pbParentChain,
    __in                                  const DRM_BCERTFORMAT_CERT_DEVICE_DATA        *f_pCertData,
    __in                                  const DRM_ID                                  *f_pCertificateID,
    __in_opt                              const DRM_ID                                  *f_pClientID,
    __out_ecount( 1 )                           DRM_BCERTFORMAT_BUILDER_CONTEXT         *f_pcontextBuilder )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pcontextBuilder );

    ChkArg( f_pcontextBuilder != NULL );
    ChkArg( f_pCertificateID != NULL );

    ZEROMEM( &pCtx->oNewCert  , sizeof(pCtx->oNewCert  ) );
    ZEROMEM( &pCtx->oCertChain, sizeof(pCtx->oCertChain) );

    ChkDR( DRM_XB_StartFormatFromStack(
        f_pStack,
        DRM_BCERTFORMAT_LWRRENT_VERSION,
        &pCtx->oBuilderCtx,
        s_DRM_BCERTFORMAT_CERT_FormatDescription ) );

    ChkDR( _DRM_BCERTFORMAT_SetDefaultBuilderDeviceData( pCtx, f_pCertData, f_pCertificateID, f_pClientID ) );

    if( f_pbParentChain != NULL )
    {
        DRM_DWORD cbHeader    = f_cbParentChain;
        DRM_DWORD cbContainer = 0;
        DRM_DWORD dwVersion   = 0;
        DRM_XB_BUILDER_CONTEXT_INTERNAL *pXBCtx = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, &pCtx->oBuilderCtx );

        ChkDR( DRM_XB_UnpackHeader(
            f_pbParentChain,
            cbHeader,
            pXBCtx->pcontextStack,
            s_DRM_BCERTFORMAT_CHAIN_FormatDescription,
            DRM_NO_OF(s_DRM_BCERTFORMAT_CHAIN_FormatDescription),
            &dwVersion,
            &cbHeader,
            &cbContainer,
            NULL,
            &pCtx->oCertChain ) );

        ChkBOOL( dwVersion == DRM_BCERTFORMAT_LWRRENT_VERSION, DRM_E_BCERT_ILWALID_CERT_VERSION );
        ChkBOOL( pCtx->oCertChain.Header.cCerts < DRM_BCERT_MAX_CERTS_PER_CHAIN, DRM_E_BCERT_CHAIN_TOO_DEEP );
    }

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCERTFORMAT_FinishCertificateChain
**
** Synopsis:    This function finishes the creation of the certificate (or certificate chain)
**              by serializing the certificate data stored in the builder context.  If a parent
**              certificate chain was provided in the DRM_BCERTFORMAT_StartCertificateChain,
**              then the entire chain will be serialized with the new certificate at the front.
**              Otherwise only a single certificate will be serialized with the chain header.
**              If the optional signature callback parameter (f_pfnSignature) is provided, it
**              will be ilwoked after the certificate has been serialized.  It may be called
**              more than once if extended certificate data is provided (f_pbExtData).
**
** Arguments:   [f_pBuilderCtx]         : The BCERT format builder context.
**              [f_pOemTeeContext]      : Optional. The OEM TEE context used when generating the
**                                        signature by calling Oem_Broker_ECDSA_P256_Sign.
**              [f_pIssuerPrivateKey]   : The issuer private key; if not specified, signature
**                                        is not filled, but field still reserved.
**              [f_pIssuerPublicKey]    : The public key for the issuer of this certificate.
**              [f_pcbCertificateChain] : On input, the size in bytes of the certificate chain buffer
**                                        parameter (f_pbCertificateChain).  On output, the size of
**                                        the serialized certificate chain.  If, f_pbCertificateChain
**                                        is NULL, the out value for this parameter will be the required
**                                        size in bytes for the serialized certificate chain.
**              [f_pbCertificateChain]  : The buffer that will hold the serialized certificate chain.
**                                        If this parameter is NULL, then f_pcbCertificateChain will
**                                        be set to the required size for the certificate chain.
**              [f_pSignatureInfo]      : Optional. A pointer that will hold the signature information
**                                        related to the certificate being created.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCERTFORMAT_FinishCertificateChain(
    __inout_ecount( 1 )                          DRM_BCERTFORMAT_BUILDER_CONTEXT         *f_pBuilderCtx,
    __inout_opt                                  OEM_TEE_CONTEXT                         *f_pOemTeeContext,
    __in_opt                               const PRIVKEY_P256                            *f_pIssuerPrivateKey,
    __in                                   const PUBKEY_P256                             *f_pIssuerPublicKey,
    __inout_ecount( 1 )                          DRM_DWORD                               *f_pcbCertificateChain,
    __inout_bcount_opt( *f_pcbCertificateChain ) DRM_BYTE                                *f_pbCertificateChain,
    __inout_opt                                  DRM_BCERTFORMAT_CERT_SIGNATURE_INFO     *f_pSignatureInfo )
{
    DRM_RESULT                                dr            = DRM_SUCCESS;
    DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL *pCtx          = DRM_REINTERPRET_CAST( DRM_BCERTFORMAT_BUILDER_CONTEXT_INTERNAL, f_pBuilderCtx );
    DRM_XB_BUILDER_CONTEXT_INTERNAL          *pXBCtx        = NULL;
    DRM_BCERTFORMAT_SIGNATURE_INFO           *pSig          = NULL;
    DRM_BYTE                                  rgbHeaderStack[128] = {0};
    DRM_XB_BUILDER_CONTEXT                    oHeaderCtx    = {{0}};
    DRM_BYTE                                 *pbBuffer      = f_pbCertificateChain;
    DRM_DWORD                                 cbHeader      = 0;
    DRM_DWORD                                 cbCert        = 0;
    DRM_DWORD                                 cbChain       = 0;
    DRM_DWORD                                 dwResult      = 0;
    DRM_STACK_ALLOCATOR_CONTEXT               oStack       = {0};

    ChkArg( f_pBuilderCtx         != NULL );
    ChkArg( f_pIssuerPublicKey    != NULL );
    ChkArg( f_pcbCertificateChain != NULL );

    if( f_pbCertificateChain != NULL )
    {
        ZEROMEM( f_pbCertificateChain, *f_pcbCertificateChain );
    }

    if( f_pSignatureInfo != NULL )
    {
        ZEROMEM( f_pSignatureInfo, sizeof(*f_pSignatureInfo) );
    }

    pXBCtx   = DRM_REINTERPRET_CAST( DRM_XB_BUILDER_CONTEXT_INTERNAL, &pCtx->oBuilderCtx );
    pSig     = &pCtx->oNewCert.SignatureInformation;
    cbHeader = DRM_MIN_CERT_CHAIN_LEN;
    cbChain  = pCtx->oCertChain.Header.cbChain;

    /* Skip size of chain header and any space before the first cert */
    if( cbChain > DRM_MIN_CERT_CHAIN_LEN )
    {
        if( pCtx->oCertChain.Certificates.cEntries > 0 )
        {
            ChkDR( DRM_DWordSubSame( &cbChain, pCtx->oCertChain.Certificates.pCertHeaders[0].dwOffset ) );
        }
        else
        {
            cbChain -= DRM_MIN_CERT_CHAIN_LEN;
        }
    }

    /*
    ** Add room for the signature
    */
    if( !pSig->fValid )
    {
        ChkDR( DRM_STK_Alloc(
            pXBCtx->pcontextStack,
            ECDSA_P256_SIGNATURE_SIZE_IN_BYTES,
            ( DRM_VOID ** )&pSig->xbbaSignature.pbDataBuffer ) );

        pSig->fValid                     = TRUE;
        pSig->wSignatureType             = DRM_BCERT_SIGNATURE_TYPE_P256;
        pSig->xbbaSignature.fValid       = TRUE;
        pSig->xbbaSignature.iData        = 0;
        pSig->xbbaSignature.cbData       = ECDSA_P256_SIGNATURE_SIZE_IN_BYTES;
        pSig->xbbaIssuerKey.fValid       = TRUE;
        pSig->xbbaIssuerKey.iData        = 0;
        pSig->xbbaIssuerKey.cbData       = sizeof( *f_pIssuerPublicKey );
        pSig->xbbaIssuerKey.pbDataBuffer = ( DRM_BYTE * )f_pIssuerPublicKey;

        ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_SIGNATURE_INFO_ENTRY_TYPE, pSig ) );
    }

    if( !pCtx->oNewCert.FeatureInformation.fValid )
    {
        pCtx->oNewCert.FeatureInformation.fValid = TRUE;
        pCtx->oNewCert.FeatureInformation.dwFeatureSet = 0;
        pCtx->oNewCert.FeatureInformation.dwlFeatures.fValid = TRUE;
        pCtx->oNewCert.FeatureInformation.dwlFeatures.cDWORDs = 0;
        pCtx->oNewCert.FeatureInformation.dwlFeatures.iDwords = 0;
        pCtx->oNewCert.FeatureInformation.dwlFeatures.pdwordBuffer = NULL;

        ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_FEATURE_INFO_ENTRY_TYPE, &pCtx->oNewCert.FeatureInformation ) );
    }

    /*
    ** Add the manufacturing strings
    ** This oclwrs here because we have individual functions for setting each string, however the XBinary
    ** structure contains all 3 strings.
    */
    if( !pCtx->oNewCert.ManufacturerInformation.fValid &&
        (pCtx->oNewCert.ManufacturerInformation.xbbaManufacturerName.fValid ||
         pCtx->oNewCert.ManufacturerInformation.xbbaModelName.fValid ||
         pCtx->oNewCert.ManufacturerInformation.xbbaModelNumber.fValid) )
    {
        pCtx->oNewCert.ManufacturerInformation.fValid  = TRUE;
        ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_MANUFACTURER, &pCtx->oNewCert.ManufacturerInformation ) );
    }


    /*
    ** Add room for the new certificate header data
    */
    if( !pCtx->oNewCert.HeaderData.fValid )
    {
        /* Length and signed length will be filled in after serialization */
        pCtx->oNewCert.HeaderData.dwLength       = 0;
        pCtx->oNewCert.HeaderData.dwSignedLength = 0;
        pCtx->oNewCert.HeaderData.dwVersion      = DRM_BCERTFORMAT_LWRRENT_VERSION;
        pCtx->oNewCert.HeaderData.fValid         = TRUE;

        ChkDR( DRM_XB_SetExtendedHeader( &pCtx->oBuilderCtx, &pCtx->oNewCert.HeaderData ) );
    }

    /*
    ** If we have not yet added a extended data signature and we have a valid HWID record,
    ** add the signature buffer for the extended data.
    */
    if( !pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.fValid &&
         pCtx->oNewCert.ExDataContainer.HwidRecord.fValid )
    {
        ChkDR( DRM_STK_Alloc(
            pXBCtx->pcontextStack,
            ECDSA_P256_SIGNATURE_SIZE_IN_BYTES,
            ( DRM_VOID ** )&pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.xbbaSignature.pbDataBuffer ) );

        pCtx->oNewCert.ExDataContainer.fValid = TRUE;
        pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.fValid               = TRUE;
        pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.wSignatureType       = DRM_BCERT_SIGNATURE_TYPE_P256;
        pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.xbbaSignature.fValid = TRUE;
        pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.xbbaSignature.iData  = 0;
        pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation.xbbaSignature.cbData = ECDSA_P256_SIGNATURE_SIZE_IN_BYTES;

        ChkDR( DRM_XB_AddEntry( &pCtx->oBuilderCtx, DRM_BCERTFORMAT_OBJTYPE_EXTDATASIGNATURE, &pCtx->oNewCert.ExDataContainer.ExDataSignatureInformation ) );
    }

    if( pbBuffer != NULL )
    {
        /*
        ** Callwlate the Digest Value by SHA256 hashing the public key of this certificate.
        ** pbDataBuffer is guaranteed to be 8-byte (struct) aligned since it was created by
        ** DRM_STK_Alloc, so we don't need to worry about alignment issues with the cast to
        ** DRM_SHA256_Digest.
        */
        ChkDR( Oem_Broker_SHA256_CreateDigest(
            ( DRM_VOID * )f_pOemTeeContext,
            XBBA_TO_CB( pCtx->oNewCert.KeyInformation.pHead[0].xbbaKeyValue ),
            XBBA_TO_PB( pCtx->oNewCert.KeyInformation.pHead[0].xbbaKeyValue ),
            ( DRM_SHA256_Digest * )pCtx->oNewCert.BasicInformation.xbbaDigestValue.pbDataBuffer ) );

        /* Skip cert chain header until we have the chain length data */
        ChkDR( DRM_DWordPtrAddSame( ( DRM_DWORD_PTR* )&pbBuffer, cbHeader) );
    }

    if( *f_pcbCertificateChain != 0 )
    {
        cbCert = *f_pcbCertificateChain - cbHeader;
    }

    ChkDR( DRM_XB_FinishFormat( &pCtx->oBuilderCtx, pbBuffer, &cbCert ) );

    if( pbBuffer != NULL )
    {
        DRM_DWORD cbSignData  = cbCert;
        DRM_DWORD ibSignature;
        DRM_DWORD cbSignedDataSize;
        DRM_DWORD cbSigSize;
        DRM_DWORD cbExtDataSize = 0;
        DRM_BYTE *pbSignature = NULL;
        const DRM_BYTE *pbSignData  = pbBuffer;

        ChkDR( _DRM_BCERTFORMAT_CalcSignatureInfoSize( DRM_BCERT_SIGNATURE_TYPE_P256, sizeof(*f_pIssuerPublicKey), &cbSigSize ) );
        ChkDR( _DRM_BCERTFORMAT_CalcExtDataContainerSize( &pCtx->oNewCert.ExDataContainer, &cbExtDataSize ) );

        ChkDR( DRM_DWordAddSame( &cbSigSize, cbExtDataSize ) );

        ChkArg( cbSigSize < cbSignData );
        cbSignData = cbSignData - cbSigSize; /* No underflow check because of ChkArg */
        cbSignedDataSize = cbSignData;
        ChkDR( DRM_DWordAdd(
            cbSignedDataSize,
            DRM_BCERT_OBJECT_HEADER_LEN /* object header  */
                + sizeof(DRM_WORD)      /* signature type */
                + sizeof(DRM_WORD),     /* signature size */
            &ibSignature ) );

        /* Save signed data size in BCERT header */
        DWORD_TO_NETWORKBYTES( pbBuffer, DRM_BCERT_HEADER_SIGNED_DATA_SIZE_OFFSET, cbSignedDataSize );

        ChkDR( DRM_DWordPtrAdd( ( DRM_DWORD_PTR )pbBuffer, ibSignature, ( DRM_DWORD_PTR *)&pbSignature ) );
        if ( f_pIssuerPrivateKey != NULL )
        {
            SIGNATURE_P256 oSignature;

            ChkDR( Oem_Broker_ECDSA_P256_Sign(
                f_pOemTeeContext,
                NULL,
                pbSignData,
                cbSignData,
                f_pIssuerPrivateKey,
                &oSignature ) );

            OEM_SELWRE_MEMCPY( pbSignature, &oSignature, sizeof(oSignature) );
        }

        if( f_pSignatureInfo != NULL )
        {
            f_pSignatureInfo->pbCertficate  = pbSignData;
            f_pSignatureInfo->cbCertificate = cbSignData;
            f_pSignatureInfo->pbSignature   = pbSignature;
            f_pSignatureInfo->cbSignature   = sizeof( SIGNATURE_P256 );
        }

        /* Add extended data signature */
        if( pCtx->oNewCert.ExDataContainer.fValid )
        {
            DRM_DWORD ibExtSignature  = cbCert;
            DRM_DWORD ibExtSignData   = cbCert;
            DRM_BYTE *pbExtSignature  = NULL;
            DRM_BYTE *pbExtSignData   = NULL;

            ChkBOOL( pCtx->pbExtPrivKey != NULL            , DRM_E_BCERT_EXTDATA_PRIVKEY_MUST_PRESENT );
            ChkBOOL( pCtx->oNewCert.ExDataSigKeyInfo.fValid, DRM_E_BCERT_EXTDATA_PRIVKEY_MUST_PRESENT );

            ChkArg( cbExtDataSize > 0 );
            ChkDR( DRM_DWordSubSame( &ibExtSignData, cbExtDataSize ) );
            ChkDR( DRM_DWordSubSame( &ibExtSignature, ECDSA_P256_SIGNATURE_SIZE_IN_BYTES ) );

            /* Skip container header */
            ChkDR( DRM_DWordAddSame( &ibExtSignData, DRM_BCERT_OBJECT_HEADER_LEN ) );
            ChkDR( DRM_DWordSubSame( &cbExtDataSize, DRM_BCERT_OBJECT_HEADER_LEN ) );

            ChkDR( DRM_DWordSubSame(
                &cbExtDataSize,
                DRM_BCERT_OBJECT_HEADER_LEN /* object header  */
                    + sizeof(DRM_WORD)      /* signature type */
                    + sizeof(DRM_WORD) ) ); /* signature size */
            ChkDR( DRM_DWordSubSame( &cbExtDataSize, ECDSA_P256_SIGNATURE_SIZE_IN_BYTES ) );

            ChkDR( DRM_DWordPtrAdd( ( DRM_DWORD_PTR )pbBuffer, ibExtSignData , ( DRM_DWORD_PTR * )&pbExtSignData  ) );
            ChkDR( DRM_DWordPtrAdd( ( DRM_DWORD_PTR )pbBuffer, ibExtSignature, ( DRM_DWORD_PTR * )&pbExtSignature ) );

            {
                PRIVKEY_P256        oPrivKey;
                SIGNATURE_P256      oSignature;

                OEM_SELWRE_MEMCPY( &oPrivKey, pCtx->pbExtPrivKey, sizeof(oPrivKey) );
                ChkDR( Oem_Broker_ECDSA_P256_Sign(
                    f_pOemTeeContext,
                    NULL,
                    pbExtSignData,
                    cbExtDataSize,
                    &oPrivKey,
                    &oSignature ) );

                OEM_SELWRE_MEMCPY( pbExtSignature, &oSignature, sizeof(oSignature) );
                OEM_SELWRE_ZERO_MEMORY( &oPrivKey, sizeof(oPrivKey) );
            }
        }
    }

    ChkDR( DRM_DWordAdd( cbHeader, cbCert, &dwResult ) );
    ChkDR( DRM_DWordAddSame( &dwResult, cbChain ) );

    /*
    ** Copy the parent chain
    */

    if( pbBuffer != NULL )
    {
        if( cbChain > 0 )
        {
            DRM_BYTE *pbParenChain = XBBA_TO_PB( pCtx->oCertChain.xbbaRawData );
            DRM_DWORD ibFirstCert;

            if( pCtx->oCertChain.Certificates.cEntries > 0 )
            {
                ibFirstCert = pCtx->oCertChain.Certificates.pCertHeaders[0].dwOffset;
            }
            else
            {
                ibFirstCert = DRM_MIN_CERT_CHAIN_LEN;
            }

            ChkBOOL( dwResult <= *f_pcbCertificateChain, DRM_E_BUFFERTOOSMALL );
            ChkDR( DRM_DWordPtrAddSame( ( DRM_DWORD_PTR* )&pbParenChain, ibFirstCert ) );
            ChkDR( DRM_DWordPtrAddSame( ( DRM_DWORD_PTR* )&pbBuffer    , cbCert      ) );
            MEMCPY( pbBuffer, pbParenChain, cbChain );
        }

        /* Serialize the chain header */
        /* Set header size to include standard header and outer container object (even though it is not serialize) */
        cbHeader = DRM_MIN_CERT_CHAIN_LEN + XB_BASE_OBJECT_LENGTH + XB_HEADER_LENGTH( sizeof(DRM_DWORD) );
        ChkBOOL( cbHeader <= *f_pcbCertificateChain, DRM_E_BUFFERTOOSMALL );
        ChkDR( DRM_STK_Init( &oStack, rgbHeaderStack, sizeof(rgbHeaderStack), TRUE ) );
        ChkDR( DRM_XB_StartFormatFromStack( &oStack, DRM_BCERTFORMAT_LWRRENT_VERSION, &oHeaderCtx, s_DRM_BCERTFORMAT_CHAIN_FormatDescription ) );
        pCtx->oCertChain.Header.fValid = TRUE;
        pCtx->oCertChain.Header.cbChain = dwResult;
        pCtx->oCertChain.Header.cCerts++;
        pCtx->oCertChain.Header.dwVersion = DRM_BCERTFORMAT_LWRRENT_VERSION;
        ChkDR( DRM_XB_SetExtendedHeader( &oHeaderCtx, &pCtx->oCertChain.Header ) );
        ChkDR( DRM_XB_FinishFormat( &oHeaderCtx, f_pbCertificateChain, &cbHeader ) );
        DRMASSERT( cbHeader == DRM_MIN_CERT_CHAIN_LEN );

        /* Save data size in BCERT header (XBinary serialization sets it to the size of the chain header only) */
        DWORD_TO_NETWORKBYTES( f_pbCertificateChain, DRM_BCERT_HEADER_DATA_SIZE_OFFSET, dwResult );
    }

    *f_pcbCertificateChain = dwResult;

ErrorExit:
    /* Set the required buffer size if the function failed with DRM_E_BUFFERTOOSMALL */
    if( dr == DRM_E_BUFFERTOOSMALL )
    {
        DRM_RESULT drTmp = DRM_DWordAdd( cbHeader, cbCert, &dwResult );
        if( DRM_SUCCEEDED( drTmp ) )
        {
            drTmp = DRM_DWordAddSame( &dwResult, cbChain );

            if( DRM_SUCCEEDED( drTmp ) )
            {
                *f_pcbCertificateChain = dwResult;
            }
        }
        if( DRM_FAILED( drTmp ) )
        {
            dr = drTmp;
        }
    }

    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

