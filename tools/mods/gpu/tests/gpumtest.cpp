/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mods_reg_hal.h"
#include "core/include/memerror.h"
#include "gpumtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/device.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jsonlog.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#if defined(TEGRA_MODS)
#include "linux/xp_mods_kd.h"
#endif
#include "cheetah/include/cheetah.h"
#include <boost/smart_ptr/make_shared.hpp>

//------------------------------------------------------------------------------
GpuMemTest::GpuMemTest()
 :   m_ChunkSizeMb(0.0)
    ,m_MinFbMemPercent(0)
    ,m_MaxFbMb(0.0)
    ,m_EndFbMb(0)
    ,m_StartFbMb(0)
    ,m_L2ModeToTest(GpuSubdevice::L2_DEFAULT)
    ,m_NumRechecks(0)
    ,m_AutoRefreshValue(0)
    ,m_OrgAutoRefreshValue(0)
    ,m_IntermittentError(false)
    ,m_DumpLaneRepair("")
    ,m_hPreRunCallbackMemError(0)
    ,m_hPostRunCallbackMemError(0)
{
    SetName("GpuMemTest");
    // We intend to change the L2 cache mode in our Setup function. So if this
    // test is run in a background thread we need to anounce that this Setup
    // function must complete before the forground test tries to execute its
    // Run function.
    SetSetupSyncRequired(true);
    m_hPreRunCallbackMemError =
        GetCallbacks(PRE_RUN)->Connect(&GpuMemTest::OnPreRun, this);
    m_hPostRunCallbackMemError =
        GetCallbacks(POST_RUN)->Connect(&GpuMemTest::OnPostRun, this);
}

//------------------------------------------------------------------------------
GpuMemTest::~GpuMemTest()
{
    m_ErrorObjs.clear();

    if (m_hPreRunCallbackMemError)
        GetCallbacks(PRE_RUN)->Disconnect(m_hPreRunCallbackMemError);
    if (m_hPostRunCallbackMemError)
        GetCallbacks(POST_RUN)->Disconnect(m_hPostRunCallbackMemError);
}

