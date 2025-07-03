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
#include <drmstrsafe.h>
#include <drmteeproxystubcommon.h>
#include <drmteeproxystubcommon.h>
#include <drmteeproxyformat_generated.h>
#include <drmversionconstants.h>
#include <drmteekbcryptodata.h>
#include <drmxbparser.h>
#include <oemtee.h>
#include <oemteecrypto.h>
#include <oemaescommon.h>
#include <oemcommon.h>
#include <oembyteorder.h>

#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/* These all have to be different. They also have to be different from s_idUniqueIDLabel in oemtee.c */
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignPPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignPPKB)     = {{ 0x42, 0x8c, 0x44, 0x0f, 0x03, 0x81, 0x47, 0x5b, 0xb6, 0xbf, 0x1b, 0xac, 0x8a, 0x55, 0xaa, 0x20 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignLKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignLKB)      = {{ 0x42, 0x63, 0x24, 0x28, 0xd5, 0x6f, 0x4d, 0xb0, 0x9c, 0x53, 0x7f, 0xe4, 0x63, 0x69, 0xa7, 0x5b }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignCDKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignCDKB)     = {{ 0x35, 0xde, 0xab, 0x19, 0x94, 0x77, 0x42, 0x88, 0x8b, 0x98, 0x52, 0x5a, 0x15, 0xa5, 0xe2, 0x52 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignDKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignDKB)      = {{ 0xbd, 0x22, 0xf4, 0xc0, 0xc5, 0xdc, 0x49, 0x63, 0xae, 0xca, 0xf6, 0x10, 0x5f, 0x86, 0xdd, 0xa6 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignRKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignRKB)      = {{ 0xf6, 0x02, 0xf1, 0x9b, 0x89, 0xb5, 0x40, 0x49, 0xbd, 0x4e, 0x48, 0x82, 0xe1, 0x5a, 0xd5, 0x90 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignCEKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignCEKB)     = {{ 0x95, 0x94, 0xf8, 0x89, 0x58, 0x75, 0x47, 0xd7, 0x9a, 0x1d, 0x51, 0x07, 0x69, 0x5d, 0x28, 0x33 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignTPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignTPKB)     = {{ 0x2c, 0x1d, 0x86, 0x25, 0x5e, 0xe4, 0x00, 0xd6, 0x46, 0x3e, 0x95, 0x3c, 0xdd, 0x5a, 0x27, 0x5c }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignSPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignSPKB)     = {{ 0x8F, 0xF2, 0x2A, 0x57, 0xA5, 0x07, 0x4F, 0xC6, 0x92, 0x79, 0xB7, 0xB6, 0x6F, 0xE7, 0x00, 0x59 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignNKB       PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignNKB)      = {{ 0x52 ,0xb9 ,0x32 ,0xb9 ,0x86 ,0x2b ,0x1b ,0xa5 ,0xaa ,0x81 ,0x52 ,0xd1 ,0x5b ,0xea ,0x44 ,0x1b }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignNTKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignNTKB)     = {{ 0x31, 0xe9, 0xe8, 0xff, 0x9a, 0xc0, 0x48, 0x0c, 0x8b, 0xb9, 0xb1, 0x9b, 0x71, 0x11, 0x2c, 0xe7 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignRPCKB     PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignRPCKB)    = {{ 0x8a, 0x86, 0x12, 0xeb, 0xee, 0xca, 0x4a, 0x6f, 0xbe, 0x00, 0x1b, 0x98, 0xd5, 0x40, 0xf3, 0x80 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignLPKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignLPKB)     = {{ 0xa8, 0xef, 0x69, 0x5c, 0x66, 0x07, 0x41, 0x96, 0xab, 0x99, 0xc8, 0xde, 0xcc, 0xc0, 0xf1, 0xa3 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationSignSSKB      PR_ATTR_DATA_OVLY(_g_idKeyDerivationSignSSKB)     = {{ 0x4e, 0x5b, 0x0f, 0xa4, 0x8e, 0xc1, 0x03, 0x45, 0xb6, 0x3c, 0xec, 0x65, 0x18, 0x06, 0x50, 0x5f }};

static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptPPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptPPKB)  = {{ 0x06, 0xa2, 0x96, 0x61, 0x03, 0x14, 0x4f, 0x63, 0x8c, 0x47, 0xa3, 0xb6, 0xdc, 0x23, 0x14, 0xa8 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptLKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptLKB)   = {{ 0x6a, 0x37, 0x83, 0x82, 0x62, 0x74, 0x46, 0x22, 0xad, 0xd0, 0xf3, 0x21, 0x37, 0x1a, 0x8c, 0xb7 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptCDKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptCDKB)  = {{ 0xd1, 0x67, 0xaa, 0xeb, 0x3b, 0x70, 0x42, 0xe5, 0x8b, 0x2b, 0x06, 0xa6, 0xfe, 0xbb, 0x96, 0x72 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptDKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptDKB)   = {{ 0xf0, 0x7b, 0x66, 0x60, 0xd5, 0xcb, 0x4c, 0x4f, 0x8d, 0xb7, 0x19, 0xa5, 0x2e, 0x3d, 0x1d, 0xac }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptRKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptRKB)   = {{ 0xa6, 0xab, 0xa5, 0x65, 0xcf, 0xbf, 0x4d, 0x8b, 0x80, 0x60, 0xb5, 0x49, 0x7e, 0x0a, 0x8b, 0x15 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptCEKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptCEKB)  = {{ 0xed, 0x4d, 0xe9, 0x91, 0x89, 0xee, 0x45, 0x8d, 0x80, 0x8a, 0x61, 0xb8, 0x92, 0x68, 0x3c, 0x18 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptTPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptTPKB)  = {{ 0x66, 0x79, 0xee, 0x23, 0x10, 0xff, 0xe5, 0x92, 0x33, 0xda, 0x6e, 0x26, 0x1b, 0x41, 0x97, 0x37 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptSPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptSPKB)  = {{ 0x59, 0xC8, 0xED, 0xE9, 0x9D, 0xB1, 0x48, 0xA5, 0xBF, 0x40, 0x07, 0xC0, 0x50, 0x3D, 0xCA, 0x43 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptNKB    PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptNKB)   = {{ 0xdf ,0x1f ,0xd4 ,0xa6 ,0x79 ,0x06 ,0x51 ,0x32 ,0xe7 ,0x09 ,0x3e ,0x46 ,0x5c ,0x52 ,0x8f ,0x6e }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptNTKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptNTKB)  = {{ 0xfb, 0x31, 0xe0, 0x4f, 0x26, 0xe3, 0x45, 0x21, 0x8e, 0x8c, 0x6e, 0x66, 0x6e, 0x4d, 0x0c, 0xcd }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptRPCKB  PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptRPCKB) = {{ 0x75, 0x2b, 0x50, 0x42, 0x03, 0x6c, 0x48, 0x4d, 0x91, 0x36, 0xab, 0x72, 0xbd, 0x97, 0xed, 0xb9 }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptLPKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptLPKB)  = {{ 0x36, 0x06, 0x5b, 0x44, 0x13, 0xb9, 0x4b, 0xd0, 0x96, 0x52, 0x5e, 0x59, 0xeb, 0xa5, 0x36, 0x3a }};
static DRM_GLOBAL_CONST DRM_ID g_idKeyDerivationEncryptSSKB   PR_ATTR_DATA_OVLY(_g_idKeyDerivationEncryptSSKB)  = {{ 0x7d, 0xd7, 0x8e, 0xab, 0xbc, 0xd2, 0x5c, 0x4b, 0x8e, 0x01, 0x7f, 0x19, 0xb3, 0xb9, 0xc5, 0xf5 }};

DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_rgdstrDrmTeeProperties[ DRM_TEE_PROPERTY_MAX + 1 ] PR_ATTR_DATA_OVLY(_g_rgdstrDrmTeeProperties) = {
    DRM_CREATE_DRM_ANSI_STRING( "0" ),  /* SUPPORTS_HEVC_HW_DECODING */
    DRM_CREATE_DRM_ANSI_STRING( "1" ),  /* SUPPORTS_REMOTE_PROVISIONING */
    DRM_CREATE_DRM_ANSI_STRING( "2" ),  /* SUPPORTS_PRE_PROCESS_ENCRYPTED_DATA */
    DRM_CREATE_DRM_ANSI_STRING( "3" ),  /* REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES */
    DRM_CREATE_DRM_ANSI_STRING( "4" ),  /* REQUIRES_SAMPLE_PROTECTION */
    DRM_CREATE_DRM_ANSI_STRING( "5" ),  /* SUPPORTS_SELWRE_CLOCK */
    DRM_CREATE_DRM_ANSI_STRING( "6" ),  /* SUPPORTS_SELWRE_STOP */
    DRM_CREATE_DRM_ANSI_STRING( "7" ),  /* SUPPORTS_SELWRE_HDCP_TYPE_1 */
    DRM_CREATE_DRM_ANSI_STRING( "8" ),  /* REQUIRES_PREPARE_POLICY_INFO */
#if DRM_DBG
    DRM_CREATE_DRM_ANSI_STRING( "9" ),  /* SUPPORTS_DEBUG_TRACING */
#else   /* DRM_DBG */
    DRM_CREATE_DRM_ANSI_STRING( "" ),   /* SUPPORTS_DEBUG_TRACING - note that this is always FALSE on FRE builds */
#endif  /* DRM_DBG */
    DRM_CREATE_DRM_ANSI_STRING( "10" ), /* REQUIRES_MINIMAL_REVOCATION_DATA */
    DRM_CREATE_DRM_ANSI_STRING( "11" ), /* SUPPORTS_OPTIMIZED_CONTENT_KEY2 */
};

DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_rgstrDrmTeeAPIs[ DRM_TEE_METHOD_FUNCTION_MAP_COUNT ] PR_ATTR_DATA_OVLY(_g_rgstrDrmTeeAPIs) = {
    DRM_CREATE_DRM_ANSI_STRING( "1" ),  /* DRM_TEE_BASE_FreeTEEContext */
    DRM_CREATE_DRM_ANSI_STRING( "2" ),  /* DRM_TEE_BASE_SignDataWithSelwreStoreKey */
    DRM_CREATE_DRM_ANSI_STRING( "3" ),  /* DRM_TEE_BASE_CheckDeviceKeys */
    DRM_CREATE_DRM_ANSI_STRING( "4" ),  /* DRM_TEE_BASE_GetDebugInformation */
    DRM_CREATE_DRM_ANSI_STRING( "5" ),  /* DRM_TEE_BASE_GenerateNonce */
    DRM_CREATE_DRM_ANSI_STRING( "6" ),  /* DRM_TEE_BASE_GetSystemTime */
    DRM_CREATE_DRM_ANSI_STRING( "7" ),  /* DRM_TEE_LPROV_GenerateDeviceKeys */
    DRM_CREATE_DRM_ANSI_STRING( "8" ),  /* DRM_TEE_RPROV_GenerateBootstrapChallenge */
    DRM_CREATE_DRM_ANSI_STRING( "9" ),  /* DRM_TEE_RPROV_ProcessBootstrapResponse */
    DRM_CREATE_DRM_ANSI_STRING( "10" ), /* DRM_TEE_RPROV_GenerateProvisioningRequest */
    DRM_CREATE_DRM_ANSI_STRING( "11" ), /* DRM_TEE_RPROV_ProcessProvisioningResponse */
    DRM_CREATE_DRM_ANSI_STRING( "12" ), /* DRM_TEE_LICPREP_PackageKey */
    DRM_CREATE_DRM_ANSI_STRING( "13" ), /* DRM_TEE_SAMPLEPROT_PrepareSampleProtectionKey */
    DRM_CREATE_DRM_ANSI_STRING( "14" ), /* DRM_TEE_DECRYPT_PreparePolicyInfo */
    DRM_CREATE_DRM_ANSI_STRING( "15" ), /* DRM_TEE_DECRYPT_PrepareToDecrypt */
    DRM_CREATE_DRM_ANSI_STRING( "16" ), /* DRM_TEE_DECRYPT_CreateOEMBlobFromCDKB */
    DRM_CREATE_DRM_ANSI_STRING( "17" ), /* DRM_TEE_AES128CTR_DecryptContent */
    DRM_CREATE_DRM_ANSI_STRING( "18" ), /* DRM_TEE_SIGN_SignHash */
    DRM_CREATE_DRM_ANSI_STRING( "19" ), /* DRM_TEE_DOM_PackageKeys */
    DRM_CREATE_DRM_ANSI_STRING( "20" ), /* DRM_TEE_RESERVED_20 */
    DRM_CREATE_DRM_ANSI_STRING( "21" ), /* DRM_TEE_RESERVED_21 */
    DRM_CREATE_DRM_ANSI_STRING( "22" ), /* DRM_TEE_RESERVED_22 */
    DRM_CREATE_DRM_ANSI_STRING( "23" ), /* DRM_TEE_RESERVED_23 */
    DRM_CREATE_DRM_ANSI_STRING( "24" ), /* DRM_TEE_REVOCATION_IngestRevocationInfo */
    DRM_CREATE_DRM_ANSI_STRING( "25" ), /* DRM_TEE_LICGEN_CompleteLicense */
    DRM_CREATE_DRM_ANSI_STRING( "26" ), /* DRM_TEE_LICGEN_AES128CTR_EncryptContent */
    DRM_CREATE_DRM_ANSI_STRING( "27" ), /* DRM_TEE_RESERVED_27 */
    DRM_CREATE_DRM_ANSI_STRING( "28" ), /* DRM_TEE_RESERVED_28 */
    DRM_CREATE_DRM_ANSI_STRING( "29" ), /* DRM_TEE_RESERVED_29 */
    DRM_CREATE_DRM_ANSI_STRING( "30" ), /* DRM_TEE_RESERVED_30 */
    DRM_CREATE_DRM_ANSI_STRING( "31" ), /* DRM_TEE_RESERVED_31 */
    DRM_CREATE_DRM_ANSI_STRING( "32" ), /* DRM_TEE_RESERVED_32 */
    DRM_CREATE_DRM_ANSI_STRING( "33" ), /* DRM_TEE_RESERVED_33 */
    DRM_CREATE_DRM_ANSI_STRING( "34" ), /* DRM_TEE_H264_PreProcessEncryptedData */
    DRM_CREATE_DRM_ANSI_STRING( "35" ), /* DRM_TEE_SELWRESTOP_GetGenerationID */
    DRM_CREATE_DRM_ANSI_STRING( "36" ), /* DRM_TEE_AES128CTR_DecryptAudioContentMultiple */
    DRM_CREATE_DRM_ANSI_STRING( "37" ), /* DRM_TEE_SELWRETIME_GenerateChallengeData */
    DRM_CREATE_DRM_ANSI_STRING( "38" ), /* DRM_TEE_SELWRETIME_ProcessResponseData */
    DRM_CREATE_DRM_ANSI_STRING( "39" ), /* DRM_TEE_AES128CTR_DecryptContentMultiple */
    DRM_CREATE_DRM_ANSI_STRING( "40" ), /* DRM_TEE_AES128CBC_DecryptContentMultiple */
    DRM_CREATE_DRM_ANSI_STRING( "41" ), /* DRM_TEE_SELWRESTOP2_GetSigningKeyBlob */
    DRM_CREATE_DRM_ANSI_STRING( "42" ), /* DRM_TEE_SELWRESTOP2_SignChallenge */
    DRM_CREATE_DRM_ANSI_STRING( "43" ), /* DRM_TEE_BASE_GetFeatureInformation */
};

DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrTeeOpenTag          PR_ATTR_DATA_OVLY(_g_dstrTeeOpenTag)          = DRM_CREATE_DRM_ANSI_STRING("<TEE>");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrTeeCloseTag         PR_ATTR_DATA_OVLY(_g_dstrTeeCloseTag)         = DRM_CREATE_DRM_ANSI_STRING("</TEE>");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrPropertyOpenTag     PR_ATTR_DATA_OVLY(_g_dstrPropertyOpenTag)     = DRM_CREATE_DRM_ANSI_STRING("<Prop Name=\"");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrPropertyCloseTag    PR_ATTR_DATA_OVLY(_g_dstrPropertyCloseTag)    = DRM_CREATE_DRM_ANSI_STRING("\"></Prop>");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrAPIOpenTag          PR_ATTR_DATA_OVLY(_g_dstrAPIOpenTag)          = DRM_CREATE_DRM_ANSI_STRING("<API Name=\"");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrAPICloseTag         PR_ATTR_DATA_OVLY(_g_dstrAPICloseTag)         = DRM_CREATE_DRM_ANSI_STRING("\"></API>");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrHardCodedAPIValue   PR_ATTR_DATA_OVLY(_g_dstrHardCodedAPIValue)   = DRM_CREATE_DRM_ANSI_STRING("<API Name=\"0\"></API>"); /* DRM_TEE_BASE_AllocTEEContext */
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrSignatureOpenTag    PR_ATTR_DATA_OVLY(_g_dstrSignatureOpenTag)    = DRM_CREATE_DRM_ANSI_STRING("<TEESignature Version=\"");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrSignatureCloseBrace PR_ATTR_DATA_OVLY(_g_dstrSignatureCloseBrace) = DRM_CREATE_DRM_ANSI_STRING("\">");
DRM_GLOBAL_CONST DRM_ANSI_CONST_STRING g_dstrSignatureCloseTag   PR_ATTR_DATA_OVLY(_g_dstrSignatureCloseTag)   = DRM_CREATE_DRM_ANSI_STRING("</TEESignature>");

#ifdef NONE
DRM_GLOBAL_CONST DRM_DISCARDABLE PUBKEY_P256   g_ECC256MSPlayReadyRootIssuerPubKeyTEE = DRM_ECC256_MS_PLAYREADY_ROOT_ISSUER_PUBKEY;
#endif

#if defined (SEC_COMPILE)
static DRM_FRE_INLINE DRM_RESULT DRM_CALL _ParseApplicationInfo(
    __in_opt              const DRM_TEE_BYTE_BLOB            *f_pApplicationInfo,
    __inout                     DRM_ID                       *f_pidApplication )
{
    DRM_RESULT                   dr                                 = DRM_SUCCESS;
    DRM_BYTE                     rgbUnused[sizeof(DRM_SIZE_T)];     /* memory for stack */
    DRM_STACK_ALLOCATOR_CONTEXT  stack;                             /* Initialized by DRM_STK_Init */
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
#endif

#ifdef NONE
static DRM_BOOL DRM_CALL _SupportedInBaseLibrary( DRM_VOID )
{
    return TRUE;
}


/* Used for deprecated APIs which are no longer supported by any TEE */
static DRM_BOOL DRM_CALL _NeverSupported( DRM_VOID )
{
    return FALSE;
}
#endif
typedef struct __tagTEEFunctionSupported
{
    DRM_METHOD_ID_DRM_TEE   eMethodID;
    DRM_BOOL                fSupported;
} TEEFunctionSupported;

// LWE (nkuo): function pointers are removed to help the static analysis
static DRM_GLOBAL_CONST TEEFunctionSupported s_rgTEEFunctionSupportedList[] PR_ATTR_DATA_OVLY(_s_rgTEEFunctionSupportedList) =
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
    { DRM_METHOD_ID_DRM_TEE_DECRYPT_PreparePolicyInfo,                          FALSE },
    { DRM_METHOD_ID_DRM_TEE_DECRYPT_PrepareToDecrypt,                           TRUE  },
    { DRM_METHOD_ID_DRM_TEE_DECRYPT_CreateOEMBlobFromCDKB,                      TRUE  },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContent,                           TRUE  },
    { DRM_METHOD_ID_DRM_TEE_SIGN_SignHash,                                      TRUE  },
    { DRM_METHOD_ID_DRM_TEE_DOM_PackageKeys,                                    TRUE  },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_20,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_21,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_22,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_23,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_REVOCATION_IngestRevocationInfo,                    TRUE  },
    { DRM_METHOD_ID_DRM_TEE_LICGEN_CompleteLicense,                             FALSE },
    { DRM_METHOD_ID_DRM_TEE_LICGEN_AES128CTR_EncryptContent,                    FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_27,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_28,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_29,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_30,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_31,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_32,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_RESERVED_33,                                        FALSE },
    { DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData,                       TRUE  },
    { DRM_METHOD_ID_DRM_TEE_SELWRESTOP_GetGenerationID,                         FALSE },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptAudioContentMultiple,              TRUE  },
    { DRM_METHOD_ID_DRM_TEE_SELWRETIME_GenerateChallengeData,                   FALSE },
    { DRM_METHOD_ID_DRM_TEE_SELWRETIME_ProcessResponseData,                     FALSE },
    { DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContentMultiple,                   TRUE  },
    { DRM_METHOD_ID_DRM_TEE_AES128CBC_DecryptContentMultiple,                   TRUE  },
    { DRM_METHOD_ID_DRM_TEE_SELWRESTOP2_GetSigningKeyBlob,                      FALSE },
    { DRM_METHOD_ID_DRM_TEE_SELWRESTOP2_SignChallenge,                          FALSE },
    { DRM_METHOD_ID_DRM_TEE_BASE_GetFeatureInformation,                         TRUE  },
};
#if defined(SEC_COMPILE)
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
    DRM_XB_BUILDER_CONTEXT      *pxbContextSystemInfo       = NULL;
    XB_DRM_TEE_PROXY_SYSINFO    *pxbSystemInfo              = NULL;
    DRM_DWORD                    cbVersion                  = 0;
    DRM_BYTE                    *pbVersion                  = NULL;

    /* All forms of decryption require the DECRYPT module. */
    if( DRM_TEE_IMPL_AES128CTR_IsAES128CTRSupported() || DRM_TEE_IMPL_AES128CBC_IsAES128CBCSupported() )
    {
        AssertChkBOOL( DRM_TEE_IMPL_DECRYPT_IsDECRYPTSupported() );
    }

    ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, DRM_TEE_METHOD_FUNCTION_MAP_COUNT * sizeof( DRM_DWORD ), (DRM_VOID**)&pdwFunctionMap ) );

    /* Even if we aren't returning the system info, we still validate that certain OEM-defined values are valid. */
    ChkDR( DRM_TEE_BASE_IMPL_GetVersionInformation(
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

    if( f_pSystemInfo != NULL )
    {
        ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, sizeof( *pxbContextSystemInfo ), (DRM_VOID**)&pxbContextSystemInfo ) );
        ChkVOID( OEM_TEE_ZERO_MEMORY( pxbContextSystemInfo, sizeof( *pxbContextSystemInfo ) ) );
        ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, sizeof( *pxbSystemInfo ), (DRM_VOID**)&pxbSystemInfo ) );
        ChkVOID( OEM_TEE_ZERO_MEMORY( pxbSystemInfo, sizeof( *pxbSystemInfo ) ) );

        cbStack = DRM_TEE_STUB_STACK_SIZE_FOR_SYSINFO;
        ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, cbStack, (DRM_VOID**)&pbStack ) );

        ChkDR( DRM_XB_StartFormat( pbStack, cbStack, DRM_TEE_PROXY_LWRRENT_VERSION, pxbContextSystemInfo, s_XB_DRM_TEE_PROXY_SYSINFO_FormatDescription ) );

        pxbSystemInfo->fValid = TRUE;

        /* Note: The application ID will be all zeroes if no application info was passed, but that's OK for a unique ID being created here. */
        pxbSystemInfo->BaseInfo.fValid = TRUE;
        ChkDR( DRM_TEE_IMPL_BASE_GetAppSpecificHWID( f_pOemTeeCtx, f_pidApplication, &pxbSystemInfo->BaseInfo.idUnique ) );

        ChkDR( DRM_TEE_IMPL_BASE_GetPKVersion(
            &pxbSystemInfo->BaseInfo.dwPKMajorVersion,
            &pxbSystemInfo->BaseInfo.dwPKMinorVersion,
            &pxbSystemInfo->BaseInfo.dwPKBuildVersion,
            &pxbSystemInfo->BaseInfo.dwPKQFEVersion ) );

        pxbSystemInfo->OEMInfo.fValid = TRUE;

        ChkDR( OEM_TEE_BASE_GetVersion(
            f_pOemTeeCtx,
            &pxbSystemInfo->OEMInfo.dwVersion ) );

        ChkDRAllowENOTIMPL( OEM_TEE_BASE_GetExtendedVersion(
            f_pOemTeeCtx,
            &cbVersion,
            &pbVersion ) );

        if( cbVersion != 0 )
        {
            pxbSystemInfo->OEMInfo2.fValid = TRUE;
            pxbSystemInfo->OEMInfo2.xbbaVersion2.cbData = cbVersion;
            pxbSystemInfo->OEMInfo2.xbbaVersion2.pbDataBuffer = pbVersion;
        }

        AssertChkBOOL( cchOEMManufacturerName > 0 && pwszOEMManufacturerName != NULL );
        AssertChkBOOL( cchOEMModelName > 0 && pwszOEMModelName != NULL );
        AssertChkBOOL( cchOEMModelNumber > 0 && pwszOEMModelNumber != NULL );
        AssertChkBOOL( cbSystemProperties > 0 && pbSystemProperties != NULL );

        pxbSystemInfo->OEMInfo.dwlFunctionMap.fValid = TRUE;
        pxbSystemInfo->OEMInfo.dwlFunctionMap.iDwords = 0;
        pxbSystemInfo->OEMInfo.dwlFunctionMap.cDWORDs = DRM_TEE_METHOD_FUNCTION_MAP_COUNT;
        pxbSystemInfo->OEMInfo.dwlFunctionMap.pdwordBuffer = (DRM_BYTE*)pdwFunctionMap;

        pxbSystemInfo->OEMInfo.xbbaManufacturerName.fValid = TRUE;
        pxbSystemInfo->OEMInfo.xbbaManufacturerName.iData = 0;
        pxbSystemInfo->OEMInfo.xbbaManufacturerName.cbData = cchOEMManufacturerName * sizeof( DRM_WCHAR );
        pxbSystemInfo->OEMInfo.xbbaManufacturerName.pbDataBuffer = (DRM_BYTE*)pwszOEMManufacturerName;

        pxbSystemInfo->OEMInfo.xbbaModelName.fValid = TRUE;
        pxbSystemInfo->OEMInfo.xbbaModelName.iData = 0;
        pxbSystemInfo->OEMInfo.xbbaModelName.cbData = cchOEMModelName * sizeof( DRM_WCHAR );
        pxbSystemInfo->OEMInfo.xbbaModelName.pbDataBuffer = (DRM_BYTE*)pwszOEMModelName;

        pxbSystemInfo->OEMInfo.xbbaModelNumber.fValid = TRUE;
        pxbSystemInfo->OEMInfo.xbbaModelNumber.iData = 0;
        pxbSystemInfo->OEMInfo.xbbaModelNumber.cbData = cchOEMModelNumber * sizeof( DRM_WCHAR );
        pxbSystemInfo->OEMInfo.xbbaModelNumber.pbDataBuffer = (DRM_BYTE*)pwszOEMModelNumber;

        pxbSystemInfo->OEMInfo.xbbaSystemProperties.fValid = TRUE;
        pxbSystemInfo->OEMInfo.xbbaSystemProperties.iData = 0;
        pxbSystemInfo->OEMInfo.xbbaSystemProperties.cbData = cbSystemProperties;
        pxbSystemInfo->OEMInfo.xbbaSystemProperties.pbDataBuffer = pbSystemProperties;

        ChkDR( DRM_XB_AddEntry( pxbContextSystemInfo, XB_OBJECT_GLOBAL_HEADER, pxbSystemInfo ) );
        
        dr = DRM_XB_FinishFormat( pxbContextSystemInfo, NULL, &cbSystemInfo );
        DRM_REQUIRE_BUFFER_TOO_SMALL( dr );
        ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW, cbSystemInfo, NULL, &oSystemInfo ) );
        ChkDR( DRM_XB_FinishFormat( pxbContextSystemInfo, (DRM_BYTE*)oSystemInfo.pb, &oSystemInfo.cb ) );

        ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pSystemInfo, &oSystemInfo ) );
    }

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pbStack ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pbSystemProperties ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pdwFunctionMap ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszOEMManufacturerName ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszOEMModelName ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszOEMModelNumber ) );

    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oSystemInfo );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID**)&pxbContextSystemInfo ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID**)&pbVersion ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID**)&pxbSystemInfo ) );
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
**                               This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
*/
#if DRM_DBG
#if TARGET_LITTLE_ENDIAN
static DRM_GLOBAL_CONST DRM_BYTE s_rgbEndianTest[] = { 0xee,0xff,0xaa,0xcc, };
#else   /* TARGET_LITTLE_ENDIAN */
static DRM_GLOBAL_CONST DRM_BYTE s_rgbEndianTest[] = { 0xcc,0xaa,0xff,0xee, };
#endif  /* TARGET_LITTLE_ENDIAN */
static DRM_GLOBAL_CONST DRM_DWORD s_dwEndiannessTest = 0xccaaffee;
#endif  /* DRM_DBG */
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_AllocTEEContext(
    __inout                     DRM_TEE_CONTEXT             **f_ppContext,
    __in_opt              const DRM_TEE_BYTE_BLOB            *f_pApplicationInfo,
    __out_opt                   DRM_TEE_BYTE_BLOB            *f_pSystemInfo )
{
    DRM_RESULT           dr             = DRM_SUCCESS;
    DRM_RESULT           drTmp          = DRM_SUCCESS;
    DRM_TEE_CONTEXT     *pContext       = NULL;
    OEM_TEE_CONTEXT     *pOemTeeCtx     = NULL;
    DRM_TEE_BYTE_BLOB    oSystemInfo    = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_ID               idApplication  = DRM_ID_EMPTY;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_ppContext   != NULL );

