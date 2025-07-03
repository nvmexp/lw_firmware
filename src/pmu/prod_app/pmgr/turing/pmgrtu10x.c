/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmgrtu10x.c
 * @brief  Contains all PCB Management (PGMR) routines specific to TU10X+.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pmgr.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */
/*!
 * @brief Load tach functionality (init HW state).
 *
 * @param[in]   tachSource      Requested TACH source as PMU_PMGR_TACH_SOURCE_<xyz>
 * @param[in]   gpioRequested   Requested GPIO pin to load tachometer
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid TACH source passed as an input argument.
 * @return FLCN_OK
 *      Tachometer loaded successfully.
 */
FLCN_STATUS
pmgrTachLoad_TU10X
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU8                    gpioRequested
)
{
    LwU32       hwEnumIndex = LW_PMGR_GPIO_INPUT_CNTL_0__UNASSIGNED;
    FLCN_STATUS status      = FLCN_ERR_ILWALID_ARGUMENT;

    if ((tachSource == PMU_PMGR_PMGR_TACH_SOURCE) ||
        (tachSource == PMU_PMGR_THERM_TACH_SOURCE_0))
    {
        hwEnumIndex = LW_PMGR_GPIO_INPUT_FUNC_TACH_0;
    }
    else if (tachSource == PMU_PMGR_THERM_TACH_SOURCE_1)
    {
        hwEnumIndex = LW_PMGR_GPIO_INPUT_FUNC_TACH_1;
    }

    if (hwEnumIndex != LW_PMGR_GPIO_INPUT_CNTL_0__UNASSIGNED)
    {
        LwU8    gpioLwrrent;

        // Get the pin on which TACH is right now.
        gpioLwrrent =
            REG_FLD_IDX_RD_DRF(BAR0, _PMGR, _GPIO_INPUT_CNTL, hwEnumIndex, _PINNUM);

        // If current assignment does not match DCB.
        if (gpioLwrrent != gpioRequested)
        {
            // Program TACH on requested pin. Recovery.
            REG_FLD_IDX_WR_DRF_NUM(BAR0, _PMGR, _GPIO_INPUT_CNTL, hwEnumIndex,
                _PINNUM, gpioRequested);
        }

        status = FLCN_OK;
    }

    return status;
}

/*!
 * @brief Map TACH GPIO pin to TACH source.
 *
 * @param[in]  gpioPin  Requested TACH GPIO pin for mapping
 *
 * @return PMU_PMGR_TACH_SOURCE:
 *         PMU_PMGR_TACH_SOURCE_ILWALID if TACH is not supported on given GPIO
 *         PMU_PMGR_TACH_SOURCE_<xyz> depending on GPIO crossbar
 */
PMU_PMGR_TACH_SOURCE
pmgrMapTachPinToTachSource_TU10X
(
    LwU8    tachPin
)
{
    PMU_PMGR_TACH_SOURCE    tachSource = PMU_PMGR_TACH_SOURCE_ILWALID;
    LwU8                    gpioPin;

    // Get the GPIO pin corresponding to TACH_0.
    gpioPin = REG_FLD_IDX_RD_DRF(BAR0, _PMGR, _GPIO_INPUT_CNTL,
                LW_PMGR_GPIO_INPUT_FUNC_TACH_0, _PINNUM);
    if (gpioPin == tachPin)
    {
        tachSource = PMU_PMGR_THERM_TACH_SOURCE_0;
        goto pmgrMapTachPinToTachSource_TU10X_exit;
    }

    // Get the GPIO pin corresponding to TACH_1.
    gpioPin = REG_FLD_IDX_RD_DRF(BAR0, _PMGR, _GPIO_INPUT_CNTL,
                LW_PMGR_GPIO_INPUT_FUNC_TACH_1, _PINNUM);
    if (gpioPin == tachPin)
    {
        tachSource = PMU_PMGR_THERM_TACH_SOURCE_1;
        goto pmgrMapTachPinToTachSource_TU10X_exit;
    }

pmgrMapTachPinToTachSource_TU10X_exit:
    return tachSource;
}

/*!
 * Capture ILLUM debug info to be exposed to RM in case of PMU HALT
 * 
 * Lwrrently we are exposing the following -
 *  - RGB configuration
 *  - Illum device index
 *  - I2C1_CNTL information
 *  - status code
 */
void
pmgrIllumDebugInfoCapture_TU10X
(
    ILLUM_ZONE *pIllumZone,
    FLCN_STATUS status
)
{
    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(0), status);
    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(1), (pIllumZone->ctrlMode << 8) | pIllumZone->provIdx);
    
    switch (pIllumZone->ctrlMode)
    {
        case LW2080_CTRL_ILLUM_CTRL_MODE_MANUAL_RGB:
        {
            ILLUM_ZONE_RGB *pIllumZoneRGB = (ILLUM_ZONE_RGB *)pIllumZone;
            LW2080_CTRL_PMGR_ILLUM_ZONE_CTRL_MODE_MANUAL_RGB_PARAMS
                 *pManualRGBParams = &pIllumZoneRGB->ctrlModeParams.manualRGB.rgbParams;

            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(2), pManualRGBParams->colorR | (pManualRGBParams->colorG << 8) |
                    (pManualRGBParams->colorB << 16) | (pManualRGBParams->brightnessPct << 24));
            break;
        }
        case LW2080_CTRL_ILLUM_CTRL_MODE_PIECEWISE_LINEAR_RGB:
        {
            ILLUM_ZONE_RGB *pIllumZoneRGB = (ILLUM_ZONE_RGB *)pIllumZone;
            LW2080_CTRL_PMGR_ILLUM_ZONE_CTRL_MODE_PIECEWISE_LINEAR_RGB
                 *pPcLnrRGBParams = &pIllumZoneRGB->ctrlModeParams.piecewiseLinearRGB;

            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(2), pPcLnrRGBParams->rgbParams[0].colorR | (pPcLnrRGBParams->rgbParams[0].colorG << 8) |
                    (pPcLnrRGBParams->rgbParams[0].colorB << 16) | (pPcLnrRGBParams->rgbParams[0].brightnessPct << 24));
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(3), pPcLnrRGBParams->rgbParams[1].colorR | (pPcLnrRGBParams->rgbParams[1].colorG << 8) |
                    (pPcLnrRGBParams->rgbParams[1].colorB << 16) | (pPcLnrRGBParams->rgbParams[1].brightnessPct << 24));
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(4), pPcLnrRGBParams->riseTimems);
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(5), pPcLnrRGBParams->fallTimems);
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(6), pPcLnrRGBParams->ATimems);
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(7), pPcLnrRGBParams->BTimems);
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(8), pPcLnrRGBParams->grpIdleTimems);
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(9), pPcLnrRGBParams->grpIdleTimems);
            break;
        }
    }
}
