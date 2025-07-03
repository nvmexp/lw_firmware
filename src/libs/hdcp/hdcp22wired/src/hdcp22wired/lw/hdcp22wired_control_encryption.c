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
 * @file   hdcp22wired_control_encryption.c
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
static void _hdcp22wiredControlEncryptionEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22ControlEncryption", "start");
#endif

FLCN_STATUS hdcp22wiredControlEncryptionHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22ControlEncryption", "hdcp22wiredControlEncryptionHandler");

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Definitions ---------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredControlEncryptionEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/*!
 * @brief Function to enable/disable HW encryption.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredControlEncryptionHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_CONTROL_ENCRYPTION_ARG pActionArg = &pArg->action.secActionControlEncryption;
    HDCP22_STREAM       streamIdType[HDCP22_NUM_STREAMS_MAX];
    SELWRE_ACTION_TYPE  prevState           = SELWRE_ACTION_ILWALID;
    FLCN_STATUS         status              = FLCN_OK;
    LwU8                maxNoOfSors         = 0;
    LwU8                maxNoOfHeads        = 0;

    if ((pActionArg->controlAction != ENC_ENABLE) &&
        (pActionArg->controlAction != ENC_DISABLE) &&
        (pActionArg->controlAction != HDMI_NONREPEATER_TYPECHANGE))
    {
        status = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto label_return;
    }

    if (pActionArg->controlAction == ENC_ENABLE)
    {
        LwU8            sorNum          = 0;
        LwU8            linkIndex       = 0;
        LwBool          bIsRepeater     = LW_FALSE;
        LwU32           dpTypeMask[HDCP22_NUM_DP_TYPE_MASK];

        // Check previous state.
        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_PREV_STATE,
                                                         (LwU8 *)&prevState));

        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_IS_REPEATER,
                                                         &bIsRepeater));

        if (((bIsRepeater) && (prevState != SELWRE_ACTION_MPRIME_VALIDATION)) ||
            ((!bIsRepeater) && (prevState != SELWRE_ACTION_EKS_GEN)))
        {
            status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
            goto label_return;
        }

        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_SOR_NUM,
                                                         &sorNum));
        if (pActionArg->sorNum != sorNum)
        {
            status = FLCN_ERR_HS_CHK_ILWALID_INPUT;
            goto label_return;
        }

        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_LINK_IDX,
                                                         &linkIndex));
        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_MAX_NO_SORS,
                                                         &maxNoOfSors));
        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_MAX_NO_HEADS,
                                                         &maxNoOfHeads));
        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_STREAM_ID_TYPE,
                                                         (LwU8 *)streamIdType));
        CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                         HDCP22_SECRET_DP_TYPE_MASK,
                                                         (LwU8 *)dpTypeMask));
        // Write HW StreamType reg.
        CHECK_STATUS(hdcp22wiredWriteStreamType_HAL(&Hdcp22wired, sorNum,
                                                    linkIndex, streamIdType,
                                                    dpTypeMask,
                                                    pActionArg->bIsAux));

        // Enable HW Control reg.
        CHECK_STATUS(hdcp22wiredHandleEnableEnc_HAL(&Hdcp22wired, sorNum,
                                                    linkIndex, bIsRepeater,
                                                    pActionArg->bApplyHwDrmType1LockedWar));

        // Update scratch reg streamType value for preVolta.
        CHECK_STATUS(hdcp22wiredWriteStreamTypeInScratch_HAL(&Hdcp22wired, sorNum,
                                                             streamIdType));
        // Update Previous state.
        prevState = SELWRE_ACTION_CONTROL_ENCRYPTION;
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_PREV_STATE,
                                                        (LwU8 *)&prevState,
                                                        LW_FALSE));
    }
    else if (pActionArg->controlAction == ENC_DISABLE)
    {
        // TODO: review to add proper condition check when disabling Encryption.

        // Disable encryption to specific sor won't impact to existing session statemachine.
        CHECK_STATUS(HDCP22WIRED_SEC_GET_MAX_NO_SORS_HEADS(&Hdcp22wired, &maxNoOfSors,
                                                           &maxNoOfHeads));
        if (pActionArg->sorNum >= maxNoOfSors)
        {
            status = FLCN_ERR_HS_CHK_ILWALID_INPUT;
            goto label_return;
        }

        status = hdcp22wiredHandleDisableEnc_HAL(&Hdcp22wired, maxNoOfSors, maxNoOfHeads,
                                                 pActionArg->sorNum,
                                                 pActionArg->bCheckCryptStatus);
    }
    else
    {
        LwBool bType1LockActive = LW_FALSE;

        CHECK_STATUS(HDCP22WIRED_SEC_GET_MAX_NO_SORS_HEADS(&Hdcp22wired, &maxNoOfSors,  NULL));
        CHECK_STATUS(HDCP22WIRED_IS_TYPE1LOCK_ACTIVE(&Hdcp22wired, &bType1LockActive));

        if ((pActionArg->bIsAux != LW_FALSE)                        ||
            (pActionArg->sorNum >= maxNoOfSors)                     ||
            ((pActionArg->linkIndex != HDCP22_LINK_PRIMARY) && 
                (pActionArg->linkIndex != HDCP22_LINK_SECONDARY))   ||
            ((pActionArg->hdmiNonRepeaterStreamType != 0) &&
                (pActionArg->hdmiNonRepeaterStreamType != 1))       ||
            ((bType1LockActive == LW_TRUE) &&
                (pActionArg->hdmiNonRepeaterStreamType == 0)))
        {
            status = FLCN_ERR_HS_CHK_ILWALID_INPUT;
            goto label_return;
        }

        // HDMI non-repeater only has 1 stream id 0.
        HDCP22WIRED_SEC_ACTION_MEMSET(streamIdType, 0, sizeof(streamIdType));
        streamIdType[0].streamType = pActionArg->hdmiNonRepeaterStreamType;

        //
        // If type1 lock is not enforced then reauthenticate HDCP 2.2 with
        // modified type1 values. Reauthentication not needed for HDMI HDCP2.2
        // non-repeater case with type change. As for encryption on HDMI non-repeater
        // type does not matter.
        //

        // Write HW StreamType reg.
        CHECK_STATUS(hdcp22wiredWriteStreamType_HAL(&Hdcp22wired,
                                                    pActionArg->sorNum,
                                                    pActionArg->linkIndex,
                                                    streamIdType,
                                                    NULL,
                                                    LW_FALSE));
        //
        // PreVolta needs to update scratch register then RM get correct
        // type.
        // hdcp22wiredWriteStreamTypeInScratch stub returns FLCN_OK for
        // Volta+.
        //
        CHECK_STATUS(hdcp22wiredWriteStreamTypeInScratch_HAL(&Hdcp22wired,
                                                             pActionArg->sorNum,
                                                             streamIdType));
    }

label_return:
    return status;
}
