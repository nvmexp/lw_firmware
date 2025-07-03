/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   speedomonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "gpu/utility/bglogger.h"
#include "bgmonitorfactory.h"

#include "core/include/jsonlog.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "math.h"
#include "speedomonitor.h"

#include <sstream>

//-----------------------------------------------------------------------------
// Speedo Monitor
//-----------------------------------------------------------------------------
BgMonFactoryRegistrator<SpeedoMonitor> RegisterSpeedoMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::SPEEDO
);
BgMonFactoryRegistrator<SpeedoMonitor> RegisterSpeedoTsoscMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::SPEEDO_TSOSC
);

const char * SpeedoMonitor::speedoNames[] =
{
    "COMP"
  , "BIN"
  , "METAL"
  , "MINI"
  , "TSOSC"
  , "VNSENSE"
  , "NMEAS"
  , "HOLD"
  , "AGING"
  , "BIN_AGING"
  , "AGING2"
  , "CPM"
  , "ALL"
};

//-----------------------------------------------------------------------------
RC SpeedoMonitor::HandleJsParamsPreInit
(
    const JsArray     &params,
    const set<UINT32> &monitorDevices
)
{
    RC rc;
    JavaScriptPtr js;

    if (NUM_PARAMS != params.size())
    {
        Printf(Tee::PriHigh, "SpeedoMonitor : Invalid parameters\n");
        return RC::BAD_PARAMETER;
    }

    UINT32 speedoType;
    INT32 oscIdx;
    UINT32 durationClks;
    INT32 outDiv;
    INT32 mode;
    INT32 refclk;
    INT32 init;
    INT32 finit;

    CHECK_RC(js->FromJsval(params[TYPE], &speedoType));
    CHECK_RC(js->FromJsval(params[OSCIDX], &oscIdx));
    CHECK_RC(js->FromJsval(params[DURATION], &durationClks));
    CHECK_RC(js->FromJsval(params[OUTDIV], &outDiv));
    CHECK_RC(js->FromJsval(params[MODE], &mode));

    JsArray altSettingsArray;
    CHECK_RC(js->FromJsval(params[ALT_PARAMS], &altSettingsArray));
    JSObject *initJsObj;
    CHECK_RC(js->FromJsval(params[INIT_PARAMS], &initJsObj));
    CHECK_RC(js->GetProperty(initJsObj, "init", &init));
    CHECK_RC(js->GetProperty(initJsObj, "finit", &finit));
    CHECK_RC(js->GetProperty(initJsObj, "refclk", &refclk));

    m_CommonSpeedoUtil.SetPart(speedoType);
    m_CommonSpeedoUtil.SetOscIdx(oscIdx);
    m_CommonSpeedoUtil.SetDuration(durationClks);
    m_CommonSpeedoUtil.SetOutDiv(outDiv);
    m_CommonSpeedoUtil.SetInit(init);
    m_CommonSpeedoUtil.SetFinit(finit);
    m_CommonSpeedoUtil.SetRefClk(refclk);

    if (TSOSC == speedoType)
    {
        m_TsoscType = true;
        m_CommonSpeedoUtil.SetAdj(mode);
    }
    else
    {
        m_TsoscType = false;
        m_CommonSpeedoUtil.SetMode(mode);
    }
    JSObject *altSettingsJsObj;
    for (size_t i = 0; i < altSettingsArray.size(); i++)
    {
        INT32 startBit;
        INT32 oscIdx;
        INT32 outDiv;
        INT32 mode;
        CHECK_RC(js->FromJsval(altSettingsArray[i], &altSettingsJsObj));
        CHECK_RC(js->GetProperty(altSettingsJsObj, "startBit", &startBit));
        CHECK_RC(js->GetProperty(altSettingsJsObj, "oscIdx", &oscIdx));
        CHECK_RC(js->GetProperty(altSettingsJsObj, "outDiv", &outDiv));
        CHECK_RC(js->GetProperty(altSettingsJsObj, "mode", &mode));
        m_CommonSpeedoUtil.AddAltSettings(startBit, oscIdx, outDiv, mode);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC SpeedoMonitor::InitPerGpu(GpuSubdevice* pSubdev)
{
    RC rc;

    UINT32 gpuInst = pSubdev->GetGpuInst();
    m_PerGpuSpeedoUtil[gpuInst] = m_CommonSpeedoUtil;
    m_PerGpuSpeedoUtil[gpuInst].SetSubdev(pSubdev);

    m_PerGpuSigma[gpuInst] = 0;
    m_PerGpuNumSpeedos[gpuInst] = 0;

    CHECK_RC(StartJsonDumper());

    return rc;
}

vector<BgMonitor::SampleDesc> SpeedoMonitor::GetSampleDescs(GpuSubdevice* pSubdev)
{
    vector<SampleDesc> descs;

    const UINT32 gpuInst = pSubdev->GetGpuInst();

    vector<SpeedoUtil::SpeedoValues> samples;
    if (m_PerGpuSpeedoUtil[gpuInst].ReadSpeedos(&samples) != RC::OK)
    {
        return descs;
    }

    for (const auto& sample : samples)
    {
        string name = speedoNames[sample.type];
        if (!m_TsoscType)
        {
            name += " Speedo";
        }
        for (UINT32 idx = 0; idx < sample.values.size(); idx++)
        {
            const char* unit = (sample.info[0].clkKHz != 0) ? "MHz" : "";
            descs.push_back({ name, unit, false, FLOAT });
        }
    }

    return descs;
}

RC SpeedoMonitor::GatherData(GpuSubdevice* pSubdev, Sample* pSample)
{
    const UINT32 gpuInst = pSubdev->GetGpuInst();

    vector<SpeedoUtil::SpeedoValues> samples;
    RC rc;
    CHECK_RC(m_PerGpuSpeedoUtil[gpuInst].ReadSpeedos(&samples));

    SendDataToJsonDumper(gpuInst, samples);

    // TODO don't collect all the samples forever!
    m_GpuSensorHistory[gpuInst].push_back(samples);

    for (const auto& sample : samples)
    {
        // values are in 100Hz units, colwert to MHz
        const FLOAT32 multiplier = (sample.info[0].clkKHz != 0) ? 0.0001f : 1.0f;

        for (UINT32 value : sample.values)
        {
            const FLOAT32 adjustedVal = value * multiplier;
            pSample->Push(adjustedVal);

            m_PerGpuSigma[gpuInst] += adjustedVal;
            m_PerGpuNumSpeedos[gpuInst]++;
        }
    }

    return RC::OK;
}

RC SpeedoMonitor::StatsChecker()
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
        Range<UINT32> range;
        for (UINT32 i = 0; i < m_GpuSensorHistory[gpuInst].size(); i++)
        {
            for (UINT32 j = 0; j < m_GpuSensorHistory[gpuInst][i].size(); j++)
            {
                for (UINT32 k = 0; k < m_GpuSensorHistory[gpuInst][i][j].values.size(); k++)
                {
                    UINT32 value = m_GpuSensorHistory[gpuInst][i][j].values[k];
                    stdDev += pow(value - mean, 2);
                    range.Update(value);
                }
            }
        }
        stdDev = sqrt(stdDev / m_PerGpuNumSpeedos[gpuInst]);

        Printf(GetStatsPri(), "\tSpeedoMonitor Stats: "
               "mean speedo %.0f, std dev %.0f, min %u, max %u\n",
               mean, stdDev, range.min, range.max);
    }

    return OK;
}

