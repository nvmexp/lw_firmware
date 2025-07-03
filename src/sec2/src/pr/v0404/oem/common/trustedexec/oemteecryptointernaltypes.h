/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _OEMTEECRYPTOINTERNALTYPES_H_
#define _OEMTEECRYPTOINTERNALTYPES_H_ 1

#include <oemteetypes.h>
#include <oembroker.h>
#include "oemcommonlw.h"

ENTER_PK_NAMESPACE;

typedef struct __tagOEM_TEE_KEY_ECC
{
    PRIVKEY_P256         oPriv;
} OEM_TEE_KEY_ECC;

typedef struct __tagOEM_TEE_KEY_AES
{
    DRM_BYTE             rgbRawKey[DRM_AES_KEYSIZE_128];
#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)
    /*
    ** LWE: (vyadav) Dont need to store the data struct below as
    **  AES is HW accelerated. We just need to store the raw key
    */
    DRM_AES_KEY          oKey;
#endif
} OEM_TEE_KEY_AES;

#define OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_BYTES( _pKey )    ( DRM_REINTERPRET_CAST( OEM_TEE_KEY_AES, (_pKey)->pKey )->rgbRawKey)

#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)
/*
** LWE: (vyadav) Only do this if we're using SW implementation of AES. If we
** use HW implementation then we don't need to generate the round keys in SW.
*/
#define OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY( _pKey )  (&DRM_REINTERPRET_CAST( OEM_TEE_KEY_AES, (_pKey)->pKey )->oKey)
#else
/*
** LWE: (vyadav) For our HW accelerated usage, make the usage of this macro as fatal.
** This macro can't possibly get the right key since we're compiling out the key structs it uses
*/
#define OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY( _pKey )     (doCleanupAndHalt(ERROR_CODE_000_ILWALID_USE_OF_COLWERT_OEM_TEE_KEY_MACRO))
#endif

#define OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256( _pKey ) (&DRM_REINTERPRET_CAST( OEM_TEE_KEY_ECC, (_pKey)->pKey )->oPriv)
#define OEM_TEE_INTERNAL_COLWERT_ECC256_PRIVKEY_P256_TO_OEM_TEE_KEY( _pKey ) (&DRM_REINTERPRET_CAST( OEM_TEE_KEY, (_pKey)->m_rgbPrivkey )->pKey)

#define OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_BROKER( _pKey )  \
    ( Oem_Broker_IsTEE() ? (DRM_REINTERPRET_CAST(DRM_AES_KEY, (OEM_TEE_KEY *)(_pKey))) : OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY((OEM_TEE_KEY *)(_pKey)) )

#define OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_BROKER( _pKey )  \
    ( Oem_Broker_IsTEE() ? DRM_REINTERPRET_CAST(PRIVKEY_P256, (_pKey)) : OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_ECC256_TO_PRIVKEY_P256((OEM_TEE_KEY *)(_pKey)) )


EXIT_PK_NAMESPACE;

#endif /* _OEMTEECRYPTOINTERNALTYPES_H_ */

