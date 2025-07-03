/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SHA256_H
#define SHA256_H

#include <string.h>

/*!
 * Constants for open standard implementations
 *
 * Constants used by some open-standard implementations.
 * These values rarely changes.
 */                                    
#define SHA256_BLOCK_SIZE       (512 / 8)
#define SHA256_MSG_BLOCK        (sizeof(LwU32)*16)
#define SHA256_HASH_BLOCK       (sizeof(LwU32)*8)
#define SHA256_HMAC_BLOCK       64
#define SHA256_ROTR(x,n)        (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_RSHI(x,n)        (x >> n)
#define SHA256_CHA(x,y,z)       (((x) & (y)) ^ ((~(x)) & (z)))
#define SHA256_MAJ(x,y,z)       (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_SUM0(x)          (SHA256_ROTR(x,2) ^ SHA256_ROTR(x,13) ^ SHA256_ROTR(x,22))
#define SHA256_SUM1(x)          (SHA256_ROTR(x,6) ^ SHA256_ROTR(x,11) ^ SHA256_ROTR(x,25))
#define SHA256_SIG0(x)          (SHA256_ROTR(x,7) ^ SHA256_ROTR(x,18) ^ SHA256_RSHI(x,3))
#define SHA256_SIG1(x)          (SHA256_ROTR(x,17) ^ SHA256_ROTR(x,19) ^ SHA256_RSHI(x,10))
#define SHA256_HASH_BLOCK_SIZE  (sizeof(LwU32)*8)
#define SHA256_MSG_BLOCK_SIZE   (sizeof(LwU32)*16)

#define PACK32(str, x)                        \
{                                             \
    *(x) =   ((LwU32) *((str) + 3)      )     \
           | ((LwU32) *((str) + 2) <<  8)     \
           | ((LwU32) *((str) + 1) << 16)     \
           | ((LwU32) *((str) + 0) << 24);    \
}

#define UNPACK32(x, str)                      \
{                                             \
    *((str) + 3) = (LwU8) ((x)      );        \
    *((str) + 2) = (LwU8) ((x) >>  8);        \
    *((str) + 1) = (LwU8) ((x) >> 16);        \
    *((str) + 0) = (LwU8) ((x) >> 24);        \
}

#define shamemcmp memcmp
#define shamemcpy memcpy
#define shamemset memset

typedef struct
{
    LwU64 totalMsgLen;
    LwU32 lwrMsgLen;
    LwU8  block[2 * SHA256_BLOCK_SIZE];
    LwU32 H[8];
} SHA256_CTX;

void sha256(LwU8 *pDigest, const LwU8 *pMessage, LwU32 len)
    GCC_ATTRIB_SECTION("imem_libSha", "sha256");
void hmacSha256(LwU8 *pMac, const LwU8 *pMsg, const LwU32 msgSize,
                    const LwU8 *pKey, const LwU32 keySize)
    GCC_ATTRIB_SECTION("imem_libSha", "hmacSha256");

void sha256Final(SHA256_CTX *pShaCtx, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libSha", "sha256Final");
void sha256Update(const LwU8 *pMsg, LwU32 len, SHA256_CTX *pShaCtx)
    GCC_ATTRIB_SECTION("imem_libSha", "sha256Update");
void sha256Transform(SHA256_CTX *pShaCtx, LwU8 *pMsg)
    GCC_ATTRIB_SECTION("imem_libSha", "sha256Transform");
void sha256Init(SHA256_CTX *pShaCtx)
    GCC_ATTRIB_SECTION("imem_libSha", "sha256Init");

#endif // SHA256_H
