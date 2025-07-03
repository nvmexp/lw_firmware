/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_JS_PERFMON_H
#define INCLUDED_JS_PERFMON_H

#ifndef INCLUDED_JSAPI_H
#include "jsapi.h"
#define INCLUDED_JSAPI_H
#endif

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

#ifndef INCLUDED_GPUPM_H
#include "gpu/include/gpupm.h"
#endif

using namespace std;

class JsPerfmon
{
public:
    JsPerfmon();

    RC                              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *                      GetJSObject() {return m_pJsPerfmonObj;}
    void                            SetPerfmonObj(GpuPerfmon * pPerfmon);
    GpuPerfmon *                    GetPerfmonObj();
    UINT32                          ExpVecAdd(const GpuPerfmon::Experiment* pExp);
    RC                              ExpVecSet(UINT32 index, const GpuPerfmon::Experiment* pExp);
    const GpuPerfmon::Experiment*   ExpVecGet(UINT32 index);

private:
    GpuPerfmon *                          m_pPerfmon;
    JSObject *                            m_pJsPerfmonObj;
    vector<const GpuPerfmon::Experiment*> m_ExpVector;
};

#endif // INCLUDED_JS_Perfmon_H
