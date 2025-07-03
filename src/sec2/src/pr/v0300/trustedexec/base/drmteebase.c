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
#include <drmteedebug.h>
#include <drmerr.h>
#include <drmmathsafe.h>
#include <drmteeproxystubcommon.h>
#include <drmprndformats_generated.h>
#include <drmteeproxystubcommon.h>
#include <drmteeproxy_generated.h>
#include <drmversionconstants.h>
#include <drmteekbcryptodata.h>
#include <drmxbparser.h>
#include <oemtee.h>
#include <oemaescommon.h>
#include <oemcommon.h>
#include <oembyteorder.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/* These all have to be different. They also have to be different from s_idUniqueIDLabel in oemtee.c */
static const DRM_ID g_idKeyDerivationSignPPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignPPKB)     = {{ 0x42, 0x8c, 0x44, 0x0f, 0x03, 0x81, 0x47, 0x5b, 0xb6, 0xbf, 0x1b, 0xac, 0x8a, 0x55, 0xaa, 0x20 }};
static const DRM_ID g_idKeyDerivationSignLKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignLKB)      = {{ 0x42, 0x63, 0x24, 0x28, 0xd5, 0x6f, 0x4d, 0xb0, 0x9c, 0x53, 0x7f, 0xe4, 0x63, 0x69, 0xa7, 0x5b }};
static const DRM_ID g_idKeyDerivationSignCDKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignCDKB)     = {{ 0x35, 0xde, 0xab, 0x19, 0x94, 0x77, 0x42, 0x88, 0x8b, 0x98, 0x52, 0x5a, 0x15, 0xa5, 0xe2, 0x52 }};
static const DRM_ID g_idKeyDerivationSignDKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignDKB)      = {{ 0xbd, 0x22, 0xf4, 0xc0, 0xc5, 0xdc, 0x49, 0x63, 0xae, 0xca, 0xf6, 0x10, 0x5f, 0x86, 0xdd, 0xa6 }};
static const DRM_ID g_idKeyDerivationSignMRKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignMRKB)     = {{ 0x31, 0x97, 0x7c, 0x63, 0x4e, 0xad, 0x4a, 0xc8, 0x9e, 0x6a, 0x26, 0xdc, 0x1a, 0xa7, 0x92, 0x99 }};
static const DRM_ID g_idKeyDerivationSignMTKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignMTKB)     = {{ 0x47, 0x1e, 0x96, 0xb4, 0xc5, 0x77, 0x4c, 0x9b, 0x9f, 0xf4, 0xc0, 0x6e, 0x9c, 0x96, 0xb1, 0x5f }};
static const DRM_ID g_idKeyDerivationSignRKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignRKB)      = {{ 0xf6, 0x02, 0xf1, 0x9b, 0x89, 0xb5, 0x40, 0x49, 0xbd, 0x4e, 0x48, 0x82, 0xe1, 0x5a, 0xd5, 0x90 }};
static const DRM_ID g_idKeyDerivationSignCEKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignCEKB)     = {{ 0x95, 0x94, 0xf8, 0x89, 0x58, 0x75, 0x47, 0xd7, 0x9a, 0x1d, 0x51, 0x07, 0x69, 0x5d, 0x28, 0x33 }};
static const DRM_ID g_idKeyDerivationSignTPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignTPKB)     = {{ 0x2c, 0x1d, 0x86, 0x25, 0x5e, 0xe4, 0x00, 0xd6, 0x46, 0x3e, 0x95, 0x3c, 0xdd, 0x5a, 0x27, 0x5c }};
static const DRM_ID g_idKeyDerivationSignSPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignSPKB)     = {{ 0x8F, 0xF2, 0x2A, 0x57, 0xA5, 0x07, 0x4F, 0xC6, 0x92, 0x79, 0xB7, 0xB6, 0x6F, 0xE7, 0x00, 0x59 }};
static const DRM_ID g_idKeyDerivationSignNKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignNKB)      = {{ 0x52 ,0xb9 ,0x32 ,0xb9 ,0x86 ,0x2b ,0x1b ,0xa5 ,0xaa ,0x81 ,0x52 ,0xd1 ,0x5b ,0xea ,0x44 ,0x1b }};
static const DRM_ID g_idKeyDerivationSignRPCKB     PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignRPCKB)    = {{ 0x8a, 0x86, 0x12, 0xeb, 0xee, 0xca, 0x4a, 0x6f, 0xbe, 0x00, 0x1b, 0x98, 0xd5, 0x40, 0xf3, 0x80 }};
static const DRM_ID g_idKeyDerivationSignLPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignLPKB)     = {{ 0xa8, 0xef, 0x69, 0x5c, 0x66, 0x07, 0x41, 0x96, 0xab, 0x99, 0xc8, 0xde, 0xcc, 0xc0, 0xf1, 0xa3 }};

static const DRM_ID g_idKeyDerivationEncryptPPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptPPKB)  = {{ 0x06, 0xa2, 0x96, 0x61, 0x03, 0x14, 0x4f, 0x63, 0x8c, 0x47, 0xa3, 0xb6, 0xdc, 0x23, 0x14, 0xa8 }};
static const DRM_ID g_idKeyDerivationEncryptLKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptLKB)   = {{ 0x6a, 0x37, 0x83, 0x82, 0x62, 0x74, 0x46, 0x22, 0xad, 0xd0, 0xf3, 0x21, 0x37, 0x1a, 0x8c, 0xb7 }};
static const DRM_ID g_idKeyDerivationEncryptCDKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptCDKB)  = {{ 0xd1, 0x67, 0xaa, 0xeb, 0x3b, 0x70, 0x42, 0xe5, 0x8b, 0x2b, 0x06, 0xa6, 0xfe, 0xbb, 0x96, 0x72 }};
static const DRM_ID g_idKeyDerivationEncryptDKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptDKB)   = {{ 0xf0, 0x7b, 0x66, 0x60, 0xd5, 0xcb, 0x4c, 0x4f, 0x8d, 0xb7, 0x19, 0xa5, 0x2e, 0x3d, 0x1d, 0xac }};
static const DRM_ID g_idKeyDerivationEncryptMRKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptMRKB)  = {{ 0x37, 0xbd, 0xbb, 0xd5, 0x6c, 0x49, 0x46, 0x1e, 0x99, 0x71, 0x69, 0x7f, 0x48, 0x2f, 0x29, 0x89 }};
static const DRM_ID g_idKeyDerivationEncryptMTKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptMTKB)  = {{ 0xf5, 0xbd, 0x78, 0xd9, 0x35, 0x53, 0x4d, 0xb7, 0x85, 0x55, 0xf0, 0x9f, 0xe8, 0x7a, 0x45, 0x61 }};
static const DRM_ID g_idKeyDerivationEncryptRKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptRKB)   = {{ 0xa6, 0xab, 0xa5, 0x65, 0xcf, 0xbf, 0x4d, 0x8b, 0x80, 0x60, 0xb5, 0x49, 0x7e, 0x0a, 0x8b, 0x15 }};
static const DRM_ID g_idKeyDerivationEncryptCEKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptCEKB)  = {{ 0xed, 0x4d, 0xe9, 0x91, 0x89, 0xee, 0x45, 0x8d, 0x80, 0x8a, 0x61, 0xb8, 0x92, 0x68, 0x3c, 0x18 }};
static const DRM_ID g_idKeyDerivationEncryptTPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptTPKB)  = {{ 0x66, 0x79, 0xee, 0x23, 0x10, 0xff, 0xe5, 0x92, 0x33, 0xda, 0x6e, 0x26, 0x1b, 0x41, 0x97, 0x37 }};
static const DRM_ID g_idKeyDerivationEncryptSPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptSPKB)  = {{ 0x59, 0xC8, 0xED, 0xE9, 0x9D, 0xB1, 0x48, 0xA5, 0xBF, 0x40, 0x07, 0xC0, 0x50, 0x3D, 0xCA, 0x43 }};
static const DRM_ID g_idKeyDerivationEncryptNKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptNKB)   = {{ 0xdf ,0x1f ,0xd4 ,0xa6 ,0x79 ,0x06 ,0x51 ,0x32 ,0xe7 ,0x09 ,0x3e ,0x46 ,0x5c ,0x52 ,0x8f ,0x6e }};
static const DRM_ID g_idKeyDerivationEncryptRPCKB  PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptRPCKB) = {{ 0x75, 0x2b, 0x50, 0x42, 0x03, 0x6c, 0x48, 0x4d, 0x91, 0x36, 0xab, 0x72, 0xbd, 0x97, 0xed, 0xb9 }};
static const DRM_ID g_idKeyDerivationEncryptLPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptLPKB)  = {{ 0x36, 0x06, 0x5b, 0x44, 0x13, 0xb9, 0x4b, 0xd0, 0x96, 0x52, 0x5e, 0x59, 0xeb, 0xa5, 0x36, 0x3a }};

#ifdef NONE
const DRM_DISCARDABLE PUBKEY_P256   g_ECC256MSPlayReadyRootIssuerPubKeyTEE = DRM_ECC256_MS_PLAYREADY_ROOT_ISSUER_PUBKEY;
#endif
#if defined (SEC_COMPILE)
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ParseApplicationInfo(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __in_opt              const DRM_TEE_BYTE_BLOB            *f_pApplicationInfo,
    __inout                     DRM_ID                       *f_pidApplication )
{
    DRM_RESULT                   dr                                 = DRM_SUCCESS;
    DRM_BYTE                     rgbUnused[sizeof(DRM_DWORD_PTR)];         /* memory for stack */
    DRM_STACK_ALLOCATOR_CONTEXT  stack;                                    /* Initialized by DRM_STK_Init */
    DRM_DWORD                    dwVersionFound                     = 0;
    XB_DRM_TEE_PROXY_APPINFO     xbApplicationInfo                  = { 0 };

    /* Note: *f_pidApplication is already be initialized to all zeroes by the caller, and we leave it that way if no application info was passed. */
    if( f_pApplicationInfo != NULL && IsBlobAssigned( f_pApplicationInfo ) )
    {
        ChkDR( DRM_STK_Init( &stack, (DRM_BYTE*)rgbUnused, sizeof(rgbUnused), TRUE ) );

        ChkDR( DRM_XB_UnpackBinary(
            f_pApplicationInfo->pb,
            f_pApplicationInfo->cb,
            &stack,
            s_XB_DRM_TEE_PROXY_APPINFO_FormatDescription,
            DRM_NO_OF( s_XB_DRM_TEE_PROXY_APPINFO_FormatDescription ),
            &dwVersionFound,
            NULL,
            (DRM_VOID *)&xbApplicationInfo ) );
        ChkBOOL( xbApplicationInfo.fValid, DRM_E_XB_ILWALID_OBJECT );
        ChkBOOL( xbApplicationInfo.BaseInfo.fValid, DRM_E_XB_ILWALID_OBJECT );
        ChkBOOL( dwVersionFound == DRM_TEE_PROXY_LWRRENT_VERSION, DRM_E_XB_ILWALID_OBJECT );
        *f_pidApplication = xbApplicationInfo.BaseInfo.idApplication;
    }

ErrorExit:
    return dr;
}

