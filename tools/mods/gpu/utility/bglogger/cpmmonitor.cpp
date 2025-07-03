/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   cpmmonitor.cpp
 *
 * @brief  Background logging utility for CPMs.
 *
 */

#include "gpu/utility/bglogger.h"
#include "bgmonitorfactory.h"
#include "core/include/jsonlog.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpuism.h"
#include "core/include/platform.h"
#include "math.h"
#include "cpmmonitor.h"

#include <sstream>

//------------------------------------------------------------------------------------------------
// CPM Monitor
//------------------------------------------------------------------------------------------------
BgMonFactoryRegistrator<CpmMonitor> RegisterCpmMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::SPEEDO_CPM
);

//------------------------------------------------------------------------------------------------
RC CpmMonitor::HandleParamsPreInit
(
    const vector<UINT32> &params,
    const set<UINT32>    &monitorDevices
)
{
    if (NUM_PARAMS != params.size())
    {
        Printf(Tee::PriError, "CpmMonitor : Invalid parameters, %d detected %d required\n",
               static_cast<UINT32>(params.size()), NUM_PARAMS);
        return RC::BAD_PARAMETER;
    }

    m_PathId = params[PATH_ID];
    m_SamplePeriod = params[SAMPLE_PERIOD];

    return RC::OK;
}

//------------------------------------------------------------------------------------------------
RC CpmMonitor::HandleJsParamsPreInit
(
    const JsArray     &params,
    const set<UINT32> &monitorDevices
)
{
    RC rc;
    JavaScriptPtr js;

    if (NUM_PARAMS != params.size())
    {
        Printf(Tee::PriError, "CpmMonitor : Invalid parameters\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(js->FromJsval(params[PATH_ID], &m_PathId));
    CHECK_RC(js->FromJsval(params[SAMPLE_PERIOD], &m_SamplePeriod));

    return rc;
}

//------------------------------------------------------------------------------------------------
RC CpmMonitor::InitPerGpu(GpuSubdevice* pSubdev)
{
    RC rc;
    // We want to  generate a frequency value in Mhz for each sample based on the Jtag clk.
    GpuIsm * pISM       = nullptr;
    CHECK_RC(pSubdev->GetISM(&pISM));
    CHECK_RC(pISM->GetISMClkFreqKhz(&m_JtagClkKhz));

    const UINT32 gpuInst = pSubdev->GetGpuInst();
    m_pSubdev = pSubdev;
    m_PerGpuSigma[gpuInst] = 0;
    m_PerGpuNumSpeedos[gpuInst] = 0;

    CHECK_RC(StartJsonDumper());

    return rc;
}

//------------------------------------------------------------------------------------------------
RC CpmMonitor::ReadCpms(GpuSubdevice* pSubdev, vector<CpmValues> *pCpmValues)
{
    RC rc;
    GpuIsm * pISM       = nullptr;
    RegHal& regs  = pSubdev->Regs();

    CHECK_RC(pSubdev->GetISM(&pISM));

    if (!m_bAvfsProgrammed)
    {
        m_HardMacroUpdate = regs.Read32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CFG_CPM_HARD_MACRO_UPDATE);
        m_ClkCfgClear = regs.Read32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CLK_CFG_CLEAR);

        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CLK_CFG_CLEAR_SET);

        // we need to see the rising edge (0 -> 1) of HARD_MACRO_UPDATE
        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CFG_CPM_HARD_MACRO_UPDATE, 0);
        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CFG_CPM_HARD_MACRO_UPDATE, 1);
        Platform::Delay(10);

        // we need to see the rising edge (0 -> 1) of COMMON_INST_CONFIG
        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CFG_CPM_COMMON_INST_CONFIG, 0);
        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CFG_CPM_COMMON_INST_CONFIG, 1);
        Platform::Delay(1);

        // At this point the CPM & NAFLL logic to boost GPCCLK is made inactive
    }

    // The sequence here is described in
    // https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=2927828&cmtNo=29
    vector<CpmInfo> cpmData;
    CpmInfo params{ 0 };
    params.hdr.version = 3;
    params.hdr.chainId = -1;
    params.hdr.chiplet = -1;
    params.hdr.offset  = -1;
    params.cpm2.hardMacroCtrlOverride = 1;
    cpmData.push_back(params);
    CHECK_RC(pISM->WriteCPMs(cpmData));

    cpmData[0].cpm2.samplingPeriod = m_SamplePeriod;
    CHECK_RC(pISM->WriteCPMs(cpmData));

    cpmData[0].cpm2.spareIn = (m_PathId << 2);
    CHECK_RC(pISM->WriteCPMs(cpmData));

    cpmData[0].cpm2.spareIn = (1 << 7) | (m_PathId << 2);
    CHECK_RC(pISM->WriteCPMs(cpmData));

    cpmData[0].cpm2.skewCharEn = 1;
    CHECK_RC(pISM->WriteCPMs(cpmData));
    Platform::Delay(50);

    CpmValues cpmResults(IsmSpeedo::CPM, m_JtagClkKhz);
    CHECK_RC(pISM->ReadCPMs(&cpmResults.info, cpmData));

    cpmData[0].cpm2.spareIn = (m_PathId << 2);
    cpmData[0].cpm2.hardMacroCtrlOverride = 0;
    cpmData[0].cpm2.skewCharEn = 0;
    CHECK_RC(pISM->WriteCPMs(cpmData));

    // Multiplier to colwert the grayCnt into MHz units.
    const FLOAT32 multiplier =
        (static_cast<FLOAT32>(m_JtagClkKhz) * 0.001f) / (m_SamplePeriod + 1);

    cpmResults.valuesMhz.resize(cpmResults.info.size());
    cpmResults.binCounts.resize(cpmResults.info.size());
    for (size_t idx = 0; idx < cpmResults.info.size(); idx++)
    {
        cpmResults.binCounts[idx] = GrayToBinary32(cpmResults.info[idx].cpm2.grayCnt);
        cpmResults.valuesMhz[idx] =
            ((static_cast<FLOAT32>(cpmResults.binCounts[idx]) * multiplier));
    }
    pCpmValues->push_back(cpmResults);
    return rc;
}

