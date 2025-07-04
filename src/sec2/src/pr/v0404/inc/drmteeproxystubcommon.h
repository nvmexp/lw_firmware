/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEEPROXYSTUBCOMMON_H_
#define _DRMTEEPROXYSTUBCOMMON_H_ 1

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

#define DRM_TEE_PROXY_MESSAGE_RESPONSE_OVERHEAD 1024
#define DRM_TEE_PROXY_MESSAGE_OVERHEAD_MIN      128
#define DRM_TEE_PROXY_MAX_MESSAGE_SIZE_DEFAULT  (12 * 1024)

/*
** IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT
**
** All values in this enum MUST be referenced by OEM_TEE_BASE_GetVersionInformation.
** When this enum changes, the following MUST be updated:
**  DRM_METHOD_ID_DRM_TEE_Count
**  OEM_TEE_BASE_GetVersionInformation
**  Any test cases that do verification of this data.
** This enum MUST not be changed without a firmware update to match.
**
** IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT
*/
typedef enum _tagDRM_METHOD_ID_DRM_TEE
{
    /*
    ** The following enum value DRM_METHOD_ID_DRM_TEE_BASE_AllocTEEContext
    ** is a fixed value and must not change.
    */
    DRM_METHOD_ID_DRM_TEE_BASE_AllocTEEContext                                  = 0,

    /* Value to indicate the start of the IDs with OEM function map values */
    DRM_METHOD_ID_START_OF_OEM_FUNCTION_MAPPED_IDS                              = 1,

    DRM_METHOD_ID_DRM_TEE_BASE_FreeTEEContext                                   = 1,
    DRM_METHOD_ID_DRM_TEE_BASE_SignDataWithSelwreStoreKey                       = 2,
    DRM_METHOD_ID_DRM_TEE_BASE_CheckDeviceKeys                                  = 3,
    DRM_METHOD_ID_DRM_TEE_BASE_GetDebugInformation                              = 4,
    DRM_METHOD_ID_DRM_TEE_BASE_GenerateNonce                                    = 5,
    DRM_METHOD_ID_DRM_TEE_BASE_GetSystemTime                                    = 6,
    DRM_METHOD_ID_DRM_TEE_LPROV_GenerateDeviceKeys                              = 7,
    DRM_METHOD_ID_DRM_TEE_RPROV_GenerateBootstrapChallenge                      = 8,
    DRM_METHOD_ID_DRM_TEE_RPROV_ProcessBootstrapResponse                        = 9,
    DRM_METHOD_ID_DRM_TEE_RPROV_GenerateProvisioningRequest                     = 10,
    DRM_METHOD_ID_DRM_TEE_RPROV_ProcessProvisioningResponse                     = 11,
    DRM_METHOD_ID_DRM_TEE_LICPREP_PackageKey                                    = 12,
    DRM_METHOD_ID_DRM_TEE_SAMPLEPROT_PrepareSampleProtectionKey                 = 13,
    DRM_METHOD_ID_DRM_TEE_DECRYPT_PreparePolicyInfo                             = 14,   /* Formerly DRM_METHOD_ID_DRM_TEE_AES128CTR_PreparePolicyInfo     */
    DRM_METHOD_ID_DRM_TEE_DECRYPT_PrepareToDecrypt                              = 15,   /* Formerly DRM_METHOD_ID_DRM_TEE_AES128CTR_PrepareToDecrypt      */
    DRM_METHOD_ID_DRM_TEE_DECRYPT_CreateOEMBlobFromCDKB                         = 16,   /* Formerly DRM_METHOD_ID_DRM_TEE_AES128CTR_CreateOEMBlobFromCDKB */
    DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContent                              = 17,
    DRM_METHOD_ID_DRM_TEE_SIGN_SignHash                                         = 18,
    DRM_METHOD_ID_DRM_TEE_DOM_PackageKeys                                       = 19,
    DRM_METHOD_ID_DRM_TEE_RESERVED_20                                           = 20,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDRX_ProcessRegistrationResponseMessage */
    DRM_METHOD_ID_DRM_TEE_RESERVED_21                                           = 21,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDRX_GenerateProximityResponseNonce     */
    DRM_METHOD_ID_DRM_TEE_RESERVED_22                                           = 22,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDRX_CompleteLicenseRequestMessage      */
    DRM_METHOD_ID_DRM_TEE_RESERVED_23                                           = 23,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDRX_ProcessLicenseTransmitMessage      */
    DRM_METHOD_ID_DRM_TEE_REVOCATION_IngestRevocationInfo                       = 24,
    DRM_METHOD_ID_DRM_TEE_LICGEN_CompleteLicense                                = 25,
    DRM_METHOD_ID_DRM_TEE_LICGEN_AES128CTR_EncryptContent                       = 26,
    DRM_METHOD_ID_DRM_TEE_RESERVED_27                                           = 27,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDTX_ProcessRegistrationRequestMessage        */
    DRM_METHOD_ID_DRM_TEE_RESERVED_28                                           = 28,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDTX_CompleteRegistrationResponseMessage      */
    DRM_METHOD_ID_DRM_TEE_RESERVED_29                                           = 29,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDTX_GenerateProximityDetectionChallengeNonce */
    DRM_METHOD_ID_DRM_TEE_RESERVED_30                                           = 30,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDTX_VerifyProximityDetectionResponseNonce    */
    DRM_METHOD_ID_DRM_TEE_RESERVED_31                                           = 31,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDTX_ProcessLicenseRequestMessage             */
    DRM_METHOD_ID_DRM_TEE_RESERVED_32                                           = 32,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDTX_CompleteLicenseTransmitMessage           */
    DRM_METHOD_ID_DRM_TEE_RESERVED_33                                           = 33,   /* No longer supported.  Was DRM_METHOD_ID_DRM_TEE_PRNDTX_RebindLicenseToReceiver                  */
    DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData                          = 34,
    DRM_METHOD_ID_DRM_TEE_SELWRESTOP_GetGenerationID                            = 35,
    DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptAudioContentMultiple                 = 36,
    DRM_METHOD_ID_DRM_TEE_SELWRETIME_GenerateChallengeData                      = 37,
    DRM_METHOD_ID_DRM_TEE_SELWRETIME_ProcessResponseData                        = 38,
    DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContentMultiple                      = 39,
    DRM_METHOD_ID_DRM_TEE_AES128CBC_DecryptContentMultiple                      = 40,
    DRM_METHOD_ID_DRM_TEE_SELWRESTOP2_GetSigningKeyBlob                         = 41,
    DRM_METHOD_ID_DRM_TEE_SELWRESTOP2_SignChallenge                             = 42,
    DRM_METHOD_ID_DRM_TEE_BASE_GetFeatureInformation                            = 43,
    DRM_METHOD_ID_DRM_TEE_Count                                                 = 44,

    DRM_METHOD_ID_RESERVED0                                                     = 0x000F0000,
    DRM_METHOD_ID_RESERVED1                                                     = 0x000F0001,
    DRM_METHOD_ID_RESERVED2                                                     = 0x000F0002,
    DRM_METHOD_ID_RESERVED3                                                     = 0x000F0003,
    DRM_METHOD_ID_RESERVED4                                                     = 0x000F0004,
    DRM_METHOD_ID_RESERVED5                                                     = 0x000F0005,

} DRM_METHOD_ID_DRM_TEE;

