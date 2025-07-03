/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// GsyncLink : Test for display interaces using the gsync external CRC hw

#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0073.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gsynccrc.h"
#include "gpu/include/gsyncdev.h"
#include "gpu/include/gsyncdisp.h"
#include "gputest.h"
#include "gpu/utility/surffill.h"
#include "Lwcm.h"
#ifndef INCLUDED_STL_UTILITY_H
#include <utility> //pair
#endif

// GsyncLink Class
//
// Test a Gsync enabled DP display by displaying various test patterns and then
// reading and checking CRCs from the Gsync device against callwlated CRCs
// or alternatively using golden values
//

class GsyncLink : public GpuTest
{
public:
    GsyncLink();
    virtual ~GsyncLink() {};

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);

    SETGET_PROP(NumFrameCrcs, UINT32);
    SETGET_PROP(CalcCrc, bool);
    SETGET_PROP(IncludeRandom, bool);
    SETGET_PROP(EnableXbarCombinations, bool);
    SETGET_PROP(RunParallel, bool);
    SETGET_PROP(DumpImages, bool);
    SETGET_PROP(TestResolution, string);
    SETGET_PROP(GsyncDisplayId, UINT32);
    SETGET_PROP(EnableDMA, bool);
    SETGET_PROP(UseMemoryFillFns, bool);

private:
    typedef pair<UINT32,UINT32> Resolution;

    RC SetupSurfacePatterns();
    RC SetupSurfaceFill();
    RC GenerateSurfaces(Display::Mode surfaceRes, bool isR3Device);
    RC CheckCallwlatedCrcs(UINT32 gsyncIdx, UINT32 patternIdx);
    RC LwDisplaySetModeGsync
    (
        UINT32 displayMask,
        UINT32 dispWidth,
        UINT32 dispHeight,
        UINT32 dispRefRate
    );
    RC LwDisplaySetImageGsync(UINT32 displayMask, Surface2D* pSurf);
    RC CleanupDisplaysAttachedToGsyncDevices();

    RC UpdateTestResolution(const UINT32 displayMask);

    GpuTestConfiguration *  m_pTestConfig;  //!< Pointer to test configuration
    vector<GpuGoldenSurfaces> m_pGGSurfs;     //!< Pointer to Gpu golden surfaces
    Goldelwalues *          m_pGolden;      //!< Pointer to golden values
    UINT32                  m_OrigDisplays; //!< Originally connected display
    Display *               m_pDisplay;     //!< Pointer to real display
    bool                    m_CrcMiscompare; //!< True on a crc miscompare
    UINT32                  m_LoopIdx;      //!< For inter-function comm
    vector<UINT32>          m_DisplaySors;  //!< For inter-function comm
    map<UINT32, UINT32>     m_RefreshRate;  //!< Map of display Mask, Refresh Rate
    map<UINT32, Resolution> m_WdAndHt; //!< Test resolution width and height

    vector<GsyncDevice>     m_GsyncDevices;

    //! map from displayMask[UINT32] to associated window's settings
    map <UINT32, LwDispWindowSettings> m_UsedWindowMap;
    //! Structure containing surfaces
    struct SurfaceData
    {
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId; //!< Surface id of surface containing 
                                                        //!< the pattern
        UINT32    calcCrcs[4];
    };

    using SurfaceResMap = map<Resolution, SurfaceData>;

    //! Structure containing the patterns and the surfaces they use
    struct SurfacePattern
    {
        bool           random;
        vector<UINT32> pattern; //!< Pattern description
        SurfaceResMap  surfaceData; //!< Surface information for each resolution used
    };
    vector<SurfacePattern> m_SurfacePatterns;   //!< Test patterns
    std::unique_ptr<SurfaceUtils::SurfaceFill> m_pSurfaceFill; // Handler for filling surfaces with
                                                               // patterns
    RC GetSurfacePatternsFromJsArray
    (
        const JsArray& jsaTestPatterns,
        vector<SurfacePattern>* pOutSurfPatterns
    );

    // Variables set from JS
    UINT32 m_NumFrameCrcs;  //!< Number of frames to capture CRCs
    bool   m_CalcCrc;       //!< Callwlate expected CRCs instead of using goldens
    bool   m_IncludeRandom; //!< Include random pattern
    bool   m_EnableXbarCombinations; //!< On GM20x and above iterate over all SORs
    bool   m_RunParallel;   //!< Run on all available gsync devices in parallel
    bool   m_DumpImages; // If true dump surface contents after setting up.
    bool   m_EnableDMA; // Use DmaWrapper to fill window surfaces
    bool   m_UseMemoryFillFns; // Use the memoryFill fns from memory api directly
    string m_TestResolution;
    static constexpr UINT32 RESOLUTION_DEFAULT_DEPTH = 32;
    DisplayID m_GsyncDisplayId = Display::NULL_DISPLAY_ID; //!< DisplayMask of the selected Gsync  
                                                           //!< device to run tests exclusively on
                                                           //!< that display 
    RC DumpSurfaceInPng
    (
        UINT32 frame,
        UINT32 loop,
        const string &suffix,
        Surface2D *pSurf
    ) const;
    static const UINT32 s_DefaultPatterns[][4];

    RC AssignSorToGsyncDisplay
    (
        const UINT32 gsyncDevIndex,
        const OrIndex sorIdxBegin,
        const OrIndex sorIdxEnd,
        UINT32* pAssignedSORMask,
        OrIndex* pAssignedSorIdx
    );
    RC AssignSorToAllGsyncDisplays
    (
        const UINT32 gsyncIdxBegin,
        const UINT32 gsyncIdxEnd,
        const OrIndex sorIdxBegin,
        const OrIndex sorIdxEnd,
        const bool testXbarCombinations,
        DisplayIDs *pOutTestedDispIds
    );
    RC SetModeOnGsyncDisplays
    (
        const UINT32 gsyncIdxBegin,
        const UINT32 gsyncIdxEnd
    );
    RC DisplayTestImagesOnGsyncDisplays
    (
        const UINT32 patternIdx,
        const UINT32 gsyncIdxBegin,
        const UINT32 gsyncIdxEnd
    );
    RC GoldenRunForGsyncDevice
    (
        const UINT32 gsyncDevIdx,
        const UINT32 testedPatternIdx
    );
};

const UINT32 GsyncLink::s_DefaultPatterns[][4] =
{
    { 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF },
    { 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFAAAAAA, 0xFF555555, 0xFFAAAAAA, 0xFF555555 },
    { 0xFFAAAAAA, 0xFFAAAAAA, 0xFF555555, 0xFF555555 },
};

//
//------------------------------------------------------------------------------
// Method implementation
//------------------------------------------------------------------------------

