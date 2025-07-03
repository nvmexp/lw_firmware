/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_wkrthd.c
 * @brief  Worker thread entry function that can execute same code on multiple
 *         stacks
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"

/* ------------------------- Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "scp_rand.h"
#include "sec2_objsec2.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
//TODO-ssapre: This should go to a header in the next changelist.
extern FLCN_STATUS hs_switch_entry(LwU32 val, LwU8 regType, LwU32 *pRetValue);
extern FLCN_STATUS fakeidle_test_entry(LwU8 op);
extern FLCN_STATUS rttimer_test_entry(LwU32 count, LwBool bCheckTime,
                                      LwU32 *pOneShotNs, LwU32 *pContinuousNs);

/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used to dispatch work-items to all worker
 * threads. It is assumed that this queue is setup/created before this task is
 * scheduled to run.
 */
LwrtosQueueHandle Disp2QWkrThd;

/* ------------------------- Static Function Prototypes  -------------------- */
static void _handleTest(LwU8 testType, LwU8 cmdQueueId, RM_FLCN_CMD_SEC2 *pCmd)
    GCC_ATTRIB_SECTION("imem_wkrthd", "_handleTest");

/*!
 * Common entry point for all worker threads
 */
lwrtosTASK_FUNCTION(task_wkrthd, pvParameters)
{
    DISP2UNIT_CMD   disp2wkrThd;

    for (;;)
    {
        if (OS_QUEUE_WAIT_FOREVER(Disp2QWkrThd, &disp2wkrThd))
        {
            RM_FLCN_CMD_SEC2 *pCmd = disp2wkrThd.pCmd;
            switch(pCmd->hdr.unitId)
            {
                case RM_SEC2_UNIT_TEST:
                {
                    if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_HS_SWITCH) ||
                        SEC2CFG_FEATURE_ENABLED(SEC2JOB_RTTIMER_TEST) ||
                        SEC2CFG_FEATURE_ENABLED(SEC2JOB_FAKEIDLE_TEST))
                    {
                        _handleTest(pCmd->cmd.sec2Test.cmdType,
                                    disp2wkrThd.cmdQueueId, pCmd);
                    }
                    break;
                }

                default:
                {
                    //
                    // Do nothing. Don't halt since this is a security risk. In
                    // the future, we could return an error code to RM.
                    //
                }
            }
        }
    }
}

