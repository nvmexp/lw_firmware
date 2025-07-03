/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xbar/lwdaxbar.h"
#include "gpu/tests/lwdastst.h"
#include "core/include/framebuf.h"
#include "gpu/include/gpusbdev.h"

// Enable this define to dump L2 Cache data. Useful for verifying that patterns
// are being setup correctly
//#define DUMP_DEBUG_DATA 1

//! LWCA-based test for stressing the GPU Xbar Interface
//!
//! This test runs a LWCA kernel to perform read/writes independently targetting
//! L2 Cache. The memory is organized in such a way that the memory region being
//! read/written from/to by each SM will fit in L2 cache.
//!
class LwdaXbar : public LwdaStreamTest
{
public:
    LwdaXbar();
    virtual ~LwdaXbar() { }
    virtual bool IsSupported();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // Select how L2 slice patterns are interleaved when in XBAR_MODE_L2 mode
    enum SliceInterleaveModeEnum
    {
        SIL_OFF = 0, // No interleaving, lines in L2 are defined as 0 lines, or Pattern lines
        SIL_4X  = 1, // Min interleaving, Ex: PPPP0000
        SIL_2X  = 2, // Med interleaving, Ex: PP00PP00
        SIL_1X  = 3, // Max interleaving, Ex: P0P0P0P0
    };

    // JS property accessors
    SETGET_PROP(Mode,               UINT32);
    SETGET_PROP(SwitchingMode,      UINT32);
    SETGET_PROP(RuntimeMs,          UINT32);
    SETGET_PROP(Iterations,         UINT64);
    SETGET_PROP(InnerIterations,    UINT64);
    SETGET_PROP(NumBlocks,          UINT32);
    SETGET_PROP(NumThreads,         UINT32);
    SETGET_PROP(MaxErrorsPerBlock,  UINT32);
    SETGET_PROP(EnableSecondaryPortMask, UINT64);
    SETGET_PROP(EnableSecondaryPortMaskUpper, UINT64);
    SETGET_PROP(SliceInterleaveMode, UINT32);
    SETGET_PROP(NumL2LinesToRead,   UINT32);
    SETGET_PROP(DefaultPat,         UINT32);
    SETGET_PROP(TimeoutSec,         UINT32);
    SETGET_PROP(SyncLoops,          UINT64);

private:
    static const UINT32 SLICE_STRIDE_DWORDS = 64;
    static const UINT32 SLICE_STRIDE_BYTES = SLICE_STRIDE_DWORDS * sizeof(UINT32);
    static const UINT32 NUM_BITS_PER_DWORD = 32;
    static const UINT32 NUM_BITS_PER_XBAR_BUS = NUM_BITS_PER_DWORD*MAX_XBAR_PATS;
    static const UINT32 BIT_MASK_MAX_SIZE = 64;

    static void ColwertPatternsToLogical(const vector<UINT32> &map, XbarPatterns *pats);

    RC GetPatternData(const char* propName, XbarModeMask mode, XbarPatterns *retPats);

    void CallwlateKernelGeometry();
    RC   ToggleAllSecondaryPorts(UINT64 maskLower, UINT64 maskUpper);
    RC   SaveOrigSecondaryPorts();
    void SetDefaultPatterns();
    void SetL2PatternsAlt();

    RC DumpFbData(UINT32 block, UINT64 offsetDwords, UINT64 numDwords);
    RC ReportErrors();
    UINT32 GetResultsSize() const;

    static constexpr UINT64 s_InnerIterationsNotSet = ~0ULL;

    // JS properties
    UINT32 m_Mode;
    UINT32 m_SwitchingMode;
    UINT32 m_RuntimeMs;
    UINT64 m_Iterations;
    UINT64 m_InnerIterations;
    UINT32 m_NumBlocks;
    UINT32 m_NumThreads;
    UINT32 m_MaxErrorsPerBlock;
    UINT64 m_EnableSecondaryPortMask;
    UINT64 m_EnableSecondaryPortMaskUpper;
    UINT32 m_SliceInterleaveMode;
    UINT32 m_NumL2LinesToRead;
    UINT32 m_DefaultPat;
    UINT32 m_TimeoutSec;
    UINT64 m_SyncLoops;
    vector<UINT32> m_PhysToLogicalPinMap;

    Lwca::Module        m_Module;
    Lwca::Function      m_LwdaXbar;
    Lwca::Function      m_LwdaInitXbar;
    Lwca::ClientMemory  m_LwdaMem;
    Surface2D           m_LwdaMemSurf;
    Lwca::HostMemory    m_LwdaResultsMem;

    UINT32          m_DwordPerSM;
    UINT32          m_NumL2LinesPerSM;
    UINT32          m_NumL2Slices;
    UINT64          m_OrigSecondaryPortMask;
    UINT64          m_OrigSecondaryPortMaskUpper;
    XbarPatterns    m_RdPatterns;
    XbarPatterns    m_WrPatterns;
    vector< XbarPatterns > m_L2Patterns;
    vector< XbarPatterns > m_L2PatternsAlt;
    UINT32 m_PteKind = 0;
    UINT32 m_PageSizeKB = 0;
};

JS_CLASS_INHERIT(LwdaXbar, LwdaStreamTest, "LWCA Xbar Interface Test");

