/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
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
#include "rmsec2cmdif.h"
#include "lwosselwreovly.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwos_dma.h"
#include "bigint.h"
#include "sha1.h"
#include "sha256.h"

#include "hdcpmc/hdcpmc_constants.h"
#include "hdcpmc/hdcpmc_crypt.h"
#include "hdcpmc/hdcpmc_data.h"
#include "hdcpmc/hdcpmc_mem.h"
#include "hdcpmc/hdcpmc_srm.h"
#include "hdcpmc/hdcpmc_rsa.h"

/* ------------------------- Macros and Defines ----------------------------- */
// SRM Header
#define LW_HDCP_SRM_HEADER_BITS_ID                                        31:28
#define LW_HDCP_SRM_HEADER_BITS_HDCP2_IND                                 27:24
#define LW_HDCP_SRM_HEADER_BITS_HDCP2_IND_TRUE                              0x1
#define LW_HDCP_SRM_HEADER_BITS_HDCP2_IND_FALSE                             0x0
#define LW_HDCP_SRM_HEADER_BITS_RESERVED                                  23:16
#define LW_HDCP_SRM_HEADER_BITS_VERSION                                    15:0

// SRM Header: 2nd Word
#define LW_HDCP_SRM_1X_BITS_GENERATION                                    31:24
#define LW_HDCP_SRM_1X_BITS_LENGTH                                         23:0

// SRM Minimum Lengths
#define HDCP_SRM_1X_FIRST_GEN_MIN_LEN                                      0x2B
#define HDCP_SRM_1X_NEXT_GEN_MIN_LEN                                       0x2A
#define HDCP_SRM_2X_FIRST_GEN_MIN_LEN                                     0x187
#define HDCP_SRM_2X_NEXT_GEN_MIN_LEN                                      0x184

// SRM Version 2 Next Gen Header Fields
#define LW_HDCP_SRM_2X_NEXT_GEN_LENGTH                                    31:16
#define LW_HDCP_SRM_2X_NEXT_GEN_RESERVED                                  15:10
#define LW_HDCP_SRM_2X_NEXT_GEN_NUM_DEVICES                                 9:0

// Various sizes for SRM values
#define SRM_MODULUS_SIZE                                                    128
#define SRM_DIVISOR_SIZE                                                     20
#define SRM_GENERATOR_SIZE                                                  128
#define SRM_PUBLIC_KEY_SIZE                                                 128
#define SRM_DSA_SIGNATURE_SIZE                                          (320/8)
#define SRM_DCP_SIGNATURE_SIZE                                         (3072/8)

/*!
 * @brief Reverses the byte order of a 32-bit integer in place.
 *
 * @param[in/out]  a  32-bit integer to have it's byte order swapped.
 */
#define REVERSE_BYTE_ORDER(a) \
    (((a) >> 24) | ((a) << 24) | (((a) >> 8) & 0xFF00) | \
    (((a) << 8) & 0xFF0000))

/* ------------------------- Prototypes ------------------------------------- */
static void _hdcpIntToBigInt(LwU8 *pVal)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "_hdcpIntToBigInt");
static LwBool _hdcpIsRevokedDevicePresent(LwU8  *pKsvList, LwU32  numKsvs, LwU8 *pRevokedDevice, LwU8 *pFirstRevokedDevice)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "_hdcpIsRevokedDevicePresent");
static FLCN_STATUS _hdcpConsumeAndReadFromFb(LwU32 fbAddr, LwU32 *pFbOffset, LwU8 *pTempBuf, LwU32 *pDoneB, LwU8 *pDest, LwU32 destSize)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "_hdcpConsumeAndReadFromFb");
static LwBool _hdcpSrmValidateDSA(LwU8 *pSha1Hash, LwU8 *pSignature, LwBool bUseTestKeys)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "_hdcpSrmValidateDSA");
static FLCN_STATUS _hdcpHandleRevocate_1X(LwU8 *pKsvList, LwU32 numKsvs, LwU32 srmFbAddr, LwU8 *pTempBuf, PSHA1_CONTEXT pShaCtx, LwBool bUseTestKeys, LwU8 *pFirstRevokedDevice)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "_hdcpHandleRevocate_1X");
static LwBool _hdcpSrmCompareDCP(LwU8 *pHash, HDCP_CERTIFICATE *pCert, LwU32 dcpExponent)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "_hdcpSrmCompareDCP");
static FLCN_STATUS _hdcpHandleRevocate_2X(LwU8 *pKsvList, LwU32 numKsvs, LwU32 srmFbAddr, LwU8 *pTempBuf, SHA256_CTX *pShaCtx, LwBool bUseTestKeys, LwU8 *pFirstRevokedDevice, LwU32 dcpExponent, HDCP_CERTIFICATE *pCert)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "_hdcpHandleRevocate_2X");

/* ------------------------- Global Variables ------------------------------- */
/*
 * Big integer Montgomery constants.  The HDCP signature constants have been
 * pre-packaged to conform with the big integer Montgomery paper.
 * Big integers are associated with pre-callwlated constants to aid in
 * big integer arithmetic.
 */

/*!
 * @brief Prime divisor value.
 *
 * Prime divisor value as defined by the HDCP DisplayPort Spec, pg. 70.
 * Includes the Montgomery constant. Both values are stored in little
 * endian format.
 */
BigIntModulus HDCP_Divisor =
{
    5,
    0x4fc27529,
    { 0xf9c708e7, 0x97ef3f4d, 0xcd6d14e2, 0x5e6db56a, 0xee8af2ce },
    { 0x216222aa, 0xb7696a56, 0x32520fa6, 0x571e4275, 0xc6da1382 }
};

