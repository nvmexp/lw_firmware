/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   pergpumonitor.h
 *
 * @brief  Background monitor base class.
 *
 */

#pragma once

#ifndef INCLUDED_PERGPUMONITOR_H
#define INCLUDED_PERGPUMONITOR_H

#include "gpu/utility/bglogger.h"
#include "bgmonitor.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"

class PerGpuMonitor : public BgMonitor
{
public:
    using BgMonitor::BgMonitor;
    virtual ~PerGpuMonitor(){};
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

    virtual RC InitPerGpu(GpuSubdevice *pSubdev) = 0;

    bool IsLwSwitchBased() override { return false; }
    bool IsSubdevBased() override { return true; }
    UINT32 GetNumDevices() override { return m_NumGpus; }

protected:
    const vector<bool>& MonitoredGpus() const
    {
        return m_Gpus;
    }
    bool IsMonitoringGpu(UINT32 gpuId) const;

private:
    bool IsMonitoring(UINT32 devInst) override;

    RC InitGpus(const set<UINT32>& monitorDevices);

    vector<SampleDesc> GetSampleDescs(UINT32 devIdx) override final;
    RC GatherData(UINT32 devIdx, Sample* pSample) override final;

    virtual RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) = 0;
    virtual vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) = 0;

    vector<bool> m_Gpus;
    vector<bool> m_Devices;
    UINT32 m_NumGpus = 0u;
};
#endif
