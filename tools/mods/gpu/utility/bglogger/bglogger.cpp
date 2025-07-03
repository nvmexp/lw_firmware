/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   bglogger.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "gpu/utility/bglogger.h"

#include "bgmonitor.h"
#include "bgmonitorfactory.h"
#include "inc/bytestream.h"
#include "core/include/cpu.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "gpu/utility/bglogger/speedomonitor.h"
#include <map>
#include <memory>
#include <set>
#include <algorithm>

namespace BgLogger
{
    bool GetVerbose();
    RC   SetVerbose(bool bEnable);

    bool GetDetached();
    RC   SetDetached(bool bDetached);

    bool GetMleOnly();
    RC   SetMleOnly(bool bMleOnly);

    bool d_Verbose = false;
    bool d_MleOnly = false;
}

namespace
{
    class BgLoggerMgr;

    class BgLoggerInstance
    {
        public:
            BgLoggerInstance
            (
                BgLoggerMgr*          pMgr,
                unique_ptr<BgMonitor> pMonitor,
                FLOAT64               readIntervalMs
            )
            : m_pMgr(pMgr)
            , m_MonitorName(pMonitor->GetName())
            , m_pMonitor(move(pMonitor))
            , m_ReadIntervalMs(readIntervalMs)
            {
            }

            RC CacheDevices();
            void DeterminePrintingParams();
            void PrintMleHeaders();
            RC Start();
            RC Stop();
            RC StatsChecker() const { return m_pMonitor->StatsChecker(); }
            RC Shutdown() { return m_pMonitor->Shutdown(); }

            using Sample = BgMonitor::Sample;
            using SampleDesc = BgMonitor::SampleDesc;

            // The printing thread can iterate over collected samples using
            // a range-for loop.  The BgLoggerMgr's m_BgMonitorGuard must be
            // locked for the period of iterating over samples.
            using SampleIter = Sample*;
            SampleIter begin() { return &m_SampleSlots[0]; }
            SampleIter end()   { return &m_SampleSlots[0] + m_NextFreeSlot; }

            RC PrintSamples(UINT32* pNumPrinted);
            RC PrintSamplesToMle(UINT32* pNumPrinted);
            RC LogSamplesToJson();

            // After the samples are printed, they must be discarded using
            // this function.
            void DiscardSamples() { m_NextFreeSlot = 0; }

            const string& GetMonitorName() const { return m_pMonitor->GetName(); }

            bool IsPrinting() const { return m_pMonitor->IsPrinting(); }

        private:
            static void MonitorThreadFunc(void* instanceCookie);
            void MonitorThread();
            Sample& AllocSampleSlot(UINT32 devIdx, UINT64 prevTimeNs);

            BgLoggerMgr*          m_pMgr            = nullptr;
            string                m_MonitorName;
            unique_ptr<BgMonitor> m_pMonitor        = nullptr;
            Tasker::ThreadID      m_MonitorThreadId = Tasker::NULL_THREAD;
            FLOAT64               m_ReadIntervalMs  = 0;
            RC                    m_ThreadExitRc;
            vector<UINT32>        m_DeviceIndices;
            bool                  m_bUsesDevInst    = false;

            struct PrintContext
            {
                string             deviceName;
                vector<SampleDesc> descs;
                bool               printOneLine    = true;
                bool               sameUnits       = true;
                size_t             maxColNameSize  = 0;
                size_t             maxUnitNameSize = 0;
            };

            // Per-device
            vector<PrintContext> m_PrintContexts;

            // We keep collected samples individually for each monitor,
            // because the printing thread will print them grouped by monitor.
            vector<Sample> m_SampleSlots;
            size_t         m_NextFreeSlot = 0;
    };

    class BgLoggerMgr
    {
        public:
            RC Initialize();
            RC Shutdown();
            template<typename TParams>
            RC AddMonitor
            (
                BgMonitorType      type,
                const TParams&     params,
                const set<UINT32>& monitorDevices,
                FLOAT64            readIntervalMs
            );
            RC WaitSlice(FLOAT64 waitTimeMs) const;
            void* GetGuard() const { return m_BgMonitorGuard; }

            SETGET_PROP(PrintIntervalMs, FLOAT64);
            SETGET_PROP(Flush, bool);

            void PauseLogging();
            void UnpauseLogging();
            bool IsPaused() const { return m_PausedCount; }
            bool IsDetached() const { return m_Detached; }
            void SetDetached(bool detached) { m_Detached = detached; }

        private:
            RC AddAndStartMonitor
            (
                unique_ptr<BgMonitor> pNewMonitor,
                FLOAT64               readIntervalMs
            );
            static void PrintThreadFunc(void* instanceCookie);
            void PrintThread();
            RC PrintSamples();

            using InstancePtr = unique_ptr<BgLoggerInstance>;

            void*               m_BgMonitorGuard  = nullptr;
            Tasker::ThreadID    m_PrinterThreadId = Tasker::NULL_THREAD;
            ModsEvent*          m_ExitEvent       = nullptr;
            vector<InstancePtr> m_Monitors;
            RC                  m_ThreadExitRc;
            atomic<UINT32>      m_PausedCount     { 0 };
            atomic<FLOAT64>     m_PrintIntervalMs { 1000.0 };
            atomic<bool>        m_Flush           { false };
            bool                m_Detached        = true;
            bool                m_LogToJson       = false;
    };

    BgLoggerMgr s_Loggers;
}

// This is still used by volterra code in thermsub.cpp
FLOAT64 BgLogger::GetReadIntervalMs()
{
    return 1000;
}

RC BgLoggerMgr::Initialize()
{
    if (m_BgMonitorGuard)
    {
        return RC::OK;
    }

    MASSERT(!m_ExitEvent);
    MASSERT(m_PrinterThreadId == Tasker::NULL_THREAD);

    bool initOK = false;

    DEFER
    {
        if (!initOK)
        {
            if (m_BgMonitorGuard)
            {
                Tasker::FreeMutex(m_BgMonitorGuard);
                m_BgMonitorGuard = nullptr;
            }

            if (m_ExitEvent)
            {
                Tasker::FreeEvent(m_ExitEvent);
                m_ExitEvent = nullptr;
            }
        }
    };

    m_BgMonitorGuard = Tasker::AllocMutex("BgLoggerMgr", Tasker::mtxUnchecked);
    if (!m_BgMonitorGuard)
    {
        return RC::LWRM_OPERATING_SYSTEM;
    }

    m_ExitEvent = Tasker::AllocEvent("BgLoggerMgr::ExitEvent", true);
    if (!m_ExitEvent)
    {
        return RC::LWRM_OPERATING_SYSTEM;
    }

    // Init went well, prevent the events and mutex from being freed by DEFER
    initOK = true;

    return RC::OK;
}

