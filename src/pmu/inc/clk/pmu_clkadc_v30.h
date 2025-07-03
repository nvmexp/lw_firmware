/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADC_V30_H
#define PMU_CLKADC_V30_H

/*!
 * @file pmu_clkadc_v30.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing an individual V30 ADC device
 */
typedef struct
{
    /*!
     * Super class representing the ADC device object
     */
    CLK_ADC_DEVICE                             super;

    /*!
     * Static information specific to the V30 ADC device
     */
    LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_V30   infoData;

    /*!
     * Status information specific to the V30 ADC device
     */
    LW2080_CTRL_CLK_ADC_DEVICE_STATUS_DATA_V30 statusData;

    /*!
     * Target ADC code correction offset value computed for ADC runtime
     * calibration. This is callwlated prior to every voltage and temperature
     * change and programmed pre/post voltage/temperature change depending on
     * whether it's increasing/decreasing.
     */
    LwS8                                       targetAdcCodeCorrectionOffset;
} CLK_ADC_DEVICE_V30;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet (clkAdcDevGrpIfaceModel10ObjSetImpl_V30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkAdcDevGrpIfaceModel10ObjSetImpl_V30");
BoardObjIfaceModel10GetStatus     (clkAdcDeviceIfaceModel10GetStatusEntry_V30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcDeviceIfaceModel10GetStatusEntry_V30");
FLCN_STATUS clkAdcCalCoeffCache_V30(CLK_ADC_DEVICE *pAdcDev)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcCalCoeffCache_V30");
FLCN_STATUS clkAdcComputeCodeOffset_V30(CLK_ADC_DEVICE *pAdcDev, LwBool bTempChange, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pTargetVoltList)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcComputeCodeOffset_V30");
FLCN_STATUS clkAdcProgramCodeOffset_V30(CLK_ADC_DEVICE *pAdcDev, LwBool bPreVoltOffsetProg)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcProgramCodeOffset_V30");

#endif // PMU_CLKADC_V30_H