#define DRM_TEE_PROXY_METHOD_ID(m) DRM_METHOD_ID_##m

/* Only certain methods support structured serialization. */
#define DRM_METHOD_ID_DRM_TEE_SUPPORTS_STRUCTURE_SERIALIZATION( eID )               \
    ( ( ( eID ) == DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData          )    \
   || ( ( eID ) == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptAudioContentMultiple )    \
   || ( ( eID ) == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContent              )    \
   || ( ( eID ) == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContentMultiple      )    \
   || ( ( eID ) == DRM_METHOD_ID_DRM_TEE_AES128CBC_DecryptContentMultiple      ) )

/*
** All TEE APIs should generate a request/response message with a size no bigger than
** OEM_TEE_PROXY_MAX_PRITEE_MESSAGE_SIZE (defined in oemteeproxy.h).  The only exceptions
** are the methods below until either TEE shared memory support is added
** or structured serialization is only allowed for these functions.
*/
#define DRM_TEE_PROXY_METHOD_IS_MESSAGE_SIZE_EXCEPTIONS(eMethodID)                      \
     ( ( (eMethodID) == DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData          )   \
    || ( (eMethodID) == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContent              )   \
    || ( (eMethodID) == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptAudioContentMultiple )   \
    || ( (eMethodID) == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContentMultiple      )   \
    || ( (eMethodID) == DRM_METHOD_ID_DRM_TEE_AES128CBC_DecryptContentMultiple      )   \
    || ( (eMethodID) == DRM_METHOD_ID_DRM_TEE_LICGEN_AES128CTR_EncryptContent       ) )

