/*
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pmu_chip_info_gm20b.c
 * @brief  Code that retrieves and caches chip information for the GPU on which
 *         the PMU is running.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

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
pmuPreInitChipInfo_GA10B(void)
{
    LwU32 boot42 = REG_RD32(BAR0, LW_PMC_BOOT_42);

    Pmu.chipInfo.id.sId.arch = DRF_VAL(_PMC, _BOOT_42, _ARCHITECTURE,   boot42);
    Pmu.chipInfo.id.sId.impl = DRF_VAL(_PMC, _BOOT_42, _IMPLEMENTATION, boot42);
    Pmu.chipInfo.majorRev    = DRF_VAL(_PMC, _BOOT_42, _MINOR_REVISION, boot42);
    Pmu.chipInfo.minorRev    = DRF_VAL(_PMC, _BOOT_42, _MAJOR_REVISION, boot42);
}

