/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    sha256.c
 * @brief   Library to implement common SHA256 functions for uproc
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */
#include "sha256.h"
#include "sha256_const.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief SHA 256 Initialization function.
 */
void sha256Init(SHA256_CTX *pShaCtx)
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
void sha256Transform(SHA256_CTX *pShaCtx, LwU8 *pMsg)
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
void sha256Update(const LwU8 *pMsg, LwU32 len, SHA256_CTX *pShaCtx)
{
    LwU8 *pVmsg = NULL;
    LwU32 remLen = SHA256_BLOCK_SIZE - pShaCtx->lwrMsgLen;
    LwU32 tbLen  = (len > remLen)?remLen:len;
    shamemcpy(&(pShaCtx->block[pShaCtx->lwrMsgLen]), pMsg, tbLen);

    pShaCtx->totalMsgLen += len;
    if ((tbLen + pShaCtx->lwrMsgLen) != SHA256_BLOCK_SIZE)
    {
        pShaCtx->lwrMsgLen += tbLen;
        return;
    }

    pVmsg = pShaCtx->block;

    while (pVmsg)
    {
        sha256Transform(pShaCtx, pVmsg);
        pVmsg = (LwU8*)&(pMsg[tbLen]);
        remLen = len-tbLen;
        if (remLen < SHA256_BLOCK_SIZE)
        {
            // Copy the remaining message to block
            pShaCtx->lwrMsgLen = remLen;
            shamemcpy(pShaCtx->block, pVmsg, remLen);
            break;
        }
        tbLen += SHA256_BLOCK_SIZE;
    }
}

/*!
 * @brief SHA 256 Final function.
 */
void sha256Final(SHA256_CTX *pShaCtx, LwU8 *pDigest)
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
    shamemset(finalBlock, 0, SHA256_BLOCK_SIZE*2);
    finalBlock[0]=0x80;

    finalBlock[kb - 1] = pTot[0];
    finalBlock[kb - 2] = pTot[1];
    finalBlock[kb - 3] = pTot[2];
    finalBlock[kb - 4] = pTot[3];
    finalBlock[kb - 5] = pTot[4];
    finalBlock[kb - 6] = pTot[5];
    finalBlock[kb - 7] = pTot[6];
    finalBlock[kb - 8] = pTot[7];
    sha256Update(finalBlock, kb, pShaCtx);

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
void sha256(LwU8 *pDigest, const LwU8 *pMessage, LwU32 len)
{
    SHA256_CTX ctx;

    sha256Init(&ctx);
    sha256Update(pMessage, len, &ctx);
    sha256Final(&ctx, pDigest);
}

/*!
 * @brief Implements HMAC SHA 256 .
 */
void hmacSha256(LwU8 *pMac, const LwU8 *pMsg, const LwU32 msgSize,
                    const LwU8 *pKey, const LwU32 keySize)
{
    LwU32 i;
    LwU8  K[SHA256_HMAC_BLOCK] = {0};
    LwU8  ipad[SHA256_HMAC_BLOCK] = {0}; 
    LwU8  opad[SHA256_HMAC_BLOCK] = {0};
    LwU8  tmp_msg1[SHA256_HMAC_BLOCK + SHA256_MSG_BLOCK] = {0};
    LwU8  tmp_hash[SHA256_HASH_BLOCK] = {0};
    LwU8  tmp_msg2[SHA256_HMAC_BLOCK + SHA256_HASH_BLOCK] = {0};

    shamemset(K, 0, SHA256_HMAC_BLOCK);
    shamemcpy(K, pKey, (keySize < SHA256_HMAC_BLOCK) ? keySize : SHA256_HMAC_BLOCK);

    for (i = 0; i < SHA256_HMAC_BLOCK; i++)
    {
        ipad[i] = K[i] ^ 0x36;
        opad[i] = K[i] ^ 0x5c;
    }

    shamemcpy(tmp_msg1, ipad, SHA256_HMAC_BLOCK);
    shamemcpy(tmp_msg1 + SHA256_HMAC_BLOCK, pMsg, msgSize);

    sha256(tmp_hash, tmp_msg1, SHA256_HMAC_BLOCK + msgSize);
    shamemcpy(tmp_msg2, opad, SHA256_HMAC_BLOCK);
    shamemcpy(tmp_msg2 + SHA256_HMAC_BLOCK, tmp_hash, SHA256_HASH_BLOCK);
    sha256(pMac, tmp_msg2, SHA256_HMAC_BLOCK + SHA256_HASH_BLOCK);
}

