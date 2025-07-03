/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_srm_revocation.c
 * @brief  Hdcp22 wrapper Functions for security
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_srm.h"
#include "csb.h"
#include "common_hslib.h"
#include "lib_hdcpauth.h"
#ifdef GSPLITE_RTOS
#include "sha1_hs.h"
#else
#include "sha1.h"
#endif

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
static FLCN_STATUS _hdcp22CheckRevocation(LwU8 *pKsvList, LwU32 noOfKsvs, LwU8 *pRevDev)
    GCC_ATTRIB_SECTION("imem_hdcp22SrmRevocation", "_hdcp22CheckRevocation");
static FLCN_STATUS _hdcp22Srm1VRLsRevocation(LwU8 *pKsvList, LwU32 noOfKsvs, LwU8 *pSrmBuf, LwU32 *pLength, LwU32 *pDoneB)
    GCC_ATTRIB_SECTION("imem_hdcp22SrmRevocation", "_hdcp22Srm1VRLsRevocation");
static FLCN_STATUS _hdcp22SrmRevocation(LwU8 *pKsvList, LwU32 noOfKsvs, LwU8 *pSrmBuf, LwU32  ctlOptions)
    GCC_ATTRIB_SECTION("imem_hdcp22SrmRevocation", "_hdcp22SrmRevocation");

/* ------------------------ Static variables -------------------------------- */
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
LwU8 g_HDCP_SRM_Generator[SRM_GENERATOR_SIZE] GCC_ATTRIB_ALIGN(sizeof(LwU32)) =
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
LwU8 g_HDCP_SRM_PublicKey[SRM_MODULUS_SIZE] GCC_ATTRIB_ALIGN(sizeof(LwU32)) =
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

/*!
 * @brief DSA test public key for use with test vectors.
 */
LwU8 g_HDCP_SRM_TestPublicKey[SRM_MODULUS_SIZE] GCC_ATTRIB_ALIGN(sizeof(LwU32)) =
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

/* ------------------------ Static Functions -------------------------------- */
/*
 * _hdcp22CheckRevocation
 *
 * @brief Find if given known revoked receiver ID is present in the KsvList
 *
 * @param[in]  pKsvList     List of receiver Ids to be checked against SRM
 * @param[in]  noOfKsvs     Number of Ksvs in the above list
 * @param[in]  pRevDev      Known revoked device from SRM
 */
static FLCN_STATUS
_hdcp22CheckRevocation
(
    LwU8  *pKsvList,
    LwU32  noOfKsvs,
    LwU8  *pRevDev
)
{
    LwU32 indx = 0;

    while (indx < noOfKsvs)
    {
        if(!HDCP22WIRED_SEC_ACTION_MEMCMP(&pKsvList[indx * HDCP22_SIZE_RECV_ID_8],
                                         pRevDev,
                                         HDCP22_SIZE_RECV_ID_8))
        {
            return FLCN_ERR_HDCP_RECV_REVOKED;
        }
        ++indx;
    }
    return FLCN_OK;
}

/*
 * @brief Handles SRM1 VRLs revocation check
 * @param[in]   pKsvList    List of receiver Ids to be checked against SRM
 * @param[in]   noOfKsvs    Number of Ksvs in the above list
 * @param[in]   pSrmBuf     Destination in DMEM.
 * @param[in]   pLength     Pointer to length var.
 * @param[in]   pDoneB      Pointer to doneB var.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
static FLCN_STATUS
_hdcp22Srm1VRLsRevocation
(
    LwU8               *pKsvList,
    LwU32               noOfKsvs,
    LwU8               *pSrmBuf,
    LwU32              *pLength,
    LwU32              *pDoneB
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwU32       noOfDevices = 0;
    LwU32       tmp         = 0;

    // Process VRLs.
    while (*pLength)
    {
        tmp = pSrmBuf[(*pDoneB)];
        noOfDevices = DRF_VAL(_HDCP22, _SRM_1X_BITS, _NO_OF_DEVICES, tmp);
        (*pDoneB) += 1;
        (*pLength) -= 1;

        if ((*pLength) < (noOfDevices * HDCP22_SIZE_RECV_ID_8))
        {
            status = FLCN_ERR_HDCP_ILWALID_SRM;
            goto label_return;
        }

        // Revocation check.
        while (noOfDevices)
        {
            CHECK_STATUS(_hdcp22CheckRevocation(pKsvList, noOfKsvs, &pSrmBuf[(*pDoneB)]));
            noOfDevices--;
            (*pDoneB) += HDCP22_SIZE_RECV_ID_8;
            (*pLength) -= HDCP22_SIZE_RECV_ID_8;
        }
    }

    if ((*pLength) != 0)
    {
        status = FLCN_ERR_HDCP_ILWALID_SRM;
    }

label_return:
    return status;
}

/*
 * _hdcp22SrmRevocation
 *
 * @brief Find if given known revoked receiver ID is present in the KsvList with HDCP2 SRM.
 *
 * @param[in]  pKsvList     List of receiver Ids to be checked against SRM
 * @param[in]  noOfKsvs     Number of Ksvs in the above list
 * @param[in]  pSrmBuf      Pointer to SRM buffer.
 * @param[in]  ctlOptions   Flagto tell if using test vector for SRM1X
 *
 * @returns FLCN_STATUS: FLCN_OK indicates SRM verified and device not revocated.
 */
