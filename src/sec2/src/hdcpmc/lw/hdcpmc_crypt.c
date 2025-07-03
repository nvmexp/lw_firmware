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
 * @file    hdcpmc_crypto.c
 * @brief   Interface to obtain keys and encrypt data.
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"

/* ------------------------- Application Includes --------------------------- */
#include "mem_hs.h"
#include "sec2_hs.h"
#include "hdcpmc/hdcpmc_crypt.h"
#include "hdcpmc/hdcpmc_data.h"
#include "hdcpmc/hdcpmc_mem.h"
#include "hdcpmc/hdcpmc_scp.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Transfer size of blocks when transferring data from IMEM to DMEM.
 */
#define HDCP_IMEM_TRANSFER_SIZE                                             256

/*!
 * Required shift to colwert offsets into blocks and vise-versa.
 */
#define HDCP_IMEM_TRANSFER_SHIFT                                              8

/* ------------------------- Prototypes ------------------------------------- */
void _hdcpDecryptLc128(void)
    GCC_ATTRIB_SECTION("imem_hdcpEncrypt", "_hdcpDecryptLc128");
void _hdcpSwapEndianness_hs(void *pOutData, const void *pInData, const LwU32 size)
    GCC_ATTRIB_SECTION("imem_hdcpEncrypt", "_hdcpSwapEndianness_hs");
void _hdcpGetLc128(LwU8 *pLC)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "_hdcpGetLc128");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief The value for Kpub_dcp.
 *
 * This public key is stored in IMEM to validate the authenticity of the value.
 * By storing the value in an HS IMEM overlay, we can make use of the overlay's
 * signature to ensure that the value has not been tampered with.
 *
 * Two versions of this key are stored: production and test. Only one version
 * of the key is stored based on the build.
 */