RC BgLoggerMgr::Shutdown()
{
    // Signal all threads to exit
    if (m_ExitEvent)
    {
        Tasker::SetEvent(m_ExitEvent);
    }

    StickyRC rc;

    // Wait for all threads to exit
    if (m_BgMonitorGuard)
    {
        for (const auto& pMon : m_Monitors)
        {
            rc = pMon->Stop();
        }
    }

    // Stop the printing thread
    if (m_PrinterThreadId != Tasker::NULL_THREAD)
    {
        Tasker::Join(m_PrinterThreadId);
        m_PrinterThreadId = Tasker::NULL_THREAD;
        rc = m_ThreadExitRc;
        m_ThreadExitRc.Clear();
    }

    // Print any remaining cached prints in case the printing thread
    // exited before monitor threads and the monitor threads had
    // a chance to gather some data after that.
    // Note: This still references bg monitors for printing,
    //       so don't free the bg monitors yet.
    RC printRc = PrintSamples();
    if (printRc != RC::OK && printRc != RC::EXIT_OK)
    {
        rc = printRc;
    }

    // Print summary/statistics
    if (m_BgMonitorGuard)
    {
        for (const auto& pMon : m_Monitors)
        {
            rc = pMon->StatsChecker();
        }
    }

    // Now free the bg monitors after we've printed all the samples
    m_Monitors.clear();

    // Free resources
    if (m_BgMonitorGuard)
    {
        Tasker::FreeMutex(m_BgMonitorGuard);
        m_BgMonitorGuard = nullptr;
    }
    if (m_ExitEvent)
    {
        Tasker::FreeEvent(m_ExitEvent);
        m_ExitEvent = nullptr;
    }

    return rc;
}

RC BgLoggerMgr::AddAndStartMonitor
(
    unique_ptr<BgMonitor> pNewMonitor,
    FLOAT64               readIntervalMs
)
{
    Tasker::MutexHolder lock(m_BgMonitorGuard);

    m_Monitors.push_back(make_unique<BgLoggerInstance>(
        this,
        move(pNewMonitor),
        readIntervalMs));

    bool success = false;
    DEFER
    {
        if (!success)
        {
            m_Monitors.back()->Shutdown();
            m_Monitors.pop_back();
        }
    };

    auto* const pMonitor = m_Monitors.back().get();

    RC rc;
    CHECK_RC(pMonitor->CacheDevices());

    pMonitor->DeterminePrintingParams();

    pMonitor->PrintMleHeaders();

    if (m_PrinterThreadId == Tasker::NULL_THREAD)
    {
        m_PrinterThreadId = Tasker::CreateThread(PrintThreadFunc, this, 0, "BgLogger Printer");
        if (m_PrinterThreadId == Tasker::NULL_THREAD)
        {
            return RC::LWRM_OPERATING_SYSTEM;
        }
    }

    CHECK_RC(pMonitor->Start());

    success = true;
    return RC::OK;
}

namespace
{
    RC InitMonitor
    (
        unique_ptr<BgMonitor>& pMonitor,
        const vector<UINT32>&  params,
        const set<UINT32>&     monitorDevices
    )
    {
        return pMonitor->Initialize(params, monitorDevices);
    }

    RC InitMonitor
    (
        unique_ptr<BgMonitor>& pMonitor,
        const JsArray&         params,
        const set<UINT32>&     monitorDevices
    )
    {
        return pMonitor->InitFromJs(params, monitorDevices);
    }
}

template<typename TParams>
RC BgLoggerMgr::AddMonitor
(
    BgMonitorType      type,
    const TParams&     params,
    const set<UINT32>& monitorDevices,
    FLOAT64            readIntervalMs
)
{
    if (readIntervalMs <= 0)
    {
        Printf(Tee::PriError, "Read interval %f is not positive\n", readIntervalMs);
        return RC::BAD_PARAMETER;
    }

    auto pNewMonitor = BgMonitorFactories::GetInstance().CreateBgMonitor(type);

    if (!pNewMonitor.get())
    {
        Printf(Tee::PriError, "Cannot add sensor type %d\n", static_cast<UINT32>(type));
        return RC::UNSUPPORTED_FUNCTION;
    }

    const RC initRc = InitMonitor(pNewMonitor, params, monitorDevices);
    MASSERT(initRc != RC::UNSUPPORTED_FUNCTION);
    if (initRc == RC::UNSUPPORTED_FUNCTION)
    {
        Printf(Tee::PriError, "Sensor type %d does not support initializer interface\n",
               static_cast<UINT32>(type));
        return RC::SOFTWARE_ERROR;
    }
    RC rc;
    CHECK_RC(initRc);

    CHECK_RC(AddAndStartMonitor(move(pNewMonitor), readIntervalMs));

    return RC::OK;
}

//! Pauses the current thread for the given amount of time.
//!
//! Also obeys the exit event which signals all threads to exit
//! immediately.
//!
//! @param waitTimeMs Time to pause the thread for
//!
//! Returns RC::OK if the pause was uninterrupted, which means the
//!         caller can continue doing what it's doing in a loop.
//! Returns RC::EXIT_OK if the exit event was signaled and the
//!         caller must stop looping.
//! Returns any other RC when an error oclwrred.
//!
RC BgLoggerMgr::WaitSlice(FLOAT64 waitTimeMs) const
{
    // Wait until we exit or until we reach the desired wait time
    const RC rc = Tasker::WaitOnEvent(m_ExitEvent, waitTimeMs);

    // The full wait time has passed without interruption,
    // notify the caller that he can continue
    if (rc == RC::TIMEOUT_ERROR)
    {
        return RC::OK;
    }

    // The caller will terminate the loop when the exit event was
    // triggered or when there was an error
    return rc == RC::OK ? RC::EXIT_OK : rc;
}

