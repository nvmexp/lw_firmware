/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */
#include "bigint.h"
#include "sha256.h"
#include "hdcpmc/hdcpmc_crypt.h"
#include "hdcpmc/hdcpmc_mem.h"
#include "hdcpmc/hdcpmc_rsa.h"
#include "hdcpmc/hdcpmc_data.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * OAEP_HLEN ("hlen" in PKCS #1 v2.1 page 4) is the length in octets of
 * the specified hash function. DCP chooses SHA-256, hence it is 32 octets.
 */
#define OAEP_HLEN                                                            32

/*!
 * OAEP_CLEN is the length in octets of the "counter" (PKCS #1 v2.1 page 48)
 * of the MGF1 function. DCP chooses 32-bit counter, hence it is 4 octets.
 */
#define OAEP_CLEN                                                             4

/*!
 * This buffer size is sufficient for 1024-bit RSAES-OAEP decryption
 */
#define SEED_BUFFER                                                         128
#define MAX_SEEDLEN                                   (SEED_BUFFER - OAEP_CLEN)

#define RSAOAEP_BUFFERSIZE                                                  128

/*!
 * AES block size constant
 */
#define AES_B                                                                16

/*!
 * Gets the offset of a data member of a struture.
 *
 * @param[in] member  The data member to get the offset of.
 * @param[in] type    The data structure type to query.
 *
 * @return The byte offset within the structure the data member begins.
 */
#define OFFSET_OF(member, type)      (((LwU8*)&(((type*)0)->member)) - (LwU8*)0)

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _hdcpOaepMaskGenFunction(LwU8 *pMask, LwU32 maskLen, LwU8 *pSeed, LwU32 seedLen)
    GCC_ATTRIB_SECTION("imem_hdcpRsa", "_hdcpOaepMaskGenFunction");
static FLCN_STATUS _hdcpRsaOaepEncode(LwU8 *pOutput, const LwU8 *pInput, const LwU32 dbsize)
    GCC_ATTRIB_SECTION("imem_hdcpRsa", "_hdcpRsaOaepEncode");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * oaep_lhash is the hash of a "label" string defined in PKCS #1 v2.1 page 18.
 * DCP chooses this value to be the SHA-256 of the empty string.
 */
static LwU8 oaep_lhash[OAEP_HLEN] =
{
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
    0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
    0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
    0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
};

/*!
 * Fixed seed value for RSA-OAEP
 */
static LwU8 HDCP2_RSA_OAEP_Test_Seed[OAEP_HLEN] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};

/*!
 * Arrays used for testing, defines for certificates, reference Ekpub values, etc
 */
static LwU8 T_prefix[] =
{
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x04, 0x20,
};

/* ------------------------- Private Functions ------------------------------ */
/*!
 * MGF1 is a "Mask Generation Function" based on a hash function
 * see PKCS #1 v2.1 page 48. DCP chooses SHA-256 with 32-bit counter.
 *
 * @param[out]  pMask    Buffer to store the generated mask.
 * @param[in]   maskLen  Length of the mask buffer.
 * @param[in]   pSeed    Buffer containing the seed for the mask.
 * @param[in]   seedLen  Length of the seed buffer.
 *
 * @return FLCN_OK if the mask was generated; FLCN_ERR_ILWALID_ARGUMENT if
 *         the seed length is greater than the maximum allowable seed length.
 */
static FLCN_STATUS
_hdcpOaepMaskGenFunction
(
    LwU8  *pMask,
    LwU32  maskLen,
    LwU8  *pSeed,
    LwU32  seedLen
)
{
    LwU32 i;
    LwU32 bytesToCopy;

    LwU8 mgfseed[SEED_BUFFER];
    LwU8 mgfhash[OAEP_HLEN];

    if (seedLen > MAX_SEEDLEN)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memcpy(mgfseed, pSeed, seedLen);

    for (i = 0; maskLen > 0; i++)
    {
        mgfseed[seedLen]     = (LwU8)(i >> 24);
        mgfseed[seedLen + 1] = (LwU8)(i >> 16);
        mgfseed[seedLen + 2] = (LwU8)(i >> 8);
        mgfseed[seedLen + 3] = (LwU8)(i);

        sha256(mgfhash, mgfseed, seedLen + OAEP_CLEN);

        bytesToCopy = (maskLen >= OAEP_HLEN) ? OAEP_HLEN : maskLen;
        memcpy(pMask, mgfhash, bytesToCopy);
        maskLen -= bytesToCopy;
        pMask   += bytesToCopy;
    }

    return FLCN_OK;
}

