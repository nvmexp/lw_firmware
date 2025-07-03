/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "jsapi.h"
#include "core/include/rc.h"

#include <string>

class Fuse;

class JsFuse
{
public:
    JsFuse();

    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *      GetJSObject() { return m_pJsFuseObj; }
    void            SetFuseObj(Fuse * pFuse);
    Fuse *          GetFuseObj();
    RC              SetReworkBinaryFilename(const string& filename);

private:
    Fuse *          m_pFuse;
    JSObject *      m_pJsFuseObj;
};