//----------------------------------------------------------------------------
//! \brief Constructor
//!
GsyncLink::GsyncLink()
: m_OrigDisplays(0),
    m_pDisplay(nullptr),
    m_CrcMiscompare(false),
    m_LoopIdx(0),
    m_pSurfaceFill(nullptr),
    m_NumFrameCrcs(10),
    m_CalcCrc(true),
    m_IncludeRandom(true),
    m_EnableXbarCombinations(true),
    m_RunParallel(true),
    m_DumpImages(false),
    m_EnableDMA(true),
    m_UseMemoryFillFns(true),
    m_TestResolution("native")
{
    SetName("GsyncLink");
    m_pTestConfig = GetTestConfiguration();
    m_pGolden     = GetGoldelwalues();
}

bool GsyncLink::IsSupported()
{
    return true;
}

RC GsyncLink::UpdateTestResolution(const UINT32 displayMask)
{
    RC rc;
    vector <string> resStr;
    string usage("-testarg 186 TestResolution \"<Wd>:<Ht>:<RefRate>\" or \"native\"");
    UINT32 wd = 0;
    UINT32 ht = 0;
    UINT32 rr = 0;
    if (0 == m_TestResolution.compare("native"))
    {
        CHECK_RC(m_pDisplay->GetNativeResolution(displayMask,
                    &wd,
                    &ht,
                    &rr));
    }
    else
    {
        CHECK_RC(Utility::Tokenizer(m_TestResolution, ":", &resStr));
        if (resStr.size() != 3)
        {
            Printf(Tee::PriError, "Invalid argument. Usage %s\n", usage.c_str());
            return RC::BAD_PARAMETER;
        }
        wd = static_cast<UINT32> (Utility::Strtoull(resStr[0].c_str(), nullptr, 10));
        ht = static_cast<UINT32> (Utility::Strtoull(resStr[1].c_str(), nullptr, 10));
        rr = static_cast<UINT32> (Utility::Strtoull(resStr[2].c_str(), nullptr, 10));
        if (wd == 0 || ht == 0 || rr == 0)
        {
            Printf(Tee::PriError, "Invalid resolution. Please use non zero values\n");
            return RC::BAD_PARAMETER;
        }
    }
    m_WdAndHt[displayMask] = make_pair(wd, ht);
    m_RefreshRate[displayMask] = rr;
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Setup the GsyncLink
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC GsyncLink::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());
    m_pDisplay = GetDisplay();
    m_OrigDisplays = m_pDisplay->Selected();

    m_GsyncDevices.clear();
    CHECK_RC(GsyncDevice::FindGsyncDevices(m_pDisplay,
                                           GetBoundGpuSubdevice(),
                                           m_GsyncDisplayId,
                                           &m_GsyncDevices));

    if (m_GsyncDevices.size() == 0)
    {
        Printf(Tee::PriError,
               "No Gsync capable display found on any connected display!\n");
        return RC::EXT_DEV_NOT_FOUND;
    }

    // Test captures CRCs for same number of frames across all Gsync devices. Hence
    // need to check if requested number of frames can be captured on all detected
    // Gsync devices
    UINT32 maxAllowedCrcs = ~0;
    for (const auto &gsyncDevice : m_GsyncDevices)
    {
        if (gsyncDevice.GetMaxCrcFrames(GsyncDevice::CRC_PRE_AND_POST_MEM)
                < maxAllowedCrcs)
        {
            maxAllowedCrcs =
                gsyncDevice.GetMaxCrcFrames(GsyncDevice::CRC_PRE_AND_POST_MEM);
        }
    }
    if ((m_NumFrameCrcs == 0) || (m_NumFrameCrcs > maxAllowedCrcs))
    {
        UINT32 defaultNumFrameCrcs = (maxAllowedCrcs > 10) ? 10: maxAllowedCrcs;
        Printf(Tee::PriError,
              "%s : NumFrameCrcs (%u) out of range (1,%u), defaulting to %u\n",
              GetName().c_str(), m_NumFrameCrcs,
              maxAllowedCrcs, defaultNumFrameCrcs);
        m_NumFrameCrcs = defaultNumFrameCrcs;
    }

    UINT32 conlwrrentDevices = (m_RunParallel) ?
        static_cast<UINT32>(m_GsyncDevices.size()) : 1;

    m_DisplaySors.resize(conlwrrentDevices);

    if (!m_CalcCrc)
    {
        m_pGGSurfs.resize(m_GsyncDevices.size(), GpuGoldenSurfaces(GetBoundGpuDevice()));
    }
 
    CHECK_RC(SetupSurfaceFill());

    for (size_t gsyncIdx = 0; gsyncIdx < m_GsyncDevices.size(); gsyncIdx++)
    {
        const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
        CHECK_RC(UpdateTestResolution(displayMask));

        m_GsyncDevices[gsyncIdx].SetNumCrcFrames(m_NumFrameCrcs);

        const Display::Mode resolutionMode(m_WdAndHt[displayMask].first,
                                           m_WdAndHt[displayMask].second,
                                           RESOLUTION_DEFAULT_DEPTH,
                                           m_RefreshRate[displayMask]);

        if (m_SurfacePatterns[0].surfaceData.find(m_WdAndHt[displayMask])
                == m_SurfacePatterns[0].surfaceData.end())
        {
            CHECK_RC(GenerateSurfaces(resolutionMode,
                        m_GsyncDevices[gsyncIdx].IsDeviceR3Capable()));
        }

        if (!m_CalcCrc)
        {
            Resolution res = make_pair(resolutionMode.width, resolutionMode.height);
            m_pGGSurfs[gsyncIdx].AttachSurface(m_pSurfaceFill->GetSurface(m_SurfacePatterns[0].
                                               surfaceData[res].surfaceId),
                                               "C",
                                               displayMask,
                                               &m_GsyncDevices[gsyncIdx]);
        }
    }

    return rc;
}

