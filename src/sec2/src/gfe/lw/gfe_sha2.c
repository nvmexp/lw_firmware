/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   gfe_sha2.c
 * @brief  Functions that use the SHA256 library for RSA sign
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

#include "lwosselwreovly.h"
/* ------------------------- Application Includes --------------------------- */
#include "gfe/gfe_mthds.h"
#include "sha256.h"
#include "scp_rand.h"
#include "scp_internals.h"

/* ------------------------- Macros and Defines ----------------------------- */

//
// OAEP_CLEN is the length in octets of the "counter" (PKCS #1 v2.1 page 48)
// of the MGF1 function. DCP chooses 32-bit counter, hence it is 4 octets.
//
#define OAEP_CLEN       4

//
// This buffer size is sufficient for 1024-bit RSAES-OAEP decryption
//
#define SEED_BUFFER     128
#define MAX_SEEDLEN     (SEED_BUFFER - OAEP_CLEN)

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Static Variables ------------------------------- */
/* Buffer used to get a random number */
static LwU8 _gfeRandBuf[SCP_RAND_NUM_SIZE_IN_BYTES]
    GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_gfe", "_gfeRandBuf");

// TODO for lvaz by 07/10/2016: Move gfeGetRandNo out of GFE into a common usable location by other LS code
/*!
 * gfeGetRandNo
 *
 * @brief Get Random number
 *
 * @param[in] pDest    Destination for random number
 * @param[in] Size     Size of random number in bytes
 */