#ifdef NONE
static DRM_BOOL DRM_CALL _SupportedInBaseLibrary( DRM_VOID )
{
    return TRUE;
}
#endif

typedef struct __tagTEEFunctionSupported
{
    DRM_METHOD_ID_DRM_TEE   eMethodID;
    DRM_BOOL                fSupported;
} TEEFunctionSupported;

// LWE (nkuo): function pointers are removed to help the static analysis
static const TEEFunctionSupported s_rgTEEFunctionSupportedList[] PR_ATTR_DATA_OVLY(_s_rgTEEFunctionSupportedList) =
{
    { DRM_METHOD_ID_DRM_TEE_BASE_AllocTEEContext,                               TRUE  },
    { DRM_METHOD_ID_DRM_TEE_BASE_FreeTEEContext,                                TRUE  },
    { DRM_METHOD_ID_DRM_TEE_BASE_SignDataWithSelwreStoreKey,                    TRUE  },
    { DRM_METHOD_ID_DRM_TEE_BASE_CheckDeviceKeys,                               TRUE  },
    { DRM_METHOD_ID_DRM_TEE_BASE_GetDebugInformation,                           TRUE  },
    { DRM_METHOD_ID_DRM_TEE_BASE_GenerateNonce,                                 TRUE  },
    { DRM_METHOD_ID_DRM_TEE_BASE_GetSystemTime,                                 TRUE  },
    { DRM_METHOD_ID_DRM_TEE_LPROV_GenerateDeviceKeys,                           TRUE  },
    { DRM_METHOD_ID_DRM_TEE_RPROV_GenerateBootstrapChallenge,                   FALSE },
    { DRM_METHOD_ID_DRM_TEE_RPROV_ProcessBootstrapResponse,                     FALSE },
    { DRM_METHOD_ID_DRM_TEE_RPROV_GenerateProvisioningRequest,                  FALSE },
    { DRM_METHOD_ID_DRM_TEE_RPROV_ProcessProvisioningResponse,                  FALSE },
    { DRM_METHOD_ID_DRM_TEE_LICPREP_PackageKey,                                 TRUE  },
    { DRM_METHOD_ID_DRM_TEE_SAMPLEPROT_PrepareSampleProtectionKey,              TRUE  },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_PreparePolicyInfo,                        TRUE  },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_PrepareToDecrypt,                         TRUE  },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_CreateOEMBlobFromCDKB,                    TRUE  },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContent,                           TRUE  },
    { DRM_METHOD_ID_DRM_TEE_SIGN_SignHash,                                      TRUE  },
    { DRM_METHOD_ID_DRM_TEE_DOM_PackageKeys,                                    TRUE  },
    { DRM_METHOD_ID_DRM_TEE_PRNDRX_ProcessRegistrationResponseMessage,          FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDRX_GenerateProximityResponseNonce,              FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDRX_CompleteLicenseRequestMessage,               FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDRX_ProcessLicenseTransmitMessage,               FALSE },
    { DRM_METHOD_ID_DRM_TEE_REVOCATION_IngestRevocationInfo,                    TRUE  },
    { DRM_METHOD_ID_DRM_TEE_LICGEN_CompleteLicense,                             FALSE },
    { DRM_METHOD_ID_DRM_TEE_LICGEN_AES128CTR_EncryptContent,                    FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDTX_ProcessRegistrationRequestMessage,           FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDTX_CompleteRegistrationResponseMessage,         FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDTX_GenerateProximityDetectionChallengeNonce,    FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDTX_VerifyProximityDetectionResponseNonce,       FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDTX_ProcessLicenseRequestMessage,                FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDTX_CompleteLicenseTransmitMessage,              FALSE },
    { DRM_METHOD_ID_DRM_TEE_PRNDTX_RebindLicenseToReceiver,                     FALSE },
    { DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData,                       TRUE  },
    { DRM_METHOD_ID_DRM_TEE_SELWRESTOP_GetGenerationID,                         FALSE },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptAudioContentMultiple,              TRUE  },
};

static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ValidateAndInitializeSystemInfo(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __inout                     OEM_TEE_CONTEXT              *f_pOemTeeCtx,
    __in                  const DRM_ID                       *f_pidApplication,
    __out_opt                   DRM_TEE_BYTE_BLOB            *f_pSystemInfo )
{
    DRM_RESULT                   dr                         = DRM_SUCCESS;
    DRM_RESULT                   drTmp                      = DRM_SUCCESS;
    DRM_DWORD                    cbSystemInfo               = 0;
    DRM_TEE_BYTE_BLOB            oSystemInfo                = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD                   *pdwFunctionMap             = NULL;
    DRM_DWORD                    idx                        = 0;
    DRM_DWORD                    cbSystemProperties         = 0;
    DRM_BYTE                    *pbSystemProperties         = NULL;
    DRM_DWORD                    cchOEMManufacturerName     = 0;
    DRM_WCHAR                   *pwszOEMManufacturerName    = NULL;
    DRM_DWORD                    cchOEMModelName            = 0;
    DRM_WCHAR                   *pwszOEMModelName           = NULL;
    DRM_DWORD                    cchOEMModelNumber          = 0;
    DRM_WCHAR                   *pwszOEMModelNumber         = NULL;
    DRM_DWORD                    cbStack                    = 0;
    DRM_BYTE                    *pbStack                    = NULL;

    ChkDR( DRM_TEE_BASE_MemAlloc( f_pContext, DRM_TEE_METHOD_FUNCTION_MAP_COUNT * sizeof( DRM_DWORD ), (DRM_VOID**)&pdwFunctionMap ) );

    /* Even if we aren't returning the system info, we still validate that certain OEM-defined values are valid. */
    ChkDR( OEM_TEE_BASE_GetVersionInformation(
        f_pOemTeeCtx,
        &cchOEMManufacturerName,
        &pwszOEMManufacturerName,
        &cchOEMModelName,
        &pwszOEMModelName,
        &cchOEMModelNumber,
        &pwszOEMModelNumber,
        pdwFunctionMap,
        &cbSystemProperties,
        &pbSystemProperties ) );

    /* Only the odd values in the function map are OEM-defined values */
    for( idx = 1; idx < DRM_TEE_METHOD_FUNCTION_MAP_COUNT; idx += 2 )
    {
        /*
        ** Verify that all OEM-returned values for the Function Map correctly have our reserved bits set to 0.
        ** except for any reserved bits they are allowed to set.
        ** If we define values for these bits, a firmware update will be required in order for the OEM to set them,
        ** and this mask can/must be updated as part of the same update.
        */
        DRM_DWORD idx2;     /* loop variable */
        AssertChkArg( ( pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BITS_OEM_ALLOWED ) == pdwFunctionMap[ idx ] );

        /*
        ** PreProcessEncryptedData contains two things.
        ** 1. Slice header parsing.
        ** 2. Additional OEM parsing on the full frame, e.g. transcryption.
        ** If additional OEM parsing is supported, this API MUST use structured serialization.
        ** We don't support non-structured serialization when the full frame is passed.
        */
        /* Can't underflow because we are looping through positive odd values */
        if( ( pdwFunctionMap[ idx - 1 ] == DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData )
         && DRM_TEE_PROPERTY_IS_SET( cbSystemProperties, pbSystemProperties, DRM_TEE_PROPERTY_REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES ) )
        {
            /* We fail if OEM_TEE_BASE_GetVersionInformation did not tell us to use structured serialization for full frames. */
            AssertChkArg( ( pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS ) == DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS );
        }

        /*
        ** Automatically mark as unsupported any API which has the stub version linked in.
        */
        DRMCASSERT( DRM_NO_OF( s_rgTEEFunctionSupportedList ) == DRM_METHOD_ID_DRM_TEE_Count );
        for( idx2 = 0; idx2 < DRM_NO_OF( s_rgTEEFunctionSupportedList ); idx2++ )
        {
            /* Can't underflow because we are looping through positive odd values */
            if( pdwFunctionMap[ idx - 1 ] == (DRM_DWORD)s_rgTEEFunctionSupportedList[ idx2 ].eMethodID )
            {
                /* LWE (nkuo): avoid using the function pointers by using the static values */
                if( !s_rgTEEFunctionSupportedList[ idx2 ].fSupported )
                {
                    pdwFunctionMap[ idx ] |= DRM_TEE_PROXY_BIT_API_UNSUPPORTED;
                }
                break;
            }
        }
        AssertChkArg( idx2 < DRM_NO_OF( s_rgTEEFunctionSupportedList ) );
    }

    if( DRM_TEE_PROPERTY_IS_SET( cbSystemProperties, pbSystemProperties, DRM_TEE_PROPERTY_REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES ) )
    {
        AssertChkArg( DRM_TEE_PROPERTY_IS_SET( cbSystemProperties, pbSystemProperties, DRM_TEE_PROPERTY_SUPPORTS_PRE_PROCESS_ENCRYPTED_DATA ) );
    }

    if( f_pSystemInfo != NULL )
    {
        DRM_XB_BUILDER_CONTEXT    xbContextSystemInfo = {{ 0 }};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
        XB_DRM_TEE_PROXY_SYSINFO  xbSystemInfo        = { 0 };

        cbStack = DRM_TEE_XB_PROXY_SYSINFO_STACK_SIZE;
        ChkDR( DRM_TEE_BASE_MemAlloc( f_pContext, cbStack, (DRM_VOID**)&pbStack ) );

        ChkDR( DRM_XB_StartFormat( pbStack, cbStack, DRM_TEE_PROXY_LWRRENT_VERSION, &xbContextSystemInfo, s_XB_DRM_TEE_PROXY_SYSINFO_FormatDescription ) );

        xbSystemInfo.fValid = TRUE;

        /* Note: The application ID will be all zeroes if no application info was passed, but that's OK for a unique ID being created here. */
        xbSystemInfo.BaseInfo.fValid = TRUE;
        ChkDR( DRM_TEE_BASE_IMPL_GetAppSpecificHWID( f_pOemTeeCtx, f_pidApplication, &xbSystemInfo.BaseInfo.idUnique ) );

        ChkDR( DRM_TEE_BASE_IMPL_GetPKVersion(
            &xbSystemInfo.BaseInfo.dwPKMajorVersion,
            &xbSystemInfo.BaseInfo.dwPKMinorVersion,
            &xbSystemInfo.BaseInfo.dwPKBuildVersion,
            &xbSystemInfo.BaseInfo.dwPKQFEVersion ) );

        xbSystemInfo.OEMInfo.fValid = TRUE;

        ChkDR( OEM_TEE_BASE_GetVersion(
            f_pOemTeeCtx,
            &xbSystemInfo.OEMInfo.dwVersion ) );

        AssertChkArg( cchOEMManufacturerName > 0 && pwszOEMManufacturerName != NULL );
        AssertChkArg( cchOEMModelName > 0 && pwszOEMModelName != NULL );
        AssertChkArg( cchOEMModelNumber > 0 && pwszOEMModelNumber != NULL );
        AssertChkArg( cbSystemProperties > 0 && pbSystemProperties != NULL );

        xbSystemInfo.OEMInfo.dwlFunctionMap.fValid = TRUE;
        xbSystemInfo.OEMInfo.dwlFunctionMap.iDwords = 0;
        xbSystemInfo.OEMInfo.dwlFunctionMap.cDWORDs = DRM_TEE_METHOD_FUNCTION_MAP_COUNT;
        xbSystemInfo.OEMInfo.dwlFunctionMap.pdwordBuffer = (DRM_BYTE*)pdwFunctionMap;

        xbSystemInfo.OEMInfo.xbbaManufacturerName.fValid = TRUE;
        xbSystemInfo.OEMInfo.xbbaManufacturerName.iData = 0;
        xbSystemInfo.OEMInfo.xbbaManufacturerName.cbData = cchOEMManufacturerName * sizeof( DRM_WCHAR );
        xbSystemInfo.OEMInfo.xbbaManufacturerName.pbDataBuffer = (DRM_BYTE*)pwszOEMManufacturerName;

        xbSystemInfo.OEMInfo.xbbaModelName.fValid = TRUE;
        xbSystemInfo.OEMInfo.xbbaModelName.iData = 0;
        xbSystemInfo.OEMInfo.xbbaModelName.cbData = cchOEMModelName * sizeof( DRM_WCHAR );
        xbSystemInfo.OEMInfo.xbbaModelName.pbDataBuffer = (DRM_BYTE*)pwszOEMModelName;

        xbSystemInfo.OEMInfo.xbbaModelNumber.fValid = TRUE;
        xbSystemInfo.OEMInfo.xbbaModelNumber.iData = 0;
        xbSystemInfo.OEMInfo.xbbaModelNumber.cbData = cchOEMModelNumber * sizeof( DRM_WCHAR );
        xbSystemInfo.OEMInfo.xbbaModelNumber.pbDataBuffer = (DRM_BYTE*)pwszOEMModelNumber;

        xbSystemInfo.OEMInfo.xbbaSystemProperties.fValid = TRUE;
        xbSystemInfo.OEMInfo.xbbaSystemProperties.iData = 0;
        xbSystemInfo.OEMInfo.xbbaSystemProperties.cbData = cbSystemProperties;
        xbSystemInfo.OEMInfo.xbbaSystemProperties.pbDataBuffer = pbSystemProperties;

        ChkDR( DRM_XB_AddEntry( &xbContextSystemInfo, XB_OBJECT_GLOBAL_HEADER, &xbSystemInfo ) );

        dr = DRM_XB_FinishFormat( &xbContextSystemInfo, NULL, &cbSystemInfo );
        DRM_REQUIRE_BUFFER_TOO_SMALL( dr );
        ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW, cbSystemInfo, NULL, &oSystemInfo ) );
        ChkDR( DRM_XB_FinishFormat( &xbContextSystemInfo, (DRM_BYTE*)oSystemInfo.pb, &oSystemInfo.cb ) );

        ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pSystemInfo, &oSystemInfo ) );
    }

