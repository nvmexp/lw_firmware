/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file task4_dpaux.c
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"
#include "dev_pmgr.h"
#include "ctrl/ctrl0073.h"

/* ------------------------- Application Includes -------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "flcntypes.h"
#include "pmu_dpaux.h"
#include "unit_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "dbgprintf.h"
#include "displayport/displayport.h"
#include "pmu_i2capi.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Static variables ------------------------------ */
/* ------------------------- Task declaration ------------------------------ */
/* ------------------------- Static functions ------------------------------ */

static FLCN_STATUS dpAuxAccess(DPAUX_CMD *pCmd, LwBool bIsReadAccess);
static FLCN_STATUS dpAuxAccessHelper(DPAUX_CMD *pCmd, LwBool bIsReadAccess);
static FLCN_STATUS dpAuxChSetOwnership(LwU8 port, LwU32 newOwner);
static FLCN_STATUS dpAuxChSetOwnershipHelper(LwU8 port, LwU32 newOwner);

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   This is the command handler for DP AUX/PMU transactions.
 *
 * @param[in] pDpAuxCmd
 *          Pointer to a DPAUX_CMD.
 */
void
dpauxCommand
(
    DPAUX_CMD *pDpAuxCmd
)
{
    LwrtosQueueHandle pRetQueue = NULL;
    FLCN_STATUS       status=0;
    I2C_INT_MESSAGE   msg;

    pRetQueue = pDpAuxCmd->hQueue;
    if (pRetQueue == NULL)
    {
        PMU_HALT();
        return; // Coverity
    }

    switch (pDpAuxCmd->cmd)
    {
        case LW_PMGR_DP_AUXCTL_CMD_I2CWR:
        case LW_PMGR_DP_AUXCTL_CMD_MOTWR:
        case LW_PMGR_DP_AUXCTL_CMD_AUXWR:
            status = dpAuxAccess(pDpAuxCmd, LW_FALSE);
            break;

        case LW_PMGR_DP_AUXCTL_CMD_I2CRD:
        case LW_PMGR_DP_AUXCTL_CMD_MOTRD:
        case LW_PMGR_DP_AUXCTL_CMD_AUXRD:
            status = dpAuxAccess(pDpAuxCmd, LW_TRUE);
            break;

        default:
            PMU_HALT();
    }

    msg.errorCode = status;

    lwrtosQueueSendBlocking(pRetQueue, &msg, sizeof(msg));
}

/*!
 * @brief   Helper interface to detect whether the display is connected by
 *          reading the AUX hot plug detect status.
 *
 * @return  True    If any of the AUX's HDP status showcases that display is plugged.
 * @return  False   Otherwise
 */
LwBool
dpauxIsHpdStatusPlugged(void)
{
    LwU8    i;
    LwU32   auxStat            = 0;
    LwBool  isHpdStatusPlugged = LW_FALSE;

    // Iterate over all DP AUX
    for (i = 0; i < LW_PMGR_DP_AUXCTL__SIZE_1; i++)
    {
        // Read DP AUX status
        auxStat = REG_RD32(BAR0, LW_PMGR_DP_AUXSTAT(i));

        if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _HPD_STATUS, _PLUGGED, auxStat))
        {
            isHpdStatusPlugged = LW_TRUE;
            break;
        }
    }

    return isHpdStatusPlugged;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   Sets mutex ownership for PMU.  This mutex is shared between
 *          RM, VBIOS, and PMU.
 *
 * @param[in] port
 *          Port index to reserve ownership for.
 *
 * @param[in] newOwner
 *          Type of owner.
 *
 * @return FLCN_OK
 *          The operation completed successfully.
 *
 * @return FLCN_ERR_MUTEX_ACQUIRED
 *          PMU failed to acquire the AUX channel mutex.
 */
