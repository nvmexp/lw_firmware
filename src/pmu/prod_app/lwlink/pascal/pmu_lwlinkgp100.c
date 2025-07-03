/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lwlinkgp100.c
 * @brief  Contains LWLINK routines specific to GP100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"
#include "pmu_oslayer.h"
#include "dev_hshub.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlwlink.h"

#include "config/g_lwlink_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Update the HSHUB config registers
 *
 * @param[in]   config0    HSHUB config0 register
 * @param[in]   config1    HSHUB config1 register
 * @param[in]   config2    HSHUB config2 register
 * @param[in]   config6    HSHUB config6 register
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
lwlinkHshubUpdate_GP100
(
    LwU32 config0,
    LwU32 config1,
    LwU32 config2,
    LwU32 config6
)
{
    // Get the current mask of peers on PCIe
    LwU32 lwrPeerPcieMask = DRF_VAL(_PFB_HSHUB, _CONFIG0, _PEER_PCIE_MASK,
                                    BAR0_REG_RD32(LW_PFB_HSHUB_CONFIG0));

    // Get the updated mask of peers on PCIe
    LwU32 newPeerPcieMask = DRF_VAL(_PFB_HSHUB, _CONFIG0, _PEER_PCIE_MASK,
                                    config0);

    //
    // While switching HSHUB, the config registers should be updated
    // together in order to avoid any race with the flushes
    //
    appTaskCriticalEnter();
    {
        //
        // If a peer is being switched from LWLink to PCIe, update
        // config0 after updating config1 and config2 registers
        //
        if ((lwrPeerPcieMask & newPeerPcieMask) != newPeerPcieMask)
        {
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG1, config1);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG2, config2);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG0, config0);
        }
        else // Update config0 before config1 and config2
        {
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG0, config0);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG1, config1);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG2, config2);
        }
    }
    appTaskCriticalExit();

    return FLCN_OK;
}