ErrorExit:
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID **)&pbStack ) );
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID **)&pbSystemProperties ) );
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID **)&pdwFunctionMap ) );
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszOEMManufacturerName ) );
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszOEMModelName ) );
    ChkVOID( DRM_TEE_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszOEMModelNumber ) );

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_BASE_FreeBlob(f_pContext, &oSystemInfo);
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    return dr;
}
#endif

#if defined(SEC_COMPILE)
/*
** Synopsis:
**
** This function allocates and initializes a DRM_TEE_CONTEXT
** and returns TEE system properties.
**
** Operations Performed:
**
**  1. If *f_ppContext is NULL, allocate and initialize a new DRM_TEE_CONTEXT.
**  2. Allocate and return TEE system properties.
**
** Arguments:
**
** f_ppContext:         (in/out) On input, this may be an existing DRM_TEE_CONTEXT
**                               in which case this function should perform no operation
**                               on this parameter.
**                               Otherwise, on output this will be a newly allocated
**                               DRM_TEE_CONTEXT.
**                               This will be freed by DRM_TEE_BASE_FreeTEEContext.
** f_pApplicationInfo:  (in)     Application information from outside the TEE.
** f_pSystemInfo:       (out)    TEE system info from OEM_TEE_BASE_GetVersionInformation
**                               plus additional information about this TEE.
**                               The calling code in inselwre-world will colwert this buffer
**                               back from a byte array into several members of DRM_TEE_PROXY_CONTEXT.
**                               This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_AllocTEEContext(
    __inout                     DRM_TEE_CONTEXT             **f_ppContext,
    __in_opt                    DRM_TEE_BYTE_BLOB            *f_pApplicationInfo,
    __out_opt                   DRM_TEE_BYTE_BLOB            *f_pSystemInfo )
{
    DRM_RESULT           dr             = DRM_SUCCESS;
    DRM_RESULT           drTmp          = DRM_SUCCESS;
    DRM_TEE_CONTEXT     *pContext       = NULL;
    OEM_TEE_CONTEXT     *pOemTeeCtx     = NULL;
    DRM_TEE_BYTE_BLOB    oSystemInfo    = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_ID               idApplication  = {{ 0 }};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_ppContext   != NULL );

#if DRM_DBG
    /* Endianness enforcement. */
    {
        DRM_DWORD dwEndiannessTest = 0xccaaffee;
#if TARGET_LITTLE_ENDIAN
        static const DRM_BYTE rgbTest[] = {0xee,0xff,0xaa,0xcc,};
#else
        static const DRM_BYTE rgbTest[] = {0xcc,0xaa,0xff,0xee,};
#endif
        /*
        ** an Endianness test - in case platform Endianness mismatches the one defined.
        ** This can save some debugging time.
        */
        DRMASSERT( OEM_SELWRE_ARE_EQUAL(&dwEndiannessTest, rgbTest, sizeof(dwEndiannessTest)) );
        ChkBOOL( OEM_SELWRE_ARE_EQUAL(&dwEndiannessTest, rgbTest, sizeof(dwEndiannessTest)), DRM_E_LOGICERR );
    }
#endif

    /* Don't allocate if it has already been allocated by a previous call */
    if( *f_ppContext == NULL )
    {
        OEM_TEE_CONTEXT oTmpTEECtx = OEM_TEE_CONTEXT_EMPTY;

#if DRM_DBG
        ChkDR( DRM_TEE_BASE_IMPL_DEBUG_Init() );
#else
        ChkVOID( DRM_TEE_BASE_IMPL_DEBUG_Init() );
#endif
        ChkDR( OEM_TEE_BASE_AllocTEEContext( &oTmpTEECtx ) );

        dr = OEM_TEE_BASE_SelwreMemAlloc( &oTmpTEECtx, sizeof(DRM_TEE_CONTEXT), (DRM_VOID**)&pContext );
        if( DRM_FAILED( dr ) )
        {
            /* Prevent the temporary OEM_TEE_CONTEXT memory from being leaked */
            ChkVOID( OEM_TEE_BASE_FreeTEEContext( &oTmpTEECtx ) );
            ChkDR( dr );
        }
        pOemTeeCtx = &pContext->oContext;

        /* Transfer ownership of OEM_TEE_CONTEXT members */
        pOemTeeCtx->cbUnprotectedOEMData = oTmpTEECtx.cbUnprotectedOEMData;
        pOemTeeCtx->pbUnprotectedOEMData = oTmpTEECtx.pbUnprotectedOEMData;

        ChkDR( OEM_TEE_BASE_GenerateRandomBytes( pOemTeeCtx, sizeof(pContext->idSession), (DRM_BYTE*)&pContext->idSession ) );

        *f_ppContext = pContext;
        pContext = NULL;
    }
    else
    {
        pOemTeeCtx = &(*f_ppContext)->oContext;
    }

    ChkDR( _ParseApplicationInfo( *f_ppContext, f_pApplicationInfo, &idApplication ) );

    ChkDR( _ValidateAndInitializeSystemInfo( *f_ppContext, pOemTeeCtx, &idApplication, f_pSystemInfo == NULL ? NULL : &oSystemInfo ) );

    if( f_pSystemInfo != NULL )
    {
        ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pSystemInfo, &oSystemInfo ) );
    }

ErrorExit:
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_BASE_FreeBlob(*f_ppContext, &oSystemInfo);
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }
    ChkVOID( DRM_TEE_BASE_FreeTEEContext( &pContext ) );
    return dr;
}