/*!
 * @brief Prime modulus value.
 *
 * Prime modulus value as defined by the HDCP DisplayPort Spec, pg. 70.
 * Includes the Montgomery constant. Both values are stored in little
 * endian format.
 */
BigIntModulus HDCP_Modulus =
{
    32,
    0x43ae5569,
    {
        0xf3287527, 0x8c59802b, 0x46edc211, 0x2a39951c,
        0x96891954, 0x028a49fd, 0x3275733b, 0x5c7b9c14,
        0xb9982049, 0xa73f3207, 0xb3721530, 0x10715509,
        0xd1974c3a, 0xf404a0bc, 0x5447cf35, 0xe52ba70e,
        0xd4c6b983, 0xb844c747, 0xae7c7667, 0x4f34dc0c,
        0x1d969e4b, 0xa0d28482, 0xf500e0dc, 0x8e7fa164,
        0x6a7058ff, 0xa1a24fc3, 0x5a52c7b8, 0x17395b35,
        0x9343786b, 0x018d75f7, 0xfd1761b7, 0xd3c3f5b2
    },
    {
        0x37b05821, 0x8464acc9, 0x70af59dc, 0x93804c0c,
        0xd8941b25, 0x2d034a86, 0xfcfb5c58, 0x7b31fe62,
        0x9a09de8b, 0x28c7998d, 0x2067722f, 0xe1e57124,
        0x7e979475, 0x60ee4474, 0x4ef3779a, 0x194d131e,
        0x07b3186a, 0xa933120b, 0xc9179238, 0x25da1db3,
        0x99fb1b78, 0x90d6a2de, 0xd3ec24ef, 0x1991fca0,
        0x1871b5cc, 0xd8ac0cfa, 0x1350d85e, 0x71899867,
        0x360e32c8, 0x053c8444, 0x26e4e64b, 0x03674270
    }
};

/*!
 * @brief Generator value.
 *
 * Generator values as defined by the HDCP DisplayPort Spec, pg. 70.
 * The value is stored in little endian format.
 */
LwU8 HDCP_SRM_Generator[SRM_GENERATOR_SIZE] =
{
    0xd9, 0x0b, 0xba, 0xc2, 0x42, 0x24, 0x46, 0x69,
    0x5b, 0x40, 0x67, 0x2f, 0x5b, 0x18, 0x3f, 0xb9,
    0xe8, 0x6f, 0x21, 0x29, 0xac, 0x7d, 0xfa, 0x51,
    0xc2, 0x9d, 0x4a, 0xab, 0x8a, 0x9b, 0x8e, 0xc9,
    0x42, 0x42, 0xa5, 0x1d, 0xb2, 0x69, 0xab, 0xc8,
    0xe3, 0xa5, 0xc8, 0x81, 0xbe, 0xb6, 0xa0, 0xb1,
    0x7f, 0xba, 0x21, 0x2c, 0x64, 0x35, 0xc8, 0xf7,
    0x5f, 0x58, 0x78, 0xf7, 0x45, 0x29, 0xdd, 0x92,
    0x9e, 0x79, 0x3d, 0xa0, 0x0c, 0xcd, 0x29, 0x0e,
    0xa9, 0xe1, 0x37, 0xeb, 0xbf, 0xc6, 0xed, 0x8e,
    0xa8, 0xff, 0x3e, 0xa8, 0x7d, 0x97, 0x62, 0x51,
    0xd2, 0xa9, 0xec, 0xbd, 0x4a, 0xb1, 0x5d, 0x8f,
    0x11, 0x86, 0x27, 0xcd, 0x66, 0xd7, 0x56, 0x5d,
    0x31, 0xd7, 0xbe, 0xa9, 0xac, 0xde, 0xaf, 0x02,
    0xb5, 0x1a, 0xde, 0x45, 0x24, 0x3e, 0xe4, 0x1a,
    0x13, 0x52, 0x4d, 0x6a, 0x1b, 0x5d, 0xf8, 0x92
};

/*!
 * @brief DSA public key.
 *
 * Public Key as defined by the HDCP DisplayPort spec, pg. 70.
 * The value is stored in little endian format.
 */
LwU8 HDCP_SRM_PublicKey[SRM_MODULUS_SIZE] =
{
    // Production public key
    0x99, 0x37, 0xe5, 0x36, 0xfa, 0xf7, 0xa9, 0x62,
    0x83, 0xfb, 0xb3, 0xe9, 0xf7, 0x9d, 0x8f, 0xd8,
    0xcb, 0x62, 0xf6, 0x66, 0x8d, 0xdc, 0xc8, 0x95,
    0x10, 0x24, 0x6c, 0x88, 0xbd, 0xff, 0xb7, 0x7b,
    0xe2, 0x06, 0x52, 0xfd, 0xf7, 0x5f, 0x43, 0x62,
    0xe6, 0x53, 0x65, 0xb1, 0x38, 0x90, 0x25, 0x87,
    0x8d, 0xa4, 0x9e, 0xfe, 0x56, 0x08, 0xa7, 0xa2,
    0x0d, 0x4e, 0xd8, 0x43, 0x3c, 0x97, 0xba, 0x27,
    0x6c, 0x56, 0xc4, 0x17, 0xa4, 0xb2, 0x5c, 0x8d,
    0xdb, 0x04, 0x17, 0x03, 0x4f, 0xe1, 0x22, 0xdb,
    0x74, 0x18, 0x54, 0x1b, 0xde, 0x04, 0x68, 0xe1,
    0xbd, 0x0b, 0x4f, 0x65, 0x48, 0x0e, 0x95, 0x56,
    0x8d, 0xa7, 0x5b, 0xf1, 0x55, 0x47, 0x65, 0xe7,
    0xa8, 0x54, 0x17, 0x8a, 0x65, 0x76, 0x0d, 0x4f,
    0x0d, 0xff, 0xac, 0xa3, 0xe0, 0xfb, 0x80, 0x3a,
    0x86, 0xb0, 0xa0, 0x6b, 0x52, 0x00, 0x06, 0xc7
};

