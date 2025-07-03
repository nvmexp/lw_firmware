/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Prafull Gupta
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_CLK3_H
#define CLK3_CLK3_H

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "pmu_bar0.h"
#include "clk3/clksignal.h"
#include "g_pmurmifrpc.h"

/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief   Register I/O
 *
 * @details These aliases are used to read/write LW_PVTRIM_xxx or LW_PTRIM_xxx
 *          registers throughout Clocks 3.x.  This is useful so that we can
 *          quickly choose among BAR0 v. FECS with or without stall.
 *
 * @warning It is critically important that all Clocks 3.x code use the same
 *          mechanism to avoid out-of-order register I/O.
 *
 *          The rare exception reading outside LW_PVTRIM_xxx or LW_PTRIM_xxx
 *          when ordering does not matter.  For example, clkConstruct_SchematicDag
 *          may read LW_PFB_xxx to determine the memory type.
 *
 *          For BAR0:                   BAR0_REG_RD32   BAR0_REG_WR32
 *          For FECS without stall:     FECS_REG_RD32   FECS_REG_WR32
 *          For FECS with stall:        FECS_REG_RD32   FECS_REG_WR32_STALL
 */
/**@{*/
#define CLK_REG_RD32 FECS_REG_RD32
#define CLK_REG_WR32 FECS_REG_WR32
/**@}*/

/*!
 * @brief   Fields for RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS
 *
 * @details These macros define the fields which report lolws information
 *          back to RM via the mailbox regrster.  The register is:
 *          LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS)
 */
/**@{*/
#define LW_MAILBOX_CLK_ERROR_LOLWS_STATUS                         7:0      // FLCN_STATUS
#define LW_MAILBOX_CLK_ERROR_LOLWS_STATUS_OK                      FLCN_OK
#define LW_MAILBOX_CLK_ERROR_LOLWS_DOMAIN_IDX                     12:8      // BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_xxx)
#define LW_MAILBOX_CLK_ERROR_LOLWS_ACTION                         14:13
#define LW_MAILBOX_CLK_ERROR_LOLWS_ACTION_CONFIG                  0x00000000
#define LW_MAILBOX_CLK_ERROR_LOLWS_ACTION_PROGRAM                 0x00000001
#define LW_MAILBOX_CLK_ERROR_LOLWS_ACTION_READ                    0x00000002
#define LW_MAILBOX_CLK_ERROR_LOLWS_ACTION_FREQ_DOMAIN_NULL        0x00000003
// In MHz precision, with 13 bits we can represent a maximum of 16 GHz which is sufficient for now
#define LW_MAILBOX_CLK_ERROR_LOLWS_TARGET_FREQ_MHz                31:19
/**@}*/

/*!
 * @brief   Fields for RM_PMU_MAILBOX_ID_CLK_ERROR_INFO
 *
 * @details These macros define the fields which report any additional clocks
 *          information back to RM via the mailbox regrster.  The register is:
 *          LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_INFO)
 */
/**@{*/
#define LW_MAILBOX_CLK_ERROR_INFO_FIELD0                           7:0
#define LW_MAILBOX_CLK_ERROR_INFO_FIELD1                           15:8
#define LW_MAILBOX_CLK_ERROR_INFO_FIELD2                           23:16
#define LW_MAILBOX_CLK_ERROR_INFO_FIELD3                           31:24
/**@}*/


/* ------------------------ Datatypes -------------------------------------- */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Public thread-safe interface
 * @public
 */
FLCN_STATUS clkReadDomain_AnyTask(LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM *pDomainItem)
    GCC_ATTRIB_SECTION("imem_libClkReadAnyTask", "clkReadDomain_AnyTask");

/*!
 * @brief       PERF_DAEMON-specific interfaces
 * @public
 *
 * @warning     These functions are NOT thread-safe and MUST be called only from
 *              the PERF_DAEMON task.  They use the half of the double buffer
 *              pointed to by CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN and grab a
 *              semaphore only when updating 'ClkFreqDomain::bOutputFlip'.
 */
/**@{*/
FLCN_STATUS clkReadDomainList_PerfDaemon(LW2080_CTRL_CLK_CLK_DOMAIN_LIST *pDomainList)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkReadDomainList_PerfDaemon");
FLCN_STATUS clkProgramDomainList_PerfDaemon(LW2080_CTRL_CLK_CLK_DOMAIN_LIST *pDomainList)
    GCC_ATTRIB_SECTION("imem_clkLibClk3", "clkProgramDomainList_PerfDaemon");
/**@}*/

// Interfaces to get quantized/supported frequencies for a given clock domain
FLCN_STATUS clkQuantizeFreq(LwU32 clkDomain, LwU16 *pFreqMHz, LwBool bFloor)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkQuantizeFreq");
FLCN_STATUS clkGetNextLowerSupportedFreq(LwU32 clkDomain, LwU16 *pFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkGetNextLowerSupportedFreq");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_CLK3_H