/*
** Synopsis:
**
** This function uninitializes and deallocates a DRM_TEE_CONTEXT.
**
** Operations Performed:
**
**  1. Check if *f_ppContext is NULL.  If so, return.
**  2. Remove the context from the cache of valid sessions.
**  3. Uninitialize and deallocate the DRM_TEE_CONTEXT.
**  4. Set *f_ppContext = NULL.
**
** Arguments:
**
** f_ppContext:          (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
**                                On output, *f_ppContext = NULL.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_FreeTEEContext(
    __inout               DRM_TEE_CONTEXT             **f_ppContext )
{
    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT(  f_ppContext != NULL );

    if( *f_ppContext != NULL )
    {
        ChkVOID( DRM_TEE_BASE_IMPL_DEBUG_DeInit() );

        ChkVOID( OEM_TEE_BASE_FreeTEEContext( &(*f_ppContext)->oContext ) );

        /* We MUST pass NULL here because we've destroyed the OEM_TEE_CONTEXT (previous call) and it's no longer valid */
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( NULL, (DRM_VOID**)f_ppContext ) );

        *f_ppContext = NULL;
    }

    DRMASSERT( f_ppContext == NULL || *f_ppContext == NULL );
}

/*
** Synopsis:
**
** The secure-world version of this function allocates (or weak-references)
** memory in secure-world space.
** The inselwre-world version of this function allocates (or weak-references)
** memory in inselwre-world space.
** This function should never cross the secure/inselwre boundary.
**
** Operations Performed:
**
**  1. Allocate the given number of bytes.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_cb:            (in)     The number of bytes to allocate.
** f_ppv:           (out)    A pointer to the memory allocated
**                           from this call.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_MemAlloc( 
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_DWORD                     f_cb,
    __deref_out_bcount(f_cb)    DRM_VOID                    **f_ppv )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

    ChkArg( f_ppv != NULL );
    ChkArg( f_cb   > 0    );

    if( f_pContext != NULL )
    {
        pOemTeeCtx = &f_pContext->oContext;
    }

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, f_cb, f_ppv ) );

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function frees the given memory.
** This function should never cross the secure/inselwre boundary.
** The proxy for the normal world side will implement the corresponding
** non-secure memory allocation method.
**
** Operations Performed:
**
**  1. Free the given memory.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_ppv:           (in/out) The memory to free.
**
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID   DRM_CALL DRM_TEE_BASE_MemFree(  
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __inout                     DRM_VOID                    **f_ppv )
{
    DRMASSERT( f_ppv != NULL );

    if( (f_ppv != NULL) )
    {
        OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

        if( f_pContext != NULL )
        {
            pOemTeeCtx = &f_pContext->oContext;
        }

        ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, f_ppv ) );
    }
}

/*
** Synopsis:
**
** The secure-world version of this function allocates (or weak-references)
** memory in secure-world space.
** The inselwre-world version of this function allocates (or weak-references)
** memory in inselwre-world space.
** This function should never cross the secure/inselwre boundary.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Allocate a blob using the given behavior.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_eBehavior:     (in)     The style of allocation to use.
** f_cb:            (in)     The number of bytes to allocate
**                           and/or the number of bytes in f_pb.
** f_pb:            (in)     Pointer to bytes to copy or take
**                           a weak reference.  Must be NULL
**                           if f_eBehavior == DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW.
**                           Must be non-NULL otherwise.
** f_pBlob:         (in/out) On output, eType is set to
**                           DRM_TEE_BYTE_BLOB_TYPE_USER_MODE or
**                           DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF
**                           (depending on f_eBehavior),
**                           dwSubType is set to 0, and pb is set
**                           to memory allocated in user mode or
**                           a direct pointer to f_pb.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_AllocBlob( 
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                        DRM_TEE_BLOB_ALLOC_BEHAVIOR   f_eBehavior,
    __in                        DRM_DWORD                     f_cb,
    __in_bcount_opt(f_cb) const DRM_BYTE                     *f_pb,
    __inout                     DRM_TEE_BYTE_BLOB            *f_pBlob )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

    /*
    ** This function should NEVER cross the secure boundary. The proxy in the normal world will
    ** will duplicate this method with normal world allocations.
    */
    ChkArg( f_pBlob    != NULL );

    ChkArg( IsBlobEmpty( f_pBlob ) );

    if( f_pContext != NULL )
    {
        pOemTeeCtx = &f_pContext->oContext;
    }

    switch( f_eBehavior )
    {
    case DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW:
        ChkArg( f_cb  > 0 );
        ChkArg( f_pb == NULL );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, f_cb, ( DRM_VOID ** )&f_pBlob->pb ) );
        f_pBlob->cb        = f_cb;
        f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_USER_MODE;
        f_pBlob->dwSubType = 0;
        break;
    case DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY:
        ChkArg( f_cb  > 0 );
        ChkArg( f_pb != NULL );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, f_cb, ( DRM_VOID ** )&f_pBlob->pb ) );
        ChkVOID( OEM_TEE_MEMCPY( (DRM_BYTE*)f_pBlob->pb, f_pb, f_cb ) );
        f_pBlob->cb        = f_cb;
        f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_USER_MODE;
        f_pBlob->dwSubType = 0;
        break;
    case DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF:
        ChkArg( ( f_pb == NULL ) == ( f_cb == 0 ) );
        f_pBlob->pb        = f_pb;
        f_pBlob->cb        = f_cb;
        f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF;
        f_pBlob->dwSubType = 0;
        break;
    default:
        ChkArgFail();
        break;
    }

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function frees the given blob.
**
** Operations Performed:
**
**  1. Free the given blob.
**
** Arguments:
**
** f_pContext:      (in/out) The TEE context returned from
**                           DRM_TEE_BASE_AllocTEEContext.
** f_pBlob:         (in/out) The Blob to free.
**                           On output, cb is set to 0 and pb is set to NULL.
**                           Note that this oclwrs even if f_pBlob->eType
**                           is DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF, i.e.
**                           the caller is expected to have their own pointer
**                           f_pBlob->pb which they can free
**                           (if they have not already freed it).
**                           f_pBlob->eType must be one of the following values:
**                           DRM_TEE_BYTE_BLOB_TYPE_ILWALID
**                            (No operation is performed)
**                           DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF
**                            (No operation is performed)
**                           DRM_TEE_BYTE_BLOB_TYPE_USER_MODE
**                            (Memory is freed)
**                           DRM_TEE_BYTE_BLOB_TYPE_CCD
**                            (Freed using OEM_TEE_BASE_FreeBlob)
**
** LWE (kwilson) - In Microsoft PK code, DRM_TEE_BASE_FreeBlob returns DRM_VOID.
**                 Lwpu had to change the return value to DRM_RESULT in order
**                 to support a return code from acquiring/releasing the critical
**                 section, which may not always succeed.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_FreeBlob(
    __inout_opt           DRM_TEE_CONTEXT              *f_pContext,
    __inout               DRM_TEE_BYTE_BLOB            *f_pBlob )
{
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;
    DRM_RESULT       dr         = DRM_SUCCESS;

    /*
    ** This function should NEVER cross the secure boundary. The proxy in the normal world will
    ** will duplicate this method.
    */
    DRMASSERT( f_pBlob != NULL );

    if( f_pContext != NULL )
    {
        pOemTeeCtx = &f_pContext->oContext;
    }

    if( f_pBlob != NULL )
    {
        if( f_pBlob->eType == DRM_TEE_BYTE_BLOB_TYPE_ILWALID )
        {
            /* Free allows invalid because memory might never have been allocated, e.g. on failure. */
            DRMASSERT( f_pBlob->pb          == NULL );
            DRMASSERT( f_pBlob->cb          == 0    );
            DRMASSERT( f_pBlob->dwSubType   == 0    );
        }
        else
        {
            DRMASSERT( IsBlobConsistent( f_pBlob ) );
            if( f_pBlob->pb != NULL && f_pBlob->cb != 0 )
            {
                switch( f_pBlob->eType )
                {
                case DRM_TEE_BYTE_BLOB_TYPE_WEAK_REF:
                    /* no-op */
                    break;
                case DRM_TEE_BYTE_BLOB_TYPE_USER_MODE:
                    ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, ( DRM_VOID ** )&f_pBlob->pb ) );
                    break;
                case DRM_TEE_BYTE_BLOB_TYPE_CCD:
                    ChkDR( OEM_TEE_BASE_SelwreMemHandleFree( pOemTeeCtx, (OEM_TEE_MEMORY_HANDLE*)&f_pBlob->pb ) );
                    break;
                case DRM_TEE_BYTE_BLOB_TYPE_SELWRED_MODE:   __fallthrough;      /* Caller should never have seen this type! */
                default:
                    DRMASSERT( FALSE );
                    break;
                }
            }
        }
        ChkVOID( OEM_TEE_ZERO_MEMORY( f_pBlob, sizeof(*f_pBlob) ) );
    }

ErrorExit:
    return dr;
}

/*
** Synopsis:
**
** This function transfers the ownership of one blob to another.
** No memory is allocated by this function since the transfer is
** just a reassignment of a pointer. This function lives ONLY inside the TEE.
**
** Operations Performed:
**
**  1. Copy source DRM_TEE_BYTE_BLOB structure to destination blob.
**  2. Zero the source blob object so that it doesn't reference
**     the blob data.
**
** Arguments:
**
** f_pDest:          (out) The destination TEE blob object
** f_pSource:        (in/out) The source TEE blob object
**
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_TransferBlobOwnership(
    __inout                            DRM_TEE_BYTE_BLOB          *f_pDest,
    __inout                            DRM_TEE_BYTE_BLOB          *f_pSource )
{
    DRMASSERT( f_pDest     != NULL );
    DRMASSERT( f_pSource   != NULL );

    if( f_pDest != NULL && f_pSource != NULL )
    {
        DRMASSERT( f_pDest->pb == NULL ); /* Catch memory leaks */

        ChkVOID( OEM_TEE_MEMCPY( f_pDest, f_pSource, sizeof(*f_pDest) ) );
        ChkVOID( OEM_TEE_ZERO_MEMORY( f_pSource, sizeof(*f_pSource) ) );
    }
}

