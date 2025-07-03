/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_lut_reg_map_30.c
 * @brief  Mapping of ADC registers.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
#include "dev_pwr_csb.h"
#include "dev_chiplet_pwr.h"
#include "dev_chiplet_pwr_addendum.h"
#endif // PMU_CLK_ADC_RAM_ASSIST

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Mapping between the ADC ID and the various ADC registers
 * @note From Ampere onwards _ADC_OVERRIDE has been physically moved to LUT
 */
const CLK_ADC_ADDRESS_MAP ClkAdcRegMap_HAL[]
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_ADCS_DMEM_DEFINED))
    GCC_ATTRIB_SECTION("dmem_clkAdcs", "ClkAdcRegMap_HAL") =
#else
    GCC_ATTRIB_SECTION("dmem_perf", "ClkAdcRegMap_HAL") =
#endif
{
    {
        LW2080_CTRL_CLK_ADC_ID_SYS,
        {
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_ACC,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_NUM_SAMPLES,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_MONITOR,
#ifdef LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
#else
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_2,
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_CPWR_THERM_MSVDD_RAM_ASSIST_ADC_CTRL,
            LW_CPWR_THERM_MSVDD_RAM_ASSIST_THRESH,
            LW_CPWR_THERM_MSVDD_RAM_ASSIST_DBG_0,
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC0,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(0),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(0),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(0),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(0),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(0),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(0),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(0),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC1,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(1),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(1),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(1),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(1),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(1),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(1),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(1),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC2,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(2),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(2),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(2),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(2),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(2),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(2),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(2),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC3,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(3),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(3),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(3),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(3),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(3),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(3),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(3),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC4,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(4),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(4),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(4),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(4),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(4),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(4),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(4),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC5,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(5),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(5),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(5),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(5),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(5),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(5),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(5),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC6,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(6),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(6),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(6),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(6),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(6),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(6),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(6),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(6),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(6),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(6),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC7,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(7),
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(7),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(7),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(7),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(7),
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(7),
#else
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL_2(7),
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_ADC_CTRL(7),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_THRESH(7),
            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(7),
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPCS,
        {
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL,
            CLK_ADC_REG_UNDEFINED,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_ACC,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_NUM_SAMPLES,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CAL,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_MONITOR,
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2,
#else
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL_2,
#endif
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            LW_PCHIPLET_PWR_GPCS_LWVDD_RAM_ASSIST_ADC_CTRL,
            LW_PCHIPLET_PWR_GPCS_LWVDD_RAM_ASSIST_THRESH,
            LW_PCHIPLET_PWR_GPCS_LWVDD_RAM_ASSIST_DBG_0,
#endif // PMU_CLK_ADC_RAM_ASSIST
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
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
            CLK_ADC_REG_UNDEFINED,
#endif // PMU_CLK_ADC_RAM_ASSIST
        }
    },
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
