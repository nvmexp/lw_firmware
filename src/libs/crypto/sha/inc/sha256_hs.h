/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SHA256_HS_H
#define SHA256_HS_H

/*!
 * @file sha256_hs.h
 * This file contains the HS version of the SHA-256 functions.
 * This file should only be included by heavy secure microcode. Additionally,
 * this calls the HS versions of memset, memcmp, and memcpy (found in
 * sec2_mem_hs.h)
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sha256.h"     // Import common data structures and constants

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
void sha256_hs(LwU8 *pDigest, const LwU8 *pMessage, LwU32 len)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha256_hs");
void hmacSha256_hs(LwU8 *pMac, const LwU8 *pMsg, const LwU32 msgSize,
                    const LwU8 *pKey, const LwU32 keySize)
    GCC_ATTRIB_SECTION("imem_libShaHs", "hmacSha256_hs");

void sha256Final_hs(SHA256_CTX *pShaCtx, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha256Final_hs");
void sha256Update_hs(const LwU8 *pMsg, LwU32 len, SHA256_CTX *pShaCtx)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha256Update_hs");
void sha256Transform_hs(SHA256_CTX *pShaCtx, LwU8 *pMsg)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha256Transform_hs");
void sha256Init_hs(SHA256_CTX *pShaCtx)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha256Init_hs");

#endif // SHA256_HS_H
