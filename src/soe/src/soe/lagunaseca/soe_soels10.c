/*
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soels10.c
 * @brief  SOE HAL Functions for LS10 and later chips
 *
 * SOE HAL functions implement chip-specific initialization, helper functions
 */
/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "dev_soe_csb.h"
#include "dev_nxbar_tile_ip.h"
#include "dev_lwlsaw_ip.h"
#include "dev_npg_ip.h"
#include "soe_objdiscovery.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_soe_private.h"
#include "config/g_bif_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*
 * @brief Setup the PROD values for LS10 registers
 * that requires LS privileges.
 */
void
soeSetupProdValues_LS10(void)
{
    LwU32 baseAddress;
    LwU32 data32;

    //
    // Only adjust priv level masks when we're booted in LS mode
    // so as to avoid soe halt.
    //

    data32 = REG_RD32_STALL(CSB, LW_CSOE_FALCON_SCTL);
    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        //
        // Setup PRODs for NXBAR Engines
        //
        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_TILE_MULTICAST, 0, ADDRESS_BROADCAST, 0);
        REG_WR32(BAR0, baseAddress + LW_NXBAR_TILE_CTRL0,
            DRF_DEF(_NXBAR_TILE, _CTRL0, _PRI_FGCG_CTRL, __PROD));

        //
        // Setup PRODs for NPG Engines
        //
        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NPG, 0, ADDRESS_UNICAST, 0);
        data32 = REG_RD32(BAR0, baseAddress + LW_NPG_CTRL_CLOCK_GATING);
        data32 = FLD_SET_DRF(_NPG, _CTRL_CLOCK_GATING, _CG1_SLCG, __PROD, data32);
        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NPG_BCAST, 0, ADDRESS_BROADCAST, 0);
        REG_WR32(BAR0, baseAddress + LW_NPG_CTRL_CLOCK_GATING, data32);

        //
        // Setup PRODs for SAW
        //
        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_SAW, 0, ADDRESS_UNICAST, 0);

        REG_WR32(BAR0, baseAddress + LW_LWLSAW_PCIE_PRI_CLOCK_GATING,
            DRF_DEF(_LWLSAW, _PCIE_PRI_CLOCK_GATING, _CG1_SLCG, __PROD));

        //
        // Setup PRODs for XP/XVE
        //
        bifSetupProdValues_HAL();

        //
        // Setup PRODs for SOE Engine
        //
        REG_WR32(CSB, LW_CSOE_PRIV_BLOCKER_CTRL_CG1,
            DRF_DEF(_CSOE, _PRIV_BLOCKER_CTRL_CG1, _SLCG, __PROD));
    }
}

