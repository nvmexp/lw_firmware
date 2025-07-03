/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADC_V10_H
#define PMU_CLKADC_V10_H

/*!
 * @file pmu_clkadc_v10.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing an individual V10 ADC Device
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
    LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_V10 data;
} CLK_ADC_DEVICE_V10;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet (clkAdcDevGrpIfaceModel10ObjSetImpl_V10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkAdcDevGrpIfaceModel10ObjSetImpl_V10");
BoardObjIfaceModel10GetStatus (clkAdcDeviceIfaceModel10GetStatusEntry_V10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcDeviceIfaceModel10GetStatusEntry_V10");

#endif // PMU_CLKADC_V10_H