CLASS_PROP_READWRITE(LwdaXbar, Mode, UINT32,
                     "Mode Mask: Bit 0->Read, Bit 1->Write, Bit 2->Use pin mapping, Bit 3->L2 Slice Patterns");//$
CLASS_PROP_READWRITE(LwdaXbar, SwitchingMode, UINT32,
                     "SwitchingMode: 0->Zeroes (default), 1->Ones, 2->Ilwerted");
CLASS_PROP_READWRITE(LwdaXbar, RuntimeMs, UINT32,
                     "Requested time to run the active portion of the test in ms");
CLASS_PROP_READWRITE(LwdaXbar, Iterations, UINT64,
                     "Number of times to execute LwdaXbar kernel");
CLASS_PROP_READWRITE(LwdaXbar, InnerIterations, UINT64,
                     "Number of times to repeatedly perform reads/writes inside LwdaXbar kernel");
CLASS_PROP_READWRITE(LwdaXbar, NumBlocks, UINT32,
                     "Number of kernel blocks to run");
CLASS_PROP_READWRITE(LwdaXbar, NumThreads, UINT32,
                     "Number of kernel threads to run");
CLASS_PROP_READWRITE(LwdaXbar, MaxErrorsPerBlock, UINT32,
                     "Max number of errors reported per LWCA block (0=default)");
CLASS_PROP_READWRITE(LwdaXbar, EnableSecondaryPortMask, UINT64,
                     "Bit mask for ports 0-63 to specify which secondary ports should be "
                     "enabled. By default, all secondary ports are enabled");
CLASS_PROP_READWRITE(LwdaXbar, EnableSecondaryPortMaskUpper, UINT64,
                     "Bit mask for ports 64-127 to specify which secondary ports should be "
                     "enabled. By default, all secondary ports are enabled");
CLASS_PROP_READWRITE(LwdaXbar, SliceInterleaveMode, UINT32,
                     "How L2 slice patterns are interleaved when in XBAR_MODE_L2. 0->Off, 1->PPPP0000, 2->PP00PP00, 3->P0P0P0P0");
CLASS_PROP_READWRITE(LwdaXbar, NumL2LinesToRead, UINT32,
                     "When using both Rd and Wr mode, this parameter defines how many L2 lines are used for Rd mode; remainder are used for Wr mode");
CLASS_PROP_READWRITE(LwdaXbar, DefaultPat, UINT32,
                     "Default pattern to initialize all patterns that aren't overriden explicitly. Default DefaultPat=0xFFFFFFFF");
CLASS_PROP_READWRITE(LwdaXbar, TimeoutSec, UINT32,
                     "Timeout in seconds when test will stop waiting for kernel to complete. Default is no-timeout");//$
CLASS_PROP_READWRITE(LwdaXbar, SyncLoops, UINT64,
                     "When TimeoutSec is set, number of inner loops (ie. number of kernel launches) before we perform a polling sync. Default is 128");//$

LwdaXbar::LwdaXbar()
: m_Mode(XBAR_MODE_RD | XBAR_MODE_WR | XBAR_MODE_L2)
, m_SwitchingMode(XBAR_SWITCHING_MODE_ZEROES)
, m_RuntimeMs(0)
, m_Iterations(0)
, m_InnerIterations(s_InnerIterationsNotSet)
, m_NumBlocks(0)
, m_NumThreads(1024)
, m_MaxErrorsPerBlock(0)
, m_EnableSecondaryPortMask(_UINT64_MAX)
, m_EnableSecondaryPortMaskUpper(_UINT64_MAX)
, m_SliceInterleaveMode(SIL_1X)
, m_NumL2LinesToRead(_UINT32_MAX)
, m_DefaultPat(0xFFFFFFFF)
, m_TimeoutSec(20)
, m_SyncLoops(32)
, m_DwordPerSM(0)
, m_NumL2LinesPerSM(0)
, m_NumL2Slices(0)
, m_OrigSecondaryPortMask(_UINT64_MAX)
, m_OrigSecondaryPortMaskUpper(_UINT64_MAX)
{
    SetName("LwdaXbar");
}

bool LwdaXbar::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    // We need to compile the xbar lwbins for new arches we wish to support
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_60 ||
        (GetBoundGpuSubdevice()->IsSOC() && cap < Lwca::Capability::SM_86))
    {
        Printf(Tee::PriLow, "%s is not available for SM %d.%d products\n",
               GetName().c_str(), cap.MajorVersion(), cap.MinorVersion());
        return false;
    }

    return true;
}

void LwdaXbar::ColwertPatternsToLogical(const vector<UINT32> &map, XbarPatterns *pats)
{
    MASSERT(pats);
    MASSERT(map.size());

    UINT32 colwPat[MAX_XBAR_PATS] = {0};

    for (UINT32 i = 0; i < MAX_XBAR_PATS; i++)
    {
        for (UINT32 j = 0; j < NUM_BITS_PER_DWORD; j++)
        {
            // Need to flip the index to account for little-endianess. Basically,
            // the last element in the Map array is Bit0
            UINT32 idxMap = (MAX_XBAR_PATS * NUM_BITS_PER_DWORD) - (i * NUM_BITS_PER_DWORD + j) - 1;

            // Determine which dword/bit need to be set in the new array
            UINT32 colwIdxDword = map[idxMap] / NUM_BITS_PER_DWORD;
            UINT32 colwIdxBit = map[idxMap] % NUM_BITS_PER_DWORD;

            // Determine physical bit value from user parameter, then translate
            // it to logical location and set it in a new pattern struct
            UINT32 bitVal = (pats->p[i] & (0x1U << j));
            colwPat[colwIdxDword] |= (bitVal << colwIdxBit);
        }
    }

    // Copy over the colwerted patterns
    for (UINT32 i = 0; i < MAX_XBAR_PATS; i++)
    {
        pats->p[i] = colwPat[i];
    }
}