#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
/*!
 * @brief DSA test public key for use with test vectors.
 */
LwU8 HDCP_SRM_TestPublicKey[SRM_MODULUS_SIZE] =
{
    // Test public key
    0x46, 0xb9, 0xc2, 0xe5, 0xbe, 0x57, 0x3b, 0xa6,
    0x22, 0x7b, 0xaa, 0x83, 0x81, 0xa9, 0xd2, 0x0f,
    0x03, 0x2e, 0x0b, 0x70, 0xac, 0x96, 0x42, 0x85,
    0x4e, 0x78, 0x8a, 0xdf, 0x65, 0x35, 0x97, 0x6d,
    0xe1, 0x8d, 0xd1, 0x7e, 0xa3, 0x83, 0xca, 0x0f,
    0xb5, 0x8e, 0xa4, 0x11, 0xfa, 0x14, 0x6d, 0xb1,
    0x0a, 0xcc, 0x5d, 0xff, 0xc0, 0x8c, 0xd8, 0xb1,
    0xe6, 0x95, 0x72, 0x2e, 0xbd, 0x7c, 0x85, 0xde,
    0xe8, 0x52, 0x69, 0x92, 0xa0, 0x22, 0xf7, 0x01,
    0xcd, 0x79, 0xaf, 0x94, 0x83, 0x2e, 0x01, 0x1c,
    0xd7, 0xef, 0x86, 0x97, 0xa3, 0xbb, 0xcb, 0x64,
    0xa6, 0xc7, 0x08, 0x5e, 0x8e, 0x5f, 0x11, 0x0b,
    0xc0, 0xe8, 0xd8, 0xde, 0x47, 0x2e, 0x75, 0xc7,
    0xaa, 0x8c, 0xdc, 0xb7, 0x02, 0xc4, 0xdf, 0x95,
    0x31, 0x74, 0xb0, 0x3e, 0xeb, 0x95, 0xdb, 0xb0,
    0xce, 0x11, 0x0e, 0x34, 0x9f, 0xe1, 0x13, 0x8d
};
#endif

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * Colwerts a 20 byte big-endian integer into big integer format.
 *
 * @param[in/out] pVal  Pointer to a 20 byte big-endian integer.
 */
static void
_hdcpIntToBigInt(LwU8 *pVal)
{
    LwU32 *pVal32 = (LwU32*)pVal;
    LwU32  temp;
    LwU8   i;

    // Byte swap individual words
    for (i = 0; i < 5; i++)
    {
        pVal32[i] = REVERSE_BYTE_ORDER(pVal32[i]);
    }

    // Swap words
    temp = pVal32[0]; pVal32[0] = pVal32[4]; pVal[4] = temp;
    temp = pVal32[1]; pVal32[1] = pVal32[3]; pVal[4] = temp;
}

/*!
 * @brief Determines if the specific revoked device is in the KSV list.
 *
 * @param[in]  pKsvList            List of receiver IDs to be checked against.
 * @param[in]  numKsvs             Number of entries in the receiver list.
 * @param[in]  pRevokedDevice      Known revoked device from the SRM.
 * @param[out] pFirstRevoedDevice  First seen revoked device in the KSV list.
 *
 * @return LW_TRUE if the known revoked device is present in the KSV list;
 *         LW_FALSE if it is not.
 */
static LwBool
_hdcpIsRevokedDevicePresent
(
    LwU8  *pKsvList,
    LwU32  numKsvs,
    LwU8  *pRevokedDevice,
    LwU8  *pFirstRevokedDevice
)
{
    LwBool bPresent = LW_FALSE;
    LwU32  byteOffset;
    LwU8   ksvIdx;

    if (NULL == pKsvList)
    {
        return LW_FALSE;
    }

    for (ksvIdx = 0, byteOffset = 0;
         ksvIdx < numKsvs;
         ksvIdx++, byteOffset += HDCP_SIZE_RCVR_ID)
    {
        if (0 == memcmp(&(pKsvList[byteOffset]), pRevokedDevice,
                        HDCP_SIZE_RCVR_ID))
        {
            if (NULL != pFirstRevokedDevice)
            {
                memcpy(pFirstRevokedDevice, &(pKsvList[byteOffset]),
                       HDCP_SIZE_RCVR_ID);
            }
            bPresent = LW_TRUE;
            break;
        }
    }

    return bPresent;
}

/*!
 * @brief Used to read the SRM from its source to destination.
 *
 * @param[in]      fbAddr      Starting address of the SRM in FB.
 * @param[in/out]  pFbOffset   Current FB offset to be used for accessing data
 *                             in FB.
 * @param[in/out]  pTempBuf    Source that holds the data. Must be 256 byte
 *                             aligned.
 * @param[in/out]  pBytesDone  Offset into the source until which bytes are
 *                             consumed. In case 256-pDoneB is less than
 *                             required bytes, data is pulled from the FB and
 *                             pFbOffset is updated.
 * @param[in/out]  pDest       Destination
 * @param[in]      destSize    Number of bytes required.
 */