/*
** Synopsis:
**
** This function signs data using the Secure Store (SST) Key.
**
** Oracle access to this function outside the TEE is acceptable.
** This function is used to sign data that is stored inside the SST.
** For example, the triggering of Expire After First Play is signed.
** However, all checks related to SST occur in the inselwre world
** and could be bypassed by an attacker (e.g. using a debugger).
** In addition, the data stored on disk can be rolled back by an attacker.
** Validating and using SST-backed data inside the TEE would require
** a large amount of policy-evaluation code to be included inside the
** the TEE which would both increase the TEE code size and increase
** its attack surface area.  Because evaluation based on SST-data is
** considered a relatively low-value asset (i.e. insufficient value
** to perform within the TEE itself), only the key itself is protected
** inside the TEE while the actual usage of the key is not.
**
** This function must return a consistent signature for given data to
** sign across the lifetime of the device.
**
** This function is called numerous places throughout the general PK.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Parse the given PlayReady Private Key Blob (PPKB) and verify its signature.
**  3. Sign the given data using the SST Key.
**
** Arguments:
**
** f_pContext:     (in/out) The TEE context returned from
**                          OEM_TEE_BASE_AllocTEEContext.
** f_pPPKB:        (in)     The current PlayReady Private Key Blob (PPKB).
** f_pDataToSign:  (in)     The data over which to create the SST signature.
** f_pSignature:   (out)    The signature to be written to the SST.
**                          This should be freed with DRM_TEE_BASE_FreeBlob.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_SignDataWithSelwreStoreKey(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __in            const DRM_TEE_BYTE_BLOB            *f_pDataToSign,
    __out                 DRM_TEE_BYTE_BLOB            *f_pSignature )
{
    DRM_RESULT          dr              = DRM_SUCCESS;
    DRM_DWORD           cKeys           = 0;
    DRM_TEE_KEY        *pKeys           = NULL;
    DRM_BYTE            rgbSig[DRM_AES128OMAC1_SIZE_IN_BYTES]; /* Initialized later */
    const DRM_TEE_KEY  *pSSTKeyWeakRef  = NULL;
    DRM_TEE_KEY_TYPE    eType           = DRM_TEE_KEY_TYPE_PR_SST;
    OEM_TEE_CONTEXT    *pOemTeeCtx      = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext        != NULL );
    DRMASSERT( f_pSignature      != NULL );
    DRMASSERT( f_pDataToSign     != NULL );

    DRMASSERT( IsBlobAssigned( f_pPPKB       ) );
    DRMASSERT( IsBlobAssigned( f_pDataToSign ) );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_TEE_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, NULL ) );

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pSignature, sizeof(*f_pSignature) ) );

    pSSTKeyWeakRef = DRM_TEE_BASE_IMPL_LocateKeyInPPKBWeakRef( &eType, cKeys, pKeys );

    ChkBOOL( pSSTKeyWeakRef != NULL, DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( OEM_TEE_BASE_SignWithOMAC1( pOemTeeCtx, &pSSTKeyWeakRef->oKey, f_pDataToSign->cb, f_pDataToSign->pb, rgbSig ) );

    ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, sizeof(rgbSig), rgbSig, f_pSignature ) );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    return dr;
}

/*
** Synopsis:
**
** This function ensures that a PlayReady Private Key Blob (PPKB)
** with current keys is available to the caller to perform other
** TEE operations without any provisioning being required.
**
** This function is called inside Drm_Initialize.
**
** Operations Performed:
**
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Verify that the Secure clock did not go out of sync.
**  3. If the given PlayReady Private Key Blob (PPKB)
**     has a size of 0, return DRM_E_TEE_PROVISIONING_REQUIRED.
**  4. Parse the given PPKB and verify its signature.
**  5. If the TEE Key Identifier (TKID) [which references the
**     TEE Key (TK) which was used to encrypt/sign the data] in
**     the PPKB is not equal to the Current TEE Key Identifier
**     (CTKID), return DRM_E_TEE_PROVISIONING_REQUIRED.
**
** Arguments:
**
** f_pContext:     (in/out) The TEE context returned from
**                          DRM_TEE_BASE_AllocTEEContext.
** f_pPPKB:        (in)     The current PlayReady Private Key Blob (PPKB),
**                          if any.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_CheckDeviceKeys(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pPPKB )
{
    DRM_RESULT           dr      = DRM_SUCCESS;
    DRM_DWORD            cKeys   = 0;
    DRM_TEE_KEY         *pKeys   = NULL;
    DRM_DWORD            dwidCTK;  /* Initialized by OEM_TEE_BASE_GetCTKID */
    DRM_TEE_KB_PPKBData  oPPKB   = {0};

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pPPKB    != NULL );

    ChkDR( OEM_TEE_BASE_GetCTKID( &f_pContext->oContext, &dwidCTK ) );

    /*
    ** This check is redundant with the one in DRM_TEE_KB_ParseAndVerifyPPKB, but we
    ** need to explicitly map this error case to DRM_E_TEE_PROVISIONING_REQUIRED. It is
    ** possible that this value changes prior to the TOCTOU protection before parsing, but it
    ** will fail anyway when it re-checks this value after TOCTOU protection.
    */
    ChkBOOL( f_pPPKB->cb > 0, DRM_E_TEE_PROVISIONING_REQUIRED );

    ChkDR( DRM_TEE_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, &oPPKB ) );

    /*
    ** If the TK used to generate TKDs to encrypt keys in the PPKB and sign the PPKB
    ** does not match the CTK, then CTK has been changed since we generated PPKB.
    ** Therefore, we are in a key history scenario and we need to re-provision.
    */
    ChkBOOL( oPPKB.dwidTK == dwidCTK, DRM_E_TEE_PROVISIONING_REQUIRED );

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    return dr;
}

/*
** Synopsis:
**
** This function generates a new nonce bound to the current TEE session
** and returns it to the caller. The nonce is to be used to bind
** in memory only licenses to the TEE session and protect against replay attacks.
**
** Operations Performed:
**
** 1. Generate a new Nonce
** 2. Return both clear and encrypted version of the newly generated nonce to the caller
**
** Arguments:
**
** f_pContext:     (in/out) The TEE context returned from
**                          DRM_TEE_BASE_AllocTEEContext.
** f_pNKB:         (out)    Signed and Encrypted copy of the Nonce
** f_pNonce:       (out)    The newly generated nonce in clear.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_GenerateNonce(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __out                 DRM_TEE_BYTE_BLOB            *f_pNKB,
    __out                 DRM_ID                       *f_pNonce )
{
    DRM_RESULT           dr           = DRM_SUCCESS;
    DRM_RESULT           drTmp        = DRM_SUCCESS;
    DRM_ID               idNonce;     /* Initialized by OEM_TEE_BASE_GenerateRandomBytes */
    DRM_TEE_BYTE_BLOB    oNKB         = DRM_TEE_BYTE_BLOB_EMPTY;
    OEM_TEE_CONTEXT     *pOemTeeCtx   = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pNonce   != NULL );
    DRMASSERT( f_pNKB     != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    /* Ensure that the Secure clock did not go out of sync */
    if( OEM_TEE_CLOCK_SelwreClockNeedsReSync( pOemTeeCtx ) )
    {
        ChkDR( DRM_E_TEE_CLOCK_DRIFTED );
    }

    ChkDR( OEM_TEE_BASE_GenerateRandomBytes( pOemTeeCtx, sizeof(idNonce), (DRM_BYTE*)&idNonce ) );

    ChkDR( DRM_TEE_KB_BuildNKB( f_pContext, &idNonce, &oNKB ) );

    /* Transfer ownership to the output parameters */
    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pNKB, &oNKB ) );
    *f_pNonce = idNonce;

ErrorExit:

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_BASE_FreeBlob(f_pContext, &oNKB);
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    return dr;
}

