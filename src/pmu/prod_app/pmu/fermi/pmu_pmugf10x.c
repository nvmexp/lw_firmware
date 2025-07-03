/*
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pmugf10x.c
 * @brief  PMU Hal Functions for GF10X
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"
#include "pmu_objic.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief   Configure GPTimer.
 */
void
pmuPreInitGPTimer_GMXXX(void)
{
#ifdef UPROC_FALCON
    // General-purpose timer setup (used by the scheduler).

    LwU32 val = PmuInitArgs.cpuFreqHz / LWRTOS_TICK_RATE_HZ - 1U;

    REG_FLD_WR_DRF_NUM(CSB, _CMSDEC_FALCON, _GPTMRINT, _VAL, val);
    REG_FLD_WR_DRF_NUM(CSB, _CMSDEC_FALCON, _GPTMRVAL, _VAL, val);
#endif // UPROC_FALCON
}

/*!
 * @brief Get the IRQSTAT mask for the OS timer
 */
LwU32
pmuGetOsTickIntrMask_GMXXX(void)
{
    return DRF_SHIFTMASK(LW_CMSDEC_FALCON_IRQSTAT_GPTMR);
}
