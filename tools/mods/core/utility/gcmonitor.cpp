/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

 /**
 * @file   gcmonitor.cpp
 *
 * This file contains a class meant to monitor the
 * Javascript Garbage Collector.
 */

#include "jsapi.h"
#include "jsgc.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include <map>

namespace GCMonitor
{
    void AddVariable(jsval var);
    bool CheckVariable(JSContext* ctx, jsval var);
    void StartMonitor(JSContext* ctx);
    void StopMonitor(JSContext* ctx);

    JSBool GCCallback(JSContext* cx, JSGCStatus status);

    bool started = false;
    std::map<jsval,bool> monitorVars;
    void* monitorVarsMutex = nullptr;
    JSGCCallback oldCallback;
}

void GCMonitor::AddVariable(jsval var)
{
    monitorVars[var] = false;
}

bool GCMonitor::CheckVariable(JSContext* ctx, jsval var)
{
    if (!GCMonitor::started)
    {
        JS_ReportError(ctx, "Must start monitor before checking a variable");
        return false;
    }
    Tasker::AcquireMutex(GCMonitor::monitorVarsMutex);
    std::map<jsval,bool>::iterator it = GCMonitor::monitorVars.find(var);
    bool result = (it != GCMonitor::monitorVars.end() && GCMonitor::monitorVars[var]);
    Tasker::ReleaseMutex(GCMonitor::monitorVarsMutex);
    return result;
}

void GCMonitor::StartMonitor(JSContext* ctx)
{
    if (GCMonitor::started)
    {
        JS_ReportError(ctx, "GCMonitor has already been started");
        return;
    }
    GCMonitor::monitorVarsMutex = Tasker::AllocMutex("GCMonitor::monitorVarsMutex", Tasker::mtxUnchecked);
    GCMonitor::oldCallback = JS_SetGCCallback(ctx, GCMonitor::GCCallback);
    GCMonitor::started = true;
}

void GCMonitor::StopMonitor(JSContext* ctx)
{
    if (!GCMonitor::started)
    {
        JS_ReportError(ctx, "Must start monitor before stopping it");
        return;
    }
    JS_SetGCCallback(ctx, GCMonitor::oldCallback);
    GCMonitor::started = false;
    Tasker::FreeMutex(GCMonitor::monitorVarsMutex);
    GCMonitor::monitorVarsMutex = nullptr;
    GCMonitor::monitorVars.clear();
}

JSBool GCMonitor::GCCallback(JSContext* cx, JSGCStatus status)
{
    if (status == JSGC_END)
    {
        Tasker::AcquireMutex(GCMonitor::monitorVarsMutex);
        for (std::map<jsval,bool>::iterator it = GCMonitor::monitorVars.begin();
             it != GCMonitor::monitorVars.end();
             it++)
        {
            if (!it->second)
            {
                jsval value = it->first;
                void* thing = JSVAL_TO_GCTHING(value);
                uint8 flags = *js_GetGCThingFlags(thing);
                if (flags & GCF_FINAL)
                {
                    //this variable has been finalized, ie. cleaned up
                    it->second = true;
                }
            }
        }
        Tasker::ReleaseMutex(GCMonitor::monitorVarsMutex);
    }
    return JS_TRUE;
}

JS_CLASS(GCMonitor);
static SObject GCMonitor_Object
(
    "GCMonitor",
    GCMonitorClass,
    0,
    0,
    "Garbage Collection Monitor"
);

JS_SMETHOD_LWSTOM(GCMonitor,
                  AddVariable,
                  1,
                  "Add a variable to monitor in the GC")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: GCMonitor.AddVariable(var)");
        return JS_FALSE;
    }

    GCMonitor::AddVariable(pArguments[0]);
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(GCMonitor,
                  CheckVariable,
                  1,
                  "Check if a variable has been cleaned up already")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: GCMonitor.CheckVariable(var)");
        return JS_FALSE;
    }

    bool checkReturn = GCMonitor::CheckVariable(pContext, pArguments[0]);
    JavaScriptPtr pJavaScript;
    if (OK != pJavaScript->ToJsval(checkReturn, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwred in GCMonitor.CheckVariable.");
        *pReturlwalue = 0;
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(GCMonitor,
                  StartMonitor,
                  0,
                  "Start Monitoring the GC")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    GCMonitor::StartMonitor(pContext);
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(GCMonitor,
                  StopMonitor,
                  0,
                  "Stop Monitoring the GC")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    GCMonitor::StopMonitor(pContext);
    return JS_TRUE;
}

