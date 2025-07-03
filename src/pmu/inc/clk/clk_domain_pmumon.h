/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    clk_domain_pmumon.h
 * @copydoc clk_domain_pmumon.c
 */

#ifndef CLK_DOMAIN_PMUMON_H
#define CLK_DOMAIN_PMUMON_H

/* ------------------------- System Includes -------------------------------- */
#include "lib/lib_pmumon.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief   Period between samples while in active power callback mode.
 *
 * @note    DLSteer needs the GPC clock at a 20 ms polling period.
 *
 * @todo    Replace hardcoding of 100ms with vbios specified sampling rate.
 */
#define CLK_DOMAINS_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()                   \
    (20000U)

/*!
 * @brief   Period between samples while in low power callback mode.
 *
 * @todo    Replace hardcoding of 1s with vbios specified sampling rate.
 */
#define CLK_DOMAINS_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()                      \
    (1000000U)
/* ------------------------- Types Definitions ------------------------------ */
/*!
 * All state required for CLK_DOMAINS PMUMON feature.
 */
typedef struct _CLK_DOMAINS_PMUMON
{
    /*!
     * Queue descriptor for CLK_DOMAINS PMUMON queue.
     */
    PMUMON_QUEUE_DESCRIPTOR queueDescriptor;

    /*!
     * CLK_DOMAINS boardobjgrp index for GPCCLK.
     */
    LwBoardObjIdx gpcClkIdx;

    /*!
     * CLK_DOMAINS boardobjgrp index for DRAMCLK.
     */
    LwBoardObjIdx dramClkIdx;

    /*!
     * Scratch buffer with sample. Used to relieve stack pressure.
     */
    LW2080_CTRL_CLK_PMUMON_CLK_DOMAINS_SAMPLE sample;
} CLK_DOMAINS_PMUMON;

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Constructor for CLK_DOMAINS PMUMON interface.
 *
 * @return     FLCN_OK if successful.
 */
FLCN_STATUS clkClkDomainsPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_perf", "clkClkDomainsPmumonConstruct");

/*!
 * @brief      Creates and schedules the tmr callback responsible for publishing
 *             PMUMON updates.
 *
 * @return     FLCN_OK on success.
 */
FLCN_STATUS clkClkDomainsPmumonLoad(void)
    GCC_ATTRIB_SECTION("imem_perf", "clkClkDomainsPmumonLoad");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // CLK_DOMAIN_PMUMON_H
