/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/jscript.h"
#include "gpu/perf/zpi.h"

class JsZpiHandler
{
public:
    JsZpiHandler() = default;

    RC CreateJSObject(JSContext* cx);
    JSObject* GetJSObject() const { return m_pJsZpiHandlerObj; }
    ZeroPowerIdle& GetZpi() { return m_Zpi; }
    const ZeroPowerIdle& GetZpi() const { return m_Zpi; }

    RC EnterZPI(UINT32 enterDelayUsec);
    RC ExitZPI();
    RC Cleanup();
    
    SETGET_PROP_LWSTOM(DelayBeforeOOBEntryMs, UINT32);
    SETGET_PROP_LWSTOM(DelayAfterOOBExitMs, UINT32);
    SETGET_PROP_LWSTOM(DelayBeforeRescanMs, UINT32);
    SETGET_PROP_LWSTOM(RescanRetryDelayMs, UINT32);
    SETGET_PROP_LWSTOM(IstMode, bool);
    SETGET_PROP_LWSTOM(DelayAfterPexRstAssertMs, UINT32);
    SETGET_PROP_LWSTOM(DelayBeforePexDeassertMs, UINT32);
    SETGET_PROP_LWSTOM(LinkPollTimeoutMs, UINT32);
    SETGET_PROP_LWSTOM(DelayAfterExitZpiMs, UINT32);
    SETGET_PROP_LWSTOM(IsSeleneSystem, bool);
    
private:
    ZeroPowerIdle m_Zpi;
    JSObject* m_pJsZpiHandlerObj = nullptr;
};