#if (!SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
LwU8 hdcpDcpCert_imem[ALIGN_UP((HDCP_SIZE_DCP_KPUB + 4), 16)]
    GCC_ATTRIB_ALIGN(256)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyCertRx", "hdcpDcpCert_imem") =
{
    0xb0, 0xe9, 0xaa, 0x45, 0xf1, 0x29, 0xba, 0x0a, 0x1c, 0xbe, 0x17, 0x57, 0x28, 0xeb, 0x2b, 0x4e,
    0x8f, 0xd0, 0xc0, 0x6a, 0xad, 0x79, 0x98, 0x0f, 0x8d, 0x43, 0x8d, 0x47, 0x04, 0xb8, 0x2b, 0xf4,
    0x15, 0x21, 0x56, 0x19, 0x01, 0x40, 0x01, 0x3b, 0xd0, 0x91, 0x90, 0x62, 0x9e, 0x89, 0xc2, 0x27,
    0x8e, 0xcf, 0xb6, 0xdb, 0xce, 0x3f, 0x72, 0x10, 0x50, 0x93, 0x8c, 0x23, 0x29, 0x83, 0x7b, 0x80,
    0x64, 0xa7, 0x59, 0xe8, 0x61, 0x67, 0x4c, 0xbc, 0xd8, 0x58, 0xb8, 0xf1, 0xd4, 0xf8, 0x2c, 0x37,
    0x98, 0x16, 0x26, 0x0e, 0x4e, 0xf9, 0x4e, 0xee, 0x24, 0xde, 0xcc, 0xd1, 0x4b, 0x4b, 0xc5, 0x06,
    0x7a, 0xfb, 0x49, 0x65, 0xe6, 0xc0, 0x00, 0x83, 0x48, 0x1e, 0x8e, 0x42, 0x2a, 0x53, 0xa0, 0xf5,
    0x37, 0x29, 0x2b, 0x5a, 0xf9, 0x73, 0xc5, 0x9a, 0xa1, 0xb5, 0xb5, 0x74, 0x7c, 0x06, 0xdc, 0x7b,
    0x7c, 0xdc, 0x6c, 0x6e, 0x82, 0x6b, 0x49, 0x88, 0xd4, 0x1b, 0x25, 0xe0, 0xee, 0xd1, 0x79, 0xbd,
    0x39, 0x85, 0xfa, 0x4f, 0x25, 0xec, 0x70, 0x19, 0x23, 0xc1, 0xb9, 0xa6, 0xd9, 0x7e, 0x3e, 0xda,
    0x48, 0xa9, 0x58, 0xe3, 0x18, 0x14, 0x1e, 0x9f, 0x30, 0x7f, 0x4c, 0xa8, 0xae, 0x53, 0x22, 0x66,
    0x2b, 0xbe, 0x24, 0xcb, 0x47, 0x66, 0xfc, 0x83, 0xcf, 0x5c, 0x2d, 0x1e, 0x3a, 0xab, 0xab, 0x06,
    0xbe, 0x05, 0xaa, 0x1a, 0x9b, 0x2d, 0xb7, 0xa6, 0x54, 0xf3, 0x63, 0x2b, 0x97, 0xbf, 0x93, 0xbe,
    0xc1, 0xaf, 0x21, 0x39, 0x49, 0x0c, 0xe9, 0x31, 0x90, 0xcc, 0xc2, 0xbb, 0x3c, 0x02, 0xc4, 0xe2,
    0xbd, 0xbd, 0x2f, 0x84, 0x63, 0x9b, 0xd2, 0xdd, 0x78, 0x3e, 0x90, 0xc6, 0xc5, 0xac, 0x16, 0x77,
    0x2e, 0x69, 0x6c, 0x77, 0xfd, 0xed, 0x8a, 0x4d, 0x6a, 0x8c, 0xa3, 0xa9, 0x25, 0x6c, 0x21, 0xfd,
    0xb2, 0x94, 0x0c, 0x84, 0xaa, 0x07, 0x29, 0x26, 0x46, 0xf7, 0x9b, 0x3a, 0x19, 0x87, 0xe0, 0x9f,
    0xeb, 0x30, 0xa8, 0xf5, 0x64, 0xeb, 0x07, 0xf1, 0xe9, 0xdb, 0xf9, 0xaf, 0x2c, 0x8b, 0x69, 0x7e,
    0x2e, 0x67, 0x39, 0x3f, 0xf3, 0xa6, 0xe5, 0xcd, 0xda, 0x24, 0x9b, 0xa2, 0x78, 0x72, 0xf0, 0xa2,
    0x27, 0xc3, 0xe0, 0x25, 0xb4, 0xa1, 0x04, 0x6a, 0x59, 0x80, 0x27, 0xb5, 0xda, 0xb4, 0xb4, 0x53,
    0x97, 0x3b, 0x28, 0x99, 0xac, 0xf4, 0x96, 0x27, 0x0f, 0x7f, 0x30, 0x0c, 0x4a, 0xaf, 0xcb, 0x9e,
    0xd8, 0x71, 0x28, 0x24, 0x3e, 0xbc, 0x35, 0x15, 0xbe, 0x13, 0xeb, 0xaf, 0x43, 0x01, 0xbd, 0x61,
    0x24, 0x54, 0x34, 0x9f, 0x73, 0x3e, 0xb5, 0x10, 0x9f, 0xc9, 0xfc, 0x80, 0xe8, 0x4d, 0xe3, 0x32,
    0x96, 0x8f, 0x88, 0x10, 0x23, 0x25, 0xf3, 0xd3, 0x3e, 0x6e, 0x6d, 0xbb, 0xdc, 0x29, 0x66, 0xeb,
    0x03, 0x00, 0x00, 0x00
};
#else
LwU8 hdcpDcpCert_imem[ALIGN_UP((HDCP_SIZE_DCP_KPUB + 4), 16)]
    GCC_ATTRIB_ALIGN(256)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyCertRx", "hdcpDcpCert_imem") =
{
    0xa2, 0xc7, 0x55, 0x57, 0x54, 0xcb, 0xaa, 0xa7, 0x7a, 0x27, 0x92, 0xc3, 0x1a, 0x6d, 0xc2, 0x31,
    0xcf, 0x12, 0xc2, 0x24, 0xbf, 0x89, 0x72, 0x46, 0xa4, 0x8d, 0x20, 0x83, 0xb2, 0xdd, 0x04, 0xda,
    0x7e, 0x01, 0xa9, 0x19, 0xef, 0x7e, 0x8c, 0x47, 0x54, 0xc8, 0x59, 0x72, 0x5c, 0x89, 0x60, 0x62,
    0x9f, 0x39, 0xd0, 0xe4, 0x80, 0xca, 0xa8, 0xd4, 0x1e, 0x91, 0xe3, 0x0e, 0x2c, 0x77, 0x55, 0x6d,
    0x58, 0xa8, 0x9e, 0x3e, 0xf2, 0xda, 0x78, 0x3e, 0xba, 0xd1, 0x05, 0x37, 0x07, 0xf2, 0x88, 0x74,
    0x0c, 0xbc, 0xfb, 0x68, 0xa4, 0x7a, 0x27, 0xad, 0x63, 0xa5, 0x1f, 0x67, 0xf1, 0x45, 0x85, 0x16,
    0x49, 0x8a, 0xe6, 0x34, 0x1c, 0x6e, 0x80, 0xf5, 0xff, 0x13, 0x72, 0x85, 0x5d, 0xc1, 0xde, 0x5f,
    0x01, 0x86, 0x55, 0x86, 0x71, 0xe8, 0x10, 0x33, 0x14, 0x70, 0x2a, 0x5f, 0x15, 0x7b, 0x5c, 0x65,
    0x3c, 0x46, 0x3a, 0x17, 0x79, 0xed, 0x54, 0x6a, 0xa6, 0xc9, 0xdf, 0xeb, 0x2a, 0x81, 0x2a, 0x80,
    0x2a, 0x46, 0xa2, 0x06, 0xdb, 0xfd, 0xd5, 0xf3, 0xcf, 0x74, 0xbb, 0x66, 0x56, 0x48, 0xd7, 0x7c,
    0x6a, 0x03, 0x14, 0x1e, 0x55, 0x56, 0xe4, 0xb6, 0xfa, 0x38, 0x2b, 0x5d, 0xfb, 0x87, 0x9f, 0x9e,
    0x78, 0x21, 0x87, 0xc0, 0x0c, 0x63, 0x3e, 0x8d, 0x0f, 0xe2, 0xa7, 0x19, 0x10, 0x9b, 0x15, 0xe1,
    0x11, 0x87, 0x49, 0x33, 0x49, 0xb8, 0x66, 0x32, 0x28, 0x7c, 0x87, 0xf5, 0xd2, 0x2e, 0xc5, 0xf3,
    0x66, 0x2f, 0x79, 0xef, 0x40, 0x5a, 0xd4, 0x14, 0x85, 0x74, 0x5f, 0x06, 0x43, 0x50, 0xcd, 0xde,
    0x84, 0xe7, 0x3c, 0x7d, 0x8e, 0x8a, 0x49, 0xcc, 0x5a, 0xcf, 0x73, 0xa1, 0x8a, 0x13, 0xff, 0x37,
    0x13, 0x3d, 0xad, 0x57, 0xd8, 0x51, 0x22, 0xd6, 0x32, 0x1f, 0xc0, 0x68, 0x4c, 0xa0, 0x5b, 0xdd,
    0x5f, 0x78, 0xc8, 0x9f, 0x2d, 0x3a, 0xa2, 0xb8, 0x1e, 0x4a, 0xe4, 0x08, 0x55, 0x64, 0x05, 0xe6,
    0x94, 0xfb, 0xeb, 0x03, 0x6a, 0x0a, 0xbe, 0x83, 0x18, 0x94, 0xd4, 0xb6, 0xc3, 0xf2, 0x58, 0x9c,
    0x7a, 0x24, 0xdd, 0xd1, 0x3a, 0xb7, 0x3a, 0xb0, 0xbb, 0xe5, 0xd1, 0x28, 0xab, 0xad, 0x24, 0x54,
    0x72, 0x0e, 0x76, 0xd2, 0x89, 0x32, 0xea, 0x46, 0xd3, 0x78, 0xd0, 0xa9, 0x67, 0x78, 0xc1, 0x2d,
    0x18, 0xb0, 0x33, 0xde, 0xdb, 0x27, 0xcc, 0xb0, 0x7c, 0xc9, 0xa4, 0xbd, 0xdf, 0x2b, 0x64, 0x10,
    0x32, 0x44, 0x06, 0x81, 0x21, 0xb3, 0xba, 0xcf, 0x33, 0x85, 0x49, 0x1e, 0x86, 0x4c, 0xbd, 0xf2,
    0x3d, 0x34, 0xef, 0xd6, 0x23, 0x7a, 0x9f, 0x2c, 0xda, 0x84, 0xf0, 0x83, 0x83, 0x71, 0x7d, 0xda,
    0x6e, 0x44, 0x96, 0xcd, 0x1d, 0x05, 0xde, 0x30, 0xf6, 0x1e, 0x2f, 0x9c, 0x99, 0x9c, 0x60, 0x07,
    0x03, 0x00, 0x00, 0x00
};
#endif

