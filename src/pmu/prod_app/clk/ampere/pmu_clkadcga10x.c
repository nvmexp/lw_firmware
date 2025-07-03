/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcga10x.c
 * @brief  Hal functions for ADC code in GA10X. Part of AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Return the current HW enable/disable state of the given ADC device
 *
 * @param[in]   pAdcDev         Pointer to ADC device
 *
 * @return LW_TRUE  ADC HW state enabled
 * @return LW_FALSE ADC HW state disabled or ADC device is NULL
 */
LwBool
clkAdcIsHwEnabled_GA10X
(
    CLK_ADC_DEVICE *pAdcDev
)
{
    if (pAdcDev == NULL)
    {
        return LW_FALSE;
    }

    return FLD_TEST_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _ENABLE, _YES,
                        REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, CTRL)));
}

/*!
 * @brief   Return the current HW power-on/power-off state of the given ADC device
 *
 * @param[in]   pAdcDev         Pointer to ADC device
 *
 * @return LW_TRUE  ADC HW state powered-on
 * @return LW_FALSE ADC HW state powered-off or ADC device is NULL
 */
LwBool
clkAdcIsHwPoweredOn_GA10X
(
    CLK_ADC_DEVICE *pAdcDev
)
{
    if (pAdcDev == NULL)
    {
        return LW_FALSE;
    }

    return FLD_TEST_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _IDDQ, _POWER_ON,
                        REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, CTRL)));
}

/*!
 * @brief   Return if the ADC ACC is correctly initialized
 *
 * @param[in]   pAdcDev         Pointer to ADC device
 *
 * @return LW_TRUE  ADC ACC correctly initialized
 * @return LW_FALSE ADC ACC not initialized or the ADC device is NULL
 */
LwBool
clkAdcAccIsInitialized_GA10X
(
    CLK_ADC_DEVICE *pAdcDev
)
{
    LwU32 data32 = REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, CTRL));
    if (pAdcDev == NULL)
    {
        return LW_FALSE;
    }

    return (FLD_TEST_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _ACC_ENABLE, _YES, data32)                &&
            FLD_TEST_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _CAPTURE_ACC, _CONTINUAL_CAPTURE, data32) &&
            FLD_TEST_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _ACC_SEL, _CALIBRATED, data32));
}
/* ------------------------- Private Functions ------------------------------ */
