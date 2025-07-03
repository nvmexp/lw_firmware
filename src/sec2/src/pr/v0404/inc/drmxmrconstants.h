/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRM_XMR_CONSTANTS_H__
#define __DRM_XMR_CONSTANTS_H__

ENTER_PK_NAMESPACE;

/*
** constants, enums and data types
*/
#define XMR_UNLIMITED             DRM_MAX_UNSIGNED_TYPE( DRM_DWORD )

#define XMR_AES_OMAC_SIGNATURE_LENGTH     16

#define XMR_SOURCEID_MAXCOUNT   100

#define XMR_RID_OFFSET           ( sizeof( DRM_DWORD ) * 2 )
#define XMR_HEADER_LENGTH        ( XMR_RID_OFFSET + sizeof( DRM_ID ) )
#define XMR_BASE_OBJECT_LENGTH   ( sizeof( DRM_WORD ) * 2  + sizeof( DRM_DWORD ) )

typedef enum __tagXMR_SOURCEID
{
    XMR_SOURCEID_NONE          = 0,
    XMR_SOURCEID_MACROVISION   = 1,
    XMR_SOURCEID_CGMSA         = 2,
    XMR_SOURCEID_WSS           = 3,
    XMR_SOURCEID_DIGITAL_CABLE = 4,
    XMR_SOURCEID_ATSC          = 5,
    XMR_SOURCEID_PDRM          = 260,
    XMR_SOURCEID_LEGACY_DVR    = 261,
    XMR_SOURCEID_V1            = 262,
} XMR_SOURCEID;

typedef enum __tagXMR_VERSION
{
    XMR_VERSION_ILWALID  = 0x0000,
    XMR_VERSION_1        = 0x0001,
    XMR_VERSION_2        = 0x0002,
    XMR_VERSION_3        = 0x0003,

    XMR_VERSION_MAX      = XMR_VERSION_3,
} XMR_VERSION;

typedef enum __tagXMR_OBJECT_FLAGS
{
    XMR_FLAGS_NONE                  = 0x0000,
    XMR_FLAGS_MUST_UNDERSTAND       = 0x0001,
    XMR_FLAGS_CONTAINER             = 0x0002,
    XMR_FLAGS_ALLOW_EXTERNAL_PARSE  = 0x0004,
    XMR_FLAGS_BEST_EFFORT           = 0x0008,
    XMR_FLAGS_HAS_SELWRE_STATE      = 0x0010
} XMR_OBJECT_FLAGS;

typedef enum __tagXMR_SETTINGS_FLAGS
{
    XMR_SETTINGS_FLAG_CANNOT_PERSIST    = 0x0001
} XMR_SETTINGS_FLAGS;

/*
** Symmetric encryption types used for content encryption
*/
typedef enum __tagXMR_SYMMETRIC_ENCRYPTION_TYPE
{
    XMR_SYMMETRIC_ENCRYPTION_TYPE_ILWALID      = 0x0000,
    XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CTR  = 0x0001,
    XMR_SYMMETRIC_ENCRYPTION_TYPE_RC4_CIPHER   = 0x0002,
    XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_ECB  = 0x0003,
    XMR_SYMMETRIC_ENCRYPTION_TYPE_COCKTAIL     = 0x0004,
    XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CBC  = 0x0005,
} XMR_SYMMETRIC_ENCRYPTION_TYPE;

/*
** Asymmetric encryption types used for encrypting the content key
*/
typedef enum __tagXMR_ASYMMETRIC_ENCRYPTION_TYPE
{
    XMR_ASYMMETRIC_ENCRYPTION_TYPE_ILWALID                     = 0x0000,
    XMR_ASYMMETRIC_ENCRYPTION_TYPE_RSA_1024                    = 0x0001,
    XMR_ASYMMETRIC_ENCRYPTION_TYPE_CHAINED_LICENSE             = 0x0002,
    XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256                     = 0x0003,
    XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256_WITH_KZ             = 0x0004,
    XMR_ASYMMETRIC_ENCRYPTION_TYPE_TEE_TRANSIENT               = 0x0005,
    XMR_ASYMMETRIC_ENCRYPTION_TYPE_ECC_256_VIA_SYMMETRIC       = 0x0006
} XMR_ASYMMETRIC_ENCRYPTION_TYPE;

/*
** Symmetric encryption types used for encrypting the content key (optimized)
*/
typedef enum __tagXMR_SYMMETRIC_KEY_ENCRYPTION_TYPE
{
    XMR_SYMMETRIC_KEY_ENCRYPTION_TYPE_ILWALID          = 0x0000,
    XMR_SYMMETRIC_KEY_ENCRYPTION_TYPE_AES_128_ECB      = 0x0001,
    XMR_SYMMETRIC_KEY_ENCRYPTION_TYPE_AES_128_ECB_SLK  = 0x0002
} XMR_SYMMETRIC_KEY_ENCRYPTION_TYPE;

typedef enum __tagXMR_ECC_LWRVE_TYPE
{
    XMR_ECC_LWRVE_TYPE_ILWALID                = 0x0000,
    XMR_ECC_LWRVE_TYPE_P256                   = 0x0001
} XMR_ECC_LWRVE_TYPE;

typedef enum __tagXMR_SIGNATURE_TYPE
{
    XMR_SIGNATURE_TYPE_ILWALID                = 0x0000,
    XMR_SIGNATURE_TYPE_AES_128_OMAC           = 0x0001,
    XMR_SIGNATURE_TYPE_SHA_256_HMAC           = 0x0002
} XMR_SIGNATURE_TYPE;