static FLCN_STATUS
_hdcpConsumeAndReadFromFb
(
    LwU32  fbAddr,
    LwU32 *pFbOffset,
    LwU8  *pTempBuf,
    LwU32 *pBytesDone,
    LwU8  *pDest,
    LwU32  destSize
)
{
    RM_FLCN_MEM_DESC memDesc;
    FLCN_STATUS      status    = FLCN_OK;
    LwU32            bytesLeft = HDCP_SIZE_MAX_DMA - (*pBytesDone);

    if (bytesLeft < destSize)
    {
        memcpy(pDest, &(pTempBuf[*pBytesDone]), bytesLeft);

        memset(&memDesc, 0 , sizeof(RM_FLCN_MEM_DESC));
        memDesc.address.lo = fbAddr << 8;
        memDesc.address.hi = fbAddr >> 24;
        memDesc.params =
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, bytesLeft) |
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_SEC2_DMAIDX_VIRT);
        status = dmaRead(pTempBuf, &memDesc, *pFbOffset, HDCP_SIZE_MAX_DMA);
        if (FLCN_OK != status)
        {
            goto hdcpConsumeAndReadFromFb_exit;
        }
        (*pFbOffset) += HDCP_SIZE_MAX_DMA;
        memcpy(&(pDest[bytesLeft]), pTempBuf, destSize - bytesLeft);
        (*pBytesDone) += destSize;
    }
    else
    {
        memcpy(pDest, &(pTempBuf[*pBytesDone]), destSize);
        (*pBytesDone) += destSize;
    }

hdcpConsumeAndReadFromFb_exit:
    return status;
}

/*!
 * @brief Validates the DSA signature of the older SRM version.
 *
 * The DSA signature is verified as follows:
 *
 * Variables:
 * p = prime modulus: Provided by the HDCP specification (Global: HDCP_Modulus)
 * q = prime divisor: Provided by the HDCP specification (Global: HDCP_Divisor)
 * g = generator: Provided by the HDCP specification (Global: HDCP_Generator)
 * y = public key: Provided by the sender (Global: HDCP_SRM_PublicKey)
 * r = SRM signature, "r" value: Found in the SRM (Provided: hdcpSrmSig_R)
 * s = SRM signature, "s" value: Found in the SRM (Provided: hdcpSrmSig_S)
 * h = SHA-1(message): Callwlated from SRM (Provided: pSha1Hash)
 *
 * Steps needed to verify the DSA signature
 *  w = pow(s, -1) % q
 * u1 = (sha1(m) * w) % q
 * u2 = (r * w) % q
 *  v = ((pow(g, u1) * pow(y, u2)) % p) %q
 *
 * If (v == r) the the signature is verified, and the verifier can have
 * high confidence that the received message was sent by the party holding
 * the public key.
 *
 * @param[in] pSha1Hash
 * @param[in] pSignature
 * @param[in] bUseTestKeys
 *
 * @return LW_TRUE if the signature is valid; LW_FALSE otherwise.
 */
