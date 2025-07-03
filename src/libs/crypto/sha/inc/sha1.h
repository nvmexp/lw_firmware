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

#ifndef SHA1_H
#define SHA1_H

#include "lwuproc.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*
 * The size of each key blob used during SHA1 operation.
 */
#define SHA1_BLOCK_LENGTH   64
#define SHA1_DIGEST_LENGTH  20
#define SHA1_MESSAGE_LENGTH 12

/*
 * The following values are defined by the SHA-1 algorithm for initial values.
 */
#define SHA1_INIT_H0    0x67452301  //!< Initial H0 value
#define SHA1_INIT_H1    0xEFCDAB89  //!< Initial H1 value
#define SHA1_INIT_H2    0x98BADCFE  //!< Initial H2 value
#define SHA1_INIT_H3    0x10325476  //!< Initial H3 value
#define SHA1_INIT_H4    0xC3D2E1F0  //!< Initial H4 value

/*!
 * @brief Reverses the byte order of a word; that is, switching the endianness
 * of the word.
 *
 * @param[in]   a   A 32-bit word
 *
 * @returns The 32-bit word with its byte order reversed.
 */
#define SHA1_REVERSE_BYTE_ORDER(a) \
    (((a) >> 24) | ((a) << 24) | (((a) >> 8) & 0xFF00) | (((a) << 8) & 0xFF0000))

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief Structure used by the SHA-1 functions to maintain the state of the
 * callwlations.
 */
typedef struct
{
    LwU32  state[5];
    LwU32  count;
    LwU8   buffer[128];
} SHA1_CONTEXT, *PSHA1_CONTEXT;

/* ------------------------- Prototypes ------------------------------------- */
void sha1(LwU8 *pHash, void *pData, LwU32 nBytes)
    GCC_ATTRIB_SECTION("imem_libSha1", "sha1");

void sha1Initialize(PSHA1_CONTEXT pContext)
    GCC_ATTRIB_SECTION("imem_libSha1", "sha1Initialize");
void sha1Update(PSHA1_CONTEXT pContext, void *pData, LwU32 len)
    GCC_ATTRIB_SECTION("imem_libSha1", "sha1Update");
void sha1Final(LwU8 *pDigest, PSHA1_CONTEXT pContext)
    GCC_ATTRIB_SECTION("imem_libSha1", "sha1Final");

void hmacSha1(LwU8 pHash[SHA1_DIGEST_LENGTH], void *pMessage, LwU32 nMessageBytes, void *pKey, LwU32 nKeyBytes)
    GCC_ATTRIB_SECTION("imem_libSha1", "hmacSha1");

#endif // SHA1_H
