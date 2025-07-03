/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_CLOCKMON_H
#define CLK_CLOCKMON_H

#include "g_clk_clockmon.h"

#ifndef G_CLK_CLOCKMON_H
#define G_CLK_CLOCKMON_H

/*!
 * @file clk_clockmon.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Enumeration of register types used to control clock monitors.
 */
#define CLK_CLOCK_MON_REG_TYPE_FMON_THRESHOLD_HIGH            0x00
#define CLK_CLOCK_MON_REG_TYPE_FMON_THRESHOLD_LOW             0x01
#define CLK_CLOCK_MON_REG_TYPE_FMON_REF_WINDOW_COUNT          0x02
#define CLK_CLOCK_MON_REG_TYPE_FMON_REF_WINDOW_DC_CHECK_COUNT 0x03
#define CLK_CLOCK_MON_REG_TYPE_FMON_CONFIG                    0x04
#define CLK_CLOCK_MON_REG_TYPE_FMON_ENABLE_STATUS             0x05
#define CLK_CLOCK_MON_REG_TYPE_FMON_FAULT_STATUS              0x06
#define CLK_CLOCK_MON_REG_TYPE_FMON_CLEAR_COUNTER             0x07
#define CLK_CLOCK_MON_REG_TYPE__COUNT                         0x08
#define CLK_CLOCK_MON_REG_TYPE__ILWALID                       0xFF

/*!
 * Structure to hold the map of ADC ID and its register addresses
 */
typedef struct
{
    /*!
     * The global ID @ref  LW2080_CTRL_CLK_DOMAIN_*
     */
    LwU32    clkApiDomain;

    /*!
     * An array of register addresses indexed by register type enum.
     */
    LwU32   regAddr[CLK_CLOCK_MON_REG_TYPE__COUNT];
} CLK_CLOCK_MON_ADDRESS_MAP;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS clkClockMonsProgram(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *pStepClkMon)
    GCC_ATTRIB_SECTION("imem_libClockMon", "clkClockMonsProgram");
FLCN_STATUS clkClockMonsHandlePrePostPerfChangeNotification(LwBool bIsPreChange)
    GCC_ATTRIB_SECTION("imem_libClockMon", "clkClockMonsHandlePrePostPerfChangeNotification");
mockable FLCN_STATUS clkClockMonsThresholdEvaluate(LW2080_CTRL_CLK_CLK_DOMAIN_LIST * const pClkDomainList,
                                                   LW2080_CTRL_VOLT_VOLT_RAIL_LIST * const pVoltRailList,
                                                   LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *pStepClkMon)
    GCC_ATTRIB_SECTION("imem_libClockMon", "clkClockMonsThresholdEvaluate");
FLCN_STATUS clkClockMonSanityCheckFaultStatusAll(void)
    GCC_ATTRIB_SECTION("imem_libClockMon", "clkClockMonSanityCheckFaultStatusAll");

/* ------------------------ Include Derived Types -------------------------- */

#endif // G_CLK_CLOCKMON_H
#endif // CLK_CLOCKMON_H
