/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMBCERTCACHETYPES_H__
#define __DRMBCERTCACHETYPES_H__

#include <drmtypes.h>
#include <oemcommon.h>
#include <oemeccp256.h>

ENTER_PK_NAMESPACE;

typedef enum
{
    DRM_BINARY_DEVICE_CERT_KEY_SIGN         = 0,
    DRM_BINARY_DEVICE_CERT_KEY_ENCRYPT      = 1,
} eDRM_BINARY_DEVICE_CERT_KEYUSAGE;

typedef struct
{
    PUBKEY_P256       PublicKey;
DRM_OBFUS_PTR_TOP
    OEM_CRYPTO_HANDLE hPublicKey;
DRM_OBFUS_PTR_BTM
} DRM_PUBLIC_KEY_CONTEXT;

typedef struct
{
    DRM_PUBLIC_KEY_CONTEXT PublicKeyContext;
} DRM_BINARY_DEVICE_CERT_KEYPAIR;

typedef struct
{
    DRM_ID                          DeviceSerialNumber;
    DRM_DWORD                       dwGroupSelwrityLevel;
    DRM_DWORD                       dwFeatureFlags;
    DRM_BINARY_DEVICE_CERT_KEYPAIR  DeviceKeySign;
    DRM_BINARY_DEVICE_CERT_KEYPAIR  DeviceKeyEncrypt;
} DRM_BINARY_DEVICE_CERT_CACHED_VALUES;

#define DRM_BINARY_DEVICE_CERT_CACHED_VALUES_EMPTY { DRM_ID_EMPTY, 0, 0, {0}, {0} }

EXIT_PK_NAMESPACE;

#endif /* __DRMBCERTCACHETYES_H__ */

