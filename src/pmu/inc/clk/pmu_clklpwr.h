/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_clklpwr.h
 * @brief   Common clocks defines needed for lpwr specific code @ref pmu_clklpwr.c
 */

#ifndef PMU_CLKLPWR_H
#define PMU_CLKLPWR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Fields for RM_PMU_MAILBOX_ID_CLK_LOLWS
 *
 * @details These macros define the fields which report lolws information
 *          back to RM via the mailbox register.  The register is:
 *          LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS)
 */
/**@{*/
#define LW_MAILBOX_CLK_LPWR_ERR_STATE                          3:0
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_DOUBLE_BUF             (0x1U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_CLK_CNTR               (0x2U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_ACC_INIT           (0x3U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_CAL                (0x4U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_POWER              (0x5U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_EN_MASK            (0x6U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_HW_REG             (0x7U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_HW_EN_STATE        (0x8U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_IDDQ_POWER_ON      (0x9U)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_REGIME           (0xAU)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_FREQ             (0xBU)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_LUT_SW_STATE     (0xLW)
#define LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_LUT_HW_STATE     (0xDU)
#define LW_MAILBOX_CLK_LPWR_DOMAIN_IDX                         8:4      // BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_xxx)
#define LW_MAILBOX_CLK_LPWR_STATE                             10:9
#define LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT                (0x1U)
#define LW_MAILBOX_CLK_LPWR_STATE_GRRG_INIT                  (0x2U)
#define LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE               (0x3U)
#define LW_MAILBOX_CLK_LPWR_DATA                             31:11
/**@}*/

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS clkLpwrPostInit(void)
    GCC_ATTRIB_SECTION("imem_clkLpwr", "clkLpwrPostInit");

// GPC-RG
FLCN_STATUS clkGrRgDeInit(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_clkLpwr", "clkGrRgDeInit");
FLCN_STATUS clkGrRgInit(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_clkLpwr", "clkGrRgInit");
FLCN_STATUS clkGrRgRestore(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_clkLpwr", "clkGrRgRestore");

/* ------------------------ Include Derived Types --------------------------- */
#endif // PMU_CLKLPWR_H
