/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmbatu10x.c
 * @brief   Thermal PMU BA HAL functions for TU10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmgr.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LUT_VOLTAGE_STEP_SIZE_UV  \
     (BIT32(LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_ADC_OUTPUT_WIDTH - LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_LUT_INDEX_WIDTH) \
     * LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_STEP_SIZE_UV)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief    Setup HW realtime FACTOR_A control
 *
 * @param[in]   pDev      Pointer to a PWR_DEVICE object
 */
void
thermInitFactorALut_TU10X
(
    PWR_DEVICE *pDev
)
{
    LwU8               lutIdx;
    LwU32              regVal = 0;
    LwU32              factorA;
    LwU32              voltageuV;
    PWR_DEVICE_BA15HW *pBa15Hw = (PWR_DEVICE_BA15HW *)pDev;

    ct_assert(LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_ADC_OUTPUT_WIDTH >
              LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_LUT_INDEX_WIDTH);
    ct_assert(LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_STEP_SIZE_UV > 0);

    // populate FACTOR_A LUT
    regVal = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_WRITE_EN,
                         _WRITE, regVal);
    regVal = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_SEL,
                         _FACTOR_A, regVal);

    for (lutIdx = 0; lutIdx < LW_CPWR_THERM_PEAKPOWER_CONFIG14_LUT_INDEX__SIZE_1; lutIdx++)
    {
        //
        // the factor A LUT maps voltage readouts from the HW ADC to factor A values.
        // each LUT entry holds the factor A value valid for equal-sized range of voltages.
        // HW expects each factor A LUT entry to be computed against the lower bound of
        // the voltage range that the LUT entry handles. therefore, the voltage that
        // an entry should compute factor A against =
        //         <lowest handled voltage> + (<voltage range size of each entry> * <entry indx>)
        //
        voltageuV = LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_ZERO_UV +
                    (lutIdx * LUT_VOLTAGE_STEP_SIZE_UV);
        factorA = pmgrComputeFactorA(pDev, voltageuV);

        regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_DATA,  factorA, regVal);
        regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_INDEX, lutIdx,  regVal);
        REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG14(pBa15Hw->super.windowIdx), regVal);
    }
}