BgLoggerInstance::Sample& BgLoggerInstance::AllocSampleSlot(UINT32 devIdx, UINT64 prevTimeNs)
{
    MASSERT(m_NextFreeSlot <= m_SampleSlots.size());

    // We re-use existing slots rather than reallocating them.
    // This avoids freeing all the memory that was allocated, so that
    // subsequent new samples in the next round until the next print
    // will re-use memory and won't inlwr any runtime penalty for malloc.
    if (m_NextFreeSlot == m_SampleSlots.size())
    {
        m_SampleSlots.emplace_back();
    }

    auto& sampleSlot = m_SampleSlots[m_NextFreeSlot++];

    sampleSlot.mleContext = Mle::Context();
    Tee::SetContextUidAndTimestamp(&sampleSlot.mleContext);
    sampleSlot.mleContext.threadId = m_MonitorThreadId;
    if (m_bUsesDevInst)
    {
        sampleSlot.mleContext.devInst = devIdx;
    }
    sampleSlot.ClearData();
    sampleSlot.devIdx      = devIdx;
    sampleSlot.timeDeltaNs = sampleSlot.mleContext.timestamp - prevTimeNs;
    sampleSlot.pMonitor    = nullptr;

    return sampleSlot;
}

//! Prints any samples which were gathered by monitor threads.
//!
//! Returns RC::EXIT_OK if there were no samples to print.
//! Returns RC::OK if some samples were actually printed.
//! Returns any other RC when an error oclwrred.
//!
RC BgLoggerMgr::PrintSamples()
{
    if (!m_BgMonitorGuard)
    {
        return RC::OK;
    }

    Tasker::MutexHolder lock(m_BgMonitorGuard);

    unsigned numSamples = 0;

    // Print samples to MLE
    for (const auto& pMonInst : m_Monitors)
    {
        UINT32 numPrinted = 0;
        pMonInst->PrintSamplesToMle(&numPrinted);

        // Discard samples early for monitors which are not printed to the normal log
        if (!pMonInst->IsPrinting())
        {
            pMonInst->DiscardSamples();
        }

        numSamples += numPrinted;
    }

    // Exit early if there were no samples to print
    // Note: there were only non-printing monitors, so all their samples
    // have already been discarded.
    if (!numSamples)
    {
        return RC::OK;
    }

    // Print samples to the normal log and discard them
    numSamples = 0;
    for (const auto& pMonInst : m_Monitors)
    {
        if (pMonInst->IsPrinting())
        {
            UINT32 numPrinted = 0;
            RC rc;
            CHECK_RC(pMonInst->PrintSamples(&numPrinted));

            if (m_LogToJson && numPrinted)
            {
                CHECK_RC(pMonInst->LogSamplesToJson());
            }

            pMonInst->DiscardSamples();

            numSamples += numPrinted;
        }
    }

    return numSamples ? RC::OK : RC::EXIT_OK;
}

//! Prints samples from one monitor to normal MODS log
RC BgLoggerInstance::PrintSamples(UINT32* pNumPrinted)
{
    if (m_SampleSlots.empty())
    {
        *pNumPrinted = 0;
        return RC::OK;
    }
    // Gather samples from the same devices together
    stable_sort(begin(), end(), [](const auto& a, const auto& b)
    {
        return a.devIdx < b.devIdx;
    });

    vector<string> linesToPrint;
    string deviceHeader;

    const auto PrintPreviousSamples = [&linesToPrint]()
    {
        for (const auto& line : linesToPrint)
        {
            Printf(Tee::PriNormal, "%s ]\n", line.c_str());
        }
        linesToPrint.clear();
    };

    // prevDevIdx is used to detect when to print a device header.
    // We initialize it to a value different than the device index of
    // the first sample to force printing the device header for the first
    // sample.
    UINT32 prevDevIdx = m_NextFreeSlot ? ~begin()->devIdx : ~0U;

    UINT32 numPrinted = 0;

    for (const auto& sample : *this)
    {
        // If this monitor is not printing or if data capture failed,
        // just ignore this incomplete sample
        BgMonitor* const pMonitor = sample.pMonitor;
        if (!pMonitor)
        {
            continue;
        }

        MASSERT(sample.devIdx < m_PrintContexts.size());
        const PrintContext& ctx = m_PrintContexts[sample.devIdx];

        // Construct new device headers if this sample is for a different
        // device than the previous sample
        if (sample.devIdx != prevDevIdx)
        {
            PrintPreviousSamples();

            const string& deviceName = ctx.deviceName;

            string line;
            if (!deviceName.empty())
            {
                line = deviceName + " ";
            }

            line += GetMonitorName();
            if (deviceName.empty() && m_DeviceIndices.size() > 1)
            {
                line += Utility::StrPrintf("(%u)", sample.devIdx);
            }

            if (ctx.sameUnits && !ctx.descs.empty() && !ctx.descs.front().unit.empty())
            {
                line += " [";
                line += ctx.descs.front().unit;
                line += "]";
            }

            if (ctx.printOneLine)
            {
                line += " : [";
                linesToPrint.push_back(move(line));
            }
            else
            {
                deviceHeader = line + " ";
            }

            prevDevIdx = sample.devIdx;
        }

        // Colwert values for this sample to strings
        vector<string> values;
        RC rc;
        CHECK_RC(pMonitor->GetPrintable(sample, &values));

        // Print subsequent values
        UINT32 descIdx = 0;
        for (const auto& valStr : values)
        {
            const UINT32 lwrColIdx = descIdx;
            const SampleDesc* pDesc = nullptr;
            if (descIdx < ctx.descs.size())
            {
                pDesc = &ctx.descs[descIdx++];
            }
            else if (ctx.descs.size() == 1 && (ctx.descs.front().dataFormat == BgMonitor::INT_ARRAY))
            {
                pDesc = &ctx.descs.front();
            }

            if (ctx.printOneLine)
            {
                string& line = linesToPrint[0];
                line += " ";
                line += valStr;

                if (!ctx.sameUnits && pDesc && !pDesc->unit.empty())
                {
                    line += " ";
                    line += pDesc->unit;
                }
            }
            else
            {
                if (lwrColIdx >= linesToPrint.size())
                {
                    linesToPrint.resize(lwrColIdx + 1);
                }

                string& line = linesToPrint[lwrColIdx];

                if (line.empty())
                {
                    line = deviceHeader;
                    size_t colNameSize = 0;
                    if (pDesc && !pDesc->columnName.empty())
                    {
                        line += pDesc->columnName;
                        colNameSize = pDesc->columnName.size();
                    }

                    if (colNameSize < ctx.maxColNameSize)
                    {
                        line.append(ctx.maxColNameSize - colNameSize, ' ');
                    }

                    if (!ctx.sameUnits)
                    {
                        size_t unitNameSize = 0;
                        if (pDesc && !pDesc->unit.empty())
                        {
                            line += " [";
                            line += pDesc->unit;
                            line += "]";
                            unitNameSize = pDesc->unit.size();
                        }
                        else
                        {
                            // This is for space, [ and ], because these
                            // characters are counted separately from unit name size.
                            line.append(3, ' ');
                        }

                        if (unitNameSize < ctx.maxUnitNameSize)
                        {
                            line.append(ctx.maxUnitNameSize - unitNameSize, ' ');
                        }
                    }

                    line += " : [";
                }

                line += " ";
                line += valStr;
            }
        }

        ++numPrinted;
    }

    PrintPreviousSamples();

    *pNumPrinted = numPrinted;

    return RC::OK;
}

