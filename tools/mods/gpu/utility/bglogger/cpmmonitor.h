/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
#include "core/include/ismspeedo.h"

#include <deque>

//-----------------------------------------------------------------------------
// Speedo Monitor
//-----------------------------------------------------------------------------
class CpmMonitor : public PerGpuMonitor
{
public:
    explicit CpmMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Cpm")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override;

    RC HandleJsParamsPreInit(const JsArray     &Param,
                             const set<UINT32> &MonitorDevices) override;
    RC HandleParamsPreInit(const vector<UINT32> &Param,
                                   const set<UINT32>    &MonitorDevices) override;

    RC InitPerGpu(GpuSubdevice* pSubdev) override;

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override;
    RC StatsChecker() override;
    RC Shutdown() override;

    enum CpmMonParam
    {
        FLAGS,      //PerGpuMonitor flags
        PATH_ID,
        SAMPLE_PERIOD,
        NUM_PARAMS
    };
    struct CpmValues
    {
        UINT32 type;
        UINT32 jtagClkKhz;
        vector<UINT32> binCounts;
        vector<FLOAT32> valuesMhz;
        vector<IsmSpeedo::CpmInfo> info;
        CpmValues(UINT32 speedoType, UINT32 jtagClk):type(speedoType), jtagClkKhz(jtagClk){};
    };

private:
    RC StartJsonDumper();
    static void JsonDumperFunc(void* that);
    void JsonDumper();
    void SendDataToJsonDumper
    (
        UINT32 gpuInst,
        const vector<CpmValues>& samples
    );
    void DumpToJson
    (
        UINT32 gpuInst,
        UINT64 timeUs,
        const vector<CpmValues>& samples
    );

    RC ReadCpms
    (
        GpuSubdevice* pSubdev,
        vector<CpmValues> *pCpmSamples
    );

    UINT32 GrayToBinary32(UINT32 num);

    // Input parameters for the CPM programming
    INT32  m_PathId             = 0;
    INT32  m_SamplePeriod       = 15;

    // Internal variables to restore the GPU state.
    bool   m_bAvfsProgrammed    = false;
    UINT32 m_HardMacroUpdate    = 0;
    UINT32 m_ClkCfgClear        = 0;
    GpuSubdevice* m_pSubdev     = nullptr;

    // Internal variable used to callwlate each sample's frequency
    UINT32 m_JtagClkKhz         = 0;

    // For StatsChecker
    map<UINT32, vector<vector<CpmValues> > > m_GpuSensorHistory;
    map<UINT32, FLOAT32> m_PerGpuSigma;
    map<UINT32, UINT32> m_PerGpuNumSpeedos;

    // JSON dumper thread
    struct JsonSample
    {
        UINT32            gpuInst;
        UINT64            timeUs;
        vector<CpmValues> samples;
    };
    Tasker::ThreadID  m_JsonThread    = Tasker::NULL_THREAD;
    void*             m_JsonMutex     = nullptr;
    SemaID            m_JsonQueueSema = nullptr;
    deque<JsonSample> m_JsonQueue;
};
