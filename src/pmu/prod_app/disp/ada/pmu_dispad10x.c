/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file   pmu_dispad10x.c
 * @brief  HAL-interface for the AD10x Display Engine only.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objdisp.h"
#include "pmu_disp.h"
#include "config/g_disp_private.h"

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief     Returns the frame time that's lwrrently programmed 
 *
 * @param[in] index of head, counted within the domain of VRR heads
 *
 * @return    frame time value programmed in the register, in Us
 */
LwU32 
dispGetLwrrentFrameTimeUs_AD10X
(
    LwU32 idx
)
{
    LwU32 lwrrFrameTimeUs;

    lwrrFrameTimeUs = 
        DRF_VAL(_PDISP, _IHUB_LWRS_FRAME_TIME, _IN_USEC, REG_RD32(BAR0,
            LW_PDISP_IHUB_LWRS_FRAME_TIME(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex)));

    return lwrrFrameTimeUs;
}

/*!
 * @brief     Programs the new frame time value in the register
 *
 * @param[in] index of head, counted within the domain of VRR heads
 */
void 
dispProgramNewFrameTime_AD10X
(
    LwU32 idx
)
{
    LwU32 regVal;

    regVal = DRF_NUM(_PDISP, _IHUB_LWRS_FRAME_TIME, _IN_USEC,
                     (DispImpMclkSwitchSettings.vrrBasedMclk.oldFrameTimeUs[idx] +
                      DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnUs));

    // Program new frame time.
    REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_FRAME_TIME(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);
}

/*!
 * @brief     Gets the old frame time value and programs the register
 *
 * @param[in] index of head, counted within the domain of VRR heads
 */
void 
dispRestoreFrameTime_AD10X
(
    LwU32 idx
)
{
    LwU32 regVal;

    regVal = DRF_NUM(_PDISP, _IHUB_LWRS_FRAME_TIME, _IN_USEC, DispImpMclkSwitchSettings.vrrBasedMclk.oldFrameTimeUs[idx]);

    REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_FRAME_TIME(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);
}
