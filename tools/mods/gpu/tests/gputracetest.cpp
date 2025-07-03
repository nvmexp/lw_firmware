/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gputest.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jsonlog.h"
#include "core/include/platform.h"
#include "cheetah/include/teggpufirewallown.h"
#include "core/include/utility.h"
#include "traceplayer/trace_parser.h"
#include "traceplayer/trace_player_engine.h"

namespace MfgTest
{
    using namespace MfgTracePlayer;

    class GpuTrace : public GpuTest
    {
    public:
        GpuTrace() = default;
        virtual ~GpuTrace() { }

        virtual void PrintJsProperties(Tee::Priority pri);
        virtual bool IsSupported();
        virtual bool IsSupportedVf();
        virtual RC Setup();
        virtual RC Run();
        virtual RC Cleanup();

        SETGET_PROP(Trace,          string);
        SETGET_PROP(AllowFailLoops, UINT32);
        SETGET_PROP(CRCTolerance,   UINT32);
        SETGET_PROP(SkipCRCFrames,  string);
        SETGET_PROP(CacheSurfaces,  bool);
        SETGET_PROP(StrictWFI,      bool);
        SETGET_PROP(KeepRunning,    bool);
        SETGET_PROP(PauseForDisplay,UINT32);
        SETGET_PROP_LWSTOM(Dump,    string);
        SETGET_PROP(SurfaceAccessorSizeMB, UINT32);

    private:
        RC ParseSkipCRCFrames(set<UINT32>* pOut) const;

        struct DumpEntry
        {
            string surfName;
            string eventName;
        };
        GpuTestConfiguration*    m_pTestCfg = nullptr;
        unique_ptr<Engine>       m_pEngine;
        TegraGpuRegFirewallOwner m_GpuRegFirewall;

        // JS properties
        string m_Trace;
        UINT32 m_AllowFailLoops        = 0U;
        UINT32 m_CRCTolerance          = 0U;
        string m_SkipCRCFrames;
        bool   m_CacheSurfaces         = true;
        bool   m_StrictWFI             = false;
        bool   m_KeepRunning           = false;
        UINT32 m_PauseForDisplay       = 0U;
        vector<DumpEntry> m_Dump;
        UINT32 m_SurfaceAccessorSizeMB = 16;
    };
}

using namespace MfgTest;

JS_CLASS_INHERIT(GpuTrace, GpuTest, "GPU trace test");

CLASS_PROP_READWRITE(GpuTrace, Trace,          string, "Trace file or directory");
CLASS_PROP_READWRITE(GpuTrace, AllowFailLoops, UINT32, "Number of loops allowed to fail with a CRC error");
CLASS_PROP_READWRITE(GpuTrace, CRCTolerance,   UINT32, "CRC tolerance");
CLASS_PROP_READWRITE(GpuTrace, SkipCRCFrames,  string, "Indices in the CRC surface to skip");
CLASS_PROP_READWRITE(GpuTrace, CacheSurfaces,  bool,   "Enable surface caching between loops (default: true)");
CLASS_PROP_READWRITE(GpuTrace, StrictWFI,      bool,   "Force WFI commands to do a wait on the CPU");
CLASS_PROP_READWRITE(GpuTrace, KeepRunning,    bool,   "Continue looping until this flag is set to false");
CLASS_PROP_READWRITE(GpuTrace, PauseForDisplay,UINT32, "Pause for specified amount of milliseconds when DISPLAY_IMAGE command is encountered");
CLASS_PROP_READWRITE(GpuTrace, Dump,           string, "Dump one or more surfaces in response to events.  Arg format is 'surface:event[,surface2:event2,...]'");
CLASS_PROP_READWRITE(GpuTrace, SurfaceAccessorSizeMB, UINT32, "Size of sysmem surface used as an intermediate surface when accessing FB memory"); //$

void MfgTest::GpuTrace::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "GpuTrace Js Properties:\n");
    Printf(pri, "\tTrace:                 %s\n",   m_Trace.c_str());
    Printf(pri, "\tAllowFailLoops:        %u\n",   m_AllowFailLoops);
    Printf(pri, "\tCRCTolerance:          %u\n",   m_CRCTolerance);
    Printf(pri, "\tSkipCRCFrames:         %s\n",   m_SkipCRCFrames.c_str());
    Printf(pri, "\tCacheSurfaces:         %s\n",   m_CacheSurfaces ? "true" : "false");
    Printf(pri, "\tStrictWFI:             %s\n",   m_StrictWFI     ? "true" : "false");
    Printf(pri, "\tKeepRunning:           %s\n",   m_KeepRunning   ? "true" : "false");
    Printf(pri, "\tPauseForDisplay:       %ums\n", m_PauseForDisplay);
    Printf(pri, "\tDump:                  %s\n",   GetDump().c_str());
    Printf(pri, "\tSurfaceAccessorSizeMB: %u\n",   m_SurfaceAccessorSizeMB);
}

