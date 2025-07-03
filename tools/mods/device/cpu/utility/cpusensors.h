/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "core/include/rc.h"

namespace CpuSensors
{
    RC GetTccC(UINT64 *pTcc);
    RC GetTempC(UINT32 cpuid, INT64 *pTcc);
    RC GetPkgTempC(UINT32 cpuid, INT64 *pTcc);
    RC GetPkgPowermW(UINT32 cpuid, UINT64 *pPower, UINT32 pollPeriodMs = 10);
};
