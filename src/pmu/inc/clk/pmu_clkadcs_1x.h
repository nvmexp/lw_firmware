
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADCS_1X_H
#define PMU_CLKADCS_1X_H

/*!
 * @file pmu_clkadcs_1x.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clkadc.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */

/*
 * Extends CLK_ADC providing global attributes specific to V10 of ADC devices
 */
typedef struct
{
    /*!
     * Super class representing the global properties and boardobjgrp for ADC
     * devices.
     */
    CLK_ADC        super;

    // Type specific members
} CLK_ADC_V10;

/* ------------------------ Function Prototypes ---------------------------- */
// CLK_ADC_V10 interfaces
ClkAdcDevicesIfaceModel10SetHeader (clkIfaceModel10SetHeaderAdcDevices_1X)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkIfaceModel10SetHeaderAdcDevices_1X");

#endif // PMU_CLKADCS_1X_H
