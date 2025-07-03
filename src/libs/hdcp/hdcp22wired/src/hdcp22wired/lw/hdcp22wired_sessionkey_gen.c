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
 * @file   hdcp22wired_sessionkey_gen.c
 * @brief  Hdcp22 wrapper Functions for security
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
#ifdef GSPLITE_RTOS
static void _hdcp22wiredGenerateSessionkeyEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateSessionkey", "start");
#endif

static FLCN_STATUS _hdcp22GenerateEks(LwU64* pRrx, LwU32 *pKs, LwU32 *pEks)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateSessionkey", "_hdcp22GenerateEks");
FLCN_STATUS hdcp22wiredEksGenHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22GenerateSessionkey", "hdcp22wiredEksGenHandler");

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Private Functions ------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredGenerateSessionkeyEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/*
 * @brief: This function generates encrypted Ks
 * @param[in]  pRrx             Pointer to receiver rRx.
 * @param[in]  pKs              Pointer to Ks array.
 * @param[out] pEks             Pointer to encrypted Ks array.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22GenerateEks
(
    LwU64      *pRrx,
    LwU32      *pKs,
    LwU32      *pEks
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU64       ksTmpBuff[(HDCP22_SIZE_KS + HDCP22_SIZE_EKS)/LW_SIZEOF32(LwU64)];
    LwU64       dKeyTmpBuf[HDCP22_SIZE_DKEY/LW_SIZEOF32(LwU64)];    // LwU64 aligned because LwU64 xor operation.
    LwU64       dKey2LSB;
    LwU8       *pdKey2  = (LwU8 *)dKeyTmpBuf;
    LwU8       *pTmpKs  = NULL;
    LwU8       *pTmpEks = NULL;
    LwU32       km[HDCP22_SIZE_KM/LW_SIZEOF32(LwU32)];

    HDCP22WIRED_SEC_ACTION_MEMSET(ksTmpBuff, 0, sizeof(ksTmpBuff));
    HDCP22WIRED_SEC_ACTION_MEMSET(dKeyTmpBuf, 0, sizeof(dKeyTmpBuf));

    // Retrieve dkey2
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_DKEY,
                                                     (LwU8 *)pdKey2));

    // eKs = ks  XOR (dkey2 XOR Rrx) where Rrx is XORed with the 64 LSB of dkey2
    HDCP22WIRED_SEC_ACTION_MEMCPY(&dKey2LSB, pRrx, HDCP22_SIZE_R_RX);
    dKey2LSB  = dKey2LSB ^ (*(LwU64*)(pdKey2 + HDCP22_SIZE_DKEY - HDCP22_SIZE_R_RX));
    HDCP22WIRED_SEC_ACTION_MEMCPY((void*)(pdKey2 + HDCP22_SIZE_DKEY - HDCP22_SIZE_R_RX),
                                  (void *)&dKey2LSB, HDCP22_SIZE_R_RX);

    // Use tmpBuff to hold necessary keys in 16 byte aligned address
    pTmpKs  = (LwU8 *)ksTmpBuff;
    pTmpEks = pTmpKs + HDCP22_SIZE_KS;
    HDCP22WIRED_SEC_ACTION_MEMCPY(&pTmpKs[0], &pKs[0], HDCP22_SIZE_KS);

    // XOR pdKey2 result with KS  (128 bits)
    *(LwU64*)pTmpEks       = *(LwU64*)pTmpKs ^ *(LwU64*)pdKey2 ;
    *((LwU64*)pTmpEks +1)  = *((LwU64*)pTmpKs+1) ^ *((LwU64*)pdKey2 +1) ;

    HDCP22WIRED_SEC_ACTION_MEMCPY(pEks, &pTmpEks[0], HDCP22_SIZE_EKS);

    // Scrub secret keys from stack
    HDCP22WIRED_SEC_ACTION_MEMSET(ksTmpBuff, 0, LW_SIZEOF32(ksTmpBuff));
    HDCP22WIRED_SEC_ACTION_MEMSET(dKeyTmpBuf, 0, LW_SIZEOF32(dKeyTmpBuf));
    HDCP22WIRED_SEC_ACTION_MEMSET((LwU8 *)km, 0, LW_SIZEOF32(km));
    HDCP22WIRED_SEC_ACTION_MEMSET((LwU8 *)&dKey2LSB, 0, LW_SIZEOF32(dKey2LSB));
label_return:

    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to to geneate SKE stage eKs.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredEksGenHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_EKS_GEN_ARG  pActionArg  = &pArg->action.secActionEksGen;
    SELWRE_ACTION_TYPE          prevState   = SELWRE_ACTION_ILWALID;
    FLCN_STATUS                 status      = FLCN_OK;
    LwU8                        sorNum      = 0;
    LwU8                        linkIndex   = 0;
    LwBool                      bIsMst      = LW_FALSE;
    LwU32                       ks[HDCP22_SIZE_KS/LW_SIZEOF32(LwU32)];
    LwU32                       eKs[HDCP22_SIZE_EKS/LW_SIZEOF32(LwU32)];
    LwU32                       rRiv[HDCP22_SIZE_R_IV/LW_SIZEOF32(LwU32)];

    // Check previous state.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_PREV_STATE,
                                                     (LwU8 *)&prevState));
    if(prevState != SELWRE_ACTION_LPRIME_VALIDATION)
    {
        status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
        goto label_return;
    }

    // Generate Ks here but not storing to secure memory.
    CHECK_STATUS(hdcp22wiredChooseRandNum(ks, HDCP22_RAND_TYPE_KS,
                                          HDCP22_SIZE_KS));

    // Generate Riv but not storing to secure memory.
    CHECK_STATUS(hdcp22wiredChooseRandNum(rRiv, HDCP22_RAND_TYPE_RIV,
                                          HDCP22_SIZE_R_IV));

    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_SOR_NUM,
                                                     &sorNum));
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_LINK_IDX,
                                                     &linkIndex));
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_IS_MST,
                                                     &bIsMst));

    // Write Riv to HW regs.
    CHECK_STATUS(hdcp22wiredWriteRiv_HAL(&Hdcp22wired, sorNum, linkIndex, bIsMst,
                                         rRiv));
    // Output generated rRiv.
    HDCP22WIRED_SEC_ACTION_MEMCPY(pActionArg->rRiv, rRiv, HDCP22_SIZE_R_IV);

    // Generate eKs.
    CHECK_STATUS(_hdcp22GenerateEks(pActionArg->rRx, ks, eKs));

    // Write Ks to HW regs.
    CHECK_STATUS(hdcp22wiredWriteKs_HAL(&Hdcp22wired, sorNum, linkIndex, bIsMst,
                                        ks));

    // Output generated eKs.
    HDCP22WIRED_SEC_ACTION_MEMCPY(pActionArg->eKs, eKs, HDCP22_SIZE_EKS);

    // Update Previous state.
    prevState = SELWRE_ACTION_EKS_GEN;
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_PREV_STATE,
                                                    (LwU8 *)&prevState,
                                                    LW_FALSE));

label_return:

    // Scrub secret from stack.
    HDCP22WIRED_SEC_ACTION_MEMSET(ks, 0, HDCP22_SIZE_KS);
    HDCP22WIRED_SEC_ACTION_MEMSET(rRiv, 0, HDCP22_SIZE_R_IV);

    return status;
}