RC LwdaXbar::GetPatternData(const char* propName, XbarModeMask mode, XbarPatterns *retPats)
{
    JavaScriptPtr pJs;
    vector<UINT32> propVals;
    RC rc = pJs->GetProperty(GetJSObject(), propName, &propVals);
    if (!(m_Mode & mode) && rc == OK)
    {
        Printf(Tee::PriError, "Cannot set %s when not running %s Mode\n", propName, ((mode & XBAR_MODE_RD) ? "Read" : "Write"));
        rc = RC::ILWALID_ARGUMENT;
    }
    else if (rc == RC::ILWALID_OBJECT_PROPERTY)
    {
        // If no pattern was provided, we'll exit early and use default patterns
        return OK;
    }
    CHECK_RC(rc);

    if (propVals.size() != MAX_XBAR_PATS)
    {
        Printf(Tee::PriError, "An incorrect number of %s patterns (%zu) was provided, expected: %u\n", propName, propVals.size(), MAX_XBAR_PATS);
        return RC::ILWALID_ARGUMENT;
    }

    // Copy over the patterns in reverse order due to endianess
    for (UINT32 i = 0; i < propVals.size(); i++)
    {
        retPats->p[i] = propVals[propVals.size() - i - 1];
    }

    VerbosePrintf("%s Pattern: %08X_%08X_%08X_%08X_%08X_%08X_%08X_%08X\n", propName, retPats->p[7], retPats->p[6], retPats->p[5], retPats->p[4], retPats->p[3], retPats->p[2], retPats->p[1], retPats->p[0]);

    if (m_Mode & XBAR_MODE_PM)
    {
        ColwertPatternsToLogical(m_PhysToLogicalPinMap, retPats);
        VerbosePrintf("Colw Rd Pattern: %08X_%08X_%08X_%08X_%08X_%08X_%08X_%08X\n", retPats->p[7], retPats->p[6], retPats->p[5], retPats->p[4], retPats->p[3], retPats->p[2], retPats->p[1], retPats->p[0]);
    }

    return rc;
}

