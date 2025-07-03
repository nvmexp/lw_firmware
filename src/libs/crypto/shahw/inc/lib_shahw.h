/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SHAHW_H
#define LIB_SHAHW_H

/*!
 * @file   lib_shahw.h
 * @brief  Common SHA HW defines
*/

/* ------------------------ Macros and Defines ----------------------------- */
//
// Mutex IDs for various usecases.
// Defined here in an attempt to synchronize and prevent overlap.
//
#define SHA_MUTEX_ID_SPDM_SHA_256    0x01
#define SHA_MUTEX_ID_SPDM_HMAC_256   0x02
#define SHA_MUTEX_ID_APM             0x03
#define SHA_MUTEX_ID_HDCP            0x04
#define SHA_MUTEX_ID_SHAHW_FUNCS     0xFE
#define SHA_MUTEX_ID_SHAHW_SOFTRESET 0xFF

//
// Timeouts extrapolated from maximum message size performance.
// HW recommends IDLE_TIMEOUT + 20ns for reset.
//
#define SHAHW_ENGINE_IDLE_TIMEOUT_NS       (100000000)
#define SHAHW_ENGINE_SW_RESET_TIMEOUT_NS   (SHAHW_ENGINE_IDLE_TIMEOUT_NS + 20)

#define SHA_1_BLOCK_SIZE_BYTE              (64)
#define SHA_224_BLOCK_SIZE_BYTE            (64)
#define SHA_256_BLOCK_SIZE_BYTE            (64)
#define SHA_384_BLOCK_SIZE_BYTE            (128)
#define SHA_512_BLOCK_SIZE_BYTE            (128)
#define SHA_512_224_BLOCK_SIZE_BYTE        (128)
#define SHA_512_256_BLOCK_SIZE_BYTE        (128)

#define SHA_1_HASH_SIZE_BYTE               (20)
#define SHA_224_HASH_SIZE_BYTE             (28)
#define SHA_256_HASH_SIZE_BYTE             (32)
#define SHA_384_HASH_SIZE_BYTE             (48)
#define SHA_512_HASH_SIZE_BYTE             (64)
#define SHA_512_224_HASH_SIZE_BYTE         (28)
#define SHA_512_256_HASH_SIZE_BYTE         (32)

#define SHA_1_HASH_SIZE_DWORD              (SHA_1_HASH_SIZE_BYTE       / sizeof(LwU32))
#define SHA_224_HASH_SIZE_DWORD            (SHA_224_HASH_SIZE_BYTE     / sizeof(LwU32))
#define SHA_256_HASH_SIZE_DWORD            (SHA_256_HASH_SIZE_BYTE     / sizeof(LwU32))
#define SHA_384_HASH_SIZE_DWORD            (SHA_384_HASH_SIZE_BYTE     / sizeof(LwU32))
#define SHA_512_HASH_SIZE_DWORD            (SHA_512_HASH_SIZE_BYTE     / sizeof(LwU32))
#define SHA_512_224_HASH_SIZE_DWORD        (SHA_512_224_HASH_SIZE_BYTE / sizeof(LwU32))
#define SHA_512_256_HASH_SIZE_DWORD        (SHA_512_256_HASH_SIZE_BYTE / sizeof(LwU32))

#define SHA_512_224_IV_SIZE_BYTE           (64)
#define SHA_512_256_IV_SIZE_BYTE           (64)

#define SHA_512_224_IV_SIZE_DWORD          (SHA_512_224_IV_SIZE_BYTE   / sizeof(LwU32))
#define SHA_512_256_IV_SIZE_DWORD          (SHA_512_256_IV_SIZE_BYTE   / sizeof(LwU32))

// For SHA-512/t initial vector, please refer to SHA IAS, 4.2.3 SHA-512/t Support.
#define SHA_512_224_IV_DWORD_0             (0x19544DA2)
#define SHA_512_224_IV_DWORD_1             (0x8C3D37C8)
#define SHA_512_224_IV_DWORD_2             (0x89DCD4D6)
#define SHA_512_224_IV_DWORD_3             (0x73E19966)
#define SHA_512_224_IV_DWORD_4             (0x32FF9C82)
#define SHA_512_224_IV_DWORD_5             (0x1DFAB7AE)
#define SHA_512_224_IV_DWORD_6             (0x582F9FCF)
#define SHA_512_224_IV_DWORD_7             (0x679DD514)
#define SHA_512_224_IV_DWORD_8             (0x7BD44DA8)
#define SHA_512_224_IV_DWORD_9             (0x0F6D2B69)
#define SHA_512_224_IV_DWORD_10            (0x04C48942)
#define SHA_512_224_IV_DWORD_11            (0x77E36F73)
#define SHA_512_224_IV_DWORD_12            (0x6A1D36C8)
#define SHA_512_224_IV_DWORD_13            (0x3F9D85A8)
#define SHA_512_224_IV_DWORD_14            (0x91D692A1)
#define SHA_512_224_IV_DWORD_15            (0x1112E6AD)

