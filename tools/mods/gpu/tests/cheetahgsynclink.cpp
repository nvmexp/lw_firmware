/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// TegraGsyncLink : Test for display interaces using the gsync external CRC hw

#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0073.h"
#include "gpu/display/tegra_disp.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gsynccrc.h"
#include "gpu/include/tegragsyncdev.h"
#include "gputest.h"

#ifndef INCLUDED_STL_UTILITY_H
#include <utility> //pair
#endif

// TegraGsyncLink Class
//
// Test a Gsync enabled DP display by displaying various test patterns and then
// reading and checking CRCs from the Gsync device against callwlated CRCs
// or alternatively using golden values
//

class TegraGsyncLink : public GpuTest
{
public:
    TegraGsyncLink();
    virtual ~TegraGsyncLink() {};

    bool IsSupported() override;
    virtual RC Setup() override;
    virtual RC Run() override;
    virtual RC Cleanup() override;
    virtual RC InitFromJs() override;
    virtual void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(NumFrameCrcs, UINT32);
    SETGET_PROP(CalcCrc, bool);
    SETGET_PROP(IncludeRandom, bool);
    SETGET_PROP(EnableXbarCombinations, bool);
    SETGET_PROP(RunParallel, bool);
    SETGET_PROP(DumpImages, bool);
    SETGET_PROP(TestResolution, string);

private:
    typedef pair<UINT32, UINT32> Resolution;

    RC GenerateSurfaces(Resolution surfaceRes, bool isR3Device);
    RC CheckCallwlatedCrcs(UINT32 gsyncIdx, UINT32 patternIdx);
    void CleanSORExcludeMask(vector<UINT32>* displaysToRestore);

    RC UpdateTestResolution(const UINT32 displayMask);

    GpuTestConfiguration *    m_pTestConfig;            //!< Pointer to test configuration
    vector<GpuGoldenSurfaces> m_pGGSurfs;               //!< Pointer to Gpu golden surfaces
    Goldelwalues *            m_pGolden;                //!< Pointer to golden values
    UINT32                    m_OrigDisplays  = 0;      //!< Originally connected display
    TegraDisplay *            m_pDisplay      = nullptr;//!< Pointer to TegraDisplay
    bool                      m_CrcMiscompare = false;  //!< True on a crc miscompare
    UINT32                    m_LoopIdx       = 0;      //!< For inter-function comm
    vector<UINT32>            m_DisplaySors;            //!< For inter-function comm
    map<UINT32, UINT32>       m_RefreshRate;            //!< Map of display Mask, Refresh Rate
    map<UINT32, Resolution>   m_WdAndHt;                //!< Test resolution width and height

    vector<TegraGsyncDevice>  m_GsyncDevices;

    //! Structure containing surfaces
    struct SurfaceData
    {
        Surface2D surface; //!< Surface containing the pattern
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
    vector<SurfacePattern> m_SurfacePatterns;  //!< Test patterns

    // Variables set from JS
    UINT32 m_NumFrameCrcs           = 10;    //!< Number of frames to capture CRCs
    bool   m_CalcCrc                = true;  //!< Callwlate expected CRC instead of using goldens
    bool   m_IncludeRandom          = true;  //!< Include random pattern
    // TODO Need to test the argument when SOR to Head cross bar enabled
    bool   m_EnableXbarCombinations = true;  //!< On GM20x and above iterate over all SORs
    // TODO Need to test the argument when multiple panels are connected
    bool   m_RunParallel            = true;  //!< Run on all available gsync devices in parallel
    bool   m_DumpImages             = false; //If true dump surface contents after setting up.
    string m_TestResolution         = "native";

    RC DumpSurfaceInPng
    (
        UINT32 frame
        ,UINT32 loop
        ,const string &suffix
        ,Surface2D *pSurf
    ) const;
    static const UINT32 s_DefaultPatterns[][4];
};

const UINT32 TegraGsyncLink::s_DefaultPatterns[][4] =
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
TegraGsyncLink::TegraGsyncLink()
{
    SetName("TegraGsyncLink");
    m_pTestConfig = GetTestConfiguration();
    m_pGolden     = GetGoldelwalues();
}

bool TegraGsyncLink::IsSupported()
{
    return GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_GSYNC_R3);
}

