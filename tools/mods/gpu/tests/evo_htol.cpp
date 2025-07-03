/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <stdio.h>
#include "gputest.h"
#include "core/include/golden.h"
#include "gpu/utility/surf2d.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "core/include/display.h"
#include "gpu/include/displaycleanup.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"
#include "gpu/utility/gpurectfill.h"
#include "ctrl/ctrl0073.h"
#include "class/cl507dcrcnotif.h"
#include "disp/v02_06/dev_disp.h"

#ifndef INCLUDED_STL_SET_H
#include <set>
#endif

#ifndef INCLUDED_STL_VECTOR_H
#include <vector>
#endif

#ifndef INCLUDED_STL_UTILITY_H
#include <utility> //pair
#endif

class EvoHtol : public GpuTest
{
private:
    GpuGoldenSurfaces*    m_pGGSurfs;
    GpuTestConfiguration* m_pTestConfig;
    Goldelwalues*         m_pGolden;

    UINT32 m_OrigDisplays;
    DisplayIDs m_DisplaysToTest;
    map<DisplayID,DisplayID> m_DualSSTPairs;

    Surface2D* m_pImage;

    struct
    {
        UINT32 Width;
        UINT32 Height;
        UINT32 Depth;
        UINT32 RefreshRate;
    } m_Mode;
    FLOAT64 m_TimeoutMs;

    UINT32 m_SkipDisplayMask;
    UINT32 m_RuntimeMs;
    bool m_KeepRunning;
    UINT32 m_DPDriveLwrrentLevel;
    UINT32 m_DPPreEmphasisLevel;

    RC ConfigureValidDisplays();
    RC AssignSORs();
    RC AssignSOR(UINT32 display, UINT32 sorIdx);
    RC CheckCrcs();

    RC InitProperties();

    RC GetNumSorConfigs(UINT32* numConfigs);

    // Cleanup Functions
    DisplayIDs m_CleanSORExcludeMaskIDs;
    RC CleanSORExcludeMask();

    DisplayIDs m_DisableSimulatedDPIDs;
    RC DisableSimulatedDP();

    DisplayIDs m_DisableSingleHeadMultistreamIDs;
    RC DisableSingleHeadMultistream();

public:
    EvoHtol();

    RC Setup();
    RC Run();
    RC Cleanup();

    virtual bool IsSupported();

    SETGET_PROP(SkipDisplayMask, UINT32);
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(DPDriveLwrrentLevel, UINT32);
    SETGET_PROP(DPPreEmphasisLevel, UINT32);

};

EvoHtol::EvoHtol()
: m_OrigDisplays(0)
, m_TimeoutMs(1000)
, m_SkipDisplayMask(0)
, m_RuntimeMs(1000)
, m_KeepRunning(false)
, m_DPDriveLwrrentLevel(LW0073_CTRL_DP_LANE_DATA_DRIVELWRRENT_LEVEL3)
, m_DPPreEmphasisLevel(LW0073_CTRL_DP_LANE_DATA_PREEMPHASIS_NONE)
{
    SetName("EvoHtol");
    m_pGGSurfs = NULL;
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
    m_pImage = NULL;
}

JS_CLASS_INHERIT(EvoHtol, GpuTest,
                 "Evo Display Htol test.");

CLASS_PROP_READWRITE(EvoHtol, SkipDisplayMask, UINT32,
                     "Do not run EvoHtol on these display masks.");
CLASS_PROP_READWRITE(EvoHtol, RuntimeMs, UINT32,
                     "How long in milliseconds to run the test.");
CLASS_PROP_READWRITE(EvoHtol, KeepRunning, bool,
                     "Run the test indefinitely. For use as a background test.");
CLASS_PROP_READWRITE(EvoHtol, DPDriveLwrrentLevel, UINT32,
                     "DP lane drive current level to use.");
