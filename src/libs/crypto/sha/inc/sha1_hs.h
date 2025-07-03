/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @copydoc sha1.h
 */

#ifndef SHA1_HS_H
#define SHA1_HS_H

#include "lwuproc.h"
#include "sha1.h"

/* ------------------------- Prototypes ------------------------------------- */
void sha1_hs(LwU8 *pHash, void *pData, LwU32 nBytes)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha1_hs");

void sha1Initialize_hs(PSHA1_CONTEXT pContext)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha1Initialize_hs");
void sha1Update_hs(PSHA1_CONTEXT pContext, void *pData, LwU32 len)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha1Update_hs");
void sha1Final_hs(LwU8 *pDigest, PSHA1_CONTEXT pContext)
    GCC_ATTRIB_SECTION("imem_libShaHs", "sha1Final_hs");

void hmacSha1_hs(LwU8 pHash[SHA1_DIGEST_LENGTH], void *pMessage, LwU32 nMessageBytes, void *pKey, LwU32 nKeyBytes)
    GCC_ATTRIB_SECTION("imem_libShaHs", "hmacSha1_hs");

#endif // SHA1_HS_H
