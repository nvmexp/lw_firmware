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

#include "cpusensors.h"

RC CpuSensors::GetTccC(UINT64 *pTcc)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC CpuSensors::GetTempC(UINT32 cpuid, INT64 *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC CpuSensors::GetPkgTempC(UINT32 cpuid, INT64 *pTemp)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC CpuSensors::GetPkgPowermW(UINT32 cpuid, UINT64 *pPower, UINT32 pollPeriodMs)
{
    return RC::UNSUPPORTED_FUNCTION;
}