RC TegraGsyncLink::UpdateTestResolution(const UINT32 displayMask)
{
    RC rc;
    vector <string> resStr;
    string usage("-testarg 425 TestResolution \"<Wd>:<Ht>:<RefRate>\" or \"native\"");
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
//! \brief Setup the TegraGsyncLink
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC TegraGsyncLink::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());
    m_pDisplay = GetDisplay()->GetTegraDisplay();
    m_OrigDisplays = m_pDisplay->Selected();

    m_GsyncDevices.clear();
    CHECK_RC(TegraGsyncDevice::FindTegraGsyncDevices(m_pDisplay,
                                           GetBoundGpuSubdevice(),
                                           &m_GsyncDevices));

    for (auto it = m_GsyncDevices.begin(); it != m_GsyncDevices.end();)
    {
        ++it;
    }

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
        if (gsyncDevice.GetMaxCrcFrames(GsyncDevice::CRC_PRE_AND_POST_MEM) < maxAllowedCrcs)
        {
            maxAllowedCrcs = gsyncDevice.GetMaxCrcFrames(GsyncDevice::CRC_PRE_AND_POST_MEM);
        }
    }
    if ((m_NumFrameCrcs == 0) || (m_NumFrameCrcs > maxAllowedCrcs))
    {
        UINT32 defaultNumFrameCrcs = (maxAllowedCrcs > 10) ? 10: maxAllowedCrcs;
        Printf(Tee::PriError,
              "%s : NumFrameCrcs (%u) out of range (1, %u), defaulting to %u\n",
              GetName().c_str(), m_NumFrameCrcs, maxAllowedCrcs, defaultNumFrameCrcs);
        m_NumFrameCrcs = defaultNumFrameCrcs;
    }

    UINT32 conlwrrentDevices = (m_RunParallel) ?
        static_cast<UINT32>(m_GsyncDevices.size()) : 1;

    m_DisplaySors.resize(conlwrrentDevices);

    if (!m_CalcCrc)
    {
        m_pGGSurfs.resize(m_GsyncDevices.size(), GpuGoldenSurfaces(GetBoundGpuDevice()));
    }

    for (size_t gsyncIdx = 0; gsyncIdx < m_GsyncDevices.size(); gsyncIdx++)
    {
        const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
        CHECK_RC(UpdateTestResolution(displayMask));

        m_GsyncDevices[gsyncIdx].SetNumCrcFrames(m_NumFrameCrcs);

        const Resolution resolution = m_WdAndHt[displayMask];
        if (m_SurfacePatterns[0].surfaceData.find(m_WdAndHt[displayMask])
                == m_SurfacePatterns[0].surfaceData.end())
        {
            CHECK_RC(GenerateSurfaces(resolution,
                        m_GsyncDevices[gsyncIdx].IsDeviceR3Capable()));
        }

        if (!m_CalcCrc)
        {
            m_pGGSurfs[gsyncIdx].AttachSurface(&m_SurfacePatterns[0].
                                               surfaceData[resolution].surface,
                                               "C",
                                               displayMask,
                                               &m_GsyncDevices[gsyncIdx]);
        }
    }
    return rc;
}

