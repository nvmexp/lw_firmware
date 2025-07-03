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
 * @file   hdcp22wired_start_session.c
 * @brief  Hdcp22 StartSession secure action handler.
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "flcnifcmn.h"
#include "hdcp22wired_cmn.h"
#include "mem_hs.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
FLCN_STATUS hdcp22wiredStartSessionHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22StartSession", "hdcp22wiredStartSessionHandler");
#ifdef GSPLITE_RTOS
static void _hdcp22wiredStartSessionEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22StartSession", "start");

FLCN_STATUS hdcp22wiredSelwreRegAccessHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredSelwreRegAccessHandler");
#endif // GSPLITE_RTOS

/* ------------------------ Global variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredStartSessionEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to start authentication session.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredStartSessionHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_START_SESSION_ARG pActionArg = &pArg->action.secActionStartSession;
    SELWRE_ACTION_TYPE  prevState = SELWRE_ACTION_ILWALID;  // preVolta compiler cannot use data32 for strict aliasing rule.
    FLCN_STATUS         status  = FLCN_OK;
    LwU32               data32[HDCP22_SIZE_RN_MAX_U32];
    LwU8                bIsMst;
    LwU8                maxNoOfSors = 0;
    LwU8                maxNoOfHeads = 0;
    LwBool              bEncryptionActive = LW_FALSE;
    LwU32               index;
    LwBool              bType1LockActive = LW_FALSE;

    // Check if the previous Session is ended.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_PREV_STATE,
                                                     (LwU8 *)&prevState));
    if(prevState != SELWRE_ACTION_END_SESSION)
    {
        status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
        goto label_return;
    }

#ifdef HDCP22_KMEM_ENABLED
    //
    // TODO: remove the check after adding Initialize and removing regAccess
    // selwreActions.
    //
    CHECK_STATUS(hdcp22wiredKmemCheckProtinfoHs_HAL(&Hdcp22wired));
#endif

    CHECK_STATUS(HDCP22WIRED_SEC_GET_MAX_NO_SORS_HEADS(&Hdcp22wired, &maxNoOfSors,
                                                       &maxNoOfHeads));
    if ((pActionArg->sorNum >= maxNoOfSors) ||
        ((pActionArg->linkIndex != HDCP22_LINK_PRIMARY) &&
            (pActionArg->linkIndex != HDCP22_LINK_SECONDARY)) ||
         (pActionArg->numStreams > HDCP22_NUM_STREAMS_MAX))
    {
        status = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto label_return;
    }

    // Check if type1Lock case and check input type.
    CHECK_STATUS(HDCP22WIRED_IS_TYPE1LOCK_ACTIVE(&Hdcp22wired, &bType1LockActive));
#ifndef HDCP22_FORCE_TYPE1_ONLY
    // Only type1 is allowed in CheetAh
    if (bType1LockActive)
#endif // !HDCP22_FORCE_TYPE1_ONLY
    {
        for (index=0; index<HDCP22_NUM_STREAMS_MAX; index++)
        {
            if (pActionArg->streamIdType[index].streamType != 1)
            {
                status = FLCN_ERR_HS_HDCP22_WRONG_TYPE;
                goto label_return;
            }
        }

        for (index=0; index<HDCP22_NUM_DP_TYPE_MASK; index++)
        {
            if (pActionArg->dpTypeMask[index] != 0xFFFFFFFF)
            {
                status = FLCN_ERR_HS_HDCP22_WRONG_TYPE;
                goto label_return;
            }
        }
    }

    CHECK_STATUS(HDCP22WIRED_ENCRYPTION_ACTIVE(&Hdcp22wired, pActionArg->sorNum, &bEncryptionActive));
    if (bEncryptionActive && !pActionArg->bApplyHwDrmType1LockedWar)
    {
        //
        // If already active and not HWDRMTYPE1LOCKED WAR case, simply need
        // decrypted kd for repater authentication stage actions.
        //

        // Retrieve session random number and encKd for later Kd decryption.
        CHECK_STATUS(hdcp22wiredRetrieveSessionSecrets_HAL(&Hdcp22wired, pActionArg->sorNum));

        // Reset internal hash without check.
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_PREV_STATE,
                                                        (LwU8 *)NULL,
                                                        LW_TRUE));

        // Restore streamIdType which is retrieved from LS and also type1Lock checked.
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_STREAM_ID_TYPE,
                                                        (LwU8 *)pActionArg->streamIdType,
                                                        LW_FALSE));

        // Update Previous state as REPEATER Start Session which means SOR encryption state already checked.
        prevState = SELWRE_ACTION_REPEATER_START_SESSION;
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_PREV_STATE,
                                                        (LwU8 *)&prevState,
                                                        LW_FALSE));
    }
    else
    {
        // Store the required data in secure memory with integrity required flag.
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_SOR_NUM,
                                                        (LwU8 *)&pActionArg->sorNum,
                                                        LW_FALSE));

        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_LINK_IDX,
                                                        (LwU8 *)&pActionArg->linkIndex,
                                                        LW_FALSE));

        // HDMI, SST's streamId must be 0, while MST must not be 0.
        bIsMst = ((pActionArg->streamIdType[0].streamId !=0) ? LW_TRUE : LW_FALSE);
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_IS_MST,
                                                        &bIsMst,
                                                        LW_FALSE));

        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_NUM_STREAMS,
                                                        (LwU8 *)&pActionArg->numStreams,
                                                        LW_FALSE));
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_STREAM_ID_TYPE,
                                                        (LwU8 *)pActionArg->streamIdType,
                                                        LW_FALSE));
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_DP_TYPE_MASK,
                                                        (LwU8 *)pActionArg->dpTypeMask,
                                                        LW_FALSE));

        // Generate Rtx and store in secure memory with integrity required flag.
        CHECK_STATUS(hdcp22wiredChooseRandNum(data32, HDCP22_RAND_TYPE_RTX,
                                              HDCP22_SIZE_R_TX));
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_RTX,
                                                        (LwU8 *)data32,
                                                        LW_FALSE));
        HDCP22WIRED_SEC_ACTION_MEMCPY(pActionArg->rTx, data32, HDCP22_SIZE_R_TX);

        // Generate Rn and store in secure memory with integrity required flag.
        CHECK_STATUS(hdcp22wiredChooseRandNum(data32, HDCP22_RAND_TYPE_RN,
                                              HDCP22_SIZE_R_N));
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_RN,
                                                        (LwU8 *)data32,
                                                        LW_FALSE));
        HDCP22WIRED_SEC_ACTION_MEMCPY(pActionArg->rRn, data32, HDCP22_SIZE_R_N);

        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_MAX_NO_SORS,
                                                        (LwU8 *)&maxNoOfSors,
                                                        LW_FALSE));

        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_MAX_NO_HEADS,
                                                        (LwU8 *)&maxNoOfHeads,
                                                        LW_FALSE));

#ifndef HDCP22_KMEM_ENABLED
        // Generate for session confidential secrets.
        CHECK_STATUS(hdcp22wiredChooseRandNum(data32, HDCP22_RAND_TYPE_SESSION,
                                              HDCP22_SIZE_SESSION_CRYPT));
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_RSESSION,
                                                        (LwU8 *)data32,
                                                        LW_FALSE));
#endif

        // Update Previous state as Start Session.
        prevState = SELWRE_ACTION_START_SESSION;
        CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                        HDCP22_SECRET_PREV_STATE,
                                                        (LwU8 *)&prevState,
                                                        LW_FALSE));
    }

label_return:
    return status;
}

#ifdef GSPLITE_RTOS
/*!
 * @brief Function to access register.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredSelwreRegAccessHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    FLCN_STATUS status = FLCN_OK;
    PSELWRE_ACTION_REG_ACCESS_ARG pRegAccessArg = &pArg->action.regAccessArg;
    LwU32                         addr          = pRegAccessArg->addr;

    //
    // Secure Bus access (read or write)
    // TODO: add Init selwreAction that move hdcp22wiredInitSorHdcp22CtrlReg()
    // initialization call and remove selwreRegAccess action completely.
    //
    if (pRegAccessArg->bIsReadReq)
    {
        status = hdcp22wiredReadRegisterHs(addr, &pRegAccessArg->retVal);
    }
    else
    {
        status = hdcp22wiredWriteRegisterHs(addr, pRegAccessArg->val);
    }

    return status;
}
#endif // GSPLITE_RTOS
