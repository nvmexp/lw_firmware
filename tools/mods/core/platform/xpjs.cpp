/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/xp.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/deprecat.h"

//------------------------------------------------------------------------------
// Javascript linkage
//------------------------------------------------------------------------------

JS_CLASS(Xp);

SObject Xp_Object
(
    "Xp",
    XpClass,
    0,
    0,
    "MODS Cross Platform Interface."
);

C_(Xp_GetDeviceLocalCpus);
static STest Xp_GetDeviceLocalCpus
(
    Xp_Object,
    "GetDeviceLocalCpus",
    C_Xp_GetDeviceLocalCpus,
    4,
    "Get the CPUs closest to the specified PCI device (NUMA systems)."
);

C_(Xp_GetDeviceLocalCpus)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    INT32 domain = 0;
    INT32 bus = 0;
    INT32 device = 0;
    INT32 function = 0;
    JSObject *pCpuMasks;
    if (((NumArguments != 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &domain)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &bus)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &device)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &function)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &pCpuMasks)))
        && ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &bus)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &device)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &function)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &pCpuMasks))))
    {
        JS_ReportError(pContext,
              "Usage: Xp.GetDeviceLocalCpus(domain, bus, dev, func, cpuMasks[])");
        return JS_FALSE;
    }
    if (NumArguments == 4)
    {
        static Deprecation depr("Xp.GetDeviceLocalCpus", "6/1/2015",
                                "Xp.GetDeviceLocalCpus requires PCI domain number as the first argument.\n"
                                "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    RC rc;
    vector<UINT32> cpuMasks;
    C_CHECK_RC(Xp::GetDeviceLocalCpus(domain, bus, device, function, &cpuMasks));

    for (size_t maskIdx = 0; maskIdx < cpuMasks.size(); maskIdx++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pCpuMasks,
                                           static_cast<UINT32>(maskIdx),
                                           cpuMasks[maskIdx]));
    }
    RETURN_RC(rc);
}