void Calc24bitGsyncSingleCrc(const bool*lfsr_q, UINT32 value, bool *lfsr_c)
{
    bool data_in[24];
    for (UINT32 bitIdx = 0; bitIdx < 24; bitIdx++)
    {
        data_in[bitIdx] = (value>>bitIdx) & 1;
    }

    // The below "hw" equasions are from Tom Verbeure <tverbeure@lwpu.com>

    // I callwlate 4 CRC32s (the Ethernet one) for each
    //  vertical column modulo 4.
    // That is: CRC0 as pixel 0,4,8,... Etc. CRC1 has 1,5,9,...

    // "The polynomial is the following one:
    // crc[31:0]=1+x^1+x^2+x^4+x^5+x^7+x^8+x^10+x^11+x^12+x^16+x^22+x^23+x^26+x^32;
    // The LFSR is initialized with a value of 1 at vsync.
    // The 24-bit input is arranged as ( blue[7:0] << 16 | green[7:0] << 8 | red[7:0] )

    // To check that your algorithm is correct, if you feed the CRC engine
    // with the following sequence:
    //  0x000000
    //  0x000004
    //  0x000008
    //  0x00000c
    //  0x000010

    // You will get the following outputs:
    //  0x01000000
    //  0x12dcda5b
    //  0x41d5acc5
    //  0x077a14fb
    //  0xb6012d6d

    // Parallelized with a 24-bit wide input, this results in this:

    lfsr_c[0] = lfsr_q[8] ^ lfsr_q[14] ^ lfsr_q[17] ^ lfsr_q[18] ^ lfsr_q[20] ^ lfsr_q[24] ^ data_in[0] ^ data_in[6] ^ data_in[9] ^ data_in[10] ^ data_in[12] ^ data_in[16];
    lfsr_c[1] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[17] ^ lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[21] ^ lfsr_q[24] ^ lfsr_q[25] ^ data_in[0] ^ data_in[1] ^ data_in[6] ^ data_in[7] ^ data_in[9] ^ data_in[11] ^ data_in[12] ^ data_in[13] ^ data_in[16] ^ data_in[17];
    lfsr_c[2] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[26] ^ data_in[0] ^ data_in[1] ^ data_in[2] ^ data_in[6] ^ data_in[7] ^ data_in[8] ^ data_in[9] ^ data_in[13] ^ data_in[14] ^ data_in[16] ^ data_in[17] ^ data_in[18];
    lfsr_c[3] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[17] ^ lfsr_q[18] ^ lfsr_q[22] ^ lfsr_q[23] ^ lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[27] ^ data_in[1] ^ data_in[2] ^ data_in[3] ^ data_in[7] ^ data_in[8] ^ data_in[9] ^ data_in[10] ^ data_in[14] ^ data_in[15] ^ data_in[17] ^ data_in[18] ^ data_in[19];
    lfsr_c[4] = lfsr_q[8] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[14] ^ lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[23] ^ lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[28] ^ data_in[0] ^ data_in[2] ^ data_in[3] ^ data_in[4] ^ data_in[6] ^ data_in[8] ^ data_in[11] ^ data_in[12] ^ data_in[15] ^ data_in[18] ^ data_in[19] ^ data_in[20];
    lfsr_c[5] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[18] ^ lfsr_q[21] ^ lfsr_q[27] ^ lfsr_q[28] ^ lfsr_q[29] ^ data_in[0] ^ data_in[1] ^ data_in[3] ^ data_in[4] ^ data_in[5] ^ data_in[6] ^ data_in[7] ^ data_in[10] ^ data_in[13] ^ data_in[19] ^ data_in[20] ^ data_in[21];
    lfsr_c[6] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[22] ^ lfsr_q[28] ^ lfsr_q[29] ^ lfsr_q[30] ^ data_in[1] ^ data_in[2] ^ data_in[4] ^ data_in[5] ^ data_in[6] ^ data_in[7] ^ data_in[8] ^ data_in[11] ^ data_in[14] ^ data_in[20] ^ data_in[21] ^ data_in[22];
    lfsr_c[7] = lfsr_q[8] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[13] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[18] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[29] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[0] ^ data_in[2] ^ data_in[3] ^ data_in[5] ^ data_in[7] ^ data_in[8] ^ data_in[10] ^ data_in[15] ^ data_in[16] ^ data_in[21] ^ data_in[22] ^ data_in[23];
    lfsr_c[8] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[16] ^ lfsr_q[18] ^ lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[25] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[0] ^ data_in[1] ^ data_in[3] ^ data_in[4] ^ data_in[8] ^ data_in[10] ^ data_in[11] ^ data_in[12] ^ data_in[17] ^ data_in[22] ^ data_in[23];
    lfsr_c[9] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[17] ^ lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[21] ^ lfsr_q[26] ^ lfsr_q[31] ^ data_in[1] ^ data_in[2] ^ data_in[4] ^ data_in[5] ^ data_in[9] ^ data_in[11] ^ data_in[12] ^ data_in[13] ^ data_in[18] ^ data_in[23];
    lfsr_c[10] = lfsr_q[8] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[13] ^ lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[27] ^ data_in[0] ^ data_in[2] ^ data_in[3] ^ data_in[5] ^ data_in[9] ^ data_in[13] ^ data_in[14] ^ data_in[16] ^ data_in[19];
    lfsr_c[11] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[17] ^ lfsr_q[20] ^ lfsr_q[22] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[28] ^ data_in[0] ^ data_in[1] ^ data_in[3] ^ data_in[4] ^ data_in[9] ^ data_in[12] ^ data_in[14] ^ data_in[15] ^ data_in[16] ^ data_in[17] ^ data_in[20];
    lfsr_c[12] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[17] ^ lfsr_q[20] ^ lfsr_q[21] ^ lfsr_q[23] ^ lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[29] ^ data_in[0] ^ data_in[1] ^ data_in[2] ^ data_in[4] ^ data_in[5] ^ data_in[6] ^ data_in[9] ^ data_in[12] ^ data_in[13] ^ data_in[15] ^ data_in[17] ^ data_in[18] ^ data_in[21];
    lfsr_c[13] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[18] ^ lfsr_q[21] ^ lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[30] ^ data_in[1] ^ data_in[2] ^ data_in[3] ^ data_in[5] ^ data_in[6] ^ data_in[7] ^ data_in[10] ^ data_in[13] ^ data_in[14] ^ data_in[16] ^ data_in[18] ^ data_in[19] ^ data_in[22];
    lfsr_c[14] = lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[22] ^ lfsr_q[23] ^ lfsr_q[25] ^ lfsr_q[27] ^ lfsr_q[28] ^ lfsr_q[31] ^ data_in[2] ^ data_in[3] ^ data_in[4] ^ data_in[6] ^ data_in[7] ^ data_in[8] ^ data_in[11] ^ data_in[14] ^ data_in[15] ^ data_in[17] ^ data_in[19] ^ data_in[20] ^ data_in[23];
    lfsr_c[15] = lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[17] ^ lfsr_q[20] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[26] ^ lfsr_q[28] ^ lfsr_q[29] ^ data_in[3] ^ data_in[4] ^ data_in[5] ^ data_in[7] ^ data_in[8] ^ data_in[9] ^ data_in[12] ^ data_in[15] ^ data_in[16] ^ data_in[18] ^ data_in[20] ^ data_in[21];
    lfsr_c[16] = lfsr_q[8] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[16] ^ lfsr_q[20] ^ lfsr_q[21] ^ lfsr_q[25] ^ lfsr_q[27] ^ lfsr_q[29] ^ lfsr_q[30] ^ data_in[0] ^ data_in[4] ^ data_in[5] ^ data_in[8] ^ data_in[12] ^ data_in[13] ^ data_in[17] ^ data_in[19] ^ data_in[21] ^ data_in[22];
    lfsr_c[17] = lfsr_q[9] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[22] ^ lfsr_q[26] ^ lfsr_q[28] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[1] ^ data_in[5] ^ data_in[6] ^ data_in[9] ^ data_in[13] ^ data_in[14] ^ data_in[18] ^ data_in[20] ^ data_in[22] ^ data_in[23];
    lfsr_c[18] = lfsr_q[10] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[18] ^ lfsr_q[22] ^ lfsr_q[23] ^ lfsr_q[27] ^ lfsr_q[29] ^ lfsr_q[31] ^ data_in[2] ^ data_in[6] ^ data_in[7] ^ data_in[10] ^ data_in[14] ^ data_in[15] ^ data_in[19] ^ data_in[21] ^ data_in[23];
    lfsr_c[19] = lfsr_q[11] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[28] ^ lfsr_q[30] ^ data_in[3] ^ data_in[7] ^ data_in[8] ^ data_in[11] ^ data_in[15] ^ data_in[16] ^ data_in[20] ^ data_in[22];
    lfsr_c[20] = lfsr_q[12] ^ lfsr_q[16] ^ lfsr_q[17] ^ lfsr_q[20] ^ lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[29] ^ lfsr_q[31] ^ data_in[4] ^ data_in[8] ^ data_in[9] ^ data_in[12] ^ data_in[16] ^ data_in[17] ^ data_in[21] ^ data_in[23];
    lfsr_c[21] = lfsr_q[13] ^ lfsr_q[17] ^ lfsr_q[18] ^ lfsr_q[21] ^ lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[30] ^ data_in[5] ^ data_in[9] ^ data_in[10] ^ data_in[13] ^ data_in[17] ^ data_in[18] ^ data_in[22];
    lfsr_c[22] = lfsr_q[8] ^ lfsr_q[17] ^ lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[31] ^ data_in[0] ^ data_in[9] ^ data_in[11] ^ data_in[12] ^ data_in[14] ^ data_in[16] ^ data_in[18] ^ data_in[19] ^ data_in[23];
    lfsr_c[23] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[14] ^ lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[27] ^ lfsr_q[28] ^ data_in[0] ^ data_in[1] ^ data_in[6] ^ data_in[9] ^ data_in[13] ^ data_in[15] ^ data_in[16] ^ data_in[17] ^ data_in[19] ^ data_in[20];
    lfsr_c[24] = lfsr_q[0] ^ lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[15] ^ lfsr_q[18] ^ lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[28] ^ lfsr_q[29] ^ data_in[1] ^ data_in[2] ^ data_in[7] ^ data_in[10] ^ data_in[14] ^ data_in[16] ^ data_in[17] ^ data_in[18] ^ data_in[20] ^ data_in[21];
    lfsr_c[25] = lfsr_q[1] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[23] ^ lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[29] ^ lfsr_q[30] ^ data_in[2] ^ data_in[3] ^ data_in[8] ^ data_in[11] ^ data_in[15] ^ data_in[17] ^ data_in[18] ^ data_in[19] ^ data_in[21] ^ data_in[22];
    lfsr_c[26] = lfsr_q[2] ^ lfsr_q[8] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[14] ^ lfsr_q[18] ^ lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[28] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[0] ^ data_in[3] ^ data_in[4] ^ data_in[6] ^ data_in[10] ^ data_in[18] ^ data_in[19] ^ data_in[20] ^ data_in[22] ^ data_in[23];
    lfsr_c[27] = lfsr_q[3] ^ lfsr_q[9] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[15] ^ lfsr_q[19] ^ lfsr_q[27] ^ lfsr_q[28] ^ lfsr_q[29] ^ lfsr_q[31] ^ data_in[1] ^ data_in[4] ^ data_in[5] ^ data_in[7] ^ data_in[11] ^ data_in[19] ^ data_in[20] ^ data_in[21] ^ data_in[23];
    lfsr_c[28] = lfsr_q[4] ^ lfsr_q[10] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[16] ^ lfsr_q[20] ^ lfsr_q[28] ^ lfsr_q[29] ^ lfsr_q[30] ^ data_in[2] ^ data_in[5] ^ data_in[6] ^ data_in[8] ^ data_in[12] ^ data_in[20] ^ data_in[21] ^ data_in[22];
    lfsr_c[29] = lfsr_q[5] ^ lfsr_q[11] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[29] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[3] ^ data_in[6] ^ data_in[7] ^ data_in[9] ^ data_in[13] ^ data_in[21] ^ data_in[22] ^ data_in[23];
    lfsr_c[30] = lfsr_q[6] ^ lfsr_q[12] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[18] ^ lfsr_q[22] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[4] ^ data_in[7] ^ data_in[8] ^ data_in[10] ^ data_in[14] ^ data_in[22] ^ data_in[23];
    lfsr_c[31] = lfsr_q[7] ^ lfsr_q[13] ^ lfsr_q[16] ^ lfsr_q[17] ^ lfsr_q[19] ^ lfsr_q[23] ^ lfsr_q[31] ^ data_in[5] ^ data_in[8] ^ data_in[9] ^ data_in[11] ^ data_in[15] ^ data_in[23];

}

