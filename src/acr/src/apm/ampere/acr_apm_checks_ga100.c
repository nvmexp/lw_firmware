/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_init_ga100.c
 */
 /* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_master.h"
#include "lwdevid.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

/**
 * @brief Check if APM has been enabled by the Hypervisor using either OOB or LwFlash
 *        VBIOS snaps the APM enablement bit in a secure scratch register for Ucode & RM to consume.
 *        Bit 0 in LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20 is used for CC enable/disable bit.
 */
ACR_STATUS
acrCheckIfApmEnabled_GA100()
{
    ACR_STATUS  status       = ACR_ERROR_APM_NOT_ENABLED;
    LwU32       chipId       = 0;
    LwU32       devId        = 0;
    LwU32       val          = 0;
    LwU32       apmMode      = 0;

    // If this is a debug board, continue allowing APM to be enabled
    val = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS);
    if (val != LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_NO)
    {
        // Check for APM Mode
        apmMode = ACR_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20_CC);

        if (!FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_20_CC, _MODE_ENABLED, _TRUE, apmMode))
        {
            status = ACR_ERROR_APM_NOT_ENABLED;
            goto ErrorExit;
        }

        // SKU Check
        devId  = ACR_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDA);
        chipId = ACR_REG_RD32(BAR0, LW_PMC_BOOT_42);
        devId  = DRF_VAL( _FUSE, _OPT_PCIE_DEVIDA, _DATA, devId);
        chipId = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chipId);

        // Adding ct_assert to alert incase if the DevId values are changed for the defines
        ct_assert(LW_PCI_DEVID_DEVICE_P1001_SKU230 == 0x20B5);

        if (!((chipId == LW_PMC_BOOT_42_CHIP_ID_GA100) && ( devId == LW_PCI_DEVID_DEVICE_P1001_SKU230)))
        {
            status = ACR_ERROR_APM_NOT_ENABLED;
            goto ErrorExit;
        }
    }

    status = ACR_OK;

ErrorExit:
    return status;
}