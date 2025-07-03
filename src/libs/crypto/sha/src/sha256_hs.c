/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    sha256_hs.c
 * @brief   Library to implement HS versions of the SHA256 functions for uproc.
 *          This library requires access to the HS version of various memory
 *          functions: memcpy, memset, memcmp.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */
#include "flcnifcmn.h"
#include "sha256_hs.h"
#include "sha256_const.h"
#include "mem_hs.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
static void _libShaHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libShaHs", "start")
    GCC_ATTRIB_USED();

/* ------------------------- Private Functions ------------------------------ */
/*!
 * _libShaHsEntry
 *
 * @brief Entry function of SHA-256 library. This function merely returns. It
 *        is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_libShaHsEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}

/*!
 * @brief SHA 256 Initialization function.
 */
void sha256Init_hs(SHA256_CTX *pShaCtx)
{
    LwU32 ind = 0;
    for (ind = 0; ind < 8; ind++)
    {
        pShaCtx->H[ind] = sha256H[ind];
    }
    pShaCtx->totalMsgLen = 0;
    pShaCtx->lwrMsgLen = 0;
}

/*!
 * @brief SHA 256 Transform function.
 */
void sha256Transform_hs(SHA256_CTX *pShaCtx, LwU8 *pMsg)
{
    LwU32    m[64];
    LwU32    H[8];
    LwU32    i = 0;

    for (i = 0; i < 16; i++)
    {
        PACK32(&(pMsg[i*4]), &(m[i]));
    }
    for (; i < 64; i++)
    {
        m[i] = SHA256_SIG1(m[i-2]) + m[i-7] + SHA256_SIG0(m[i-15]) + m[i-16];
    }

    for (i=0; i< 8; i++)
    {
        H[i] = pShaCtx->H[i];
    }

    for (i=0; i < 64; i++)
    {
        LwU32 T1 = H[7] + SHA256_SUM1(H[4]) + SHA256_CHA(H[4], H[5], H[6]) + sha256K[i] + m[i];
        LwU32 T2 = SHA256_SUM0(H[0]) + SHA256_MAJ(H[0], H[1], H[2]);
        H[7] = H[6];
        H[6] = H[5];
        H[5] = H[4];
        H[4] = H[3] + T1;
        H[3] = H[2];
        H[2] = H[1];
        H[1] = H[0];
        H[0] = T1 + T2;
    }

    for (i=0; i< 8; i++)
    {
        pShaCtx->H[i] += H[i];
    }
}

/*!
 * @brief SHA 256 Update function.
 */
void sha256Update_hs(const LwU8 *pMsg, LwU32 len, SHA256_CTX *pShaCtx)
{
    LwU8 *pVmsg = NULL;
    LwU32 remLen = SHA256_BLOCK_SIZE - pShaCtx->lwrMsgLen;
    LwU32 tbLen  = (len > remLen)?remLen:len;
    memcpy_hs(&(pShaCtx->block[pShaCtx->lwrMsgLen]), pMsg, tbLen);

    pShaCtx->totalMsgLen += len;
    if ((tbLen + pShaCtx->lwrMsgLen) != SHA256_BLOCK_SIZE)
    {
        pShaCtx->lwrMsgLen += tbLen;
        return;
    }

    pVmsg = pShaCtx->block;

    while (pVmsg)
    {
        sha256Transform_hs(pShaCtx, pVmsg);
        pVmsg = (LwU8*)&(pMsg[tbLen]);
        remLen = len-tbLen;
        if (remLen < SHA256_BLOCK_SIZE)
        {
            // Copy the remaining message to block
            pShaCtx->lwrMsgLen = remLen;
            memcpy_hs(pShaCtx->block, pVmsg, remLen);
            break;
        }
        tbLen += SHA256_BLOCK_SIZE;
    }
}

/*!
 * @brief SHA 256 Final function.
 */