/*!
 * @brief Encodes the data using the RSAES-OAEP method.
 *
 * @param[out] pOutput  Pointer to buffer to store encoded data.
 * @param[in]  pInput   Pointer to buffer to encode.
 * @param[in]  dbsize   Size of the buffer.
 *
 * @return FLCN_OK if sucessful; FLCN_ERR_ILWALID_ARGUMENT if the size of
 *         buffer is too large to encode.
 */
static FLCN_STATUS
_hdcpRsaOaepEncode
(
    LwU8        *pOutput,
    const LwU8  *pInput,
    const LwU32  dbsize
)
{
    LwU8  seed[OAEP_HLEN];
    LwU8  seedMask[OAEP_HLEN];
    LwU8  db[RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1];
    LwU8  dbMask[RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1];
    LwU32 i;

    if (dbsize > (RSAOAEP_BUFFERSIZE - (2 * OAEP_HLEN) - 2))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pOutput[0] = 0;

    memset(db, 0, sizeof(db));
    memcpy(db, (LwU8*)oaep_lhash, OAEP_HLEN);
    memcpy(db + sizeof(db) - dbsize, (LwU8*)pInput, dbsize);
    db[sizeof(db) - dbsize - 1] = 0x1;

    memcpy(seed, (LwU8*)HDCP2_RSA_OAEP_Test_Seed, OAEP_HLEN);
    _hdcpOaepMaskGenFunction(dbMask, sizeof(dbMask), seed, sizeof(seed));

    for (i = 0; i < sizeof(dbMask); i++)
    {
        pOutput[i + OAEP_HLEN + 1] = db[i] ^ dbMask[i];
    }

    _hdcpOaepMaskGenFunction(seedMask, sizeof(seedMask), pOutput + OAEP_HLEN + 1, sizeof(dbMask));
    for (i = 0; i < sizeof(seedMask); i++)
    {
        pOutput[i + 1] = seedMask[i] ^ seed[i];
    }

    return FLCN_OK;
}

/*!
 * @brief Decodes the data using the RSAES-OAEP method.
 *
 * @param[out] pOutput  Pointer to buffer to store decoded data.
 * @param[in]  pInput   Pointer to buffer to decode.
 *
 * @return FLCN_OK.
 */