/*
** Synopsis:
**
** This function returns the current system time to the caller.
**
** Operations Performed:
**
** 1. Return the current system time to the caller
**
** Arguments:
**
** f_pContext:       (in/out) The TEE context returned from
**                            DRM_TEE_BASE_AllocTEEContext.
** f_pui64SystemTime (out)    The current system time.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_GetSystemTime(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __out                 DRM_UINT64                   *f_pui64SystemTime )
{
    DRM_RESULT       dr           = DRM_SUCCESS;
    OEM_TEE_CONTEXT *pOemTeeCtx   = NULL;
    DRMFILETIME      ftSystemTime = {0};

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext        != NULL );
    DRMASSERT( f_pui64SystemTime != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( OEM_TEE_CLOCK_GetSelwrelySetSystemTime( pOemTeeCtx, &ftSystemTime ) );
    ChkVOID( FILETIME_TO_UI64( ftSystemTime, *f_pui64SystemTime ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocBlobAndTakeOwnership(
    __inout_opt                                           DRM_TEE_CONTEXT              *f_pContext,
    __in                                                  DRM_DWORD                     f_cb,
    __inout_bcount(f_cb)                                  DRM_BYTE                    **f_ppb,
    __inout                                               DRM_TEE_BYTE_BLOB            *f_pBlob )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_ppb  != NULL );
    ChkArg( *f_ppb != NULL );
    ChkArg( f_cb   > 0 );

    ChkDR( DRM_TEE_BASE_AllocBlob(
        f_pContext,
        DRM_TEE_BLOB_ALLOC_BEHAVIOR_WEAKREF,
        f_cb,
       *f_ppb,
        f_pBlob ) );
    f_pBlob->eType     = DRM_TEE_BYTE_BLOB_TYPE_USER_MODE;
    f_pBlob->dwSubType = 0;

    *f_ppb = NULL; /* Clear the original pointer */
ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_ValidateLicenseExpiration(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in                  DRM_XMRFORMAT                *f_pLicense,
    __in                  DRM_BOOL                      f_fValidateBeginDate,
    __in                  DRM_BOOL                      f_fValidateEndDate )
{
    DRM_RESULT  dr             = DRM_SUCCESS;
    DRMFILETIME ftSystemTime   = {0};
    DRMFILETIME ftBeginTime    = {0};
    DRMFILETIME ftEndTime      = {0};
    DRM_UINT64  ui64SystemTime = {0};
    DRM_UINT64  ui64BeginTime  = {0};
    DRM_UINT64  ui64EndTime    = {0};

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pLicense != NULL );

    if( XMRFORMAT_IS_EXPIRATION_VALID( f_pLicense ) )
    {
        dr = OEM_TEE_CLOCK_GetSelwrelySetSystemTime( &f_pContext->oContext, &ftSystemTime );

        if ( dr == DRM_E_NOTIMPL )
        {
            /* DRM_E_NOTIMPL is a special case when an OEM does not wish to enforce time validation inside TEE */
            dr = DRM_SUCCESS;
        }
        else
        {
            ChkDR( dr );
            FILETIME_TO_UI64( ftSystemTime, ui64SystemTime );

            if( f_fValidateBeginDate && f_pLicense->OuterContainer.GlobalPolicyContainer.Expiration.dwBeginDate != 0 )
            {
                DRM_CREATE_FILE_TIME( f_pLicense->OuterContainer.GlobalPolicyContainer.Expiration.dwBeginDate, ftBeginTime );
                FILETIME_TO_UI64( ftBeginTime, ui64BeginTime );
                ChkBOOL( DRM_UI64GEq( ui64SystemTime, ui64BeginTime ), DRM_E_LICENSE_EXPIRED );
            }

            if( f_fValidateEndDate && f_pLicense->OuterContainer.GlobalPolicyContainer.Expiration.dwEndDate != XMR_UNLIMITED )
            {
                DRM_CREATE_FILE_TIME( f_pLicense->OuterContainer.GlobalPolicyContainer.Expiration.dwEndDate, ftEndTime );
                FILETIME_TO_UI64( ftEndTime, ui64EndTime );
                ChkBOOL( DRM_UI64Les( ui64SystemTime, ui64EndTime ), DRM_E_LICENSE_EXPIRED );
            }
        }
    }

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_CheckLicenseSignature(
    __inout                DRM_TEE_CONTEXT              *f_pContext,
    __in             const DRM_XMRFORMAT                *f_pXMRLicense,
    __in_ecount( 2 ) const DRM_TEE_KEY                  *f_pKeys )
{
    DRM_RESULT          dr      = DRM_SUCCESS;

    DRMASSERT( f_pContext    != NULL );
    DRMASSERT( f_pXMRLicense != NULL );
    DRMASSERT( f_pKeys       != NULL );

    ChkDR( OEM_TEE_BASE_VerifyOMAC1Signature(
        &f_pContext->oContext,
        &f_pKeys[0].oKey, /* CI is always first */
        XMRFORMAT_GET_LICENSE_LENGTH_WITHOUT_SIGNATURE( f_pXMRLicense ),
        XBBA_TO_PB( f_pXMRLicense->xbbaRawData ),
        XBBA_TO_CB( f_pXMRLicense->OuterContainer.Signature.xbbaSignature ),
        XBBA_TO_PB( f_pXMRLicense->OuterContainer.Signature.xbbaSignature ) ) );

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocateKeyArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __deref_out                                           DRM_TEE_KEY               **f_ppKeys )
{
    DRM_RESULT       dr         = DRM_SUCCESS;
    DRM_DWORD        cbKeys     = 0;
    DRM_TEE_KEY     *pKeys      = NULL;
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_cKeys     > 0    );
    DRMASSERT( f_ppKeys   != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( DRM_DWordMult( f_cKeys, sizeof(DRM_TEE_KEY), &cbKeys ) );
    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( pOemTeeCtx, cbKeys, (DRM_VOID**)&pKeys ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pKeys, cbKeys ) );

    *f_ppKeys  = pKeys;
    pKeys = NULL;

ErrorExit:
    ChkVOID( DRM_TEE_BASE_IMPL_FreeKeyArray( f_pContext, f_cKeys, &pKeys ) );
    return dr;
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_IMPL_FreeKeyArray(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in                                                  DRM_DWORD                   f_cKeys,
    __deref_inout_ecount( f_cKeys )                       DRM_TEE_KEY               **f_ppKeys )
{
    DRM_TEE_KEY     *pKeys      = NULL;
    OEM_TEE_CONTEXT *pOemTeeCtx = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_ppKeys   != NULL );

    pOemTeeCtx = &f_pContext->oContext;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_READ_OVERRUN_6385, "In spite of correct SAL annotations on f_ppKeys, this warning is thrown.  No overrun can occur." )
    pKeys = *f_ppKeys;
    if( pKeys != NULL && f_pContext != NULL )
    {
        DRM_DWORD iKey = 0;
        DRMASSERT( f_cKeys > 0 );
        for( iKey = 0; iKey < f_cKeys; iKey++ )
        {
            switch( pKeys[iKey].eType )
            {
            case DRM_TEE_KEY_TYPE_ILWALID:
                /* Nothing to free */
                break;
            case DRM_TEE_KEY_TYPE_TK:                   __fallthrough;
            case DRM_TEE_KEY_TYPE_TKD:                  __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_SST:               __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_CI:                __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_CK:                __fallthrough;
            case DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY:    __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_MI:                __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_MK:                __fallthrough;
            case DRM_TEE_KEY_TYPE_SAMPLEPROT:           __fallthrough;
            case DRM_TEE_KEY_TYPE_DOMAIN_SESSION:       __fallthrough;
            case DRM_TEE_KEY_TYPE_AES128_DERIVATION:    __fallthrough;
                ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &pKeys[ iKey ].oKey ) );
                break;
            case DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN:       __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT:    __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND:       __fallthrough;
                ChkVOID( OEM_TEE_BASE_FreeKeyECC256( pOemTeeCtx, &pKeys[iKey].oKey ) );
                break;
            default:
                DRMASSERT( FALSE );
                break;
            }
        }
        {
            DRM_DWORD cbKeys = 0;
#if DRM_DBG
            DRM_RESULT drTmp = DRM_DWordMult( f_cKeys, sizeof(DRM_TEE_KEY), &cbKeys );
            DRMASSERT( DRM_SUCCESS == drTmp );
#else  /* DRM_DBG */
            (DRM_VOID)DRM_DWordMult( f_cKeys, sizeof(DRM_TEE_KEY), &cbKeys );
#endif /* DRM_DBG */
            ChkVOID( OEM_TEE_BASE_SelwreMemFree( pOemTeeCtx, (DRM_VOID**)&pKeys ) );
        }
    }
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */
    *f_ppKeys = NULL;
}

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_BASE_IMPL_GetKeyDerivationIDSign(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType )
{
    DRM_ID idKeyDerivation = {{0}};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"

    switch( f_eType )
    {
    case DRM_TEE_XB_KB_TYPE_PPKB : idKeyDerivation = g_idKeyDerivationSignPPKB ; break;
    case DRM_TEE_XB_KB_TYPE_LKB  : idKeyDerivation = g_idKeyDerivationSignLKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CDKB : idKeyDerivation = g_idKeyDerivationSignCDKB ; break;
    case DRM_TEE_XB_KB_TYPE_DKB  : idKeyDerivation = g_idKeyDerivationSignDKB  ; break;
    case DRM_TEE_XB_KB_TYPE_MRKB : idKeyDerivation = g_idKeyDerivationSignMRKB ; break;
    case DRM_TEE_XB_KB_TYPE_MTKB : idKeyDerivation = g_idKeyDerivationSignMTKB ; break;
    case DRM_TEE_XB_KB_TYPE_RKB  : idKeyDerivation = g_idKeyDerivationSignRKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CEKB : idKeyDerivation = g_idKeyDerivationSignCEKB ; break;
    case DRM_TEE_XB_KB_TYPE_TPKB : idKeyDerivation = g_idKeyDerivationSignTPKB ; break;
    case DRM_TEE_XB_KB_TYPE_SPKB : idKeyDerivation = g_idKeyDerivationSignSPKB ; break;
    case DRM_TEE_XB_KB_TYPE_NKB  : idKeyDerivation = g_idKeyDerivationSignNKB  ; break;
    case DRM_TEE_XB_KB_TYPE_RPCKB: idKeyDerivation = g_idKeyDerivationSignRPCKB; break;
    case DRM_TEE_XB_KB_TYPE_LPKB : idKeyDerivation = g_idKeyDerivationSignLPKB ; break;
    default                      : DRMASSERT( FALSE )                          ; break;
    }

    return idKeyDerivation;
}

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_BASE_IMPL_GetKeyDerivationIDEncrypt(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType )
{
    DRM_ID idKeyDerivation = {{0}};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"

    switch( f_eType )
    {
    case DRM_TEE_XB_KB_TYPE_PPKB : idKeyDerivation = g_idKeyDerivationEncryptPPKB ; break;
    case DRM_TEE_XB_KB_TYPE_LKB  : idKeyDerivation = g_idKeyDerivationEncryptLKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CDKB : idKeyDerivation = g_idKeyDerivationEncryptCDKB ; break;
    case DRM_TEE_XB_KB_TYPE_DKB  : idKeyDerivation = g_idKeyDerivationEncryptDKB  ; break;
    case DRM_TEE_XB_KB_TYPE_MRKB : idKeyDerivation = g_idKeyDerivationEncryptMRKB ; break;
    case DRM_TEE_XB_KB_TYPE_MTKB : idKeyDerivation = g_idKeyDerivationEncryptMTKB ; break;
    case DRM_TEE_XB_KB_TYPE_RKB  : idKeyDerivation = g_idKeyDerivationEncryptRKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CEKB : idKeyDerivation = g_idKeyDerivationEncryptCEKB ; break;
    case DRM_TEE_XB_KB_TYPE_TPKB : idKeyDerivation = g_idKeyDerivationEncryptTPKB ; break;
    case DRM_TEE_XB_KB_TYPE_SPKB : idKeyDerivation = g_idKeyDerivationEncryptSPKB ; break;
    case DRM_TEE_XB_KB_TYPE_NKB  : idKeyDerivation = g_idKeyDerivationEncryptNKB  ; break;
    case DRM_TEE_XB_KB_TYPE_RPCKB: idKeyDerivation = g_idKeyDerivationEncryptRPCKB; break;
    case DRM_TEE_XB_KB_TYPE_LPKB : idKeyDerivation = g_idKeyDerivationEncryptLPKB ; break;
    default                      : DRMASSERT( FALSE )                             ; break;
    }

    return idKeyDerivation;
}

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_BASE_IMPL_IsKBTypePersistedToDisk(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType )
{
    DRM_BOOL fPersist = FALSE;

    switch( f_eType )
    {
    case DRM_TEE_XB_KB_TYPE_PPKB : fPersist = TRUE   ; break;
    case DRM_TEE_XB_KB_TYPE_DKB  : fPersist = TRUE   ; break;
    case DRM_TEE_XB_KB_TYPE_TPKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_CDKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_MRKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_MTKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_CEKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_RKB  : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_SPKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_RPCKB: fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_NKB  : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_LPKB : fPersist = TRUE   ; break;
        /* caller should always know LKB type */
    case DRM_TEE_XB_KB_TYPE_LKB  : __fallthrough;
    default                      : DRMASSERT( FALSE ); break;
    }

    return fPersist;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_DeriveTKDFromTK(
    __inout                                               DRM_TEE_CONTEXT            *f_pContext,
    __in_opt                                        const DRM_TEE_KEY                *f_pTK,
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType,
    __in                                                  DRM_TEE_XB_KB_OPERATION     f_eOperation,
    __in_opt                                        const DRM_BOOL                   *f_pfPersist,
    __out                                                 DRM_TEE_KEY                *f_pTKD )
{
    /*
    ** This function facilitates the general requirement for cryptographic
    ** separation of operations within the TEE. The inputs to this function
    ** control the cryptographic separation, therefore the caller must pass
    ** appropriately high quality and differentiated values for the inputs.
    */
    DRM_RESULT       dr               = DRM_SUCCESS;
    DRM_ID           idKeyDerivation  = {{0}};  // LWE (nkuo) - changed due to compile error "missing braces around initializer"
    DRM_BOOL         fPersist         = FALSE;
    DRM_TEE_KEY      oCTK             = DRM_TEE_KEY_EMPTY;
    OEM_TEE_CONTEXT *pOemTeeCtx       = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pTKD     != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    if( f_pfPersist == NULL )
    {
        fPersist = DRM_TEE_BASE_IMPL_IsKBTypePersistedToDisk( f_eType );
    }
    else
    {
        fPersist = *f_pfPersist;
    }

    if( f_eOperation == DRM_TEE_XB_KB_OPERATION_SIGN )
    {
        idKeyDerivation = DRM_TEE_BASE_IMPL_GetKeyDerivationIDSign( f_eType );
    }
    else if( f_eOperation == DRM_TEE_XB_KB_OPERATION_ENCRYPT )
    {
        idKeyDerivation = DRM_TEE_BASE_IMPL_GetKeyDerivationIDEncrypt( f_eType );
    }
    else
    {
        AssertChkArg( FALSE );
    }

    /* Use CTK if no PTK is specified */
    if( f_pTK == NULL )
    {
        ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_TK, &oCTK ) );
        ChkDR( OEM_TEE_BASE_GetCTKID( pOemTeeCtx, &oCTK.dwidTK ) );
        ChkDR( OEM_TEE_BASE_GetTKByID( pOemTeeCtx, oCTK.dwidTK, &oCTK.oKey ) );
        f_pTK = &oCTK;
    }
    else
    {
        DRMASSERT( f_pTK->eType == DRM_TEE_KEY_TYPE_TK );
    }

    ChkDR( DRM_TEE_BASE_IMPL_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_TKD, f_pTKD ) );
    ChkDR( OEM_TEE_BASE_DeriveKey(
        pOemTeeCtx,
        &f_pTK->oKey,
        &idKeyDerivation,
        fPersist ? NULL : &f_pContext->idSession,
        &f_pTKD->oKey ) );

    f_pTKD->dwidTK = f_pTK->dwidTK;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &oCTK.oKey ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_TEE_KEY* DRM_CALL DRM_TEE_BASE_IMPL_LocateKeyInPPKBWeakRef(
    __in_opt                 const DRM_TEE_KEY_TYPE    *f_peType,
    __in                           DRM_DWORD            f_cPPKBKeys,
    __in_ecount( f_cPPKBKeys )     DRM_TEE_KEY         *f_pPPKBKeys )
{
    DRM_DWORD    iKey       = 0;
    DRM_TEE_KEY *pTKWeakRef = NULL;

    DRMASSERT( ( f_cPPKBKeys > 0 ) && ( f_pPPKBKeys != NULL ) );

    for( iKey = 0; iKey < f_cPPKBKeys && pTKWeakRef == NULL; iKey++ )
    {
        if( f_peType != NULL && *f_peType != f_pPPKBKeys[iKey].eType )
        {
            continue;
        }

        pTKWeakRef = &f_pPPKBKeys[iKey];
    }

    /* Return NULL if no key is found to indicate that we're using the CTK */

    return pTKWeakRef;
}