#define DRM_TEE_PROXY_PARAMETER_TYPE__ILWALID               ( ( DRM_DWORD ) -1 )
#define DRM_TEE_PROXY_PARAMETER_TYPE__DRM_DWORD             ( ( DRM_DWORD ) 0  )
#define DRM_TEE_PROXY_PARAMETER_TYPE__DRM_QWORD             ( ( DRM_DWORD ) 1  )
#define DRM_TEE_PROXY_PARAMETER_TYPE__DRM_ID                ( ( DRM_DWORD ) 2  )
#define DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_BYTE_BLOB     ( ( DRM_DWORD ) 3  )
#define DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_DWORDLIST     ( ( DRM_DWORD ) 4  )
#define DRM_TEE_PROXY_PARAMETER_TYPE__DRM_TEE_QWORDLIST     ( ( DRM_DWORD ) 5  )

typedef DRM_DWORD DRM_TEE_PROXY_PARAMETER_TYPE;

/*
** The function map does not include the fixed method ID value for DRM_TEE_BASE_AllocTEEContext.
*/
#define DRM_TEE_METHOD_FUNCTION_MAP_COUNT ((DRM_DWORD)((DRM_METHOD_ID_DRM_TEE_Count - DRM_METHOD_ID_START_OF_OEM_FUNCTION_MAPPED_IDS) * 2 ))


#define DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS     ((DRM_DWORD)0x80000000)
#define DRM_TEE_PROXY_BIT_RESERVED_7            ((DRM_DWORD)0x40000000)
#define DRM_TEE_PROXY_BIT_API_UNSUPPORTED       ((DRM_DWORD)0x20000000)
#define DRM_TEE_PROXY_BIT_RESERVED_5            ((DRM_DWORD)0x10000000)
#define DRM_TEE_PROXY_BIT_RESERVED_4            ((DRM_DWORD)0x08000000)
#define DRM_TEE_PROXY_BIT_RESERVED_FOR_MF_1     ((DRM_DWORD)0x04000000)
#define DRM_TEE_PROXY_BIT_RESERVED_2            ((DRM_DWORD)0x02000000)
#define DRM_TEE_PROXY_BIT_RESERVED_1            ((DRM_DWORD)0x01000000)

/*
** The OEM is only allowed to set certain bits.
*/
#define DRM_TEE_PROXY_BITS_OEM_MASK             ((DRM_DWORD)( 0x00FFFFFF ))
#define DRM_TEE_PROXY_BITS_OEM_ALLOWED          ((DRM_DWORD)( DRM_TEE_PROXY_BITS_OEM_MASK | DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS | DRM_TEE_PROXY_BIT_API_UNSUPPORTED ))