bool MfgTest::GpuTrace::IsSupported()
{
    if (!GpuTest::IsSupported())
    {
        return false;
    }

    if (!Engine::IsSupported(GetTestConfiguration()->GetRmClient(),
                             GetTestConfiguration()->GetGpuDevice()))
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
bool MfgTest::GpuTrace::IsSupportedVf()
{
    return !(GetBoundGpuSubdevice()->IsSMCEnabled());
}

RC MfgTest::GpuTrace::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());

    if (m_CRCTolerance >= 32)
    {
        Printf(Tee::PriHigh, "Error: Invalid CRC tolerance - %u\n", m_CRCTolerance);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    m_pTestCfg = GetTestConfiguration();

    // The GPU driver in the kernel has a "firewall" and only
    // allows the FECS to access certain GPU registers.
    // However the traces we have use SET_FALCON method to
    // access more registers than are normally whitelisted.
    // With this, we can turn off the firewall and run the traces.
    if (Platform::UsesLwgpuDriver())
    {
        CHECK_RC(m_GpuRegFirewall.DisableFirewall());
    }

    // Parse user's skip frames
    set<UINT32> skipCRCFrames;
    CHECK_RC(ParseSkipCRCFrames(&skipCRCFrames));

    m_pEngine.reset(new Engine);

    Engine::Config cfg;
    cfg.pLwRm          = m_pTestCfg->GetRmClient();
    cfg.pGpuDevice     = m_pTestCfg->GetGpuDevice();
    cfg.pTestConfig    = m_pTestCfg;
    cfg.pDisplayMgrTestContext = GetDisplayMgrTestContext();
    cfg.defTimeoutMs   = m_pTestCfg->TimeoutMs();
    cfg.cacheSurfaces  = m_CacheSurfaces;
    cfg.cpuWfi         = m_StrictWFI;
    cfg.displayPauseMs = m_PauseForDisplay;
    cfg.pri            = GetVerbosePrintPri();
    cfg.crcTolerance   = m_CRCTolerance;
    cfg.skipCRCFrames  = skipCRCFrames;
    cfg.surfaceAccessorSizeMB = m_SurfaceAccessorSizeMB;

    m_pEngine->Init(cfg);

    if (m_Trace.empty())
    {
        Printf(Tee::PriHigh, "Error: Missing Trace testarg.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    Parser parser(*m_pEngine);
    CHECK_RC(parser.Parse(m_Trace));

    if (!m_pEngine->IsSupported())
    {
        Printf(Tee::PriHigh, "Error: This GPU cannot play trace %s\n", m_Trace.c_str());
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    for (const auto& item : m_Dump)
    {
        CHECK_RC(m_pEngine->DumpSurfaceOnEvent(item.surfName, item.eventName));
    }

    CHECK_RC(m_pEngine->AllocResources());

    return rc;
}

RC MfgTest::GpuTrace::Run()
{
    StickyRC finalRc;
    RC rc;
    UINT32 numCRCFailures = 0;

    const bool bPerpetualRun = m_KeepRunning;
    const UINT32 numLoops = m_pTestCfg->Loops();
    if (!bPerpetualRun)
    {
        CHECK_RC(PrintProgressInit(numLoops));
    }

    for (UINT32 iloop = 0; iloop < numLoops || m_KeepRunning; iloop++)
    {
        vector<Engine::Error> errors;
        rc = m_pEngine->PlayTrace(&errors);
        if (rc == RC::GOLDEN_VALUE_MISCOMPARE)
        {
            ++numCRCFailures;
            if (numCRCFailures <= m_AllowFailLoops)
            {
                Printf(Tee::PriHigh, "Ignoring CRC error at loop %u\n", iloop);
                rc.Clear();
            }
        }
        finalRc = rc;
        if (rc != OK)
        {
            if (!bPerpetualRun)
            {
                CHECK_RC(PrintErrorUpdate(iloop, rc));
            }
            GetJsonExit()->AddFailLoop(iloop);
        }
        if (rc != OK && GetGoldelwalues()->GetStopOnError())
        {
            break;
        }
        rc.Clear();
        CHECK_RC(EndLoop(iloop));

        if (!bPerpetualRun)
        {
            CHECK_RC(PrintProgressUpdate(iloop + 1));
        }
    }
    return finalRc;
}

RC MfgTest::GpuTrace::Cleanup()
{
    StickyRC rc;
    rc = m_pEngine->FreeResources();
    m_pEngine.reset(nullptr);
    m_pTestCfg = nullptr;
    m_GpuRegFirewall.RestoreFirewall();
    rc = GpuTest::Cleanup();
    return rc;
}

//! Store the Dump testarg, parsing it into an internal struct
RC MfgTest::GpuTrace::SetDump(string val)
{
    vector<string> tokens;
    RC rc;

    m_Dump.clear();
    CHECK_RC(Utility::Tokenizer(val, ",", &tokens));
    for (const auto& token : tokens)
    {
        size_t colonPos = token.find_last_of(':');
        if (colonPos == string::npos)
        {
            Printf(Tee::PriHigh, "Badly formatted Dump string.\n");
            return RC::BAD_PARAMETER;
        }
        DumpEntry dumpEntry;
        dumpEntry.surfName  = token.substr(0, colonPos);
        dumpEntry.eventName = token.substr(colonPos + 1);
        m_Dump.push_back(dumpEntry);
    }
    return OK;
}

//! Colwert the internal m_Dump struct back to the original testarg string
string MfgTest::GpuTrace::GetDump() const
{
    string stringVal;
    for (const auto& item : m_Dump)
    {
        const char* separator = stringVal.empty() ? "" : ",";
        stringVal += separator + item.surfName + ":" + item.eventName;
    }
    return stringVal;
}

RC MfgTest::GpuTrace::ParseSkipCRCFrames(set<UINT32>* pOut) const
{
    RC rc = Utility::ParseIndices(m_SkipCRCFrames, pOut);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Invalid SkipCRCFrames argument: %s\n",
            m_SkipCRCFrames.c_str());
    }
    return rc;
}