static FLCN_STATUS
_hdcpRsaOaepDecode
(
    LwU8       *pOutput,
    const LwU8 *pInput
)
{
    LwU8 y;
    LwU8 seed[OAEP_HLEN];
    LwU8 seedMask[OAEP_HLEN];
    LwU8 maskedSeed[OAEP_HLEN];
    LwU8 db[RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1];
    LwU8 maskedDb[RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1];
    LwU8 dbMask[RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1];
    LwU32 msgLen;
    LwU32 dbLen = RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1;
    LwU32 i;

    y = pInput[0];
    memset(maskedSeed, 0, OAEP_HLEN);
    memset(seed, 0, OAEP_HLEN);
    memset(seedMask, 0, OAEP_HLEN);
    memset(db, 0, RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1);
    memset(maskedDb, 0, RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1);
    memset(dbMask, 0, RSAOAEP_BUFFERSIZE - OAEP_HLEN - 1);

    memcpy(maskedSeed, &pInput[sizeof(y)], OAEP_HLEN);
    memcpy(maskedDb, &pInput[sizeof(y) + OAEP_HLEN], dbLen);

    _hdcpOaepMaskGenFunction(seedMask, OAEP_HLEN, maskedDb, dbLen);

    for (i = 0; i < OAEP_HLEN; i++)
    {
        seed[i] = seedMask[i] ^ maskedSeed[i];
    }

    _hdcpOaepMaskGenFunction(dbMask, dbLen, seed, OAEP_HLEN);
    for (i = 0; i < dbLen; i++)
    {
        db[i] = dbMask[i] ^ maskedDb[i];
    }
    for (i = OAEP_HLEN; i < dbLen; i++)
    {
        if (db[i] != 0x00)
        {
            break;
        }
    }
    if ((i == dbLen) || (db[i++] != 0x01))
    {
        return FLCN_ERROR;
    }

    msgLen = dbLen - i;
    if (i < dbLen)
    {
        memcpy((LwU8*)pOutput, &db[i], msgLen);
    }

    return FLCN_OK;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Function to encrypt the master key (Km).
 *
 * @param[out]  pEkpub     Pointer to encrypted version of Km.
 * @param[in]   pKm        Pointer to unencrypted version of Km.
 * @param[in]   pModulus   Modulus used to encrypt.
 * @param[in]   pExponent  Exponent used to encrypt.
 *
 * @return LW_TRUE if Km is encrypted; value pointed to by pEkpub is valid.
 *         LW_FALSE if Km was not encrypted; value pointed to by pEkpub is not
 *         valid.
 */
LwBool
hdcpRsaOaep
(
    LwU8       *pEkpub,
    const LwU8 *pKm,
    const LwU8 *pModulus,
    const LwU8 *pExponent
)
{
    LwU8 x[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 x_c[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);

    LwU32 exponent = 0;
    BigIntModulus modulus;

    bigIntModulusInit(&modulus, pModulus, HDCP_SIZE_RX_MODULUS);
    if (FLCN_OK != _hdcpRsaOaepEncode(x_c, pKm, AES_B))
    {
        return LW_FALSE;
    }

    memcpy(&exponent, pExponent, HDCP_SIZE_RX_EXPONENT);
    hdcpSwapEndianness(x, x_c, sizeof(x));

    bigIntPowerMod((LwU32*)x_c, (LwU32*)x, &exponent, &modulus, 1);
    hdcpSwapEndianness(pEkpub, x_c, RSAOAEP_BUFFERSIZE);

    return LW_TRUE;
}

/*!
 * @brief Function to decrypt the master key (Km).
 *
 * @param[in]   p     P value (stored as big endian number).
 * @param[in]   q     Q value (stored as big endian number).
 * @param[in]   dP    D%P value (stored as big endian number).
 * @param[in]   dQ    D%Q value (stored as big endian number).
 * @param[in]   qIlw  (Q^-1)%P value (stored as big endian number).
 * @param[in]   pEkm  Buffer containing the encrypted master key (Ekm).
 * @param[out]  pKm   Buffer to store the unecrypted master key (Km).
 * @param[in]   n     Modulus value (stored as big endian number).
 *
 * @return LW_TRUE if Km is decrypted; value pointed to by pKm is valid.
 *         LW_FALSE if Km was not decrypted; value pointed to by pKm is not
 *         valid.
 */
LwBool
hdcpRsaOaepDecrypt
(
    const LwU8 *p,
    const LwU8 *q,
    const LwU8 *dP,
    const LwU8 *dQ,
    const LwU8 *qIlw,
    const LwU8 *pEkm,
    LwU8 *pKm,
    const LwU8 *n
)
{
    LwU8 m1[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 m2[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 m[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 h[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 diff[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 qh[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 Q[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 ekmModP[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 ekmModQ[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 qIlwModP[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);
    LwU32 dwPExponent[HDCP_SIZE_LwU32(HDCP_SIZE_RX_PRIV_EXPONENT)] GCC_ATTRIB_ALIGN(4);
    LwU32 dwQExponent[HDCP_SIZE_LwU32(HDCP_SIZE_RX_PRIV_EXPONENT)] GCC_ATTRIB_ALIGN(4);
    BigIntModulus pModulus;
    BigIntModulus qModulus;
    BigIntModulus nModulus;
    LwU8 enc_km[RSAOAEP_BUFFERSIZE] GCC_ATTRIB_ALIGN(4);

    bigIntModulusInit(&pModulus, p, HDCP_SIZE_P);
    bigIntModulusInit(&qModulus, q, HDCP_SIZE_Q);
    bigIntModulusInit(&nModulus, n, HDCP_SIZE_RX_MODULUS);
    memset(diff, 0x0, RSAOAEP_BUFFERSIZE);
    memset(qIlwModP, 0x0, RSAOAEP_BUFFERSIZE);
    memset(h, 0x0, RSAOAEP_BUFFERSIZE);
    memset(Q, 0x0, RSAOAEP_BUFFERSIZE);
    memset(m1, 0x0, RSAOAEP_BUFFERSIZE);
    memset(m2, 0x0, RSAOAEP_BUFFERSIZE);
    memset(m, 0x0, RSAOAEP_BUFFERSIZE);
    memset(qh, 0x0, RSAOAEP_BUFFERSIZE);
    memset(ekmModP, 0x0, RSAOAEP_BUFFERSIZE);
    memset(ekmModQ, 0x0, RSAOAEP_BUFFERSIZE);
    memcpy((LwU8*)dwPExponent, dP, HDCP_SIZE_RX_PRIV_EXPONENT);
    memcpy((LwU8*)dwQExponent, dQ, HDCP_SIZE_RX_PRIV_EXPONENT);
    memcpy((LwU8*)qIlwModP, qIlw, HDCP_SIZE_RX_PRIV_EXPONENT);
    memcpy((LwU8*)Q, q, HDCP_SIZE_RX_PRIV_EXPONENT);

    hdcpSwapEndianness(enc_km, pEkm, RSAOAEP_BUFFERSIZE);
    hdcpSwapEndianness((LwU8*)dwPExponent, (LwU8*)dwPExponent,
                       HDCP_SIZE_RX_PRIV_EXPONENT);
    hdcpSwapEndianness((LwU8*)dwQExponent, (LwU8*)dwQExponent,
                       HDCP_SIZE_RX_PRIV_EXPONENT);
    hdcpSwapEndianness(qIlwModP, qIlwModP, HDCP_SIZE_RX_PRIV_EXPONENT);
    hdcpSwapEndianness(Q, Q, HDCP_SIZE_RX_PRIV_EXPONENT);

    bigIntMod((LwU32*)ekmModP, (LwU32*)enc_km, &pModulus,
              HDCP_SIZE_LwU32(HDCP_SIZE_RX_MODULUS));
    bigIntMod((LwU32*)ekmModQ, (LwU32*)enc_km, &qModulus,
              HDCP_SIZE_LwU32(HDCP_SIZE_RX_MODULUS));

    bigIntPowerMod((LwU32*)m1, (LwU32*)ekmModP, dwPExponent, &pModulus,
                   HDCP_SIZE_LwU32(HDCP_SIZE_RX_PRIV_EXPONENT));
    bigIntPowerMod((LwU32*)m2, (LwU32*)ekmModQ, dwQExponent, &qModulus,
                   HDCP_SIZE_LwU32(HDCP_SIZE_RX_PRIV_EXPONENT));
    bigIntSubtractMod((LwU32*)diff, (LwU32*)m1, (LwU32*)m2, &pModulus);
    bigIntMultiplyMod((LwU32*)h, (LwU32*)diff, (LwU32*)qIlwModP, &pModulus);
    bigIntMultiplyMod((LwU32*)qh, (LwU32*)h, (LwU32*)Q, &nModulus);
    bigIntAdd((LwU32*)m, (LwU32*)qh, (LwU32*)m2,
              HDCP_SIZE_LwU32(HDCP_SIZE_RX_MODULUS));
    hdcpSwapEndianness(m, m, RSAOAEP_BUFFERSIZE);

    if (FLCN_OK != _hdcpRsaOaepDecode(pKm, m))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief Validates the certificate using RSA-SSA-3072: RFC 3447.
 *
 * @param[in] pModulusRoot  Modulus root to use to validate the certificate.
 * @param[in] exponentRoot  Exponent to use to validate the certificate.
 * @param[in] pCert         Pointer to the certificate to validate.
 * @param[in] pHash         Optional hash to compare against.
 *
 * @return LW_TRUE if the certificate is valid; LW_FALSE otherwise.
 */
LwBool
hdcpVerifyCertificate
(
    const LwU8             *pModulusRoot,
    const LwU32             exponentRoot,
    const HDCP_CERTIFICATE *pCert,
    LwU8                   *pHash
)
{
    LwU32   psSize;
    LwU8   *pFinalHash = NULL;
    LwU8    hash[SHA256_HASH_BLOCK];
    LwU8   *pEM  = hdcpTempBuffer;
    LwU8   *temp = hdcpTempBuffer + ALIGN_UP(HDCP_SIZE_RX_MODULUS_ROOT, 16);
    LwU32   exponent[1];
    BigIntModulus modulus;

    exponent[0] = exponentRoot;

    if (NULL != pHash)
    {
        pFinalHash = pHash;
    }
    else
    {
        sha256(hash, (LwU8*)pCert, OFFSET_OF(signature, HDCP_CERTIFICATE));
        pFinalHash = hash;
    }

    //
    // Prepare encoded message EM: see RFC 3447 section 9.2: EMSA-PKCS1-v1_5
    // EM = 0x00 || 0x01 || PS || 0x00 || T.
    //
    psSize = HDCP_SIZE_RX_MODULUS_ROOT - 3 - sizeof(T_prefix) - SHA256_HASH_BLOCK;
    pEM[0] = 0x00;
    pEM[1] = 0x01;
    memset(&pEM[2], 0xff, psSize);
    pEM[2 + psSize] = 0x00;
    memcpy(&pEM[3 + psSize], T_prefix, sizeof(T_prefix));
    memcpy(&pEM[3 + psSize + sizeof(T_prefix)], pFinalHash, SHA256_HASH_BLOCK);

    //
    // RSA signature verification - raise signature to the power of exponent and
    // compare with EM. Note that all numbers are big-endian encoded under
    // PKCS1-v1_5.
    //
    bigIntModulusInit(&modulus, pModulusRoot, HDCP_SIZE_RX_MODULUS_ROOT);
    hdcpSwapEndianness(temp, pCert->signature, HDCP_SIZE_RX_MODULUS_ROOT);
    bigIntPowerMod((LwU32*)temp, (LwU32*)temp, exponent, &modulus, 1);
    hdcpSwapEndianness(temp, temp, HDCP_SIZE_RX_MODULUS_ROOT);

    if (0 != memcmp(temp, pEM, HDCP_SIZE_RX_MODULUS_ROOT))
    {
        return LW_FALSE;
    }
    else
    {
        return LW_TRUE;
    }
}
#endif