static string CreateSorString(const vector<UINT32>& displaySors)
{
    string ret = "";
    for (UINT32 i = 0; i < displaySors.size(); i++)
        ret += Utility::StrPrintf("%s%d%s",
                                  (i != 0)?" ":"",
                                  displaySors[i],
                                  (i != displaySors.size()-1)?",":"");
    return ret;
}

RC GsyncLink::SetupSurfaceFill()
{
    RC rc;
    MASSERT(m_pTestConfig && m_pDisplay);
    if (m_EnableDMA)
    {
        // UseMemoryFillFns = true by default
        m_pSurfaceFill = make_unique<SurfaceUtils::SurfaceFillDMA>(m_UseMemoryFillFns);
        Printf(GetVerbosePrintPri(), "Gsynclink test will use DMA for displaying the image\n");
    }
    else
    {
        // UseMemoryFillFns = true by default
        m_pSurfaceFill =  make_unique<SurfaceUtils::SurfaceFillFB>(m_UseMemoryFillFns);
        Printf(GetVerbosePrintPri(), "Gsynclink test will not use DMA for displaying the"
                " image\n");
    }
    CHECK_RC(m_pSurfaceFill->Setup(this, m_pTestConfig));
    return rc;
}

RC GsyncLink::GenerateSurfaces(Display::Mode surfaceRes, bool isR3Device)
{
    RC rc; 
    // This static variable keeps surface ids unique accross multiple calls to this function
    // with multiple resolutions
    static UINT32 surfIndex = 0;
    // This format makes UINT32 pixel values to be in the same color order
    // as used during CRC callwlations:
    ColorUtils::Format format = ColorUtils::A8B8G8R8;
    // TODO: Supporting pitch by default now, later BlockLinear can be added too
    m_pSurfaceFill->SetLayout(Surface2D::Pitch);
    Resolution res = make_pair(surfaceRes.width, surfaceRes.height);
    for (size_t patIdx = 0; patIdx < m_SurfacePatterns.size(); patIdx++)
    {
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId =
            static_cast<SurfaceUtils::SurfaceFill::SurfaceId> (++surfIndex);
        m_SurfacePatterns[patIdx].surfaceData[res].surfaceId = surfaceId;

        CHECK_RC(m_pSurfaceFill->PrepareSurface(surfaceId, surfaceRes, format));
        if (m_SurfacePatterns[patIdx].random)
        {
            CHECK_RC(m_pSurfaceFill->FillPatternRect(surfaceId,
                        "random",
                        nullptr,
                        nullptr));
        }
        else
        {
            // Fill surface with user defined pattern data
            CHECK_RC(m_pSurfaceFill->FillPattern(surfaceId,
                        m_SurfacePatterns[patIdx].pattern));
        }

        if (m_CalcCrc)
        {
            GsyncCRC(isR3Device).GenCrc(m_pSurfaceFill->GetSurface(surfaceId),
                    m_SurfacePatterns[patIdx].surfaceData[res].calcCrcs);
        }
        CHECK_RC(m_pSurfaceFill->CopySurface(surfaceId));
    }

    return rc;
}