#if DRM_DBG
    /*
    ** Endianness enforcement.
    ** Fails initialization if platform Endianness does not match the one defined.
    ** This can save some debugging time.
    */
    DRMASSERT( OEM_SELWRE_ARE_EQUAL( &s_dwEndiannessTest, s_rgbEndianTest, sizeof( s_dwEndiannessTest ) ) );
    AssertChkBOOL( OEM_SELWRE_ARE_EQUAL( &s_dwEndiannessTest, s_rgbEndianTest, sizeof( s_dwEndiannessTest ) ) );
#endif

    /* Don't allocate if it has already been allocated by a previous call */
    if( *f_ppContext == NULL )
    {
        OEM_TEE_CONTEXT oTmpTEECtx = OEM_TEE_CONTEXT_EMPTY;

        ChkVOID( DRM_TEE_IMPL_BASE_DEBUG_Init() );
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

    ChkDR( _ParseApplicationInfo( f_pApplicationInfo, &idApplication ) );

    ChkDR( _ValidateAndInitializeSystemInfo( *f_ppContext, pOemTeeCtx, &idApplication, f_pSystemInfo == NULL ? NULL : &oSystemInfo ) );

    if( f_pSystemInfo != NULL )
    {
        ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pSystemInfo, &oSystemInfo ) );
    }

