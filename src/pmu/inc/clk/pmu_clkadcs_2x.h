/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADCS_2X_H
#define PMU_CLKADCS_2X_H

/*!
 * @file pmu_clkadcs_2x.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clkadc.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*
 * Extends CLK_ADC providing global attributes specific to V20 of ADC devices
 */
typedef struct
{
    /*!
     * Super class representing the global properties and boardobjgrp for ADC
     * devices.
     */
    CLK_ADC                                                super;

    /*!
     * Static information specific to the V20 ADC devices group
     */
    RM_PMU_CLK_CLK_ADC_DEVICE_V20_BOARDOBJGRP_SET_HEADER   data;

    /*!
     * Mask of ADCs to be programmed with target ADC correction offset (1-1
     * mapped with ADC index) prior to the voltage change. For each ADC to be
     * programmed, the corresonding bit in this mask will be set and the target
     * ADC code correction will be saved in @ref targetAdcCodeCorrectionOffset
     * stored in individual ADC device structure.
     */
    BOARDOBJGRPMASK_E32                                    preVoltAdcMask;

    /*!
     * Mask of ADCs to be programmed with target ADC correction offset (1-1
     * mapped with ADC index)  either after a temperature change or just after
     * a voltage change. For each ADC to be programmed, the corresonding bit
     * in this mask will be set and the target ADC code correction will be
     * saved in @ref targetAdcCodeCorrectionOffset stored in individual ADC
     * device structure.
     */
    BOARDOBJGRPMASK_E32                                    postVoltOrTempAdcMask;
} CLK_ADC_V20;

/* ------------------------ Function Prototypes ---------------------------- */
// CLK_ADC_V20 interfaces
ClkAdcDevicesIfaceModel10SetHeader (clkIfaceModel10SetHeaderAdcDevices_2X)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkIfaceModel10SetHeaderAdcDevices_2X");

#endif // PMU_CLKADCS_2X_H