FLCN_STATUS
gfeGetRandNo
(
    LwU8 *pDest,
    LwS32 size
)
{
    LwU32       nBytesToCopy;

    if ((pDest == NULL) || (size < 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    while (size > 0)
    {
        // SCP generates 128-bit random numbers
        scpGetRand128Inline(_gfeRandBuf);

        nBytesToCopy = LW_MIN(size, sizeof(_gfeRandBuf));
        memcpy(pDest, _gfeRandBuf, nBytesToCopy);
        size -= nBytesToCopy;
        pDest += nBytesToCopy;
    }
    return FLCN_OK;
}

//
// from hdcprsa.c: hdcpOaepMaskGenFunction... redone for gfe
// MGF1 is a "Mask Generation Function" based on a hash function
// see PKCS #1 v2.1 page 48. DCP chooses SHA-256 with 32-bit counter.
//
static FLCN_STATUS
gfeEmsaPssMaskGenFunction
(
    LwU8 *pMask,
    LwS32 nMaskLen,
    LwU8 *pSeed,
    LwS32 nSeedLen
)
{
    LwS32 i;
    LwS32 nBytesToCopy;
    LwS32 oaep_hLen = SHA256_HASH_BLOCK_SIZE;

    LwU8 mgfseed[SEED_BUFFER];
    LwU8 mgfhash[SHA256_HASH_BLOCK_SIZE];

    if (nSeedLen > MAX_SEEDLEN)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memcpy(mgfseed, pSeed, nSeedLen);

    for (i = 0; nMaskLen > 0; i++)
    {
        // The 32-bit counter must be appended to the seed in big-endian order
        mgfseed[nSeedLen]   = (LwU8)(i >> 24);
        mgfseed[nSeedLen+1] = (LwU8)(i >> 16);
        mgfseed[nSeedLen+2] = (LwU8)(i >> 8 );
        mgfseed[nSeedLen+3] = (LwU8)(i      );

        sha256(mgfhash, mgfseed, nSeedLen + OAEP_CLEN);

        nBytesToCopy = (nMaskLen >= oaep_hLen) ? oaep_hLen : nMaskLen;
        memcpy(pMask, mgfhash, nBytesToCopy);
        nMaskLen -= nBytesToCopy;
        pMask    += nBytesToCopy;
    }

    return FLCN_OK;
}

//
// EMSA-PSS-ENCODE (M, emBits)
//
//   Options:
//   Hash     hash function (hLen denotes the length in octets of the hash
//            function output) (can we base this/use sha256)
//   MGF      mask generation function (can we base this/use on hdcpOaepMaskGenFunction)
//   sLen     intended length in octets of the salt
//
//   Input:
//   pMsg(M) message to be encoded, an octet string
//   emBits   maximal bit length of the integer OS2IP (EM) (see Section
//            4.2), at least 8hLen + 8sLen + 9
//
//   Output:
//   EM       encoded message, an octet string of length emLen = \ceil
//            (emBits/8)
//
//   Errors:  "encoding error"; "message too long"
//   FLCN_ERROR
//
// Warning: change from emBits to emLen... so only strings which are on byte boundarys work.
//
FLCN_STATUS
gfeEmsaPssEncode
(
    LwU8 *pMsg,
    LwU32 msgLen,
    LwU8 *pEm,
    LwU32 emLen
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        dbMask[GFE_RSA_SIGN_SIZE - SHA256_HASH_BLOCK_SIZE - 1];
    LwU8        salt[SHA256_HASH_BLOCK_SIZE];
    LwS32       hLen = SHA256_HASH_BLOCK_SIZE;
    LwS32       sLen = SHA256_HASH_BLOCK_SIZE;
    LwS32       psLen;
    LwS32       maskSize;
    LwS32       i;
    LwS32       msbit;
    LwU32       bitcount;
    SHA256_CTX  ctx;

    //1.  If the length of M is greater than the input limitation for the
    //    hash function (2^61 - 1 octets for SHA-1), output "message too long" and stop.
    if (hLen > emLen)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto gfeEmsaPssEncode_exit;
    }

    // Note: This value is used near the end to shift 0's into the first byte.
    //       The bit count is based on the key length (1024 vs 2048)...
    bitcount = RSA_KEY_SIZE;
    msbit = (bitcount - 1) & 0x7;

    //
    // This code below is part of the general RSA padding algorithm.
    // Commenting it out since the key size is a power of 2, so the
    // condition is always false.
    //
    // startbyte = 0;
    // if (msbit == 0)
    // {
    //    // If there are no high bits shifted in then we need the whole first byte set to 0.
    //     startbyte++;
    //     emLen--;
    // }

    // 2.  Let pMsg = Hash(M), an octet string of length hLen.

    // 3.  If emLen < hLen + sLen + 2, output "encoding error" and stop.
    // in other words, if the emLen (emBits colwerted to bytes) is smaller than the hash size 32 plus salt size (8) plus 2... give an error
    if (emLen < hLen + sLen + 2)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto gfeEmsaPssEncode_exit;
    }

    // 4.  Generate a random octet string salt of length sLen; if sLen = 0,
    //     then salt is the empty string.
    // Note: We always use a salt the length of the current hash.
    status = gfeGetRandNo(salt, sLen);
    if (status != FLCN_OK)
    {
        goto gfeEmsaPssEncode_exit;
    }

    // Note: Clear whole of the output message pEm... so we can use it in the next several steps including
    //       1) The first 8 bytes of zeros that go into the digest (H)
    //       2) The storage used named db below.
    memset(pEm, 0, emLen);
    // Note: Determine the region to mask.
    maskSize = emLen - hLen - 1;

    // 5.  Let
    //       M' = (0x)00 00 00 00 00 00 00 00 || pMsg || salt;
    //       M' is an octet string of length 8 + hLen + sLen with eight initial zero octets.

    // 6.  Let H = Hash(M'), an octet string of length hLen.

    // Note: Modification from original plan of making one long string with 8 0's followed by
    //       original message (pMsg) then salt and hashing the whole string.
    //       This can be done separately with several calls to sha256Update... saving stack space.

    sha256Init(&ctx);
    // start with the hash of the starting 8 zeros (pEm was cleared above).
    sha256Update(pEm, 8, &ctx);
    // add the hash of the actual message (pMsg)
    sha256Update(pMsg, msgLen, &ctx);
    // then hash the salt.
    sha256Update(salt, sLen, &ctx);
    // Put the hash directly into pEm where it needs to be. This is right after the masked portion of pEm.
    // This hash is used as an input (the seed) to the MGF function.
    sha256Final(&ctx, &pEm[maskSize]);

    // 7.  Generate an octet string PS consisting of emLen - sLen - hLen - 2
    //     zero octets.  The length of PS may be 0.
    // Note: We cleared pEm before the hash... so we just need to know how long it is.
    psLen = emLen - sLen - hLen - 2;

    // 8.  Let DB = PS || 0x01 || salt; DB is an octet string of length emLen - hLen - 1.
    // Note: I'm using pEm instead of another string db, saving stack space.
    pEm[psLen] = 0x1;
    memcpy(pEm + psLen + 1, salt, sLen);

    // 9.  Let dbMask = MGF(H, emLen - hLen - 1).
    // Notes:
    //   The output string is called dbMask, it's length is maskSize.
    //   The input seed to the MGF is H (stored at &pEm[masksize]).
    //   The length of the seed is hLen.
    gfeEmsaPssMaskGenFunction(dbMask, maskSize, &pEm[maskSize], hLen);

    // 10. Let maskedDB = DB \xor dbMask.
    // Note: Using pEm as DB and the final destination array.
    for (i = 0; i < maskSize; i++)
    {
        pEm[i] ^= dbMask[i];
    }

    // 11. Set the leftmost 8emLen - emBits bits of the leftmost octet in maskedDB to zero.
    pEm[0] &= 0xff >> (8 - msbit);

    // 12. Let EM = maskedDB || H || 0xbc.
    // 13. Output EM.
    // Note: We've used pEm since the hash digest so no need to copy. Last byte is 0xbc.
    pEm[maskSize + hLen] = 0xbc;

    status = FLCN_OK;

gfeEmsaPssEncode_exit:
    return status;
}