//------------------------------------------------------------------------------
RC GpuMemTest::Setup()
{
    const GpuDevice *pGpuDevice = GetBoundGpuDevice();
    RC rc = OK;

    // On CheetAh, tell the kernel to release any and all caches and buffers that
    // it may have allocated.  This will free up some memory for testing and it
    // will also make the memory tests more reliable.
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        if (Xp::SystemDropCaches() != RC::OK)
        {
            Printf(Tee::PriWarn, "Failed to drop system caches, memory may be fragmented\n");
        }
    }

    CHECK_RC(GpuTest::Setup());

    m_OrgL2Mode.clear();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        GpuSubdevice *pSubdev = pGpuDevice->GetSubdevice(subdev);
        GpuSubdevice::L2Mode mode;
        CHECK_RC(pSubdev->GetL2Mode(&mode));
        m_OrgL2Mode.push_back(mode);
        if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_RUNTIME_L2_MODE_CHANGE))
        {
            CHECK_RC(pSubdev->SetL2Mode((GpuSubdevice::L2Mode)m_L2ModeToTest));
        }
        else
        {
            CHECK_RC(pSubdev->CheckL2Mode((GpuSubdevice::L2Mode)m_L2ModeToTest));
        }
    }

    // m_EndFbMb and m_StartFbMb are the user's requests, which we will
    // honor only as feasible.  Warn the user if they aren't getting
    // what they asked for.

    if ((m_EndFbMb != 0) &&  // 0 means "to end of bound subdev's FB"
        (m_EndFbMb != GetEndLocation()))
    {
        Printf(Tee::PriWarn, "End location %d is not allowed, forcing %d.\n",
                m_EndFbMb, GetEndLocation());
    }
    if (m_StartFbMb != GetStartLocation())
    {
        Printf(Tee::PriWarn, "Start location %d is not allowed, forcing %d.\n",
                m_StartFbMb, GetStartLocation());
    }

    // RM created an API to set AutoRefresh a long time ago, however it was incorrectly
    // implemented (Bug 1834557). Therefore, we'll program the register directly
    // ourselves until they fix the bug. Afterwards, we should change the AutoRefreshValue
    // parameter to be AutoRefreshMs, so that a user does not need to know the exact
    // register value needed to program to a specific refresh period.
    if (m_AutoRefreshValue)
    {
        RegHal& regHal = GetBoundGpuSubdevice()->Regs();
        if (!regHal.IsSupported(MODS_PFB_FBPA_CONFIG4))
        {
            Printf(Tee::PriError,
                "Setting the AutoRefresh register is not supported on this chip!\n");
            CHECK_RC(RC::UNSUPPORTED_HARDWARE_FEATURE);
        }

        UINT32 refreshLo = regHal.GetField(m_AutoRefreshValue,
                                           MODS_PFB_FBPA_CONFIG4_REFRESH_LO);
        UINT32 refreshHi = regHal.GetField(m_AutoRefreshValue,
                                           MODS_PFB_FBPA_CONFIG4_REFRESH);

        if (refreshLo > regHal.LookupValue(MODS_PFB_FBPA_CONFIG4_REFRESH_LO_MAX) ||
            refreshHi > regHal.LookupValue(MODS_PFB_FBPA_CONFIG4_REFRESH_MAX))
        {
            Printf(Tee::PriError, "Invalid AutoRefreshValue provide: 0x%x\n", m_AutoRefreshValue);
            CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }

        m_OrgAutoRefreshValue = regHal.Read32(MODS_PFB_FBPA_CONFIG4);

        UINT32 newRefreshVal = m_OrgAutoRefreshValue;
        regHal.SetField(&newRefreshVal, MODS_PFB_FBPA_CONFIG4_REFRESH_LO, refreshLo);
        regHal.SetField(&newRefreshVal, MODS_PFB_FBPA_CONFIG4_REFRESH, refreshHi);
        regHal.Write32(MODS_PFB_FBPA_CONFIG4, newRefreshVal);

        Printf(Tee::PriDebug, "Setting AutoRefreshValue = 0x%x, reg = 0x%x\n",
            m_AutoRefreshValue, regHal.Read32(MODS_PFB_FBPA_CONFIG4));
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Print intermittent error if required
void GpuMemTest::PrintIntermittentError(RC rc)
{
    if (GetIntermittentError())
    {
        if (OK == rc)
            Printf(Tee::PriWarn, "Possible marginal system memory."
                    " Retries of dma/compare required to pass the test.\n");
        else
            Printf(Tee::PriWarn, "Possible corrupt system memory and FB.\n");
    }
}

//------------------------------------------------------------------------------
RC GpuMemTest::AllocateEntireFramebuffer
(
    bool blockLinear,
    UINT32 hChannel
)
{
    // Get largest page possible, to prevent performance issues due to
    // TLB ilwalidation when testing large regions of memory.
    const GpuDevice* pGpuDevice = GetBoundGpuDevice();
    return AllocateEntireFramebuffer2(blockLinear,
                                      pGpuDevice->GetBigPageSize(),
                                      pGpuDevice->GetMaxPageSize(),
                                      hChannel, true);
}

//------------------------------------------------------------------------------
RC GpuMemTest::AllocateEntireFramebuffer2
(
    bool blockLinear,
    UINT64 minPageSize,
    UINT64 maxPageSize,
    UINT32 hChannel,
    bool contiguous
)
{
    const UINT64 maxSize  = min(static_cast<UINT64>(GetEndLocation() * 1_MB),
                                static_cast<UINT64>(GetMaxFbMb() * 1_MB));
    RC rc;

    JavaScriptPtr pJs;
    JSObject * pGlobObj = pJs->GetGlobalObject();
    bool forceSmallPages = false;
    pJs->GetProperty(pGlobObj, "g_ForceSmallPages", &forceSmallPages);

    if (forceSmallPages)
    {
        minPageSize = GpuDevice::PAGESIZE_SMALL;
        maxPageSize = GpuDevice::PAGESIZE_SMALL;
    }

    // Set min chunk size to smallest page size
    const UINT64 minChunkSize = (GetBoundGpuSubdevice()->IsSOC() ?
                                 Xp::GetPageSize() :
                                 GpuDevice::PAGESIZE_SMALL);
    const UINT64 maxChunkSize = static_cast<UINT64>(GetChunkSizeMb() * 1_MB);

    CHECK_RC(GpuUtility::AllocateEntireFramebuffer(
                      minChunkSize,
                      maxChunkSize,
                      maxSize,
                      GetChunks(),
                      blockLinear,
                      minPageSize,
                      maxPageSize,
                      hChannel,
                      GetBoundGpuDevice(),
                      GetMinFbMemPercent(),
                      contiguous));

    if (GetChunks()->size() == 0)
    {
        Printf(Tee::PriNormal, "ERROR:  Memory was not allocated.\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GpuMemTest::InitFromJs()
{
    RC rc;
    bool bSupported = false;
    UINT32 enableMask = 0;
    GpuTest::InitFromJs();
    CHECK_RC(GetBoundGpuSubdevice()->GetEccEnabled(&bSupported, &enableMask));
    if (bSupported && Ecc::IsUnitEnabled(Ecc::Unit::DRAM, enableMask))
    {
        SetL2ModeToTest(GpuSubdevice::L2_ENABLED);
    }
    return rc;
}

//------------------------------------------------------------------------------
void GpuMemTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "GpuMemTest Js Properties:\n");
    Printf(pri, "\tMinFbMemPercent:\t\t%d%%\n", m_MinFbMemPercent);
    Printf(pri, "\tMaxFbMb:\t\t\t%.3f\n", m_MaxFbMb);
    Printf(pri, "\tChunkSizeMb:\t\t\t%.3f\n", m_ChunkSizeMb);
    Printf(pri, "\tStart:\t\t\t\t%d\n", GetStartLocation());
    Printf(pri, "\tEnd:\t\t\t\t%d\n", GetEndLocation());
    Printf(pri, "\tL2ModeToTest:\t\t\t%d\n", m_L2ModeToTest);
    Printf(pri, "\tDumpLaneRepair:\t\t\t%s\n", m_DumpLaneRepair.c_str());

    return;
}

//! \brief Check that we are supported on the given HW
bool GpuMemTest::IsSupported()
{
    if (GetBoundGpuSubdevice()->FbHeapSize() == 0 &&
        ! GetBoundGpuSubdevice()->IsSOC())
        return false;

#if defined(TEGRA_MODS)
    if (!Xp::GetMODSKernelDriver()->IsOpen())
    {
        Printf(Tee::PriNormal, "%s not supported without MODS kernel driver\n",
               GetName().c_str());
        return false;
    }
#endif

    return true;
}

//-----------------------------------------------------------------------------
//! \brief Identify the type of ModsTest subclass
//!
/* virtual */ bool GpuMemTest::IsTestType(TestType tt)
{
    return (tt == GPU_MEM_TEST) || GpuTest::IsTestType(tt);
}

/* virtual */ RC GpuMemTest::EndLoop(UINT32 loop /* = 0xffffffff */)
{
    StickyRC firstRc;
    firstRc = GpuTest::EndLoop(loop);

    if (GetGoldelwalues()->GetStopOnError())
    {
        for (UINT32 s = 0; s < GetBoundGpuDevice()->GetNumSubdevices(); s++)
        {
            RC rc = GetMemError(s).ResolveMemErrorResult();
            if (rc != OK)
            {
                if (loop != 0xffffffff)
                {
                    GetJsonExit()->AddFailLoop(loop);
                }
                firstRc = rc;
                m_DidResolveMemErrorResult = true;
                break;
            }
        }
    }
    return firstRc;
}

//! \brief Cleanup the GpuMemTest
//!
//! Each memory test at minimum should free up the MemError objects on cleanup.
RC GpuMemTest::Cleanup()
{
    StickyRC firstRc;

    if (GetBoundGpuDevice()->GetNumSubdevices() == m_OrgL2Mode.size())
    {
        for (UINT32 subdev = 0;
             subdev < GetBoundGpuDevice()->GetNumSubdevices(); subdev++)
        {
            GpuSubdevice *pSubdev = GetBoundGpuDevice()->GetSubdevice(subdev);
            firstRc = pSubdev->SetL2Mode(
                    static_cast<GpuSubdevice::L2Mode>(m_OrgL2Mode[subdev]));
        }
    }

    firstRc = GpuUtility::FreeEntireFramebuffer(GetChunks(),
                                                GetBoundGpuDevice());

    if (m_OrgAutoRefreshValue)
    {
        RegHal& regHal = GetBoundGpuSubdevice()->Regs();

        UINT32 restoreRefreshVal = regHal.Read32(MODS_PFB_FBPA_CONFIG4);
        UINT32 refreshLo = regHal.GetField(m_OrgAutoRefreshValue,
                                           MODS_PFB_FBPA_CONFIG4_REFRESH_LO);
        UINT32 refreshHi = regHal.GetField(m_OrgAutoRefreshValue,
                                           MODS_PFB_FBPA_CONFIG4_REFRESH);

        regHal.SetField(&restoreRefreshVal, MODS_PFB_FBPA_CONFIG4_REFRESH_LO, refreshLo);
        regHal.SetField(&restoreRefreshVal, MODS_PFB_FBPA_CONFIG4_REFRESH, refreshHi);
        regHal.Write32(MODS_PFB_FBPA_CONFIG4, restoreRefreshVal);
    }

    firstRc = GpuTest::Cleanup();
    return firstRc;
}

//------------------------------------------------------------------------------
// Protected Member functions
//------------------------------------------------------------------------------

//! Set the end FB testing location
RC GpuMemTest::SetEndLocation(UINT32 end)
{
    m_EndFbMb = end;

    return OK;
}

//! Get the end FB testing location (force valid end)
UINT32 GpuMemTest::GetEndLocation()
{
    if (m_ErrorObjs.size() > 0 && GetMemError(0).IsSetup())
    {
        FbRanges ranges;
        RC rc = GetMemError(0).GetFB()->GetFbRanges(&ranges);
        if (OK != rc)
        {
            Printf(Tee::PriWarn, "GetFbRanges failed! rc = %d\n", rc.Get());
            return m_EndFbMb;
        }

        UINT64 endLoc = ranges[0].size + ranges[0].start;
        if (ranges.size() > 1)
        {
            for (UINT32 i = 1; i < ranges.size(); i++)
                if (endLoc < (ranges[i].size + ranges[i].start))
                    endLoc = ranges[i].size + ranges[i].start;
        }
        endLoc = endLoc / 1_MB;

        if ((m_EndFbMb == 0) || (m_EndFbMb > endLoc))
            return UNSIGNED_CAST(UINT32, endLoc);
    }

    // Else, we don't have a bound FB yet, can't fixup EndLocation.
    return m_EndFbMb;
}

//! Set the start FB testing location
RC GpuMemTest::SetStartLocation(UINT32 start)
{
    m_StartFbMb = start;

    return OK;
}

//! Get the start FB testing location (force valid start)
UINT32 GpuMemTest::GetStartLocation()
{
    if (m_ErrorObjs.size() > 0 && GetMemError(0).IsSetup())
    {
        UINT32 end = GetEndLocation();
        if ((m_StartFbMb > 0) && (m_StartFbMb >= end))
            return end-1;
    }
    return m_StartFbMb;
}

//--------------------------------------------------------------------
//! Return the memory chunk (allocated by AllocateEntireFramebuffer)
//! that contains the specified FB offset, or NULL if none.
//!
const GpuUtility::MemoryChunkDesc *GpuMemTest::GetChunk(UINT64 fbOffset) const
{
    for (GpuUtility::MemoryChunks::const_iterator it = m_MemoryChunks.begin();
         it != m_MemoryChunks.end(); ++it)
    {
        if (it->contiguous &&
            (fbOffset >= it->fbOffset) &&
            (fbOffset < (it->fbOffset + it->size)))
        {
            return &(*it);
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
// JS linkage
//------------------------------------------------------------------------------
JS_CLASS_VIRTUAL_INHERIT(GpuMemTest, GpuTest, "Memory test base class");

CLASS_PROP_READWRITE(GpuMemTest, MinFbMemPercent,   UINT32,
                     "Required min %% of FB memory free");
CLASS_PROP_READWRITE(GpuMemTest, MaxFbMb,           FLOAT64,
                     "Max FB to test in MB.");
CLASS_PROP_READWRITE(GpuMemTest, ChunkSizeMb,       FLOAT64,
                     "Split FB into chunks of size ChunkSizeMb (aligned up to page size)");
CLASS_PROP_READWRITE(GpuMemTest, Start,             UINT32,
                     "Start location (absolute FB offset, in MB)");
CLASS_PROP_READWRITE(GpuMemTest, End,               UINT32,
                     "End location (absolute FB offset, in MB)");
CLASS_PROP_READWRITE(GpuMemTest, L2ModeToTest,      UINT32,
                     "Set L2 disable/enable/default");
CLASS_PROP_READWRITE(GpuMemTest, NumRechecks,       UINT32,
                     "Set no. of retries in case of miscompare in sysmem");
CLASS_PROP_READWRITE(GpuMemTest, AutoRefreshValue,  UINT32,
                     "Set the memory auto refresh register value");
CLASS_PROP_READWRITE(GpuMemTest, DumpLaneRepair,    string,
                     "Dump a JavaScript lane-repair file. The argument is the filename");
P_(GpuMemTest_Get_ErrorCount);
static SProperty ErrorCount
(
    GpuMemTest_Object,
    "ErrorCount",
    0,
    0,
    GpuMemTest_Get_ErrorCount,
    0,
    0,
    "Errors in last iteration of the memory test"
);

P_(GpuMemTest_Get_LaneErrorCount);
static SProperty LaneErrorCount
(
    GpuMemTest_Object,
    "LaneErrorCount",
    0,
    0,
    GpuMemTest_Get_LaneErrorCount,
    0,
    0,
    "Number of lane errors in the last memory test"
);

C_( GpuMemTest_PrintErrors);
static STest GpuMemTest_PrintErrors
(
    GpuMemTest_Object,
    "PrintErrors",
    C_GpuMemTest_PrintErrors,
    0,
    "Prints Errors Encountered in the Last Run"
);

C_( GpuMemTest_PrintDescriptions);
static STest GpuMemTest_PrintDescriptions
(
    GpuMemTest_Object,
    "PrintDescriptions",
    C_GpuMemTest_PrintDescriptions,
    0,
    "Prints Errors descriptions and number of times they are encountered"
    " in the Last Run"
);

C_(GpuMemTest_Status);
static STest GpuMemTest_Status
(
    GpuMemTest_Object,
    "Status",
    C_GpuMemTest_Status,
    0,
    "Prints Information about the last run"
);

C_(GpuMemTest_GetLaneError);
static STest GpuMemTest_GetLaneError
(
    GpuMemTest_Object,
    "GetLaneError",
    C_GpuMemTest_GetLaneError,
    2,
    "Get information about a lane error from last run memory run"
);

C_(GpuMemTest_Bits);
static STest GpuMemTest_Bits
(
    GpuMemTest_Object,
    "Bits",
    C_GpuMemTest_Bits,
    0,
    "Prints detailed per-bit info from the last run"
);

C_(GpuMemTest_ErrorInfo);
static STest GpuMemTest_ErrorInfo
(
    GpuMemTest_Object,
    "ErrorInfo",
    C_GpuMemTest_ErrorInfo,
    0,
    "Get error information for a specific partition/subpartition"
);

C_(GpuMemTest_ErrorInfoBits);
static STest GpuMemTest_ErrorInfoBits
(
    GpuMemTest_Object,
    "ErrorInfoBits",
    C_GpuMemTest_ErrorInfoBits,
    0,
    "Get per-bit error information for a specific partition/subpartition"
);

C_(GpuMemTest_PrintErrors)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 count;

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    if (NumArguments==0)
        count = 15;

    else if (NumArguments == 1)
    {
        if (OK != pJavaScript->FromJsval(pArguments[0], &count))
        {
            JS_ReportWarning(pContext, "Error in %s.PrintErrors()",
                           pTest->GetName().c_str());
            RETURN_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        JS_ReportWarning(
                pContext,
                "Usage: %s.PrintErrors(Count); OK for count to be null",
                pTest->GetName().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    StickyRC firstRc;
    const GpuDevice *pGpuDevice = pTest->GetBoundGpuDevice();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        if (pTest->GetMemError(subdev).IsSetup())
        {
            Printf(Tee::PriNormal, "Subdevice %d\n", subdev);
            firstRc = pTest->GetMemError(subdev).PrintErrorLog(count);
        }
    }

    RETURN_RC(firstRc);
}

C_(GpuMemTest_PrintDescriptions)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 count;

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    if (NumArguments==0)
        count = 30;

    else if (NumArguments == 1)
    {
        if (OK != pJavaScript->FromJsval(pArguments[0], &count))
        {
            JS_ReportWarning(pContext, "Error in %s.PrintDescriptions()",
                             pTest->GetName().c_str());
            RETURN_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        JS_ReportWarning(pContext,
                         "Usage: %s.PrintDescriptions(Count); "
                         "OK for count to be null",
                         pTest->GetName().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    StickyRC firstRc;
    const GpuDevice *pGpuDevice = pTest->GetBoundGpuDevice();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        if (pTest->GetMemError(subdev).IsSetup())
        {
            Printf(Tee::PriNormal, "Subdevice %d\n", subdev);
            firstRc = pTest->GetMemError(subdev).PrintErrorsPerPattern(count);
        }
    }

    RETURN_RC(firstRc);
}

C_(GpuMemTest_Status)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    if (NumArguments != 0)
    {
        JS_ReportWarning(pContext, "Usage: %s.Status()",
                         pTest->GetName().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    StickyRC firstRc;
    const GpuDevice *pGpuDevice = pTest->GetBoundGpuDevice();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        if (pTest->GetMemError(subdev).IsSetup())
        {
            Printf(Tee::PriNormal, "Subdevice %d:\n", subdev);
            firstRc = pTest->GetMemError(subdev).PrintStatus();
        }
    }
    RETURN_RC(firstRc);
}

//-----------------------------------------------------------------------------
C_(GpuMemTest_GetLaneError)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    if (NumArguments != 2)
    {
        JS_ReportWarning(pContext, "Usage: %s.GetLaneError(errIdx, outObj)",
                         pTest->GetName().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    UINT64 errIdx = 0;
    JSObject *pRetObj = nullptr;
    if (OK != pJs->FromJsval(pArguments[0], &errIdx) ||
        OK != pJs->FromJsval(pArguments[1], &pRetObj))
    {
        JS_ReportError(pContext, "Error in %s.GetLaneError(idx, outObj)",
                       pTest->GetName().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    RC rc;
    const GpuDevice *pGpuDevice = pTest->GetBoundGpuDevice();
    if (pGpuDevice->GetNumSubdevices() != 1)
    {
        Printf(Tee::PriError, "HBM MemRepair lwrrently only supports repairing 1 gpu at a time\n");
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        if (pTest->GetMemError(subdev).IsSetup())
        {
            vector<LaneError> lanes = pTest->GetMemError(subdev).GetLaneErrors();
            if (errIdx >= lanes.size())
            {
                Printf(Tee::PriError, "Invalid errIdx: %llu, max possible: %zu\n", errIdx,
                    lanes.size());
                RETURN_RC(RC::SOFTWARE_ERROR);
            }

            GpuSubdevice* pSubdev = pGpuDevice->GetSubdevice(subdev);
            const UINT32 subp = lanes[errIdx].laneBit / pSubdev->GetFB()->GetSubpartitionBusWidth();
            C_CHECK_RC(pJs->PackFields(pRetObj, "sIIIs",
                "LaneName",     lanes[errIdx].ToString().c_str(),
                "HwFbio",       lanes[errIdx].hwFbpa.Number(), // TODO(stewarts): need to change this field to hwFbpa, but lwstomers use this
                "Subpartition", subp,
                "LaneBit",      lanes[errIdx].laneBit,
                "RepairType",   LaneError::GetLaneTypeString(lanes[errIdx].laneType).c_str()));
        }
    }
    RETURN_RC(rc);
}

C_(GpuMemTest_Bits)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    if (NumArguments != 0)
    {
        JS_ReportWarning(pContext, "Usage: %s.Bits()",
                         pTest->GetName().c_str());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    StickyRC firstRc;
    const GpuDevice *pGpuDevice = pTest->GetBoundGpuDevice();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        if (pTest->GetMemError(subdev).IsSetup())
        {
            Printf(Tee::PriNormal, "Subdevice %d\n", subdev);
            firstRc = pTest->GetMemError(subdev).ErrorBitsDetailed();
        }
    }
    RETURN_RC(firstRc);
}

P_(GpuMemTest_Get_ErrorCount)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    UINT64 returlwal = 0;

    const GpuDevice *pGpuDevice = pTest->GetBoundGpuDevice();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        if (pTest->GetMemError(subdev).IsSetup())
        {
            returlwal += pTest->GetMemError(subdev).GetErrorCount();
        }
    }

    if (OK != JavaScriptPtr()->ToJsval(returlwal, pValue))
    {
        JS_ReportError(pContext, "Failed to get %s.ErrorCount.",
                       pTest->GetName().c_str());
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(GpuMemTest_Get_LaneErrorCount)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    UINT64 returlwal = 0;

    const GpuDevice *pGpuDevice = pTest->GetBoundGpuDevice();
    if (pGpuDevice->GetNumSubdevices() != 1)
    {
        Printf(Tee::PriError, "HBM LaneRepair lwrrently only supports repairing 1 gpu at a time\n");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        if (pTest->GetMemError(subdev).IsSetup())
        {
            returlwal = pTest->GetMemError(subdev).GetLaneErrors().size();
        }
    }

    if (OK != JavaScriptPtr()->ToJsval(returlwal, pValue))
    {
        JS_ReportError(pContext, "Failed to get %s.LaneErrorCount.",
                       pTest->GetName().c_str());
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(GpuMemTest_ErrorInfo)
{
    JavaScriptPtr pJs;

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    if ((NumArguments != 1) && (NumArguments != 2) && (NumArguments != 9))
    {
        JS_ReportWarning(pContext,
                "Usage: %s.ErrorInfo(virtualFbio) to print to screen or",
                pTest->GetName().c_str());
        JS_ReportWarning(pContext,
                "       %s.ErrorInfo(virtualFbio, [error count]) or",
                pTest->GetName().c_str());
        JS_ReportWarning(pContext,
                "       %s.ErrorInfo(virtualFbio,",
                pTest->GetName().c_str());
        JS_ReportWarning(pContext, "                rankMask,");
        JS_ReportWarning(pContext, "                beatMask,");
        JS_ReportWarning(pContext, "                includeReadErrors,");
        JS_ReportWarning(pContext, "                includeWriteErrors,");
        JS_ReportWarning(pContext, "                includeUnknownErrors,");
        JS_ReportWarning(pContext, "                includeR0X1,");
        JS_ReportWarning(pContext, "                includeR1X0,");
        JS_ReportWarning(pContext, "                [error count])");
        return JS_FALSE;
    }

    RC rc;
    UINT32    virtualFbio      = 0;
    UINT32    rankMask         = 0xffffffffU;
    UINT32    beatMask         = 0xffffffffU;
    bool      incReadErrors    = true;
    bool      incWriteErrors   = true;
    bool      inlwnknownErrors = true;
    bool      incR0X1          = true;
    bool      incR1X0          = true;
    JSObject *pReturlwals      = nullptr;

    if (OK != pJs->FromJsval(pArguments[0], &virtualFbio))
    {
        JS_ReportError(pContext, "Error in %s.ErrorInfo()",
                       pTest->GetName().c_str());
        return JS_FALSE;
    }

    if (NumArguments == 2)
    {
        if (OK != pJs->FromJsval(pArguments[1], &pReturlwals))
        {
            JS_ReportError(pContext, "Error in %s.ErrorInfo()",
                           pTest->GetName().c_str());
            return JS_FALSE;
        }
    }
    else if (NumArguments == 9)
    {
        if (OK != pJs->FromJsval(pArguments[1], &rankMask)         ||
            OK != pJs->FromJsval(pArguments[2], &beatMask)         ||
            OK != pJs->FromJsval(pArguments[3], &incReadErrors)    ||
            OK != pJs->FromJsval(pArguments[4], &incWriteErrors)   ||
            OK != pJs->FromJsval(pArguments[5], &inlwnknownErrors) ||
            OK != pJs->FromJsval(pArguments[6], &incR0X1)          ||
            OK != pJs->FromJsval(pArguments[7], &incR1X0)          ||
            OK != pJs->FromJsval(pArguments[8], &pReturlwals)        )
        {
            JS_ReportError(pContext, "Error in %s.ErrorInfo()",
                           pTest->GetName().c_str());
            return JS_FALSE;
        }
    }

    vector<UINT64> errBits;
    UINT64 errorCount = 0;
    C_CHECK_RC(pTest->GetMemError(0).ErrorInfoPerBit(virtualFbio,
                                                     rankMask,
                                                     beatMask,
                                                     incReadErrors,
                                                     incWriteErrors,
                                                     inlwnknownErrors,
                                                     incR0X1,
                                                     incR1X0,
                                                     &errBits,
                                                     &errorCount));

    if (pReturlwals)
    {
        /* The user passed in an array, so store the data there */
        C_CHECK_RC(pJs->SetElement(pReturlwals, 0, errorCount));
    }
    else
    {
        /* The user didn't pass in an array, so print to screen */
        Printf(Tee::PriNormal, "Error count: %12llu\n", errorCount);
    }
    RETURN_RC(OK);
}

C_(GpuMemTest_ErrorInfoBits)
{
    JavaScriptPtr pJs;

    GpuMemTest *pTest = (GpuMemTest *)JS_GetPrivate(pContext, pObject);

    if ((NumArguments != 1) && (NumArguments != 2) && (NumArguments != 9))
    {
        JS_ReportWarning(pContext,
                "Usage: %s.ErrorInfoBits(virtualFbio) to print to screen or",
                pTest->GetName().c_str());
        JS_ReportWarning(pContext,
                "       %s.ErrorInfoBits(virtualFbio, [array]) or",
                pTest->GetName().c_str());
        JS_ReportWarning(pContext,
                "       %s.ErrorInfoBits(virtualFbio,",
                pTest->GetName().c_str());
        JS_ReportWarning(pContext, "                rankMask,");
        JS_ReportWarning(pContext, "                beatMask,");
        JS_ReportWarning(pContext, "                includeReadErrors,");
        JS_ReportWarning(pContext, "                includeWriteErrors,");
        JS_ReportWarning(pContext, "                includeUnknownErrors,");
        JS_ReportWarning(pContext, "                includeR0X1,");
        JS_ReportWarning(pContext, "                includeR1X0,");
        JS_ReportWarning(pContext, "                [array])");
        return JS_FALSE;
    }

    RC rc;
    UINT32    virtualFbio      = 0;
    UINT32    rankMask         = 0xffffffffU;
    UINT32    beatMask         = 0xffffffffU;
    bool      incReadErrors    = true;
    bool      incWriteErrors   = true;
    bool      inlwnknownErrors = true;
    bool      incR0X1          = true;
    bool      incR1X0          = true;
    JSObject *pReturlwals      = nullptr;

    if (OK != pJs->FromJsval(pArguments[0], &virtualFbio))
    {
        JS_ReportError(pContext, "Error in %s.ErrorInfoBits()",
                       pTest->GetName().c_str());
        return JS_FALSE;
    }

    if (NumArguments == 2)
    {
        if (OK != pJs->FromJsval(pArguments[1], &pReturlwals))
        {
            JS_ReportError(pContext, "Error in %s.ErrorInfoBits()",
                           pTest->GetName().c_str());
            return JS_FALSE;
        }
    }
    else if (NumArguments == 9)
    {
        if (OK != pJs->FromJsval(pArguments[1], &rankMask)         ||
            OK != pJs->FromJsval(pArguments[2], &beatMask)         ||
            OK != pJs->FromJsval(pArguments[3], &incReadErrors)    ||
            OK != pJs->FromJsval(pArguments[4], &incWriteErrors)   ||
            OK != pJs->FromJsval(pArguments[5], &inlwnknownErrors) ||
            OK != pJs->FromJsval(pArguments[6], &incR0X1)          ||
            OK != pJs->FromJsval(pArguments[7], &incR1X0)          ||
            OK != pJs->FromJsval(pArguments[8], &pReturlwals)        )
        {
            JS_ReportError(pContext, "Error in %s.ErrorInfoBits()",
                           pTest->GetName().c_str());
            return JS_FALSE;
        }
    }

    vector<UINT64> errBits;
    UINT64 errorCount = 0;
    C_CHECK_RC(pTest->GetMemError(0).ErrorInfoPerBit(virtualFbio,
                                                     rankMask,
                                                     beatMask,
                                                     incReadErrors,
                                                     incWriteErrors,
                                                     inlwnknownErrors,
                                                     incR0X1,
                                                     incR1X0,
                                                     &errBits,
                                                     &errorCount));

    if (pReturlwals)
    {
        /* The user passed in an array, so store the data there */
        JsArray jsArrayErrBits;
        for (UINT64 errCount: errBits)
        {
            jsval jsvalErrCount;
            C_CHECK_RC(pJs->ToJsval(errCount, &jsvalErrCount));
            jsArrayErrBits.push_back(jsvalErrCount);
        }
        C_CHECK_RC(pJs->SetElement(pReturlwals, 0, &jsArrayErrBits));
    }
    else
    {
        /* The user didn't pass in an array, so print to screen */
        Printf(Tee::PriNormal, "Error counts for the %u bits:\n",
               static_cast<UINT32>(errBits.size()));
        string buffer;
        for (UINT32 ii = 0; ii < errBits.size();  ++ii)
        {
            buffer += Utility::StrPrintf(" %12llu", errBits[ii]);
            if ((ii % 8) == 7 || ii == errBits.size() - 1) // end of line
            {
                Printf(Tee::PriNormal, "%8u:%s\n", ii & ~0x7, buffer.c_str());
                buffer.clear();
            }
        }
    }

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
RC GpuMemTest::OnPreRun(const CallbackArguments &args)
{
    SetIntermittentError(false);
    return OK;
}

//-----------------------------------------------------------------------------
RC GpuMemTest::OnPostRun(const CallbackArguments &args)
{
    StickyRC firstRc;

    // Read the RC from Run()
    INT32 RcFromRun = OK;
    firstRc = args.At(0, &RcFromRun);
    firstRc = RcFromRun;

    // Check for error-counter errors, such as ECC errors
    firstRc = ErrCounter::CheckExitStackFrame(this, GetRunFrameId());
    // Print intermittent errors, if any
    PrintIntermittentError(firstRc);

    if (!m_DumpLaneRepair.empty())
    {
        DumpLaneRepairFile();
    }

    return firstRc;
}

//-----------------------------------------------------------------------------
RC GpuMemTest::DumpLaneRepairFile()
{
    RC rc;
    MASSERT(!m_DumpLaneRepair.empty());

    MemError* pMemError = &GetMemError(0);
    vector<LaneError> laneErrors = pMemError->GetLaneErrors();

    // Open the file
    FileHolder fileHolder;
    CHECK_RC(fileHolder.Open(m_DumpLaneRepair, "w"));
    FILE* pFile = fileHolder.GetFile();

    // Write the file
    for (const LaneError& laneError : laneErrors)
    {
        switch (laneError.laneType)
        {
            case LaneError::RepairType::DATA:
            case LaneError::RepairType::DBI:
                break;
            default:
                MASSERT("!Illegal lane repair type!");
        }

        const UINT32 subp =
            laneError.laneBit / pMemError->GetFB()->GetSubpartitionBusWidth();
        const string laneTypeStr = LaneError::GetLaneTypeString(laneError.laneType);

        fprintf(pFile, "g_FailingLanes[\"%s\"] = {\n",    laneError.ToString().c_str());
        fprintf(pFile, "    \"LaneName\": \"%s\",\n",     laneError.ToString().c_str());
        fprintf(pFile, "    \"HwFbio\": \"%d\",\n",       laneError.hwFbpa.Number());
        fprintf(pFile, "    \"Subpartition\": \"%d\",\n", subp);
        fprintf(pFile, "    \"LaneBit\": \"%d\",\n",      laneError.laneBit);
        fprintf(pFile, "    \"RepairType\": \"%s\"\n",    laneTypeStr.c_str());
        fprintf(pFile, "};\n");
        fprintf(pFile, "\n");
    }

    return rc;
}
