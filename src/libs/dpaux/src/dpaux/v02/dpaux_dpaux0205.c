/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file dpaux_dpaux0205.c
 */

/* ------------------------- System Includes ------------------------------- */
#include "flcncmn.h"
#include "dpaux_cmn.h"

/* ------------------------- Application Includes -------------------------- */
#include "dev_pmgr.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Static variables ------------------------------ */
/* ------------------------- Task declaration ------------------------------ */
/* ------------------------- Static functions ------------------------------ */

/*!
 * @brief   Sets mutex ownership for FLCN.  This mutex is shared between
 *          RM, PMU, VBIOS, and FLCN.
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
 *          FLCN failed to acquire the AUX channel mutex.
 */
FLCN_STATUS
dpauxChSetOwnership_v02_05
(
    LwU8  port,
    LwU32 newOwner
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       data    = 0;

    // Check if request owner is valid.
    if ((LW_PMGR_DP_AUXCTL_SEMA_REQUEST_RELEASE != newOwner) &&
        (LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN != newOwner))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    PMGR_REG_RD32_ERR_CHK(LW_PMGR_DP_AUXCTL(port), &data);
    data = FLD_SET_DRF_NUM(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, newOwner, data);

    if (newOwner == DRF_VAL(_PMGR, _DP_AUXCTL, _SEMA_GRANT, data))
    {
        // If same to current owner, return succeed directly.
        status = FLCN_OK;
        goto ErrorExit;
    } else if ((LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN == newOwner) &&
               (LW_PMGR_DP_AUXCTL_SEMA_GRANT_NONE != 
                    DRF_VAL(_PMGR, _DP_AUXCTL, _SEMA_GRANT, data)))
    {
        //
        // Check if semaphore not acquired before trying to set ownership, that reduces
        // contention to LW_PMGR_DP_AUXCTL reg write leading to error.
        //
        status = FLCN_ERR_AUX_SEMA_ACQUIRED;
        goto ErrorExit;
    } else if ((LW_PMGR_DP_AUXCTL_SEMA_REQUEST_RELEASE == newOwner) &&
               (LW_PMGR_DP_AUXCTL_SEMA_GRANT_DISPFLCN != 
                    DRF_VAL(_PMGR, _DP_AUXCTL, _SEMA_GRANT, data)))
    {
        // If want to release but owner is others, return error.
        status = FLCN_ERR_AUX_SEMA_ILWALID_RELEASE;
        goto ErrorExit;
    }

    //
    // dpauxChSetOwnership has error handling to retry thus ignore possible reg
    // write failure here.
    //
    (void)PMGR_REG_WR32(LW_PMGR_DP_AUXCTL(port), data);

    PMGR_REG_RD32_ERR_CHK(LW_PMGR_DP_AUXCTL(port), &data);
    if (newOwner != DRF_VAL(_PMGR, _DP_AUXCTL, _SEMA_GRANT, data))
    {
        status = FLCN_ERR_AUX_SEMA_ACQUIRED;
    }

ErrorExit:
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
FLCN_STATUS
dpauxAccess_v02_05
(
    DPAUX_CMD *pCmd,
    LwBool     bIsReadAccess
)
{
    FLCN_STATUS  flcnStatus     = FLCN_OK;
    LwU32        auxCtl         = 0;
    LwU32        auxStat        = 0;
    LwU32        auxData[DP_AUX_CHANNEL_MAX_BYTES / sizeof(LwU32)];
    LwU32        timeOutRetries  = DP_AUX_CHANNEL_TIMEOUT_MAX_TRIES;
    LwU32        deferRetries    = DP_AUX_CHANNEL_DEFAULT_DEFER_MAX_TRIES;
    LwU8         cmd             = pCmd->cmd;
    LwU32        i;
    LwBool       bIsDone         = LW_FALSE;
    LwU8         port;
    LwU32        addr;
    LwU8         size;
    LwU8        *pData;
    LwBool       bDeferRetry = LW_FALSE;

    port     = pCmd->port;
    addr     = pCmd->addr;
    size     = pCmd->size;
    pData    = (LwU8*)pCmd->pData;

    if ((port >= LW_PMGR_DP_AUXCTL__SIZE_1) ||
        (size > DP_AUX_CHANNEL_MAX_BYTES))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXSTAT(port), &auxStat);
    if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _HPD_STATUS, _UNPLUG, auxStat))
    {
        // Report Error
        return FLCN_ERR_HPD_UNPLUG;
    }

    // Write the data out for write access.
    if (!bIsReadAccess)
    {
        memset(auxData, 0, sizeof(auxData));
        memcpy(auxData, pData, size);

        // Set the dpauxData
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXDATA_WRITE_W0(port), auxData[0]);
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXDATA_WRITE_W1(port), auxData[1]);
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXDATA_WRITE_W2(port), auxData[2]);
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXDATA_WRITE_W3(port), auxData[3]);
    }

    // Set the dpauxCh addr
    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXADDR(port), addr);
    PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXCTL(port), &auxCtl);
    auxCtl = FLD_SET_DRF_NUM(_PMGR, _DP_AUXCTL, _CMD, cmd, auxCtl);
    //
    // CMDLEN field should be set to N-1, where N is the number of bytes
    // in the transaction (see dev_pmgr.ref)
    //
    auxCtl = FLD_SET_DRF_NUM(_PMGR, _DP_AUXCTL, _CMDLEN, size - 1, auxCtl);

    while (!bIsDone)
    {
        if (!bDeferRetry)
        {
            // Reset DPAUX
            auxCtl = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _RESET, _ASSERT, auxCtl);
            PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXCTL(port), auxCtl);

            //
            // Found regWrite fail at RESET_ASSERT and waiting HW tells root cause.
            // Below code is for internal testing only that guarantee Unigraf CTS
            // pass consistently without intermittent failure.
            //
            /*
            flcnStatus = PMGR_REG_WR32(LW_PMGR_DP_AUXCTL(port), auxCtl);
            if (flcnStatus != FLCN_OK)
            {
                PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXCTL(port), auxCtl);
            }
            */

            auxCtl = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _RESET, _DEASSERT, auxCtl);
            PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXCTL(port), auxCtl);
        }
        else
        {
            bDeferRetry = LW_FALSE;
        }

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

        // This initiates the transaction.
        auxCtl = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _TRANSACTREQ, _TRIGGER, auxCtl);
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXCTL(port), auxCtl);

        i = 0;

        PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXCTL(port), &auxCtl);
        while (FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _TRANSACTREQ, _PENDING, auxCtl))
        {
            PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXSTAT(port), &auxStat);
            if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _HPD_STATUS, _UNPLUG, auxStat))
            {
                flcnStatus = FLCN_ERR_HPD_UNPLUG;
                goto ErrorExit;
            }

            OS_PTIMER_SPIN_WAIT_US(DP_AUX_DELAY_US);
            if (i++ > DP_AUX_NUM_ATTEMPTS)
            {
                //
                // We ignore the timeout error and continue as in the normal
                // case. This is intentional (RM DPAUX code does the same).
                //
                break;
            }
            PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXCTL(port), &auxCtl);
        }

        PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXSTAT(port), &auxStat);
        if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _HPD_STATUS, _UNPLUG, auxStat))
        {
            bIsDone = LW_TRUE;
            flcnStatus = FLCN_ERR_HPD_UNPLUG;
            continue;
        }

        //
        // Bug 1790206: RX_ERROR, NO_STOP_ERROR bits are not reliable and
        // without any response, ignore and continue to following reply check.
        //
        if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _TIMEOUT, _PENDING, auxStat) ||
            FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _SINKSTAT_ERROR, _PENDING, auxStat))
        {
            if (timeOutRetries > 0)
            {
                PMGR_REG_WR32_ERR_EXIT(LW_PMGR_DP_AUXSTAT(port), auxStat);
                --timeOutRetries;
            }
            else
            {
                bIsDone = LW_TRUE;
                flcnStatus = FLCN_ERR_TIMEOUT;
            }
            continue;
        }

        if (FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _REPLYTYPE, _I2CDEFER, auxStat) ||
            FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _REPLYTYPE, _DEFER, auxStat))
        {
            if (deferRetries > 0)
            {
                --deferRetries;
                bDeferRetry = LW_TRUE;
            }
            else
            {
                bIsDone = LW_TRUE;
                flcnStatus = FLCN_ERR_TIMEOUT;
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

                // Set the dpauxData
                PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXDATA_READ_W0(port), &auxData[0]);
                PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXDATA_READ_W1(port), &auxData[1]);
                PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXDATA_READ_W2(port), &auxData[2]);
                PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXDATA_READ_W3(port), &auxData[3]);

                memcpy(pData, auxData, pCmd->size);
            }
            else
            {
                pCmd->size = (LwU8)DRF_VAL(_PMGR, _DP_AUXSTAT, _REPLY_M, auxStat);
            }
            flcnStatus = FLCN_OK;
        }
        else
        {
            flcnStatus = FLCN_ERROR;
        }
    }