/* No DWORD value within the version should ever exceed 5 digits. */
#define MAX_EXPECTED_CHARS_IN_ONE_VERSION_ENTRY 5

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_GetPKVersion(
    __out   DRM_DWORD   *f_pdwPKMajorVersion,
    __out   DRM_DWORD   *f_pdwPKMinorVersion,
    __out   DRM_DWORD   *f_pdwPKBuildVersion,
    __out   DRM_DWORD   *f_pdwPKQFEVersion )
{
    DRM_RESULT dr = DRM_SUCCESS;

    AssertChkArg( f_pdwPKMajorVersion != NULL );
    AssertChkArg( f_pdwPKMinorVersion != NULL );
    AssertChkArg( f_pdwPKBuildVersion != NULL );
    AssertChkArg( f_pdwPKQFEVersion   != NULL );

    DRMCASSERT( DRM_NO_OF(g_dwReqTagPlayReadyClientVersionData) >= DRM_VERSION_LEN );

    /* Note: The format for PlayReady versions is Major.Minor.QFE.Build */
    *f_pdwPKMajorVersion = g_dwReqTagPlayReadyClientVersionData[0];
    *f_pdwPKMinorVersion = g_dwReqTagPlayReadyClientVersionData[1];
    *f_pdwPKQFEVersion   = g_dwReqTagPlayReadyClientVersionData[2];
    *f_pdwPKBuildVersion = g_dwReqTagPlayReadyClientVersionData[3];

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_CopyDwordPairsToAlignedBufferAndSwapEndianness(
    __inout    DRM_TEE_CONTEXT      *f_pContext,
    __out      DRM_TEE_BYTE_BLOB    *f_pDest,
    __in const DRM_TEE_BYTE_BLOB    *f_pSource )
{
    DRM_RESULT        dr    = DRM_SUCCESS;

#if TARGET_LITTLE_ENDIAN

    DRM_TEE_BYTE_BLOB oDest = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD         ib; /* loop variable initialized below */

    DRMCASSERT( sizeof(DRM_DWORD) == 4 );
    DRMASSERT( f_pDest   != NULL );
    DRMASSERT( f_pSource != NULL );

    /* f_pSource must have a multiple of 2 DWORDs. */
    ChkArg( ( f_pSource->cb & (DRM_DWORD)( 2 * sizeof(DRM_DWORD) - 1 ) ) == 0 );

    ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW, f_pSource->cb, NULL, &oDest ) );

    for( ib = 0; ib < f_pSource->cb; ib += sizeof(DRM_DWORD) )
    {
        /* oDest.pb must already be aligned because we allocated it just above. */
        DRM_DWORD *pdwDest = (DRM_DWORD*)&oDest.pb[ib];
        DRM_BIG_ENDIAN_BYTES_TO_NATIVE_DWORD( *pdwDest, &f_pSource->pb[ib] );
    }
    ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pDest, &oDest ) );

#else   /* TARGET_LITTLE_ENDIAN */

    /* f_pSource must have a multiple of 2 DWORDs. */
    ChkArg( ( f_pSource->cb & (DRM_DWORD)( 2 * sizeof(DRM_DWORD) - 1 ) ) == 0 );
    ChkDR( DRM_TEE_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pSource->cb, f_pSource->pb, f_pDest ) );

#endif  /* TARGET_LITTLE_ENDIAN */

