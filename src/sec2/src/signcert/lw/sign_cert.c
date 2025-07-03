/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sign_cert.c
 * @brief  Functions which are used to create and sign EK Certificate.  All of 
 *            these funcitons need to be in HS in order to directly access EK 
 *            private.   Funcitons needed:
 *         signCertEkSignHash -   ECC P256 signature using EK private.
 *         signCertGetEkPrivate - Return Public key gnerated from EK Private.
 *         Other functions as needed for Certificate fields such s TCG-DICE-FWID.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
// ONLY use if APM
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
#include "lwosselwreovly.h"
#include "sec2_hs.h"
#include "mem_hs.h"
#include "dev_sec_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objsec2.h"
#include "seapi.h"
#include "base.h"
#include "lwtypes.h"
#include "lw_crypt.h"
#include "hwref/ampere/ga100/bootrom_api_parameters.h"  //@@@CPB-HALIFY
#include "hwref/ampere/ga100/dev_fuse.h"
#include "hwref/ampere/ga100/dev_sec_pri.h"
#include "sign_cert.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define BROM_API_SIGN_ECDSA_P256(parm)  falc_strap(LW_PSEC_FALCON_BROM_FUNID_ID_ECDSASIGN, (LwU32*)parm)

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS derivePubKeyFromPrivKey(LwU32 *pPubKey, LwU32 *pPrivKey)
    GCC_ATTRIB_SECTION("imem_spdm", "_derivePubKeyFromPrivKey");
    
FLCN_STATUS signCertCheckIfApmIsSupported_GA100(void)
    GCC_ATTRIB_SECTION("imem_spdm", "_signCertCheckIfApmIsSupported_GA100");

/* ------------------------- Global Variables ------------------------------- */
SW_PARAM_SIGN_ECDSA_P256 g_bromEcdsaParms
    GCC_ATTRIB_SECTION("dmem_spdm", "g_bromEcdsaParms");

//@@@CPB-TEMP-EK
// "Fake" EK Private.  This is a hard coded "fake" EK which will be used on debug cards
//      and Fuse Decrypt Fails.  Should never be used on produciton cards.
// 
LwU8  g_defaultEkPrivate[ECC_P256_SIZE_DWORDS * 4]
    GCC_ATTRIB_SECTION("dmem_spdm", "g_defaultEkPrivate") = 
{
    0xc3, 0x2a, 0xb7, 0xe7, 0xa8, 0x84, 0x21, 0xd0, 0x42, 0x86, 0xea, 0x13, 0x20, 0x51, 0x5f, 0x00,
    0xe5, 0x7c, 0x60, 0x63, 0xe4, 0x6f, 0x94, 0xd8, 0xeb, 0xce, 0xb0, 0x48, 0x04, 0x94, 0x82, 0x37,
};

LwU32 g_ekPrivate[ECC_P256_PRIVKEY_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "g_ekPrivate");

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/* The constants for the lwrve NIST P-256 aka secp256r1 aka secp256v1 */
//#define ECC_P256_INTEGER_SIZE_IN_DWORDS        8u

extern LwU32 ECC_P256_A[ECC_P256_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_A");

extern LwU32 ECC_P256_B[ECC_P256_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_B");

extern LwU32 ECC_P256_GenPoint[ECC_P256_POINT_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_GenPoint");

extern LwU32 ECC_P256_Modulus[ECC_P256_INTEGER_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "ECC_P256_Modulus");

// For now, only allow on debug boards.
//  TODO - needs to be HAL.
FLCN_STATUS
signCertCheckIfApmIsSupported_GA100
(
    void
)
{
    LwU32       chip;
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       val;
    
    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);
    
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GA100:
            flcnStatus = FLCN_OK;
            break;
    }

    if (flcnStatus == FLCN_OK)
    {
        //
        // If this is a debug board, continue allowing APM to be enabled
        //
        CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS, &val));
        if (val == LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_NO)
        {
            return FLCN_OK;
        }
    }
ErrorExit:
    return flcnStatus;
}
    