ErrorExit:
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( *f_ppContext, &oSystemInfo );
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }

    ChkVOID( DRM_TEE_BASE_FreeTEEContext( &pContext ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_IMPL_GetVersionInformation(
    __inout                                               OEM_TEE_CONTEXT          *f_pContext,
    __out                                                 DRM_DWORD                *f_pchManufacturerName,
    __deref_out_ecount( *f_pchManufacturerName )          DRM_WCHAR               **f_ppwszManufacturerName,
    __out                                                 DRM_DWORD                *f_pchModelName,
    __deref_out_ecount( *f_pchModelName )                 DRM_WCHAR               **f_ppwszModelName,
    __out                                                 DRM_DWORD                *f_pchModelNumber,
    __deref_out_ecount( *f_pchModelNumber )               DRM_WCHAR               **f_ppwszModelNumber,
    __out_ecount_opt( DRM_TEE_METHOD_FUNCTION_MAP_COUNT ) DRM_DWORD                *f_pdwFunctionMap,
    __out_opt                                             DRM_DWORD                *f_pcbProperties,
    __deref_opt_out_bcount( *f_pcbProperties )            DRM_BYTE                **f_ppbProperties )
{
    DRM_RESULT dr                    = DRM_SUCCESS;
    DRM_DWORD  idx                   = 0;
    DRM_BOOL   fCBCMultipleSupported = FALSE;
    DRM_BOOL   fCTRMultipleSupported = FALSE;

    ChkDR( OEM_TEE_BASE_GetVersionInformation(
        f_pContext,
        f_pchManufacturerName,
        f_ppwszManufacturerName,
        f_pchModelName,
        f_ppwszModelName,
        f_pchModelNumber,
        f_ppwszModelNumber,
        f_pdwFunctionMap,
        f_pcbProperties,
        f_ppbProperties ) );

#if !DRM_BUILD_PROFILE_IS_MICROSOFT_INTERNAL
    /* If not a test profile, cannot support both local and remote provisioning */
    AssertChkBOOL( DRM_TEE_IMPL_LPROV_IsLPROVSupported() != DRM_TEE_IMPL_RPROV_IsRPROVSupported() );

    /* If not a test profile, ensure that remote provisioning property matches whether the associated API(s) are supported */
    AssertChkBOOL( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_REMOTE_PROVISIONING ) == DRM_TEE_IMPL_RPROV_IsRPROVSupported() );
#endif  /* !DRM_BUILD_PROFILE_IS_MICROSOFT_INTERNAL */

#if OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE > 0
    DRM_TEE_PROPERTY_SET_VALUE( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_OPTIMIZED_CONTENT_KEY2 );
#else   /* OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE > 0 */
    DRM_TEE_PROPERTY_UNSET_VALUE( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_OPTIMIZED_CONTENT_KEY2 );
#endif  /* OEM_TEE_OPTIMIZED_CONTENT_KEY2_CACHE_SIZE > 0 */

    /* Debug tracing must be set if and only if this is a DRM_DBG build */
#if DRM_DBG
    AssertChkBOOL( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_DEBUG_TRACING ) );
#else   /* DRM_DBG */
    AssertChkBOOL( !DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_DEBUG_TRACING ) );
