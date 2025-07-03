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
 * @file   hdcp22wired_lprime_validation.c
 * @brief  Hdcp22 wrapper Functions for security
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "lib_hdcp22wired.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
#ifdef GSPLITE_RTOS
static void _hdcp22wiredLprimeValidationEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22LprimeValidation", "start");
#endif

static FLCN_STATUS  _hdcp22CallwlateL(LwU64 *pRrx, LwU8 *pL)
    GCC_ATTRIB_SECTION("imem_hdcp22LprimeValidation", "_hdcp22CallwlateL");
FLCN_STATUS hdcp22wiredLprimeValidationHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22LprimeValidation", "hdcp22wiredLprimeValidationHandler");

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Private Functions ------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredLprimeValidationEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/*
 * @brief: This function calulwlates L value
 * @param[in]  pRrx             Pointer to rRx from receiver.
 * @param[out] pL               Pointer to L array.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22CallwlateL
(
    LwU64  *pRrx,
    LwU8   *pL
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU64       hmacoperand = 0;
    LwU64       kd[HDCP22_SIZE_KD/sizeof(LwU64)]; // Align to LwU64 for RRx, Kd 8bytes XOR computation.
    LwU64       rRn[HDCP22_SIZE_R_N/sizeof(LwU64)];

    HDCP22WIRED_SEC_ACTION_MEMSET(kd, 0, HDCP22_SIZE_KD);
    HDCP22WIRED_SEC_ACTION_MEMSET(rRn, 0, HDCP22_SIZE_R_N);

    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_KD,
                                                     (LwU8 *)kd));
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_RN,
                                                     (LwU8 *)rRn));

    // RRX XORed with least 64 bits(8 bytes) of Kd
    HDCP22WIRED_SEC_ACTION_MEMCPY(&hmacoperand, (LwU8 *)pRrx, HDCP22_SIZE_R_RX);
    hmacoperand = hmacoperand ^ (*(LwU64*)((LwU8 *)kd + HDCP22_SIZE_KD - HDCP22_SIZE_R_RX));
    HDCP22WIRED_SEC_ACTION_MEMCPY(((LwU8 *)kd + HDCP22_SIZE_KD - HDCP22_SIZE_R_RX), (void *)&hmacoperand, HDCP22_SIZE_R_RX);

    status = HDCP22WIRED_SEC_ACTION_HMAC_SHA256(pL, (LwU8 *)rRn, HDCP22_SIZE_R_N, (LwU8 *)kd, HDCP22_SIZE_KD);

label_return:
    // Scrub secret from stack.
    HDCP22WIRED_SEC_ACTION_MEMSET(kd, 0, HDCP22_SIZE_KD);

    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to validate Lprime from sink.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredLprimeValidationHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_LPRIME_VALIDATION_ARG pActionArg = &pArg->action.secActionLprimeValidation;
    SELWRE_ACTION_TYPE  prevState   = SELWRE_ACTION_ILWALID;
    FLCN_STATUS         status = FLCN_OK;
    LwU8                bufferL[HDCP22_SIZE_L];

    // Check previous state.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_PREV_STATE,
                                                     (LwU8 *)&prevState));
    if(prevState != SELWRE_ACTION_HPRIME_VALIDATION)
    {
        status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
        goto label_return;
    }

    HDCP22WIRED_SEC_ACTION_MEMSET(bufferL, 0, HDCP22_SIZE_L);

    status = _hdcp22CallwlateL(pActionArg->rRx, bufferL);
    if (status == FLCN_OK)
    {
        if (HDCP22WIRED_SEC_ACTION_MEMCMP(pActionArg->lPrime, bufferL, HDCP22_SIZE_L))
        {
            status = FLCN_ERR_HDCP22_L_PRIME;
        }
        else
        {
            // Update Previous state.
            prevState = SELWRE_ACTION_LPRIME_VALIDATION;
            CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                            HDCP22_SECRET_PREV_STATE,
                                                            (LwU8 *)&prevState,
                                                            LW_FALSE));
        }
    }

label_return:

    // Scrub signatures from stack
    HDCP22WIRED_SEC_ACTION_MEMSET(bufferL, 0, HDCP22_SIZE_L);

    return status;
}

