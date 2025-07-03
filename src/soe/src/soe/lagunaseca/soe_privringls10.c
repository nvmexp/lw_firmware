/*
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soeprivringls10.c
 *
 * @brief  SOE HAL functions for priv ring initialization.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objdiscovery.h"
#include "dev_pri_masterstation_ip.h"
#include "dev_pri_hub_sys_ip.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_soe_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// Timeout for priv ring command interface
#define SOE_PRIV_MASTER_RING_COMMAND_IDLE_TIMEOUT_NS_LS10 (10*1000*1000)  // 10ms

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Initialize priv ring(s)
 */
void
soeInitPrivRing_LS10(void)
{
    LwU32 val;
    LwU32 status0, status1;
    LwBool bIsFmodel;
    FLCN_TIMESTAMP timeStart;
    LwU32 engBase;

    // Skip on fmodel
    bIsFmodel = FLD_TEST_DRF(_PSMC, _BOOT_2, _FMODEL, _YES,
        REG_RD32(BAR0, LW_PSMC_BOOT_2));
    if (bIsFmodel)
    {
        return;
    }

    // 1. Enumerate and start the PRI Ring
    engBase = getEngineBaseAddress(
        LW_DISCOVERY_ENGINE_NAME_SYS_PRI_HUB, 0, ADDRESS_UNICAST, 0);

    val = REG_RD32(BAR0, engBase + LW_PPRIV_SYS_PRI_RING_INIT);
    if (FLD_TEST_DRF(_PPRIV_SYS, _PRI_RING_INIT, _STATUS, _ALIVE, val) || 
        FLD_TEST_DRF(_PPRIV_SYS, _PRI_RING_INIT, _STATUS, _ALIVE_IN_SAFE_MODE, val))
    {
        return;
    }

    REG_WR32(BAR0, engBase + LW_PPRIV_SYS_PRI_RING_INIT,
        DRF_DEF(_PPRIV_SYS, _PRI_RING_INIT, _CMD, _ENUMERATE_AND_START));

    // 2. Wait for command completion.
    osPTimerTimeNsLwrrentGet(&timeStart);
    do
    {
        val = REG_RD32(BAR0, engBase + LW_PPRIV_SYS_PRI_RING_INIT);
        if (FLD_TEST_DRF(_PPRIV_SYS, _PRI_RING_INIT, _CMD, _NONE, val))
        {
            break;
        }

    } while (osPTimerTimeNsElapsedGet(&timeStart) <
        SOE_PRIV_MASTER_RING_COMMAND_IDLE_TIMEOUT_NS_LS10);

    if (!FLD_TEST_DRF(_PPRIV_SYS, _PRI_RING_INIT, _CMD, _NONE, val))
    {
        SOE_HALT();
        return;
    }

    // 3. Confirm PRI Ring initialized properly
    osPTimerTimeNsLwrrentGet(&timeStart);
    do
    {
        val = REG_RD32(BAR0, engBase + LW_PPRIV_SYS_PRI_RING_INIT);
        if (FLD_TEST_DRF(_PPRIV_SYS, _PRI_RING_INIT, _STATUS, _ALIVE, val))
        {
            break;
        }

    } while (osPTimerTimeNsElapsedGet(&timeStart) <
        SOE_PRIV_MASTER_RING_COMMAND_IDLE_TIMEOUT_NS_LS10);

    if (!FLD_TEST_DRF(_PPRIV_SYS, _PRI_RING_INIT, _STATUS, _ALIVE, val))
    {
        SOE_HALT();
        return;
    }

    // 4. If there were any issues, halt
    engBase = getEngineBaseAddress(
        LW_DISCOVERY_ENGINE_NAME_PRI_MASTER_RS, 0, ADDRESS_UNICAST, 0);
    status0 = REG_RD32(BAR0, engBase + LW_PPRIV_MASTER_RING_INTERRUPT_STATUS0);
    status1 = REG_RD32(BAR0, engBase + LW_PPRIV_MASTER_RING_INTERRUPT_STATUS1);

    if ((!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
            _DISCONNECT_FAULT, 0, status0))         ||
        (!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
            _GBL_WRITE_ERROR_FBP, 0, status0))      ||
        (!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
            _GBL_WRITE_ERROR_SYS, 0, status0))      ||
        (!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
            _OVERFLOW_FAULT, 0, status0))           ||
        (!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS0,
            _RING_START_CONN_FAULT, 0, status0))    ||
        (!FLD_TEST_DRF_NUM(_PPRIV_MASTER, _RING_INTERRUPT_STATUS1,
            _GBL_WRITE_ERROR_GPC, 0x0, status1)))
    {
            SOE_HALT();
            return; 
    }
}

