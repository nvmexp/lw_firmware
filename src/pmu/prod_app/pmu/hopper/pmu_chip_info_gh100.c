/*
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pmu_chip_info_gh100.c
 * @brief  Code that retrieves and caches chip information for the GPU on which
 *         the PMU is running.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "dev_master.h"
#include "dev_xtl_ep_pcfg_gpu.h"
#include "dev_bus_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"
#include "config/g_bif_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief   Cache chip information (arch, impl, netlist).
 */
void
pmuPreInitChipInfo_GH100(void)
{
    LwU32 regVal;
    LwU32 boot42;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bif)
    };

    boot42 = REG_RD32(BAR0, LW_PMC_BOOT_42);

    Pmu.chipInfo.id.sId.arch = DRF_VAL(_PMC, _BOOT_42, _ARCHITECTURE,   boot42);
    Pmu.chipInfo.id.sId.impl = DRF_VAL(_PMC, _BOOT_42, _IMPLEMENTATION, boot42);
    Pmu.chipInfo.majorRev    = DRF_VAL(_PMC, _BOOT_42, _MINOR_REVISION, boot42);
    Pmu.chipInfo.minorRev    = DRF_VAL(_PMC, _BOOT_42, _MAJOR_REVISION, boot42);

    Pmu.chipInfo.netList     = REG_RD32(BAR0, LW_PBUS_EMULATION_REV0);

    // Attach overlays -- Must exit through after this point.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Find out GPU's device ID.
        regVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_EP_PCFG_GPU_ID);
        Pmu.gpuDevId = DRF_VAL(_EP_PCFG_GPU, _ID, _DEVICE, regVal);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}
