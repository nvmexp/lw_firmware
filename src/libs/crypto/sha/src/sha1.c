/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * Utility header file to generate a one-way hash from an arbitrary
 * byte array, using the Secure Hashing Algorithm 1 (SHA-1) as defined
 * in FIPS PUB 180-1 published April 17, 1995:
 *
 *   http://www.itl.nist.gov/fipspubs/fip180-1.htm
 *
 * Some common test cases (see Appendices A and B of the above document):
 *
 *   SHA1("abc") =
 *     A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
 *
 *   SHA1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") =
 *     84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */
#include "string.h"
#include "sha1.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
//
// Func name _sha1Transform is identical to lwSha1.h's and cause analyis
// failure. hdcp1.x/hdcp22 need lwSha1.h's sha1Generate with callback handler.
// TODO: check if hdcp1.x/hdcp22 can use libSha's sha1 without callback and 
// name as oringal _sha1Transform here.
//
static void _libSha1Transform(LwU32 *pState, LwU8 *pBuffer)
    GCC_ATTRIB_SECTION("imem_libSha1", "_libSha1Transform");

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Computation step as defined by SHA-1.
 *
 * Unlike the 64 byte buffer version outlined in the SHA-1 algorithm, this
 * function uses a 128 byte buffer to minimize the callwlation needed to
 * index the data.
 *
 * @param[in,out]  pState
 *         Pointer to State word array.
 *
 * @param[in]  pBuffer
 *      Data to operate on. 128 bytes in length. No length checking is done,
 *      and is assumed to have been done by the calling function.
 */
