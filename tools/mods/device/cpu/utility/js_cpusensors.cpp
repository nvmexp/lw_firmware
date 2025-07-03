/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cpusensors.h"
#include "core/include/rc.h"
#include "core/include/jscript.h"
#include "core/include/script.h"

JS_CLASS(CpuSensors);

static SObject CpuSensors_Object
(
   "CpuSensors",
   CpuSensorsClass,
   0,
   0,
   "CpuSensors interface."
);

JS_SMETHOD_LWSTOM(CpuSensors,
                  GetTccC,
                  1,
                  "Get CPU thermal circuit control temp")
{
    STEST_HEADER(1, 1, "Usage: CpuSensors.GetTccC(pTemp)");
    STEST_ARG(0, JSObject*, pTemp);

    RC rc;
    UINT64 temp = 0;
    C_CHECK_RC(CpuSensors::GetTccC(&temp));
    RETURN_RC(pJavaScript->SetElement(pTemp, 0, temp));
}

JS_SMETHOD_LWSTOM(CpuSensors,
                  GetTempC,
                  2,
                  "Get current temp for given CPU index")
{
    STEST_HEADER(2, 2, "Usage: CpuSensors.GetTempC(idx, pTemp)");
    STEST_ARG(0, UINT32, idx);
    STEST_ARG(1, JSObject*, pTemp);

    RC rc;
    INT64 temp = 0;
    C_CHECK_RC(CpuSensors::GetTempC(idx, &temp));
    RETURN_RC(pJavaScript->SetElement(pTemp, 0, temp));
}

JS_SMETHOD_LWSTOM(CpuSensors,
                  GetPkgTempC,
                  2,
                  "Get current temp for package the given CPU idx is in")
{
    STEST_HEADER(2, 2, "Usage: CpuSensors.GetTempC(idx, pTemp)");
    STEST_ARG(0, UINT32, idx);
    STEST_ARG(1, JSObject*, pTemp);

    RC rc;
    INT64 temp = 0;
    C_CHECK_RC(CpuSensors::GetPkgTempC(idx, &temp));
    RETURN_RC(pJavaScript->SetElement(pTemp, 0, temp));
}

JS_SMETHOD_LWSTOM(CpuSensors,
                  GetPkgPowermW,
                  3,
                  "Get current CPU power for package the given CPU idx lies in (in mW)")
{
    STEST_HEADER(2, 3, "Usage: CpuSensors.GetPowermW(idx, pPwr, [PollPeriodMs])");
    STEST_ARG(0, UINT32, idx);
    STEST_ARG(1, JSObject*, pPwr);
    STEST_OPTIONAL_ARG(2, UINT32, PollPeriodMs, 10);

    RC rc;
    UINT64 pwr = 0;
    C_CHECK_RC(CpuSensors::GetPkgPowermW(idx, &pwr, PollPeriodMs));
    RETURN_RC(pJavaScript->SetElement(pPwr, 0, pwr));
}
