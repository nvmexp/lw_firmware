/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLSMCENGINE_H
#define INCLUDED_UTLSMCENGINE_H

#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/smc/smcengine.h"
#include <memory>
class UtlGpuPartition;
class UtlGpuRegCtrl;

// This class represents the C++ backing for the UTL Python SmcEngine object.
// This class is a wrapper around SmcEngine.
//
class UtlSmcEngine
{
public:
    static void Register(py::module module);
    
    static UtlSmcEngine* Create(UtlGpuPartition * pUtlGpuPartition, SmcEngine * pSmcEngine);
    static UtlSmcEngine* GetFromSmcEnginePtr(SmcEngine* pSmcEngine);
    static void FreeSmcEngines();
    static void FreeSmcEngine(SmcEngine* pSmcEngine);

    SmcResourceController::SYS_PIPE GetSysPipe() const { return m_pSmcEngine->GetSysPipe();  }
    string GetName() const { return m_pSmcEngine->GetName(); }
    UINT32 GetGpcCount() const { return m_pSmcEngine->GetGpcCount();  }
    UtlGpuPartition * GetGpuPartition() const { return m_pUtlGpuPartition;  }
    SmcEngine * GetSmcEngineRawPtr() {return m_pSmcEngine;}

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlSmcEngine() = delete;
    UtlSmcEngine(UtlSmcEngine&) = delete;
    UtlSmcEngine &operator=(UtlSmcEngine&) = delete;

    // The following functions can be called from Python
    UtlGpuRegCtrl* GetRegCtrl();
    UINT32 GetSysPipePy();
    UINT32 GetGpcMaskPy();

private:
    UtlSmcEngine(UtlGpuPartition * pGpuPartition, SmcEngine * pSmcEngine);
    static map<SmcEngine*, unique_ptr<UtlSmcEngine>> s_SmcEngines;
    UtlGpuPartition * m_pUtlGpuPartition;
    SmcEngine * m_pSmcEngine;
};

#endif
