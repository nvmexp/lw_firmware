/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mgrmgr.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "nbsensors.h"

JS_CLASS_NO_ARGS(NbSensors,
                 "Notebook Sensors",
                 "Usage: new NbSensors()");

CLASS_PROP_READWRITE(NbSensors, NumFans, UINT32, "Number of fans");
CLASS_PROP_READWRITE(NbSensors, FanRpmAclwracy, UINT32, "Fan Rpm Accuracy");

JS_STEST_LWSTOM(NbSensors,
                SetFanPolicyAuto,
                0,
                "Set Fan Policy to Auto")
{
    STEST_HEADER(0, 0, "Usage: NbSensors.SetFanPolicyAuto()");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    RETURN_RC(pNbSensors->SetFanPolicyAuto());
}

JS_STEST_LWSTOM(NbSensors,
                SetFanPolicyManual,
                0,
                "Set Fan Policy to Manual")
{
    STEST_HEADER(0, 0, "Usage: NbSensors.GetFanModeManual()");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    RETURN_RC(pNbSensors->SetFanPolicyManual());
}

JS_STEST_LWSTOM(NbSensors,
                GetFanRpm,
                2,
                "Get Fan RPM")
{
    STEST_HEADER(2, 2, "Usage: NbSensors.GetFanRpm(FanIdx, pRpm)");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    STEST_ARG(0, UINT32, FanIdx);
    STEST_ARG(1, JSObject*, pRpm);

    RC rc;
    UINT32 rpm;
    C_CHECK_RC(pNbSensors->GetFanRpm(FanIdx, &rpm));
    RETURN_RC(pJavaScript->SetElement(pRpm, 0, rpm));
}

JS_STEST_LWSTOM(NbSensors,
                GetFanRpms,
                1,
                "Get all the fan RPMs")
{
    STEST_HEADER(1, 1, "Usage: NbSensors.GetFanRpm(pRpms)");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    STEST_ARG(0, JSObject*, pRpms);

    RC rc;
    UINT32 rpm;
    UINT32 numFans = pNbSensors->GetNumFans();
    for (UINT32 idx = 0; idx < numFans; idx++)
    {
        C_CHECK_RC(pNbSensors->GetFanRpm(idx, &rpm));
        C_CHECK_RC(pJavaScript->SetElement(pRpms, idx, rpm));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(NbSensors,
                SetFanRpm,
                2,
                "Get Fan RPM")
{
    STEST_HEADER(2, 2, "Usage: NbSensors.GetFanRpm(FanIdx, Rpm)");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    STEST_ARG(0, UINT32, FanIdx);
    STEST_ARG(1, UINT32, Rpm);

    RETURN_RC(pNbSensors->SetFanRpm(FanIdx, Rpm));
}

JS_STEST_LWSTOM(NbSensors,
                GetCpuVrTempC,
                1,
                "Get current CPU VRAM temp")
{
    STEST_HEADER(1, 1, "Usage: NbSensors.GetCpuVrTempC(pTemp)");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    STEST_ARG(0, JSObject*, pTemp);

    RC rc;
    INT32 temp;
    C_CHECK_RC(pNbSensors->GetCpuVrTempC(&temp));
    RETURN_RC(pJavaScript->SetElement(pTemp, 0, temp));
}

JS_STEST_LWSTOM(NbSensors,
                GetGpuVrTempC,
                1,
                "Get current GPU VRAM temp")
{
    STEST_HEADER(1, 1, "Usage: NbSensors.GetGpuVrTempC(pTemp)");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    STEST_ARG(0, JSObject*, pTemp);

    RC rc;
    INT32 temp;
    C_CHECK_RC(pNbSensors->GetGpuVrTempC(&temp));
    RETURN_RC(pJavaScript->SetElement(pTemp, 0, temp));
}

JS_STEST_LWSTOM(NbSensors,
                GetDdrTempC,
                1,
                "Get current DDR temp")
{
    STEST_HEADER(1, 1, "Usage: NbSensors.GetDdrTempC(pTemp)");
    STEST_PRIVATE(NbSensors, pNbSensors, "NbSensors");
    STEST_ARG(0, JSObject*, pTemp);

    RC rc;
    INT32 temp;
    C_CHECK_RC(pNbSensors->GetDdrTempC(&temp));
    RETURN_RC(pJavaScript->SetElement(pTemp, 0, temp));
}