/*!
 * Global constant value.
 */
LwU8 hdcpLc128[16]
    GCC_ATTRIB_ALIGN(16) =
{
    0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa,
    0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa
};

LwU8 hdcpLc128e[16]
    GCC_ATTRIB_ALIGN(16) =
{
    0x55,0xdd,0x4d,0x41,0xa4,0x8a,0x93,0x7f,
    0xa2,0x4a,0x5e,0x47,0x07,0x4e,0x7a,0x50
};

#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
LwU8 hdcpTestLc128[HDCP_SIZE_LwU8(HDCP_SIZE_LC)]
    GCC_ATTRIB_ALIGN(16) =
{

    0x93,0xce,0x5a,0x56,0xa0,0xa1,0xf4,0xf7,
    0x3c,0x65,0x8a,0x1b,0xd2,0xae,0xf0,0xf7
};
#endif

/* ------------------------- Private Functions ------------------------------ */
/*!
 * _hdcpDecryptLc128
 *
 * Decrypts LC128 global constant.
 *
 */
void
_hdcpDecryptLc128(void)
{
    LwU32 hdcpLc128ePa = osDmemAddressVa2PaGet((LwU8*)hdcpLc128e);
    LwU32 hdcpLc128Pa  = osDmemAddressVa2PaGet((LwU8*)hdcpLc128);

    falc_scp_trap(TC_INFINITY);
    falc_scp_secret(HDCP_SCP_KEY_INDEX_LC128, SCP_R1);

    falc_scp_encrypt_sig(SCP_R1, SCP_R2);

    falc_scp_rkey10(SCP_R2, SCP_R1);
    falc_scp_key(SCP_R1);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)hdcpLc128ePa, SCP_R2));
    falc_dmwait();
    falc_scp_decrypt(SCP_R2, SCP_R3);
    falc_trapped_dmread(falc_sethi_i((LwU32)hdcpLc128Pa, SCP_R3));
    falc_dmwait();
    falc_scp_trap(0);
}