//------------------------------------------------------------------------------------------------
// Colwert the gray code to a binary value.
UINT32 CpmMonitor::GrayToBinary32(UINT32 num)
{
    num ^= num >> 16;
    num ^= num >>  8;
    num ^= num >>  4;
    num ^= num >>  2;
    num ^= num >>  1;
    return num;
}

//------------------------------------------------------------------------------------------------
vector<BgMonitor::SampleDesc> CpmMonitor::GetSampleDescs(GpuSubdevice* pSubdev)
{
    vector<SampleDesc> descs;
    vector<CpmValues> samples;
    if (ReadCpms(pSubdev, &samples) != RC::OK)
    {
        return descs;
    }

    for (const auto& sample : samples)
    {
        for (UINT32 idx = 0; idx < sample.valuesMhz.size(); idx++)
        {
            descs.push_back({ "CPM Speedo", "MHz", false, FLOAT });
        }
    }

    return descs;
}

//------------------------------------------------------------------------------------------------
RC CpmMonitor::GatherData(GpuSubdevice* pSubdev, Sample* pSample)
{
    RC rc;
    const UINT32 gpuInst    = pSubdev->GetGpuInst();
    vector<CpmValues> samples;

    CHECK_RC(ReadCpms(pSubdev, &samples));

    SendDataToJsonDumper(gpuInst, samples);

    // TODO don't collect all the samples forever!
    m_GpuSensorHistory[gpuInst].push_back(samples);

    for (const auto& sample : samples)
    {
        for (const auto& value : sample.valuesMhz)
        {
            pSample->Push(value);
            m_PerGpuSigma[gpuInst] += value;
            m_PerGpuNumSpeedos[gpuInst]++;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------------------------
RC CpmMonitor::StatsChecker()
{
    for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        if (!IsMonitoringGpu(gpuInst))
        {
            continue;
        }

        Printf(GetStatsPri(), "%s : \n", pSubdev->GpuIdentStr().c_str());

        const float mean = m_PerGpuSigma[gpuInst] /
                           static_cast<float>(m_PerGpuNumSpeedos[gpuInst]);
        float stdDev = 0;
        Range<FLOAT32> range;
        for (UINT32 i = 0; i < m_GpuSensorHistory[gpuInst].size(); i++)
        {
            for (UINT32 j = 0; j < m_GpuSensorHistory[gpuInst][i].size(); j++)
            {
                for (UINT32 k = 0; k < m_GpuSensorHistory[gpuInst][i][j].valuesMhz.size(); k++)
                {
                    FLOAT32 value = m_GpuSensorHistory[gpuInst][i][j].valuesMhz[k];
                    stdDev += pow(value - mean, 2);
                    range.Update(value);
                }
            }
        }
        stdDev = sqrt(stdDev / m_PerGpuNumSpeedos[gpuInst]);

        Printf(GetStatsPri(), "\tSpeedoMonitor Stats: "
               "mean CPM %.0f, std dev %.0f, min %1.5f, max %1.5f\n",
               mean, stdDev, range.min, range.max);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------------------------
RC CpmMonitor::StartJsonDumper()
{
    if (!JsonLogStream::IsOpen())
    {
        Printf(Tee::PriLow, "JSON log is not open, so we're not dumping CPMs to JSON\n");
        return RC::OK;
    }

    if (m_JsonThread != Tasker::NULL_THREAD)
    {
        return RC::OK;
    }

    m_JsonMutex = Tasker::AllocMutex("Cpm Json Dumpaer", Tasker::mtxUnchecked);
    if (!m_JsonMutex)
    {
        return RC::LWRM_OPERATING_SYSTEM;
    }

    m_JsonQueueSema = Tasker::CreateSemaphore(0, "Cpm Json Dumper");
    if (!m_JsonQueueSema)
    {
        Tasker::FreeMutex(m_JsonMutex);
        return RC::LWRM_OPERATING_SYSTEM;
    }

    m_JsonThread = Tasker::CreateThread(JsonDumperFunc, this, 0, "Cpm Json Dumper");
    if (m_JsonThread == Tasker::NULL_THREAD)
    {
        Tasker::DestroySemaphore(m_JsonQueueSema);
        Tasker::FreeMutex(m_JsonMutex);
        return RC::LWRM_OPERATING_SYSTEM;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------------------------
RC CpmMonitor::Shutdown()
{
    StickyRC rc;
    if (m_bAvfsProgrammed)
    {
        RegHal& regs  = m_pSubdev->Regs();
        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CFG_CPM_HARD_MACRO_UPDATE, 0);
        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CFG_CPM_HARD_MACRO_UPDATE, m_HardMacroUpdate);
        Platform::Delay(10);
        regs.Write32(MODS_PTRIM_GPC_BCAST_AVFS_CPM_CLK_CFG_CLEAR, m_ClkCfgClear);
    }

    if (m_JsonThread != Tasker::NULL_THREAD)
    {
        // To signal the end of the thread, just bump the semaphore without
        // putting any data in the queue
        Tasker::ReleaseSemaphore(m_JsonQueueSema);

        rc = Tasker::Join(m_JsonThread, Tasker::NO_TIMEOUT);

        Tasker::DestroySemaphore(m_JsonQueueSema);
        Tasker::FreeMutex(m_JsonMutex);

        m_JsonThread = Tasker::NULL_THREAD;
        m_JsonQueueSema = nullptr;
        m_JsonMutex = nullptr;
    }

    return rc;
}

//------------------------------------------------------------------------------------------------
void CpmMonitor::JsonDumperFunc(void* that)
{
    static_cast<CpmMonitor*>(that)->JsonDumper();
}

//------------------------------------------------------------------------------------------------
void CpmMonitor::JsonDumper()
{
    Printf(Tee::PriDebug, "Cpm Json Dumper: Starting thread\n");

    while (Tasker::AcquireSemaphore(m_JsonQueueSema, Tasker::NO_TIMEOUT) == RC::OK)
    {
        JsonSample s;
        {
            Tasker::MutexHolder lock(m_JsonMutex);

            // The end condition for this thread is when the semaphore is signalled
            // but there is no data pushed into the queue
            if (m_JsonQueue.empty())
            {
                return;
            }

            // To avoid blocking the bg monitor thread, we simply pop the data
            // off the queue and we release the mutex guarding the queue so
            // the bg monitor thread can push more data, then we dump the data
            // to JSON.  Dumping ilwolves going round trip through JS multiple
            // times so it's not efficient.
            s = move(m_JsonQueue.front());
            m_JsonQueue.pop_front();
        }

        DumpToJson(s.gpuInst, s.timeUs, s.samples);
    }

    Printf(Tee::PriDebug, "Cpm Json Dumper: Exiting thread\n");
}

//------------------------------------------------------------------------------------------------
void CpmMonitor::SendDataToJsonDumper
(
    UINT32 gpuInst,
    const vector<CpmValues>& samples
)
{
    // JSON dumping lwrrently uses JS to produce the JSON.  Because of this, we
    // cannot simply dump JSON data on the current thread, because this would
    // lock out all the other background monitors, because JS access requires
    // attached thread.  So we put the samples in a queue and we let a separate
    // attached thread do the dumping to JSON.

    if (m_JsonThread == Tasker::NULL_THREAD)
    {
        return;
    }

    JsonSample s;
    s.gpuInst = gpuInst;
    s.timeUs  = Xp::GetWallTimeUS();
    s.samples = samples;

    {
        Tasker::MutexHolder lock(m_JsonMutex);
        m_JsonQueue.push_back(move(s));
    }
    Tasker::ReleaseSemaphore(m_JsonQueueSema);
}

//------------------------------------------------------------------------------------------------
void CpmMonitor::DumpToJson
(
    UINT32 gpuInst,
    UINT64 timeUs,
    const vector<CpmValues>& samples
)
{
    if (!JsonLogStream::IsOpen())
    {
        return;
    }

    JavaScriptPtr pJs;

    JsonItem jsi;

    jsi.SetTag("CPM");
    jsi.SetField("name", "CPM");
    jsi.SetField("gpu_inst", gpuInst);
    jsi.SetField("samples", (UINT32)samples.size());
    jsi.SetField("jtagClkKhz", samples[0].jtagClkKhz);
    jsi.SetField("pathId", m_PathId);
    // for each sample
    JsArray jsaT; // timestamps
    JsArray jsaV; // colwerted grayCnt values to frequency in Mhz
    JsArray jsaBinCnt;
    JsArray jsaVersion;
    JsArray jsaChiplet;
    JsArray jsaStartBit;
    JsArray jsaChainId;
    JsArray jsaGrayCnt;
    JsArray jsaMacroFailureCnt;
    JsArray jsaMiscOut;
    JsArray jsaPathTimingStatus;
    JsArray jsaHardMacroCtrlOverride;
    JsArray jsaHardMacroCtrl;
    JsArray jsaInstanceIdOverride;
    JsArray jsaInstanceId;
    JsArray jsaSamplingPeriod;
    JsArray jsaSkewCharEn;
    JsArray jsaSpareIn;

    for (const auto& sample : samples)
    {
        jsi.SetField("name", "CPM");
        jsi.SetField("type", sample.type);
        jsi.SetField("valuesPerSample", (UINT32)sample.valuesMhz.size());

        jsval tmpjs;
        pJs->ToJsval(timeUs, &tmpjs);
        jsaT.push_back(tmpjs);

        for (UINT32 idx = 0; idx < sample.info.size(); idx++)
        {
            pJs->ToJsval(sample.valuesMhz[idx], &tmpjs);
            jsaV.push_back(tmpjs);

            pJs->ToJsval(sample.binCounts[idx], &tmpjs);
            jsaBinCnt.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].hdr.version, &tmpjs);
            jsaVersion.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].hdr.chainId, &tmpjs);
            jsaChainId.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].hdr.offset, &tmpjs);
            jsaStartBit.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].hdr.chiplet, &tmpjs);
            jsaChiplet.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.grayCnt, &tmpjs);
            jsaGrayCnt.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.macroFailureCnt, &tmpjs);
            jsaMacroFailureCnt.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.miscOut, &tmpjs);
            jsaMiscOut.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.pathTimingStatus, &tmpjs);
            jsaPathTimingStatus.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.hardMacroCtrlOverride, &tmpjs);
            jsaHardMacroCtrlOverride.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.hardMacroCtrl, &tmpjs);
            jsaHardMacroCtrl.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.instanceIdOverride, &tmpjs);
            jsaInstanceIdOverride.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.instanceId, &tmpjs);
            jsaInstanceId.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.samplingPeriod, &tmpjs);
            jsaSamplingPeriod.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.skewCharEn, &tmpjs);
            jsaSkewCharEn.push_back(tmpjs);

            pJs->ToJsval(sample.info[idx].cpm2.spareIn, &tmpjs);
            jsaSpareIn.push_back(tmpjs);
        }
    }
    jsi.SetField("version", &jsaVersion);
    jsi.SetField("chainId", &jsaChainId);
    jsi.SetField("chiplet", &jsaChiplet);
    jsi.SetField("startBit", &jsaStartBit);
    jsi.SetField("grayCnt", &jsaGrayCnt);
    jsi.SetField("macroFailureCnt", &jsaMacroFailureCnt);
    jsi.SetField("miscOut", &jsaMiscOut);
    jsi.SetField("pathTimingStatus", &jsaPathTimingStatus);
    jsi.SetField("hardMacroCtrlOverride", &jsaHardMacroCtrlOverride);
    jsi.SetField("hardMacroCtrl", &jsaHardMacroCtrl);
    jsi.SetField("instanceIdOverride", &jsaInstanceIdOverride);
    jsi.SetField("instanceId", &jsaInstanceId);
    jsi.SetField("samplingPeriod", &jsaSamplingPeriod);
    jsi.SetField("skewCharEn", &jsaSkewCharEn);
    jsi.SetField("spareIn", &jsaSpareIn);
    jsi.SetField("binCnt", &jsaBinCnt);
    jsi.SetField("values(Mhz)", &jsaV);
    jsi.SetField("timestamps(usec)", &jsaT);
    jsi.WriteToLogfile();
}
