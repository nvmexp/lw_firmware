/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2013, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/**
 * @file    gpu_stub.h
 * @brief   Stub out gpu functions until I have time to do something better
 */

#ifndef INCLUDED_GPU_STUB_H
#define INCLUDED_GPU_STUB_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_BASIC_STUB_H
#include "bascstub.h"
#endif
#ifndef INCLUDED_DISPLAY_STUB_H
#include "dispstub.h"
#endif
#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

class Display;

namespace Gpu
{
    enum { MaxNumDevices = 1 }; // 7 on GPU
}

class GpuDevice
{
public:
    GpuDevice* LwrrentGpuDevice() { return this; };
    Display* GetDisplay() { return DisplayPtr().Instance(); };
    bool IsInitialized() /* const */ { return true; }

    RC PreVBIOSSetup(UINT32 GpuInstance);
    RC PostVBIOSSetup(UINT32 GpuInstance);
    UINT32 GetRegWriteMask (UINT32 GpuInst, UINT32 Offset) const;
    bool IsEmOrSim();
    bool GrIntrHook(UINT32 GrIdx, UINT32 GrIntr, UINT32 Nstatus, UINT32 Addr, UINT32 DataLo);
    bool DispIntrHook(UINT32 Intr0, UINT32 Intr1);
    RC HandleRmPolicyManagerEvent(UINT32 GpuInst, UINT32 EventId);
    bool RcCheckCallback(UINT32 GpuInstance);
    bool RcCallback(UINT32 GpuInstance, LwRm::Handle hClient,
                    LwRm::Handle hDevice, LwRm::Handle hObject,
                    UINT32 errorLevel, UINT32 errorType,
                    void *pData, void *pRecoveryCallback);
    void StopEngineScheduling(UINT32 GpuInstance, UINT32 EngineId, bool Stop);
    bool CheckEngineScheduling(UINT32 GpuInstance, UINT32 EngineId);

protected:
    GpuDevice();

private:
    friend class GpuPtr;

    BasicStub m_Stub;
    string m_Bogus;
};

class GpuPtr
{
public:
    explicit GpuPtr() {}

    static GpuDevice s_Gpu;

    GpuDevice * Instance()   const { return &s_Gpu; }
    GpuDevice * operator->() const { return &s_Gpu; }
    GpuDevice & operator*()  const { return s_Gpu; }
};

#endif // !INCLUDED_GPU_STUB_H