CLASS_PROP_READWRITE(EvoHtol, DPPreEmphasisLevel, UINT32,
                     "DP lane pre-emphasis level to use.");

//------------------------------------------------------------------------------
bool EvoHtol::IsSupported()
{
    if (GetDisplay()->GetDisplayClassFamily() != Display::EVO)
    {
        // Need an EVO display.
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
RC EvoHtol::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    CHECK_RC(GpuTest::AllocDisplay());

    CHECK_RC(InitProperties());

    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());

    m_pImage = new Surface2D();
    m_pImage->SetWidth(m_Mode.Width);
    m_pImage->SetHeight(m_Mode.Height);
    m_pImage->SetColorFormat(ColorUtils::A8R8G8B8);
    m_pImage->SetType(LWOS32_TYPE_PRIMARY);
    m_pImage->SetLocation(Memory::Optimal);
    m_pImage->SetDisplayable(true);
    rc = m_pImage->Alloc(GetBoundGpuDevice());
    if (rc != OK)
    {
        delete m_pImage;
        return rc;
    }

    CHECK_RC(m_pImage->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    Memory::FillRandom(m_pImage->GetAddress(),
                       m_pTestConfig->Seed(),
                       m_pImage->GetSize());
    m_pImage->Unmap();

    m_OrigDisplays = GetDisplay()->Selected();

    return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::InitProperties()
{
    RC rc;

    m_Mode.Width       = m_pTestConfig->DisplayWidth();
    m_Mode.Height      = m_pTestConfig->DisplayHeight();
    m_Mode.Depth       = m_pTestConfig->DisplayDepth();
    m_Mode.RefreshRate = m_pTestConfig->RefreshRate();
    m_TimeoutMs        = m_pTestConfig->TimeoutMs();

    return rc;
}

namespace
{
    UINT32 CalcEvoPclk
    (
        UINT32 Width,
        UINT32 Height,
        UINT32 RefreshRate
    )
    {
        return ((Width+200)*(Height+150)*RefreshRate);
    }

    void CreateRaster
    (
        UINT32 ScaledWidth,
        UINT32 ScaledHeight,
        UINT32 StressModeWidth,
        UINT32 StressModeHeight,
        UINT32 RefreshRate,
        UINT32 LaneCount,
        UINT32 LinkRate,
        EvoRasterSettings *ers
    )
    {
        // Values in the raster were determined by experiments so they work
        // both for DACs and SORs in various modes.
        // They should also have plenty of vblank margin across the full
        // range of pclks used, so that there are no problems callwlating
        // the DMI duration value.
        *ers = EvoRasterSettings(ScaledWidth+200,  101, ScaledWidth+100+49, 149,
                                 ScaledHeight+150,  50, ScaledHeight+50+50, 100,
                                 1, 0,
                                 CalcEvoPclk(StressModeWidth,
                                             StressModeHeight,
                                             RefreshRate),
                                 LaneCount, LinkRate,
                                 false);
    }
}

//------------------------------------------------------------------------------
RC EvoHtol::Run()
{
    RC rc;

    Display* pDisplay = GetDisplay();

    CHECK_RC(pDisplay->DetachAllDisplays());

    CHECK_RC(ConfigureValidDisplays());

    UINT32 displaysToTest = 0x0;
    for (DisplayIDs::const_iterator it = m_DisplaysToTest.begin();
         it != m_DisplaysToTest.end(); it++)
    {
        displaysToTest |= it->Get();
    }
    VerbosePrintf("DisplaysToTest = 0x%x\n", displaysToTest);
    CHECK_RC(pDisplay->Select(displaysToTest));

    pDisplay->SetPendingSetModeChange();
    CHECK_RC(pDisplay->SetMode(m_Mode.Width,
                               m_Mode.Height,
                               m_Mode.Depth,
                               m_Mode.RefreshRate));
    CHECK_RC(pDisplay->SetImage(m_pImage));

    UINT64 startTime = Platform::GetTimeMS();
    for (UINT64 lwrTime = startTime;
        m_KeepRunning || (lwrTime - startTime) < m_RuntimeMs;
        lwrTime = Platform::GetTimeMS())
    {
        VerbosePrintf("Starting display CRC engines\n");
        for (DisplayIDs::iterator dispIt = m_DisplaysToTest.begin();
             dispIt != m_DisplaysToTest.end(); dispIt++)
        {
            CHECK_RC(pDisplay->StartRunningCrc(dispIt->Get(), Display::CRC_DURING_SNOOZE_ENABLE));
        }

        Tasker::Yield();

        VerbosePrintf("Stopping display CRC engines\n");
        for (DisplayIDs::iterator dispIt = m_DisplaysToTest.begin();
             dispIt != m_DisplaysToTest.end(); dispIt++)
        {
            CHECK_RC(pDisplay->StopRunningCrc(dispIt->Get()));
        }

        CHECK_RC(CheckCrcs());

        if (m_pGolden->GetAction() == Goldelwalues::Store)
            break;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::ConfigureValidDisplays()
{
    RC rc;
    Display* pDisplay = GetDisplay();

    m_DisplaysToTest.clear();

    DisplayIDs dpConnectors;
    DisplayIDs nonDpConnectors;
    DisplayIDs allConnectors;

    set<char> dpLinks;
    map<UINT32,DisplayID> padToDisplayId;

    CHECK_RC(pDisplay->GetConnectors(&allConnectors));

    // Find all of the connectors and record the DP connectors' links and pad indices
    for (DisplayIDs::iterator dispIt = allConnectors.begin();
         dispIt != allConnectors.end(); dispIt++)
    {
        UINT32 connector = dispIt->Get();
        VerbosePrintf("Display 0x%08x\n", connector);

        if (connector & m_SkipDisplayMask)
        {
            VerbosePrintf("\tSkipping display 0x%x due to user override\n",
                          connector);
            continue;
        }

        Display::DisplayType displayType = pDisplay->GetDisplayType(connector);
        if (displayType == Display::DFP)
        {
            Display::Encoder encoder;
            CHECK_RC(pDisplay->GetEncoder(connector, &encoder));

            if (encoder.ORType != LW0073_CTRL_SPECIFIC_OR_TYPE_SOR)
            {
                // We don't test PIORs here
                continue;
            }

            string linkString = Display::Encoder::GetLinksString(encoder.ActiveLink);

            if (encoder.Signal == Display::Encoder::DP)
            {
                dpConnectors.push_back(connector);
                for (UINT32 i = 0; i < linkString.size(); i++)
                    dpLinks.insert(linkString[i]);

                if (!encoder.EmbeddedDP || encoder.Link != Display::Encoder::SINGLE)
                {
                    VerbosePrintf("\tDual DP\n");

                    LW0073_CTRL_DFP_GET_PADLINK_MASK_PARAMS getPadlinkMaskParams = {0};
                    getPadlinkMaskParams.subDeviceInstance =
                        GetBoundGpuSubdevice()->GetSubdeviceInst();
                    getPadlinkMaskParams.displayId = connector;
                    CHECK_RC(GetDisplay()->RmControl(LW0073_CTRL_CMD_DFP_GET_PADLINK_MASK,
                        &getPadlinkMaskParams, sizeof(getPadlinkMaskParams)));

                    if (Utility::CountBits(getPadlinkMaskParams.padlinkMask) != 1)
                        return RC::SOFTWARE_ERROR;

                    UINT32 padIdx =
                        Utility::BitScanForward(getPadlinkMaskParams.padlinkMask);

                    padToDisplayId[padIdx] = connector;
                }
                else
                    VerbosePrintf("\tDP\n");
            }
            else if (encoder.Signal == Display::Encoder::TMDS)
            {
                nonDpConnectors.push_back(connector);
                if (encoder.Link == Display::Encoder::DUAL)
                {
                    VerbosePrintf("\tDual TMDS\n");
                }
                else
                    VerbosePrintf("\tTMDS\n");
            }
            else if (encoder.Signal == Display::Encoder::LVDS)
            {
                nonDpConnectors.push_back(connector);
                if (encoder.Link == Display::Encoder::DUAL)
                {
                    VerbosePrintf("\tDual LVDS\n");
                }
                else
                    VerbosePrintf("\tLVDS\n");
            }
            VerbosePrintf("\tLinks: %s\n", linkString.c_str());
        }
    }

    // Match up DualSST pairs by their pad indices
    for (map<UINT32,DisplayID>::const_iterator padIter = padToDisplayId.begin();
         padIter != padToDisplayId.end();
         padIter++)
    {
        UINT32 pairPadIdx = padIter->first ^ 1;
        if (padToDisplayId.find(pairPadIdx) != padToDisplayId.end())
        {
            m_DualSSTPairs[padIter->second] = padToDisplayId[pairPadIdx];
        }
    }

    // Populate a list of DP connectors (with their DualSST pair if it exists)
    list<pair<DisplayID,DisplayID> > dpPairs;
    UINT32 dpConnectorsMask = 0x0;
    for (UINT32 i = 0; i < dpConnectors.size(); i++)
        dpConnectorsMask |= dpConnectors[i];
    while (dpConnectorsMask)
    {
        UINT32 display = dpConnectorsMask & ~(dpConnectorsMask - 1);
        dpConnectorsMask ^= display;

        UINT32 secondaryDisplay = Display::NULL_DISPLAY_ID;
        if (m_DualSSTPairs.find(display) != m_DualSSTPairs.end())
        {
            secondaryDisplay = m_DualSSTPairs[display];
            dpConnectorsMask &= ~secondaryDisplay;
        }

        pair<DisplayID,DisplayID> newPair(display, secondaryDisplay);
        dpPairs.push_back(newPair);
    }

    // Weed out all the TMDS and LVDS connectors that share a link with a DP connector
    for (DisplayIDs::iterator it = nonDpConnectors.begin(); it != nonDpConnectors.end(); )
    {
        Display::Encoder encoder;
        CHECK_RC(pDisplay->GetEncoder(it->Get(), &encoder));
        string linkString = Display::Encoder::GetLinksString(encoder.ActiveLink);
        bool erase = false;
        for (UINT32 i = 0; i < linkString.size(); i++)
        {
            if (dpLinks.find(linkString[i]) != dpLinks.end())
            {
                erase = true;
                break;
            }
        }
        if (erase)
            it = nonDpConnectors.erase(it);
        else
            it++;
    }

    UINT32 numSorConfigs = 0;
    CHECK_RC(GetNumSorConfigs(&numSorConfigs));

    // Break apart DualSST pairs until all of the SORs can be covered
    for (list<pair<DisplayID,DisplayID> >::iterator it = dpPairs.begin();
         dpPairs.size() + nonDpConnectors.size() < numSorConfigs
             && it != dpPairs.end(); )
    {
        if (it->second != Display::NULL_DISPLAY_ID)
        {
            pair<DisplayID,DisplayID> newPair1(it->first, Display::NULL_DISPLAY_ID);
            pair<DisplayID,DisplayID> newPair2(it->second, Display::NULL_DISPLAY_ID);
            map<DisplayID,DisplayID>::iterator itt = m_DualSSTPairs.find(it->first);
            if (itt != m_DualSSTPairs.end())
                m_DualSSTPairs.erase(itt);
            itt = m_DualSSTPairs.find(it->second);
            if (itt != m_DualSSTPairs.end())
                m_DualSSTPairs.erase(itt);
            dpPairs.push_back(newPair1);
            dpPairs.push_back(newPair2);
            it = dpPairs.erase(it);
            continue;
        }
        it++;
    }

    if (dpPairs.size() + nonDpConnectors.size() < numSorConfigs)
    {
        Printf(Tee::PriNormal, "Warning: Too few connectors (%u) for number of SORs (%u)",
               static_cast<UINT32>(dpPairs.size() + nonDpConnectors.size()), numSorConfigs);
    }

    VerbosePrintf("Running with Displays:\n");
    // Setup DP simulations and multistream mode
    for (list<pair<DisplayID,DisplayID> >::iterator it = dpPairs.begin();
         it != dpPairs.end(); it++)
    {
        UINT32 primary = it->first;
        UINT32 secondary = it->second;
        UINT32 simulatedDisplayIDsMask = 0x0;
        CHECK_RC(pDisplay->EnableSimulatedDPDevices(
            primary, secondary, Display::DPMultistreamDisabled, 1,
            nullptr, 0, &simulatedDisplayIDsMask));
        m_DisableSimulatedDPIDs.push_back(primary);

        if (secondary != Display::NULL_DISPLAY_ID)
        {
            CHECK_RC(pDisplay->SetSingleHeadMultiStreamMode(
                    primary, secondary, Display::SHMultiStreamSST));
            m_DisableSingleHeadMultistreamIDs.push_back(simulatedDisplayIDsMask);
            VerbosePrintf("\tDualSST 0x%08x + 0x%08x\n", primary, secondary);
        }
        else
        {
            VerbosePrintf("\tSimDP 0x%08x\n", simulatedDisplayIDsMask);
        }

        m_DisplaysToTest.push_back(simulatedDisplayIDsMask);
    }

    // Supplement the DP connectors with TMDS or LVDS connectors
    for (UINT32 i = 0; i < nonDpConnectors.size() && m_DisplaysToTest.size() < numSorConfigs; i++)
    {
        m_DisplaysToTest.push_back(nonDpConnectors[i]);
        VerbosePrintf("\t0x%08x\n", nonDpConnectors[i].Get());
    }

    CHECK_RC(AssignSORs());

    CHECK_RC(pDisplay->SetTimings(NULL));
    vector<UINT32> displayVector;
    vector<EvoRasterSettings*> pEvoRSVector;
    vector<EvoRasterSettings> ers(m_DisplaysToTest.size());
    vector<Display::MaxPclkQueryInput> pclkQueryVector(m_DisplaysToTest.size());
    for (UINT32 headIndex = 0; headIndex <  m_DisplaysToTest.size(); ++headIndex)
    {
        CreateRaster(m_Mode.Width, m_Mode.Height,
                     m_Mode.Width, m_Mode.Height,
                     m_Mode.RefreshRate, 0, 0, &(ers[headIndex]));
        pclkQueryVector[headIndex].displayID = m_DisplaysToTest[headIndex].Get();
        pclkQueryVector[headIndex].ersMaxPclk = &(ers[headIndex]);
    }
    CHECK_RC(pDisplay->SetRasterForMaxPclkPossible(&pclkQueryVector,
                                                    0));
    for (UINT32 headIndex = 0; headIndex <  m_DisplaysToTest.size(); ++headIndex)
    {
        VerbosePrintf("Detected max pclk = %d on 0x%08x\n",
                      ers[headIndex].PixelClockHz, m_DisplaysToTest[headIndex].Get());

        CHECK_RC(pDisplay->AddTimings(m_DisplaysToTest[headIndex].Get(), &(ers[headIndex])));
    }
    // Request a lane drive current / preemphasis level for DP since link training is skipped
    for (DisplayIDs::iterator it = dpConnectors.begin();
         it != dpConnectors.end(); it++)
    {
        LW0073_CTRL_DP_LANE_DATA_PARAMS dpLaneDataParams = {0};
        dpLaneDataParams.subDeviceInstance =
            GetBoundGpuSubdevice()->GetSubdeviceInst();
        dpLaneDataParams.displayId = it->Get();
        dpLaneDataParams.numLanes = 4;
        for ( UINT32 laneIdx = 0; laneIdx < dpLaneDataParams.numLanes; laneIdx++ )
        {
            dpLaneDataParams.data[laneIdx] =
                  DRF_NUM(0073, _CTRL_DP_LANE_DATA, _PREEMPHASIS, m_DPPreEmphasisLevel)
                | DRF_NUM(0073, _CTRL_DP_LANE_DATA, _DRIVELWRRENT, m_DPDriveLwrrentLevel);
        }

        CHECK_RC(GetDisplay()->RmControl(LW0073_CTRL_CMD_DP_SET_LANE_DATA,
                            &dpLaneDataParams, sizeof(dpLaneDataParams)));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::AssignSORs()
{
    RC rc;
    Display* pDisplay = GetDisplay();
    VerbosePrintf("Assigning SORs\n");

    UINT32 numSorConfigs = 0;
    CHECK_RC(GetNumSorConfigs(&numSorConfigs));

    UINT32 assignedSORs = 0x0;

    // Look for an LVDS connector. It needs to be assigned to SOR0.
    // Assumes there will be at most 1 LVDS connector
    for (DisplayIDs::iterator dispIt = m_DisplaysToTest.begin();
         dispIt != m_DisplaysToTest.end(); dispIt++)
    {
        Display::Encoder encoder;
        CHECK_RC(pDisplay->GetEncoder(dispIt->Get(), &encoder));
        if (encoder.Signal == Display::Encoder::LVDS)
        {
            // Reserve SOR0 for the LVDS connector
            assignedSORs |= 0x1;
            break;
        }
    }

    for (DisplayIDs::iterator dispIt = m_DisplaysToTest.begin();
         dispIt != m_DisplaysToTest.end(); dispIt++)
    {
        const UINT32 display = dispIt->Get();
        Display::Encoder encoder;
        CHECK_RC(pDisplay->GetEncoder(display, &encoder));
        if (encoder.Signal == Display::Encoder::LVDS)
        {
            // Mark SOR0 as unassigned so that it can now be actually assigned to the LVDS connector
            assignedSORs &= ~0x1;
            break;
        }

        for (UINT32 assignedSORIdx = 0;
             assignedSORIdx < numSorConfigs;
             assignedSORIdx++)
        {
            if ((1 << assignedSORIdx) & assignedSORs)
                continue;

            RC assignRc = AssignSOR(display, assignedSORIdx);

            if (assignRc == RC::LWRM_NOT_SUPPORTED)
                continue;
            CHECK_RC(assignRc);

            assignedSORs |= (1 << assignedSORIdx);
            VerbosePrintf("\tDisplay 0x%08x : SOR %d\n", display, assignedSORIdx);

            if (m_DualSSTPairs.find(*dispIt) != m_DualSSTPairs.end())
            {
                DisplayID secondary = m_DualSSTPairs[*dispIt];
                if (OK != (rc = AssignSOR(secondary.Get(), assignedSORIdx)))
                {
                    Printf(Tee::PriHigh, "Could not assign secondary display 0x%08x to same SOR %d as primary display 0x%08x\n",
                           secondary.Get(), assignedSORIdx, display);
                    return rc;
                }
                VerbosePrintf("\tDisplay 0x%08x : SOR %d\n", secondary.Get(), assignedSORIdx);
            }

            break;
        }
    }

    if (static_cast<UINT32>(Utility::CountBits(assignedSORs)) != m_DisplaysToTest.size())
    {
        Printf(Tee::PriHigh, "ERROR: Unable to assign an SOR to each display.\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC EvoHtol::AssignSOR(UINT32 display, UINT32 sorIdx)
{
    RC rc;
    Display* pDisplay = GetDisplay();

    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
    {
        CHECK_RC(pDisplay->DetachAllDisplays());
        UINT32 excludeMask = ~(1 << sorIdx);
        vector<UINT32> sorList;
        // Call this function just to check if the assignment is
        // possible. The SOR will be assigned again in SetupXBar.
        CHECK_RC(pDisplay->AssignSOR(DisplayIDs(1,display),
                                     excludeMask, sorList,
                                     Display::AssignSorSetting::ONE_HEAD_MODE));

        m_CleanSORExcludeMaskIDs.push_back(display);
        // Need to call SetSORExcludeMask here as GetORProtocol calls
        // SetupXBar now.
        CHECK_RC(pDisplay->SetSORExcludeMask(display, excludeMask));

        UINT32 sorNumber = 0xBAD;
        CHECK_RC(pDisplay->GetORProtocol(display,
                                         &sorNumber, nullptr, nullptr));

        if (sorNumber != sorIdx)
        {
            Printf(Tee::PriHigh,
                "ERROR: GetORProtocol returned unexpected SOR%d,"
                " when SOR%d was expected.\n",
                sorNumber, sorIdx);
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

RC EvoHtol::GetNumSorConfigs(UINT32* numConfigs)
{
    MASSERT(numConfigs);
    RC rc;

    Display::SplitSorConfig sorConfig;
    CHECK_RC(GetDisplay()->GetSupportedSplitSorConfigs(&sorConfig));
    bool testSplitSors = ((sorConfig.size() != 0));
    bool testXbarCombinations =
        GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR);
    UINT32 numSorConfigurations =
        (testSplitSors ?
            static_cast<UINT32>(sorConfig.size()) :
            (testXbarCombinations ?
                LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS :
                1));

     *numConfigs = numSorConfigurations;
     return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::CheckCrcs()
{
    RC rc;
    Display *pDisplay = GetDisplay();

    for (DisplayIDs::iterator dispIt = m_DisplaysToTest.begin();
         dispIt != m_DisplaysToTest.end(); dispIt++)
    {
        m_pGGSurfs->ClearSurfaces();
        m_pGolden->ClearSurfaces();

        Surface2D crcSurface;
        crcSurface.SetWidth(1);
        crcSurface.SetHeight(1);
        crcSurface.SetColorFormat(ColorUtils::VOID32);
        CHECK_RC(crcSurface.Alloc(GetBoundGpuDevice()));
        m_pGGSurfs->AttachSurface(&crcSurface, "C", dispIt->Get());
        CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));

        UINT32 *crcBuf = (UINT32 *)pDisplay->GetCrcBuffer(dispIt->Get());
        if (!crcBuf)
        {
            Printf(Tee::PriHigh, "ERROR: Null CRC buffer!\n");
            return RC::SOFTWARE_ERROR;
        }

        UINT32 numCrcs = DRF_VAL(507D, _NOTIFIER_CRC_1, _STATUS_0_COUNT, MEM_RD32(crcBuf));
        VerbosePrintf("Num CRCs on display 0x%08x is %d\n", dispIt->Get(), numCrcs);

        UINT32 goldenCrc = MEM_RD32(crcBuf + LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_4);
        for (UINT32 i = 0, crcOffset = LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2;
             i < numCrcs;
             ++i, crcOffset += (LW507D_NOTIFIER_CRC_1_CRC_ENTRY1_6
                                - LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2))
        {
            UINT32 *primaryCrcAddr = (crcBuf + crcOffset
                                      + LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_4
                                      - LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2);
            UINT32 lwrCrc = MEM_RD32(primaryCrcAddr);
            VerbosePrintf("\tRead primary CRC as 0x%08x in frame %d\n", lwrCrc, i);

            if (lwrCrc != goldenCrc)
            {
                Printf(Tee::PriHigh, "ERROR: Crc Miscompare: display 0x%08x, frame %d, 0x%08x != 0x%08x\n",
                       dispIt->Get(), i, lwrCrc, goldenCrc);
                return RC::GOLDEN_VALUE_MISCOMPARE;
            }

            if (i == numCrcs-1 || m_pGolden->GetAction() == Goldelwalues::Store)
            {
                // Only crc check the last frame because crc checking messes with the crc buffer
                string crcSignature;
                CHECK_RC(pDisplay->GetCrcSignature(dispIt->Get(), Display::DAC_CRC, &crcSignature));
                CHECK_RC(m_pGolden->OverrideSuffix(crcSignature.c_str()));
                m_pGolden->SetLoop(0);
                CHECK_RC(crcSurface.LoadPitchImage(reinterpret_cast<UINT08*>(&lwrCrc),
                                                   GetBoundGpuSubdevice()->GetSubdeviceInst()));
                CHECK_RC(m_pGolden->Run());
            }

            if (m_pGolden->GetAction() == Goldelwalues::Store)
                break;
        }

        CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));

        m_pGGSurfs->ClearSurfaces();
        m_pGolden->ClearSurfaces();
    }

    return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::Cleanup()
{
    StickyRC rc;
    Display* pDisplay = GetDisplay();

    rc = pDisplay->SetImage((Surface2D*)NULL);
    rc = DisableSingleHeadMultistream();
    rc = CleanSORExcludeMask();
    rc = pDisplay->DetachAllDisplays();
    rc = DisableSimulatedDP();
    rc = pDisplay->Select(m_OrigDisplays);
    rc = pDisplay->SetTimings((void*)NULL);

    rc = GpuTest::Cleanup();

    if (m_pImage)
    {
        m_pImage->Free();
        delete m_pImage;
        m_pImage = NULL;
    }

    if (m_pGGSurfs)
    {
        m_pGGSurfs->ClearSurfaces();
        delete m_pGGSurfs;
        m_pGGSurfs = NULL;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::CleanSORExcludeMask()
{
    RC rc;
    if (m_CleanSORExcludeMaskIDs.size() > 0)
    {
        Display *pDisplay = GetDisplay();
        CHECK_RC(pDisplay->DetachAllDisplays());

        for (DisplayIDs::iterator it = m_CleanSORExcludeMaskIDs.begin();
             it != m_CleanSORExcludeMaskIDs.end(); it++)
        {
            CHECK_RC(pDisplay->SetSORExcludeMask(it->Get(), 0));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::DisableSimulatedDP()
{
    RC rc;
    if (m_DisableSimulatedDPIDs.size() > 0)
    {
        Display *pDisplay = GetDisplay();
        CHECK_RC(pDisplay->DetachAllDisplays());

        for (DisplayIDs::iterator it = m_DisableSimulatedDPIDs.begin();
             it != m_DisableSimulatedDPIDs.end(); it++)
        {
            UINT32 secondaryDisplay = Display::NULL_DISPLAY_ID;
            if (m_DualSSTPairs.find(*it) != m_DualSSTPairs.end())
                secondaryDisplay = m_DualSSTPairs[*it];
            CHECK_RC(pDisplay->DisableSimulatedDPDevices(it->Get(),
                     secondaryDisplay, Display::SkipRealDisplayDetection));
        }
        m_DisableSimulatedDPIDs.clear();
        CHECK_RC(pDisplay->DetectRealDisplaysOnAllConnectors());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC EvoHtol::DisableSingleHeadMultistream()
{
    RC rc;
    if (m_DisableSingleHeadMultistreamIDs.size() > 0)
    {
        Display *pDisplay = GetDisplay();

        for (DisplayIDs::iterator it = m_DisableSingleHeadMultistreamIDs.begin();
             it != m_DisableSingleHeadMultistreamIDs.end(); it++)
        {
            CHECK_RC(pDisplay->SetSingleHeadMultiStreamMode(it->Get(), 0,
                                                   Display::SHMultiStreamDisabled));
        }
        m_DisableSingleHeadMultistreamIDs.clear();
    }
    return rc;
}
