/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJFUSE_H
#define PMU_OBJFUSE_H

/*!
 * @file    pmu_objfuse.h
 */

/* ------------------------ System includes --------------------------------- */
#include "flcntypes.h"
/*
 * Note: include pmu_bar0.h with <angle brackets> so that when we do a unit test
 * build, it includes the unit test version of pmu_bar0.h. Using "" would cause
 * The pmu_bar0.h in the same directory as this header to take precedence.
 */
#include <pmu_bar0.h>

/* ------------------------ Application includes ---------------------------- */
#include "config/g_fuse_hal.h"

/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/*!
 * FUSE object Definition
 */
typedef struct
{
    LwU8    dummy;    // unused -- for compilation purposes only
} OBJFUSE;

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// Consolidate all fuse register R/W to go through a single interface.
#if (PMUCFG_FEATURE_ENABLED(PMU_ALTER_JTAG_CLK_AROUND_FUSE_ACCESS))
LwU32 fuseRead(LwU32 fuseReg)
    GCC_ATTRIB_SECTION("imem_resident", "fuseRead");
void  fuseWrite(LwU32 fuseReg, LwU32 fuseVal)
    GCC_ATTRIB_SECTION("imem_resident", "fuseWrite");
#else
#define fuseRead(fuseReg)               REG_RD32(BAR0, (fuseReg))
#define fuseWrite(fuseReg, fuseVal)     REG_WR32(BAR0, (fuseReg), (fuseVal))
#endif
/* ------------------------ Misc macro definitions -------------------------- */

#endif // PMU_OBJFUSE_H