#define DRM_TEE_PROXY_SHOULD_USE_STRUCT_PARAMS( dwFunctionMapOEMValue )     ( ( (DRM_DWORD)( (dwFunctionMapOEMValue) & DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS ) != 0 ) )
#define DRM_TEE_PROXY_RESERVED_FOR_MF_1( dwFunctionMapOEMValue )            ( ( (DRM_DWORD)( (dwFunctionMapOEMValue) & DRM_TEE_PROXY_BIT_RESERVED_FOR_MF_1 ) != 0 ) )

/*
** An API is supported if (and only if) it is supported in both user mode AND it is available inside TEE.
** Non-OEM mapped IDs are ALWAYS supported.
*/
#define DRM_TEE_PROXY_IS_FUNCTION_SUPPORTED_VIA_MAP( _rgdwFunctionMap, _eMethodID, _pIsSupported ) DRM_DO {                             \
    DRM_BOOL                *_pSupported  = ( _pIsSupported );                                                                          \
    DRM_METHOD_ID_DRM_TEE    _id          = ( _eMethodID );                                                                             \
    *_pSupported = FALSE;                                                                                                               \
    if( _id < DRM_METHOD_ID_START_OF_OEM_FUNCTION_MAPPED_IDS )                                                                          \
    {                                                                                                                                   \
        *_pSupported = TRUE;                                                                                                            \
    }                                                                                                                                   \
    else                                                                                                                                \
    {                                                                                                                                   \
        const DRM_DWORD *_macroMap = ( _rgdwFunctionMap );                                                                              \
        DRM_DWORD  _macroIdx;                                                                                                           \
        for( _macroIdx = 0; _macroIdx < DRM_METHOD_ID_DRM_TEE_Count - DRM_METHOD_ID_START_OF_OEM_FUNCTION_MAPPED_IDS; _macroIdx++ )     \
        {                                                                                                                               \
            if( _macroMap[ _macroIdx * 2 ] == ((DRM_DWORD)_id) )                                                                        \
            {                                                                                                                           \
                if( ( _macroMap[ _macroIdx * 2 + 1 ] & DRM_TEE_PROXY_BIT_API_UNSUPPORTED ) == 0 )                                       \
                {                                                                                                                       \
                    *_pSupported = TRUE;                                                                                                \
                }                                                                                                                       \
                break;                                                                                                                  \
            }                                                                                                                           \
        }                                                                                                                               \
    }                                                                                                                                   \
} DRM_WHILE_FALSE

#define DRM_TEE_PROXY_IS_FUNCTION_SUPPORTED( _pTeeCtx, _dwFunctionMapPKValue, _pIsSupported )      DRM_TEE_PROXY_IS_FUNCTION_SUPPORTED_VIA_MAP( ( _pTeeCtx )->oOEMVersionInfo.rgdwFunctionMap, ( _dwFunctionMapPKValue ), ( _pIsSupported ) )

#define DRM_TEE_PROXY_GET_STRUCT_PARAMS_METHOD_ID( dwFunctionMapOEMValue ) ( (DRM_DWORD)( (dwFunctionMapOEMValue) & DRM_TEE_PROXY_BITS_OEM_MASK ) )

#define DRM_TEE_IS_8_BYTE_ALIGNED(x) (0 == ( (DRM_SIZE_T)(x) & (DRM_SIZE_T)7))
#define DRM_TEE_IS_4_BYTE_ALIGNED(x) (0 == ( (DRM_SIZE_T)(x) & (DRM_SIZE_T)3))

#define Chk8ByteAligned(expr) ChkBOOL( DRM_TEE_IS_8_BYTE_ALIGNED(expr), DRM_E_TEE_PROXY_ILWALID_ALIGNMENT )
#define Chk4ByteAligned(expr) ChkBOOL( DRM_TEE_IS_4_BYTE_ALIGNED(expr), DRM_E_TEE_PROXY_ILWALID_ALIGNMENT )

EXIT_PK_NAMESPACE;

#endif /* _DRMTEEPROXYSTUBCOMMON_H_ */