ErrorExit:

    return flcnStatus;
}

/*!@brief Checks the current aux state on given auxPhysicalPort
 *
 *  @param[in]  Port    Physical port number
 *
 *  @return     LW_TRUE on IDLE else returns LW_FALSE
 *
 */
LwBool
dpauxCheckStateIdle_v02_05
(
    LwU32       port
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwBool      bIsIdle     = LW_FALSE; // default is not idle, and caller wait till aux idle.
    LwU32       data32      = 0;

    PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUXSTAT(port), &data32);
    bIsIdle = FLD_TEST_DRF(_PMGR, _DP_AUXSTAT, _AUXCTL_STATE, _IDLE, data32);

ErrorExit:
    return bIsIdle;
}

/*!@brief Disable HPD auto DPCD read function
 *
 *  @param[in]  port                Physical port number
 *  @param[in]  bEnable             LW_TRUE will enable AUTO_DPCD_READ
 *                                  LW_FALSE will disable AUTO_DPCD_READ
 *
 *  @return     FLCN_OK             Success
 *  @return     FLCN_ERR_TIMEOUT    Timeout to wait aux idle.
 *
 */
FLCN_STATUS
dpauxHpdConfigAutoDpcdRead_v02_05
(
    LwU32       port,
    LwBool      bEnable
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU32       data32      = 0;

    if (bEnable)
    {
        PMGR_REG_RD32_ERR_EXIT(LW_PMGR_HPD_IRQ_CONFIG(port), &data32);
        data32 = FLD_SET_DRF(_PMGR, _HPD_IRQ_CONFIG, _AUTO_DPCD_READ, _ENABLE, data32);
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_HPD_IRQ_CONFIG(port), data32);
    }
    else
    {
        LwU32  timeoutUs;
        LwU32  maxWaitIdleRetries;
        LwBool bAuxIdle;

        PMGR_REG_RD32_ERR_EXIT(LW_PMGR_HPD_IRQ_CONFIG(port), &data32);
        data32 = FLD_SET_DRF(_PMGR, _HPD_IRQ_CONFIG, _AUTO_DPCD_READ, _DISABLE, data32);
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_HPD_IRQ_CONFIG(port), data32);

        PMGR_REG_RD32_ERR_EXIT(LW_PMGR_DP_AUX_CONFIG(port), &data32);

        timeoutUs = LW_MAX(DRF_VAL(_PMGR, _DP_AUX_CONFIG, _TIMEOUT, data32),
                           DP_AUX_CHANNEL_TIMEOUT_WAITIDLE);
        maxWaitIdleRetries = (timeoutUs >> DP_AUX_WAIT_IDLE_DELAY_US_SHIFT); // Use shift instead division considering HS build.

        do
        {
            bAuxIdle = dpauxCheckStateIdle_v02_05(port);
            if (bAuxIdle)
            {
                break;
            }

            if (maxWaitIdleRetries == 0)
            {
                flcnStatus = FLCN_ERR_TIMEOUT;
                goto ErrorExit;
            }
            maxWaitIdleRetries--;

            OS_PTIMER_SPIN_WAIT_US(DP_AUX_WAIT_IDLE_DELAY_US);

        } while (flcnStatus != FLCN_ERR_TIMEOUT);
    }

ErrorExit:
    return flcnStatus;
}