/*!
 * Gets the lc128 global constant.
 *
 * @param [out]  Pointer to the 128-bit buffer to store lc128.
 */
void
_hdcpGetLc128(LwU8 *pLc)
{
#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
    memcpy_hs(pLc, hdcpTestLc128, HDCP_SIZE_LwU8(HDCP_SIZE_LC));
#else
    memcpy_hs(pLc, hdcpLc128, HDCP_SIZE_LwU8(HDCP_SIZE_LC));
#endif
}

/*!
 * @brief Swaps the endianness of an integer stored as a byte array.
 * Functions works in-place or out-of-place. Used for HS code.
 *
 * @param[out]  pOutData  Pointer to buffer to store the result.
 * @param[in]   pInData   Byte array representing a integer to have its
 *                        endianness swapped.
 * @param[in]   size      The number of bytes the integer consumes.
 */
void
_hdcpSwapEndianness_hs(void* pOutData, const void* pInData, const LwU32 size)
{
    LwU32 i;

    for (i = 0; i < size / 2; i++)
    {
        LwU8 b1 = ((LwU8*)pInData)[i];
        LwU8 b2 = ((LwU8*)pInData)[size - 1 - i];
        ((LwU8*)pOutData)[i] = b2;
        ((LwU8*)pOutData)[size - 1 - i] = b1;
    }
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Swaps the endianness of an integer stored as a byte array.
 * Functions works in-place or out-of-place.
 *
 * @param[out]  pOutData  Pointer to buffer to store the result.
 * @param[in]   pInData   Byte array representing a integer to have its
 *                        endianness swapped.
 * @param[in]   size      The number of bytes the integer consumes.
 */
void
hdcpSwapEndianness(void* pOutData, const void* pInData, const LwU32 size)
{
    LwU32 i;

    for (i = 0; i < size / 2; i++)
    {
        LwU8 b1 = ((LwU8*)pInData)[i];
        LwU8 b2 = ((LwU8*)pInData)[size - 1 - i];
        ((LwU8*)pOutData)[i]            = b2;
        ((LwU8*)pOutData)[size - 1 - i] = b1;
    }
}

/*!
 * @brief Reads the DCP key into the supplied buffer.
 *
 * @param[out] pDcpKey  Buffer to store the DCP key.
 * @param[in]  size     Size of key in bytes.
 *
 * @return FLCN_OK
 *             The random number was generated.
 *         FLCN_ERR_ILWALID_ARGUMENT
 *             The buffer is not 16-byte aligned and/or the size is not a
 *             multiple of 16.
 *         FLCN_ERR_ILLEGAL_OPERATION
 *             This HS function is not allowed to run.
 */
FLCN_STATUS
hdcpGetDcpKey()
{
    FLCN_STATUS status;
    LwU32       blk        = 0;
    LwU32       imemOffset = 0;
    LwU32       dmemOffset = 0;
    LwU32       size       = 0;
    LwU32       leftSize  = 0;
    LwU32       i;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);
    OS_SEC_HS_LIB_VALIDATE(libHdcpCryptHs);

    leftSize = ALIGN_UP(HDCP_SIZE_DCP_KPUB + 4, 16);

    for (i = 0; ; i++)
    {
        falc_imtag(&blk, (LwU32)&hdcpDcpCert_imem[i * 256]);
        blk &= 0xFFFFFF;
        imemOffset = (blk * 256);
        dmemOffset = (LwU32)&hdcpKpubDcp[i * 256];

        if (leftSize > 256)
        {
            size = 256;
        }
        else
        {
            size = leftSize;
        }

        status = hdcpEcbCrypt(imemOffset, size, dmemOffset, LW_FALSE,
                              LW_FALSE, HDCP_ECB_CRYPT_MODE_IMEM_TRANSFER);

        if (leftSize > 256)
        {
            leftSize -= 256;
        }
        else
        {
            break;
        }
    }

    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return status;
}

