/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2015,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   pergpumonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "gpu/utility/bglogger.h"
#include "bgmonitor.h"
#include "pergpumonitor.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/tee.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/testdevicemgr.h"
#include <map>
#include <set>

RC PerGpuMonitor::InitGpus(const set<UINT32>& monitorDevices)
{
    RC rc;

    for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        // TODO monitorDevices comes from JS.  We should probably make the
        // following changes for it:
        // 1) Use pSubdev->DevInst() below.
        // 2) Modify perlwswitchmonitor.cpp to do the same.
        // 3) Change set<UINT32> to vector<bool>.
        if (!monitorDevices.count(pSubdev->GetParentDevice()->GetDeviceInst()))
        {
            continue;
        }

        CHECK_RC(InitPerGpu(pSubdev));

        const UINT32 gpuInst = pSubdev->GetGpuInst();
        if (gpuInst >= m_Gpus.size())
        {
            m_Gpus.resize(gpuInst + 1, false);
        }
        if (!m_Gpus[gpuInst])
        {
            ++m_NumGpus;
        }
        m_Gpus[gpuInst] = true;

        const UINT32 devInst = pSubdev->DevInst();
        if (devInst >= m_Devices.size())
        {
            m_Devices.resize(devInst + 1, false);
        }
        m_Devices[devInst] = true;
    }

    return RC::OK;
}

bool PerGpuMonitor::IsMonitoringGpu(UINT32 gpuId) const
{
    return (gpuId < m_Gpus.size()) && m_Gpus[gpuId];
}

bool PerGpuMonitor::IsMonitoring(UINT32 devInst)
{
    return (devInst < m_Devices.size()) && m_Devices[devInst];
}

RC PerGpuMonitor::InitializeImpl(const vector<UINT32>& params,
                                 const set<UINT32>&    monitorDevices)
{
    RC rc;
    CHECK_RC(HandleParamsPreInit(params, monitorDevices));
    CHECK_RC(InitGpus(monitorDevices));
    CHECK_RC(HandleParamsPostInit(params, monitorDevices));
    return RC::OK;
}

RC PerGpuMonitor::InitFromJsImpl(const JsArray&     params,
                                 const set<UINT32>& monitorDevices)
{
    RC rc;
    CHECK_RC(HandleJsParamsPreInit(params, monitorDevices));
    CHECK_RC(InitGpus(monitorDevices));
    CHECK_RC(HandleJsParamsPostInit(params, monitorDevices));
    return RC::OK;
}

vector<PerGpuMonitor::SampleDesc> PerGpuMonitor::GetSampleDescs(UINT32 devIdx)
{
    const UINT32 gpuInst = DevMgrMgr::d_TestDeviceMgr->GetDriverId(devIdx);
    GpuSubdevice* const pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(gpuInst);
    if (!pSubdev)
    {
        Printf(Tee::PriError, "Invalid device instance %u\n", devIdx);
        MASSERT(pSubdev);
        return vector<SampleDesc>{ };
    }

    return GetSampleDescs(pSubdev);
}

RC PerGpuMonitor::GatherData(UINT32 devIdx, Sample* pSample)
{
    const UINT32 gpuInst = DevMgrMgr::d_TestDeviceMgr->GetDriverId(devIdx);
    GpuSubdevice* const pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(gpuInst);
    if (!pSubdev)
    {
        Printf(Tee::PriError, "Invalid device instance %u\n", devIdx);
        MASSERT(pSubdev);
        return RC::SOFTWARE_ERROR;
    }

    return GatherData(pSubdev, pSample);
}