#if 0
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
    // That is: CRC0 as pixel 0, 4, 8, ... Etc. CRC1 has 1, 5, 9, ...

    // "The polynomial is the following one:
    // crc[31:0]=1+x^1+x^2+x^4+x^5+x^7+x^8+x^10+x^11+x^12+x^16+x^22+x^23+x^26+x^32;
    // The LFSR is initialized with a value of 1 at vsync.
    // The 24-bit input is arranged as (blue[7:0] << 16 | green[7:0] << 8 | red[7:0])

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

    lfsr_c[0] = lfsr_q[8] ^ lfsr_q[14] ^ lfsr_q[17] ^ lfsr_q[18] ^ lfsr_q[20] ^ lfsr_q[24] ^
            data_in[0] ^ data_in[6] ^ data_in[9] ^ data_in[10] ^ data_in[12] ^ data_in[16];

    lfsr_c[1] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[17] ^ lfsr_q[19] ^
            lfsr_q[20] ^ lfsr_q[21] ^ lfsr_q[24] ^ lfsr_q[25] ^ data_in[0] ^ data_in[1] ^
            data_in[6] ^ data_in[7] ^ data_in[9] ^ data_in[11] ^ data_in[12] ^ data_in[13] ^
            data_in[16] ^ data_in[17];

    lfsr_c[2] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[16] ^
            lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[26] ^
            data_in[0] ^ data_in[1] ^ data_in[2] ^ data_in[6] ^ data_in[7] ^ data_in[8] ^
            data_in[9] ^ data_in[13] ^ data_in[14] ^ data_in[16] ^ data_in[17] ^ data_in[18];

    lfsr_c[3] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[17] ^
            lfsr_q[18] ^ lfsr_q[22] ^ lfsr_q[23] ^ lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[27] ^
            data_in[1] ^ data_in[2] ^ data_in[3] ^ data_in[7] ^ data_in[8] ^ data_in[9] ^
            data_in[10] ^ data_in[14] ^ data_in[15] ^ data_in[17] ^ data_in[18] ^ data_in[19];

    lfsr_c[4] = lfsr_q[8] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[14] ^ lfsr_q[16] ^
            lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[23] ^ lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[28] ^
            data_in[0] ^ data_in[2] ^ data_in[3] ^ data_in[4] ^ data_in[6] ^ data_in[8] ^
            data_in[11] ^ data_in[12] ^ data_in[15] ^ data_in[18] ^ data_in[19] ^ data_in[20];

    lfsr_c[5] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[14] ^
            lfsr_q[15] ^ lfsr_q[18] ^ lfsr_q[21] ^ lfsr_q[27] ^ lfsr_q[28] ^ lfsr_q[29] ^
            data_in[0] ^ data_in[1] ^ data_in[3] ^ data_in[4] ^ data_in[5] ^ data_in[6] ^
            data_in[7] ^ data_in[10] ^ data_in[13] ^ data_in[19] ^ data_in[20] ^ data_in[21];

    lfsr_c[6] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[15] ^
            lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[22] ^ lfsr_q[28] ^ lfsr_q[29] ^ lfsr_q[30] ^
            data_in[1] ^ data_in[2] ^ data_in[4] ^ data_in[5] ^ data_in[6] ^ data_in[7] ^
            data_in[8] ^ data_in[11] ^ data_in[14] ^ data_in[20] ^ data_in[21] ^ data_in[22];

    lfsr_c[7] = lfsr_q[8] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[13] ^ lfsr_q[15] ^ lfsr_q[16] ^
            lfsr_q[18] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[29] ^ lfsr_q[30] ^ lfsr_q[31] ^
            data_in[0] ^ data_in[2] ^ data_in[3] ^ data_in[5] ^ data_in[7] ^ data_in[8] ^
            data_in[10] ^ data_in[15] ^ data_in[16] ^ data_in[21] ^ data_in[22] ^ data_in[23];

    lfsr_c[8] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[16] ^ lfsr_q[18] ^
            lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[25] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[0] ^
            data_in[1] ^ data_in[3] ^ data_in[4] ^ data_in[8] ^ data_in[10] ^ data_in[11] ^
            data_in[12] ^ data_in[17] ^ data_in[22] ^ data_in[23];
    lfsr_c[9] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[17] ^ lfsr_q[19] ^
            lfsr_q[20] ^ lfsr_q[21] ^ lfsr_q[26] ^ lfsr_q[31] ^ data_in[1] ^ data_in[2] ^
            data_in[4] ^ data_in[5] ^ data_in[9] ^ data_in[11] ^ data_in[12] ^ data_in[13] ^
            data_in[18] ^ data_in[23];

    lfsr_c[10] = lfsr_q[8] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[13] ^ lfsr_q[17] ^ lfsr_q[21] ^
            lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[27] ^ data_in[0] ^ data_in[2] ^ data_in[3] ^
            data_in[5] ^ data_in[9] ^ data_in[13] ^ data_in[14] ^ data_in[16] ^ data_in[19];

    lfsr_c[11] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[17] ^ lfsr_q[20] ^
            lfsr_q[22] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[28] ^ data_in[0] ^
            data_in[1] ^ data_in[3] ^ data_in[4] ^ data_in[9] ^ data_in[12] ^ data_in[14] ^
            data_in[15] ^ data_in[16] ^ data_in[17] ^ data_in[20];

    lfsr_c[12] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[14] ^
            lfsr_q[17] ^ lfsr_q[20] ^ lfsr_q[21] ^ lfsr_q[23] ^ lfsr_q[25] ^ lfsr_q[26] ^
            lfsr_q[29] ^ data_in[0] ^ data_in[1] ^ data_in[2] ^ data_in[4] ^ data_in[5] ^
            data_in[6] ^ data_in[9] ^ data_in[12] ^ data_in[13] ^ data_in[15] ^ data_in[17] ^
            data_in[18] ^ data_in[21];

    lfsr_c[13] = lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[15] ^
            lfsr_q[18] ^ lfsr_q[21] ^ lfsr_q[22] ^ lfsr_q[24] ^ lfsr_q[26] ^ lfsr_q[27] ^
            lfsr_q[30] ^ data_in[1] ^ data_in[2] ^ data_in[3] ^ data_in[5] ^ data_in[6] ^
            data_in[7] ^ data_in[10] ^ data_in[13] ^ data_in[14] ^ data_in[16] ^ data_in[18] ^
            data_in[19] ^ data_in[22];

    lfsr_c[14] = lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[16] ^
            lfsr_q[19] ^ lfsr_q[22] ^ lfsr_q[23] ^ lfsr_q[25] ^ lfsr_q[27] ^ lfsr_q[28] ^
            lfsr_q[31] ^ data_in[2] ^ data_in[3] ^ data_in[4] ^ data_in[6] ^ data_in[7] ^
            data_in[8] ^ data_in[11] ^ data_in[14] ^ data_in[15] ^ data_in[17] ^ data_in[19] ^
            data_in[20] ^ data_in[23];

    lfsr_c[15] = lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[17] ^
            lfsr_q[20] ^ lfsr_q[23] ^ lfsr_q[24] ^ lfsr_q[26] ^ lfsr_q[28] ^ lfsr_q[29] ^
            data_in[3] ^ data_in[4] ^ data_in[5] ^ data_in[7] ^ data_in[8] ^ data_in[9] ^
            data_in[12] ^ data_in[15] ^ data_in[16] ^ data_in[18] ^ data_in[20] ^ data_in[21];

    lfsr_c[16] = lfsr_q[8] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[16] ^ lfsr_q[20] ^ lfsr_q[21] ^
            lfsr_q[25] ^ lfsr_q[27] ^ lfsr_q[29] ^ lfsr_q[30] ^ data_in[0] ^ data_in[4] ^
            data_in[5] ^ data_in[8] ^ data_in[12] ^ data_in[13] ^ data_in[17] ^ data_in[19] ^
            data_in[21] ^ data_in[22];
    lfsr_c[17] = lfsr_q[9] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[22] ^
            lfsr_q[26] ^ lfsr_q[28] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[1] ^ data_in[5] ^
            data_in[6] ^ data_in[9] ^ data_in[13] ^ data_in[14] ^ data_in[18] ^ data_in[20] ^
            data_in[22] ^ data_in[23];

    lfsr_c[18] = lfsr_q[10] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[18] ^ lfsr_q[22] ^ lfsr_q[23] ^
            lfsr_q[27] ^ lfsr_q[29] ^ lfsr_q[31] ^ data_in[2] ^ data_in[6] ^ data_in[7] ^
            data_in[10] ^ data_in[14] ^ data_in[15] ^ data_in[19] ^ data_in[21] ^ data_in[23];

    lfsr_c[19] = lfsr_q[11] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[23] ^ lfsr_q[24] ^
            lfsr_q[28] ^ lfsr_q[30] ^ data_in[3] ^ data_in[7] ^ data_in[8] ^ data_in[11] ^
            data_in[15] ^ data_in[16] ^ data_in[20] ^ data_in[22];

    lfsr_c[20] = lfsr_q[12] ^ lfsr_q[16] ^ lfsr_q[17] ^ lfsr_q[20] ^ lfsr_q[24] ^ lfsr_q[25] ^
            lfsr_q[29] ^ lfsr_q[31] ^ data_in[4] ^ data_in[8] ^ data_in[9] ^ data_in[12] ^
            data_in[16] ^ data_in[17] ^ data_in[21] ^ data_in[23];

    lfsr_c[21] = lfsr_q[13] ^ lfsr_q[17] ^ lfsr_q[18] ^ lfsr_q[21] ^ lfsr_q[25] ^ lfsr_q[26] ^
            lfsr_q[30] ^ data_in[5] ^ data_in[9] ^ data_in[10] ^ data_in[13] ^ data_in[17] ^
            data_in[18] ^ data_in[22];

    lfsr_c[22] = lfsr_q[8] ^ lfsr_q[17] ^ lfsr_q[19] ^ lfsr_q[20] ^ lfsr_q[22] ^ lfsr_q[24] ^
            lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[31] ^ data_in[0] ^ data_in[9] ^ data_in[11] ^
            data_in[12] ^ data_in[14] ^ data_in[16] ^ data_in[18] ^ data_in[19] ^ data_in[23];

    lfsr_c[23] = lfsr_q[8] ^ lfsr_q[9] ^ lfsr_q[14] ^ lfsr_q[17] ^ lfsr_q[21] ^ lfsr_q[23] ^
            lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[27] ^ lfsr_q[28] ^ data_in[0] ^ data_in[1] ^
            data_in[6] ^ data_in[9] ^ data_in[13] ^ data_in[15] ^ data_in[16] ^ data_in[17] ^
            data_in[19] ^ data_in[20];

    lfsr_c[24] = lfsr_q[0] ^ lfsr_q[9] ^ lfsr_q[10] ^ lfsr_q[15] ^ lfsr_q[18] ^ lfsr_q[22] ^
            lfsr_q[24] ^ lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[28] ^ lfsr_q[29] ^ data_in[1] ^
            data_in[2] ^ data_in[7] ^ data_in[10] ^ data_in[14] ^ data_in[16] ^ data_in[17] ^
            data_in[18] ^ data_in[20] ^ data_in[21];

    lfsr_c[25] = lfsr_q[1] ^ lfsr_q[10] ^ lfsr_q[11] ^ lfsr_q[16] ^ lfsr_q[19] ^ lfsr_q[23] ^
            lfsr_q[25] ^ lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[29] ^ lfsr_q[30] ^ data_in[2] ^
            data_in[3] ^ data_in[8] ^ data_in[11] ^ data_in[15] ^ data_in[17] ^ data_in[18] ^
            data_in[19] ^ data_in[21] ^ data_in[22];

    lfsr_c[26] = lfsr_q[2] ^ lfsr_q[8] ^ lfsr_q[11] ^ lfsr_q[12] ^ lfsr_q[14] ^ lfsr_q[18] ^
            lfsr_q[26] ^ lfsr_q[27] ^ lfsr_q[28] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[0] ^
            data_in[3] ^ data_in[4] ^ data_in[6] ^ data_in[10] ^ data_in[18] ^ data_in[19] ^
            data_in[20] ^ data_in[22] ^ data_in[23];

    lfsr_c[27] = lfsr_q[3] ^ lfsr_q[9] ^ lfsr_q[12] ^ lfsr_q[13] ^ lfsr_q[15] ^ lfsr_q[19] ^
            lfsr_q[27] ^ lfsr_q[28] ^ lfsr_q[29] ^ lfsr_q[31] ^ data_in[1] ^ data_in[4] ^
            data_in[5] ^ data_in[7] ^ data_in[11] ^ data_in[19] ^ data_in[20] ^ data_in[21] ^
            data_in[23];

    lfsr_c[28] = lfsr_q[4] ^ lfsr_q[10] ^ lfsr_q[13] ^ lfsr_q[14] ^ lfsr_q[16] ^ lfsr_q[20] ^
            lfsr_q[28] ^ lfsr_q[29] ^ lfsr_q[30] ^ data_in[2] ^ data_in[5] ^ data_in[6] ^
            data_in[8] ^ data_in[12] ^ data_in[20] ^ data_in[21] ^ data_in[22];

    lfsr_c[29] = lfsr_q[5] ^ lfsr_q[11] ^ lfsr_q[14] ^ lfsr_q[15] ^ lfsr_q[17] ^ lfsr_q[21] ^
            lfsr_q[29] ^ lfsr_q[30] ^ lfsr_q[31] ^ data_in[3] ^ data_in[6] ^ data_in[7] ^
            data_in[9] ^ data_in[13] ^ data_in[21] ^ data_in[22] ^ data_in[23];

    lfsr_c[30] = lfsr_q[6] ^ lfsr_q[12] ^ lfsr_q[15] ^ lfsr_q[16] ^ lfsr_q[18] ^ lfsr_q[22] ^
            lfsr_q[30] ^ lfsr_q[31] ^ data_in[4] ^ data_in[7] ^ data_in[8] ^ data_in[10] ^
            data_in[14] ^ data_in[22] ^ data_in[23];

    lfsr_c[31] = lfsr_q[7] ^ lfsr_q[13] ^ lfsr_q[16] ^ lfsr_q[17] ^ lfsr_q[19] ^ lfsr_q[23] ^
            lfsr_q[31] ^ data_in[5] ^ data_in[8] ^ data_in[9] ^ data_in[11] ^ data_in[15] ^
            data_in[23];

}
#endif