namespace
{
void PrintVector
(
    const vector<UINT32>& inputList,
    const char* pListName = nullptr,
    Tee::Priority pri = Tee::PriNormal,
    UINT32 indent = 0
)
{
    if (pListName)
    {
        Printf(pri, "%*s%s:\n", indent, " ", pListName);
    }
    Printf(pri, "%*s(", indent, " ");
    for (const int val : inputList)
    {
        Printf(pri, "%u, ", val);
    }
    Printf(pri, ")\n");
}
}

RC GsyncLink::CleanupDisplaysAttachedToGsyncDevices()
{
    return GsyncDisplayHelper::DetachAllDisplays(m_GsyncDevices);
}

RC GsyncLink::AssignSorToGsyncDisplay
(
    const UINT32 gsyncDevIndex,
    const OrIndex sorIdxBegin,
    const OrIndex sorIdxEnd,
    UINT32* pAssignedSORMask,
    OrIndex* pAssignedSorIdx
)
{
    RC rc;
    UINT32 indent = 0;
    Printf(GetVerbosePrintPri(),
            "%*s%s: gsyncDev[%u], sorIdxBegin [%u], sorIdxEnd [%u]\n",
            indent, " ", __FUNCTION__, gsyncDevIndex,
            sorIdxBegin, sorIdxEnd);

    UINT32 assignedSORMask = *pAssignedSORMask;
    const GsyncDevice& gsyncDev = m_GsyncDevices[gsyncDevIndex];
    for (OrIndex sorIdx = sorIdxBegin; sorIdx < sorIdxEnd; sorIdx++)
    {
        if ((1 << sorIdx) & assignedSORMask)
        {
            continue;
        }
        UINT32 indent = 4;
        Printf(GetVerbosePrintPri(), "%*sWorking on SorBitMask 0x%x\n",
                indent, " ", 1 << sorIdx);
        vector<UINT32> sorList;
        rc = GsyncDisplayHelper::AssignSor(gsyncDev,
                    sorIdx,
                    Display::AssignSorSetting::ONE_HEAD_MODE,
                    &sorList,
                    pAssignedSorIdx);
            
        if (rc ==  RC::UNSUPPORTED_HARDWARE_FEATURE)
        {
            rc.Clear();
            continue;
        }
        CHECK_RC(rc);
        PrintVector(sorList, "AssignSorList"
                , GetVerbosePrintPri(), indent);
        assignedSORMask |= 1 << sorIdx;
        break;
    }
    *pAssignedSORMask = assignedSORMask;
    return rc;
}

RC GsyncLink::AssignSorToAllGsyncDisplays
(
    const UINT32 gsyncIdxBegin,
    const UINT32 gsyncIdxEnd,
    const OrIndex sorIdxBegin,
    const OrIndex sorIdxEnd,
    const bool testXbarCombinations,
    DisplayIDs *pOutTestedDispIds
)
{
    RC rc;
    UINT32 assignedSORMask = 0;
    for (UINT32 gsyncIdx = gsyncIdxBegin; gsyncIdx < gsyncIdxEnd; gsyncIdx++)
    {
        const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
        const UINT32 surfaceIdx = m_RunParallel ? gsyncIdx : 0x0;
        Printf(GetVerbosePrintPri(), "%s: Display[0x%x], SurfaceIdx[%u]\n",
                __FUNCTION__, displayMask, surfaceIdx);
        if (testXbarCombinations)
        {
            CHECK_RC(AssignSorToGsyncDisplay(gsyncIdx,
                        sorIdxBegin,
                        sorIdxEnd,
                        &assignedSORMask,
                        &m_DisplaySors[surfaceIdx]));
            if (pOutTestedDispIds)
            {
                pOutTestedDispIds->emplace_back(displayMask);
            }
        }
        else
        {
            CHECK_RC(m_pDisplay->GetORProtocol(displayMask,
                        &m_DisplaySors[surfaceIdx],
                        nullptr,
                        nullptr));
        }
    }
    return rc;
}

RC GsyncLink::SetModeOnGsyncDisplays
(
    const UINT32 gsyncIdxBegin,
    const UINT32 gsyncIdxEnd
)
{
    RC rc;
    for (UINT32 gsyncDevIdx = gsyncIdxBegin; gsyncDevIdx < gsyncIdxEnd;
            gsyncDevIdx++)
    {
        const UINT32 displayMask = m_GsyncDevices[gsyncDevIdx].GetDisplayMask();
        const UINT32 width  = m_WdAndHt[displayMask].first;
        const UINT32 height = m_WdAndHt[displayMask].second;
        const UINT32 dispRefRate = m_RefreshRate[displayMask];

        CHECK_RC(GsyncDisplayHelper::SetMode(m_GsyncDevices[gsyncDevIdx],
                    width,
                    height,
                    dispRefRate,
                    m_pTestConfig->DisplayDepth(),
                    Display::ORColorSpace::DEFAULT_CS
                    ));
        CHECK_RC(m_GsyncDevices[gsyncDevIdx].FactoryReset());
        CHECK_RC(m_GsyncDevices[gsyncDevIdx].NotifyModeset());

        UINT32 gsyncRefRate = 0;
        // Value of 1000 for retries was reached experimentally and is a WAR!
        UINT32 numRetriesLeft = 1000;
        while (gsyncRefRate == 0 && numRetriesLeft != 0)
        {
            CHECK_RC(m_GsyncDevices[gsyncDevIdx].GetRefreshRate(&gsyncRefRate));
            --numRetriesLeft;
        }
        if (gsyncRefRate == 0)
        {
            Printf(Tee::PriError,
                    "Gsync display 0x%08x disconnected!\n", displayMask);
            return RC::HW_STATUS_ERROR;
        }
        if (gsyncRefRate > (dispRefRate+1) || gsyncRefRate < (dispRefRate-1))
        {
            Printf(Tee::PriError, "Gsync display 0x%08x Refresh Rate is %dHz. "
                    "Could not be set to %dHz\n",
                    displayMask, gsyncRefRate, dispRefRate);
            return RC::HW_STATUS_ERROR;
        }
    }
    return rc;
}