void sha256Final_hs(SHA256_CTX *pShaCtx, LwU8 *pDigest)
{
    LwU8 finalBlock[SHA256_BLOCK_SIZE * 2];
    LwU32 kb;
    LwU8 *pTot = NULL;
    pShaCtx->totalMsgLen *= 8;

    pTot = (LwU8*)(&pShaCtx->totalMsgLen);

    if (pShaCtx->lwrMsgLen + 8 + 1 > SHA256_BLOCK_SIZE)
    {
        kb = (SHA256_BLOCK_SIZE * 2 ) - pShaCtx->lwrMsgLen;
    }
    else
    {
        kb = (SHA256_BLOCK_SIZE * 1 ) - pShaCtx->lwrMsgLen;
    }
    memset_hs(finalBlock, 0, SHA256_BLOCK_SIZE*2);
    finalBlock[0]=0x80;

    finalBlock[kb - 1] = pTot[0];
    finalBlock[kb - 2] = pTot[1];
    finalBlock[kb - 3] = pTot[2];
    finalBlock[kb - 4] = pTot[3];
    finalBlock[kb - 5] = pTot[4];
    finalBlock[kb - 6] = pTot[5];
    finalBlock[kb - 7] = pTot[6];
    finalBlock[kb - 8] = pTot[7];
    sha256Update_hs(finalBlock, kb, pShaCtx);

    UNPACK32(pShaCtx->H[0], &pDigest[ 0]);
    UNPACK32(pShaCtx->H[1], &pDigest[ 4]);
    UNPACK32(pShaCtx->H[2], &pDigest[ 8]);
    UNPACK32(pShaCtx->H[3], &pDigest[12]);
    UNPACK32(pShaCtx->H[4], &pDigest[16]);
    UNPACK32(pShaCtx->H[5], &pDigest[20]);
    UNPACK32(pShaCtx->H[6], &pDigest[24]);
    UNPACK32(pShaCtx->H[7], &pDigest[28]);
}

/*!
 * @brief Implements SHA 256 .
 */
void sha256_hs(LwU8 *pDigest, const LwU8 *pMessage, LwU32 len)
{
    SHA256_CTX ctx;

    sha256Init_hs(&ctx);
    sha256Update_hs(pMessage, len, &ctx);
    sha256Final_hs(&ctx, pDigest);
}

/*!
 * @brief Implements HMAC SHA 256 .
 */
void hmacSha256_hs(LwU8 *pMac, const LwU8 *pMsg, const LwU32 msgSize,
                    const LwU8 *pKey, const LwU32 keySize)
{
    LwU32 i;
    LwU8  K[SHA256_HMAC_BLOCK];
    LwU8  ipad[SHA256_HMAC_BLOCK], opad[SHA256_HMAC_BLOCK];
    LwU8  tmp_msg1[SHA256_HMAC_BLOCK + SHA256_MSG_BLOCK];
    LwU8  tmp_hash[SHA256_HASH_BLOCK];
    LwU8  tmp_msg2[SHA256_HMAC_BLOCK + SHA256_HASH_BLOCK];

    memset_hs(K, 0, SHA256_HMAC_BLOCK);
    memcpy_hs(K, pKey, (keySize < SHA256_HMAC_BLOCK) ? keySize : SHA256_HMAC_BLOCK);

    for (i = 0; i < SHA256_HMAC_BLOCK; i++)
    {
        ipad[i] = K[i] ^ 0x36;
        opad[i] = K[i] ^ 0x5c;
    }

    memcpy_hs(tmp_msg1, ipad, SHA256_HMAC_BLOCK);
    memcpy_hs(tmp_msg1 + SHA256_HMAC_BLOCK, pMsg, msgSize);

    sha256_hs(tmp_hash, tmp_msg1, SHA256_HMAC_BLOCK + msgSize);
    memcpy_hs(tmp_msg2, opad, SHA256_HMAC_BLOCK);
    memcpy_hs(tmp_msg2 + SHA256_HMAC_BLOCK, tmp_hash, SHA256_HASH_BLOCK);
    sha256_hs(pMac, tmp_msg2, SHA256_HMAC_BLOCK + SHA256_HASH_BLOCK);
}

