/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcgp10x.c
 * @brief  Common Hal functions for ADC code in GP10X applicable across all
 *         all platforms. Part of AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Initialize register mapping within ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 *
 * @return  FLCN_ERR_ILWALID_INDEX  If register mapping init has failed
 * @return  FLCN_OK                 Otherwise
 */
FLCN_STATUS
clkAdcRegMapInit_GP10X
(
 CLK_ADC_DEVICE *pAdcDev
)
{
    LwBool bPhyIndexFound = LW_FALSE;
    LwBool bLogIndexFound = LW_FALSE;
    LwU8 idx;

    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_LOGICAL_INDEXING))
    {
        bLogIndexFound = LW_TRUE;
    }

    for (idx = 0;
    (ClkAdcRegMap_HAL[idx].id != LW2080_CTRL_CLK_ADC_ID_MAX);
     idx++)
    {
        if (!bPhyIndexFound &&
            (ClkAdcRegMap_HAL[idx].id == pAdcDev->id))
        {
            pAdcDev->regMapIdx = idx;
            bPhyIndexFound = LW_TRUE;
        }

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_LOGICAL_INDEXING)
        if (!bLogIndexFound &&
           (ClkAdcRegMap_HAL[idx].id == pAdcDev->logicalApiId))
        {
            pAdcDev->logicalRegMapIdx = idx;
            bLogIndexFound = LW_TRUE;
        }
#endif

        if (bPhyIndexFound &&
        bLogIndexFound)
        {
            return FLCN_OK;
        }
    }

 return FLCN_ERR_ILWALID_INDEX;
}

/* ------------------------- Private Functions ------------------------------ */
