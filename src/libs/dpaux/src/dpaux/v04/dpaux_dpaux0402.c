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
 * @file dpaux_dpaux0402.c
 */

/* ------------------------- System Includes ------------------------------- */
#include "flcncmn.h"
#include "dpaux_cmn.h"
#include "address_map.h"
#include "flcntypes.h"
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Static variables ------------------------------ */
/* ------------------------- Task declaration ------------------------------ */
/* ------------------------- Static functions ------------------------------ */

#define dpAuxRead(address, pData, port) dioReadReg(LW_ADDRESS_MAP_DPAUX_BASE + (0x10000 * port) + address * 4, pData)
#define dpAuxWrite(address, pData, port) dioWriteReg(LW_ADDRESS_MAP_DPAUX_BASE + (0x10000 * port) + address * 4 , pData)

#define DP_AUX_MAX_INSTANCES 4
/*!
 * @brief   Sets mutex ownership for FLCN.  This mutex is shared between RM and
 *          TSEC.
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
dpauxChSetOwnership_v04_02
(
    LwU8  port,
    LwU32 newOwner
)
{

#if 0
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU32       data    = 0;

    // TODO:  Need to check if we still need to use the semaphore acquistion 
    // shared with RM because RM has entry in the DPAUX manual, need to see
    // if we are going to have an entry for tsec then this needs changes
    // in dpaux_cmn.h as well.

    // Lwrrently it is disabled as we don't need sync mechanism foe testing.

    /*
    if (port >= 1)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    // Check if request owner is valid.
    if ((LW_PDPAUX_DP_AUXCTL_SEMA_REQUEST_RELEASE != newOwner) &&
        (LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN != newOwner))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    dpAuxRead(LW_PDPAUX_DP_AUXCTL, &data);
    data = FLD_SET_DRF_NUM(_PDPAUX, _DP_AUXCTL, _SEMA_REQUEST, newOwner, data);

    if (newOwner == DRF_VAL(_PDPAUX, _DP_AUXCTL, _SEMA_GRANT, data))
    {
        // If same to current owner, return succeed directly.
        status = FLCN_OK;
        goto ErrorExit;
    } else if ((LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN == newOwner) &&
               (LW_PDPAUX_DP_AUXCTL_SEMA_GRANT_NONE != 
                    DRF_VAL(_PDPAUX, _DP_AUXCTL, _SEMA_GRANT, data)))
    {
        //
        // Check if semaphore not acquired before trying to set ownership, that reduces
        // contention to LW_PDPAUX_DP_AUXCTL reg write leading to error.
        //
        status = FLCN_ERR_AUX_SEMA_ACQUIRED;
        goto ErrorExit;
    } else if ((LW_PDPAUX_DP_AUXCTL_SEMA_REQUEST_RELEASE == newOwner) &&
               (LW_PMGR_DP_AUXCTL_SEMA_GRANT_DISPFLCN != 
                    DRF_VAL(_PDPAUX, _DP_AUXCTL, _SEMA_GRANT, data)))
    {
        // If want to release but owner is others, return error.
        status = FLCN_ERR_AUX_SEMA_ILWALID_RELEASE;
        goto ErrorExit;
    } */

    CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXCTL, data, port));
    CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXCTL, &data, port));
    if (newOwner != DRF_VAL(_PDPAUX, _DP_AUXCTL, _SEMA_GRANT, data))
    {
        flcnStatus = FLCN_ERR_AUX_SEMA_ACQUIRED;
    }

ErrorExit:
    return flcnStatus;
