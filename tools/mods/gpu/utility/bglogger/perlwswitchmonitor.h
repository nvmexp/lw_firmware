/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   perlwswitchmonitor.h
 *
 * @brief  LwLinkDevice background monitor base class.
 *
 */

#pragma once

#ifndef INCLUDED_PERLWLDEVMONITOR_H
#define INCLUDED_PERLWLDEVMONITOR_H

#include "gpu/utility/bglogger.h"
#include "bgmonitor.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "gpu/utility/lwlink/lwlinkdev.h"

class PerLwSwitchMonitor : public BgMonitor
{
public:
    using BgMonitor::BgMonitor;
    virtual ~PerLwSwitchMonitor(){};
    RC InitializeImpl(const vector<UINT32> &Param,
                      const set<UINT32>    &MonitorDevices) override;
    RC InitFromJsImpl(const JsArray        &Param,
                      const set<UINT32>    &MonitorDevices) override;
    virtual RC HandleParamsPreInit(const vector<UINT32> &Param,
                                   const set<UINT32>    &MonitorDevices) { return OK; }
    virtual RC HandleParamsPostInit(const vector<UINT32> &Param,
                                    const set<UINT32>    &MonitorDevices) { return OK; }
    virtual RC HandleJsParamsPreInit(const JsArray        &Param,
                                     const set<UINT32>    &MonitorDevices) { return OK; }
    virtual RC HandleJsParamsPostInit(const JsArray        &Param,
                                      const set<UINT32>    &MonitorDevices) { return OK; }

    virtual RC InitPerLwSwitch(TestDevicePtr pLws) { return RC::OK; }

    bool IsSubdevBased() override { return false; }
    bool IsLwSwitchBased() override { return true; }
    UINT32 GetNumDevices() override { return m_NumLwss; }
    bool IsMonitoring(UINT32 index) override
    {
        return index < m_Lwss.size() && m_Lwss[index];
    }

private:
    RC InitLwss(const set<UINT32>& monitorDevices);

    virtual RC GatherData(TestDevicePtr pDev, Sample* pSample) = 0;
    RC GatherData(UINT32 devIdx, Sample* pSample) override final;

    vector<SampleDesc> GetSampleDescs(UINT32 devIdx) override final;
    virtual vector<SampleDesc> GetSampleDescs(TestDevicePtr pDev) = 0;

    vector<bool> m_Lwss; // vector<bool> most likely uses a bitmask under the hood
    UINT32 m_NumLwss = 0u;
};
#endif
