/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_isohubga10x.c
 * @brief  HAL-interface for the GA10X Display Engine.
 */

#include "pmusw.h"
#include "dev_disp.h"
#include "pmu_objdisp.h"
#include "pmu_objlpwr.h"

/*!
 * @brief  Programs the MSCG/DIFR cycle and exit time (provided by RM) 
 *         
 * Both MSCG and DIFR use MSCG cycle and exit time registers with
 * separate values callwlated by RM.
 *
 * @param[in] pPstateMscgSettings  Pointer to RM_PMU_DISP_PSTATE_MSCG_SETTINGS
 */
void
isohubProgramMscgCycleAndExitTimeUs_GA10X
(
    RM_PMU_DISP_PSTATE_MSCG_SETTINGS *pPstateMscgSettings
)
{
    LwU32 regVal = REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_MSCG_MIN_CYCLE_TIME);

    regVal = FLD_SET_DRF_NUM(_PDISP, _IHUB_COMMON_MSCG_MIN_CYCLE_TIME, _USEC,
                             pPstateMscgSettings->cycleTimeUs, regVal);

    regVal = FLD_SET_DRF_NUM(_PDISP, _IHUB_COMMON_MSCG_MIN_CYCLE_TIME, _EXIT_USEC,
                             pPstateMscgSettings->exitTimeUs, regVal);

    REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_MSCG_MIN_CYCLE_TIME, regVal);
}

/*!
 * @brief  Enables One shot mode based Vblank MSCG in HW
 * 
 * @param[in] pPstateMscgSettings   Pointer to RM_PMU_DISP_PSTATE_MSCG_SETTINGS
 * @param[in] maxWindows            Maximum number of windows supported
 * @param[in] maxLwr                Maximum number of lwrsors supported
 * @param[in] bEnable               Enables One shot mode based Vblank MSCG
 */
void
isohubEnableOsmVblankMscg_GA10X
(
    RM_PMU_DISP_PSTATE_MSCG_SETTINGS *pPstateMscgSettings,
    LwU32 maxWindows,
    LwU32 maxLwr,
    LwBool bEnable
)
{
    LwU32 regVal;
    LwU32 winIdx;
    LwU32 lwrIdx;

    if (bEnable)
    {
        for (lwrIdx = 0; lwrIdx < maxLwr; lwrIdx++)
        {
            if (pPstateMscgSettings->cursor[lwrIdx].bEnable)
            {
                regVal =
                    REG_RD32(BAR0, LW_PDISP_IHUB_LWRS_MSCG_CTLA(lwrIdx));
                regVal |= DRF_DEF(_PDISP, _IHUB_LWRS_MSCG_CTLA, _OSM_VBLANK_MSCG, _ENABLE);
                REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_MSCG_CTLA(lwrIdx), regVal);
            }
        }

        for (winIdx = 0; winIdx < maxWindows; winIdx++)
        {
            if (pPstateMscgSettings->window[winIdx].bEnable)
            {
                regVal = 
                    REG_RD32(BAR0, LW_PDISP_IHUB_WINDOW_MSCG_CTLA(winIdx));
                regVal |= DRF_DEF(_PDISP, _IHUB_WINDOW_MSCG_CTLA, _OSM_VBLANK_MSCG, _ENABLE);
                REG_WR32(BAR0, LW_PDISP_IHUB_WINDOW_MSCG_CTLA(winIdx), regVal);
            }
        }
    }
    else
    {
        for (lwrIdx = 0; lwrIdx < maxLwr; lwrIdx++)
        {
            if (pPstateMscgSettings->cursor[lwrIdx].bEnable)
            {
                regVal =
                    REG_RD32(BAR0, LW_PDISP_IHUB_LWRS_MSCG_CTLA(lwrIdx));
                regVal |= DRF_DEF(_PDISP, _IHUB_LWRS_MSCG_CTLA, _OSM_VBLANK_MSCG, _DISABLE);
                REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_MSCG_CTLA(lwrIdx), regVal);
            }
        }

        for (winIdx = 0; winIdx < maxWindows; winIdx++)
        {
            if (pPstateMscgSettings->window[winIdx].bEnable)
            {
                regVal = 
                    REG_RD32(BAR0, LW_PDISP_IHUB_WINDOW_MSCG_CTLA(winIdx));
                regVal |= DRF_DEF(_PDISP, _IHUB_WINDOW_MSCG_CTLA, _OSM_VBLANK_MSCG, _DISABLE);
                REG_WR32(BAR0, LW_PDISP_IHUB_WINDOW_MSCG_CTLA(winIdx), regVal);
            }
        }
    }
}