RC BgLoggerInstance::PrintSamplesToMle(UINT32* pNumPrinted)
{
    UINT32 numSamples = 0;

    if (this->m_SampleSlots.empty())
    {
        // Do not access m_SampleSlots[0] in BgLoggerInstance::begin()
        *pNumPrinted = 0;
        return RC::OK;
    }

    for (const auto& sample : *this)
    {
        // Skip incomplete sample
        if (!sample.pMonitor)
        {
            continue;
        }

        ++numSamples;

        RC rc;

        const PrintContext& ctx = m_PrintContexts[sample.devIdx];
        if (ctx.descs.empty())
        {
            continue;
        }

        ByteStream bs;
        auto entry = Mle::Entry::bg_monitor(&bs);
        entry.type(m_pMonitor->GetType());
        // TODO Monitors for "devices" not tracked with devinst should use a separate field.

        const BgMonitor::ValueType firstDataFormat = ctx.descs.front().dataFormat;
        if (firstDataFormat == BgMonitor::INT_ARRAY)
        {
            MASSERT(ctx.descs.size() == 1);

            auto itemIter = sample.begin();
            MASSERT(itemIter != sample.end());

            const auto& item = *itemIter;
            MASSERT(item.type == BgMonitor::INT_ARRAY);

            // TODO add a way to pass this to protobuf without colwerting to vector
            const INT64* pValues = &item.value.intValue[0];
            const vector<INT64> values(pValues, pValues + item.numElems);
            entry.data().array_int(values);

            ++itemIter;
            MASSERT(itemIter == sample.end());
        }
        else if (ctx.descs.size() == 1 &&
                (firstDataFormat == BgMonitor::INT ||
                 firstDataFormat == BgMonitor::FLOAT))
        {
            auto itemIter = sample.begin();
            MASSERT(itemIter != sample.end());
            const BgMonitor::Sample::SampleItem& item = *itemIter;
            MASSERT(item.type == firstDataFormat);

            if (firstDataFormat == BgMonitor::FLOAT)
            {
                MASSERT(item.numElems == 1);
                entry.single_data_float(item.value.floatValue[0], Mle::Output::Force);
            }
            else
            {
                MASSERT(firstDataFormat == BgMonitor::INT);
                MASSERT(item.numElems == 1);
                entry.single_data_int(item.value.intValue[0], Mle::Output::Force);
            }

            ++itemIter;
            MASSERT(itemIter == sample.end());
        }
        else
        {
            UINT32 descIdx = 0;
            bool error = false;
            for (const BgMonitor::Sample::SampleItem& item : sample)
            {
                MASSERT(descIdx < ctx.descs.size());
                const BgMonitor::ValueType type = ctx.descs[descIdx].dataFormat;
                MASSERT(item.type == type);
                ++descIdx;

                switch (type)
                {
                    case BgMonitor::INT:
                        MASSERT(item.numElems == 1);
                        entry.data().value_int(item.value.intValue[0], Mle::Output::Force);
                        break;

                    case BgMonitor::FLOAT:
                        MASSERT(item.numElems == 1);
                        entry.data().value_float(item.value.floatValue[0], Mle::Output::Force);
                        break;

                    case BgMonitor::STR:
                        // TODO add a way to pass this to protobuf without colwerting to string
                        {
                            const string value(&item.value.stringValue[0], item.numElems);
                            entry.data().value_str(value, Mle::Output::Force);
                        }
                        break;

                    default:
                        MASSERT(!"Support for this data type not implemented!");
                        // Force termination
                        error = true;
                        break;
                }

                if (error)
                {
                    break;
                }
            }
        }

        entry.Finish();
        Tee::PrintMle(&bs, Tee::MleOnly, sample.mleContext);
    }

    *pNumPrinted = numSamples;

    return RC::OK;
}

RC BgLoggerInstance::LogSamplesToJson()
{
    RC rc;
    JavaScriptPtr pJs;
    const bool isGpu = m_pMonitor->IsSubdevBased();

    JsonItem jsi;
    jsi.SetTag(isGpu ? "PerGpuMonitor" : "BgMonitor");
    jsi.SetField("name", GetMonitorName().c_str());

    for (const auto& sample : *this)
    {
        const PrintContext& ctx = m_PrintContexts[sample.devIdx];
        if (ctx.descs.empty() || !sample.pMonitor)
        {
            continue;
        }
        const bool isArray = ctx.descs.front().dataFormat == BgMonitor::INT_ARRAY;

        jsi.SetField(isGpu ? "gpu_inst" : "dev_inst", sample.devIdx);
        jsi.SetField("timestamp", sample.mleContext.timestamp);

        // Log legacy "timestamps(usec)" field
        if (isGpu)
        {
            JsArray timestamps;
            jsval jsVal = JSVAL_NULL;
            CHECK_RC(pJs->ToJsval(sample.mleContext.timestamp / 1000u, &jsVal));
            timestamps.push_back(jsVal);
            jsi.SetField("timestamps(usec)", &timestamps);
        }

        JsArray values;
        UINT32 descIdx    = 0;
        UINT32 numEntries = 0;
        for (const auto& item : sample)
        {
            if (descIdx >= ctx.descs.size())
            {
                break;
            }

            const auto type = item.type;
            ++descIdx;

            jsval jsVal = JSVAL_NULL;

            switch (type)
            {
                case BgMonitor::INT_ARRAY:
                    for (UINT32 i = 0; i < item.numElems; i++)
                    {
                        CHECK_RC(pJs->ToJsval(item.value.intValue[i], &jsVal));
                        values.push_back(jsVal);
                    }
                    break;

                case BgMonitor::INT:
                    MASSERT(item.numElems == 1);
                    CHECK_RC(pJs->ToJsval(item.value.intValue[0], &jsVal));
                    values.push_back(jsVal);
                    break;

                case BgMonitor::FLOAT:
                    MASSERT(item.numElems == 1);
                    CHECK_RC(pJs->ToJsval(item.value.floatValue[0], &jsVal));
                    values.push_back(jsVal);
                    break;

                case BgMonitor::STR:
                    {
                        const string strValue(&item.value.stringValue[0], item.numElems);
                        CHECK_RC(pJs->ToJsval(strValue, &jsVal));
                        values.push_back(jsVal);
                    }
                    break;

                default:
                    MASSERT(!"Support for this data type not implemented!");
                    return RC::SOFTWARE_ERROR;
            }

            ++numEntries;
        }

        if (isGpu)
        {
            jsi.SetField("samples", isArray ? numEntries : 1);
        }
        jsi.SetField("values", &values);

        CHECK_RC(jsi.WriteToLogfile());
    }

    return RC::OK;
}

