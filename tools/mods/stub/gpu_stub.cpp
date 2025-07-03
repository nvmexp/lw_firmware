/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file    gpu_stub.cpp
 * @brief   Implement class GpuDevice.
 */

#include "gpu_stub.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"

GpuDevice GpuPtr::s_Gpu;

//------------------------------------------------------------------------------
GpuDevice::GpuDevice() : m_Bogus("Stub does not implement this")
{
    m_Stub.SetStubMode(BasicStub::FrankenStub); // Don't assert...yet
    m_Stub.SetDefaultPrintPri(Tee::PriLow);
}

//------------------------------------------------------------------------------
RC GpuDevice::PreVBIOSSetup(UINT32 GpuInstance)
{
    m_Stub.LogCallVoid("GpuDevice::PreVBIOSSetup");
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GpuDevice::PostVBIOSSetup(UINT32 GpuInstance)
{
    m_Stub.LogCallVoid("GpuDevice::PostVBIOSSetup");
    return RC::OK;
}

//-----------------------------------------------------------------------------
UINT32 GpuDevice::GetRegWriteMask (UINT32 GpuInst, UINT32 Offset) const
{
    return m_Stub.LogCallUINT32 ("GpuDevice::GetRegWriteMask");
}

//------------------------------------------------------------------------------
bool GpuDevice::IsEmOrSim()
{
    return m_Stub.LogCallBool("GpuDevice::IsEmOrSim");
}

//------------------------------------------------------------------------------
bool GpuDevice::GrIntrHook
(
    UINT32 GrIdx, UINT32 GrIntr, UINT32 Nstatus, UINT32 Addr, UINT32 DataLo
)
{
    m_Stub.LogCallBool("GpuDevice::GrIntrHook");
    return true;
}

//------------------------------------------------------------------------------
bool GpuDevice::DispIntrHook
(
    UINT32 Intr0,
    UINT32 Intr1
)
{
    m_Stub.LogCallBool("GpuDevice::DispIntrHook");
    return true;
}

//------------------------------------------------------------------------------
RC GpuDevice::HandleRmPolicyManagerEvent(UINT32 GpuInst, UINT32 EventId)
{
    return OK;
}

//------------------------------------------------------------------------------
bool GpuDevice::RcCheckCallback(UINT32 GpuInstance)
{
    m_Stub.LogCallBool("GpuDevice::RcCheckCallback");
    return false;
}

//------------------------------------------------------------------------------
bool GpuDevice::RcCallback
(
    UINT32 GpuInstance,
    LwRm::Handle hClient,
    LwRm::Handle hDevice,
    LwRm::Handle hObject,
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    m_Stub.LogCallBool("GpuDevice::RcCallback");
    return false;
}

//------------------------------------------------------------------------------
void GpuDevice::StopEngineScheduling
(
    UINT32 GpuInstance,
    UINT32 EngineId,
    bool Stop
)
{
}

//------------------------------------------------------------------------------
bool GpuDevice::CheckEngineScheduling
(
    UINT32 GpuInstance,
    UINT32 EngineId
)
{
    return false;
}
