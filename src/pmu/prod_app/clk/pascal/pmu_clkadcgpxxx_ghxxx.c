/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcgpxxx_ghxxx.c
 * @brief  Hal functions for ADC code from PASCAL to HOPPER. Part of AVFS.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Get the ADC step size of the given ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 *
 * @return      LwU32       Step size in uV
 */
LwU32
clkAdcStepSizeUvGet_GP10X
(
    CLK_ADC_DEVICE  *pAdcDev
)
{
    return CLK_NAFLL_LUT_STEP_SIZE_UV();
}

/*!
 * @brief Get the minimum voltage supported by the given ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 *
 * @return      LwU32       Minimum voltage in uV
 */
LwU32
clkAdcMilwoltageUvGet_GP10X
(
    CLK_ADC_DEVICE  *pAdcDev
)
{
    return CLK_NAFLL_LUT_MIN_VOLTAGE_UV();
}

/* ------------------------- Private Functions ------------------------------ */