RC SpeedoMonitor::StartJsonDumper()
{
    if (!JsonLogStream::IsOpen())
    {
        Printf(Tee::PriLow, "JSON log is not open, so we're not dumping speedos to JSON\n");
        return RC::OK;
    }

    if (m_JsonThread != Tasker::NULL_THREAD)
    {
        return RC::OK;
    }

    m_JsonMutex = Tasker::AllocMutex("Speedo Json Dumpaer", Tasker::mtxUnchecked);
    if (!m_JsonMutex)
    {
        return RC::LWRM_OPERATING_SYSTEM;
    }

    m_JsonQueueSema = Tasker::CreateSemaphore(0, "Speedo Json Dumper");
    if (!m_JsonQueueSema)
    {
        Tasker::FreeMutex(m_JsonMutex);
        return RC::LWRM_OPERATING_SYSTEM;
    }

    m_JsonThread = Tasker::CreateThread(JsonDumperFunc, this, 0, "Speedo Json Dumper");
    if (m_JsonThread == Tasker::NULL_THREAD)
    {
        Tasker::DestroySemaphore(m_JsonQueueSema);
        Tasker::FreeMutex(m_JsonMutex);
        return RC::LWRM_OPERATING_SYSTEM;
    }

    return RC::OK;
}

RC SpeedoMonitor::Shutdown()
{
    StickyRC rc;
    if (m_JsonThread != Tasker::NULL_THREAD)
    {
        // To signal the end of the thread, just bump the semaphore without
        // putting any data in the queue
        Tasker::ReleaseSemaphore(m_JsonQueueSema);

        rc = Tasker::Join(m_JsonThread, Tasker::NO_TIMEOUT);

        Tasker::DestroySemaphore(m_JsonQueueSema);
        Tasker::FreeMutex(m_JsonMutex);
    }
    return rc;
}