#define SHA_512_256_IV_DWORD_0             (0xFC2BF72C)
#define SHA_512_256_IV_DWORD_1             (0x22312194)
#define SHA_512_256_IV_DWORD_2             (0xC84C64C2)
#define SHA_512_256_IV_DWORD_3             (0x9F555FA3)
#define SHA_512_256_IV_DWORD_4             (0x6F53B151)
#define SHA_512_256_IV_DWORD_5             (0x2393B86B)
#define SHA_512_256_IV_DWORD_6             (0x5940EABD)
#define SHA_512_256_IV_DWORD_7             (0x96387719)
#define SHA_512_256_IV_DWORD_8             (0xA88EFFE3)
#define SHA_512_256_IV_DWORD_9             (0x96283EE2)
#define SHA_512_256_IV_DWORD_10            (0x53863992)
#define SHA_512_256_IV_DWORD_11            (0xBE5E1E25)
#define SHA_512_256_IV_DWORD_12            (0x2C85B8AA)
#define SHA_512_256_IV_DWORD_13            (0x2B0199FC)
#define SHA_512_256_IV_DWORD_14            (0x81C52CA2)
#define SHA_512_256_IV_DWORD_15            (0x0EB72DDC)

#define HMAC_SHA_256_IPAD_MASK             0x36
#define HMAC_SHA_256_OPAD_MASK             0x5c

typedef enum
{
   SHA_ID_SHA_1       = 0,
   SHA_ID_SHA_224     = 1,
   SHA_ID_SHA_256     = 2,
   SHA_ID_SHA_384     = 3,
   SHA_ID_SHA_512     = 4,
   SHA_ID_SHA_512_224 = 5,
   SHA_ID_SHA_512_256 = 6,
} SHA_ID;

typedef enum
{
    SHA_SRC_TYPE_DMEM = 0,
    SHA_SRC_TYPE_IMEM = 1,
    SHA_SRC_TYPE_FB   = 2,
} SHA_SRC_TYPE;

typedef enum {
    SHA_PRIV_LEVEL_MASK_LEVEL_0 = 0x0,
    SHA_PRIV_LEVEL_MASK_LEVEL_1 = 0x1,
    SHA_PRIV_LEVEL_MASK_LEVEL_2 = 0x4,
} SHA_PRIV_LEVEL_MASK;

typedef enum {
    SHA_INT_STATUS_NONE                = 0,
    SHA_INT_STATUS_OP_DONE             = 1,
    SHA_INT_STATUS_UNKNOWN             = 2,
    SHA_INT_STATUS_ERR_LEVEL_ERROR     = 3,
    SHA_INT_STATUS_ERR_LEVEL_MULTI_HIT = 4,
    SHA_INT_STATUS_ERR_STATUS_MISS     = 5,
    SHA_INT_STATUS_ERR_PA_FAULT        = 6,
    SHA_INT_STATUS_ERR_FBIF_ERROR      = 7,
    SHA_INT_STATUS_ERR_ILLEGAL_CFG     = 8,
    SHA_INT_STATUS_ERR_NS_ACCESS       = 9,
} SHA_INT_STATUS;

// Additional defines used commonly across SHA HW implementations.
#define SHAHW_REVERT_ENDIAN_DWORD(v)           (((v>>24) & 0xff)     | \
                                                ((v<<8)  & 0xff0000) | \
                                                ((v>>8)  & 0xff00)   | \
                                                ((v<<24) & 0xff000000))

#define SHAHW_BYTES_TO_BITS(v)                 (v * 8)

#define SHAHW_CHECK_BYTES_TO_BITS_OVERFLOW(v)  ((v>>29) & 0x7)

#endif // LIB_SHAHW_H
