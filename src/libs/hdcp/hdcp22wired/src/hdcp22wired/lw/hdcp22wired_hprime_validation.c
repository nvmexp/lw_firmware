/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_hprime_validation.c
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
static void _hdcp22wiredHprimeValidationEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22HprimeValidation", "start");
#endif

static FLCN_STATUS  _hdcp22GenerateH(LwU8 *pRxCaps, LwU8 *pKd, LwU8 *pH)
    GCC_ATTRIB_SECTION("imem_hdcp22HprimeValidation", "_hdcp22GenerateH");
FLCN_STATUS hdcp22wiredHprimeValidationHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22HprimeValidation", "hdcp22wiredHprimeValidationHandler");

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Private Functions ------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredHprimeValidationEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/*
 * @brief: This function computes H value
 * @param[in]  pRxCaps          Pointer to rxCaps from receiver.
 * @param[in]  pKd              Pointer to Kd.
 * @param[in]  pH               Pointer to H array.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
_hdcp22GenerateH
(
    LwU8   *pRxCaps,
    LwU8   *pKd,
    LwU8   *pH
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        msg[HDCP22_SIZE_R_TX + 6];
    LwU32       rTx[HDCP22_SIZE_R_TX/LW_SIZEOF32(LwU32)];
    LwU8        txCaps[HDCP22_SIZE_TX_CAPS] = HDCP22_TX_CAPS_PATTERN_BIG_ENDIAN;

    // Retrieve rTx.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_RTX,
                                                     (LwU8 *)rTx));

    HDCP22WIRED_SEC_ACTION_MEMSET(msg, 0, sizeof(msg));
    HDCP22WIRED_SEC_ACTION_MEMCPY(msg, (LwU8 *)rTx, HDCP22_SIZE_R_TX);

    HDCP22WIRED_SEC_ACTION_MEMCPY(&msg[HDCP22_SIZE_R_TX], pRxCaps, HDCP22_SIZE_RX_CAPS);
    HDCP22WIRED_SEC_ACTION_MEMCPY(&msg[HDCP22_SIZE_R_TX + HDCP22_SIZE_RX_CAPS], txCaps,
                 HDCP22_SIZE_TX_CAPS);

    // Load/Unload overlay will only be called for prevolta (LS version)
    hdcpAttachAndLoadOverlay(HDCP22WIRED_LIB_SHA);
    status = HDCP22WIRED_SEC_ACTION_HMAC_SHA256(pH, msg, sizeof(msg), (LwU8 *)pKd, HDCP22_SIZE_KD);
    hdcpDetachOverlay(HDCP22WIRED_LIB_SHA);

label_return:
    return status;
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to validate hprime.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredHprimeValidationHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_HPRIME_VALIDATION_ARG    pActionArg  = &pArg->action.secActionHprimeValidation;
    SELWRE_ACTION_TYPE                      prevState   = SELWRE_ACTION_ILWALID;
    FLCN_STATUS status = FLCN_OK;
    // HWSha needs to be 4bytes-aligned, HWSHDCP22_SIZE_X is 4X and no residues after division.
    LwU32       h[HDCP22_SIZE_H/sizeof(LwU32)];
    LwU32       kd[HDCP22_SIZE_KD/sizeof(LwU32)];

    // Check if the previous state.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_PREV_STATE,
                                                     (LwU8 *)&prevState));
    if(prevState != SELWRE_ACTION_KMKD_GEN)
    {
        status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
        goto label_return;
    }

    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_KD,
                                                     (LwU8 *)kd));

    CHECK_STATUS(_hdcp22GenerateH(pActionArg->rxCaps, (LwU8 *)kd, (LwU8*)h));
    if (HDCP22WIRED_SEC_ACTION_MEMCMP(pActionArg->hPrime, h, HDCP22_SIZE_H))
    {
        status = FLCN_ERR_HDCP22_H_PRIME;
        goto label_return;
    }

    //
    // TODO: bRepeater flag should be saved at certificateValidation action.
    // Move that when it's implemented.
    //
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_IS_REPEATER,
                                                    &pActionArg->bRepeater,
                                                    LW_FALSE));

    // Update Previous state.
    prevState = SELWRE_ACTION_HPRIME_VALIDATION;
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_PREV_STATE,
                                                    (LwU8 *)&prevState,
                                                    LW_FALSE));

label_return:

    return status;
}