static FLCN_STATUS
dpAuxChSetOwnershipHelper
(
    LwU8  port,
    LwU32 newOwner
)
{
    LwU32    data;

    data = REG_RD32(BAR0, LW_PMGR_DP_AUXCTL(port));
    data = FLD_SET_DRF_NUM(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, newOwner, data);
    REG_WR32(BAR0,  LW_PMGR_DP_AUXCTL(port), data);

    data = REG_RD32(BAR0, LW_PMGR_DP_AUXCTL(port));
    if (newOwner != DRF_VAL(_PMGR, _DP_AUXCTL, _SEMA_GRANT, data))
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    return FLCN_OK;
}

/*!
 * @brief   Wrapper fn of DPAUX sema set routine.
 *
 * @param[in] port
 *          Port index to reserve ownership for.
 *
 * @param[in] newOwner
 *          Type of owner.
 *
 * @return FLCN_OK
 *          The operation completed successfully.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *          Port is not valid
 *
 * @return FLCN_ERR_MUTEX_ACQUIRED
 *          PMU failed to acquire the AUX channel mutex.
 */
static FLCN_STATUS
dpAuxChSetOwnership
(
    LwU8  port,
    LwU32 newOwner
)
{
    LwU32       i      = 0;
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Port sanity check
        if (port >= LW_PMGR_DP_AUXCTL__SIZE_1)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto dpAuxChSetOwnership_exit;
        }
    }

    while ((status = dpAuxChSetOwnershipHelper(port, newOwner)) != FLCN_OK)
    {
       if (i++ > DP_AUX_SEMA_ACQUIRE_ATTEMPTS)
       {
           goto dpAuxChSetOwnership_exit;
       }

        // Wait before the next attempt.
       OS_PTIMER_SPIN_WAIT_US(DP_AUX_DELAY_US);
    }

dpAuxChSetOwnership_exit:
    return status;
}

/*!
 * @brief   Subroutine to perform the actual DP Aux Read/Write access.
 *
 * @param[in] pCmd
 *          This DPAUX_CMD has the following parameters for DP Aux access:
 *          port, addr, size, pData.  pData is the pointer to an array that
 *          will store the data read from the DP AUX interface.
 *
 * @param[in] bIsReadAccess
 *          This should be set to LW_TRUE if the current access is Read else
 *          this should be set to LW_FALSE.
 *
 * @return FLCN_OK
 *    If the transaction is successful.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *          If input data like port/size/etc are not valid.
 *
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *          If the HPD status of DP is not connected.
 *          If the panel's DPAUX client runs into an erroneous state.
 *
 * @return FLCN_ERR_TIMEOUT
 *          If the transcation times out after a fixed timeout.
 *
 * @return FLCN_ERROR
 *          If the transaction resutls in an unknown failure.
 *
 * @pre     It is expected that the caller of this function should have
 *          already set the semaphore to obtain DP AUX ch control.
 */