static string CreateSorString(const vector<UINT32>& displaySors)
{
    string ret = "";
    for (UINT32 i = 0; i < displaySors.size(); i++)
    {
        ret += Utility::StrPrintf("%s%d%s",
                                  (i != 0) ? " ":"",
                                  displaySors[i],
                                  (i != displaySors.size()-1) ? ",":"");
    }
    return ret;
}

RC TegraGsyncLink::GenerateSurfaces(Resolution surfaceRes, bool isR3Device)
{
    RC rc;

    Surface2D sysmemSurf;
    sysmemSurf.SetWidth(surfaceRes.first);
    sysmemSurf.SetHeight(surfaceRes.second);
    // This format makes UINT32 pixel values to be in the same color order
    // as used during CRC callwlations:
    sysmemSurf.SetColorFormat(ColorUtils::A8B8G8R8);
    sysmemSurf.SetLocation(Memory::Coherent);
    sysmemSurf.SetLayout(Surface2D::Pitch);
    CHECK_RC(sysmemSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(sysmemSurf.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));

    DmaWrapper dmaWrap;
    CHECK_RC(dmaWrap.Initialize(m_pTestConfig, Memory::Optimal));

    const UINT32 widthBytes = sysmemSurf.GetWidth() *
                              sysmemSurf.GetBytesPerPixel();
    for (size_t ii = 0; ii < m_SurfacePatterns.size(); ii++)
    {
        m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.SetWidth(sysmemSurf.GetWidth());
        m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.SetHeight(sysmemSurf.GetHeight());
        m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.SetColorFormat(
                sysmemSurf.GetColorFormat());
        m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.SetLocation(Memory::Fb);
        m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.SetType(LWOS32_TYPE_PRIMARY);
        m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.SetDisplayable(true);
        m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.SetLayout(Surface2D::Pitch);
        CHECK_RC(m_SurfacePatterns[ii].surfaceData[surfaceRes].surface.Alloc(GetBoundGpuDevice()));

        if (m_SurfacePatterns[ii].random)
        {
            CHECK_RC(Memory::FillRandom(sysmemSurf.GetAddress(),
                                        0,
                                        0xFF000000,
                                        0xFFFFFFFF,
                                        sysmemSurf.GetWidth(),
                                        sysmemSurf.GetHeight(),
                                        32,
                                        sysmemSurf.GetPitch()));
        }
        else
        {
            CHECK_RC(Memory::FillSurfaceWithPattern(sysmemSurf.GetAddress(),
                                                    &m_SurfacePatterns[ii].pattern,
                                                    widthBytes,
                                                    sysmemSurf.GetHeight(),
                                                    sysmemSurf.GetPitch()));
        }

        if (m_CalcCrc)
        {
            GsyncCRC(isR3Device).GenCrc(&sysmemSurf,
                    m_SurfacePatterns[ii].surfaceData[surfaceRes].calcCrcs);
        }

        CHECK_RC(dmaWrap.SetSrcDstSurf(&sysmemSurf,
                                       &m_SurfacePatterns[ii].surfaceData[surfaceRes].surface));

        // Do the DMA copy and wait for completion
        CHECK_RC(dmaWrap.Copy(0, 0, widthBytes, sysmemSurf.GetHeight(),
                              0, 0, m_pTestConfig->TimeoutMs(),
                              GetBoundGpuSubdevice()->GetSubdeviceInst(),
                              true));
    }

    sysmemSurf.Unmap();
    sysmemSurf.Free();

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the TegraGsyncLink
//!
//! \return OK if running was successful, not OK otherwise
//!
RC TegraGsyncLink::Run()
{
    RC rc;

    UINT32 numSorConfigurations = 1;
    bool testXbarCombinations = m_EnableXbarCombinations &&
        GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR);
    if (testXbarCombinations)
    {
        numSorConfigurations = LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS;
    }

    const UINT32 conlwrrentDevices = (m_RunParallel) ?
        static_cast<UINT32>(m_GsyncDevices.size()) : 1;

    if (conlwrrentDevices > numSorConfigurations)
    {
        Printf(Tee::PriError,
                "%s error: not enough SORs (%d) for number of conlwrrent gsync devices (%d)\n",
               GetName().c_str(), numSorConfigurations, conlwrrentDevices);
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    // Set to false here so multiple calls to Run are possible
    m_CrcMiscompare = false;

    // Will do one loop if m_RunParallel, otherwise will loop over each gsync device
    for (size_t gsyncIdxOffset = 0; gsyncIdxOffset < m_GsyncDevices.size();
            gsyncIdxOffset += conlwrrentDevices)
    {
        // When generating goldens it is only necessary to generate goldens once per
        // pattern per gsync device group (rather than using loop number to store CRCs in the
        // database, pattern number is used instead)
        set<UINT32> patternsToCrc;
        if (!m_CalcCrc && m_pGolden->GetAction() == Goldelwalues::Store)
        {
            for (size_t ii = 0; ii < m_SurfacePatterns.size(); ii++)
            {
                patternsToCrc.insert(static_cast<UINT32>(ii));
            }
        }

        // Will loop over each gsync device if m_RunParallel,
        // otherwise will run once for the current gsync device
        for (size_t gsyncIdx = gsyncIdxOffset; gsyncIdx < (gsyncIdxOffset + conlwrrentDevices);
                gsyncIdx++)
        {
            const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
            Printf(GetVerbosePrintPri(), "Testing Gsync display 0x%08x\n", displayMask);
        }

        // And make sure all of the SORs get tested at least once
        for (UINT32 sorConfOffset = 0;
             sorConfOffset + conlwrrentDevices <= numSorConfigurations;
             sorConfOffset += (sorConfOffset + conlwrrentDevices <= numSorConfigurations)
                              ? conlwrrentDevices : 1)
        {
// TODO Need to evaluate if SOR switching is neeeded
#if 0
            UINT32 assignedSORMask = 0;
            vector<UINT32> restoreSORExcludeMaskDisplays;
            Utility::CleanupOnReturnArg<TegraGsyncLink, void, vector<UINT32>*>
                    restoreSORExcludeMask(this, &TegraGsyncLink::CleanSORExcludeMask,
                    &restoreSORExcludeMaskDisplays);

            for (size_t gsyncIdx = gsyncIdxOffset; gsyncIdx < (gsyncIdxOffset + conlwrrentDevices);
                    gsyncIdx++)
            {
                const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
                const UINT32 surfaceIdx = (m_RunParallel) ? static_cast<UINT32>(gsyncIdx) : 0x0;

                for (UINT32 sorConfIdx = sorConfOffset; sorConfIdx < numSorConfigurations;
                    sorConfIdx++)
                {
                    if ((1 << sorConfIdx) & assignedSORMask)
                        continue;

                    if (testXbarCombinations)
                    {
                        CHECK_RC(m_pDisplay->DetachAllDisplays());
                        UINT32 excludeMask = ~(1 << sorConfIdx);
                        vector<UINT32> sorList;
                        // Call this function just to check if the assignment is
                        // possible. The SOR will be assigned again in SetupXBar.
                        RC assignRc = m_pDisplay->AssignSOR(
                            DisplayIDs(1, displayMask), excludeMask, sorList);

                        if (assignRc == RC::LWRM_NOT_SUPPORTED)
                            continue;

                        CHECK_RC(assignRc);

                        restoreSORExcludeMaskDisplays.push_back(displayMask);
                        // Need to call SetSORExcludeMask here as GetORProtocol calls
                        // SetupXBar now.
                        CHECK_RC(m_pDisplay->SetSORExcludeMask(displayMask,
                            excludeMask));

                        CHECK_RC(m_pDisplay->GetORProtocol(displayMask,
                             &m_DisplaySors[surfaceIdx], nullptr, nullptr));

                        if (m_DisplaySors[surfaceIdx] != sorConfIdx)
                        {
                            Printf(Tee::PriNormal,
                                "%s error: GetORProtocol returned unexpected "
                                "SOR%d, when SOR%d was expected.\n",
                                GetName().c_str(), m_DisplaySors[surfaceIdx], sorConfIdx);
                            return RC::SOFTWARE_ERROR;
                        }
                    }
                    else
                    {
                        CHECK_RC(m_pDisplay->GetORProtocol(displayMask,
                             &m_DisplaySors[surfaceIdx], nullptr, nullptr));
                    }

                    assignedSORMask |= (1 << sorConfIdx);
                    break;
                }
            }
            Printf(GetVerbosePrintPri(), "Testing SORs: %s\n",
                    CreateSorString(m_DisplaySors).c_str());
#endif
            UINT32 selectedDisplays = 0x0;
            for (size_t gsyncIdx = gsyncIdxOffset; gsyncIdx < (gsyncIdxOffset + conlwrrentDevices);
                    gsyncIdx++)
            {
                selectedDisplays |= m_GsyncDevices[gsyncIdx].GetDisplayMask();
            }

            CHECK_RC(m_pDisplay->Select(selectedDisplays));

            for (size_t gsyncIdx = gsyncIdxOffset; gsyncIdx < (gsyncIdxOffset + conlwrrentDevices);
                    gsyncIdx++)
            {
                const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
                const UINT32 width  = m_WdAndHt[displayMask].first;
                const UINT32 height = m_WdAndHt[displayMask].second;
                UINT32 dispRefRate = m_RefreshRate[displayMask];
                TegraDisplay::SetModeAdditionalParams params(false,
                        TegraDisplay::OutputPixelEncoding());
                CHECK_RC(m_pDisplay->SetMode(
                            displayMask,
                            width,
                            height,
                            m_pTestConfig->DisplayDepth(),
                            dispRefRate,
                            params
                         ));
                CHECK_RC(m_GsyncDevices[gsyncIdx].FactoryReset());
                CHECK_RC(m_GsyncDevices[gsyncIdx].NotifyModeset());

                UINT32 gsyncRefRate = 0;
                // Value of 1000 for retries was reached experimentally and is a WAR!
                UINT32 numRetriesLeft = 1000;
                while (gsyncRefRate == 0 && numRetriesLeft != 0)
                {
                    CHECK_RC(m_GsyncDevices[gsyncIdx].GetRefreshRate(&gsyncRefRate));
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

            UINT32 StartLoop = m_pTestConfig->StartLoop();
            UINT32 EndLoop   = StartLoop + m_pTestConfig->Loops();
            for (m_LoopIdx = StartLoop; m_LoopIdx < EndLoop; m_LoopIdx++)
            {
                const UINT32 patternIdx = m_LoopIdx % m_SurfacePatterns.size();

                for (size_t gsyncIdx = gsyncIdxOffset;
                        gsyncIdx < (gsyncIdxOffset + conlwrrentDevices);
                        gsyncIdx++)
                {
                    const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();

                    const Resolution surfaceRes = m_WdAndHt[displayMask];
                    Surface2D *pSurf = &m_SurfacePatterns[patternIdx].
                                                surfaceData[surfaceRes].surface;

                    if (m_DumpImages)
                    {
                        string rStr = Utility::StrPrintf("%u*%u@%u",
                                surfaceRes.first, surfaceRes.second, m_RefreshRate[displayMask]);
                        CHECK_RC(DumpSurfaceInPng(patternIdx, m_LoopIdx, rStr, pSurf));
                    }

                    CHECK_RC(m_pDisplay->SetImage(displayMask, pSurf));

                    CHECK_RC(m_GsyncDevices[gsyncIdx].NotifyImageChange());
                }

                for (size_t gsyncIdx = gsyncIdxOffset;
                        gsyncIdx < (gsyncIdxOffset + conlwrrentDevices); gsyncIdx++)
                {
                    if (m_CalcCrc)
                    {
                        CHECK_RC(CheckCallwlatedCrcs(static_cast<UINT32>(gsyncIdx),
                                    patternIdx));
                    }
                    else
                    {
                        const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
                        const Resolution surfaceRes = m_WdAndHt[displayMask];

                        m_pGolden->ClearSurfaces();
                        m_pGGSurfs[gsyncIdx].ReplaceSurface(0,
                                &m_SurfacePatterns[patternIdx].surfaceData[surfaceRes].surface);
                        CHECK_RC(m_pGolden->SetSurfaces(&m_pGGSurfs[gsyncIdx]));

                        m_pGolden->SetLoop(patternIdx);
                        if (OK != (rc = m_pGolden->Run()))
                        {
                            Printf(Tee::PriError,
                                    "Golden error on Loop %u "
                                    "(pattern %u, display 0x%08x, SORs: %s)\n",
                                    m_LoopIdx, displayMask, patternIdx,
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

            for (size_t gsyncIdx = gsyncIdxOffset;
                    gsyncIdx < (gsyncIdxOffset + conlwrrentDevices); gsyncIdx++)
            {
                const UINT32 display = m_GsyncDevices[gsyncIdx].GetDisplayMask();
                CHECK_RC(m_pDisplay->SetImage(display, (Surface2D*)NULL));
                CHECK_RC(m_GsyncDevices[gsyncIdx].NotifyImageChange());
            }
            CHECK_RC(m_pDisplay->DetachAllDisplays());

            if (!m_CalcCrc &&
                    m_pGolden->GetAction() == Goldelwalues::Store && patternsToCrc.empty())
                break;
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
//! \brief Cleanup the TegraGsyncLink
//!
//! \return OK if running was successful, not OK otherwise
//!
RC TegraGsyncLink::Cleanup()
{
    StickyRC rc;
    if (m_pDisplay != nullptr)
    {
        rc = TegraGsyncDevice::PowerToggleHDMI(m_pDisplay, 0);
        rc = m_pDisplay->Select(m_OrigDisplays);
        m_pDisplay = nullptr;
    }

    // Free golden buffer.
    m_pGolden->ClearSurfaces();
    for (size_t gsyncIdx = 0; gsyncIdx < m_pGGSurfs.size(); gsyncIdx++)
        m_pGGSurfs[gsyncIdx].ClearSurfaces();
    for (auto &pattern : m_SurfacePatterns)
    {
        for (auto &surfaceData : pattern.surfaceData)
        {
            surfaceData.second.surface.Free();
        }
        pattern.surfaceData.clear();
    }
    m_SurfacePatterns.clear();

    m_DisplaySors.clear();

    m_GsyncDevices.clear();

    rc = GpuTest::Cleanup();
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Initialize test properties from JS
//!
//! \return OK if successful, not OK otherwise
//!
RC TegraGsyncLink::InitFromJs()
{
    RC rc;

    // Call base-class function (for TestConfiguration and Golden properties).
    CHECK_RC(GpuTest::InitFromJs());

    // Initialize TestPatterns by getting the actual propery from JS
    // ratherthan by exporting a C++ JS property.  This is due to the nature of
    // how JS properties are copied onto the test object from test arguments
    // properties / arrays are copied individually rather than an entire
    // array assignment at once (what the C++ interface expects)
    JsArray jsaTestPatterns;
    JavaScriptPtr pJs;
    rc = pJs->GetProperty(GetJSObject(),
                          "TestPatterns",
                          &jsaTestPatterns);
    if (OK == rc)
    {
        for (size_t patIdx = 0; patIdx < jsaTestPatterns.size(); ++patIdx)
        {
            JsArray innerArray;
            rc = pJs->FromJsval(jsaTestPatterns[patIdx], &innerArray);
            if (OK == rc)
            {
                SurfacePattern surfPattern;
                for (size_t pix = 0; (pix < innerArray.size()) && (OK == rc);
                      ++pix)
                {
                    UINT32 lwrPix;
                    rc = pJs->FromJsval(innerArray[pix], &lwrPix);
                    if (OK == rc)
                    {
                        // Pixels are specified with just RGB values, force
                        // Alpha to be opaque
                        surfPattern.pattern.push_back(lwrPix | 0xFF000000);
                    }
                    else
                    {
                        Printf(Tee::PriError,
                               "%s : Unable to get custom pattern pixel\n",
                               GetName().c_str());
                    }
                }
                if (OK == rc)
                {
                    m_SurfacePatterns.push_back(surfPattern);
                }
            }
            else
            {
                Printf(Tee::PriError,
                       "%s : Unable to get custom pattern\n",
                       GetName().c_str());
            }
        }
    }
    else if (RC::ILWALID_OBJECT_PROPERTY == rc)
    {
        rc.Clear();

        if (m_IncludeRandom)
        {
            SurfacePattern surfPattern;
            surfPattern.random = true;
            m_SurfacePatterns.push_back(surfPattern);
        }

        const UINT32 numPatterns = sizeof(s_DefaultPatterns) /
                                   sizeof(s_DefaultPatterns[0]);
        const UINT32 numPixels = sizeof(s_DefaultPatterns[0]) / sizeof(UINT32);

        for (UINT32 lwrPtIdx = 0; lwrPtIdx < numPatterns; lwrPtIdx++)
        {
            SurfacePattern surfPattern;
            surfPattern.random = false;
            for (UINT32 lwrPix = 0; lwrPix < numPixels; lwrPix++)
            {
                surfPattern.pattern.push_back(s_DefaultPatterns[lwrPtIdx][lwrPix]);
            }
            m_SurfacePatterns.push_back(surfPattern);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void TegraGsyncLink::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tNumFrameCrcs:\t\t\t%u\n", m_NumFrameCrcs);
    Printf(pri, "\tCalcCrc:\t\t\t%s\n", m_CalcCrc ? "true" : "false");
    Printf(pri, "\tIncludeRandom:\t\t\t%s\n", m_IncludeRandom ? "true" : "false");
    Printf(pri, "\tTestResolution(wd:ht:rr):\t%s\n", m_TestResolution.c_str());
    Printf(pri, "\tTestPatterns:\t\t\t");
    for (size_t pat = 0; pat < m_SurfacePatterns.size(); pat++)
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
            for (size_t pix = 0; pix < m_SurfacePatterns[pat].pattern.size(); pix++)
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
RC TegraGsyncLink::CheckCallwlatedCrcs(UINT32 gsyncIdx, UINT32 patternIdx)
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

    for (size_t frameIdx = 0; frameIdx < suCrcs.size(); frameIdx++)
    {
        GsyncDevice::CrcData &suFrameCrcs  = suCrcs[frameIdx];
        GsyncDevice::CrcData &ppuFrameCrcs = ppuCrcs[frameIdx];

        for (size_t crcIdx = 0; crcIdx < suFrameCrcs.size(); crcIdx++)
        {
            if (suFrameCrcs[crcIdx] != ppuFrameCrcs[crcIdx])
            {
                Printf(Tee::PriError,
                       "!!! %s SORs:(%s), Display 0x%08x, Loop %d, Frame %u, CRC %u SU/PPU "
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
                       "!!! %s SORs:(%s), Display 0x%08x, Loop %d, Frame %u, CRC %u calc vs SU "
                       "mismatch! (SU CRC 0x%08x, calc CRC 0x%08x) pattern %u\n",
                       GetName().c_str(),
                       CreateSorString(m_DisplaySors).c_str(),
                       displayMask,
                       m_LoopIdx,
                       static_cast<UINT32>(frameIdx),
                       static_cast<UINT32>(crcIdx),
                       suFrameCrcs[crcIdx],
                       m_SurfacePatterns[patternIdx].surfaceData[surfaceRes].calcCrcs[crcIdx],
                       patternIdx);
                if (m_pGolden->GetStopOnError())
                    return RC::GOLDEN_VALUE_MISCOMPARE;
                else
                    m_CrcMiscompare = true;
            }
        }
    }
    return rc;
}

void TegraGsyncLink::CleanSORExcludeMask(vector<UINT32>* pDispsToRestore)
{
    StickyRC rc;
    Display *pDisplay = GetDisplay();
    if (pDispsToRestore->size() > 0)
    {
        rc = pDisplay->DetachAllDisplays();
        for (const auto &dispID : *pDispsToRestore)
        {
            rc = pDisplay->SetSORExcludeMask(dispID, 0);
        }
    }
}

RC TegraGsyncLink::DumpSurfaceInPng
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
// TegraGsyncLink object, properties and methods.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(TegraGsyncLink, GpuTest, "TegraGsyncLink Test");
CLASS_PROP_READWRITE(TegraGsyncLink, NumFrameCrcs, UINT32,
        "Number of frames to capture CRC for (default = 10)");
CLASS_PROP_READWRITE(TegraGsyncLink, CalcCrc, bool,
        "Set to true to enable CRC callwlations otherwise use goldens");
CLASS_PROP_READWRITE(TegraGsyncLink, IncludeRandom, bool,
        "Include random pattern in the default set");
CLASS_PROP_READWRITE(TegraGsyncLink, EnableXbarCombinations, bool,
        "Set to true to enable iterations over all SORs on supporting GPUs");
CLASS_PROP_READWRITE(TegraGsyncLink, RunParallel, bool,
        "Set to false to test each connected Gsync device sequentially.");
CLASS_PROP_READWRITE(TegraGsyncLink, DumpImages, bool,
        "Dump Images in F_<PatternIdx>.L_<LoopNum>.<Resolution>.png");
CLASS_PROP_READWRITE(TegraGsyncLink, TestResolution, std::string,
        "Test image resolution \"<Wd>:<Ht>:<refrate>\" or \"native\"(default)");
