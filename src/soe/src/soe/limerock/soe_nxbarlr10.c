/*
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_nxbarlr10.c
 * @brief  SOE NXBAR HAL Functions for LR10
 *
 * SOE NXBAR HAL functions implement chip-specific initialization, helper functions
 */
/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "dev_soe_csb.h"
#include "dev_nxbar_tile_ip.h"
#include "dev_nxbar_tc_global_ip.h"
#include "soe_objdiscovery.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_soe_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*
 * @brief Setup the Priv Level Masks for NXBAR LR10 registers
 * required for L0 access.
 */
void
soeSetupNxbarPrivLevelMasks_LR10(void)
{
    LwU32 ucastBaseAddress;
    LwU32 bcastBaseAddress;
    LwU32 data32;

    //
    // Only adjust priv level masks when we're booted in LS mode
    // so as to avoid soe halt.
    //
    data32 = REG_RD32_STALL(CSB, LW_CSOE_FALCON_SCTL);
    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        //
        // Setup PRIV LEVEL MASKS for NXBAR Engines
        //
        ucastBaseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NXBAR, 0, ADDRESS_UNICAST, 0);
        bcastBaseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NXBAR_BCAST, 0, ADDRESS_BROADCAST, 0);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_NXBAR_TC_ERR_PRIV_LEVEL_MASK);
        data32 = FLD_SET_DRF(_NXBAR_TC, _ERR_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_NXBAR_TC_ERR_PRIV_LEVEL_MASK, data32);

        ucastBaseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_TILE, 0, ADDRESS_UNICAST, 0);
        bcastBaseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_TILE_MULTICAST, 0, ADDRESS_BROADCAST, 0);

        data32 = REG_RD32(BAR0, ucastBaseAddress + LW_NXBAR_TILE_ERR_PRIV_LEVEL_MASK);
        data32 = FLD_SET_DRF(_NXBAR_TILE, _ERR_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, data32);
        REG_WR32(BAR0, bcastBaseAddress + LW_NXBAR_TILE_ERR_PRIV_LEVEL_MASK, data32);
    }
}

/*
 * @brief Setup the PROD values for NXBAR LR10 registers
 * that requires LS privileges.
 */

void
soeSetupNxbarProdValues_LR10(void)
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

        baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NXBAR_BCAST, 0, ADDRESS_BROADCAST, 0);
        REG_WR32(BAR0, baseAddress + LW_NXBAR_TC_CTRL1,
            DRF_DEF(_NXBAR_TC, _CTRL1, _PRI_FGCG_CTRL, __PROD));
    }
}

