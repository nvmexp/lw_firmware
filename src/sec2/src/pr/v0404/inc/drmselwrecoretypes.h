/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMSELWRECORETYPES_H_
#define _DRMSELWRECORETYPES_H_ 1

#include <drmkeyfilecerttypes.h>
#include <drmteetypes.h>
#include <drmblackboxtypes.h>
#include <oemteetypes.h>

ENTER_PK_NAMESPACE;

typedef enum __tagDRM_REE_FEATURE_ENUM
{
    DRM_REE_FEATURE_ENUM_AESCBCS        = 0x0001,
    DRM_REE_FEATURE_ENUM_SELWRESTOP2    = 0x0002,
} DRM_REE_FEATURE_ENUM;

typedef DRM_RESULT( DRM_CALL* DRM_SELWRECORE_TEE_DATA_STORE_PASSWORD_CALLBACK )(
    __inout                               DRM_VOID    *f_pvUserCtx,
    __in                                  DRM_BOOL     f_fRead,
    __in                                  DRM_DWORD    f_cbToSign,
    __in_bcount( f_cbToSign )       const DRM_BYTE    *f_pbToSign,
    __inout_bcount( OEM_SHA1_DIGEST_LEN ) DRM_BYTE    *f_pbPasswordSST );

/*
** Maximum number of nonces to be stored in memory in a single session
*/
#define DRM_MAX_NONCE_COUNT_PER_SESSION ((DRM_DWORD)(100))

typedef struct __tagDRM_NKB_DATA
{
    DRM_TEE_BYTE_BLOB     rgNKBs[DRM_MAX_NONCE_COUNT_PER_SESSION];
    DRM_ID                rgNonces[DRM_MAX_NONCE_COUNT_PER_SESSION];
    DRM_DWORD             cNonces;
} DRM_NKB_DATA;

typedef struct __tagDRM_SELWRECORE_TEE_DATA
{
    DRM_TEE_PROXY_CONTEXT *pTeeCtx;
    DRM_TEE_BYTE_BLOB      oCertificate;
    DRM_TEE_BYTE_BLOB      oPPKB;
    DRM_TEE_BYTE_BLOB      oRKB;
    DRM_TEE_BYTE_BLOB      oSPKB;
    DRM_TEE_BYTE_BLOB      oTeeFeatureInformationXml;
    DRM_NKB_DATA           oNKBData;
    DRM_RESULT             drError;     /* Hold error codes returned from the TEE that will trigger reprovisioning */

    DRM_SELWRECORE_TEE_DATA_STORE_PASSWORD_CALLBACK     pfnStorePasswordCallback;
} DRM_SELWRECORE_TEE_DATA;

typedef struct __tagDRM_SELWRECORE_INTERNAL_DATA
{
    DRM_VOID         *pOpaqueKeyFileContext;
    DRM_VOID         *pOpaqueBlackBoxContext;
    DRM_VOID         *pOpaqueKeyHandleCallbacks;
} DRM_SELWRECORE_INTERNAL_DATA;

typedef struct __tagDRM_SELWRECORE_CONTEXT
{
    union
    {
        DRM_SELWRECORE_TEE_DATA             *pSelwreCoreTeeData;
        DRM_SELWRECORE_INTERNAL_DATA        *pSelwreCoreInternalData;
    } uData;

    DRM_VOID                                *pOEMContext;
    DRM_CLIENT_INFO                         *pClientInfo;
    DRM_CRYPTO_CONTEXT                      *pCryptoContext;
    DRM_BINARY_DEVICE_CERT_CACHED_VALUES    *pCertCache;
    DRM_VOID                                *pFuncTbl;
    DRM_BOOL                                 fIsRunningInHWDRM;
    DRM_BOOL                                 fDisableBlobCache;
} DRM_SELWRECORE_CONTEXT;

typedef struct __tagDRM_SELWRECORE_VERSIONINFO
{
    DRM_DWORD       dwPKMajorVersion;
    DRM_DWORD       dwPKMinorVersion;
    DRM_DWORD       dwPKBuildVersion;
    DRM_DWORD       dwPKQFEVersion;
    DRM_DWORD       dwOEMVersion;
    DRM_DWORD       cbSystemProperties;
    const DRM_BYTE *pbSystemPropertiesWeakRef;
    DRM_DWORD       cchOEMManufacturerName;
    DRM_WCHAR      *pwszOEMManufacturerNameWeakRef;
    DRM_DWORD       cchOEMModelName;
    DRM_WCHAR      *pwszOEMModelNameWeakRef;
    DRM_DWORD       cchOEMModelNumber;
    DRM_WCHAR      *pwszOEMModelNumberWeakRef;
    DRM_DWORD       cbOEMVersion2;
    DRM_BYTE       *pbOEMVersion2WeakRef;
} DRM_SELWRECORE_VERSIONINFO;

#define DRM_SELWRECORE_DisableBlobCache( __pSelwreCoreCtx ) DRM_DO { (__pSelwreCoreCtx)->fDisableBlobCache = TRUE; } DRM_WHILE_FALSE

EXIT_PK_NAMESPACE;

#endif /* _DRMSELWRECORETYPES_H_ */