/*!
* @brief Generates the dkey as specified by the HDCP specification.
*
* @param[in]  pSession  Session for which the dkey is needed.
* @param[in]  pDkey     Buffer to store the dkey.
* @param[in]  pTmpBuff  16-byte aligned buffer for SCP access
*
* @return FLCN_OK if a dkey was obtained; otherwise, a detailed error code
*         why a dkey could not be obtained.
*/
FLCN_STATUS
hdcpGetDkey
(
    HDCP_SESSION *pSession,
    LwU8         *pDkey,
    LwU8         *pTmpBuf
)
{
    FLCN_STATUS  status = FLCN_OK;
    LwU8        *pRtxCtr = NULL;
    LwU8        *pKm = NULL;
    LwU8        *pRn = NULL;
    LwU64        kmRnXor = 0;

    if ((NULL == pSession) ||
        (NULL == pDkey) ||
        (NULL == pTmpBuf))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(pDkey, 0, HDCP_SIZE_DKEY * 2);
    memset(pTmpBuf, 0, HDCP_SIZE_DKEY * 2);

    pRtxCtr = pTmpBuf;
    pKm = pRtxCtr + 16;
    pRn = pKm + 16;

    memcpy(pRtxCtr, (LwU8*)&(pSession->sessionState.rtx), HDCP_SIZE_RTX);
    memcpy(pRtxCtr + HDCP_SIZE_RTX, (LwU8*)&(pSession->sessionState.dkeyCtr), 8);

    // According to the spec, the counter should be in big-endian.
    hdcpSwapEndianness((LwU8*)(pRtxCtr + HDCP_SIZE_RTX),
        (LwU8*)(pRtxCtr + HDCP_SIZE_RTX), 8);

    // HDCP 2.2 spec conc
    if (pSession->sessionType == LW95A1_HDCP_CREATE_SESSION_TYPE_TMTR)
    {
        if ((pSession->receiverInfo.version != 0) &&
            (pSession->receiverInfo.version != 1))
        {
            *((LwU64*)(pRtxCtr + HDCP_SIZE_RTX)) ^= pSession->sessionState.rrx;
        }
    }
    else
    {
        if ((pSession->transmitterInfo.version != 0) &&
            (pSession->transmitterInfo.version != 1))
        {
            *((LwU64*)(pRtxCtr + HDCP_SIZE_RTX)) ^= pSession->sessionState.rrx;
        }
    }

    // Now XOR Rn with least significant 64-bits of Km.
    memcpy(pKm, pSession->sessionState.km, HDCP_SIZE_KM);
    memcpy(pRn, (LwU8*)&pSession->sessionState.rn, HDCP_SIZE_RN);
    kmRnXor = *((LwU64*)(pKm + 8)) ^ *((LwU64*)pRn);
    memcpy((LwU8*)(pKm + 8), (LwU8*)&kmRnXor, 8);

    // Enter HS mode and process the method
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpGenerateDkey));
    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpGenerateDkey), NULL, 0);

    status = hdcpGenerateDkey(pRtxCtr, pKm, pDkey);

    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpGenerateDkey));

    if (FLCN_OK == status)
    {
        pSession->sessionState.dkeyCtr++;
    }

    return status;
}