static void
_libSha1Transform
(
    LwU32 *pState,
    LwU8  *pBuffer
)
{
    LwU32 a = pState[0];
    LwU32 b = pState[1];
    LwU32 c = pState[2];
    LwU32 d = pState[3];
    LwU32 e = pState[4];
    LwU32 *pBuf = (LwU32 *)pBuffer;
    LwU32 *p;
    LwU32 i;
    LwU32 j;
    LwU32 k;

    for (i = 0; i < 80; i++)
    {
        p = &pBuf[i & 0xf];
        j = p[0];
        if (i < 16)
        {
            j = SHA1_REVERSE_BYTE_ORDER(j);
        }
        else
        {
            j ^= p[2] ^ p[8] ^ p[13];
            j = (j << 1) + (j >> 31);
        }
        p[0] = p[16] = j;
        if (i < 40)
        {
            if (i < 20)
            {
                k = 0x5a827999 + ((b & (c ^ d)) ^ d);
            }
            else
            {
                k = 0x6ed9eba1 + (b ^ c ^ d);
            }
        }
        else
        {
            if (i < 60)
            {
                k = 0x8f1bbcdc + (((b | c) & d) | (b & c));
            }
            else
            {
                k = 0xca62c1d6 + (b ^ c ^ d);
            }
        }
        j += (a << 5) + (a >> 27) + e + k;
        e = d;
        d = c;
        c = (b << 30) + (b >> 2);
        b = a;
        a = j;
    }
    pState[0] += a;
    pState[1] += b;
    pState[2] += c;
    pState[3] += d;
    pState[4] += e;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Initializes the SHA-1 context.
 *
 * @param[out] pContext
 *      Pointer to the context to initialize.
 */
void
sha1Initialize
(
    PSHA1_CONTEXT pContext
)
{
    pContext->count = 0;
    pContext->state[0] = SHA1_INIT_H0;
    pContext->state[1] = SHA1_INIT_H1;
    pContext->state[2] = SHA1_INIT_H2;
    pContext->state[3] = SHA1_INIT_H3;
    pContext->state[4] = SHA1_INIT_H4;
}


/*!
 * @brief Divides the input buffer into multiple 64-byte buffers and computes
 * the message digest for each.
 *
 * @param[in]  pContext
 *          Pointer to a SHA1_CONTEXT.
 *
 * @param[in]  pData
 *          Pointer to the data array to compute the message digest.
 *
 * @param[in]  len
 *          Size of the data.
 */
void
sha1Update
(
    PSHA1_CONTEXT pContext,
    void         *pData,
    LwU32         len
)
{
    LwU32 buffer_offset = (pContext->count & 63);
    LwU32 copy_size;
    LwU32 idx = 0;

    pContext->count += len;
    while ((buffer_offset + len) > 63)
    {
        copy_size = 64 - buffer_offset;
        memcpy(&pContext->buffer[buffer_offset], (LwU8*)pData + idx, copy_size);
        _libSha1Transform(pContext->state, pContext->buffer);
        buffer_offset = 0;
        idx += copy_size;
        len -= copy_size;
    }
    if (len > 0)
    {
        memcpy(&pContext->buffer[buffer_offset], (LwU8*)pData + idx, len);
    }
}

/*!
 * @brief Pads the message as specified by the SHA-1 algorithm and computes
 * the message digest on the final message chunk(s).
 *
 * @param[out] pDigest
 *          The SHA-1 hash values.
 *
 * @param[in]  pContext
 *          Pointer to a SHA1_CONTEXT.
 */
void
sha1Final
(
    LwU8         *pDigest,
    PSHA1_CONTEXT pContext
)
{
    LwU32  i;
    LwU32  bufferOffset = (pContext->count & 63);
    LwU8  *pBuffer = (LwU8*)&pContext->buffer[bufferOffset];
    LwU32 *pCount;
    LwU32 *pDig32;

    // append padding pattern to the end of input
    *pBuffer++ = 0x80;
    if (bufferOffset < 56)
    {
        memset(pBuffer, 0, 59 - bufferOffset);
    }
    else
    {
        // need an extra sha1_transform
        if (bufferOffset < 63)
        {
            memset(pBuffer, 0, 63 - bufferOffset);
        }
        _libSha1Transform(pContext->state, pContext->buffer);
        memset(pContext->buffer, 0, 60);
    }

    // set final count (this is the number of *bits* not *bytes*)
    pCount = (LwU32*)&pContext->buffer[15 << 2];
    *pCount = SHA1_REVERSE_BYTE_ORDER(pContext->count << 3);

    _libSha1Transform(pContext->state, pContext->buffer);

    // output hash with each dword in big endian
    if (pDigest)
    {
        pDig32 = (LwU32*) pDigest;
        for (i = 0; i < 5; i++)
        {
            pDig32[i] = SHA1_REVERSE_BYTE_ORDER(pContext->state[i]);
        }
    }
}

/*!
 * @brief   Generates the SHA-1 hash value on the data provided.
 *
 * @param[out]  pHash
 *          Pointer to store the hash array. The buffer must be 20 bytes in
 *          length, and the result is stored in big endian format.
 *
 * @param[in]   pData
 *          The source data array to transform.
 *
 * @param[in]   nBytes
 *          The size, in bytes, of the source data.
 */
void
sha1
(
    LwU8        *pHash,
    void        *pData,
    LwU32        nBytes
)
{
    SHA1_CONTEXT context;

    sha1Initialize(&context);
    sha1Update(&context, pData, nBytes);
    sha1Final(pHash, &context);
}

/*!
 * @brief   Generates the HMAC-SHA-1 hash value on the data provided.
 *
 * @param[out]  pHash
 *          Pointer to store the hash array. The buffer must be 20 bytes in
 *          length, and the result is stored in big endian format.
 *
 * @param[in]   pMessage
 *          The Message data array to transform.
 *
 * @param[in]   nMessageBytes
 *          The size, in bytes, of the Message data.
 *
 * @param[in]   pKey
 *          The Key data array to transform.
 *
 * @param[in]   nKeyBytes
 *          The size, in bytes, of the Key data.
 */
void hmacSha1
(
    LwU8   pHash[SHA1_DIGEST_LENGTH],
    void  *pMessage,
    LwU32  nMessageBytes,
    void  *pKey,
    LwU32  nKeyBytes
)
{
    LwU8  K0[SHA1_BLOCK_LENGTH] = {0};
    LwU8  ipad[SHA1_BLOCK_LENGTH] = {0};
    LwU8  opad[SHA1_BLOCK_LENGTH] = {0};
    LwU8  tmpHash[SHA1_DIGEST_LENGTH] = {0};
    LwU8  tmpMsg1[SHA1_BLOCK_LENGTH + SHA1_MESSAGE_LENGTH] = {0};
    LwU8  tmpMsg2[SHA1_BLOCK_LENGTH + SHA1_DIGEST_LENGTH] = {0};
    LwU32 i;

    //
    // FIPS Publication 198 Section 5: HMAC Specification
    // Step 1-3: Determine K0
    //
    memset(K0, 0, SHA1_BLOCK_LENGTH);
    if (nKeyBytes <= SHA1_BLOCK_LENGTH)
    {
        memcpy(K0, pKey, nKeyBytes);
    }
    else
    {
        sha1(K0, pKey, nKeyBytes);
    }

    //
    // Step 4: K0 ^ ipad
    // Step 7: K0 ^ opad
    //
    for (i = 0; i < SHA1_BLOCK_LENGTH; i++)
    {
        ipad[i] = K0[i] ^ 0x36;
        opad[i] = K0[i] ^ 0x5c;
    }

    // Step 5: Append the stream data to the result of Step 4
    memcpy(tmpMsg1, ipad, SHA1_BLOCK_LENGTH);
    memcpy(tmpMsg1 + SHA1_BLOCK_LENGTH, pMessage, nMessageBytes);

    // Step 6: Apply SHA-1 hash to the stream generated in Step 5
    sha1(tmpHash, tmpMsg1, SHA1_BLOCK_LENGTH + nMessageBytes);

    // Step 8: Append the result
    memcpy(tmpMsg2, opad, SHA1_BLOCK_LENGTH);
    memcpy(tmpMsg2 + SHA1_BLOCK_LENGTH, tmpHash, SHA1_DIGEST_LENGTH);

    // Step 9: Apply SHA-1 hash to the result from Step 8
    sha1(pHash, tmpMsg2, SHA1_BLOCK_LENGTH + SHA1_DIGEST_LENGTH);
}