static FLCN_STATUS
_hdcp22SrmRevocation
(
    LwU8   *pKsvList,
    LwU32   noOfKsvs,
    LwU8   *pSrmBuf,
    LwU32   ctlOptions
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       doneB   = 0;
    LwU32       srmIndx = 0;
    LwU32       length  = 0;
    LwU32       tmp     = 0;
    LwU32       srmGen  = 0;
    LwU32       noOfDevices = 0;
    LwU8        srmHdcp2Indicator = 0;

    do
    {
        srmIndx += 1;

        if (srmIndx == 1)
        {
            // The 1st generation SRM.

            // The 1st word has version and HDCP2 indicator which already checked.
            // 1st word for SRM version, indicator.
            tmp = *((LwU32 *)pSrmBuf);
            HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS(&tmp, &tmp, sizeof(LwU32));
            srmHdcp2Indicator = DRF_VAL(_HDCP22,_SRM_HEADER_BITS,_HDCP2_IND, tmp);

            doneB += 4;

            // Read the 2nd workd for VRL length, generation number
            tmp = (pSrmBuf[doneB] << 24) | (pSrmBuf[doneB+1] << 16) | (pSrmBuf[doneB+2] << 8) | (pSrmBuf[doneB+3]);

            length = DRF_VAL(_HDCP22, _SRM_1X_BITS, _LENGTH, tmp);
            srmGen = DRF_VAL(_HDCP22, _SRM_1X_BITS, _GENERATION, tmp);
            doneB += 4;

            if (srmHdcp2Indicator)
            {
                // Read the 3rd word for number of devices.
                noOfDevices = ((pSrmBuf[9] & 0xC0) >> 6) | (pSrmBuf[8] << 8);
                doneB += 4;
                length -= 4;

                // Subtract signature and size of length field itself.
                length -= (3 + HDCP22_SRM_SIGNATURE_SIZE);
                if (length != (noOfDevices * HDCP22_SIZE_RECV_ID_8))
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                // Revocation check.
                while (noOfDevices)
                {
                    CHECK_STATUS(_hdcp22CheckRevocation(pKsvList, noOfKsvs,
                                                        &pSrmBuf[doneB]));
                    noOfDevices--;
                    doneB += HDCP22_SIZE_RECV_ID_8;
                }
            }
            else
            {
                // Subtract signature and size of length field itself.
                if (length < (3 + HDCP22_SRM_DSA_SIGNATURE_SIZE))
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                length -= (3 + HDCP22_SRM_DSA_SIGNATURE_SIZE);

                CHECK_STATUS(_hdcp22Srm1VRLsRevocation(pKsvList, noOfKsvs, pSrmBuf, &length, &doneB));
            }
        }
        else
        {
            // Following generations.

            if (srmHdcp2Indicator)
            {
                // Read Length for next gen, number of devices.
                tmp = (pSrmBuf[doneB] << 24) | (pSrmBuf[doneB+1] << 16) | (pSrmBuf[doneB+2] << 8) | (pSrmBuf[doneB+3]);

                length      = DRF_VAL(_HDCP22, _SRM_2X_NEXT_GEN, _LENGTH, tmp);
                noOfDevices = DRF_VAL(_HDCP22, _SRM_2X_NEXT_GEN, _NO_OF_DEVICES, tmp);
                doneB += 4;

                length -= (4 + HDCP22_SRM_SIGNATURE_SIZE);
                if (length != (noOfDevices * HDCP22_SIZE_RECV_ID_8))
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                // Read deviceIds and revocation check.
                while (noOfDevices)
                {
                    CHECK_STATUS(_hdcp22CheckRevocation(pKsvList, noOfKsvs,
                                                        &pSrmBuf[doneB]));
                    noOfDevices--;
                    doneB += HDCP22_SIZE_RECV_ID_8;
                }
            }
            else
            {
                // Read Length for next gen, number of devices.
                tmp = (pSrmBuf[doneB] << 8) | pSrmBuf[doneB+1];
                length = DRF_VAL(_HDCP22, _SRM_1X_NEXT_GEN, _LENGTH, tmp);
                doneB += 2;

                if (length < (2 + HDCP22_SRM_DSA_SIGNATURE_SIZE))
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }
                length -= (2 + HDCP22_SRM_DSA_SIGNATURE_SIZE);

                CHECK_STATUS(_hdcp22Srm1VRLsRevocation(pKsvList, noOfKsvs, pSrmBuf, &length, &doneB));
            }
        }

        // Validate SRM signature.
        if (srmHdcp2Indicator)
        {
            HDCP22_DCP_SIGNATURE sign;

            HDCP22WIRED_SEC_ACTION_MEMSET(&sign, 0, sizeof(sign));
            HDCP22WIRED_SEC_ACTION_MEMCPY(sign.signature, &pSrmBuf[doneB],
                                          HDCP22_SRM_SIGNATURE_SIZE);

            hdcpAttachAndLoadOverlay(HDCP22WIRED_RSA_SIGNATURE);
            status = hdcp22RsaVerifyDcpSignature(&pSrmBuf[0], doneB, sign.signature);
            hdcpDetachOverlay(HDCP22WIRED_RSA_SIGNATURE);

            doneB += HDCP22_SRM_SIGNATURE_SIZE;
        }
        else
        {
            hdcpAttachAndLoadOverlay(HDCP22WIRED_DSA_SIGNATURE);
            status = hdcp22DsaVerifySignature(&pSrmBuf[0], doneB, &pSrmBuf[doneB], ctlOptions);
            hdcpDetachOverlay(HDCP22WIRED_DSA_SIGNATURE);

            doneB += HDCP22_SRM_DSA_SIGNATURE_SIZE;
        }

        if (status != FLCN_OK)
        {
            goto label_return;
        }
    } while (srmIndx < srmGen);