void BgLoggerMgr::PrintThreadFunc(void* instanceCookie)
{
    static_cast<BgLoggerMgr*>(instanceCookie)->PrintThread();
}

void BgLoggerMgr::PrintThread()
{
    // Need to be attached for JSON logging, because we traverse back to JS
    // when logging data to JSON.  It would be nice if we could remove the use
    // of JS for JSON logging, because it's quite easy to write data to JSON.
    m_LogToJson = JsonLogStream::IsOpen();
    const bool detached = m_Detached && ! m_LogToJson;

    Printf(Tee::PriNormal, "BgLogger Print Thread starts %.3fms, %s (%s)\n",
           m_PrintIntervalMs.load(),
           m_Flush ? "flush" : "no flush",
           detached ? "detached" : "attached");

    unique_ptr<Tasker::DetachThread> pDetach;
    if (detached)
    {
        pDetach = make_unique<Tasker::DetachThread>();
    }

    RC rc;

    for (;;)
    {
        rc = WaitSlice(m_PrintIntervalMs);
        if (rc != RC::OK)
        {
            break;
        }

        const RC printRc = PrintSamples();

        // Flush if we printed something and if we need to flush
        if (printRc == RC::OK)
        {
            if (m_Flush)
            {
                Tee::FlushToDisk();
            }
        }
        // Stop on error
        else if (printRc != RC::EXIT_OK)
        {
            rc = printRc;
            break;
        }
    }

    if (rc != RC::EXIT_OK)
    {
        Printf(Tee::PriError, "BgLogger Print Thread - %s, exiting\n",
               rc.Message());
        m_ThreadExitRc = rc;
    }
}

RC BgLoggerInstance::Start()
{
    MASSERT(m_MonitorThreadId == Tasker::NULL_THREAD);

    m_MonitorThreadId = Tasker::CreateThread(MonitorThreadFunc, this, 0, "BgLogger Monitor");
    if (m_MonitorThreadId == Tasker::NULL_THREAD)
    {
        return RC::LWRM_OPERATING_SYSTEM;
    }

    return RC::OK;
}

RC BgLoggerInstance::Stop()
{
    StickyRC rc;

    if (m_MonitorThreadId != Tasker::NULL_THREAD)
    {
        rc = Tasker::Join(m_MonitorThreadId, m_ReadIntervalMs * 2);
        m_MonitorThreadId = Tasker::NULL_THREAD;
    }

    rc = m_ThreadExitRc;
    m_ThreadExitRc.Clear();

    return rc;
}

// Caches device indices and names for this monitor
RC BgLoggerInstance::CacheDevices()
{
    if (!m_DeviceIndices.empty())
    {
        return RC::OK;
    }

    // Generate GPU indices & names
    if (m_pMonitor->IsSubdevBased())
    {
        m_bUsesDevInst = true;
        m_DeviceIndices.reserve(16);
        m_PrintContexts.reserve(16);

        // Loop over all GPUs
        for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
        {
            const UINT32 devInst = pSubdev->DevInst();
            if (m_pMonitor->IsMonitoring(devInst))
            {
                m_DeviceIndices.push_back(devInst);

                if (devInst >= m_PrintContexts.size())
                {
                    m_PrintContexts.resize(devInst + 1);
                }
                m_PrintContexts[devInst].deviceName = pSubdev->GpuIdentStr();
            }
        }
    }
    // Generate LWSwitch indices & names
    else if (m_pMonitor->IsLwSwitchBased())
    {
        m_bUsesDevInst = true;
        m_DeviceIndices.reserve(16);
        m_PrintContexts.reserve(16);

        TestDeviceMgr* const pLwSwitchMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

        // Loop over all LWSwitches
        for (UINT32 lwswIdx = 0;
             lwswIdx < pLwSwitchMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH);
             lwswIdx++)
        {
            TestDevicePtr pLwSwDev;
            RC rc;
            CHECK_RC(pLwSwitchMgr->GetDevice(TestDevice::TYPE_LWIDIA_LWSWITCH, lwswIdx, &pLwSwDev));

            const UINT32 devInst = pLwSwDev->DevInst();
            if (m_pMonitor->IsMonitoring(devInst))
            {
                m_DeviceIndices.push_back(devInst);

                if (devInst >= m_PrintContexts.size())
                {
                    m_PrintContexts.resize(devInst + 1);
                }
                m_PrintContexts[devInst].deviceName = pLwSwDev->GetName();
            }
        }
    }
    // Generate device indices &names for a monitor which is not specific to any GPU or LWSwitch
    else
    {
        const UINT32 numDevices = m_pMonitor->GetNumDevices();
        m_DeviceIndices.reserve(numDevices);
        m_PrintContexts.reserve(numDevices);
        for (UINT32 devIdx = 0; devIdx < numDevices; devIdx++)
        {
            m_DeviceIndices.push_back(devIdx);
            m_PrintContexts.emplace_back();
        }
    }

    return RC::OK;
}

void BgLoggerInstance::DeterminePrintingParams()
{
    for (const UINT32 devIdx : m_DeviceIndices)
    {
        MASSERT(devIdx < m_PrintContexts.size());

        PrintContext& ctx = m_PrintContexts[devIdx];

        ctx.descs = m_pMonitor->GetSampleDescs(devIdx);

        ctx.printOneLine = true;
        ctx.sameUnits    = true;

        if (ctx.descs.size() > 1)
        {
            string columnName;
            string unit;
            for (const auto& desc : ctx.descs)
            {
                ctx.maxColNameSize = max(ctx.maxColNameSize, desc.columnName.size());

                ctx.maxUnitNameSize = max(ctx.maxUnitNameSize, desc.unit.size());

                if (columnName.empty())
                {
                    columnName = desc.columnName;
                }
                // If two columns have a different name, don't print on one line
                else if (columnName != desc.columnName && !columnName.empty())
                {
                    ctx.printOneLine = false;
                }

                if (unit.empty())
                {
                    unit = desc.unit;
                }
                else if (unit != desc.unit)
                {
                    ctx.sameUnits = false;
                }
            }
        }
    }
}

