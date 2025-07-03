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

/**
 * @file   speedomonitor.h
 *
 * @brief  Background logging utility for ISMs
 *
 */

#include "bgmonitor.h"
#include "pergpumonitor.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/speedo.h"

#include <deque>

//-----------------------------------------------------------------------------
// Speedo Monitor
//-----------------------------------------------------------------------------
class SpeedoMonitor : public PerGpuMonitor
{
public:
    explicit SpeedoMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Speedo")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override;

    RC HandleJsParamsPreInit(const JsArray     &Param,
                             const set<UINT32> &MonitorDevices) override;
    RC InitPerGpu(GpuSubdevice* pSubdev) override;

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override;
    RC StatsChecker() override;
    RC Shutdown() override;

    enum SpeedoMonParam
    {
        FLAGS,
        TYPE,
        OSCIDX,
        DURATION,
        OUTDIV,
        MODE,
        ADJ = MODE,     // adj & mode use the same parameter index
        ALT_PARAMS,
        INIT_PARAMS,
        NUM_PARAMS
    };

private:
    RC StartJsonDumper();
    static void JsonDumperFunc(void* that);
    void JsonDumper();
    void SendDataToJsonDumper
    (
        UINT32 gpuInst,
        const vector<SpeedoUtil::SpeedoValues>& samples
    );
    void DumpToJson
    (
        UINT32 gpuInst,
        UINT64 timeUs,
        const vector<SpeedoUtil::SpeedoValues>& samples
    );

    static const char * speedoNames[15];

    bool m_TsoscType = false;
    SpeedoUtil m_CommonSpeedoUtil;
    map<UINT32, SpeedoUtil> m_PerGpuSpeedoUtil;

    // For StatsChecker
    map<UINT32, vector<vector<SpeedoUtil::SpeedoValues> > > m_GpuSensorHistory;
    map<UINT32, FLOAT32> m_PerGpuSigma;
    map<UINT32, UINT32> m_PerGpuNumSpeedos;

    // JSON dumper thread
    struct JsonSample
    {
        UINT32                           gpuInst;
        UINT64                           timeUs;
        vector<SpeedoUtil::SpeedoValues> samples;
    };
    Tasker::ThreadID  m_JsonThread    = Tasker::NULL_THREAD;
    void*             m_JsonMutex     = nullptr;
    SemaID            m_JsonQueueSema = nullptr;
    deque<JsonSample> m_JsonQueue;
};