RC LwdaXbar::InitFromJs()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::InitFromJs());

    if (m_InnerIterations == s_InnerIterationsNotSet)
    {
        m_InnerIterations = GetBoundGpuSubdevice()->GetTpcCount() * 192;
    }

    JavaScriptPtr pJs;
    rc = pJs->GetProperty(GetJSObject(), "PhysToLogicalPinMap", &m_PhysToLogicalPinMap);
    if (rc == RC::ILWALID_OBJECT_PROPERTY && !(m_Mode & XBAR_MODE_PM))
    {
        rc.Clear();
    }
    else if (m_Mode & XBAR_MODE_PM)
    {
        if (rc == RC::ILWALID_OBJECT_PROPERTY)
        {
            Printf(Tee::PriError, "PhysToLogicalPinMap must be set when using PinMapping mode\n");
            rc = RC::ILWALID_ARGUMENT;
        }
        else if (rc == OK && m_PhysToLogicalPinMap.size() != NUM_BITS_PER_XBAR_BUS)
        {
            Printf(Tee::PriError, "PhysToLogicalPinMap does not have correct number of elements: %zu. Expected: %u\n", m_PhysToLogicalPinMap.size(), NUM_BITS_PER_XBAR_BUS);
            rc = RC::ILWALID_ARGUMENT;
        }
    }
    CHECK_RC(rc);

    CHECK_RC(GetPatternData("RdPat", XBAR_MODE_RD, &m_RdPatterns));
    CHECK_RC(GetPatternData("WrPat", XBAR_MODE_WR, &m_WrPatterns));

    if (m_Mode & XBAR_MODE_L2)
    {
        for (UINT32 i = 0; i < m_NumL2Slices; i++)
        {
            string prop = Utility::StrPrintf("L2Pat%02u", i);
            CHECK_RC(GetPatternData(prop.c_str(), XBAR_MODE_L2, &m_L2Patterns[i]));
        }

        if (m_SwitchingMode != XBAR_SWITCHING_MODE_ZEROES)
        {
            SetL2PatternsAlt();
        }
    }

    switch (m_SliceInterleaveMode)
    {
        case SIL_OFF:
        case SIL_4X:
        case SIL_2X:
        case SIL_1X:
            break;

        default:
            Printf(Tee::PriError, "Invalid Slice Interleave Mode: %u\n", m_SliceInterleaveMode);
            return RC::ILWALID_ARGUMENT;
    }

    if (m_EnableSecondaryPortMask != _UINT64_MAX ||
         m_EnableSecondaryPortMaskUpper != _UINT64_MAX)
    {
        if (!GetBoundGpuSubdevice()->IsXbarSecondaryPortSupported())
        {
            Printf(Tee::PriError, "Can't set EnableSecondaryPortMask, "
                                    "there is no secondary port on this GPU!\n");
            return RC::ILWALID_ARGUMENT;
        }
        else if (!GetBoundGpuSubdevice()->IsXbarSecondaryPortConfigurable())
        {
            Printf(Tee::PriError, "Can't set EnableSecondaryPortMask, "
                                    "the port is priv protected!\n");
            return RC::ILWALID_ARGUMENT;
        }
    }

    if (m_Mode & XBAR_MODE_RD_CMP)
    {
        if (!(m_Mode & XBAR_MODE_RD))
        {
            Printf(Tee::PriError, "Cannot enable read-compare without read mode set!\n");
            return RC::ILWALID_ARGUMENT;
        }
        if (!(m_Mode & XBAR_MODE_L2))
        {
            Printf(Tee::PriError, "Read compare only supported with L2 slice mode!\n");
            return RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}

void LwdaXbar::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "LwdaXbar Js Properties:\n");
    Printf(pri, "\tSwitchingMode:\t\t%u\n", m_SwitchingMode);
    Printf(pri, "\tRuntimeMs:\t\t\t%u\n", m_RuntimeMs);
    Printf(pri, "\tIterations:\t\t\t%llu\n", m_Iterations);
    Printf(pri, "\tInnerIterations:\t\t%llu\n", m_InnerIterations);
    Printf(pri, "\tNumBlocks:\t\t\t%u\n", m_NumBlocks);
    Printf(pri, "\tNumThreads:\t\t\t%u\n", m_NumThreads);
    Printf(pri, "\tMaxErrorsPerBlock:\t\t%u\n", m_MaxErrorsPerBlock);
    Printf(pri, "\tEnableSecondaryPortMask:\t\t0x%016llx\n", m_EnableSecondaryPortMask);
    Printf(pri, "\tEnableSecondaryPortMaskUpper:\t0x%016llx\n", m_EnableSecondaryPortMaskUpper);
    Printf(pri, "\tSliceInterleaveMode:\t\t%u\n", m_SliceInterleaveMode);
    Printf(pri, "\tNumL2LinesToRead:\t\t%u\n", m_NumL2LinesToRead);
    Printf(pri, "\tDefaultPat:\t\t\t0x%08x\n", m_DefaultPat);

    string output;
    if (m_PhysToLogicalPinMap.size())
    {
        output = Utility::StrPrintf("\tPhysToLogicalPinMap:\t[%u", m_PhysToLogicalPinMap[0]);
        for (UINT32 i = 1; i < m_PhysToLogicalPinMap.size(); i++)
        {
            output.append(Utility::StrPrintf(", %u", m_PhysToLogicalPinMap[i]));
        }
        Printf(pri, "%s]\n", output.c_str());
    }

    output = "\tMode:\t\t\t\t";
    if (m_Mode & XBAR_MODE_RD)
    {
        output += "Read ";
    }
    if (m_Mode & XBAR_MODE_WR)
    {
        output += "Write ";
    }
    if (m_Mode & XBAR_MODE_PM)
    {
        output += "PinMapping ";
    }
    if (m_Mode & XBAR_MODE_L2)
    {
        output += "L2Slices ";
    }
    if (m_Mode & XBAR_MODE_RD_CMP)
    {
        output += "ReadCompare ";
    }
    Printf(pri, "%s\n", output.c_str());

    if (m_Mode & XBAR_MODE_RD)
    {
        output = Utility::StrPrintf("\tRdPat:\t\t\t\t[0x%.8x", m_RdPatterns.p[MAX_XBAR_PATS - 1]);
        for (UINT32 i = 1; i < MAX_XBAR_PATS; i++)
        {
            output.append(Utility::StrPrintf(", 0x%.8x", m_RdPatterns.p[MAX_XBAR_PATS - i - 1]));
        }
        Printf(pri, "%s]\n", output.c_str());
    }
    if (m_Mode & XBAR_MODE_WR)
    {
        output = Utility::StrPrintf("\tWrPat:\t\t\t\t[0x%.8x", m_WrPatterns.p[MAX_XBAR_PATS - 1]);
        for (UINT32 i = 1; i < MAX_XBAR_PATS; i++)
        {
            output.append(Utility::StrPrintf(", 0x%.8x", m_WrPatterns.p[MAX_XBAR_PATS - i - 1]));
        }
        Printf(pri, "%s]\n", output.c_str());
    }
    if (m_Mode & XBAR_MODE_L2)
    {
        for (UINT32 slice = 0; slice < m_NumL2Slices; slice++)
        {
            output = Utility::StrPrintf("\tL2Pat%02u:\t\t\t[0x%.8x", slice, m_L2Patterns[slice].p[MAX_XBAR_PATS - 1]);
            for (UINT32 i = 1; i < MAX_XBAR_PATS; i++)
            {
                output.append(Utility::StrPrintf(", 0x%.8x", m_L2Patterns[slice].p[MAX_XBAR_PATS - i - 1]));
            }
            Printf(pri, "%s]\n", output.c_str());
        }
    }
}

