/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADC_V30_ISINK_V10_H
#define PMU_CLKADC_V30_ISINK_V10_H

/*!
 * @file pmu_clkadc_v30_isink_v10.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Macros ----------------------------------------- */
#define clkAdcDeviceV30IsinkIfaceModel10GetStatusEntry_V10(pModel10, pPayload)         \
    clkAdcDeviceIfaceModel10GetStatusEntry_V30((pModel10), (pPayload))

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Extends ADC_DEVICE_V30 providing attributes specific to version 10 type of
 * ISINK V30 ADC device (LW2080_CTRL_CLK_ADC_TYPE_V30_ISINK_V10).
 */
typedef struct
{
    /*!
     * Super class representing the V30 ADC Device object
     */
    CLK_ADC_DEVICE_V30  super;

    /*!
     * Static information specific to the ISINK V10 ADC device
     */
    LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_ISINK_V10
                        infoData;
} CLK_ADC_DEVICE_V30_ISINK_V10;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet (clkAdcDevV30IsinkGrpIfaceModel10ObjSetImpl_V10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkAdcDevV30IsinkGrpIfaceModel10ObjSetImpl_V10");
FLCN_STATUS clkAdcV30IsinkCalCoeffCache_V10(CLK_ADC_DEVICE *pAdcDev)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcV30IsinkCalCoeffCache_V10");
FLCN_STATUS clkAdcV30IsinkComputeCodeOffset_V10(CLK_ADC_DEVICE *pAdcDev, LwBool bTempChange, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pTargetVoltList)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcV30IsinkComputeCodeOffset_V10");

#endif // PMU_CLKADC_V30_ISINK_V10_H