static void _handleTest
(
    LwU8 testType,
    LwU8 cmdQueueId,
    RM_FLCN_CMD_SEC2 *pCmd
)
{
    RM_FLCN_QUEUE_HDR hdr;
    RM_SEC2_TEST_MSG  msg;

    // Common stuff we can initialize in the message
    hdr.unitId    = RM_SEC2_UNIT_TEST;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = pCmd->hdr.seqNumId;
    // hdr.size must be set for each test.

    msg.msgType = testType;

    switch (testType)
    {
        case RM_SEC2_TEST_CMD_ID_WR_PRIV_PROTECTED_REG:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_HS_SWITCH))
            {
                LwU32 val     = pCmd->cmd.sec2Test.wrPrivProtectedReg.val;
                LwU8  regType = pCmd->cmd.sec2Test.wrPrivProtectedReg.regType;

                LwU32 retVal = 0;
                FLCN_STATUS status = FLCN_ERROR;

                if (Sec2.bDebugMode)
                {
                    // We only support this command in debug mode
                    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(testhsswitch));
                    status = hs_switch_entry(val, regType, &retVal);
                    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(testhsswitch));
                }

                hdr.size      = RM_SEC2_MSG_SIZE(TEST, WR_PRIV_PROTECTED_REG);
                msg.wrPrivProtectedReg.val     = retVal;
                msg.wrPrivProtectedReg.regType = regType;
                msg.wrPrivProtectedReg.status  = (status == FLCN_OK) ?
                                                 RM_SEC2_TEST_MSG_STATUS_OK :
                                                 RM_SEC2_TEST_MSG_STATUS_FAIL;
            }
            break;
        }

        case RM_SEC2_TEST_CMD_ID_FAKEIDLE_TEST:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_FAKEIDLE_TEST))
            {
                FLCN_STATUS retVal = FLCN_ERROR;
                LwU8 op            = pCmd->cmd.sec2Test.fakeidle.op;

                if (Sec2.bDebugMode)
                {
                    // We only support this command in debug mode
                    OSTASK_ATTACH_IMEM_OVERLAY(OVL_INDEX_IMEM(testfakeidle));
                    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(chnmgmt));
                    retVal = fakeidle_test_entry(op);
                    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(chnmgmt));
                    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(testfakeidle));
                }

                hdr.size = RM_SEC2_MSG_SIZE(TEST, FAKEIDLE_TEST);
                msg.fakeidle.status = (retVal == FLCN_OK) ?
                                     RM_SEC2_TEST_MSG_STATUS_OK :
                                     RM_SEC2_TEST_MSG_STATUS_FAIL;
            }
            break;
        }

        case RM_SEC2_TEST_CMD_ID_RTTIMER_TEST:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_RTTIMER_TEST))
            {
                FLCN_STATUS retVal = FLCN_ERROR;
                LwBool bCheckTime;
                LwU32 rttimerCount;
                LwU32 oneShotNs = 0;
                LwU32 continuousNs = 0;

                rttimerCount = pCmd->cmd.sec2Test.rttimer.count;
                bCheckTime = pCmd->cmd.sec2Test.rttimer.bCheckTime;

                if (Sec2.bDebugMode)
                {
                    // We only support this command in debug mode
                    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(testrttimer));
                    retVal = rttimer_test_entry(rttimerCount, bCheckTime,
                                                &oneShotNs, &continuousNs);

                    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(testrttimer));
                }

                hdr.size = RM_SEC2_MSG_SIZE(TEST, RTTIMER_TEST);
                msg.rttimer.status  = (retVal == FLCN_OK) ?
                                        RM_SEC2_TEST_MSG_STATUS_OK :
                                        RM_SEC2_TEST_MSG_STATUS_FAIL;
                msg.rttimer.oneShotNs = oneShotNs;
                msg.rttimer.continuousNs = continuousNs;
            }
            break;
        }

        case RM_SEC2_TEST_CMD_ID_RD_BLACKLISTED_REG:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_TEST))
            {
                FLCN_STATUS status = FLCN_ERROR;
                LwU32 regVal       = 0;

                if (Sec2.bDebugMode)
                {
                    status = sec2WaitMscgEngaged_HAL();
                    //do not bother with MSCG operation if we can't enter MSCG
                    if (status == FLCN_OK)
                    {
                        status = sec2MscgDenylistedRegRead_HAL(&Sec2, &regVal);
                    }
                }
                hdr.size                    = RM_SEC2_MSG_SIZE(TEST, RD_BLACKLISTED_REG);
                msg.rdBlacklistedReg.val    = regVal;
                msg.rdBlacklistedReg.status = (status == FLCN_OK) ?
                                              RM_SEC2_TEST_MSG_STATUS_OK :
                                              RM_SEC2_TEST_MSG_STATUS_FAIL;
            }
            break;
        }

        case RM_SEC2_TEST_CMD_ID_MSCG_ISSUE_FB_ACCESS:
        {
            if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_TEST) && SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_FBBLOCKER_TEST))
            {
                FLCN_STATUS status = FLCN_ERROR;

                if (Sec2.bDebugMode)
                {
                    status = sec2WaitMscgEngaged_HAL();
                    if (status == FLCN_OK)
                    {
                        status = sec2MscgIssueFbAccess_HAL(&Sec2,
                                                           pCmd->cmd.sec2Test.mscgFbAccess.fbOffsetLo32,
                                                           pCmd->cmd.sec2Test.mscgFbAccess.fbOffsetHi32,
                                                           pCmd->cmd.sec2Test.mscgFbAccess.op);
                    }
                }
                hdr.size                = RM_SEC2_MSG_SIZE(TEST, MSCG_ISSUE_FB_ACCESS);
                msg.mscgFbAccess.status = (status == FLCN_OK) ?
                                          RM_SEC2_TEST_MSG_STATUS_OK :
                                          RM_SEC2_TEST_MSG_STATUS_FAIL;
            }
            break;
        }

    }
    osCmdmgmtCmdQSweep(&pCmd->hdr, cmdQueueId);
    osCmdmgmtRmQueuePostBlocking(&hdr, &msg);
}