// @@@CPB-HS-OVL - This function will be mainly in HS when the HS OVL is added.
// @@@CPB-HAL-ify - Due to key size changes, this function will be HAL-ified.
FLCN_STATUS
derivePubKeyFromPrivKey
(
    LwU32 *pPubKey,
    LwU32 *pPrivKey
)
{
    FLCN_STATUS  flcnStatus = FLCN_OK;
    LwU32       *pPubPointX = NULL;
    LwU32       *pPubPointY = NULL;
    LwU32       *pGenPointX = NULL;
    LwU32       *pGenPointY = NULL;

    // Obtain public key by multiplying generator point with private key
    pGenPointX = (LwU32 *)&ECC_P256_GenPoint[0];
    pGenPointY = (LwU32 *)&(ECC_P256_GenPoint[ECC_P256_INTEGER_SIZE_IN_DWORDS]);
    pPubPointX = pPubKey;
    pPubPointY = &(pPubKey[ECC_P256_INTEGER_SIZE_IN_DWORDS]);

    if (seECPointMult((LwU32 *)ECC_P256_Modulus, (LwU32 *)ECC_P256_A,
                      pGenPointX, pGenPointY, pPrivKey, pPubPointX, pPubPointY,
                      SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        // Either SE engine failure, or private key otherwise invalid
        flcnStatus = FLCN_ERROR;
        goto ErrorExit;
    }
    
    // Ensure public key is on lwrve.  Needs to be LS as don't yet have seECPointVerification in HS
    if (seECPointVerification(ECC_P256_Modulus, ECC_P256_A, ECC_P256_B,
                              pPubPointX, pPubPointY,
                              SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        flcnStatus = FLCN_ERROR;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;
}

// TODO CPB - @@@CPB-HS-OVL - This function will basically call HS verision of itself to get Pub EK.
// TODO CPB - @@@CPB-HAL-ify - Due to key size changes, this function will be HAL-ified.
FLCN_STATUS
signCertGetEkPublic
(
    LwU32 *pPubKey
)
{
    FLCN_STATUS  status = FLCN_OK;

    // Make sure on supported board
    if (signCertCheckIfApmIsSupported_GA100() != FLCN_OK)
    {
	status = FLCN_ERROR;
	goto signCertGetEkPublic_exit;
    }
    
    // For now get EK from HS OVL
    status = signCertGetEkPrivate(&g_ekPrivate[0], ECC_P256_PRIVKEY_SIZE_IN_BYTES); //@@@CPB-TEMP-EK
    if (status != FLCN_OK)
    {
        goto signCertGetEkPublic_exit;
    }

    // Drive Public EK form Private EK.
    status = derivePubKeyFromPrivKey(&pPubKey[0], &g_ekPrivate[0]);

signCertGetEkPublic_exit:
    return status;
}

// TODO CPB - @@@CPB-HS-OVL - This function will exist as a HS only funciton, not callable from NS.
// TODO CPB - @@@CPB-HAL-ify - Due to key size changes, this function will be HAL-ified.
FLCN_STATUS
signCertGetEkPrivate
(
    LwU32 *pPrivKey,
    LwU32 privKeySize
)
{
    FLCN_STATUS status = FLCN_OK;

    // TODO CPB - will only be allowed when debug board and Fuse Decrypt fails.
    memcpy(pPrivKey, &g_defaultEkPrivate[0], ECC_P256_SIZE_DWORDS * 4);

    return status;
}

// TODO CPB - @@@CPB-HS-OVL - This function will basically call HS verision of itself to get Pub EK.
// TODO CPB - @@@CPB-HAL-ify - Due to key size changes (and BROM API), this function will be HAL-ified.
FLCN_STATUS signCertEkSignHash
(
    LwU32  *pHash,
    LwU32  *pSig
)
{
    FLCN_STATUS status = FLCN_OK;

    // Make sure on supported board
    if (signCertCheckIfApmIsSupported_GA100() != FLCN_OK)
    {
	status = FLCN_ERROR;
	goto ErrorExit;
    }

    // Build BROM parm block.
    memset(&g_bromEcdsaParms, 0, sizeof(g_bromEcdsaParms));
    memcpy(&g_bromEcdsaParms.hash, pHash, ECC_P256_SIZE_DWORDS * 4);
    memcpy(&g_bromEcdsaParms.privKey, &g_defaultEkPrivate[0], ECC_P256_SIZE_DWORDS * 4);

    // Call BROM to do the ecdsa sign.
    BROM_API_SIGN_ECDSA_P256(&g_bromEcdsaParms);
    LwU32 brRetCode = BAR0_REG_RD32(LW_PSEC_FALCON_BROM_RETCODE);
    if (!FLD_TEST_DRF(_PSEC, _FALCON_BROM_RETCODE, _ERR, _SUCCESS, brRetCode)  ||   // BROM Fail or
         g_bromEcdsaParms.sig.R[0] == 0)                                            // return zero sig
    {
        status = FLCN_ERROR;
        goto ErrorExit;
    }
    else
    {
        // Copy out signiture
        memcpy((LwU8 *)pSig, (LwU8 *)&g_bromEcdsaParms.sig, sizeof(ECC_P256_SIGNATURE));
    }

ErrorExit:
    return status;
}
#endif //SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM)