#endif  /* DRM_DBG */

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
        AssertChkBOOL( ( f_pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BITS_OEM_ALLOWED ) == f_pdwFunctionMap[ idx ] );

        /*
        ** Automatically mark as unsupported any API which has the stub version linked in.
        */
        DRMCASSERT( DRM_NO_OF( s_rgTEEFunctionSupportedList ) == DRM_METHOD_ID_DRM_TEE_Count );
        for( idx2 = 0; idx2 < DRM_NO_OF( s_rgTEEFunctionSupportedList ); idx2++ )
        {
            if( f_pdwFunctionMap[ idx - 1 ] == (DRM_DWORD)s_rgTEEFunctionSupportedList[ idx2 ].eMethodID )    /* Can't underflow because we are looping through positive odd values */
            {
                if( !s_rgTEEFunctionSupportedList[ idx2 ].fSupported )
                {
                    f_pdwFunctionMap[ idx ] |= DRM_TEE_PROXY_BIT_API_UNSUPPORTED;
                }
                break;
            }
        }
        AssertChkBOOL( idx2 < DRM_NO_OF( s_rgTEEFunctionSupportedList ) );

        if( ( f_pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BIT_API_UNSUPPORTED ) == 0 )
        {
            if( f_pdwFunctionMap[ idx - 1 ] == DRM_METHOD_ID_DRM_TEE_AES128CBC_DecryptContentMultiple )   /* Can't underflow because we are looping through positive odd values */
            {
                fCBCMultipleSupported = TRUE;
            }

            if( f_pdwFunctionMap[ idx - 1 ] == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContentMultiple )   /* Can't underflow because we are looping through positive odd values */
            {
                fCTRMultipleSupported = TRUE;
            }
        }