static LwBool
_hdcpSrmValidateDSA
(
    LwU8   *pSha1Hash,
    LwU8   *pSignature,
    LwBool  bUseTestKeys
)
{
    LwU8 sigR[SRM_DSA_SIGNATURE_SIZE / 2] GCC_ATTRIB_ALIGN(4);
    LwU8 sigS[SRM_DSA_SIGNATURE_SIZE / 2] GCC_ATTRIB_ALIGN(4);
    LwU8 u1[SRM_MODULUS_SIZE] GCC_ATTRIB_ALIGN(4);
    LwU8 u2[SRM_MODULUS_SIZE] GCC_ATTRIB_ALIGN(4);
    LwU8  w[SRM_MODULUS_SIZE] GCC_ATTRIB_ALIGN(4);

    memset(u1, 0, SRM_MODULUS_SIZE);
    memset(u2, 0, SRM_MODULUS_SIZE);

    memcpy(sigR, pSignature, SRM_DSA_SIGNATURE_SIZE / 2);
    memcpy(sigS, pSignature + (SRM_DSA_SIGNATURE_SIZE / 2),
           SRM_DSA_SIGNATURE_SIZE / 2);

    _hdcpIntToBigInt(pSha1Hash);
    _hdcpIntToBigInt(sigR);
    _hdcpIntToBigInt(sigS);

    // w = pow(s, -1) % q
    bigIntIlwerseMod((LwU32*)w, (LwU32*)sigS, &HDCP_Divisor);

    // u1 = (sha1(m) * w) % q
    bigIntMultiplyMod((LwU32*)u1, (LwU32*)pSha1Hash, (LwU32*)w, &HDCP_Divisor);

    // u2 = (r * w) % q
    bigIntMultiplyMod((LwU32*)u2, (LwU32*)sigR, (LwU32*)w, &HDCP_Divisor);

    //
    // Since (pow(g, u1) * (pow(y, u2)) may be too large for the buffers we
    // use, we need to rely on a property of mod to keep the values in more
    // manageable chunks:
    //
    // (a * b) % c = ((a % c) * (b % c)) % c
    //

    //
    // Don't need the value stored in w anymore; reuse the variable
    // w = pow(g, u1) % p
    //
    bigIntPowerMod((LwU32*)w, (LwU32*)HDCP_SRM_Generator, (LwU32*)u1,
                   &HDCP_Modulus, SRM_MODULUS_SIZE / sizeof(LwU32));

    //
    // Don't need the value stored in u1 anymore; reuse the variable
    // u1 = pow(y, u2) % p
    //
    if (!bUseTestKeys)
    {
        bigIntPowerMod((LwU32*)u1, (LwU32*)HDCP_SRM_PublicKey, (LwU32*)u2,
                       &HDCP_Modulus,
                       SRM_MODULUS_SIZE / sizeof(LwU32));
    }
    else
    {
#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
        bigIntPowerMod((LwU32*)u1, (LwU32*)HDCP_SRM_TestPublicKey,
                       (LwU32*)u2, &HDCP_Modulus,
                       SRM_MODULUS_SIZE / sizeof(LwU32));
#else
        return LW_FALSE;
#endif
    }

    //
    // Don't need the value stored in u2 anymore; reuse the variable
    // u2 = (w * u1) % p
    //
    bigIntMultiplyMod((LwU32*)u2, (LwU32*)w, (LwU32*)u1, &HDCP_Modulus);

    // At this point, we have v = u2 % q; reuse w as v
    bigIntMod((LwU32*)w, (LwU32*)u2, &HDCP_Divisor,
              SRM_MODULUS_SIZE / sizeof(LwU32));

    // If (w == r), the signature is verified
    if (0 == bigIntCompare((LwU32*)w, (LwU32*)sigR, SRM_DIVISOR_SIZE / sizeof(LwU32)))
    {
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}

/*!
 * @brief Handles revocation of KSVs using the older SRM. Also performs SRM
 *        validation.
 *
 * @param[in]  pKsvList             List of KSVs to be verified. If NULL,
 *                                  only SRM validation is done.
 * @param[in]  numKsvs              Number of KSVs in the list.
 * @param[in]  srmFbAddr            Starting FB address of the SRM, aligned
 *                                  to a 256-byte boundary.
 * @param[in]  pTempBuf             Pointer to a 256-byte aligned buffer.
 * @param[in]  pShaCtx              SHA context that will be used to generate
 *                                  the SHA1 hashes for various generations.
 * @param[in]  bUseTestKeys         Specifies whether or not to use test keys
 *                                  for SRM validation.
 * @param[out] pFirstRevokedDevice  First revoked device (5 byte buffer).
 *
 * @return
 */
static FLCN_STATUS
_hdcpHandleRevocate_1X
(
    LwU8         *pKsvList,
    LwU32         numKsvs,
    LwU32         srmFbAddr,
    LwU8         *pTempBuf,
    PSHA1_CONTEXT pShaCtx,
    LwBool        bUseTestKeys,
    LwU8         *pFirstRevokedDevice
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwU32       fbOffset   = HDCP_SIZE_MAX_DMA;
    LwU32       bytesDone  = 4;
    LwU32       i          = 0;
    LwU32       generation = 0;
    LwU32       length     = 0;
    LwU32       temp       = 0;
    LwBool      bIsRevoked = LW_FALSE;
    LwBool      bSrmValid  = LW_FALSE;

    // Read VRL length and generations
    temp = ((LwU32*)pTempBuf)[1];
    sha1Update(pShaCtx, &temp, 4);
    hdcpSwapEndianness(&temp, &temp, sizeof(LwU32));
    generation = DRF_VAL(_HDCP, _SRM_1X_BITS, _GENERATION, temp);
    length     = DRF_VAL(_HDCP, _SRM_1X_BITS, _LENGTH, temp);
    bytesDone += 4;

    if (length < HDCP_SRM_1X_FIRST_GEN_MIN_LEN)
    {
        status = FLCN_ERR_HDCP_ILWALID_SRM;
        goto _hdcpHandleRevocate_1X_end;
    }

    // Subtract the signature and size of the length field
    length -= SRM_DSA_SIGNATURE_SIZE;
    length -= 3;

    do
    {
        LwU32 signature[HDCP_SIZE_LwU32(SRM_DSA_SIGNATURE_SIZE)];
        LwU32 sha1Hash[HDCP_SIZE_LwU32(SHA1_DIGEST_LENGTH)];
        SHA1_CONTEXT genSha1Ctx;

        RM_FLCN_MEM_DESC memDesc;
        memset(&memDesc, 0, sizeof(RM_FLCN_MEM_DESC));
        memDesc.address.lo = srmFbAddr << 8;
        memDesc.address.hi = srmFbAddr >> 24;
        memDesc.params =
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, 0) |
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                    RM_SEC2_DMAIDX_VIRT);

        memcpy(&genSha1Ctx, pShaCtx, sizeof(SHA1_CONTEXT));

        while (length > 0)
        {
            LwU8 numDevices = pTempBuf[bytesDone++];
            sha1Update(&genSha1Ctx, &numDevices, 1);

            if (length < ((numDevices * HDCP_SIZE_RCVR_ID) + 1))
            {
                status = FLCN_ERR_HDCP_ILWALID_SRM;
                goto _hdcpHandleRevocate_1X_end;
            }

            length -= ((numDevices * HDCP_SIZE_RCVR_ID) + 1);
            while (numDevices > 0)
            {
                LwU8 rcvrId[HDCP_SIZE_RCVR_ID];
                status = _hdcpConsumeAndReadFromFb(srmFbAddr, &fbOffset,
                                                   pTempBuf, &bytesDone, rcvrId,
                                                   HDCP_SIZE_RCVR_ID);
                if (FLCN_OK != status)
                {
                    goto _hdcpHandleRevocate_1X_end;
                }
                sha1Update(&genSha1Ctx, rcvrId, HDCP_SIZE_RCVR_ID);

                if (HDCP_SIZE_MAX_DMA == bytesDone)
                {
                    status = dmaRead(pTempBuf, &memDesc, fbOffset, HDCP_SIZE_MAX_DMA);
                    if (FLCN_OK != status)
                    {
                        goto _hdcpHandleRevocate_1X_end;
                    }
                    fbOffset += HDCP_SIZE_MAX_DMA;
                    bytesDone = 0;
                }

                if ((0 != numKsvs) && !bIsRevoked)
                {
                    if (_hdcpIsRevokedDevicePresent(pKsvList, numKsvs, rcvrId,
                                                    pFirstRevokedDevice))
                    {
                        bIsRevoked = LW_TRUE;
                    }
                }

                numDevices--;
            }
        }

        // Read the signature and verify it.
        memcpy(pShaCtx, &genSha1Ctx, sizeof(SHA1_CONTEXT));
        sha1Final((LwU8*)sha1Hash, &genSha1Ctx);

        status = _hdcpConsumeAndReadFromFb(srmFbAddr, &fbOffset, pTempBuf,
                                           &bytesDone, (LwU8*)signature,
                                           SRM_DSA_SIGNATURE_SIZE);
        if (FLCN_OK != status)
        {
            goto _hdcpHandleRevocate_1X_end;
        }
        if (!_hdcpSrmValidateDSA((LwU8*)sha1Hash, (LwU8*)signature, bUseTestKeys))
        {
            goto _hdcpHandleRevocate_1X_end;
        }
        bSrmValid = LW_TRUE;

        sha1Update(pShaCtx, signature, SRM_DSA_SIGNATURE_SIZE);
        i++;

        // Read the length for the next gen
        if (i < generation)
        {
            length = 0;
            status = _hdcpConsumeAndReadFromFb(srmFbAddr, &fbOffset, pTempBuf,
                                               &bytesDone, (LwU8*)&length, 2);
            if (FLCN_OK != status)
            {
                goto _hdcpHandleRevocate_1X_end;
            }

            sha1Update(pShaCtx, &length, 2);
            hdcpSwapEndianness(&length, &length, 2);

            if (length < HDCP_SRM_1X_NEXT_GEN_MIN_LEN)
            {
                status = FLCN_ERR_HDCP_ILWALID_SRM;
                goto _hdcpHandleRevocate_1X_end;
            }

            length -= 2;
            length -= SRM_DSA_SIGNATURE_SIZE;

            if (bytesDone == HDCP_SIZE_MAX_DMA)
            {
                status = dmaRead(pTempBuf, &memDesc, fbOffset, HDCP_SIZE_MAX_DMA);
                if (FLCN_OK != status)
                {
                    goto _hdcpHandleRevocate_1X_end;
                }
                fbOffset += HDCP_SIZE_MAX_DMA;
                bytesDone = 0;
            }
        }
    } while (i < generation);