RC GsyncLink::DisplayTestImagesOnGsyncDisplays
(
    const UINT32 patternIdx,
    const UINT32 gsyncIdxBegin,
    const UINT32 gsyncIdxEnd
)
{
    RC rc;
    for (UINT32 gsyncDevIdx = gsyncIdxBegin; gsyncDevIdx < gsyncIdxEnd;
            gsyncDevIdx++)
    {
        const UINT32 displayMask = m_GsyncDevices[gsyncDevIdx].GetDisplayMask();
        const Resolution surfaceRes = m_WdAndHt[displayMask];
        Surface2D *pImgSurf = m_pSurfaceFill->GetImage(m_SurfacePatterns[patternIdx].
            surfaceData[surfaceRes].surfaceId);

        if (m_DumpImages)
        {
            string rStr = Utility::StrPrintf("%u*%u@%u",
                    surfaceRes.first,
                    surfaceRes.second,
                    m_RefreshRate[displayMask]);
            CHECK_RC(DumpSurfaceInPng(patternIdx, m_LoopIdx, rStr, pImgSurf));
        }
        CHECK_RC(GsyncDisplayHelper::DisplayImage(m_GsyncDevices[gsyncDevIdx]
                    , pImgSurf));
        CHECK_RC(m_GsyncDevices[gsyncDevIdx].NotifyImageChange());
    }
    return rc;
}