#if !DRM_BUILD_PROFILE_IS_MICROSOFT_INTERNAL

        /* If not a test profile, ensure that secure stop property matches whether the associated API(s) are supported */
        if( f_pdwFunctionMap[ idx - 1 ] == DRM_METHOD_ID_DRM_TEE_SELWRESTOP_GetGenerationID )   /* Can't underflow because we are looping through positive odd values */
        {
            if( f_pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BIT_API_UNSUPPORTED )
            {
                AssertChkBOOL( !DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_SELWRE_STOP ) );
            }
            else
            {
                AssertChkBOOL( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_SELWRE_STOP ) );
            }
        }

        /* If not a test profile, do not allow secure time APIs to be supported without the secure clock property being set */
        if( f_pdwFunctionMap[ idx - 1 ] == DRM_METHOD_ID_DRM_TEE_SELWRETIME_GenerateChallengeData   /* Can't underflow because we are looping through positive odd values */
         || f_pdwFunctionMap[ idx - 1 ] == DRM_METHOD_ID_DRM_TEE_SELWRETIME_ProcessResponseData )   /* Can't underflow because we are looping through positive odd values */
        {
            if( ( f_pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BIT_API_UNSUPPORTED ) == 0 )
            {
                AssertChkBOOL( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_SELWRE_CLOCK ) );
            }
        }

#endif  /* !DRM_BUILD_PROFILE_IS_MICROSOFT_INTERNAL */

        /*
        ** PreProcessEncryptedData contains two things.
        ** 1. Slice header parsing.
        ** 2. Additional OEM parsing on the full frame, e.g. transcryption.
        ** If additional OEM parsing is supported, this API MUST use structured serialization.
        ** We don't support non-structured serialization when the full frame is passed.
        */
        if( f_pdwFunctionMap[ idx - 1 ] == DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData )     /* Can't underflow because we are looping through positive odd values */
        {

#if !DRM_BUILD_PROFILE_IS_MICROSOFT_INTERNAL
            /* If not a test profile, ensure that PPED properties match whether the API is supported */
            if( f_pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BIT_API_UNSUPPORTED )
            {
                AssertChkBOOL( !DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_PRE_PROCESS_ENCRYPTED_DATA ) );
                AssertChkBOOL( !DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES ) );
            }
            else
            {
                AssertChkBOOL( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_PRE_PROCESS_ENCRYPTED_DATA ) );
            }
#endif  /* !DRM_BUILD_PROFILE_IS_MICROSOFT_INTERNAL */

            if( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES ) )
            {
                /* We fail if OEM_TEE_BASE_GetVersionInformation did not tell us to use structured serialization for full frames. */
                AssertChkBOOL( ( f_pdwFunctionMap[ idx ] & DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS ) == DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS );
            }
        }
    }

    /* Supporting CBC multiple requires support for CTR multiple and vice-versa to avoid overly complicating the user mode code */
    AssertChkBOOL( fCBCMultipleSupported == fCTRMultipleSupported );

    if( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_REQUIRES_PRE_PROCESS_ENCRYPTED_DATA_WITH_FULL_FRAMES ) )
    {
        AssertChkBOOL( DRM_TEE_PROPERTY_IS_SET( *f_pcbProperties, *f_ppbProperties, DRM_TEE_PROPERTY_SUPPORTS_PRE_PROCESS_ENCRYPTED_DATA ) );
    }

ErrorExit:
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
        ChkVOID( DRM_TEE_IMPL_BASE_DEBUG_DeInit() );

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
**                          This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
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

    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, NULL ) );

    ChkVOID( OEM_TEE_ZERO_MEMORY( f_pSignature, sizeof(*f_pSignature) ) );

    pSSTKeyWeakRef = DRM_TEE_IMPL_BASE_LocateKeyInPPKBWeakRef( &eType, cKeys, pKeys );

    ChkBOOL( pSSTKeyWeakRef != NULL, DRM_E_TEE_ILWALID_KEY_DATA );

    ChkDR( OEM_TEE_BASE_SignWithOMAC1( pOemTeeCtx, &pSSTKeyWeakRef->oKey, f_pDataToSign->cb, f_pDataToSign->pb, rgbSig ) );

    ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, sizeof(rgbSig), rgbSig, f_pSignature ) );

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
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
**  2. If the given PlayReady Private Key Blob (PPKB)
**     has a size of 0, return DRM_E_TEE_PROVISIONING_REQUIRED.
**  3. Parse the given PPKB and verify its signature.
**  4. If the TEE Key Identifier (TKID) [which references the
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
    DRM_RESULT           dr         = DRM_SUCCESS;
    DRM_DWORD            cKeys      = 0;
    DRM_TEE_KEY         *pKeys      = NULL;
    DRM_DWORD            dwidCTK;   /* Initialized by OEM_TEE_BASE_GetCTKID */
    DRM_TEE_KB_PPKBData *pPPKBData  = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pPPKB    != NULL );

    ChkDR( OEM_TEE_BASE_GetCTKID( &f_pContext->oContext, &dwidCTK ) );

    /*
    ** This check is redundant with the one in DRM_TEE_IMPL_KB_ParseAndVerifyPPKB, but we
    ** need to explicitly map this error case to DRM_E_TEE_PROVISIONING_REQUIRED. It is
    ** possible that this value changes prior to the TOCTOU protection before parsing, but it
    ** will fail anyway when it re-checks this value after TOCTOU protection.
    */
    ChkBOOL( f_pPPKB->cb > 0, DRM_E_TEE_PROVISIONING_REQUIRED );

    ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, sizeof( *pPPKBData ), (DRM_VOID**)&pPPKBData ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pPPKBData, sizeof( *pPPKBData ) ) );

    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, pPPKBData ) );

    /*
    ** If the TK used to generate TKDs to encrypt keys in the PPKB and sign the PPKB
    ** does not match the CTK, then CTK has been changed since we generated PPKB.
    ** Therefore, we are in a key history scenario and we need to re-provision.
    */
    ChkBOOL( pPPKBData->dwidTK == dwidCTK, DRM_E_TEE_PROVISIONING_REQUIRED );