void BgLoggerInstance::PrintMleHeaders()
{
    const auto GetMleDataType = [](BgMonitor::ValueType type) -> Mle::BgMonitor::SampleDesc::DataType
    {
        switch (static_cast<int>(type))
        {
            default:
                // You are advised to use existing types.  If you choose to
                // use or implement a new type, you will go through a lot of pain
                // to plumb it through, HA HA!
                MASSERT(type == BgMonitor::INT);
                return Mle::BgMonitor::SampleDesc::type_int;

            case static_cast<int>(BgMonitor::INT_ARRAY):
                return Mle::BgMonitor::SampleDesc::type_int_array;

            case static_cast<int>(BgMonitor::FLOAT):
                return Mle::BgMonitor::SampleDesc::type_float;

            case static_cast<int>(BgMonitor::STR):
                return Mle::BgMonitor::SampleDesc::type_str;
        }
    };

    for (const UINT32 devIdx : m_DeviceIndices)
    {
        MASSERT(devIdx < m_PrintContexts.size());

        const INT32 actualDevIdx = m_bUsesDevInst ? devIdx : -1;

        ErrorMap::ScopedDevInst setDevInst(actualDevIdx);
        ByteStream bs;
        auto entry = Mle::Entry::bg_monitor(&bs);
        entry.type(m_pMonitor->GetType());

        for (const auto& desc : m_PrintContexts[devIdx].descs)
        {
            entry.desc()
                .desc(desc.columnName)
                .unit(desc.unit)
                .no_summary(!desc.summarize)
                .data_type(GetMleDataType(desc.dataFormat));
        }

        entry.Finish();
        Tee::PrintMle(&bs);
    }
}

void BgLoggerInstance::MonitorThreadFunc(void* instanceCookie)
{
    static_cast<BgLoggerInstance*>(instanceCookie)->MonitorThread();
}

void BgLoggerInstance::MonitorThread()
{
    Printf(Tee::PriNormal, "BgLogger %s Monitor starts %.3fms (%s)\n",
           GetMonitorName().c_str(),
           m_ReadIntervalMs,
           m_pMgr->IsDetached() ? "detached" : "attached");

    unique_ptr<Tasker::DetachThread> pDetach;
    if (m_pMgr->IsDetached())
    {
        pDetach = make_unique<Tasker::DetachThread>();
    }

    const bool obeysPausing = m_pMonitor->IsPauseable();

    // Allow a few conselwtive failures before exiting the thread
    UINT32 conselwtiveFailures = 0;
    constexpr UINT32 maxConselwtiveFailures = 4;

    vector<UINT64> prevTimesN;
    prevTimesN.reserve(16);

    RC rc;

    while (rc == RC::OK)
    {
        rc = m_pMgr->WaitSlice(m_ReadIntervalMs);
        if (rc != RC::OK)
        {
            break;
        }

        Tasker::MutexHolder lock(m_pMgr->GetGuard());

        // If bg monitors are paused and this bg monitor obeys pausing,
        // just go back and wait for the full interval without gathering
        // data.
        // Note: PauseLogging() also locks m_BgMonitorGuard, which ensures
        // that no pauseable monitor thread will ever run while being paused.
        if (obeysPausing && m_pMgr->IsPaused())
        {
            continue;
        }

        for (UINT32 devIdx : m_DeviceIndices)
        {
            if (devIdx >= prevTimesN.size())
            {
                prevTimesN.resize(devIdx + 1, 0);
            }

            if (m_PrintContexts[devIdx].descs.empty())
            {
                continue;
            }

            auto& sampleSlot = AllocSampleSlot(devIdx, prevTimesN[devIdx]);

            prevTimesN[devIdx] = sampleSlot.mleContext.timestamp;

            rc = m_pMonitor->GatherData(devIdx, &sampleSlot);
            if (rc == RC::OK)
            {
                conselwtiveFailures = 0;
            }
            else
            {
                INT32 pri = Tee::PriError;

                // Ignore a few conselwtive failures
                if (++conselwtiveFailures < maxConselwtiveFailures)
                {
                    rc.Clear();
                    pri = Tee::PriWarn;
                }

                Printf(pri, "BgLogger %s Monitor failed to collect data - %s\n",
                       GetMonitorName().c_str(), rc.Message());
                break;
            }

            sampleSlot.pMonitor = m_pMonitor.get();
        }
    }

    const RC shutdownRc = m_pMonitor->Shutdown();
    if ((shutdownRc != RC::OK) && (rc == RC::OK || rc == RC::EXIT_OK))
    {
        rc.Clear();
        rc = shutdownRc;
    }

    if (rc != RC::EXIT_OK)
    {
        Printf(Tee::PriError, "BgLogger %s Monitor - %s, exiting\n",
               GetMonitorName().c_str(), rc.Message());
        m_ThreadExitRc = rc;
    }
}

void BgLoggerMgr::PauseLogging()
{
    UINT32 newCount = 0;

    if (m_BgMonitorGuard)
    {
        // Note: If the bg logger was initialized, we lock this mutex to
        // ensure that no monitor thread will ignore the act of pausing,
        // because monitor threads only check whether they are paused
        // after acquiring this same mutex.  This avoids the situation
        // that some monitor thread would still be running when we
        // requested that all are paused.  This also guarantees that
        // the pausing is atomic in nature and after this function returns,
        // there is no way any monitor will be running.
        Tasker::AcquireMutex(m_BgMonitorGuard);
    }

    newCount = ++m_PausedCount;

    if (newCount == 1)
    {
        Printf(Tee::PriDebug, "Background loggers paused\n");
    }

    if (m_BgMonitorGuard)
    {
        Tasker::ReleaseMutex(m_BgMonitorGuard);
    }
}

void BgLoggerMgr::UnpauseLogging()
{
    UINT32 pausedCount = m_PausedCount.load(memory_order_relaxed);

    do
    {
        // If we're not paused; don't do anything
        if (m_PausedCount == 0)
        {
            return;
        }
    // Use CAS (compare-and-swap) to guarantee that there is no race with
    // another thread trying to pause or unpause logging.
    } while (!m_PausedCount.compare_exchange_weak(pausedCount, pausedCount - 1));

    if (pausedCount == 1)
    {
        Printf(Tee::PriDebug, "Background loggers unpaused\n");
    }
}