/*!
 * @brief Encrypts the content using the session key and counters.
 *
 * @param[in]  pState     States used for encryption.
 * @param[in]  inBuf      FB input buffer address (256-byte aligned).
 * @param[in]  numBlocks  Number of 16-byte blocks in input buffer.
 * @param[in]  streamId   Stream ID for this receiver.
 * @param[in]  outBuf     FB output buffer address (256-byte aligned).
 * @param[in]  pBuf       A 256-byte aligned buffer used for transferring data.
 *                        This buffer needs to be able to hold
 *                        HDCP_SIZE_ENCRYPT_BUFFER bytes.
 * @param[in]  inCtr      Input counter.
 * @param[in]  encOff     Offset from outBuf where the encrypted results are to
 *                        be stored.
 *
 * @return FLCN_OK if the encryption was successful; otherwise, an error code
 *         returned by the called functions.
 */
FLCN_STATUS
hdcpEncrypt
(
    HDCP_ENCRYPTION_STATE *pState,
    LwU64                  inBuf,
    LwU32                  numBlocks,
    LwU8                   streamId,
    LwU64                  outBuf,
    LwU8                  *pBuf,
    LwU64                 *inCtr,
    LwU32                  encOff
)
{
    FLCN_STATUS  status = FLCN_OK;
    LwU32        tmpNumBlks = 0;
    LwU32        fbOffset = encOff;
    LwU8        *pAesData = NULL;
    LwU8        *pKs = NULL;
    LwU8        *pLc = NULL;
    LwU32        lowriv = 0;
    LwU32        sCtr = 0;
    LwU64        iCtr = 0;
    LwU64        cntrIndex = (LwU64)streamId;
    LwU32        dmemInBufOffset;
    LwU32        scpOutBufOffset;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);
    OS_SEC_HS_LIB_VALIDATE(libMemHs);
    OS_SEC_HS_LIB_VALIDATE(libDmaHs);
    OS_SEC_HS_LIB_VALIDATE(libHdcpCryptHs);

    if (NULL == pState)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    _hdcpDecryptLc128();

    pAesData = pBuf;
    pKs = pAesData + HDCP_SIZE_AES_ENCRYPTION_BLOCK;
    pLc = pKs + HDCP_SIZE_KS;

    // Stream counter should be in big-endian.
    sCtr = (pState->streamCtr)[cntrIndex];
    _hdcpSwapEndianness_hs(&sCtr, &sCtr, sizeof(LwU32));
    memcpy_hs(&lowriv, ((LwU8*)&(pState->riv)) + 4, sizeof(LwU32));
    lowriv = lowriv ^ (sCtr);

    // Input counter should be in big-endian.
    iCtr = *inCtr;
    _hdcpSwapEndianness_hs((LwU8*)&iCtr, (LwU8*)&iCtr, sizeof(LwU64));
    memcpy_hs(pAesData, (LwU8*)&(pState->riv), sizeof(LwU32));
    memcpy_hs(pAesData + 4, (LwU8*)&lowriv, sizeof(LwU32));
    memcpy_hs(pAesData + 8, (LwU8*)&(iCtr), sizeof(LwU64));
    memcpy_hs(pKs, pState->ks, HDCP_SIZE_KS);

    _hdcpGetLc128(pLc);
    hdcpSetEncryptionKeys(pAesData, pKs, pLc);

    dmemInBufOffset = (LwU32)pBuf;
    scpOutBufOffset = ((LwU32)pBuf) + (HDCP_SIZE_ENCRYPTION_BUFFER / 2);

    while (numBlocks > 0)
    {
        tmpNumBlks = (HDCP_SIZE_ENCRYPTION_BUFFER / 2) /
            HDCP_SIZE_AES_ENCRYPTION_BLOCK;
        if (numBlocks < tmpNumBlks)
        {
            tmpNumBlks = numBlocks;
        }

        // Transfer from FB to DMEM
        status = hdcpDmaRead_hs((void*)dmemInBufOffset, inBuf + (fbOffset >> 8),
                                tmpNumBlks * HDCP_SIZE_AES_ENCRYPTION_BLOCK);
        if (FLCN_OK != status)
        {
            goto hdcpEncrypt_end;
        }

        // Encrypt and put result back in DMEM
        status = hdcpEcbCrypt((LwU32)dmemInBufOffset,
                              tmpNumBlks * HDCP_SIZE_AES_ENCRYPTION_BLOCK,
                              scpOutBufOffset, LW_TRUE, LW_FALSE,
                              HDCP_ECB_CRYPT_MODE_HDCP_ENCRYPT);
        if (FLCN_OK != status)
        {
            goto hdcpEncrypt_end;
        }
        numBlocks -= tmpNumBlks;

        // Transfer encrypted content to FB.
        status = hdcpDmaWrite_hs((void*)scpOutBufOffset, outBuf + (fbOffset >> 8),
                                 tmpNumBlks * HDCP_SIZE_AES_ENCRYPTION_BLOCK);
        if (FLCN_OK != status)
        {
            goto hdcpEncrypt_end;
        }

        fbOffset += (tmpNumBlks * HDCP_SIZE_AES_ENCRYPTION_BLOCK);
        (*inCtr) += tmpNumBlks;
    }