ErrorExit:
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID**)&pPPKBData ) );
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

    ChkDR( DRM_TEE_IMPL_KB_BuildNKB( f_pContext, &idNonce, &oNKB ) );

    /* Transfer ownership to the output parameters */
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pNKB, &oNKB ) );
    *f_pNonce = idNonce;

ErrorExit:
    //
    // LWE (kwilson) - Can't use ChkDR within ErrorExit. Cache the return value in drTmp and check value
    // of dr, before overwriting to be sure we are not overwriting a previous failure with success.
    //
    drTmp = DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oNKB );
    if ( dr == DRM_SUCCESS )
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

/*
** Synopsis:
**
** This function generates XML which includes the TEE's supported properties and APIs.
** It then signs the supported properties and APIs and returns it as an XML string.
**
** Operations Performed:
**
** 1. Callwlates the length of the XML to be created
** 2. Fetches Property and API information and appends those to the XML
** 3. Signs the above XML and appends it between a new <TEESignature> tag
**
** Arguments:
**
** f_pContext:     (in/out) The TEE context returned from
**                          DRM_TEE_BASE_AllocTEEContext.
** f_pPPKB:        (in)     The current PlayReady Private Key Blob (PPKB)
** f_pTeeXml:      (out)    The XML including the property and API information
**                          as well as the signature.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_GetFeatureInformation(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_TEE_BYTE_BLOB            *f_pPPKB,
    __out                 DRM_TEE_BYTE_BLOB            *f_pTeeXml )
{
    DRM_RESULT            dr                    = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB     oTeeXml               = DRM_TEE_BYTE_BLOB_EMPTY;
    DRM_DWORD             cchManufacturerName   = 0;
    DRM_WCHAR            *pwszManufacturerName  = NULL;
    DRM_DWORD             cchModelName          = 0;
    DRM_WCHAR            *pwszModelName         = NULL;
    DRM_DWORD             cchModelNumber        = 0;
    DRM_WCHAR            *pwszModelNumber       = NULL;
    DRM_DWORD             cbProperties          = 0;
    DRM_BYTE             *pbProperties          = NULL;
    DRM_DWORD            *pdwFunctionMap        = NULL;
    DRM_DWORD             cKeys                 = 0;
    DRM_TEE_KEY          *pKeys                 = NULL;
    const DRM_TEE_KEY    *pSigningKeyWeakRef    = NULL;
    DRM_TEE_KEY_TYPE      eType                 = DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN;
    DRM_CHAR             *pchBSignature         = NULL;
    DRM_DWORD             cchBSignature         = CCH_BASE64_EQUIV( ECDSA_P256_SIGNATURE_SIZE_IN_BYTES );
    OEM_TEE_KEY           oEccSigningKey        = OEM_TEE_KEY_EMPTY;
    DRM_DWORD             cchXml                = 0;
    DRM_DWORD             cchSignature          = 0;
    DRM_DWORD             idx                   = 0;
    SIGNATURE_P256        oSignature;            /* OEM_TEE_ZERO_MEMORY */

    DRMCASSERT( DRM_TEE_PROPERTY_MAX == 11 ); /* Make sure the g_rgstrDrmTeeProperties array gets updated if the count changes */
    DRMCASSERT( DRM_METHOD_ID_DRM_TEE_Count == 44 ); /* Make sure the g_rgstrDrmTeeAPIs array gets updated if the count changes */

    ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, DRM_TEE_METHOD_FUNCTION_MAP_COUNT * sizeof( DRM_DWORD ), (DRM_VOID**)&pdwFunctionMap ) );

    ChkDR( DRM_TEE_BASE_IMPL_GetVersionInformation(
        &f_pContext->oContext,
        &cchManufacturerName,
        &pwszManufacturerName,
        &cchModelName,
        &pwszModelName,
        &cchModelNumber,
        &pwszModelNumber,
        pdwFunctionMap,
        &cbProperties,
        &pbProperties ) );

    /* Count the number of characters in the xml being generated */
    cchSignature = /* size of all the TEESignature information */
        g_dstrSignatureOpenTag.cchString
      + TEE_SIGNATURE_VERSION_LENGTH
      + g_dstrSignatureCloseBrace.cchString
      + CCH_BASE64_EQUIV( ECDSA_P256_SIGNATURE_SIZE_IN_BYTES )
      + g_dstrSignatureCloseTag.cchString
      + 1;

    cchXml = /* size of all the constant tags within and including the TEE tags initialized above */
        g_dstrTeeOpenTag.cchString  
      + g_dstrTeeCloseTag.cchString
      + g_dstrHardCodedAPIValue.cchString
      + cchSignature;

    /* Sum the length of all the Properties that are set */
    for( idx = 0; idx < DRM_TEE_PROPERTY_MAX + 1; idx++ )
    {
        if( DRM_TEE_PROPERTY_IS_SET( cbProperties, pbProperties, idx ) && g_rgdstrDrmTeeProperties[ idx ].cchString > 0 )
        {
            cchXml += g_dstrPropertyOpenTag.cchString +
                      g_rgdstrDrmTeeProperties[idx].cchString +
                      g_dstrPropertyCloseTag.cchString;
        }
    }

    /* Sum the length of all the APIs that are supported */
    for( idx = 0; idx < DRM_TEE_METHOD_FUNCTION_MAP_COUNT; idx += 2 )
    {
        if( ( pdwFunctionMap[ idx + 1 ] & DRM_TEE_PROXY_BIT_API_UNSUPPORTED ) == 0 )
        {
            cchXml += g_dstrAPIOpenTag.cchString + 
                      g_rgstrDrmTeeAPIs[idx / 2].cchString +
                      g_dstrAPICloseTag.cchString;
        }
    }

    ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW, cchXml, NULL, &oTeeXml ) );
    OEM_TEE_ZERO_MEMORY( (DRM_VOID*)oTeeXml.pb, cchXml );
    
    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrTeeOpenTag.pszString ) );

    /* fill <Property Name="foo"></Property> */
    for( idx = 0; idx < DRM_TEE_PROPERTY_MAX + 1; idx++ )
    {
        if( DRM_TEE_PROPERTY_IS_SET( cbProperties, pbProperties, idx ) && g_rgdstrDrmTeeProperties[ idx ].cchString > 0 )
        {
            ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrPropertyOpenTag.pszString ) );
            ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_rgdstrDrmTeeProperties[idx].pszString ) );
            ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrPropertyCloseTag.pszString ) );
        }
    }

    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrHardCodedAPIValue.pszString ) );

    /* fill <API Name="foo"></API> */
    for( idx = 0; idx < DRM_TEE_METHOD_FUNCTION_MAP_COUNT; idx += 2 )
    {
        if( ( pdwFunctionMap[ idx + 1 ] & DRM_TEE_PROXY_BIT_API_UNSUPPORTED ) == 0 )
        {
            ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrAPIOpenTag.pszString ) );
            ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_rgstrDrmTeeAPIs[idx / 2].pszString ) );
            ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrAPICloseTag.pszString ) );
        }
    }

    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrTeeCloseTag.pszString ) );

    /* 
    ** Fetch the private key information from the PlayReady Private Key Blob and generate the
    ** new ECC Signing Key from it 
    */
    ChkDR( DRM_TEE_IMPL_KB_ParseAndVerifyPPKB( f_pContext, f_pPPKB, &cKeys, &pKeys, NULL ) );

    pSigningKeyWeakRef = DRM_TEE_IMPL_BASE_LocateKeyInPPKBWeakRef( &eType, cKeys, pKeys );
    ChkBOOL( pSigningKeyWeakRef != NULL, DRM_E_TEE_ILWALID_KEY_DATA );
    
    OEM_TEE_ZERO_MEMORY( &oSignature, ECDSA_P256_SIGNATURE_SIZE_IN_BYTES );

    ChkDR( OEM_TEE_BASE_AllocKeyECC256( &f_pContext->oContext, &oEccSigningKey ) );
    ChkDR( OEM_TEE_BASE_ECC256_GenerateTeeSigningPrivateKey( &f_pContext->oContext, &pSigningKeyWeakRef->oKey, &oEccSigningKey ) );

    /* Signs the data in between and including <TEE> and </TEE> and then Base64 encode the signature */
    ChkDR( OEM_TEE_BASE_ECDSA_P256_SignData(
        &f_pContext->oContext,
        oTeeXml.cb - cchSignature,
        oTeeXml.pb,
        &oEccSigningKey,
        &oSignature ) );

    ChkDR( DRM_TEE_IMPL_BASE_MemAlloc( f_pContext, cchBSignature + 1, (DRM_VOID **)&pchBSignature ) );
    OEM_TEE_ZERO_MEMORY( pchBSignature, cchBSignature + 1 );

    ChkDR( DRM_B64_EncodeA(
        oSignature.m_rgbSignature,
        ECDSA_P256_SIGNATURE_SIZE_IN_BYTES,
        pchBSignature,
        &cchBSignature,
        0 ) );

    /* <TEESignature version="{VERSION #}">{BASE 64 Signature}</TEESignature> */
    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrSignatureOpenTag.pszString ) );
    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, (DRM_CHAR *)TEE_SIGNATURE_VERSION ) );
    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, (DRM_CHAR *)g_dstrSignatureCloseBrace.pszString ) );
    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, pchBSignature ) );
    ChkDR( DRM_STR_StringCchCatA( (DRM_CHAR *)oTeeXml.pb, cchXml, g_dstrSignatureCloseTag.pszString ) );
    
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pTeeXml, &oTeeXml ) );