_hdcpHandleRevocate_1X_end:
    //
    // If the status != FLCN_OK, it means that there was an error reading
    // the SRM from FB. Return that error; otherwise, process invalid SRM/
    // revoked devices.
    //
    if (FLCN_OK == status)
    {
        if (!bSrmValid)
        {
            status = FLCN_ERR_HDCP_ILWALID_SRM;
        }
        else if (bIsRevoked)
        {
            status = FLCN_ERR_HDCP_RECV_REVOKED;
        }
    }
    return status;
}

/*!
 * @brief Validates the SRM DCP signature. Assumes DCP public key is in
 *        hdcpKpubDcp.
 *
 * @param[in]  pHash        SHA-256 hash of the SRM.
 * @param[in]  pCert        Certification with signature.
 * @param[in]  dcpExponent  Exponent for the DCP key.
 *
 * @return LW_TRUE is the SRM DCP signature is valid; LW_FALSE otherwise.
 */
LwBool
_hdcpSrmCompareDCP
(
    LwU8             *pHash,
    HDCP_CERTIFICATE *pCert,
    LwU32             dcpExponent
)
{
    return hdcpVerifyCertificate(hdcpKpubDcp, dcpExponent, pCert, pHash);
}

/*!
 * @brief Handles revocation of KSVs using the newer SRM. Also verifies the
 *        SRM using the DCP certificate. Assumes the DCP public key is in
 *        hdcpKpubDcp.
 *
 * @param[in]   pKsvList
 * @param[in]   numKsvs
 * @param[in]   srmFbAddr
 * @param[in]   pTempBuf
 * @param[in]   pShaCtx
 * @param[in]   bUseTestKeys
 * @param[in]   pFirstRevokedDevice
 * @param[in]   dcpExponent
 * @param[in]   pCert
 */
