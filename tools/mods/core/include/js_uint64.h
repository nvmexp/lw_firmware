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

#include "core/include/jscript.h"

class JsUINT64
{
public:
    JsUINT64() = default;
    JsUINT64(UINT64 value) : m_Value(value) {}

    RC CreateJSObject(JSContext* cx);
    JSObject* GetJSObject() const { return m_pJsUINT64Obj; }
    void SetValue(UINT64 val) { m_Value = val; }
    UINT64 GetValue() const { return m_Value; }

    UINT32 GetLsb() const;
    UINT32 GetMsb() const;
    RC SetLsb(UINT32 lsb);
    RC SetMsb(UINT32 msb);

private:
    UINT64 m_Value = 0;
    JSObject* m_pJsUINT64Obj = nullptr;
};
