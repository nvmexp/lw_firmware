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
 * @file   dpaux_objdpaux.c
 * @brief  Container-object for FLCN DPAUX routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pmgr.h"
#include "dpaux_cmn.h"
#include "dpaux_objdpaux.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_dpaux_private.h"
#include "config/dpaux-config.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Definitions ------------------- */
/* ------------------------- Public Functions ------------------------------- */
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
 * @return FLCN_ERR_MUTEX_ACQUIRED
 *          FLCN failed to acquire the AUX channel mutex.
 */
FLCN_STATUS
dpauxChSetOwnership
(
    LwU8  port,
    LwU32 newOwner
)
{
    LwU32      i = 0;
    FLCN_STATUS status;

    while ((status = dpauxChSetOwnership_HAL(&Dpaux, port, newOwner)) != FLCN_OK)
    {
       if (i++ > DP_AUX_SEMA_ACQUIRE_ATTEMPTS)
       {
           return status;
       }

       // Wait before the next attempt.
       OS_PTIMER_SPIN_WAIT_US(DP_AUX_DELAY_US);
    }

    return FLCN_OK;
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
 *          FLCN failed to acquire the AUX channel mutex.
 *
 * @return FLCN_ERROR
 *          If the transaction resutls in an unknown failure.
 */
FLCN_STATUS
dpauxAccess
(
    DPAUX_CMD *pCmd,
    LwBool     bIsReadAccess
)
{
    FLCN_STATUS status;
    FLCN_STATUS semaReleaseStatus;
    LwU8        port            = pCmd->port;
    LwU32       finished        = 0;
    LwU32       accessSize      = 0;

    //
    // Creating local copy because you dont want
    // to mess up the original addr and data
    //
    DPAUX_CMD rqCmd = *pCmd;

    if ((status = dpauxChSetOwnership(port, 
                      LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN)) != FLCN_OK)
    {
        return status;
    }

    //
    // Bug 200144097: The DP's AUTO_DPCD_READ function cannot guarantee
    // at least 400us elapsed from the last transaction and cause waveform
    // collision. However, we cannot set default disable because Link
    // CTS 4.3.2.1 request source need to read DPCD register within 100ms
    // while DP1.2 drivier doesn't read 0x200.
    // WAR to disable autoRead during SW transactions and restore enable for
    // HPD.
    //
    // Purposely fall through to continue aux transaction even disabling HW
    // DPCD auto read fail because the only failure case is aux not idle even
    // wait for 400us  That's safe to ignore previous transaction and continue
    // sw transactions.
    //
    (void)dpauxHpdConfigAutoDpcdRead_HAL(&Dpaux, port, LW_FALSE);

    do
    {
        accessSize = pCmd->size - finished;
        if (accessSize >= DP_AUX_CHANNEL_MAX_BYTES)
        {
            accessSize = DP_AUX_CHANNEL_MAX_BYTES;
        }

        rqCmd.size = accessSize;
        rqCmd.addr = pCmd->addr + finished;
        rqCmd.pData = pCmd->pData + finished;
        status = dpauxAccess_HAL(&Dpaux, &rqCmd, bIsReadAccess);

        if (status != FLCN_OK)
        {
            break;
        }

        if (bIsReadAccess && accessSize == 0)
        {
            // Successfully read 0 bytes? Break out
            break;
        }

        finished += accessSize;
    }
    while (finished < pCmd->size);
    pCmd->size = finished;

    status = dpauxHpdConfigAutoDpcdRead_HAL(&Dpaux, port, LW_TRUE);

    semaReleaseStatus = dpauxChSetOwnership(port,
                            LW_PMGR_DP_AUXCTL_SEMA_REQUEST_RELEASE);

    // in case of multiple errors return first error encountered
    return (status != FLCN_OK) ? status : semaReleaseStatus;
}