static FLCN_STATUS
_hdcpHandleRevocate_2X
(
    LwU8             *pKsvList,
    LwU32             numKsvs,
    LwU32             srmFbAddr,
    LwU8             *pTempBuf,
    SHA256_CTX       *pShaCtx,
    LwBool            bUseTestKeys,
    LwU8             *pFirstRevokedDevice,
    LwU32             dcpExponent,
    HDCP_CERTIFICATE *pCert
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwU32       fbOffset    = HDCP_SIZE_MAX_DMA;
    LwU32       bytesDone   = 4;
    LwU32       i           = 0;
    LwU32       generation  = 0;
    LwU32       length      = 0;
    LwU32       temp        = 0;
    LwBool      bIsRevoked  = LW_FALSE;
    LwU32       numDevices  = 0;

    // Read VRL length and generations
    temp = ((LwU32*)pTempBuf)[1];
    sha256Update((LwU8*)&temp, 4, pShaCtx);
    hdcpSwapEndianness(&temp, &temp, sizeof(LwU32));
    generation = DRF_VAL(_HDCP, _SRM_1X_BITS, _GENERATION, temp);
    length     = DRF_VAL(_HDCP, _SRM_1X_BITS, _LENGTH, temp);
    bytesDone += 4;

    // Check for minimum length needed
    if (length < HDCP_SRM_2X_FIRST_GEN_MIN_LEN)
    {
        status = FLCN_ERR_HDCP_ILWALID_SRM;
        goto _hdcpHandleRevocate_2X_end;
    }

    // Subtract the signature and size of length field
    length -= SRM_DCP_SIGNATURE_SIZE;
    length -= 3;

    do
    {
        LwU32 sha2Hash[HDCP_SIZE_LwU32(SHA256_HASH_BLOCK_SIZE)];
        SHA256_CTX genSha2Ctx;

        RM_FLCN_MEM_DESC memDesc;
        memset(&memDesc, 0, sizeof(RM_FLCN_MEM_DESC));
        memDesc.address.lo = srmFbAddr >> 8;
        memDesc.address.hi = srmFbAddr << 24;
        memDesc.params =
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, 0) |
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                    RM_SEC2_DMAIDX_VIRT);

        memcpy(&genSha2Ctx, pShaCtx, sizeof(SHA256_CTX));

        if (0 == i)
        {
            status = _hdcpConsumeAndReadFromFb(srmFbAddr, &fbOffset,
                                               pTempBuf, &bytesDone,
                                               (LwU8*)&numDevices, 4);
            if (FLCN_OK != status)
            {
                goto _hdcpHandleRevocate_2X_end;
            }
            sha256Update((LwU8*)&numDevices, 4, &genSha2Ctx);
            hdcpSwapEndianness(&numDevices, &numDevices, sizeof(LwU32));
            numDevices = numDevices >> 22;
            length    -= 4;
        }

        length -= (numDevices * HDCP_SIZE_RCVR_ID);

        if (0 != length)
        {
            status = FLCN_ERR_ILLEGAL_OPERATION;
            goto _hdcpHandleRevocate_2X_end;
        }

        while (numDevices > 0)
        {
            LwU8 rcvrId[HDCP_SIZE_RCVR_ID];
            status = _hdcpConsumeAndReadFromFb(srmFbAddr, &fbOffset,
                                               pTempBuf, &bytesDone,
                                               rcvrId, HDCP_SIZE_RCVR_ID);
            if (FLCN_OK != status)
            {
                goto _hdcpHandleRevocate_2X_end;
            }

            sha256Update(rcvrId, HDCP_SIZE_RCVR_ID, &genSha2Ctx);

            if (HDCP_SIZE_MAX_DMA == bytesDone)
            {
                status = dmaRead(pTempBuf, &memDesc, fbOffset, HDCP_SIZE_MAX_DMA);
                if (FLCN_OK != status)
                {
                    goto _hdcpHandleRevocate_2X_end;
                }
                fbOffset += HDCP_SIZE_MAX_DMA;
                bytesDone = 0;
            }
            if ((numKsvs > 0) && !bIsRevoked)
            {
                if (_hdcpIsRevokedDevicePresent(pKsvList, numKsvs, rcvrId,
                                                pFirstRevokedDevice))
                {
                    bIsRevoked = LW_TRUE;
                }
            }

            numDevices--;
        }

        // Read signature and verify it.
        memcpy(pShaCtx, &genSha2Ctx, sizeof(SHA256_CTX));
        sha256Final(&genSha2Ctx, (LwU8*)sha2Hash);

        status = _hdcpConsumeAndReadFromFb(srmFbAddr, &fbOffset, pTempBuf,
                                           &bytesDone,
                                           (LwU8*)pCert->signature,
                                           SRM_DCP_SIGNATURE_SIZE);
        if (FLCN_OK != status)
        {
            goto _hdcpHandleRevocate_2X_end;
        }
        if (!_hdcpSrmCompareDCP((LwU8*)sha2Hash, pCert, dcpExponent))
        {
            status = FLCN_ERR_ILLEGAL_OPERATION;
            goto _hdcpHandleRevocate_2X_end;
        }

        sha256Update((LwU8*)pCert->signature, SRM_DCP_SIGNATURE_SIZE, pShaCtx);
        i++;

        // Read length for nex gen.
        if (i < generation)
        {
            LwU32 genHead = 0;
            length = 0;
            status = _hdcpConsumeAndReadFromFb(srmFbAddr, &fbOffset, pTempBuf,
                                               &bytesDone, (LwU8*)&genHead, 4);
            if (FLCN_OK != status)
            {
                goto _hdcpHandleRevocate_2X_end;
            }
            sha256Update((LwU8*)&genHead, sizeof(LwU32), pShaCtx);
            hdcpSwapEndianness(&genHead, &genHead, sizeof(LwU32));

            length     = DRF_VAL(_HDCP, _SRM_2X_NEXT_GEN, _LENGTH, genHead);
            numDevices = DRF_VAL(_HDCP, _SRM_2X_NEXT_GEN, _NUM_DEVICES, genHead);

            if (length < HDCP_SRM_2X_NEXT_GEN_MIN_LEN)
            {
                status = FLCN_ERR_ILLEGAL_OPERATION;
                goto _hdcpHandleRevocate_2X_end;
            }

            length -= sizeof(LwU32);
            length -= SRM_DCP_SIGNATURE_SIZE;

            if (HDCP_SIZE_MAX_DMA == bytesDone)
            {
                status = dmaRead(pTempBuf, &memDesc, fbOffset, HDCP_SIZE_MAX_DMA);
                if (FLCN_OK != status)
                {
                    goto _hdcpHandleRevocate_2X_end;
                }
                fbOffset += HDCP_SIZE_MAX_DMA;
                bytesDone = 0;
            }
        }
    } while (i < generation);