void SpeedoMonitor::JsonDumperFunc(void* that)
{
    static_cast<SpeedoMonitor*>(that)->JsonDumper();
}

void SpeedoMonitor::JsonDumper()
{
    Printf(Tee::PriDebug, "Speedo Json Dumper: Starting thread\n");

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

    Printf(Tee::PriDebug, "Speedo Json Dumper: Exiting thread\n");
}

void SpeedoMonitor::SendDataToJsonDumper
(
    UINT32 gpuInst,
    const vector<SpeedoUtil::SpeedoValues>& samples
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

void SpeedoMonitor::DumpToJson
(
    UINT32 gpuInst,
    UINT64 timeUs,
    const vector<SpeedoUtil::SpeedoValues>& samples
)
{
    if (!JsonLogStream::IsOpen())
    {
        return;
    }

    JavaScriptPtr pJs;

    JsonItem jsi;

    jsi.SetTag("Speedo");
    jsi.SetField("name", "Speedo");
    jsi.SetField("gpu_inst", gpuInst);
    jsi.SetField("samples", (UINT32)samples.size());

    // for each sample
    JsArray jsaT; // timestamps
    JsArray jsaV; // values
    JsArray jsaChiplet;
    JsArray jsaStartBit;
    JsArray jsaChainId;
    JsArray jsaOscIdx;
    JsArray jsaOutDiv;
    JsArray jsaMode;
    JsArray jsaAdj;
    JsArray jsaInit;
    JsArray jsaFinit;
    JsArray jsaCount;
    JsArray jsaFlags;
    JsArray jsaDcdEn;
    JsArray jsaDcdCount;
    JsArray jsaOutSel;
    JsArray jsaRefClkSel;
    JsArray jsaPactSel;
    JsArray jsaNactSel;
    JsArray jsaSrcClkSel;
    float divisor = 1.0;

    for (UINT32 sample = 0; sample < samples.size(); sample++)
    {
        const UINT32 type = samples[sample].type;
        jsi.SetField("name", speedoNames[type]);
        jsi.SetField("type", type);
        jsi.SetField("jtagClkKHz", samples[sample].info[0].clkKHz);
        if (samples[sample].info[0].clkKHz == 0)
        {
            jsi.SetField("valueUnits", "counts");
            divisor = 1.0;
        }
        else
        {
            // values are in 100Hz units, colwert to MHz
            jsi.SetField("valueUnits", "MHz");
            divisor = 10000.0;
        }

        jsi.SetField("valuesPerSample", (UINT32)samples[sample].values.size());

        jsval tmpjs;
        pJs->ToJsval(timeUs, &tmpjs);
        jsaT.push_back(tmpjs);

        for (UINT32 val = 0; val < samples[sample].values.size(); val++)
        {
            pJs->ToJsval((float)samples[sample].values[val]/divisor, &tmpjs);
            jsaV.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].chainId, &tmpjs);
            jsaChainId.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].startBit, &tmpjs);
            jsaStartBit.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].chiplet, &tmpjs);
            jsaChiplet.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].oscIdx, &tmpjs);
            jsaOscIdx.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].outDiv, &tmpjs);
            jsaOutDiv.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].mode, &tmpjs);
            jsaMode.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].adj, &tmpjs);
            jsaAdj.push_back(tmpjs);

            if (MINI == type)
            {
                pJs->ToJsval(samples[sample].info[val].init, &tmpjs);
                jsaInit.push_back(tmpjs);

                pJs->ToJsval(samples[sample].info[val].finit, &tmpjs);
                jsaFinit.push_back(tmpjs);

                pJs->ToJsval(samples[sample].info[val].refClkSel, &tmpjs);
                jsaRefClkSel.push_back(tmpjs);
            }

            pJs->ToJsval(samples[sample].info[val].count, &tmpjs);
            jsaCount.push_back(tmpjs);

            pJs->ToJsval(samples[sample].info[val].flags, &tmpjs);
            jsaFlags.push_back(tmpjs);

            if (AGING == type || AGING2 == type)
            {
                pJs->ToJsval(samples[sample].info[val].dcdEn, &tmpjs);
                jsaDcdEn.push_back(tmpjs);

                pJs->ToJsval(samples[sample].info[val].pactSel, &tmpjs);
                jsaPactSel.push_back(tmpjs);

                pJs->ToJsval(samples[sample].info[val].nactSel, &tmpjs);
                jsaNactSel.push_back(tmpjs);

                pJs->ToJsval(samples[sample].info[val].srcClkSel, &tmpjs);
                jsaSrcClkSel.push_back(tmpjs);
            }

            if (AGING == type)
            {
                pJs->ToJsval(samples[sample].info[val].outSel, &tmpjs);
                jsaOutSel.push_back(tmpjs);
            }

            if (AGING2 == type)
            {
                pJs->ToJsval(samples[sample].info[val].dcdCount, &tmpjs);
                jsaDcdCount.push_back(tmpjs);
            }
        }
        jsi.SetField("values", &jsaV);
        jsi.SetField("rawCount", &jsaCount);
        jsi.SetField("chainId", &jsaChainId);
        jsi.SetField("chiplet", &jsaChiplet);
        jsi.SetField("flags", &jsaFlags);
        jsi.SetField("startBit", &jsaStartBit);
        jsi.SetField("oscIdx", &jsaOscIdx);
        jsi.SetField("outDiv", &jsaOutDiv);
        if (type == TSOSC)
        {
            jsi.SetField("adj", &jsaAdj);
        }
        else
        {
            jsi.SetField("mode", &jsaMode);
        }
        if (MINI == type || VNSENSE == type)
        {
            jsi.SetField("init", &jsaInit);
            jsi.SetField("finit", &jsaFinit);
        }
        if (MINI == type)
        {
            jsi.SetField("refclk", &jsaRefClkSel);
        }
        if (AGING == type || AGING2 == type)
        {
            jsi.SetField("dcdEn", &jsaDcdEn);
            jsi.SetField("pactSel", &jsaPactSel);
            jsi.SetField("nactSel", &jsaNactSel);
            jsi.SetField("srcClkSel", &jsaSrcClkSel);
        }
        if (AGING == type)
        {
            jsi.SetField("outSel", &jsaOutSel);
        }
        if (AGING2 == type)
        {
            jsi.SetField("dcdCount", &jsaDcdCount);
        }
    
        jsi.SetField("timestamps(usec)", &jsaT);
        jsi.WriteToLogfile();
    }
}
