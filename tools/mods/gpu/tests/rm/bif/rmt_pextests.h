/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_pextests.h
//! \RM-Test to test fucntionalities of PEX.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "turing/tu102/dev_lw_xve.h"
#include "kepler/gk104/dev_lw_xve1.h"
#include "kepler/gk104/dev_lw_xp.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "ctrl/ctrl2080/ctrl2080bus.h"
#include "ctrl/ctrl208f/ctrl208fbif.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"

#define LW_PCFG1_OFFSET              0x88000    // for xusb and ppc
#define ENABLED_TRUE                 0x80000000
typedef struct
{
    LwU32 pstate;
    LwU32 capFlag;
} PstateCapInfo;

namespace PEXTests
{
    RC ChangePstate(LwU32 targetPstate, GpuSubdevice *pSubdevice, Perf *pPerf);
    RC FectchPstatesCapInfo(GpuSubdevice *pSubDevice, vector<PstateCapInfo> &pstatesCapInfo);
    RC FectchPstateCapInfo(LwU32 pstate, GpuSubdevice *pSubDevice, PstateCapInfo &pstateCapInfo);
}