ErrorExit:
    return dr;
}
#endif
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocKeyAES128(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __in                                            DRM_TEE_KEY_TYPE            f_eType,
    __out                                           DRM_TEE_KEY                *f_pKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDR( OEM_TEE_BASE_AllocKeyAES128( f_pContextAllowNULL, &f_pKey->oKey ) );
    f_pKey->eType = f_eType;

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AllocKeyECC256(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                            DRM_TEE_KEY_TYPE            f_eType,
    __out                                           DRM_TEE_KEY                *f_pKey )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDR( OEM_TEE_BASE_AllocKeyECC256( f_pContext, &f_pKey->oKey ) );
    f_pKey->eType = f_eType;

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_GetAppSpecificHWID(
    __inout                                         OEM_TEE_CONTEXT            *f_pOemTeeCtx,
    __in                                      const DRM_ID                     *f_pidApplication,
    __out                                           DRM_ID                     *f_pidHWID )
{
    DRM_RESULT               dr                 = DRM_SUCCESS;
    DRM_DWORD               *pdwTemp            = (DRM_DWORD*)f_pidHWID;
    DRM_ID                   oCertClientID; /*Initialized in OEM_TEE_BASE_GetDeviceUniqueID*/
    DRM_SHA256_CONTEXT       sha256Context;
    DRM_SHA256_Digest        oDigest;
    DRM_DWORD                i;
    OEM_TEE_CONTEXT         *pOemTeeCtx         = NULL;

    DRMASSERT( f_pidApplication != NULL );
    DRMASSERT( f_pidHWID        != NULL );

    pOemTeeCtx = f_pOemTeeCtx;

    /* Get Base HWID */
    ChkDR( OEM_TEE_BASE_GetDeviceUniqueID( pOemTeeCtx, &oCertClientID ) );

    ChkDR( OEM_TEE_BASE_SHA256_Init( pOemTeeCtx, &sha256Context ) );

    ChkDR( OEM_TEE_BASE_SHA256_Update(
        pOemTeeCtx,
        &sha256Context,
        sizeof(oCertClientID),
        (DRM_BYTE*)&oCertClientID ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update(
        pOemTeeCtx,
        &sha256Context,
        sizeof(*f_pidApplication),
        (DRM_BYTE*)f_pidApplication ) );

    ChkDR( OEM_TEE_BASE_SHA256_Finalize( pOemTeeCtx, &sha256Context, &oDigest ) );

    DRMSIZEASSERT( sizeof(oDigest), 2*sizeof(DRM_ID) );
    /* Fold it */
    for ( i = 0; i < sizeof(oDigest)/(2*sizeof(DRM_DWORD)); i++ )
    {
        *pdwTemp++ = ((DRM_DWORD*)(oDigest.m_rgbDigest))[i]
                        ^ ((DRM_DWORD*)(oDigest.m_rgbDigest))[i + sizeof(oDigest)/(2*sizeof(DRM_DWORD))];
    }

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_AES128CBC(
    __inout                                                DRM_TEE_CONTEXT            *f_pContext,
    __in                                                   DRM_DWORD                   f_cbBlob,
    __in_bcount(f_cbBlob)                            const DRM_BYTE                   *f_pbBlob,
    __in                                             const DRM_TEE_KEY                *f_poKey,
    __inout                                                DRM_DWORD                  *f_pcbEncryptedBlob,
    __out_bcount(DRM_AESCBC_ENCRYPTED_SIZE_W_IV(f_cbBlob)) DRM_BYTE                   *f_pbEncryptedBlob )
{
    DRM_RESULT                   dr             = DRM_SUCCESS;
    OEM_TEE_CONTEXT             *pOemTeeCtx     = NULL;
    DRM_BYTE                     bPadding;      /* Initialized later */
    DRM_BYTE                    *pbPadding;     /* Initialized later */
    DRM_DWORD                    cbPadding;     /* Initialized later */

    DRMASSERT( f_pcbEncryptedBlob != NULL );
    DRMASSERT( f_pbEncryptedBlob  != NULL );
    DRMASSERT( f_pbBlob           != NULL );
    DRMASSERT( f_cbBlob           != 0 );
    DRMASSERT( f_pContext         != NULL );
    DRMASSERT( f_poKey            != NULL );

    ChkArg( *f_pcbEncryptedBlob >= DRM_AESCBC_ENCRYPTED_SIZE_W_IV(f_cbBlob) );

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( OEM_TEE_BASE_GenerateRandomBytes(
        pOemTeeCtx,
        DRM_AES_BLOCKLEN,
        f_pbEncryptedBlob ) );

    ChkVOID( OEM_TEE_MEMCPY( f_pbEncryptedBlob + DRM_AES_BLOCKLEN, f_pbBlob, f_cbBlob ) );

    pbPadding = f_pbEncryptedBlob + DRM_AES_BLOCKLEN + f_cbBlob;
    bPadding = DRM_AES_BLOCKLEN - f_cbBlob % DRM_AES_BLOCKLEN;

    for( cbPadding = 0; cbPadding < bPadding; cbPadding++ )
    {
        *pbPadding++ = bPadding;
    }

    *f_pcbEncryptedBlob = (f_cbBlob+2*DRM_AES_BLOCKLEN) - f_cbBlob%DRM_AES_BLOCKLEN;

    ChkDR( OEM_TEE_BASE_AES128CBC_EncryptData(
        pOemTeeCtx,
        &f_poKey->oKey,
        f_pbEncryptedBlob,                          /* IV */
        *f_pcbEncryptedBlob - DRM_AES_BLOCKLEN,     /* length to be encrypted */
        f_pbEncryptedBlob + DRM_AES_BLOCKLEN ) );   /* pointer to items to be encrypted*/

ErrorExit:
    return dr;
}
#endif

#if defined (SEC_COMPILE)
/*
** Synopsis:
**
** This function parses a binary certificate chain while attempting to minimize
** the amount of memory required for that parsing.
**
** Operations Performed:
**
**  1. Start with a small value for cbStack.
**  2. If cbStack exceeds a maximum, fail with DRM_E_OUTOFMEMORY.
**     This will never happen the first time we reach this step.
**  3. Allocate cbStack.
**  4. Attempt to parse the certificate.
**  5. If certificate parsing fails with DRM_E_OUTOFMEMORY,
**     double cbStack and goto 2.
**  6. Else if certificate parsing fails, return that error.
**  7. Else return the parsed certificate chain,
**     the leaf-most certificate (optional), and the memory
**     allocated.  The caller must keep this allocated memory
**     available until it is done using the parsed information.
**
** Arguments:
**
** f_pOemTeeContext:       (in/out) The TEE context returned from
**                                  OEM_TEE_BASE_AllocTEEContext.
** f_pftLwrrentTime:           (in) The current time if expiration is to be validated.
** f_dwExpectedLeafCertType:   (in) The expected leaf certificate type.
** f_cbCertData:               (in) Size of the certificate chain.
** f_pbCertData:               (in) Certificate chain.
** f_pChainData:              (out) Parsed certificate chain data.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_ParseCertificateChain(
    __inout_opt                           OEM_TEE_CONTEXT                       *f_pOemTeeContext,
    __in_ecount_opt( 1 )            const DRMFILETIME                           *f_pftLwrrentTime,
    __in                                  DRM_DWORD                              f_dwExpectedLeafCertType,
    __in                                  DRM_DWORD                              f_cbCertData,
    __in_bcount( f_cbCertData )     const DRM_BYTE                              *f_pbCertData,
    __out                                 DRM_BCERTFORMAT_PARSER_CHAIN_DATA     *f_pChainData )
{
    DRM_RESULT                  dr      = DRM_SUCCESS;
    DRM_BYTE                   *pbStack = NULL;
    DRM_DWORD                   cbStack = DRM_BCERTFORMAT_PARSER_STACK_SIZE_MIN;
    DRM_STACK_ALLOCATOR_CONTEXT oStack; /* Initialized by DRM_STK_Init */

    /*
    ** We are attempting to minimize the amount of memory used to parse a certificate chain.
    ** We do this by allocating a relatively small amount of memory (DRM_BCERTFORMAT_PARSER_STACK_SIZE_MIN)
    ** and attempting to parse the chain.  This will fail with DRM_E_OUTOFMEMORY if it is
    ** not enough space.  If this happens, we double the amount of memory we allocated
    ** and try again.  We repeat this until the amount of memory we would allocate exceeds
    ** a value which should work for all valid certificates (DRM_BCERTFORMAT_PARSER_STACK_SIZE_MAX).
    ** If we exceed that maximum, we actually fail parsing with DRM_E_OUTOFMEMORY.
    */
    do
    {
        ChkBOOL( cbStack <= DRM_BCERTFORMAT_PARSER_STACK_SIZE_MAX, DRM_E_OUTOFMEMORY );
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeContext, (DRM_VOID**)&pbStack ) );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pOemTeeContext, cbStack, (DRM_VOID**)&pbStack ) );
        ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );

        cbStack <<= 1;

        dr = DRM_BCERTFORMAT_ParseCertificateChainData(
            f_pOemTeeContext,
            f_pftLwrrentTime,
            f_dwExpectedLeafCertType,
            &oStack,
            f_cbCertData,
            f_pbCertData,
            f_pChainData );

    } while( dr == DRM_E_OUTOFMEMORY );

    ChkDR( dr );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeContext, (DRM_VOID **)&pbStack ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_ParseLicense(
    __inout                           DRM_TEE_CONTEXT              *f_pContext,
    __in                              DRM_DWORD                     f_cbLicense,
    __in_ecount( f_cbLicense )  const DRM_BYTE                     *f_pbLicense,
    __inout                           DRM_TEE_XMR_LICENSE          *f_poLicense )
{
    DRM_RESULT dr          = DRM_SUCCESS;
    DRM_DWORD  cbStackMax; /* Initialized in DRM_DWordMult below */
    DRM_DWORD  cbStack;    /* Initialized below */

    DRMASSERT( f_pContext  != NULL );
    DRMASSERT( f_poLicense != NULL );
    AssertChkArg( f_cbLicense >= DRM_XMRFORMAT_MIN_UNPACK_DIVISOR );
    AssertChkArg( f_poLicense->pbStack == NULL );

    /*
    ** The stack size required to parse a license is loosely related to the size of the license.
    ** While some licenses may contain information that will require more stack space, those
    ** licenses are generally larger. Therefore we will attempt to start with a fraction of the
    ** size of the license and double the stack size on failure. The divisor being used was
    ** empirically chosen after analyzing many different licenses.
    */
    cbStack = f_cbLicense / DRM_XMRFORMAT_MIN_UNPACK_DIVISOR;
    ChkDR( DRM_DWordMult( f_cbLicense, DRM_XMRFORMAT_MAX_UNPACK_FACTOR, &cbStackMax ) );

    /*
    ** We are attempting to minimize the amount of memory used to parse a license.
    ** We do this by allocating a relatively small amount of memory (licenseSize / DRM_XMRFORMAT_MIN_UNPACK_DIVISOR)
    ** and attempting to parse the license.  This will fail with DRM_E_OUTOFMEMORY if it is
    ** not enough space.  If this happens, we increase the amount of memory we allocated
    ** and try again.  We repeat this until the amount of memory we would allocate exceeds
    ** a value which should work for all valid licenses (licenseSize * DRM_XMRFORMAT_MAX_UNPACK_FACTOR).
    ** If we exceed that maximum, we actually fail parsing with DRM_E_OUTOFMEMORY.
    */
    do
    {
        /* Allocate stack for leaf license */
        ChkBOOL( cbStack <= cbStackMax, DRM_E_OUTOFMEMORY );
        /* Check for cbStack overflow */
        ChkBOOL( cbStack > 0, DRM_E_OUTOFMEMORY );
        f_poLicense->cbStack = cbStack;
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( &f_pContext->oContext, (DRM_VOID**)&f_poLicense->pbStack ) );
        ChkDR( OEM_TEE_BASE_SelwreMemAlloc( &f_pContext->oContext, cbStack, (DRM_VOID**)&f_poLicense->pbStack ) );
        ChkDR( DRM_STK_Init( &f_poLicense->oStack, f_poLicense->pbStack, cbStack, TRUE ) );

        cbStack <<= 1;

        /* Unpack leaf license */
        dr = DRM_XMRFORMAT_Parse( f_cbLicense, f_pbLicense, &f_poLicense->oStack, NULL, &f_poLicense->oLicense );

    } while( dr == DRM_E_OUTOFMEMORY );

    ChkDR(dr);

    f_poLicense->fValid = TRUE;

ErrorExit:
    if( DRM_FAILED( dr ) )
    {
        ChkVOID( OEM_TEE_BASE_SelwreMemFree( &f_pContext->oContext, (DRM_VOID**)&f_poLicense->pbStack ) );
    }
    return dr;
}
#endif


EXIT_PK_NAMESPACE_CODE;

