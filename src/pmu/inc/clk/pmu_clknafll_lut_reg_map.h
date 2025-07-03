/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_LUT_REG_MAP_H
#define PMU_CLKNAFLL_LUT_REG_MAP_H

/*!
 * @file pmu_clknafll_lut_reg_map.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Forward Definitions ---------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clknafll_lut.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Enumeration of register types used to control NAFLL-s.
 */
#define CLK_NAFLL_REG_TYPE_LUT_WRITE_ADDR                   0U
#define CLK_NAFLL_REG_TYPE_LUT_WRITE_DATA                   1U
#define CLK_NAFLL_REG_TYPE_LUT_CFG                          2U
#define CLK_NAFLL_REG_TYPE_LUT_SW_FREQ_REQ                  3U
#define CLK_NAFLL_REG_TYPE_NAFLL_COEFF                      4U
#define CLK_NAFLL_REG_TYPE_LUT_WRITE_OFFSET_DATA            5U
#define CLK_NAFLL_REG_TYPE_LUT_WRITE_CPM_DATA               6U
#define CLK_NAFLL_REG_TYPE_AVFS_CPM_CLK_CFG                 7U
#define CLK_NAFLL_REG_TYPE_LUT_ADC_OVERRIDE                 8U
#define CLK_NAFLL_REG_TYPE_QUICK_SLOWDOWN_CTRL              9U
#define CLK_NAFLL_REG_TYPE_QUICK_SLOWDOWN_B_CTRL           10U

/*!
 * Version specific max count.
 */
#if (PMUCFG_FEATURE_ENABLED(CLK_NAFLL_ADDRESS_MAP_HAL_V30) || \
     PMUCFG_FEATURE_ENABLED(CLK_NAFLL_ADDRESS_MAP_HAL_V40))
#define CLK_NAFLL_REG_TYPE__COUNT                           11U
#else
#define CLK_NAFLL_REG_TYPE__COUNT                           5U
#endif

#define CLK_NAFLL_REG_UNDEFINED                            (0U)

#define CLK_NAFLL_REG_GET(_pNafllDev, _type)                                  \
    (((CLK_NAFLL_REG_TYPE_##_type) >= CLK_NAFLL_REG_TYPE__COUNT) ?            \
        CLK_NAFLL_REG_UNDEFINED :                                             \
      (ClkNafllRegMap_HAL[(_pNafllDev)->regMapIdx].regAddr[CLK_NAFLL_REG_TYPE_##_type]))

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure to hold the map of NAFLL ID and its register addresses. All
 * these registers should be accessed via FECS bus to be efficient.
 */
typedef struct
{
    /*!
     * The global ID @ref LW2080_CTRL_PERF_NAFLL_ID_<xyz> of this NAFLL device.
     */
    LwU8     id;

    /*!
     * An array of register addresses indexed by register type enum.
     */
    LwU32   regAddr[CLK_NAFLL_REG_TYPE__COUNT];
} CLK_NAFLL_ADDRESS_MAP;

/* ------------------------ External Definitions --------------------------- */
extern const CLK_NAFLL_ADDRESS_MAP    ClkNafllRegMap_HAL[];

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif // PMU_CLKNAFLL_LUT_REG_MAP_H
