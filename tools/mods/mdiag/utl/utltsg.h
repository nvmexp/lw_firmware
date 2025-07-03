/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLTSG_H
#define INCLUDED_UTLTSG_H

#include "mdiag/tests/gpu/lwgpu_tsg.h"
#include "utlpython.h"
#include "utlsubctx.h"

class Channel;
class UtlChannel;
class UtlTest;

// This class represents the C++ backing for the UTL Python Tsg object.
// This class is a wrapper around LWGpuTsg.
//
class UtlTsg
{
public:
    static void Register(py::module module);

    ~UtlTsg();
    UINT32 GetEngineId() const { return m_LWGpuTsg->GetEngineId(); }
    LwRm::Handle GetVASpaceHandle() const { return m_LWGpuTsg->GetVASpaceHandle(); }
    LwRm *GetLwRmPtr() const { return m_LWGpuTsg->GetRmClient(); }
    GpuDevice *GetGpuDevice() const { return m_LWGpuTsg->GetGpuDevice(); }
    SmcEngine *GetSmcEngine() const { return m_LWGpuTsg->GetSmcEngine(); }
    LWGpuResource *GetLwGpuRes() const { return m_LWGpuTsg->GetLwGpuRes(); }
    LWGpuTsg *GetLWGpuTsg() const { return m_LWGpuTsg; }
    void AddChannel(UtlChannel *pChannel);
    UtlSubCtx* GetSubCtx(py::kwargs kwargs);

    static UtlTsg* Create(LWGpuTsg* lwgpuTsg);
    static UtlTsg* GetFromTsgPtr(LWGpuTsg* lwgpuTsg);
    static void FreeTsgs();
    static void FreeTsg(LWGpuTsg* lwgpuTsg);
    static vector<UtlTsg*> GetEngineTsgs(UINT32 engineId);

    // The following functions are called from the Python interpreter.
    static UtlTsg* CreatePy(py::kwargs kwargs);
    string GetName() const { return m_LWGpuTsg->GetName(); }
    bool IsShared() const { return m_LWGpuTsg->IsShared(); }
    py::list GetChannelsPy();

    // This vector is internal cache
    // All channels which has been registered into this tsg
    vector<UtlChannel *> m_Channels;

    UtlTsg() = delete;
    UtlTsg(UtlTsg&) = delete;
    UtlTsg  &operator=(UtlTsg&) = delete;
private:
    UtlTsg(LWGpuTsg *lwgpuTsg);

    LWGpuTsg *m_LWGpuTsg;

    // If this TSG was created from a UTL script, then this UtlTsg object
    // will own the corresponding LWGpuTsg pointer.
    unique_ptr<LWGpuTsg> m_OwnedTsg;

    // All UtlTsg objects are stored in this list.
    // This list has ownership of the UtlTsg objects.
    static map<LWGpuTsg*, unique_ptr<UtlTsg>> s_Tsgs;

    // This map holds pointers to all TSGs that are created by UTL scripts.
    // The first level of the map is referenced by either a test pointer
    // (for test-specific scripts) or nullptr, for global scripts.
    // The second level of the map is referenced by TSG name.
    static map<UtlTest*, map<string, UtlTsg*>> s_ScriptTsgs;
};

#endif