static FLCN_STATUS
dpAuxAccessHelper
(
    DPAUX_CMD *pCmd,
    LwBool    bIsReadAccess
)
{
    FLCN_STATUS  status   = FLCN_OK;
    LwU32        auxCtl;
    LwU32        auxStat  = 0;
    LwU32        auxData[DP_AUX_CHANNEL_MAX_BYTES / sizeof(LwU32)];
    LwU32        timeOutRetries  = DP_AUX_CHANNEL_TIMEOUT_MAX_TRIES;
    LwU32        deferRetries    = DP_AUX_CHANNEL_DEFAULT_DEFER_MAX_TRIES;
    LwU8         cmd      = pCmd->cmd;
    LwU32        i;
    LwBool       bIsDone   = LW_FALSE;
    LwU8         port;
    LwU32        addr;
    LwU8         size;
    LwU8        *pData;

    port     = pCmd->port;
    addr     = pCmd->addr;
    size    =  pCmd->size;
    pData    = (LwU8*)pCmd->pData;

    if ((port >= LW_PMGR_DP_AUXCTL__SIZE_1) ||
        (size > DP_AUX_CHANNEL_MAX_BYTES))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    auxStat = REG_RD32(BAR0, LW_PMGR_DP_AUXSTAT(port));
    if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _HPD_STATUS, _UNPLUG, auxStat))
    {
        // Report Error
        return FLCN_ERR_ILLEGAL_OPERATION;
    }

    // Write the data out for write access.
    if (!bIsReadAccess)
    {
        memset(auxData, 0, sizeof(auxData));
        memcpy(auxData, pData, size);

        // Set the dpAuxData
        REG_WR32(BAR0, LW_PMGR_DP_AUXDATA_WRITE_W0(port), auxData[0]);
        REG_WR32(BAR0, LW_PMGR_DP_AUXDATA_WRITE_W1(port), auxData[1]);
        REG_WR32(BAR0, LW_PMGR_DP_AUXDATA_WRITE_W2(port), auxData[2]);
        REG_WR32(BAR0, LW_PMGR_DP_AUXDATA_WRITE_W3(port), auxData[3]);
    }

    // Set the dpAuxCh addr
    REG_WR32(BAR0, LW_PMGR_DP_AUXADDR(port), addr);
    auxCtl = REG_RD32(BAR0, LW_PMGR_DP_AUXCTL(port));
    auxCtl = FLD_SET_DRF_NUM(_PMGR, _DP_AUXCTL, _CMD, cmd, auxCtl);
    //
    // CMDLEN field should be set to N-1, where N is the number of bytes
    // in the transaction (see dev_pmgr.ref)
    //
    auxCtl = FLD_SET_DRF_NUM(_PMGR, _DP_AUXCTL, _CMDLEN, size - 1, auxCtl);

    while (!bIsDone)
    {
        // Reset DPAUX
        auxCtl = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _RESET, _ASSERT, auxCtl);
        REG_WR32(BAR0, LW_PMGR_DP_AUXCTL(port), auxCtl);

        auxCtl = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _RESET, _DEASSERT, auxCtl);
        REG_WR32(BAR0, LW_PMGR_DP_AUXCTL(port), auxCtl);

        if ((deferRetries != DP_AUX_CHANNEL_DEFAULT_DEFER_MAX_TRIES) ||
            (timeOutRetries != DP_AUX_CHANNEL_TIMEOUT_MAX_TRIES))
        {
            //
            // We need to wait before initiating another transaction in case
            // we got a defer or timeout back from the panel.
            //
            // Note that deferRetries and timeOutRetries are initialized to
            // max values, so the above condition will be false on the first
            // pass through the loop
            //
            OS_PTIMER_SPIN_WAIT_US(DP_AUX_REPLY_TIMEOUT_US);
        }

        // this initiates the transaction.
        auxCtl = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _TRANSACTREQ, _PENDING, auxCtl);
        REG_WR32(BAR0, LW_PMGR_DP_AUXCTL(port), auxCtl);

        i = 0;
        while (FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _TRANSACTREQ, _PENDING,
                            REG_RD32(BAR0, LW_PMGR_DP_AUXCTL(port))))
        {
            OS_PTIMER_SPIN_WAIT_US(DP_AUX_DELAY_US);

            if (i++ > DP_AUX_NUM_ATTEMPTS)
            {
                DBG_PRINTF_DPAUX(("addr: 0x%x AUXCTL timeout\n",
                                  addr, 0, 0, 0));
                //
                // We ignore the timeout error and continue as in the normal
                // case. This is intentional (RM DPAUX code does the same).
                //
                break;
            }
        }

        auxStat = REG_RD32(BAR0, LW_PMGR_DP_AUXSTAT(port));

        if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _TIMEOUT, _PENDING, auxStat) ||
            FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _RX_ERROR, _PENDING, auxStat) ||
            FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _SINKSTAT_ERROR, _PENDING, auxStat) ||
            FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _NO_STOP_ERROR, _PENDING, auxStat))
        {
            if (timeOutRetries > 0)
            {
                REG_WR32(BAR0, LW_PMGR_DP_AUXSTAT(port), auxStat);
                --timeOutRetries;
            }
            else
            {
                DBG_PRINTF_DPAUX(("addr: 0x%x AUXSTAT timeout 0x%x timeOutRetries=%d\n",
                    addr, auxStat, timeOutRetries, 0));
                bIsDone = LW_TRUE;
                status = FLCN_ERR_TIMEOUT;
            }
            continue;
        }

        if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _REPLYTYPE, _I2CDEFER, auxStat) ||
            FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _REPLYTYPE, _DEFER, auxStat))
        {
            if (deferRetries > 0)
            {
                --deferRetries;
            }
            else
            {
                DBG_PRINTF_DPAUX(("addr: 0x%x AUXSTAT 0x%x deferRetries=%d\n",
                    addr, auxStat, deferRetries, 0));
                bIsDone = LW_TRUE;
                status = FLCN_ERR_TIMEOUT;
            }
            continue;
        }

        bIsDone = LW_TRUE;
        if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _REPLYTYPE, _ACK, auxStat))
        {
            if (bIsReadAccess)
            {
                // Truncate the packet the size requested
                if (DRF_VAL(_PMGR, _DP_AUXSTAT, _REPLY_M, auxStat) < size)
                {
                    pCmd->size = (LwU8)DRF_VAL(_PMGR, _DP_AUXSTAT, _REPLY_M, auxStat);
                }

                // Set the dpAuxData
                auxData[0] = REG_RD32(BAR0, LW_PMGR_DP_AUXDATA_READ_W0(port));
                auxData[1] = REG_RD32(BAR0, LW_PMGR_DP_AUXDATA_READ_W1(port));
                auxData[2] = REG_RD32(BAR0, LW_PMGR_DP_AUXDATA_READ_W2(port));
                auxData[3] = REG_RD32(BAR0, LW_PMGR_DP_AUXDATA_READ_W3(port));

                memcpy(pData, auxData, pCmd->size);
            }
            else
            {
                pCmd->size = (LwU8)DRF_VAL(_PMGR, _DP_AUXSTAT, _REPLY_M, auxStat);
            }
            status = FLCN_OK;
        }
        else
        {
            status = FLCN_ERROR;
        }
    }

    return status;
}