RC GsyncLink::GoldenRunForGsyncDevice
(
    const UINT32 gsyncDevIdx,
    const UINT32 testedPatternIdx
)
{
    RC rc;
    if (m_CalcCrc)
    {
        CHECK_RC(CheckCallwlatedCrcs(gsyncDevIdx, testedPatternIdx));
    }
    else
    {
        const UINT32 displayMask = m_GsyncDevices[gsyncDevIdx].GetDisplayMask();
        const Resolution surfaceRes = m_WdAndHt[displayMask];

        m_pGolden->ClearSurfaces();
        m_pGGSurfs[gsyncDevIdx].ReplaceSurface(0,
                m_pSurfaceFill->GetSurface(m_SurfacePatterns[testedPatternIdx].
                surfaceData[surfaceRes].surfaceId));
        CHECK_RC(m_pGolden->SetSurfaces(&m_pGGSurfs[gsyncDevIdx]));
        m_pGolden->SetLoop(testedPatternIdx);
        if (OK != (rc = m_pGolden->Run()))
        {
            Printf(Tee::PriError,
                    "Golden error on Loop %u "
                    "(pattern %u, display 0x%08x, SORs: %s)\n",
                    m_LoopIdx, displayMask, testedPatternIdx,
                    CreateSorString(m_DisplaySors).c_str());
            if (m_pGolden->GetStopOnError())
                return rc;
            else
            {
                rc.Clear();
                m_CrcMiscompare = true;
            }
        }
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the GsyncLink
//!
//! \return OK if running was successful, not OK otherwise
//!
RC GsyncLink::Run()
{
    RC rc;

    bool testXbarCombinations = m_EnableXbarCombinations &&
        GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR);
    OrIndex numSorConfigurations = testXbarCombinations
        ? LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS : 1;

    const UINT32 conlwrrentDevices = (m_RunParallel) ?
        static_cast<UINT32>(m_GsyncDevices.size()) : 1;

    if (conlwrrentDevices > numSorConfigurations)
    {
        Printf(Tee::PriError,
                "%s error: not enough SORs (%d) for number of conlwrrent "
                "gsync devices (%d)\n",
               GetName().c_str(), numSorConfigurations, conlwrrentDevices);
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    // Set to false here so multiple calls to Run are possible
    m_CrcMiscompare = false;

    // Will do one loop if m_RunParallel, otherwise will loop over each gsync device
    for (UINT32 gsyncIdxOffset = 0;
            gsyncIdxOffset < m_GsyncDevices.size();
            gsyncIdxOffset += conlwrrentDevices)
    {
        // When generating goldens it is only necessary to generate goldens once per
        // pattern per gsync device group (rather than using loop number to store CRCs in the database,
        // pattern number is used instead)
        set<UINT32> patternsToCrc;
        if (!m_CalcCrc && m_pGolden->GetAction() == Goldelwalues::Store)
        {
            for (UINT32 ii = 0; ii < m_SurfacePatterns.size(); ii++)
            {
                patternsToCrc.insert(static_cast<UINT32>(ii));
            }
        }

        Printf(GetVerbosePrintPri(), "Run: GsyncIdxOffset = %u\n"
                , static_cast<UINT32>(gsyncIdxOffset));

        // And make sure all of the SORs get tested at least once
        for (UINT32 sorConfOffset = 0;
                sorConfOffset + conlwrrentDevices <= numSorConfigurations;
                sorConfOffset +=
                (sorConfOffset + conlwrrentDevices <= numSorConfigurations)
                ? conlwrrentDevices : 1)
        {
            DEFER
            {
                rc = CleanupDisplaysAttachedToGsyncDevices();
            };

            //Assign SOR on tested displayIds (displayIds are 1:1 mapped to GsyncDevices)
            CHECK_RC(AssignSorToAllGsyncDisplays(gsyncIdxOffset,
                    gsyncIdxOffset + conlwrrentDevices,
                    sorConfOffset,
                    numSorConfigurations,
                    testXbarCombinations,
                    nullptr));
            Printf(GetVerbosePrintPri(), "Testing SORs: %s\n",
                    CreateSorString(m_DisplaySors).c_str());

            CHECK_RC(SetModeOnGsyncDisplays(gsyncIdxOffset,
                        gsyncIdxOffset + conlwrrentDevices));

            UINT32 StartLoop = m_pTestConfig->StartLoop();
            UINT32 EndLoop   = StartLoop + m_pTestConfig->Loops();
            for (m_LoopIdx = StartLoop; m_LoopIdx < EndLoop; m_LoopIdx++)
            {
                const UINT32 patternIdx = m_LoopIdx % m_SurfacePatterns.size();
                CHECK_RC(DisplayTestImagesOnGsyncDisplays(patternIdx,
                            gsyncIdxOffset,
                            gsyncIdxOffset + conlwrrentDevices));
                for (UINT32 gsyncIdx = gsyncIdxOffset;
                        gsyncIdx < (gsyncIdxOffset + conlwrrentDevices); gsyncIdx++)
                {
                    CHECK_RC(GoldenRunForGsyncDevice(gsyncIdx, patternIdx));
                }
                // When generating goldens, exit once all patterns have
                // been CRCd
                if (m_pGolden->GetAction() == Goldelwalues::Store)
                {
                    patternsToCrc.erase(patternIdx);
                    if (patternsToCrc.empty())
                        break;
                }
            }

            //Clear Images on tested GsyncDevices
            for (UINT32 gsyncIdx = gsyncIdxOffset;
                    gsyncIdx < (gsyncIdxOffset + conlwrrentDevices); gsyncIdx++)
            {
                CHECK_RC(GsyncDisplayHelper::DisplayImage(
                            m_GsyncDevices[gsyncIdx], nullptr));
                CHECK_RC(m_GsyncDevices[gsyncIdx].NotifyImageChange());
            }

            if (!m_CalcCrc
                    && m_pGolden->GetAction() == Goldelwalues::Store
                    && patternsToCrc.empty())
            {
                break;
            }
        }

        if (!m_CalcCrc)
        {
            // Golden errors that are deferred due to "-run_on_error" can only
            // be retrieved by running Golden::ErrorRateTest().  This must be
            // done before clearing surfaces
            CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));
        }
    }

    if (m_CrcMiscompare)
        return RC::GOLDEN_VALUE_MISCOMPARE;

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Cleanup the GsyncLink
//!
//! \return OK if running was successful, not OK otherwise
//!
RC GsyncLink::Cleanup()
{
    StickyRC rc;

    if (m_pDisplay != nullptr)
    {
        if (m_pDisplay->GetDisplayClassFamily() == Display::EVO)
        {
            rc = m_pDisplay->Select(m_OrigDisplays);
        }
        else if (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
        {
            CHECK_RC(m_pDisplay->GetLwDisplay()->ResetEntireState());
        }
        m_pDisplay = nullptr;
    }

    // Free golden buffer.
    m_pGolden->ClearSurfaces();
    for (UINT32 gsyncIdx = 0; gsyncIdx < m_pGGSurfs.size(); gsyncIdx++)
    {
        m_pGGSurfs[gsyncIdx].ClearSurfaces();
    }

    if (m_pSurfaceFill.get())
    {
        rc = m_pSurfaceFill->Cleanup();
    }

    m_SurfacePatterns.clear();

    m_DisplaySors.clear();

    m_GsyncDevices.clear();

    m_UsedWindowMap.clear();

    return GpuTest::Cleanup();
}

RC GsyncLink::GetSurfacePatternsFromJsArray
(
    const JsArray& jsaTestPatterns,
    vector<SurfacePattern>* pOutSurfPatterns
)
{
    RC rc;
    JavaScriptPtr pJs;
    MASSERT(pOutSurfPatterns);
    const UINT32 aRGBComponentAlphaMask = 0xFF << 24;
    SurfacePattern surfPattern = {};
    for (UINT32 pixelIdx = 0; pixelIdx < jsaTestPatterns.size(); ++pixelIdx)
    {
        UINT32 lwrPixel = 0;
        rc = pJs->FromJsval(jsaTestPatterns[pixelIdx], &lwrPixel);
        if (OK != rc)
        {
            Printf(Tee::PriError,
                    "GsyncLinkTest : Unable to get custom pattern pixel from"
                    " input Js Test argument\n");
            return rc;
        }
        surfPattern.pattern.push_back(lwrPixel | aRGBComponentAlphaMask);
    }
    pOutSurfPatterns->push_back(surfPattern);
    return rc;
};
//----------------------------------------------------------------------------
//! \brief Fill up surface patterns for test images
//!
RC GsyncLink::SetupSurfacePatterns()
{
    JavaScriptPtr pJs;
    JsArray jsaTestPatterns;
    RC rc;
    // Initialize TestPatterns by getting the actual propery from JS
    // ratherthan by exporting a C++ JS property.  This is due to the nature of
    // how JS properties are copied onto the test object from test arguments
    // properties / arrays are copied individually rather than an entire
    // array assignment at once (what the C++ interface expects)
    rc = pJs->GetProperty(GetJSObject(),
            "TestPatterns",
            &jsaTestPatterns);
    if (OK == rc)
    {
        CHECK_RC(GetSurfacePatternsFromJsArray(jsaTestPatterns,
                    &m_SurfacePatterns));
    }
    else if (RC::ILWALID_OBJECT_PROPERTY == rc)
    {
        rc.Clear();
        m_SurfacePatterns.clear();
        if (m_IncludeRandom)
        {
            SurfacePattern surfPattern;
            surfPattern.random = true;
            m_SurfacePatterns.push_back(surfPattern);
        }

        const UINT32 numDefaultPatterns
            = sizeof(s_DefaultPatterns) / sizeof(s_DefaultPatterns[0]);
        const UINT32 numPixelsInDefaultPattern
            = sizeof(s_DefaultPatterns[0]) / sizeof(UINT32);
        for (UINT32 lwrPtIdx = 0; lwrPtIdx < numDefaultPatterns; lwrPtIdx++)
        {
            SurfacePattern surfPattern;
            surfPattern.random = false;
            vector<UINT32> pattern(s_DefaultPatterns[lwrPtIdx],
                     s_DefaultPatterns[lwrPtIdx] + numPixelsInDefaultPattern);
            surfPattern.pattern = pattern;
            m_SurfacePatterns.push_back(surfPattern);
        }
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Initialize test properties from JS
//!
//! \return OK if successful, not OK otherwise
//!
RC GsyncLink::InitFromJs()
{
    RC rc;

    // Call base-class function (for TestConfiguration and Golden properties).
    CHECK_RC(GpuTest::InitFromJs());
    CHECK_RC(SetupSurfacePatterns());
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void GsyncLink::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tNumFrameCrcs:\t\t\t%u\n", m_NumFrameCrcs);
    Printf(pri, "\tCalcCrc:\t\t\t%s\n", m_CalcCrc ? "true" : "false");
    Printf(pri, "\tIncludeRandom:\t\t\t%s\n", m_IncludeRandom ? "true" : "false");
    Printf(pri, "\tTestResolution(wd:ht:rr):\t%s\n", m_TestResolution.c_str());
    Printf(pri, "\tEnableDMA:\t\t%s\n", m_EnableDMA ? "true" : "false");
    Printf(pri, "\tUseMemoryFillFns:\t\t%s\n", m_UseMemoryFillFns ? "true" : "false");
    Printf(pri, "\tTestPatterns:\t\t\t");
    for (UINT32 pat = 0; pat < m_SurfacePatterns.size(); pat++)
    {
        if (pat != 0)
        {
            Printf(pri, "\t\t\t\t\t");
        }
        Printf(pri, "[");
        if (m_SurfacePatterns[pat].random)
        {
            Printf(pri, "random");
        }
        else
        {
            for (UINT32 pix = 0; pix < m_SurfacePatterns[pat].pattern.size(); pix++)
            {
                // Patterns are specified with RGB values only so when printing them
                // mask out and do not print the alpha values
                Printf(pri, "0x%06x",
                       m_SurfacePatterns[pat].pattern[pix] & ~0xff000000);
                if (pix != (m_SurfacePatterns[pat].pattern.size() - 1))
                    Printf(pri, ",");
            }
        }
        Printf(pri, "]\n");
    }

    GpuTest::PrintJsProperties(pri);
}

//-----------------------------------------------------------------------------
//! \brief Verify the the SU (pre-memory) CRCs match the PPU (post-memory) CRCs
//!
//! \param display : display to verify CRCs on
//!
//! \return OK if CRC check passed, not OK otherwise
RC GsyncLink::CheckCallwlatedCrcs(UINT32 gsyncIdx, UINT32 patternIdx)
{
    vector<GsyncDevice::CrcData> suCrcs;
    vector<GsyncDevice::CrcData> ppuCrcs;
    RC rc;
    const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
    const Resolution surfaceRes = m_WdAndHt[displayMask];

    CHECK_RC(m_GsyncDevices[gsyncIdx].GetCrcs(true,
        m_NumFrameCrcs, &suCrcs, &ppuCrcs));

    // An even frames worth of CRCs should have been retrieved
    MASSERT(suCrcs.size() == ppuCrcs.size());

    for (UINT32 frameIdx = 0; frameIdx < suCrcs.size(); frameIdx++)
    {
        GsyncDevice::CrcData &suFrameCrcs  = suCrcs[frameIdx];
        GsyncDevice::CrcData &ppuFrameCrcs = ppuCrcs[frameIdx];

        for (UINT32 crcIdx = 0; crcIdx < suFrameCrcs.size(); crcIdx++)
        {
            if (suFrameCrcs[crcIdx] != ppuFrameCrcs[crcIdx])
            {
                Printf(Tee::PriError,
                       "%s SORs:(%s), Display 0x%08x, Loop %d, Frame %u, CRC %u SU/PPU "
                       "mismatch! (SU CRC 0x%08x, PPU CRC 0x%08x)\n",
                       GetName().c_str(),
                       CreateSorString(m_DisplaySors).c_str(),
                       displayMask,
                       m_LoopIdx,
                       static_cast<UINT32>(frameIdx),
                       static_cast<UINT32>(crcIdx),
                       suFrameCrcs[crcIdx],
                       ppuFrameCrcs[crcIdx]);
                if (m_pGolden->GetStopOnError())
                    return RC::GOLDEN_VALUE_MISCOMPARE;
                else
                    m_CrcMiscompare = true;
            }

            if (suFrameCrcs[crcIdx] !=
                m_SurfacePatterns[patternIdx].surfaceData[surfaceRes].calcCrcs[crcIdx])
            {
                Printf(Tee::PriError,
                       "%s SORs:(%s), Display 0x%08x, Loop %d, Frame %u, CRC %u calc vs SU "
                       "mismatch! (SU CRC 0x%08x, calc CRC 0x%08x) pattern %u\n",
                       GetName().c_str(),
                       CreateSorString(m_DisplaySors).c_str(),
                       displayMask,
                       m_LoopIdx,
                       static_cast<UINT32>(frameIdx),
                       static_cast<UINT32>(crcIdx),
                       suFrameCrcs[crcIdx],
                       m_SurfacePatterns[patternIdx].surfaceData[surfaceRes].calcCrcs[crcIdx], patternIdx);
                if (m_pGolden->GetStopOnError())
                    return RC::GOLDEN_VALUE_MISCOMPARE;
                else
                    m_CrcMiscompare = true;
            }
        }
    }
    return rc;
}

RC GsyncLink::LwDisplaySetModeGsync
(
    UINT32 dispId,
    UINT32 dispWidth,
    UINT32 dispHeight,
    UINT32 dispRefRate
)
{
    RC rc;
    LwDisplay*  pLwDisplay = static_cast<LwDisplay *>(m_pDisplay);
    const UINT32 dispDepth = m_pTestConfig->DisplayDepth();
    const Display::Mode resolutionInfo(dispWidth, dispHeight, dispDepth, dispRefRate);
    UINT32 head = Display::ILWALID_HEAD;
    CHECK_RC(pLwDisplay->GetFirstSupportedHead(dispId, &head));
    if (head == Display::ILWALID_HEAD)
    {
        Printf(Tee::PriError,
                "GsyncLink::%s - No head is free for running setmode\n",
                __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    return pLwDisplay->SetModeWithSingleWindow(dispId,
            resolutionInfo,
            head);
}

RC GsyncLink::LwDisplaySetImageGsync(UINT32 dispId, Surface2D *pSurf)
{
    LwDisplay * pLwDisplay = static_cast<LwDisplay *>(m_pDisplay);
    return pLwDisplay->DisplayImage(dispId, pSurf, nullptr);
}

RC GsyncLink::DumpSurfaceInPng
(
    UINT32 frame,
    UINT32 loop,
    const string &suffix,
    Surface2D *pSurf
) const
{
    RC rc;
    string imgName = Utility::StrPrintf(
            "F_%u.L_%u.%s.png", frame, loop, suffix.c_str());
    CHECK_RC(pSurf->WritePng(imgName.c_str()));
    return rc;
}

//
//------------------------------------------------------------------------------
// GsyncLink object, properties and methods.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GsyncLink, GpuTest, "GsyncLink Test");
CLASS_PROP_READWRITE(GsyncLink, NumFrameCrcs, UINT32,
        "Number of frames to capture CRC for (default = 10)");
CLASS_PROP_READWRITE(GsyncLink, CalcCrc, bool,
        "Set to true to enable CRC callwlations otherwise use goldens");
CLASS_PROP_READWRITE(GsyncLink, IncludeRandom, bool,
        "Include random pattern in the default set");
CLASS_PROP_READWRITE(GsyncLink, EnableXbarCombinations, bool,
        "Set to true to enable iterations over all SORs on supporting GPUs");
CLASS_PROP_READWRITE(GsyncLink, RunParallel, bool,
        "Set to false to test each connected Gsync device sequentially.");
CLASS_PROP_READWRITE(GsyncLink, DumpImages, bool,
        "Dump Images in F_<PatternIdx>.L_<LoopNum>.<Resolution>.png");
CLASS_PROP_READWRITE(GsyncLink, TestResolution, std::string,
        "Test image resolution \"<Wd>:<Ht>:<refrate>\" or \"native\"(default)");
CLASS_PROP_READWRITE(GsyncLink, EnableDMA, bool,
        "Use DmaWrapper to fill window surfaces");
CLASS_PROP_READWRITE(GsyncLink, UseMemoryFillFns, bool,
        "Use Memory fill functions to fill surfaces");
