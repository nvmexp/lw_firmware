/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADC_REG_MAP_H
#define PMU_CLKADC_REG_MAP_H

/*!
 * @file pmu_clkadc_reg_map.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Forward Definitions ---------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clkadc.h"

/* ------------------------ Macros ----------------------------------------- */
#define CLK_ADC_REG_UNDEFINED                              (0U)

#define CLK_ADC_REG_GET(_pAdcDev,_type)                                  \
    (((CLK_ADC_REG_TYPE_##_type) >= CLK_ADC_REG_TYPE__COUNT) ?           \
       CLK_ADC_REG_UNDEFINED :                                           \
     (ClkAdcRegMap_HAL[(_pAdcDev)->regMapIdx].regAddr[CLK_ADC_REG_TYPE_##_type]))

#define CLK_ADC_LOGICAL_REG_GET(_pAdcDev,_type)                          \
    (((CLK_ADC_REG_TYPE_##_type) >= CLK_ADC_REG_TYPE__COUNT) ?           \
       CLK_ADC_REG_UNDEFINED :                                           \
     (ClkAdcRegMap_HAL[(_pAdcDev)->logicalRegMapIdx].regAddr[CLK_ADC_REG_TYPE_##_type]))

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure to hold the map of ADC ID and its register addresses
 */
typedef struct
{
    /*!
     * The global ID @ref LW2080_CTRL_CLK_ADC_ID_<xyz> of this ADC device
     */
    LwU8    id;

    /*!
     * An array of register addresses indexed by register type enum.
     */
    LwU32   regAddr[CLK_ADC_REG_TYPE__COUNT];
} CLK_ADC_ADDRESS_MAP;

/* ------------------------ External Definitions --------------------------- */
extern const CLK_ADC_ADDRESS_MAP ClkAdcRegMap_HAL[];

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif // PMU_CLKADC_REG_MAP_H