hdcpEncrypt_end:
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();
    return status;
}

/*!
 * @brief Encrypts/decrypts epair.
 *
 * @param[in] pEpair      Epair to be encrypted/decrypted.
 * @param[in] pRandNum    Random number used for encrypting/decrypting epair.
 * @param[in] bIsEncrypt  Specifies whether we are encrypting (LW_TRUE) or
 *                        decrypting (LW_FALSE) epair.
 *
 * @return FLCN_OK if epair was successfully encrypted/decrypted; otherwise,
 *         an error code returned by called functions.
 */
FLCN_STATUS
hdcpCryptEpair
(
    LwU8   *pEpair,
    LwU8   *pRandNum,
    LwBool  bIsEncrypt
)
{
    FLCN_STATUS status = FLCN_OK;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);
    OS_SEC_HS_LIB_VALIDATE(libHdcpCryptHs);
    OS_SEC_HS_LIB_VALIDATE(libMemHs);

    _hdcpGetLc128(pRandNum);
    hdcpSetEpairEncryptionKey(pRandNum, bIsEncrypt);

    status = hdcpEcbCrypt((LwU32)pEpair, HDCP_SIZE_EPAIR,
                          (LwU32)pEpair, bIsEncrypt, LW_FALSE,
                          HDCP_CBC_CRYPT_MODE_CRYPT);

    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return status;
}
#endif