ErrorExit:
    DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **) &pchBSignature );
    ChkVOID( OEM_TEE_BASE_FreeKeyECC256( &f_pContext->oContext, &oEccSigningKey ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, cKeys, &pKeys ) );
    ChkVOID( DRM_TEE_IMPL_BASE_FreeBlob( f_pContext, &oTeeXml ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pdwFunctionMap ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pbProperties ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszManufacturerName ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszModelName ) );
    ChkVOID( DRM_TEE_IMPL_BASE_MemFree( f_pContext, (DRM_VOID **)&pwszModelNumber ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_ValidateLicenseExpiration(
    __inout               DRM_TEE_CONTEXT              *f_pContext,
    __in            const DRM_XMRFORMAT                *f_pLicense,
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

        if( dr == DRM_E_NOTIMPL )
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

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_CheckLicenseSignature(
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

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocateKeyArray(
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
    ChkVOID( DRM_TEE_IMPL_BASE_FreeKeyArray( f_pContext, f_cKeys, &pKeys ) );
    return dr;
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_FreeKeyArray(
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
            case DRM_TEE_KEY_TYPE_TK:                               __fallthrough;
            case DRM_TEE_KEY_TYPE_TKD:                              __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_SST:                           __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_CI:                            __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_CK:                            __fallthrough;
            case DRM_TEE_KEY_TYPE_RPROV_SESSION_KEY:                __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_MI:                            __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_MK:                            __fallthrough;
            case DRM_TEE_KEY_TYPE_SAMPLEPROT:                       __fallthrough;
            case DRM_TEE_KEY_TYPE_DOMAIN_SESSION:                   __fallthrough;
            case DRM_TEE_KEY_TYPE_AES128_DERIVATION:                __fallthrough;
            case DRM_TEE_KEY_TYPE_AES128_SELWRESTOP2:               __fallthrough;
                ChkVOID( OEM_TEE_BASE_FreeKeyAES128( pOemTeeCtx, &pKeys[ iKey ].oKey ) );
                break;
            case DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_SIGN:              __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_ENCRYPT:           __fallthrough;
            case DRM_TEE_KEY_TYPE_PR_ECC_PRIVKEY_PRND_DEPRECATED:   __fallthrough;
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

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_IMPL_BASE_GetKeyDerivationIDSign(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType )
{
    DRM_ID idKeyDerivation = DRM_ID_EMPTY;

    switch( f_eType )
    {
    case DRM_TEE_XB_KB_TYPE_PPKB : idKeyDerivation = g_idKeyDerivationSignPPKB ; break;
    case DRM_TEE_XB_KB_TYPE_LKB  : idKeyDerivation = g_idKeyDerivationSignLKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CDKB : idKeyDerivation = g_idKeyDerivationSignCDKB ; break;
    case DRM_TEE_XB_KB_TYPE_DKB  : idKeyDerivation = g_idKeyDerivationSignDKB  ; break;
    case DRM_TEE_XB_KB_TYPE_RKB  : idKeyDerivation = g_idKeyDerivationSignRKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CEKB : idKeyDerivation = g_idKeyDerivationSignCEKB ; break;
    case DRM_TEE_XB_KB_TYPE_TPKB : idKeyDerivation = g_idKeyDerivationSignTPKB ; break;
    case DRM_TEE_XB_KB_TYPE_SPKB : idKeyDerivation = g_idKeyDerivationSignSPKB ; break;
    case DRM_TEE_XB_KB_TYPE_NKB  : idKeyDerivation = g_idKeyDerivationSignNKB  ; break;
    case DRM_TEE_XB_KB_TYPE_NTKB : idKeyDerivation = g_idKeyDerivationSignNTKB ; break;
    case DRM_TEE_XB_KB_TYPE_RPCKB: idKeyDerivation = g_idKeyDerivationSignRPCKB; break;
    case DRM_TEE_XB_KB_TYPE_LPKB : idKeyDerivation = g_idKeyDerivationSignLPKB ; break;
    case DRM_TEE_XB_KB_TYPE_SSKB : idKeyDerivation = g_idKeyDerivationSignSSKB ; break;
    default                      : DRMASSERT( FALSE )                          ; break;
    }

    return idKeyDerivation;
}

DRM_NO_INLINE DRM_API DRM_ID DRM_CALL DRM_TEE_IMPL_BASE_GetKeyDerivationIDEncrypt(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType )
{
    DRM_ID idKeyDerivation = DRM_ID_EMPTY;

    switch( f_eType )
    {
    case DRM_TEE_XB_KB_TYPE_PPKB : idKeyDerivation = g_idKeyDerivationEncryptPPKB ; break;
    case DRM_TEE_XB_KB_TYPE_LKB  : idKeyDerivation = g_idKeyDerivationEncryptLKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CDKB : idKeyDerivation = g_idKeyDerivationEncryptCDKB ; break;
    case DRM_TEE_XB_KB_TYPE_DKB  : idKeyDerivation = g_idKeyDerivationEncryptDKB  ; break;
    case DRM_TEE_XB_KB_TYPE_RKB  : idKeyDerivation = g_idKeyDerivationEncryptRKB  ; break;
    case DRM_TEE_XB_KB_TYPE_CEKB : idKeyDerivation = g_idKeyDerivationEncryptCEKB ; break;
    case DRM_TEE_XB_KB_TYPE_TPKB : idKeyDerivation = g_idKeyDerivationEncryptTPKB ; break;
    case DRM_TEE_XB_KB_TYPE_SPKB : idKeyDerivation = g_idKeyDerivationEncryptSPKB ; break;
    case DRM_TEE_XB_KB_TYPE_NKB  : idKeyDerivation = g_idKeyDerivationEncryptNKB  ; break;
    case DRM_TEE_XB_KB_TYPE_NTKB : idKeyDerivation = g_idKeyDerivationEncryptNTKB ; break;
    case DRM_TEE_XB_KB_TYPE_RPCKB: idKeyDerivation = g_idKeyDerivationEncryptRPCKB; break;
    case DRM_TEE_XB_KB_TYPE_LPKB : idKeyDerivation = g_idKeyDerivationEncryptLPKB ; break;
    case DRM_TEE_XB_KB_TYPE_SSKB : idKeyDerivation = g_idKeyDerivationEncryptSSKB ; break;
    default                      : DRMASSERT( FALSE )                             ; break;
    }

    return idKeyDerivation;
}

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_BASE_IsKBTypePersistedToDisk(
    __in                                                  DRM_TEE_XB_KB_TYPE          f_eType )
{
    DRM_BOOL fPersist = FALSE;

    switch( f_eType )
    {
    case DRM_TEE_XB_KB_TYPE_PPKB : fPersist = TRUE   ; break;
    case DRM_TEE_XB_KB_TYPE_DKB  : fPersist = TRUE   ; break;
    case DRM_TEE_XB_KB_TYPE_TPKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_CDKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_CEKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_RKB  : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_SPKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_RPCKB: fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_NKB  : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_NTKB : fPersist = FALSE  ; break;
    case DRM_TEE_XB_KB_TYPE_LPKB : fPersist = TRUE   ; break;
    case DRM_TEE_XB_KB_TYPE_SSKB : fPersist = TRUE   ; break;
        /* caller should always know LKB type */
    case DRM_TEE_XB_KB_TYPE_LKB  : __fallthrough;
    default                      : DRMASSERT( FALSE ); break;
    }

    return fPersist;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_DeriveTKDFromTK(
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
    DRM_ID           idKeyDerivation  = DRM_ID_EMPTY;
    DRM_BOOL         fPersist         = FALSE;
    DRM_TEE_KEY      oCTK             = DRM_TEE_KEY_EMPTY;
    OEM_TEE_CONTEXT *pOemTeeCtx       = NULL;

    /* Arguments are checked in the PRITEE proxy layer */
    DRMASSERT( f_pContext != NULL );
    DRMASSERT( f_pTKD     != NULL );

    pOemTeeCtx = &f_pContext->oContext;

    if( f_pfPersist == NULL )
    {
        fPersist = DRM_TEE_IMPL_BASE_IsKBTypePersistedToDisk( f_eType );
    }
    else
    {
        fPersist = *f_pfPersist;
    }

    if( f_eOperation == DRM_TEE_XB_KB_OPERATION_SIGN )
    {
        idKeyDerivation = DRM_TEE_IMPL_BASE_GetKeyDerivationIDSign( f_eType );
    }
    else if( f_eOperation == DRM_TEE_XB_KB_OPERATION_ENCRYPT )
    {
        idKeyDerivation = DRM_TEE_IMPL_BASE_GetKeyDerivationIDEncrypt( f_eType );
    }
    else
    {
        AssertLogicError();
    }

    /* Use CTK if no PTK is specified */
    if( f_pTK == NULL )
    {
        ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_TK, &oCTK ) );
        ChkDR( OEM_TEE_BASE_GetCTKID( pOemTeeCtx, &oCTK.dwidTK ) );
        ChkDR( OEM_TEE_BASE_GetTKByID( pOemTeeCtx, oCTK.dwidTK, &oCTK.oKey ) );
        f_pTK = &oCTK;
    }
    else
    {
        DRMASSERT( f_pTK->eType == DRM_TEE_KEY_TYPE_TK );
    }

    ChkDR( DRM_TEE_IMPL_BASE_AllocKeyAES128( pOemTeeCtx, DRM_TEE_KEY_TYPE_TKD, f_pTKD ) );
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

DRM_NO_INLINE DRM_API DRM_TEE_KEY* DRM_CALL DRM_TEE_IMPL_BASE_LocateKeyInPPKBWeakRef(
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

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_GetPKVersion(
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

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_CopyDwordPairsToAlignedBufferAndSwapEndianness(
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

    ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW, f_pSource->cb, NULL, &oDest ) );

    for( ib = 0; ib < f_pSource->cb; ib += sizeof(DRM_DWORD) )
    {
        /* oDest.pb must already be aligned because we allocated it just above. */
        DRM_DWORD *pdwDest = (DRM_DWORD*)&oDest.pb[ib];
        DRM_BIG_ENDIAN_BYTES_TO_NATIVE_DWORD( *pdwDest, &f_pSource->pb[ib] );
    }
    ChkVOID( DRM_TEE_IMPL_BASE_TransferBlobOwnership( f_pDest, &oDest ) );

#else   /* TARGET_LITTLE_ENDIAN */

    /* f_pSource must have a multiple of 2 DWORDs. */
    ChkArg( ( f_pSource->cb & (DRM_DWORD)( 2 * sizeof(DRM_DWORD) - 1 ) ) == 0 );
    ChkDR( DRM_TEE_IMPL_BASE_AllocBlob( f_pContext, DRM_TEE_BLOB_ALLOC_BEHAVIOR_COPY, f_pSource->cb, f_pSource->pb, f_pDest ) );

#endif  /* TARGET_LITTLE_ENDIAN */

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocKeyAES128(
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

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AllocKeyECC256(
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

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_GetAppSpecificHWID(
    __inout                                         OEM_TEE_CONTEXT            *f_pOemTeeCtx,
    __in                                      const DRM_ID                     *f_pidApplication,
    __out                                           DRM_ID                     *f_pidHWID )
{
    DRM_RESULT               dr                 = DRM_SUCCESS;
    DRM_DWORD               *pdwTemp            = (DRM_DWORD*)f_pidHWID;
    DRM_ID                   oCertClientID; /*Initialized in OEM_TEE_BASE_GetDeviceUniqueID*/
    OEM_SHA256_CONTEXT       sha256Context;
    OEM_SHA256_DIGEST        oDigest;
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
        (const DRM_BYTE*)&oCertClientID ) );
    ChkDR( OEM_TEE_BASE_SHA256_Update(
        pOemTeeCtx,
        &sha256Context,
        sizeof(*f_pidApplication),
        (const DRM_BYTE*)f_pidApplication ) );

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


DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_AES128CBC(
    __inout                                                                      DRM_TEE_CONTEXT            *f_pContext,
    __in                                                                         DRM_DWORD                   f_cbBlob,
    __in_bcount( f_cbBlob )                                                const DRM_BYTE                   *f_pbBlob,
    __in                                                                   const DRM_TEE_KEY                *f_poKey,
    __inout                                                                      DRM_DWORD                  *f_pcbEncryptedBlob,
    __out_bcount( DRM_AESCBC_ENCRYPTED_SIZE_W_IV_NOT_OVERFLOW_SAFE( f_cbBlob ) ) DRM_BYTE                   *f_pbEncryptedBlob )
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

    ChkArg( ( f_cbBlob < DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ) - 3 * DRM_AES_BLOCKLEN )
         && ( DRM_AESCBC_ENCRYPTED_SIZE_W_IV_NOT_OVERFLOW_SAFE( f_cbBlob ) <= *f_pcbEncryptedBlob ) );      /* Can't overflow: Verified size on previous line */

    pOemTeeCtx = &f_pContext->oContext;

    ChkDR( OEM_TEE_BASE_GenerateRandomBytes(
        pOemTeeCtx,
        DRM_AES_BLOCKLEN,
        f_pbEncryptedBlob ) );

    ChkVOID( OEM_TEE_MEMCPY( f_pbEncryptedBlob + DRM_AES_BLOCKLEN, f_pbBlob, f_cbBlob ) );      /* Can't overflow: Already verified minimum size */

    pbPadding = f_pbEncryptedBlob + DRM_AES_BLOCKLEN + f_cbBlob;
    bPadding = DRM_AES_BLOCKLEN - f_cbBlob % DRM_AES_BLOCKLEN;

    for( cbPadding = 0; cbPadding < bPadding; cbPadding++ )
    {
        *pbPadding++ = bPadding;
    }

    *f_pcbEncryptedBlob = ( f_cbBlob + 2 * DRM_AES_BLOCKLEN ) - f_cbBlob % DRM_AES_BLOCKLEN;    /* Can't overflow: Already verified minimum size */

    ChkDR( OEM_TEE_BASE_AES128CBC_EncryptData(
        pOemTeeCtx,
        &f_poKey->oKey,
        f_pbEncryptedBlob,                          /* IV */
        *f_pcbEncryptedBlob - DRM_AES_BLOCKLEN,     /* length to be encrypted */            /* Can't overflow: Already verified minimum size */
        f_pbEncryptedBlob + DRM_AES_BLOCKLEN ) );   /* pointer to items to be encrypted*/   /* Can't overflow: Already verified minimum size */

ErrorExit:
    return dr;
}

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
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_ParseCertificateChain(
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
            f_pChainData,
            FALSE );

    } while( dr == DRM_E_OUTOFMEMORY );

    ChkDR( dr );

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pOemTeeContext, (DRM_VOID **)&pbStack ) );
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_IMPL_BASE_ParseLicenseAlloc(
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

