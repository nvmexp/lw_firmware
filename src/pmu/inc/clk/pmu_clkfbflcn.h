/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKFBFLCN_H
#define PMU_CLKFBFLCN_H

/*!
 * @file pmu_clkfbflcn.h
 *
 * @copydoc pmu_clkfbflcn.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
extern LwrtosSemaphoreHandle FbflcnRpcMutex;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS clkTriggerMClkSwitchOnFbflcn(RM_PMU_STRUCT_CLK_MCLK_SWITCH *pMclkSwitchRequest, LwU32 *pFbStopTimeUs, FLCN_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_perfDaemon", "clkTriggerMClkSwitchOnFbflcn");
FLCN_STATUS clkTriggerMClkTrrdModeSwitch(LwU8 tFAW, LwU32 mclkFreqKHz)
    GCC_ATTRIB_SECTION("imem_perfDaemon", "clkTriggerMClkTrrdModeSwitch");

#endif // PMU_CLKFBFLCN_H