/*!
 * @brief   Interface to perform DP Aux Read/Write.
 *
 * @param[in] pCmd
 *          This DPAUX_CMD has the following parameters for DP Aux access:
 *          port, addr, size, pData.  pData is the pointer to an array that
 *          will store the data read from the DP AUX interface.
 *
 * @param[in] bIsReadAccess
 *          This should be set to LW_TRUE if the current access is Read else
 *          this should be set to LW_FALSE.
 *
 * @return FLCN_OK
 *          If the transaction is successful.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *          If input data like port/size/etc are not valid.
 *
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *          If the HPD status of DP is not connected.
 *          If the panel's DPAUX client runs into an erroneous state.
 *
 * @return FLCN_ERR_TIMEOUT
 *          If the transcation times out after a fixed timeout.
 *
 * @return FLCN_ERR_MUTEX_ACQUIRED
 *          PMU failed to acquire the AUX channel mutex.
 *
 * @return FLCN_ERROR
 *          If the transaction resutls in an unknown failure.
 */
static FLCN_STATUS
dpAuxAccess
(
    DPAUX_CMD *pCmd,
    LwBool    bIsReadAccess
)
{
    FLCN_STATUS status;
    FLCN_STATUS semaReleaseStatus;
    LwU8       port = pCmd->port;

    if ((status = dpAuxChSetOwnership(port,
        DRF_DEF(0073_CTRL_DP, _AUXCH_SET_SEMA, _OWNER, _PMU))) != FLCN_OK)
    {
        DBG_PRINTF_DPAUX(("Failed to acquire DPAUX sema\n", 0, 0, 0, 0));
        return status;
    }

    status = dpAuxAccessHelper(pCmd, bIsReadAccess);

    if ((semaReleaseStatus = dpAuxChSetOwnership(port,
        DRF_DEF(0073_CTRL_DP, _AUXCH_SET_SEMA, _OWNER, _RELEASE))) != FLCN_OK)
    {
        DBG_PRINTF_DPAUX(("Failed to release DPAUX sema\n", 0, 0, 0, 0));
    }

    // in case of multiple errors return first error encountered
    return (status != FLCN_OK) ? status : semaReleaseStatus;
}
