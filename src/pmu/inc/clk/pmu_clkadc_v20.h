/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADC_V20_H
#define PMU_CLKADC_V20_H

/*!
 * @file pmu_clkadc_v20.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the ADC_DEVICE_V20
 *          class type.
 *
 * @return  Pointer to the location of the ADC_DEVICE_V20 class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V20)
#define CLK_ADC_DEVICE_V20_VTABLE() &ClkAdcDeviceV20VirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkAdcDeviceV20VirtualTable;
#else
#define CLK_ADC_DEVICE_V20_VTABLE() NULL
#endif

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing an individual V20 ADC Device
 */
typedef struct
{
    /*!
     * Super class representing the ADC Device object
     */
    CLK_ADC_DEVICE                           super;

    /*!
     * static information specific to the V20 ADC device.
     * TODO : Rename to infoData
     */
    LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_V20 data;

    /*!
     * status information specific to the V20 ADC device.
     */
    LW2080_CTRL_CLK_ADC_DEVICE_STATUS_DATA_V20 statusData;

} ADC_DEVICE_V20, CLK_ADC_DEVICE_V20;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet (clkAdcDevGrpIfaceModel10ObjSetImpl_V20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkAdcDevGrpIfaceModel10ObjSetImpl_V20");
BoardObjIfaceModel10GetStatus (clkAdcDeviceIfaceModel10GetStatusEntry_V20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcDeviceIfaceModel10GetStatusEntry_V20");
FLCN_STATUS clkAdcComputeCodeOffset_V20(CLK_ADC_DEVICE *pAdcDev, LwBool bVFEEvalRequired)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcComputeCodeOffset_V20");

#endif // PMU_CLKADC_V20_H