label_return:

    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*
 * hdcp22DsaVerifySignature
 *
 * @brief Validate HDCP1X SRM signature.
 * @param[in]   pData       Pointer to input data.
 * @param[in]   dataLength  Input data length.
 * @param[in]   pSignature  Pointer to signature data.
 * @param[in]   ctlOptions  Flagto tell if using debug/prod setting.
 *
 * @returns FLCN_STATUS: FLCN_OK indicates that SRM has been verified.
 */
FLCN_STATUS
hdcp22DsaVerifySignature
(
    LwU8   *pData,
    LwU32   dataLength,
    LwU8   *pSignature,
    LwU32   ctlOptions
)
{
    FLCN_STATUS status = FLCN_ERROR;
#ifndef HDCP22_USE_HW_SHA
    SHA1_CONTEXT context;
#endif // !HDCP22_USE_HW_SHA
    LwU32  signatureR[LW_HDCPSRM_SIGNATURE_SIZE/sizeof(LwU32)];
    LwU32  signatureS[LW_HDCPSRM_SIGNATURE_SIZE/sizeof(LwU32)];
    LwU32  sha1Hash[LW_HDCPSRM_SIGNATURE_SIZE/sizeof(LwU32)];
    LwU32  u1[SRM_MODULUS_SIZE/sizeof(LwU32)];
    LwU32  u2[SRM_MODULUS_SIZE/sizeof(LwU32)];
    LwU32  w[SRM_MODULUS_SIZE/sizeof(LwU32)];

    HDCP22WIRED_SEC_ACTION_MEMSET(signatureR, 0, sizeof(signatureR));
    HDCP22WIRED_SEC_ACTION_MEMSET(signatureS, 0, sizeof(signatureS));
    HDCP22WIRED_SEC_ACTION_MEMSET(sha1Hash, 0, sizeof(sha1Hash));
    HDCP22WIRED_SEC_ACTION_MEMSET(u1, 0, sizeof(u1));
    HDCP22WIRED_SEC_ACTION_MEMSET(u2, 0, sizeof(u2));

#ifdef HDCP22_USE_HW_SHA
    status = HDCP22WIRED_SEC_ACTION_SHA1((LwU8*)sha1Hash, pData, dataLength);
    if (status != FLCN_OK)
    {
        return status;
    }
#else
    // Callwlate the SHA-1
    hdcpAttachAndLoadOverlay(HDCP_SHA1);
    HDCP22WIRED_SEC_ACTION_SHA1_INITIALIZE(&context);
    HDCP22WIRED_SEC_ACTION_SHA1_UPDATE(&context, pData, dataLength);
    HDCP22WIRED_SEC_ACTION_SHA1_FINAL((LwU8*)sha1Hash, &context);
    hdcpDetachOverlay(HDCP_SHA1);
#endif // HDCP22_USE_HW_SHA
    HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS(sha1Hash, sha1Hash,
                                           LW_HDCPSRM_SIGNATURE_SIZE);

    HDCP22WIRED_SEC_ACTION_MEMCPY(signatureR, pSignature, LW_HDCPSRM_SIGNATURE_SIZE);

    // Colwerts a 20 bytes array into a big integer.
    HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS(signatureR, signatureR,
                                           LW_HDCPSRM_SIGNATURE_SIZE);

    // Colwerts a 20 bytes array into a big integer.
    HDCP22WIRED_SEC_ACTION_MEMCPY(signatureS, &pSignature[LW_HDCPSRM_SIGNATURE_SIZE],
                                  LW_HDCPSRM_SIGNATURE_SIZE);
    HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS(signatureS, signatureS,
                                           LW_HDCPSRM_SIGNATURE_SIZE);


    //
    // Verify the DSA signature.
    //
    // The following values need to be made available to the signature
    // verifier.
    //
    // p = prime modulus: Provided by HDCP spec (HDCP_Modulus)
    // q = prime divisor: Provided by HDCP spec (HDCP_Divisor)
    // g = generator: Provided by HDCP spec (g_HDCP_SRM_Generator)
    // y = public key: Provided by sender (g_HDCP_SRM_PublicKey)
    // r = SRM signature, "r" value: Found in SRM (signatureR)
    // s = SRM signature, "s" value: Found in SRM (signatureS)
    // h = SHA-1(message): Callwlated from SRM (pSha1Hash)
    //
    // Steps needed to verify the DSA signature:
    //
    //  w = pow(s, -1) % q
    // u1 = (sha1(m) * w) % q
    // u2 = (r * w) % q
    //  v = ((pow(g, u1) * pow(y, u2)) % p) % q
    //
    // if (v == r) then the signature is verified, and the verifier can have
    // high confidence that the received message was sent by the party holding
    // the public key
    //
    hdcpAttachAndLoadOverlay(HDCP_BIGINT);

    // w = pow(s, -1) % q
    HDCP22WIRED_SEC_ACTION_BIGINT_ILWERSE_MOD(w, signatureS, &HDCP_Divisor);

    // u1 = (sha1(m) * w) % q
    HDCP22WIRED_SEC_ACTION_BIGINT_MULTIPLY_MOD(u1, sha1Hash, w, &HDCP_Divisor);

    // u2 = (r * w) % q
    HDCP22WIRED_SEC_ACTION_BIGINT_MULTIPLY_MOD(u2, signatureR, w,
                                               &HDCP_Divisor);

    //
    // Since (pow(g, u1) * pow(y, u2)) may too large for the buffers we use,
    // we need to rely on a property of mod to keep the values in more
    // manageable chunks.
    //
    // (a * b) % c = ((a % c) * (b % c)) % c
    //

    //
    // Don't need the value stored in w anymore; reuse the variable
    // w = pow(g, u1) % p
    //
    HDCP22WIRED_SEC_ACTION_BIGINT_POWER_MOD(w, (LwU32 *)g_HDCP_SRM_Generator,
                                            u1, &HDCP_Modulus,
                                            SRM_MODULUS_SIZE/sizeof(LwU32));

    //
    // Don't need the value stored in u1 anymore; reuse the variable
    // u1 = pow(y, u2) % p
    //
   if (FLD_TEST_DRF(_FLCN, _HDCP_CTL_OPTIONS, _KEY_TYPE, _PROD, ctlOptions))
    {
        HDCP22WIRED_SEC_ACTION_BIGINT_POWER_MOD(u1, (LwU32 *)g_HDCP_SRM_PublicKey,
                                                u2, &HDCP_Modulus,
                                                SRM_MODULUS_SIZE/sizeof(LwU32));
    }
    else
    {
        HDCP22WIRED_SEC_ACTION_BIGINT_POWER_MOD(u1, (LwU32 *)g_HDCP_SRM_TestPublicKey,
                                                u2, &HDCP_Modulus,
                                                SRM_MODULUS_SIZE/sizeof(LwU32));
    }

    //
    // Don't need the value store in u2 anymore; reuse the variable
    // u2 = (w * u1) % p
    //
    HDCP22WIRED_SEC_ACTION_BIGINT_MULTIPLY_MOD(u2, w, u1, &HDCP_Modulus);

    // At this point, we have v = u2 % q; reuse w as v
    HDCP22WIRED_SEC_ACTION_BIGINT_MOD(w, u2, &HDCP_Divisor,
                                      SRM_MODULUS_SIZE/sizeof(LwU32));

    // If (w == r), the signature is verified
    if (HDCP22WIRED_SEC_ACTION_BIGINT_COMPARE(w, signatureR,
                                              SRM_DIVISOR_SIZE/sizeof(LwU32)))
    {
        status = FLCN_ERR_HDCP_ILWALID_SRM;
    }
    else
    {
        status = FLCN_OK;
    }

    hdcpDetachOverlay(HDCP_BIGINT);

    return status;
}