#endif
     return FLCN_OK;
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
dpauxAccess_v04_02
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
    LwU32        numOfAttempts   = 0;
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

    if ((port >= 1) ||
        (size > DP_AUX_CHANNEL_MAX_BYTES))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXSTAT, &auxStat, port));
    if (FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _HPD_STATUS, _UNPLUG, auxStat))
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
        CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXDATA_WRITE_W0, auxData[0], port));
        CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXDATA_WRITE_W1, auxData[1], port));
        CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXDATA_WRITE_W2, auxData[2], port));
        CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXDATA_WRITE_W3, auxData[3], port));
    }

    // Set the dpauxCh addr
    CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXADDR, addr, port));
    CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXCTL, &auxCtl, port));
    auxCtl = FLD_SET_DRF_NUM(_PDPAUX, _DP_AUXCTL, _CMD, cmd, auxCtl);
    //
    // CMDLEN field should be set to N-1, where N is the number of bytes
    // in the transaction (see dev_PDPAUX.ref)
    //
    auxCtl = FLD_SET_DRF_NUM(_PDPAUX, _DP_AUXCTL, _CMDLEN, size - 1, auxCtl);

    while (!bIsDone)
    {
        if (!bDeferRetry)
        {
            // Reset DPAUX
            auxCtl = FLD_SET_DRF(_PDPAUX, _DP_AUXCTL, _RST, _ASSERT, auxCtl);
            CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXCTL, auxCtl, port));

            auxCtl = FLD_SET_DRF(_PDPAUX, _DP_AUXCTL, _RST, _DEASSERT, auxCtl);
            CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXCTL, auxCtl, port));
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
        auxCtl = FLD_SET_DRF(_PDPAUX, _DP_AUXCTL, _TRANSACTREQ, _PENDING, auxCtl);
        CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXCTL, auxCtl, port));
        CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXCTL, &auxCtl, port));

        while (FLD_TEST_DRF(_PDPAUX, _DP_AUXCTL, _TRANSACTREQ, _PENDING, auxCtl))
        {
            CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXSTAT, &auxStat, port));
            if (FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _HPD_STATUS, _UNPLUG, auxStat))
            {
                flcnStatus = FLCN_ERR_HPD_UNPLUG;
                goto ErrorExit;
            }

            OS_PTIMER_SPIN_WAIT_US(DP_AUX_DELAY_US);
            if (numOfAttempts++ > DP_AUX_NUM_ATTEMPTS)
            {
                //
                // We ignore the timeout error and continue as in the normal
                // case. This is intentional (RM DPAUX code does the same).
                //
                break;
            }
            CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXCTL, &auxCtl, port));
        }

        CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXSTAT, &auxStat, port));
        if (FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _HPD_STATUS, _UNPLUG, auxStat))
        {
            bIsDone = LW_TRUE;
            flcnStatus = FLCN_ERR_HPD_UNPLUG;
            continue;
        }

        //
        // Bug 1790206: RX_ERROR, NO_STOP_ERROR bits are not reliable and
        // without any response, ignore and continue to following reply check.
        //
        if (FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _TIMEOUT_ERROR, _PENDING, auxStat) || 
            FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _SINKSTAT_ERROR, _PENDING, auxStat))
        {
            if (timeOutRetries > 0)
            {
                CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_DP_AUXSTAT, auxStat, port));
                --timeOutRetries;
            }
            else
            {
                bIsDone = LW_TRUE;
                flcnStatus = FLCN_ERR_TIMEOUT;
            }
            continue;
        }

        if (FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _REPLYTYPE, _I2CDEFER, auxStat) ||
            FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _REPLYTYPE, _DEFER, auxStat))
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
        if (FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _REPLYTYPE, _ACK, auxStat))
        {
            if (bIsReadAccess)
            {
                // Truncate the packet the size requested
                if (DRF_VAL(_PDPAUX, _DP_AUXSTAT, _REPLY_M, auxStat) < size)
                {
                    pCmd->size = (LwU8)DRF_VAL(_PDPAUX, _DP_AUXSTAT, _REPLY_M, auxStat);
                }

                // Set the dpauxData
                CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXDATA_READ_W0, &auxData[0], port));
                CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXDATA_READ_W1, &auxData[1], port));
                CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXDATA_READ_W2, &auxData[2], port));
                CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUXDATA_READ_W3, &auxData[3], port));

                memcpy(pData, auxData, pCmd->size);
            }
            else
            {
                pCmd->size = (LwU8)DRF_VAL(_PDPAUX, _DP_AUXSTAT, _REPLY_M, auxStat);
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
dpauxCheckStateIdle_v04_02
(
    LwU32       port
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwBool      bIsIdle     = LW_FALSE; // default is not idle, and caller wait till aux idle.
    LwU32       data32      = 0;

    if (port >= DP_AUX_MAX_INSTANCES)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    flcnStatus = dpAuxRead(LW_PDPAUX_DP_AUXSTAT, &data32, port);
    FLCN_ASSERT(flcnStatus);
    bIsIdle = FLD_TEST_DRF(_PDPAUX, _DP_AUXSTAT, _AUXCTL_STATE, _IDLE, data32);

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
dpauxHpdConfigAutoDpcdRead_v04_02
(
    LwU32       port,
    LwBool      bEnable
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU32       data32      = 0;

    if (port >= DP_AUX_MAX_INSTANCES)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (bEnable)
    {
        CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_HPD_IRQ_CONFIG, &data32, port));
        data32 = FLD_SET_DRF(_PDPAUX, _HPD_IRQ_CONFIG, _AUTO_DPCD_READ, _ENABLE, data32);
        CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_HPD_IRQ_CONFIG, data32, port));
    }
    else
    {
        LwU32  timeoutUs;
        LwU32  maxWaitIdleRetries;
        LwBool bAuxIdle;

        CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_HPD_IRQ_CONFIG, &data32, port));
        data32 = FLD_SET_DRF(_PDPAUX, _HPD_IRQ_CONFIG, _AUTO_DPCD_READ, _DISABLE, data32);
        CHECK_FLCN_STATUS(dpAuxWrite(LW_PDPAUX_HPD_IRQ_CONFIG, data32, port));

        CHECK_FLCN_STATUS(dpAuxRead(LW_PDPAUX_DP_AUX_CONFIG, &data32, port));

        timeoutUs = LW_MAX(DRF_VAL(_PDPAUX, _DP_AUX_CONFIG, _TIMEOUT, data32),
                           DP_AUX_CHANNEL_TIMEOUT_WAITIDLE);
        maxWaitIdleRetries = (timeoutUs >> DP_AUX_WAIT_IDLE_DELAY_US_SHIFT); // Use shift instead division considering HS build.

        do
        {
            bAuxIdle = dpauxCheckStateIdle_v04_02(port);
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