void BgLogger::PauseLogging()
{
    s_Loggers.PauseLogging();
}

void BgLogger::UnpauseLogging()
{
    s_Loggers.UnpauseLogging();
}

bool BgLogger::IsPaused()
{
    return s_Loggers.IsPaused();
}

BgLogger::PauseBgLogger::PauseBgLogger()
{
    s_Loggers.PauseLogging();
}

BgLogger::PauseBgLogger::~PauseBgLogger()
{
    s_Loggers.UnpauseLogging();
}

bool BgLogger::GetVerbose()
{
    return d_Verbose;
}

RC BgLogger::SetVerbose(bool bEnable)
{
    d_Verbose = bEnable;
    return OK;
}

bool BgLogger::GetDetached()
{
    return s_Loggers.IsDetached();
}

RC BgLogger::SetDetached(bool bDetached)
{
    s_Loggers.SetDetached(bDetached);
    return OK;
}

bool BgLogger::GetMleOnly()
{
    return d_MleOnly;
}

RC BgLogger::SetMleOnly(bool bMleOnly)
{
    d_MleOnly = bMleOnly;
    return OK;
}

RC BgLogger::Shutdown()
{
    return s_Loggers.Shutdown();
}

// JavaScript Glue
//-----------------------------------------------------------------------------

JS_CLASS(BgLogger);

//!  The BgLogger Object
/**
 *
 */
SObject BgLogger_Object
(
   "BgLogger",
   BgLoggerClass,
   0,
   0,
   "BgLogger Class"
);

JS_STEST_LWSTOM(BgLogger,
                AddMonitor,
                4,
                "Add a new Bg monitor")
{
    STEST_HEADER(4, 4, "Usage: BgLogger.AddMonitor(type, jsArrayOfParams, "
                       "jsArrayOfDeviceToMonitor, readIntervalMs)");
    STEST_ARG(0, UINT32,  type);
    STEST_ARG(1, JsArray, jsParams);
    STEST_ARG(2, JsArray, jsMonitorDevices);
    STEST_ARG(3, FLOAT64, readIntervalMs);

    bool paramsAreNumbers = true;
    for (const auto param : jsParams)
    {
        if (JS_TypeOfValue(pContext, param) != JSTYPE_NUMBER)
        {
            paramsAreNumbers = false;
            break;
        }
    }

    vector<UINT32> params;
    if (paramsAreNumbers)
    {
        for (const auto param : jsParams)
        {
            UINT32 paramVal;
            if (pJavaScript->FromJsval(param, &paramVal) != RC::OK)
                break;

            params.push_back(paramVal);
        }
    }

    set<UINT32> monitorDevices;
    for (const auto jsDevIdx : jsMonitorDevices)
    {
        UINT32 devIdx;
        if (pJavaScript->FromJsval(jsDevIdx, &devIdx) != RC::OK)
            break;

        monitorDevices.insert(devIdx);
    }

    RC rc;
    C_CHECK_RC(s_Loggers.Initialize());

    if (paramsAreNumbers &&
        (type != BgMonitorType::SPEEDO) &&
        (type != BgMonitorType::SPEEDO_TSOSC))
    {
        RETURN_RC(s_Loggers.AddMonitor(
            static_cast<BgMonitorType>(type),
            params,
            monitorDevices,
            readIntervalMs));
    }
    else
    {
        RETURN_RC(s_Loggers.AddMonitor(
            static_cast<BgMonitorType>(type),
            jsParams,
            monitorDevices,
            readIntervalMs));
    }
}

JS_STEST_LWSTOM(BgLogger,
                StopMonitor,
                0,
                "Stop thermal monitor")
{
    RETURN_RC(s_Loggers.Shutdown());
}

namespace BgLogger
{
    RC SetPrintIntervalMs(FLOAT64 printIntervalMs)
    {
        s_Loggers.SetPrintIntervalMs(printIntervalMs);
        return RC::OK;
    }

    FLOAT64 GetPrintIntervalMs()
    {
        return s_Loggers.GetPrintIntervalMs();
    }

    RC SetFlush(bool flush)
    {
        s_Loggers.SetFlush(flush);
        return RC::OK;
    }

    bool GetFlush()
    {
        return s_Loggers.GetFlush();
    }
}

PROP_READWRITE_NAMESPACE(BgLogger, PrintIntervalMs, FLOAT64,
                         "Background logger printing interval [ms], default=1000");
PROP_READWRITE_NAMESPACE(BgLogger, Flush, bool,
                         "Baclground logger flushing after each print [bool], default= false");