/*!
 * @brief Function to verify certificate.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredSrmRevocationHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_SRM_REVOCATION_ARG pActionArg  = &pArg->action.secActionSrmRevocation;
    SELWRE_ACTION_TYPE                prevState   = SELWRE_ACTION_ILWALID;
    FLCN_STATUS status  = FLCN_OK;
    LwU32       index   = pActionArg->totalSrmSize;
    LwU8       *pSrm    = (LwU8*)pActionArg->srm;

    //
    // Cmd VALIDATE_SRM2 is to validate SRM only without ksvList and possible any state.
    // Therefore, cannot check if 0 ksv devices and expecting state.
    //

    // Check if input SRM buffer tainted.
    while (index < HDCP22_SIZE_SRM_MAX)
    {
        if (pSrm[index] != 0x00)
        {
            status = FLCN_ERR_HS_CHK_ILWALID_INPUT;
            goto label_return;
        }
        index++;
    }

    //
    // TODO: To support pre-built SRM and check assigned SRM version if newer than
    // pre-built one.
    //

    status = hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired, HDCP22_SECRET_PREV_STATE,
                                                 (LwU8 *)&prevState);

    // Cmd VALIDATE_SRM2 is stateless and may not get prevState to check.
    if ((status == FLCN_OK) && (prevState == SELWRE_ACTION_VERIFY_CERTIFICATE))
    {
        LwU8 rxId[HDCP22_SIZE_RECV_ID_8];

        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_RX_ID,
                                                         (LwU8*)rxId));

        // If AKE stage, we are doing SRM revocation with rxId.
        status = _hdcp22SrmRevocation(rxId,
                                      1,
                                      (LwU8*)pActionArg->srm,
                                      pActionArg->ctrlOptions);

        //
        // TODO: If pre-built SRM supported, update prevState to SELWRE_ACTION_CERTIFICATE_SRM_CHECKED
        // for next action state check.
        //
    }
    else
    {
        // TODO: Add ksvList hash at vprime validation and hash here if not VALIDATE_SRM2 cmd.

        status = _hdcp22SrmRevocation(pActionArg->ksvList,
                                      pActionArg->noOfKsvs,
                                      (LwU8*)pActionArg->srm,
                                      pActionArg->ctrlOptions);

        //
        // TODO: If not VALIDATE_SRM2 cmd case and pre-built SRM supported, update prevState to
        // SELWRE_ACTION_SRM_REVOCATION for next action state check.
        //
    }

label_return:

    return status;
}