typedef enum __tagXMR_GLOBAL_RIGHTS_SETTINGS
{
    XMR_RIGHTS_CANNOT_PERSIST                 = 0x001,
    XMR_RIGHTS_ALLOW_BACKUP_RESTORE           = 0x004,
    XMR_RIGHTS_COLLABORATIVE_PLAY             = 0x008,
    XMR_RIGHTS_BASE_LICENSE                   = 0x010,
                                             /* 0x020 was previously used by THUMBNAIL right in Vista/Polaris and shouldn't be used */
    XMR_RIGHTS_CANNOT_BIND_LICENSE            = 0x040,
    XMR_RIGHTS_TEMP_STORE_ONLY                = 0x080,
} XMR_GLOBAL_RIGHTS_SETTINGS;

typedef enum __tagXMR_EXTENSIBLE_RESTRICTON_STATE
{
    XMR_EXTENSIBLE_RESTRICTON_STATE_COUNT      = 0x02,
    XMR_EXTENSIBLE_RESTRICTON_STATE_DATE       = 0x03,
    XMR_EXTENSIBLE_RESTRICTON_STATE_BYTEARRAY  = 0x04,
} XMR_EXTENSIBLE_RESTRICTON_STATE;

typedef enum __tagXMR_EMBEDDING_BEHAVIOR
{
    XMR_EMBEDDING_BEHAVIOR_ILWALID = 0x00,
    XMR_EMBEDDING_BEHAVIOR_IGNORE  = 0x01,
    XMR_EMBEDDING_BEHAVIOR_COPY    = 0x02,
    XMR_EMBEDDING_BEHAVIOR_MOVE    = 0x03
} XMR_EMBEDDING_BEHAVIOR;

typedef enum __tagXMR_UPLINK_CHECKSUM_TYPE
{
    XMR_UPLINK_CHECKSUM_TYPE_XMRV1      = 0x00,
    XMR_UPLINK_CHECKSUM_TYPE_AESOMAC1   = 0x01,
} XMR_UPLINK_CHECKSUM_TYPE;

typedef enum __tagXMR_SELWRE_STOP_2_SIGNATURE_ALGORITHM
{
    XMR_SELWRE_STOP_2_SIGNATURE_ALGORITHM_ILWALID                                              = 0x00,

    /*
    ** Algorithm:
    ** ECC256_Sign(
    **      SigningKey = [Client Cert ECC256 Private Signing Key],
    **      DataToSign = SHA256(
    **          DataToHash = AESCBC_Encrypt(
    **              EncryptionKey = xbbaEncryptedKey,
    **              IV = KID ^ [Secure Stop Session ID],
    **              DataToEncrypt = [Challenge's Original ECC256 Signature]
    **          )
    **      )
    **  )
    */
    XMR_SELWRE_STOP_2_SIGNATURE_ALGORITHM_ECC256_SHA256_AES128CBC_IVKIDSESSIONID_PLAINOLDSIG   = 0x01,
} XMR_SELWRE_STOP_2_SIGNATURE_ALGORITHM;

/*
** OptimizedContentKey2.wCacheKeyEncryptionAlgorithm
*/
typedef enum __tagXMR_OPTIMIZED_CONTENT_KEY2_CACHE_KEY_ENCRYPTION_ALGORITHM
{
    XMR_OPTIMIZED_CONTENT_KEY2_CACHE_KEY_ENCRYPTION_ALGORITHM_ILWALID                   = 0x00,

    /*
    ** CacheKey =
    **      XOR(
    **          AESOMAC1(
    **              HashingKey = First Half of ECC256_Decrypt(
    **                  DecryptionKey = [Client Cert ECC256 Private Content Encryption Key],
    **                  DataToDecrypt = xbbaCacheKey
    **                  ),
    **              DataToHash = 16 bytes all zeroes
    **          ),
    **          AESOMAC1(
    **              HashingKey = Second Half of same ECC256_Decrypt
    **              DataToHash = 16 bytes all 0xFF
    **          )
    **      )
    */
    XMR_OPTIMIZED_CONTENT_KEY2_CACHE_KEY_ENCRYPTION_ALGORITHM_ECC256_XOR_OMAC1          = 0x01,
} XMR_OPTIMIZED_CONTENT_KEY2_CACHE_KEY_ENCRYPTION_ALGORITHM;

/*
** OptimizedContentKey2.wOriginalEccOutputEncryptionAlgorithm
*/
typedef enum __tagXMR_OPTIMIZED_CONTENT_KEY2_ORIGINAL_ECC_OUTPUT_ENCRYPTION_ALGORITHM
{
    XMR_OPTIMIZED_CONTENT_KEY2_ORIGINAL_ECC_OUTPUT_ENCRYPTION_ALGORITHM_ILWALID         = 0x00,

    /*
    ** OriginalEccOutput =
    **      AESECB_Decrypt(
    **          DecryptionKey = CacheKey,
    **          DataToDecrypt = xbbaOriginalEccOutput
    **      )
    */
    XMR_OPTIMIZED_CONTENT_KEY2_ORIGINAL_ECC_OUTPUT_ENCRYPTION_ALGORITHM_ECB             = 0x01,
} XMR_OPTIMIZED_CONTENT_KEY2_ORIGINAL_ECC_OUTPUT_ENCRYPTION_ALGORITHM;


EXIT_PK_NAMESPACE;

#endif  /* __DRM_XMR_CONSTANTS_H__ */

