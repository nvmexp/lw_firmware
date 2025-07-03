/*
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soeprivringlr10.c
 *
 * @brief  SOE HAL functions for priv ring initialization.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "dev_pri_ringmaster.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_prt.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_soe_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// Timeout for priv ring command interface
#define SOE_PRIV_MASTER_RING_COMMAND_IDLE_TIMEOUT_NS_LR10 (5*1000*1000)  // 5ms

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*
 * @Brief : Send priv ring command and wait for completion
 *
 * @param[in]  cmd   encoded priv ring command
 *
 * @return Indication of successful command exelwtion
 */
LwBool
_soeIssueRingMasterCmd_LR10
(
    LwU32 cmd
)
{
    FLCN_TIMESTAMP timeStart;
    LwU32 val;

    // Issue the requested commands
    REG_WR32(BAR0, LW_PPRIV_MASTER_RING_COMMAND, cmd);

    osPTimerTimeNsLwrrentGet(&timeStart);
    do
    {
        val = REG_RD32(BAR0, LW_PPRIV_MASTER_RING_COMMAND);
        if (FLD_TEST_DRF(_PPRIV_MASTER, _RING_COMMAND, _CMD, _NO_CMD, val))
        {
            return LW_TRUE;
        }
    }
    while (osPTimerTimeNsElapsedGet(&timeStart) <
        SOE_PRIV_MASTER_RING_COMMAND_IDLE_TIMEOUT_NS_LR10);
    
    //
    // Timeout waiting for the command to complete, fail now.
    // TODO: Add appropriate fault handler, HALT for now.
    //
    SOE_HALT();

    return LW_FALSE;
}

/*!
 * @brief Initialize priv ring(s)
 */
void
soeInitPrivRing_LR10(void)
{
    LwU32 value;

    // Kick off the enum and start
    value = DRF_DEF(_PPRIV_MASTER, _RING_COMMAND, _CMD, _ENUMERATE_AND_START_RING);
    if (!_soeIssueRingMasterCmd_LR10(value))
    {
        //
        // The ENUMERATE_AND_START_RING command did not complete, fail now.
        // TODO: Add appropriate fault handler, HALT for now.
        //
        SOE_HALT();
        return; 
    }

    // Ensure we have required connectivity
    value = REG_RD32(BAR0, LW_PPRIV_MASTER_RING_START_RESULTS);
    if (!FLD_TEST_DRF(_PPRIV_MASTER, _RING_START_RESULTS, _CONNECTIVITY, _PASS, value))
    {
        //
        // PRIV ring connectivity failed.
        // TODO: Add appropriate fault handler, HALT for now.
        //
        SOE_HALT();
        return; 
    }

    // If there were any issues, ack
    value = REG_RD32(BAR0, LW_PPRIV_MASTER_RING_INTERRUPT_STATUS0);
    if (value)
    {
        if ((!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
                _RING_START_CONN_FAULT, 0, value)) ||
            (!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
                _DISCONNECT_FAULT, 0, value))      ||
            (!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
                _OVERFLOW_FAULT, 0, value)))
        {
            //
            // MISC interrupt assertions, fail now.
            // TODO: Add appropriate fault handler, HALT for now.
            //
            SOE_HALT();
            return; 
        }

        // Kick off the interrupt ack
        value = DRF_DEF(_PPRIV_MASTER, _RING_COMMAND, _CMD, _ACK_INTERRUPT);
        if (!_soeIssueRingMasterCmd_LR10(value))
        {
            //
            // The ACK_INTERRUPT command did not complete, fail now.
            // TODO: Add appropriate fault handler, HALT for now.
            //
            SOE_HALT();
            return; 
        }
    }
    return;
}