RC LwdaXbar::ToggleAllSecondaryPorts(UINT64 maskLower, UINT64 maskUpper)
{
    RC rc;
    UINT64 mask;
    UINT32 offset;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // If there are no secondary ports on this device or we cannot change them, exit with RC::OK
    if (!pSubdev->IsXbarSecondaryPortSupported() ||
        !pSubdev->IsXbarSecondaryPortConfigurable())
    {
        return rc;
    }

    for (UINT32 i = 0; i < m_NumL2Slices; i++)
    {
        if (i < BIT_MASK_MAX_SIZE)
        {
            mask = maskLower;
            offset = i;
        }
        else
        {
            mask = maskUpper;
            offset = i - BIT_MASK_MAX_SIZE;
        }

        if ((mask & (1LL << offset)))
        {
            // Enable the given port
            CHECK_RC(pSubdev->EnableXbarSecondaryPort(i, true));
        }
        else
        {
            // Disable the given port
            CHECK_RC(pSubdev->EnableXbarSecondaryPort(i, false));
        }
    }

    return rc;
}

RC LwdaXbar::SaveOrigSecondaryPorts()
{
    RC rc;

    const GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    m_OrigSecondaryPortMask = 0;
    m_OrigSecondaryPortMaskUpper = 0;

    // If there are no secondary ports on this device or we cannot change them, exit with RC::OK
    if (!pSubdev->IsXbarSecondaryPortSupported() ||
        !pSubdev->IsXbarSecondaryPortConfigurable())
    {
        return rc;
    }

    for (UINT32 i = 0; i < m_NumL2Slices; i++)
    {
        bool enabled;
        CHECK_RC(pSubdev->GetXbarSecondaryPortEnabled(i, &enabled));
        if (enabled)
        {
            if (i < BIT_MASK_MAX_SIZE)
            {
                m_OrigSecondaryPortMask |= (0x1LL << i);
            }
            else
            {
                m_OrigSecondaryPortMaskUpper |= (0x1LL << (i - BIT_MASK_MAX_SIZE));
            }
        }
    }

    return rc;
}

void LwdaXbar::SetL2PatternsAlt()
{
    m_L2PatternsAlt.resize(m_L2Patterns.size());
    for (UINT32 i = 0; i < m_L2PatternsAlt.size(); i++)
    {
        for (UINT32 j = 0; j < MAX_XBAR_PATS; j++)
        {
            if (m_SwitchingMode == XBAR_SWITCHING_MODE_ILW)
            {
                m_L2PatternsAlt[i].p[j] = ~m_L2Patterns[i].p[j];
            }
            else
            {
                m_L2PatternsAlt[i].p[j] = _UINT32_MAX;
            }
        }
    }
}

void LwdaXbar::SetDefaultPatterns()
{
    m_NumL2Slices = GetBoundGpuSubdevice()->GetFB()->GetL2SliceCount();
    m_L2Patterns.resize(m_NumL2Slices);
    for (UINT32 i = 0; i < MAX_XBAR_PATS; i++)
    {
        m_RdPatterns.p[i] = m_DefaultPat;
        m_WrPatterns.p[i] = m_DefaultPat;
        for (UINT32 j = 0; j < m_NumL2Slices; j++)
        {
            m_L2Patterns[j].p[i] = m_DefaultPat;
        }
    }
}

UINT32 LwdaXbar::GetResultsSize() const
{
    // We subtract 1 from m_MaxErrorsPerBlock since the XbarRangeErrors struct
    // already reserves memory for 1 XbarBadValue
    const UINT32 blockSize = sizeof(XbarRangeErrors) +
                             (m_MaxErrorsPerBlock-1) * sizeof(XbarBadValue);
    return blockSize;
}

