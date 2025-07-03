/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef MAIN_INIT_API_H
#define MAIN_INIT_API_H

/*!
 * @file main_init_api.h
 */
#include "g_main_init_api.h"


#ifndef G_MAIN_INIT_API_H
#define G_MAIN_INIT_API_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
int initPmuApp(RM_PMU_CMD_LINE_ARGS *pArgs)
    GCC_ATTRIB_SECTION("imem_init", "initPmuApp");

/*! 
 * Engines...
 */
FLCN_STATUS constructAp(void)           GCC_ATTRIB_SECTION("imem_init", "constructAp");
FLCN_STATUS constructBif(void)          GCC_ATTRIB_SECTION("imem_init", "constructBif");
mockable FLCN_STATUS constructClk(void)          GCC_ATTRIB_SECTION("imem_init", "constructClk");
FLCN_STATUS constructDisp(void)         GCC_ATTRIB_SECTION("imem_init", "constructDisp");
FLCN_STATUS constructEi(void)           GCC_ATTRIB_SECTION("imem_init" ,"constructEi");
FLCN_STATUS constructDfpr(void)         GCC_ATTRIB_SECTION("imem_init" ,"constructDfpr");
mockable FLCN_STATUS constructFb(void)           GCC_ATTRIB_SECTION("imem_init", "constructFb");
FLCN_STATUS constructFifo(void)         GCC_ATTRIB_SECTION("imem_init", "constructFifo");
FLCN_STATUS constructFuse(void)         GCC_ATTRIB_SECTION("imem_init", "constructFuse");
FLCN_STATUS constructGcx(void)          GCC_ATTRIB_SECTION("imem_init", "constructGcx");
FLCN_STATUS constructGr(void)           GCC_ATTRIB_SECTION("imem_init", "constructGr");
FLCN_STATUS constructI2c(void)          GCC_ATTRIB_SECTION("imem_init", "constructI2c");
mockable FLCN_STATUS constructIc(void)           GCC_ATTRIB_SECTION("imem_init", "constructIc");
FLCN_STATUS constructLpwr(void)         GCC_ATTRIB_SECTION("imem_init", "constructLpwr");
mockable FLCN_STATUS constructLsf(void)          GCC_ATTRIB_SECTION("imem_init", "constructLsf");
FLCN_STATUS constructMs(void)           GCC_ATTRIB_SECTION("imem_init", "constructMs");
FLCN_STATUS constructMutex(void)        GCC_ATTRIB_SECTION("imem_init", "constructMutex");
FLCN_STATUS constructNne(void)          GCC_ATTRIB_SECTION("imem_init", "constructNne");
mockable FLCN_STATUS constructPerf(void)         GCC_ATTRIB_SECTION("imem_init", "constructPerf");
FLCN_STATUS constructPerfCf(void)       GCC_ATTRIB_SECTION("imem_init", "constructPerfCf");
mockable FLCN_STATUS constructPerfDaemon(void)   GCC_ATTRIB_SECTION("imem_init", "constructPerfDaemon");
FLCN_STATUS constructPg(void)           GCC_ATTRIB_SECTION("imem_init", "constructPg");
FLCN_STATUS constructPmgr(void)         GCC_ATTRIB_SECTION("imem_init", "constructPmgr");
mockable FLCN_STATUS constructPmu(void)          GCC_ATTRIB_SECTION("imem_init", "constructPmu");
FLCN_STATUS constructPsi(void)          GCC_ATTRIB_SECTION("imem_init", "constructPsi");
FLCN_STATUS constructRppg(void)         GCC_ATTRIB_SECTION("imem_init", "constructRppg");
FLCN_STATUS constructSmbpbi(void)       GCC_ATTRIB_SECTION("imem_init", "constructSmbpbi");
FLCN_STATUS constructSpi(void)          GCC_ATTRIB_SECTION("imem_init", "constructSpi");
mockable FLCN_STATUS constructTherm(void)        GCC_ATTRIB_SECTION("imem_init", "constructTherm");
mockable FLCN_STATUS constructTimer(void)        GCC_ATTRIB_SECTION("imem_init", "constructTimer");
FLCN_STATUS constructVbios(void)        GCC_ATTRIB_SECTION("imem_init", "constructVbios");
mockable FLCN_STATUS constructVolt(void)         GCC_ATTRIB_SECTION("imem_init", "constructVolt");

/*!
 * System tasks...
 */
mockable FLCN_STATUS cmdmgmtPreInitTask(void)        GCC_ATTRIB_SECTION("imem_init", "cmdmgmtPreInitTask");
mockable FLCN_STATUS idlePreInitTask(void)           GCC_ATTRIB_SECTION("imem_init", "idlePreInitTask");
mockable FLCN_STATUS watchdogPreInitTask(void)       GCC_ATTRIB_SECTION("imem_init", "watchdogPreInitTask");

/*!
 * Application tasks...
 */
FLCN_STATUS acrPreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "acrPreInitTask");
FLCN_STATUS bifPreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "bifPreInitTask");
FLCN_STATUS dispPreInitTask(void)           GCC_ATTRIB_SECTION("imem_init", "dispPreInitTask");
FLCN_STATUS gcxPreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "gcxPreInitTask");
FLCN_STATUS hdcpPreInitTask(void)           GCC_ATTRIB_SECTION("imem_init", "hdcpPreInitTask");
FLCN_STATUS i2cPreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "i2cPreInitTask");
FLCN_STATUS lowlatencyPreInitTask(void)     GCC_ATTRIB_SECTION("imem_init", "lowlatencyPreInitTask");
FLCN_STATUS lpwrPreInitTask(void)           GCC_ATTRIB_SECTION("imem_init", "lpwrPreInitTask");
FLCN_STATUS lpwrLpPreInitTask(void)         GCC_ATTRIB_SECTION("imem_init", "lpwrLpPreInitTask");
FLCN_STATUS nnePreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "nnePreInitTask");
FLCN_STATUS pcmPreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "pcmPreInitTask");
mockable FLCN_STATUS perfPreInitTask(void)           GCC_ATTRIB_SECTION("imem_init", "perfPreInitTask");
FLCN_STATUS perfCfPreInitTask(void)         GCC_ATTRIB_SECTION("imem_init", "perfCfPreInitTask");
mockable FLCN_STATUS perfDaemonPreInitTask(void)     GCC_ATTRIB_SECTION("imem_init", "perfDaemonPreInitTask");
FLCN_STATUS perfmonPreInitTask(void)        GCC_ATTRIB_SECTION("imem_init", "perfmonPreInitTask");
FLCN_STATUS pmgrPreInitTask(void)           GCC_ATTRIB_SECTION("imem_init", "pmgrPreInitTask");
FLCN_STATUS seqPreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "seqPreInitTask");
FLCN_STATUS spiPreInitTask(void)            GCC_ATTRIB_SECTION("imem_init", "spiPreInitTask");
mockable FLCN_STATUS thermPreInitTask(void)          GCC_ATTRIB_SECTION("imem_init", "thermPreInitTask");
#endif // G_MAIN_INIT_API_H
#endif // MAIN_INIT_API_H
