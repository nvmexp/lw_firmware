/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   perlwswitchmonitor.cpp
 */

#include "gpu/include/testdevicemgr.h"
#include "gpu/utility/bglogger.h"
#include "bgmonitor.h"
#include "bgmonitor.h"
#include "perlwswitchmonitor.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/tee.h"
#include "core/include/mgrmgr.h"
#include <map>
#include <set>

RC PerLwSwitchMonitor::InitLwss(const set<UINT32>& monitorDevices)
{
    auto* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (pTestDeviceMgr)
    {
        TestDevicePtr pLwswDev;
        const UINT32 numDevs = pTestDeviceMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH);
        for (UINT32 lwswIdx = 0; lwswIdx < numDevs; lwswIdx++)
        {
            // Bug 1889126: At the moment, we don't have a way to apply
            // arguments to specific lwswitch devices, so for now we'll
            // always monitor all of them.
            // if (!MonitorDevices.count(lwswIdx))
            //     continue;

            RC rc;
            CHECK_RC(pTestDeviceMgr->GetDevice(TestDevice::TYPE_LWIDIA_LWSWITCH, lwswIdx, &pLwswDev));
            CHECK_RC(InitPerLwSwitch(pLwswDev));

            const UINT32 devInst = pLwswDev->DevInst();
            if (devInst >= m_Lwss.size())
            {
                m_Lwss.resize(devInst + 1, false);
            }
            if (!m_Lwss[devInst])
            {
                ++m_NumLwss;
            }
            m_Lwss[devInst] = true;
        }
    }

    return RC::OK;
}

RC PerLwSwitchMonitor::InitializeImpl(const vector<UINT32>& params,
                                      const set<UINT32>&    monitorDevices)
{
    RC rc;
    CHECK_RC(HandleParamsPreInit(params, monitorDevices));
    CHECK_RC(InitLwss(monitorDevices));
    CHECK_RC(HandleParamsPostInit(params, monitorDevices));
    return RC::OK;
}

RC PerLwSwitchMonitor::InitFromJsImpl(const JsArray&     params,
                                      const set<UINT32>& monitorDevices)
{
    RC rc;
    CHECK_RC(HandleJsParamsPreInit(params, monitorDevices));
    CHECK_RC(InitLwss(monitorDevices));
    CHECK_RC(HandleJsParamsPostInit(params, monitorDevices));
    return RC::OK;
}

RC PerLwSwitchMonitor::GatherData(UINT32 devIdx, Sample* pSample)
{
    const auto pTestDevMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    RC rc;
    TestDevicePtr pDevice;
    CHECK_RC(pTestDevMgr->GetDevice(devIdx, &pDevice));

    return GatherData(pDevice, pSample);
}

vector<BgMonitor::SampleDesc> PerLwSwitchMonitor::GetSampleDescs(UINT32 devIdx)
{
    const auto pTestDevMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pDevice;
    pTestDevMgr->GetDevice(devIdx, &pDevice);
    return GetSampleDescs(pDevice);
}