_hdcpHandleRevocate_2X_end:
    if (bIsRevoked)
    {
        return FLCN_ERR_HDCP_RECV_REVOKED;
    }
    return status;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Does SRM validation and receiver ID revocation check for both HDCP
 *        1.x and 2.x devices. Also supports older and newer versions of the
 *        SRM.
 *
 * @param[in]   pKsvList             List of KSVs to be verified. If only SRM
 *                                   validation is required, this parameter
 *                                   shall be NULL.
 * @param[in]   numKsvs              Number of entries in the KSV list.
 * @param[in]   srmFbAddr            Starting address in the FB of the SRM.
 *                                   Must be 256-byte aligned.
 * @param[in]   pTempBuf             A 256-byte aligned temporary buffer.
 * @param[in]   bUseTestKeys         Specifies to use use the test keys for
 *                                   SRM validation.
 * @param[out]  pFirstRevokedDevice  First revoked device, if any.
 */
FLCN_STATUS
hdcpRevocationCheck
(
    LwU8  *pKsvList,
    LwU32  numKsvs,
    LwU32  srmFbAddr,
    LwU8  *pTempBuf,
    LwBool bUseTestKeys,
    LwU8  *pFirstRevokedDevice
)
{
    FLCN_STATUS      status = FLCN_OK;
    HDCP_CERTIFICATE tempCert;
    LwU32            temp;
    LwU8             srmHdcp2Indicator;
    LwU8             srmId;
    LwU32            srmVersion;
    union
    {
        SHA1_CONTEXT srmSha1Ctx;
        SHA256_CTX   srmSha2Ctx;
    } shaCtx;
    RM_FLCN_MEM_DESC memDesc;

    // Validity check of the parameters
    if ((NULL == pTempBuf) ||
        ((LwU32)pTempBuf % HDCP_SIZE_MAX_DMA != 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    if ((0 == numKsvs) && (NULL == pKsvList))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Read the SRM header to determine which version and generation it is.
    // Read both the generation length and number of devices to save DMA
    // cycles.
    //
    memset(&memDesc, 0, sizeof(RM_FLCN_MEM_DESC));
    memDesc.address.lo = srmFbAddr << 8;
    memDesc.address.hi = srmFbAddr >> 24;
    memDesc.params =
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, 0) |
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_SEC2_DMAIDX_VIRT);
    status = dmaRead(pTempBuf, &memDesc, 0, HDCP_SIZE_MAX_DMA);
    if (FLCN_OK != status)
    {
        goto hdcpRevocationCheck_end;
    }

    // Process the first word.
    temp = *((LwU32*)pTempBuf);

    hdcpSwapEndianness(&temp, &temp, sizeof(LwU32));
    srmId             = DRF_VAL(_HDCP, _SRM_HEADER_BITS, _ID, temp);
    srmHdcp2Indicator = DRF_VAL(_HDCP, _SRM_HEADER_BITS, _HDCP2_IND, temp);
    srmVersion        = DRF_VAL(_HDCP, _SRM_HEADER_BITS, _VERSION, temp);

    if (LW_HDCP_SRM_HEADER_BITS_HDCP2_IND_TRUE == srmHdcp2Indicator)
    {
        LwU32 dcpExponent = 0;

        //
        // Load DCP key into hdcpKpubDcp. Overlays already loaded, so just need
        // to switch into HS mode to get the DCP key.
        //
        OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpDcpKey), NULL, 0);
        status = hdcpGetDcpKey();
        OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

        memcpy(&dcpExponent, hdcpKpubDcp + HDCP_SIZE_DCP_KPUB,
               sizeof(LwU32));
        if (0 == dcpExponent)
        {
            status = FLCN_ERROR;
            goto hdcpRevocationCheck_end;
        }

        // Initialize the SHA-256 context
        sha256Init(&shaCtx.srmSha2Ctx);
        sha256Update(pTempBuf, 4, &shaCtx.srmSha2Ctx);
        status = _hdcpHandleRevocate_2X(pKsvList, numKsvs, srmFbAddr, pTempBuf,
                                       &shaCtx.srmSha2Ctx, bUseTestKeys,
                                       pFirstRevokedDevice, dcpExponent,
                                       &tempCert);
    }
    else
    {
        // Initialize the SHA-1 context
        sha1Initialize(&shaCtx.srmSha1Ctx);
        sha1Update(&shaCtx.srmSha1Ctx, pTempBuf, 4);
        status = _hdcpHandleRevocate_1X(pKsvList, numKsvs, srmFbAddr, pTempBuf,
                                       &shaCtx.srmSha1Ctx, bUseTestKeys,
                                       pFirstRevokedDevice);
    }

hdcpRevocationCheck_end:
    return status;
}
#endif