JS_SMETHOD_LWSTOM(BgLogger,
                  PauseLogging,
                  0,
                  "Pause the background logger monitor threads")
{
    STEST_HEADER(0, 0, "Usage: BgLogger.PauseLogging()");
    BgLogger::PauseLogging();

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(BgLogger,
                  UnpauseLogging,
                  0,
                  "Unpause the background logger monitor threads, if they were paused")
{
    STEST_HEADER(0, 0, "Usage: BgLogger.UnpauseLogging()");
    BgLogger::UnpauseLogging();

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(BgLogger,
                  IsPaused,
                  0,
                  "Report whether the background logger monitor threads are paused")
{
    STEST_HEADER(0, 0, "Usage: BgLogger.IsPaused()");

    bool bIsPaused = BgLogger::IsPaused();
    if (RC::OK != pJavaScript->ToJsval(bIsPaused, pReturlwalue))
    {
       JS_ReportError(pContext, "BgLogger.IsPaused failed");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }

    return JS_TRUE;
}

PROP_CONST(BgLogger, GPUTEMP_SENSOR, BgMonitorType::GPUTEMP_SENSOR);
PROP_CONST(BgLogger, INT_TEMP_SENSOR, BgMonitorType::INT_TEMP_SENSOR);
PROP_CONST(BgLogger, BJT_TEMP_SENSOR, BgMonitorType::BJT_TEMP_SENSOR);
PROP_CONST(BgLogger, MEM_TEMP_SENSOR, BgMonitorType::MEM_TEMP_SENSOR);
PROP_CONST(BgLogger, DRAM_TEMP_SENSOR, BgMonitorType::DRAM_TEMP_SENSOR);
PROP_CONST(BgLogger, EXT_TEMP_SENSOR, BgMonitorType::EXT_TEMP_SENSOR);
PROP_CONST(BgLogger, SMBUS_TEMP_SENSOR, BgMonitorType::SMBUS_TEMP_SENSOR);
PROP_CONST(BgLogger, IPMI_TEMP_SENSOR, BgMonitorType::IPMI_TEMP_SENSOR);
PROP_CONST(BgLogger, SOC_TEMP_SENSOR, BgMonitorType::SOC_TEMP_SENSOR);
PROP_CONST(BgLogger, FAN_RPM_SENSOR, BgMonitorType::FAN_RPM_SENSOR);
PROP_CONST(BgLogger, POWER_SENSOR, BgMonitorType::POWER_SENSOR);
PROP_CONST(BgLogger, LWRRENT_SENSOR, BgMonitorType::LWRRENT_SENSOR);
PROP_CONST(BgLogger, VOLTERRA_SENSOR, BgMonitorType::VOLTERRA_SENSOR);
PROP_CONST(BgLogger, SPEEDO, BgMonitorType::SPEEDO);
PROP_CONST(BgLogger, SPEEDO_TSOSC, BgMonitorType::SPEEDO_TSOSC);
PROP_CONST(BgLogger, GPU_VOLTAGE, BgMonitorType::GPU_VOLTAGE);
PROP_CONST(BgLogger, GPU_CLOCKS, BgMonitorType::GPU_CLOCKS);
PROP_CONST(BgLogger, GPU_PART_CLOCKS, BgMonitorType::GPU_PART_CLOCKS);
PROP_CONST(BgLogger, GPC_LIMITS, BgMonitorType::GPC_LIMITS);
PROP_CONST(BgLogger, MULTIRAILCWC_LIMITS, BgMonitorType::MULTIRAILCWC_LIMITS);
PROP_CONST(BgLogger, ADC_SENSED_VOLTAGE, BgMonitorType::ADC_SENSED_VOLTAGE);
PROP_CONST(BgLogger, RAM_ASSIST_ENGAGED, BgMonitorType::RAM_ASSIST_ENGAGED);
PROP_CONST(BgLogger, POWER_POLICIES_LOGGER, BgMonitorType::POWER_POLICIES_LOGGER);
PROP_CONST(BgLogger, LWSWITCH_TEMP_SENSOR, BgMonitorType::LWSWITCH_TEMP_SENSOR);
PROP_CONST(BgLogger, LWSWITCH_VOLT_SENSOR, BgMonitorType::LWSWITCH_VOLT_SENSOR);
PROP_CONST(BgLogger, THERMAL_SLOWDOWN, BgMonitorType::THERMAL_SLOWDOWN);
PROP_CONST(BgLogger, TEST_FIXTURE_TEMPERATURE, BgMonitorType::TEST_FIXTURE_TEMPERATURE);
PROP_CONST(BgLogger, TEST_FIXTURE_RPM, BgMonitorType::TEST_FIXTURE_RPM);
PROP_CONST(BgLogger, PCIE_SPEED, BgMonitorType::PCIE_SPEED);
PROP_CONST(BgLogger, TEST_NUM, BgMonitorType::TEST_NUM);
PROP_CONST(BgLogger, TEMP_CTRL, BgMonitorType::TEMP_CTRL);
PROP_CONST(BgLogger, GR_STATUS, BgMonitorType::GR_STATUS);
PROP_CONST(BgLogger, DGX_BMC_SENSOR, BgMonitorType::DGX_BMC_SENSOR);
PROP_CONST(BgLogger, CPU_USAGE, BgMonitorType::CPU_USAGE);
PROP_CONST(BgLogger, EDPP, BgMonitorType::EDPP);
PROP_CONST(BgLogger, FLAG_NO_PRINTS, BgLogger::FLAG_NO_PRINTS);
PROP_CONST(BgLogger, FLAG_DUMPSTATS, BgLogger::FLAG_DUMPSTATS);
PROP_CONST(BgLogger, VRM_POWER, BgMonitorType::VRM_POWER);
PROP_CONST(BgLogger, GPU_USAGE, BgMonitorType::GPU_USAGE);
PROP_CONST(BgLogger, IPMI_LWSTOM_SENSOR, BgMonitorType::IPMI_LWSTOM_SENSOR);
PROP_CONST(BgLogger, FAN_COOLER_STATUS, BgMonitorType::FAN_COOLER_STATUS);
PROP_CONST(BgLogger, FAN_POLICY_STATUS, BgMonitorType::FAN_POLICY_STATUS);
PROP_CONST(BgLogger, FAN_ARBITER_STATUS, BgMonitorType::FAN_ARBITER_STATUS);
PROP_CONST(BgLogger, CABLE_CONTROLLER_TEMP_SENSOR, BgMonitorType::CABLE_CONTROLLER_TEMP_SENSOR);
PROP_CONST(BgLogger, SPEEDO_CPM, BgMonitorType::SPEEDO_CPM);
PROP_CONST(BgLogger, GPIO, BgMonitorType::GPIO);

PROP_CONST(BgLogger, SPEEDOMONITOR_FLAGS,       SpeedoMonitor::FLAGS);
PROP_CONST(BgLogger, SPEEDOMONITOR_TYPE,        SpeedoMonitor::TYPE);
PROP_CONST(BgLogger, SPEEDOMONITOR_OSCIDX,      SpeedoMonitor::OSCIDX);
PROP_CONST(BgLogger, SPEEDOMONITOR_DURATION,    SpeedoMonitor::DURATION);
PROP_CONST(BgLogger, SPEEDOMONITOR_OUTDIV,      SpeedoMonitor::OUTDIV);
PROP_CONST(BgLogger, SPEEDOMONITOR_MODE,        SpeedoMonitor::MODE);
PROP_CONST(BgLogger, SPEEDOMONITOR_ADJ,         SpeedoMonitor::ADJ);
PROP_CONST(BgLogger, SPEEDOMONITOR_ALT_PARAMS,  SpeedoMonitor::ALT_PARAMS);
PROP_CONST(BgLogger, SPEEDOMONITOR_INIT_PARAMS, SpeedoMonitor::INIT_PARAMS);
PROP_CONST(BgLogger, SPEEDOMONITOR_NUM_PARAMS,  SpeedoMonitor::NUM_PARAMS);

PROP_READWRITE_NAMESPACE(BgLogger, Verbose, bool,
   "Enable/Disable verbose printing in BgLogger monitor (default = disabled)");

PROP_READWRITE_NAMESPACE(BgLogger, Detached, bool,
   "Enable detached (asynchronous) monitoring in BgLogger (default = enabled)");

PROP_READWRITE_NAMESPACE(BgLogger, MleOnly, bool,
   "Hide standard data prints and only use Mle prints (default = don't hide)");
