/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadc_reg_map_10.c
 * @brief  Mapping of ADC registers.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Mapping between the ADC ID and the various ADC registers
 */
const CLK_ADC_ADDRESS_MAP ClkAdcRegMap_HAL[]
    GCC_ATTRIB_SECTION("dmem_perf", "ClkAdcRegMap_HAL") =
{
    {
        LW2080_CTRL_CLK_ADC_ID_SYS,
        {
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_ACC,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_NUM_SAMPLES,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_MONITOR,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC0,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(0),
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC1,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(1),
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC2,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(2),
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC3,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(3),
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC4,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(4),
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC5,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(5),
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPCS,
        {
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_OVERRIDE,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_ACC,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_NUM_SAMPLES,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CAL,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_MONITOR,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_SRAM,
        {
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_OVERRIDE,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_ACC,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_NUM_SAMPLES,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CAL,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_MONITOR,
        }
    },
    // MUST be the last to find the end of array
    {
        LW2080_CTRL_CLK_ADC_ID_MAX,
        {
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
        }
    },
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