RC LwdaXbar::Setup()
{
    RC rc;

    SetDefaultPatterns();
    CHECK_RC(SaveOrigSecondaryPorts());

    CHECK_RC(LwdaStreamTest::Setup());

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("xbar1_", &m_Module));

    // Prepare kernels
    CallwlateKernelGeometry();
    m_LwdaInitXbar = m_Module.GetFunction("LwdaInitXbar", m_NumBlocks, m_NumThreads);
    CHECK_RC(m_LwdaInitXbar.InitCheck());

    m_LwdaXbar = m_Module.GetFunction("LwdaXbar", m_NumBlocks, m_NumThreads);
    CHECK_RC(m_LwdaXbar.InitCheck());

    // Allocate Memory that fits in L2
    UINT32 l2CacheSizeBytes = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_L2_CACHE_SIZE);

    VerbosePrintf("L2 Cache size: %d\n", l2CacheSizeBytes);

    // If there are uGPUs, FB and L2 cache is split between them.
    // Each uGPU has their own xbars, so we scale the memory allocated
    // This logic will need to be updated slightly if we have chips with >2 uGPUs in the future.
    UINT32 uGpuCount = Utility::CountBits(GetBoundGpuSubdevice()->GetUGpuMask());
    if (uGpuCount > 1)
    {
        l2CacheSizeBytes /= uGpuCount;
    }
    else if (uGpuCount == 1)
    {
        // Query the L2 cache size for the ugpu that is enabled.
        // This is a special case for bringup FS configs, where the FBP pairing rule
        // across uGPUs is not guaranteed when all GPCs on one uGPU have been floorswept.
        for (UINT32 i = 0; i < GetBoundGpuSubdevice()->GetMaxUGpuCount(); i++)
        {
            if (GetBoundGpuSubdevice()->IsUGpuEnabled(i))
            {
                l2CacheSizeBytes = GetBoundGpuSubdevice()->GetL2CacheSizePerUGpu(i);
                break;
            }
        }
    }

    UINT32 l2CacheSizeDwords = l2CacheSizeBytes / sizeof(UINT32);

    if (m_RuntimeMs && m_Iterations)
    {
        Printf(Tee::PriError, "RuntimeMs and Iterations can't be both non zero.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_Iterations)
    {
        Printf(Tee::PriWarn, "Please start using TestConfiguration.Loops instead of Iterations\n");
    }
    else if (m_RuntimeMs == 0)
    {
        m_Iterations = GetTestConfiguration()->Loops();
    }

    m_NumL2LinesPerSM = (UINT32)(l2CacheSizeDwords / m_NumThreads) / m_NumBlocks;
    m_DwordPerSM = m_NumL2LinesPerSM * m_NumThreads;

    // If NumL2LinesToRead was not set by user
    if (m_NumL2LinesToRead == _UINT32_MAX)
    {
        // If write-only
        if ((m_Mode & XBAR_MODE_WR) && !(m_Mode & XBAR_MODE_RD))
        {
            m_NumL2LinesToRead = 0;
        }
        // If Slice-Interleave mode is disabled, then only use two read-lines
        else if (m_SliceInterleaveMode == SIL_OFF)
        {
            m_NumL2LinesToRead = 2;
        }
        // If both read and write are enabled, and slice interleaving is enabled
        // then split read/writing into 2
        else if ((m_Mode & XBAR_MODE_RD) && (m_Mode & XBAR_MODE_WR))
        {
            m_NumL2LinesToRead = m_NumL2LinesPerSM / 2;
        }
    }
    // Cap the number of read-lines to m_NumL2LinesPerSM
    m_NumL2LinesToRead = min(m_NumL2LinesPerSM, m_NumL2LinesToRead);

    UINT32 allocMemSizeBytes = m_DwordPerSM * m_NumBlocks * sizeof(UINT32);

    VerbosePrintf("L2 Lines per SM: %u\n", m_NumL2LinesPerSM);
    VerbosePrintf("L2 Lines to be used for Reads: %u\n", m_NumL2LinesToRead);
    VerbosePrintf("DW per SM: %u\n", m_DwordPerSM);
    VerbosePrintf("Allocated Memory: %d\n", allocMemSizeBytes);

    // Allocate memory to test
    CHECK_RC(GetLwdaInstance().AllocSurfaceClientMem(allocMemSizeBytes, Memory::Fb,
                                                     GetBoundGpuSubdevice(),
                                                     &m_LwdaMemSurf, &m_LwdaMem));
    m_PteKind = m_LwdaMemSurf.GetPteKind();
    m_PageSizeKB = m_LwdaMemSurf.GetActualPageSizeKB();

    // Allocate memory for results
    if (m_MaxErrorsPerBlock == 0)
    {
        m_MaxErrorsPerBlock = 1024;
    }

    CHECK_RC(GetLwdaInstance().AllocHostMem(GetResultsSize(), &m_LwdaResultsMem));

    CHECK_RC(m_LwdaResultsMem.Clear());

    // Set the shared size of kernel to hold Errors (XbarBadValue)
    CHECK_RC(m_LwdaXbar.SetSharedSize(m_MaxErrorsPerBlock * sizeof(XbarBadValue)));

    // Initialize FB memory
    CHECK_RC(m_LwdaInitXbar.Launch(m_LwdaMem.GetDevicePtr(),
                                   m_DwordPerSM,
                                   m_NumL2LinesToRead,
                                   m_NumL2LinesPerSM,
                                   m_Mode,
                                   m_SwitchingMode,
                                   m_RdPatterns));

    if (m_Mode & XBAR_MODE_L2)
    {
        UINT64 physPtr = 0;
        CHECK_RC(m_LwdaMemSurf.GetPhysAddress(0, &physPtr));
        Printf(Tee::PriLow, "Physical pointer: 0x%0llx\n", physPtr);

        // Verify that memory is DWORD aligned
        MASSERT(!(physPtr & 0x3));
        MASSERT(!(allocMemSizeBytes & 0x3));

        const UINT32 bytesInPattern = MAX_XBAR_PATS*sizeof(UINT32);

        const UINT32 numPatsInSlice = (0x1U << m_SliceInterleaveMode);
        const UINT64 numFillBytes = SLICE_STRIDE_BYTES / numPatsInSlice;

        FbDecode fbDecode;
        FrameBuffer *fb = GetBoundGpuSubdevice()->GetFB();
        for (UINT32 block = 0; block < m_NumBlocks; block++)
        {
            if (m_SliceInterleaveMode == SIL_OFF)
            {
                for (UINT32 thread = 0; thread < m_NumThreads; thread += SLICE_STRIDE_DWORDS)
                {
                    // If the switching mode is set to zeroes, we leave the first l2 line as zeroes
                    UINT64 l2Line = (m_SwitchingMode == XBAR_SWITCHING_MODE_ZEROES) ? 1 : 0;
                    for (; l2Line < m_NumL2LinesToRead; l2Line++)
                    {
                        const UINT64 byteOffset = block*m_DwordPerSM*sizeof(UINT32) + m_NumThreads*(l2Line)*sizeof(UINT32) + thread*sizeof(UINT32);

                        CHECK_RC(m_LwdaMemSurf.GetPhysAddress(byteOffset, &physPtr));
                        CHECK_RC(fb->DecodeAddress(&fbDecode,
                                                   physPtr,
                                                   m_PteKind, m_PageSizeKB));
                        const UINT32 ltcid = fb->VirtualFbioToVirtualLtc(
                                    fbDecode.virtualFbio, fbDecode.subpartition);
                        const UINT32 l2Slice = fb->GetFirstGlobalSliceIdForLtc(ltcid) + fbDecode.slice;

                        const UINT32 *pPatToCopy = (l2Line ? &m_L2Patterns[l2Slice].p[0] : &m_L2PatternsAlt[l2Slice].p[0]);//$

                        for (UINT32 i = 0; i < SLICE_STRIDE_BYTES; i += bytesInPattern)
                        {
                            CHECK_RC(m_LwdaMem.Set(pPatToCopy, byteOffset + i, bytesInPattern));
                        }
                    }
                }
                continue;
            }

            UINT64 byteOffset = block * m_DwordPerSM * sizeof(UINT32);
            for (UINT64 l2Line = 0;
                 l2Line < static_cast<UINT64>(m_NumThreads)*m_NumL2LinesToRead;
                 l2Line += SLICE_STRIDE_DWORDS)
            {
                CHECK_RC(fb->DecodeAddress(&fbDecode, physPtr + byteOffset,
                                           m_PteKind, m_PageSizeKB));
                const UINT32 ltcid = fb->VirtualFbioToVirtualLtc(
                                fbDecode.virtualFbio, fbDecode.subpartition);
                const UINT32 l2Slice = fb->GetFirstGlobalSliceIdForLtc(ltcid) + fbDecode.slice;

                for (UINT32 pat = 0; pat < numPatsInSlice; pat++)
                {
                    UINT64 byteOffsetInSlice = byteOffset + pat*numFillBytes;
                    MASSERT(byteOffsetInSlice < allocMemSizeBytes);

                    MASSERT(l2Slice < m_L2Patterns.size());
                    const UINT32 *pPatToCopy = &m_L2Patterns[l2Slice].p[0];
                    if (pat % 2)
                    {
                        // If the switching mode is set to zeroes, we'll leave
                        // zeroes on every 2nd interleaved section, otherwise
                        // we'll set it to the appropriate pattern
                        if (m_SwitchingMode == XBAR_SWITCHING_MODE_ZEROES)
                        {
                            continue;
                        }

                        MASSERT(l2Slice < m_L2PatternsAlt.size());
                        pPatToCopy = &m_L2PatternsAlt[l2Slice].p[0];
                    }

                    // TODO: This could probably be programmed more efficiently,
                    // specifically by utilizing the GPU.
                    for (UINT32 l = 0; l < numFillBytes; l += bytesInPattern)
                    {
                        CHECK_RC(m_LwdaMem.Set(pPatToCopy, byteOffsetInSlice + l, bytesInPattern));
                    }
                }
                byteOffset += SLICE_STRIDE_BYTES;
            }
        }
    }
    CHECK_RC(GetLwdaInstance().Synchronize());

    CHECK_RC(ToggleAllSecondaryPorts(m_EnableSecondaryPortMask, m_EnableSecondaryPortMaskUpper));

    return rc;
}

RC LwdaXbar::Cleanup()
{
    StickyRC rc;
    rc = ToggleAllSecondaryPorts(m_OrigSecondaryPortMask, m_OrigSecondaryPortMaskUpper);

    m_LwdaMem.Free();
    m_LwdaMemSurf.Free();

    m_LwdaResultsMem.Free();

    m_Module.Unload();
    rc = LwdaStreamTest::Cleanup();
    return rc;
}

void LwdaXbar::CallwlateKernelGeometry()
{
    if (!m_NumBlocks)
    {
        m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();
    }
}

RC LwdaXbar::DumpFbData(UINT32 block, UINT64 offsetDwords, UINT64 numDwords)
{
    RC rc;

    UINT64 numBytes = numDwords*sizeof(UINT32);
    UINT64 offsetBytes = offsetDwords*sizeof(UINT32);

    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> > hostData(numBytes, UINT32(), Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    CHECK_RC(m_LwdaMem.Get(&hostData[0], block*m_DwordPerSM*sizeof(UINT32) + offsetBytes, hostData.size()*sizeof(UINT32)));
    CHECK_RC(GetLwdaInstance().Synchronize());
    for (UINT32 j = 0; j < numDwords; j++)
    {
        Printf(Tee::PriAlways, "Debug data[%u, %llu]: 0x%X\n", block, offsetDwords + j, hostData[j]);
    }

    return rc;
}

RC LwdaXbar::Run()
{
    RC rc;

    // Record progress of when LwdaXbar kernels have been launched. NOTE: This
    // doesn't tell us when the kernels actually execute.
    CHECK_RC(PrintProgressInit(m_RuntimeMs ? m_RuntimeMs : m_Iterations));

    VerbosePrintf("LwdaXbar is testing LWCA device \"%s\"\n", GetBoundLwdaDevice().GetName().c_str());

#ifdef DUMP_DEBUG_DATA
    for (UINT32 i = 0; i < m_NumBlocks; i++)
    {
        DumpFbData(i, 0, m_DwordPerSM);
    }
#endif

    Utility::StopWatch cpuTimer("LwdaXbar CPU runtime", Tee::PriLow);

    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());
    CHECK_RC(startEvent.Record());

    const UINT64 runStartMs = Xp::GetWallTimeMS();
    const UINT64 numIterations = m_RuntimeMs ? _UINT64_MAX : m_Iterations;
    for (UINT64 i = 0; i < numIterations; i++)
    {
        CHECK_RC(m_LwdaXbar.Launch(m_LwdaMem.GetDevicePtr(),
                                   m_LwdaResultsMem.GetDevicePtr(GetBoundLwdaDevice()),
                                   GetResultsSize(),
                                   m_MaxErrorsPerBlock,
                                   m_DwordPerSM,
                                   m_NumL2LinesToRead,
                                   m_NumL2LinesPerSM,
                                   m_InnerIterations,
                                   m_Mode,
                                   m_SwitchingMode,
                                   m_WrPatterns));

        const UINT64 progressTimeMs = Xp::GetWallTimeMS() - runStartMs;
        const UINT64 progressIteration = m_RuntimeMs ?
            ( progressTimeMs > m_RuntimeMs ? m_RuntimeMs : progressTimeMs) :
            i;
        // Notify when kernel has been queued
        CHECK_RC(PrintProgressUpdate(progressIteration));

        if (m_TimeoutSec && m_SyncLoops && !((i + 1) % m_SyncLoops))
        {
            VerbosePrintf("Syncing after kernel launch %llu\n", i);
            if (GetLwdaInstance().Synchronize(m_TimeoutSec) != OK)
            {
                Printf(Tee::PriError, "LwdaXbar launching kernels %llu-%llu took too long to complete, "
                                      "exiting!\n", i + 1 - m_SyncLoops, i);
                GpuHung(true);
            }
        }

        if (m_RuntimeMs && (progressTimeMs >= m_RuntimeMs))
        {
            break;
        }
    }

    CHECK_RC(stopEvent.Record());

    // If a timeout was provided, use a polling timeout; otherwise, rely on Lwca's timeout mechanism
    if (m_TimeoutSec)
    {
        if (stopEvent.Synchronize(m_TimeoutSec) != OK)
        {
            Printf(Tee::PriError, "LwdaXbar kernels took too long to complete, exiting!\n");
            GpuHung(true);
        }
    }
    else
    {
        CHECK_RC(stopEvent.Synchronize());
    }

    UINT32 kernelTimeMs = stopEvent - startEvent;

    // Notify once last kernel has finished exelwtion
    CHECK_RC(PrintProgressUpdate(m_RuntimeMs ? m_RuntimeMs : m_Iterations));

    if (m_Mode & XBAR_MODE_RD_CMP)
    {
        CHECK_RC(ReportErrors());
    }

    VerbosePrintf("LwdaXbar Kernel runtime: %.2f s\n", (float)kernelTimeMs/1000.0f);

    return rc;
}

RC LwdaXbar::ReportErrors()
{
    RC rc;

    FbDecode fbDecode;
    FrameBuffer *fb = GetBoundGpuSubdevice()->GetFB();

    Printf(Tee::PriLow, "ReportErrors\n");
    CHECK_RC(GetLwdaInstance().Synchronize());

    const XbarRangeErrors *errors = (const XbarRangeErrors *)(m_LwdaResultsMem.GetPtr());

    VerbosePrintf("Number of errors: %llu, Number of reported errors: %u\n",
        errors->numErrors, errors->numReportedErrors);

    for (UINT32 i = 0; i < errors->numReportedErrors; i++)
    {
        UINT64 physPtr = 0;
        CHECK_RC(m_LwdaMemSurf.GetPhysAddress(errors->reportedValues[i].offset, &physPtr));
        CHECK_RC(fb->DecodeAddress(&fbDecode, physPtr, m_PteKind, m_PageSizeKB));
        const UINT32 ltcid = fb->VirtualFbioToVirtualLtc(
                    fbDecode.virtualFbio, fbDecode.subpartition);
        const UINT32 l2Slice = fb->GetFirstGlobalSliceIdForLtc(ltcid) + fbDecode.slice; // This seems to be off by 1???

        VerbosePrintf(" #%u: Addr=0x%010llX Offset=0x%08llX Exp=0x%08X Act=0x%08X "
                        "B=%u T=%u Bits=0x%08X Smid=%u Slice=%u\n",
                        i,
                        physPtr,
                        errors->reportedValues[i].offset,
                        errors->reportedValues[i].expected,
                        errors->reportedValues[i].actual,
                        errors->reportedValues[i].block,
                        errors->reportedValues[i].thread,
                        errors->reportedValues[i].failingBits,
                        errors->reportedValues[i].smid,
                        l2Slice);
    }

    if (errors->numReportedErrors || errors->numErrors)
    {
        Printf(Tee::PriError, "LwdaXbar detected memory miscompares!\n");
        rc = RC::BAD_MEMORY;
    }

    return rc;
}
