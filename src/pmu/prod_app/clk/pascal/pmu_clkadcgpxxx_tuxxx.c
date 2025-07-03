/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcgpxxx_tuxxx.c
 * @brief  Hal functions for ADC code from GP10X to TURING. Part of AVFS
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
#define CLK_ADC_MONITOR_DELAY_1_US  1U

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Set or clear ADC code
 *
 * @param[in]   pAdcDev       Pointer to ADC device
 * @param[in]   overrideMode  SW override mode of operation.
 * @param[in]   overrideCode  SW override ADC code.
 *
 * @return FLCN_OK                   Operation completed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT Invalid override mode specified
 */
FLCN_STATUS
clkAdcCodeOverrideSet_GP10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwU8            overrideCode,
    LwU8            overrideMode
)
{
    LwU32       data32 = 0;
    FLCN_STATUS status = FLCN_OK;

    // Compute values to write
    switch (overrideMode)
    {
        case LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_SW_REQ:
        {
            // Set ADC_DOUT to the ADC code to override with
            data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
                                     _ADC_DOUT, overrideCode, data32);
            // Set SW_OVERRIDE_ADC_YES
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
                                 _SW_OVERRIDE_ADC, _YES, data32);
            break;
        }
        case LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_HW_REQ:
        {
            // Set SW_OVERRIDE_ADC_NO
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
                                 _SW_OVERRIDE_ADC, _NO, data32);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            break;
        }
    }

    if (status == FLCN_OK)
    {
        // Now the register writes
        REG_WR32(FECS, CLK_ADC_REG_GET(pAdcDev, OVERRIDE), data32);
    }
    return status;
}

/*!
 * @brief   Gets the instantaneous ADC code value from the ADC_MONITOR register
 *
 * @param[in]   pAdcDev     Pointer to ADC device
 * @param[in]   pInstCode   Pointer to the instantaneous code value
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkAdcInstCodeGet_GP10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwU8            *pInstCode
)
{
    LwU32 reg32;
    LwU32 data32;

    reg32 = CLK_ADC_REG_GET(pAdcDev, MONITOR);

    // Engage the CAPTURE bit first
    data32 = REG_RD32(FECS, reg32);
    data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_MONITOR,
                         _CAPTURE_ADC_DOUT, _YES, data32);
    REG_WR32(FECS, reg32, data32);

    //
    // Wait 5 register clock cycles + 3 ADC clock cycles
    // Worst case all driven by XTAL, so 8 xtal (27MHz) cycles = ~300 nsec
    // 1 us is a good value
    //
    OS_PTIMER_SPIN_WAIT_US(CLK_ADC_MONITOR_DELAY_1_US);

    // Read back the ADC_DOUT for the CODE value
    data32 = REG_RD32(FECS, reg32);
    *pInstCode = (LwU8)DRF_VAL(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_MONITOR,
                         _ADC_DOUT, data32);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
