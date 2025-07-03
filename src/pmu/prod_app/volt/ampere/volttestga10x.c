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
 * @file    volttestga10x.c
 * @brief   PMU HAL functions related to volt tests for GA10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "pmu_objpmu.h"

#include "config/g_volt_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

//
// Delay for ensuring that HI_OFFSET is correctly consumed by VID_PWM logic.
// HI_OFFSET_OVERRIDE will only take effect at the boundary of VID_PWM period.
// So, worst case, we may see latency for HI_OFFSET to reflect in VID_PWM of
// upto 1 VID_PWM period.
// 1 VID_PWM period = VOLT_TEST_PWM_PERIOD utilsclk cycles = 160*9.25 ns = 1.4 us.
// Adding some buffer to accound for slow clocks on emulation.
// See http://lwbugs/200570625/18 for details.
//
#define VOLT_TEST_HI_OFFSET_UPDATE_TIME_US      (3)

/* ------------------------- Public Function Prototypes  -------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Enables/Disables the IPC HI_OFFSET override.
 *
 * @param[in]   bOverride   Boolean indicating if we need to override IPC HI_OFFSET.
 *                          LW_FALSE disables the override.
 * @param[in]   pwmHiOffset HI_OFFSET given in units of PWM to set if @ref
 *                          bOverride is LW_TRUE.
 * @note   HW hasn't verified the behavior of 13th bit of _IPC_HI_OFFSET_OVERRIDE for
           GA10X. So, this function ensures that the 13th bit of pwmHiOffset is always
           zero. For future chips where HW verifies the behavior of 13th bit, this function
           might need more changes/updates. See http://lwbugs/2080748/22 for details.
 */
void
voltTestHiOffsetOverrideEnable_GA10X
(
    LwBool bOverride,
    LwU16  pwmHiOffset
)
{
    LwU32  regIpcDebug    = REG_RD32(CSB, LW_CPWR_THERM_IPC_DEBUG);

    // Ensure that 13th bit is 0. See note above for details.
    if ((pwmHiOffset & BIT(12)) != 0)
    {
        PMU_BREAKPOINT();
    }

    if (bOverride)
    {
        regIpcDebug = FLD_SET_DRF(_CPWR_THERM, _IPC_DEBUG, _IPC_OVERRIDE_EN,
                        _ON, regIpcDebug);
        regIpcDebug = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_DEBUG,
                        _IPC_HI_OFFSET_OVERRIDE, pwmHiOffset, regIpcDebug);
    }
    else
    {
        regIpcDebug = FLD_SET_DRF(_CPWR_THERM, _IPC_DEBUG, _IPC_OVERRIDE_EN,
                        _OFF, regIpcDebug);
    }

    REG_WR32(CSB, LW_CPWR_THERM_IPC_DEBUG, regIpcDebug);
    OS_PTIMER_SPIN_WAIT_US(VOLT_TEST_HI_OFFSET_UPDATE_TIME_US);
}
