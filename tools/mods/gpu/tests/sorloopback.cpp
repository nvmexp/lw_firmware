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

// Display SOR loopback test.
// Based on //arch/traces/non3d/iso/disp/class010X/core/disp_core_sor_loopback_vaio/test.js
//          //arch/traces/ip/display/3.0/branch_trace/full_chip/sor_loopback/src/2.0/test.cpp
#include "sorloopback.h"
#include "core/include/modsedid.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/jscript.h"
#include "Lwcm.h"
#include "core/include/utility.h"
#include "gpu/include/displaycleanup.h"
#include "gpu/utility/scopereg.h"

#include "class/cl8870.h"
#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9770.h"
#include "class/cl507d.h" // Core channel
#include "class/cl907d.h" // Core channel
#include "class/clc570.h"

#include "gpu/display/evo_disp.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/lwdisplay/lw_disp_crc_handler_c3.h"

//------------------------------------------------------------------------------
// DisplaySor
//------------------------------------------------------------------------------
DisplaySor::DisplaySor(GpuSubdevice *pSubdev, Display *pDisplay)
: m_pSubdev(pSubdev)
, m_pDisplay(pDisplay)
{
}

RC DisplaySor::FillRasterSettings
(
    RasterSettings *pRasterSettings,
    UINT32 hActive,
    UINT32 vActive,
    UINT32 pclk
)
{
    // Based on raster used in:
    // //arch/traces/non3d/iso/disp/class011X/core/disp_core_sor_loopback_vaio/test.js

    const UINT32 hSync = 16;
    const UINT32 hBackPorch = 6;
    const UINT32 hFrontPorch = 4;
    const UINT32 vSync = 1;
    const UINT32 vBackPorch = 28;
    const UINT32 vFrontPorch = 1;

    UINT32 hBlankingPeriod = hFrontPorch + hSync + hBackPorch;
    UINT32 vBlankingPeriod = vFrontPorch + vSync + vBackPorch;

    pRasterSettings->RasterWidth = hActive + hBlankingPeriod;
    pRasterSettings->ActiveX = hActive;
    pRasterSettings->SyncEndX = hSync - 1;
    pRasterSettings->BlankStartX = pRasterSettings->RasterWidth - hFrontPorch - 1;
    pRasterSettings->BlankEndX = hSync - 1 + hBackPorch;

    pRasterSettings->RasterHeight = vActive + vBlankingPeriod;
    pRasterSettings->ActiveY = vActive;
    pRasterSettings->SyncEndY = vSync - 1;
    pRasterSettings->BlankStartY = pRasterSettings->RasterHeight - vFrontPorch - 1;
    pRasterSettings->BlankEndY = vSync - 1 + vBackPorch;

    pRasterSettings->Interlaced = false;
    pRasterSettings->PixelClockHz = pclk;

    return OK;
}

RC DisplaySor::Setup
(
    Tee::Priority printPriority,
    Display::Mode cleanupMode
)
{
    m_VerbosePrintPriority = printPriority;
    m_pDisplay->SetBlockHotPlugEvents(true);
    m_OriginalIMPPrintPriority = m_pDisplay->SetIsModePossiblePriority(printPriority);
    m_CleanupMode = cleanupMode;

    return OK;
}

RC DisplaySor::Cleanup()
{
    m_pDisplay->SetBlockHotPlugEvents(false);
    m_pDisplay->SetIsModePossiblePriority(m_OriginalIMPPrintPriority);

    return OK;
}

void DisplaySor::GetTMDSLinkSupport
(
    DisplayID displayID,
    bool *pLinkA,
    bool *pLinkB
)
{
    *pLinkA = true;
    *pLinkB = true;
}

RC DisplaySor::AssignSOR
(
    DisplayID displayID,
    UINT32 requestedSor,
    UINT32 *pAssignedSor,
    UINT32 *pOrType,
    UINT32 *pProtocol
)
{
    RC rc;

    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
    {
        UINT32 excludeMask = ~(1 << requestedSor);
        SorInfoList assignSorListWithTag;

        // Call this function just to check if the assignment is
        // possible. The SOR will be assigned again in SetupXBar.
        CHECK_RC(m_pDisplay->AssignSOR(DisplayIDs(1, displayID),
            excludeMask, &assignSorListWithTag, Display::AssignSorSetting::ONE_HEAD_MODE));

        // Need to call SetSORExcludeMask here as GetORProtocol calls SetupXBar now.
        CHECK_RC(m_pDisplay->SetSORExcludeMask(displayID, excludeMask));

        CHECK_RC(m_pDisplay->GetORProtocol(displayID, pAssignedSor, pOrType, pProtocol));

        if (*pAssignedSor != requestedSor)
        {
            Printf(Tee::PriError, "SorLoopback: GetORProtocol returned unexpected SOR%d,"
                    " when SOR%d was expected.\n", *pAssignedSor, requestedSor);
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        CHECK_RC(m_pDisplay->GetORProtocol(displayID, pAssignedSor, pOrType, pProtocol));
    }

    return rc;
}

RC DisplaySor::RegisterCallbackArgs(LoopbackTester* pLoopbacktester)
{
    return RC::OK;
}

//------------------------------------------------------------------------------
// LwDisplaySor
//------------------------------------------------------------------------------
LwDisplaySor::LwDisplaySor(GpuSubdevice *pSubdev, Display *pDisplay)
: DisplaySor(pSubdev, pDisplay)
{
}

RC LwDisplaySor::Setup
(
    Tee::Priority printPriority,
    Display::Mode cleanupMode
)
{
    RC rc;

    CHECK_RC(DisplaySor::Setup(printPriority, cleanupMode));

    m_pLwDisplay = static_cast<LwDisplay *>(m_pDisplay);
    LwDispChnContext *pLwDispChnContext;
    CHECK_RC(m_pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
                &pLwDispChnContext,
                LwDisplay::ILWALID_WINDOW_INSTANCE,
                Display::ILWALID_HEAD));

    m_pCoreContext = static_cast<LwDispCoreChnContext *>(pLwDispChnContext);
    return rc;
}

RC LwDisplaySor::Cleanup()
{
    return m_pLwDisplay->ResetEntireState();
}

bool LwDisplaySor::IsLinkUPhy(Display::Encoder::Links link)
{
    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_USB) &&
        link.Secondary == Display::Encoder::LINK_F)
    {
        return true;
    }

    return false;
}

RC LwDisplaySor::PerformPassingSetMode
(
    DisplayID displayID,
    Display::Mode mode,
    UINT32 assignedSor,
    UINT32 protocol,
    UINT32 requestedPixelClockHz,
    UINT32 *pPassingPixelClockHz
)
{
    RC rc;
    Display::UpdateMode originalUpdateMode = m_pDisplay->GetUpdateMode();
    m_pDisplay->SetUpdateMode(Display::ManualUpdate);

    DEFER
    {
        m_pDisplay->SetUpdateMode(originalUpdateMode);
    };

    *pPassingPixelClockHz = requestedPixelClockHz;

    HeadIndex head = Display::ILWALID_HEAD;
    CHECK_RC(m_pDisplay->GetHead(displayID, &head));

    LwDispHeadSettings *pHeadSettings = &m_pCoreContext->DispLwstomSettings.HeadSettings[head];

    if (m_FrlMode)
    {
        pHeadSettings->pDispPanelMgr.reset(new LwDispHDMIPanelMgr
                (m_pLwDisplay, displayID, head, LwDispPanelMgr::LWD_PANEL_TYPE_HDMI_FRL,
                 true, true));
        pHeadSettings->pDispPanelMgr->dirty = true;
    }

    // Fill Raster
    pHeadSettings->rasterSettings.Dirty = true;
    CHECK_RC(FillRasterSettings(&pHeadSettings->rasterSettings, mode.width,
        mode.height, requestedPixelClockHz));

    // Fill PClk
    pHeadSettings->pixelClkSettings.dirty = true;
    pHeadSettings->pixelClkSettings.pixelClkFreqHz = requestedPixelClockHz;

    // Fill OR Settings
    LwDispORSettings *pORSettings = &(m_pCoreContext->DispLwstomSettings.SorSettings[assignedSor]);
    pORSettings->Dirty = true;
    pORSettings->ORNumber = assignedSor;
    pORSettings->protocol = m_FrlMode ? LwDispSORSettings::Protocol::PROTOCOL_HDMI_FRL : protocol;
    pORSettings->DevInfoProtocol = 0;
    pORSettings->OwnerMask = 1 << head;
    pORSettings->pixelDepth = LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444;
    pHeadSettings->pORSettings = pORSettings;

    CHECK_RC(SetupOLut(head, pHeadSettings));

    CHECK_RC(m_pLwDisplay->SelectModeSetDefaults(displayID, head, mode.width,
        mode.height, requestedPixelClockHz));

    LwDisplay::IsModePossibleOutput isModePossibleOutput;
    CHECK_RC(m_pLwDisplay->IsModePossibleFromLwstomSettings((1 << head), &isModePossibleOutput));
    if (!isModePossibleOutput.isModePossible)
    {
        CHECK_RC(UpdatePixelClockForModePossible(head, mode, pHeadSettings));
    }

    DisplayIDs displays(1, displayID);
    vector<UINT32> heads(1, head);
    if (m_MaximizePclk)
    {
        m_pLwDisplay->SetMaximizePclkHeads(heads);
    }

    CHECK_RC(m_pLwDisplay->SetModeWithLwstomSettings(displays, heads));

    // Update after pclk is maximized
    *pPassingPixelClockHz = pHeadSettings->pixelClkSettings.pixelClkFreqHz;

    CHECK_RC(SendUpdates());

    return rc;
}

RC LwDisplaySor::PostSetModeCleanup(const DisplayID& displayID, UINT32 protocol)
{
    DisplayIDs displaysToDetach(1, displayID);

    return m_pDisplay->DetachDisplay(displaysToDetach);
}

RC LwDisplaySor::SetupOLut
(
    const HeadIndex head,
    LwDispHeadSettings *pHeadSettings
)
{
    RC rc;

    m_OLutSettings[head] = make_unique<LwDispLutSettings>();
    pHeadSettings->pOutputLutSettings = m_OLutSettings[head].get();

    OetfLutGenerator::OetfLwrves oetfLwrve =
        OetfLutGenerator::OETF_LWRVE_SRGB_1024;

    CHECK_RC(m_pLwDisplay->SetupOLutLwrve(pHeadSettings, oetfLwrve));

    // Values for MIRROR/INTERPOLATE mode are of no consequence as the test overrides
    // the output signals with fixed values from test registers.
    // No pixel processing done in the display pipeline affects the loopback logic.
    pHeadSettings->pOutputLutSettings->Disp30.SetControlOutputLut(
        LwDispLutSettings::Disp30Settings::INTERPOLATE_ENABLE,
        LwDispLutSettings::Disp30Settings::MIRROR_ENABLE,
        LwDispLutSettings::Disp30Settings::MODE_SEGMENTED);
    pHeadSettings->combinedHeadBounds.usageBounds.oLutUsage =
        LwDispHeadUsageBounds::OLUT_ALLOWED_TRUE;

    return rc;
}

RC LwDisplaySor::UpdatePixelClockForModePossible
(
    HeadIndex headIndex,
    Display::Mode mode,
    LwDispHeadSettings *pHeadSettings
)
{
    RC rc;

    UINT32 minPclk = 55000000;
    UINT32 maxPclk = pHeadSettings->pixelClkSettings.pixelClkFreqHz; // pclk that failed
    UINT32 midPclk = maxPclk;

    LwDisplay::IsModePossibleOutput isModePossibleOutput;
    // Binary search for the supported pclk between 55MHz and current pclk which failed
    while (minPclk < (maxPclk - 1000000))
    {
        midPclk = (minPclk + maxPclk) / 2;
        pHeadSettings->pixelClkSettings.pixelClkFreqHz = midPclk;
        CHECK_RC(m_pLwDisplay->IsModePossibleFromLwstomSettings((1 << headIndex),
            &isModePossibleOutput));
        if (isModePossibleOutput.isModePossible)
            minPclk = midPclk;
        else
            maxPclk = midPclk;
    }

    if (!isModePossibleOutput.isModePossible)
        return RC::MODE_IS_NOT_POSSIBLE;

    return rc;
}

RC LwDisplaySor::FillRasterSettings
(
    RasterSettings *pRasterSettings,
    UINT32 hActive,
    UINT32 vActive,
    UINT32 pclk
)
{
    if (m_FrlMode)
    {
        pRasterSettings->LaneCount = 4;
        pRasterSettings->LinkRate = 12;
    }

    return DisplaySor::FillRasterSettings(pRasterSettings, hActive, vActive, pclk);
}

RC LwDisplaySor::SendUpdates()
{
    return m_pLwDisplay->SendInterlockedUpdates
        (
            LwDisplay::UPDATE_CORE,
            0,
            0,
            0,
            0,
            LwDisplay::DONT_INTERLOCK_CHANNELS,
            LwDisplay::WAIT_FOR_NOTIFIER
        );
}

//------------------------------------------------------------------------------
// EvoDisplaySor
//------------------------------------------------------------------------------
EvoDisplaySor::EvoDisplaySor
(
    GpuSubdevice *pSubdev,
    Display *pDisplay,
    SorLoopback *pParent
)
: DisplaySor(pSubdev, pDisplay)
, m_pParent(pParent)
{

}

RC EvoDisplaySor::Setup
(
    Tee::Priority printPriority,
    Display::Mode cleanupMode
)
{
    RC rc;

    DisplaySor::Setup(printPriority, cleanupMode);
    m_OriginalDisplay = m_pDisplay->Selected();

    m_DefaultSplitSorReg.clear();
    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SPLIT_SOR))
    {
        CHECK_RC(m_pDisplay->CaptureSplitSor(&m_DefaultSplitSorReg));
    }

    // Test the chip's split SOR cofiguration if supported
    CHECK_RC(m_pDisplay->GetSupportedSplitSorConfigs(&m_SplitSorConfig));

    m_TestSplitSors = ((m_SplitSorConfig.size() != 0) && m_pParent->GetEnableSplitSOR());
    m_NumSplitSorCombos = (m_TestSplitSors ?
                                static_cast<UINT32>(m_SplitSorConfig.size()) : 1);

    // Fetch any per Split SOR skip display masks in split SOR mode
    if (m_TestSplitSors)
        CHECK_RC(ParseSkipDisplayMaskBySplitSOR());

    UINT32 MAX_SOR_NUM = m_pSubdev->Regs().LookupArraySize(MODS_PDISP_SOR_LANE_SEQ_CTL, 1);
    for (UINT32 i=0; i < MAX_SOR_NUM; i++) 
    {
        m_RegAddSorBasedRegLookup.push_back(m_pSubdev->Regs().LookupAddress(MODS_PDISP_SOR_LANE_SEQ_CTL, i));
    }

    return OK;
}

RC EvoDisplaySor::Cleanup()
{
    StickyRC rc;

    rc = DisplaySor::Cleanup();

    // Restore the default Split SOR configuration if needed
    if ((m_DefaultSplitSorReg.size() > 0) &&
        (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SPLIT_SOR)))
    {
        rc = m_pDisplay->ConfigureSplitSor(m_DefaultSplitSorReg);
    }

    rc = m_pDisplay->SetTimings(nullptr);
    rc = m_pDisplay->Select(m_OriginalDisplay);
    rc = m_pDisplay->SetMode(m_CleanupMode.width, m_CleanupMode.height,
                  m_CleanupMode.depth, m_CleanupMode.refreshRate);

    rc = SendUpdates();

    return rc;
}

RC EvoDisplaySor::SetTimings(UINT32 hActive, UINT32 vActive, UINT32 pclk)
{
    RC rc;
    CHECK_RC(FillRasterSettings(&m_ers, hActive, vActive, pclk));
    CHECK_RC(m_pDisplay->SetTimings(&m_ers));

    return rc;
}

RC EvoDisplaySor::SetMode
(
    UINT32 width,
    UINT32 height,
    UINT32 depth,
    UINT32 refreshRate,
    UINT32 sorIndex
)
{
    return m_pDisplay->SetMode(width, height, depth, refreshRate);
}

RC EvoDisplaySor::RetrySetMode
(
    UINT32 width,
    UINT32 height,
    UINT32 depth,
    UINT32 refreshRate,
    GpuSubdevice *pSubdev,
    UINT32 *updatedPclk
)
{
    RC rc_sm = RC::MODE_IS_NOT_POSSIBLE;
    RC rc;
    const UINT32 MinPixelClock = 55000000;
    while (rc_sm == RC::MODE_IS_NOT_POSSIBLE)
    {
        // Don't execute a SetMode above the current ers.PixelClockHz
        // We know it just failed a modeset
        // Search down from max pixel clock to as low as 55 MHz
        // Binary search for the pclk match
        Display::MaxPclkQueryInput maxPclkQuery = {m_pDisplay->Selected(), &m_ers};
        vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
        CHECK_RC(m_pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, MinPixelClock));

        CHECK_RC(m_pDisplay->SetTimings(&m_ers));
        rc_sm.Clear();

        rc_sm = pSubdev->GetPerf()->
            SetRequiredDispClk(m_ers.PixelClockHz, false);

        if (rc_sm == OK)
        {
            rc_sm = m_pDisplay->SetMode(width, height, depth, refreshRate);
        }

        if (rc_sm == OK)
        {
            *updatedPclk = m_ers.PixelClockHz;
        }
        else if (rc_sm == RC::MODE_IS_NOT_POSSIBLE)
        {
            m_ers.PixelClockHz -= 1000000;
            if (m_ers.PixelClockHz < MinPixelClock)
                return RC::MODE_IS_NOT_POSSIBLE;
        }
    }
    return rc;
}

RC EvoDisplaySor::SendUpdates()
{
    return m_pDisplay->SendUpdate
        (
            true,       // Core
            0xFFFFFFFF, // All bases
            0xFFFFFFFF, // All lwrsors
            0xFFFFFFFF, // All overlays
            0xFFFFFFFF, // All overlaysIMM
            true,       // Interlocked
            true        // Wait for notifier
        );
}

RC EvoDisplaySor::SetPixelClock(UINT32 pclk)
{
    return m_pDisplay->SetPixelClock(m_pDisplay->Selected(), pclk);
}

RC EvoDisplaySor::SendORSettings(UINT32 sorIndex, UINT32 protocol)
{
    RC rc;
    bool dualTmds = protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS;
    bool tmds = protocol != LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM;

    UINT32 head = 0;
    CHECK_RC(m_pDisplay->GetHead(m_pDisplay->Selected(), &head));
    if (m_pDisplay->GetClass() >= LW9070_DISPLAY) // >= GF119 - Can be made always???
    {
        UINT32 ownerMask = 1 << head;
        UINT32 protocol907D;
        switch (protocol)
        {
            case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM:
                protocol907D = LW907D_SOR_SET_CONTROL_PROTOCOL_LVDS_LWSTOM;
                break;
            case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A:
                protocol907D = LW907D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A;
                break;
            case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B:
                protocol907D = LW907D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_B;
                break;
            default:
                Printf(Tee::PriError,
                        "EvoSor: Protocol %d is not supported.\n", protocol);
                return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(m_pDisplay->SendCoreMethod(
                    LW907D_SOR_SET_CONTROL(sorIndex),
                    DRF_NUM(907D, _SOR_SET_CONTROL, _OWNER_MASK, ownerMask) |
                    DRF_NUM(907D, _SOR_SET_CONTROL, _PROTOCOL, protocol907D)));
    }
    else if (dualTmds || !tmds)
    {
        // Set TMDS protocol to copy mode:
        UINT32 owner = LW507D_SOR_SET_CONTROL_OWNER_HEAD0 + head;

        CHECK_RC(m_pDisplay->SendCoreMethod(
            LW507D_SOR_SET_CONTROL(sorIndex),
            DRF_NUM(507D, _SOR_SET_CONTROL, _OWNER,     owner)            |
            DRF_DEF(507D, _SOR_SET_CONTROL, _SUB_OWNER, _NONE)            |
            DRF_DEF(507D, _SOR_SET_CONTROL, _CRC_MODE,  _COMPLETE_RASTER) |
            DRF_NUM(507D, _SOR_SET_CONTROL, _PROTOCOL,
            tmds ? LW507D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_AB :
                   LW507D_SOR_SET_CONTROL_PROTOCOL_LVDS_LWSTOM) ));
    }
    return rc;
}

RC EvoDisplaySor::Select(UINT32 displayID)
{
    return m_pDisplay->Select(displayID);
}

// Parser function to generate a map of split SOR config index to
// skip display mask
// Format: <Split SOR Config Index#1>:<Skip Display Mask#1>|...
RC EvoDisplaySor::ParseSkipDisplayMaskBySplitSOR()
{
    RC rc;

    if ((m_pParent->GetSkipDisplayMaskBySplitSOR().length() == 0) ||
        m_SkipDisplayMaskBySplitSORList.size())
    {
        // If there is no per Split SOR Skip Display Mask provided, or
        // the skip list has already been parsed then ignore
        return OK;
    }

    map<UINT32, UINT32> *pSkipList = &m_SkipDisplayMaskBySplitSORList;
    vector<string> tokens;
    CHECK_RC(Utility::Tokenizer(m_pParent->GetSkipDisplayMaskBySplitSOR(), ":|", &tokens));
    if (tokens.size() % 2)
    {
        Printf(Tee::PriError,
               "Invalid no. of parameters in SkipDisplayMaskBySplitSOR: \n"
               "Expected <Split SOR Index#1>:<Skip Display Mask#1>|...\n");
        return RC::BAD_PARAMETER;
    }

    pSkipList->clear();
    UINT32 lwrrSplitSORIndex = 0xffffffff;
    UINT32 lwrrSkipDisplayMask = 0;
    for (UINT32 i = 0; i < tokens.size(); i+=2)
    {
        lwrrSplitSORIndex = strtoul(tokens[i].c_str(),NULL, 10);
        lwrrSkipDisplayMask = strtoul(tokens[i + 1].c_str(), NULL, 16);
        pSkipList->insert(pair<UINT32,UINT32>(lwrrSplitSORIndex,
                                              lwrrSkipDisplayMask));
    }

    return rc;
}

bool EvoDisplaySor::IsDisplayIdSkipped(UINT32 displayID)
{
    Display::ORCombinations ORCombos;

    // Check if the current OR/protocol needs to be skipped
    ORCombos.clear();
    if (OK != m_pDisplay->GetValidORCombinations(&ORCombos,
                                               false))
        return false;

    UINT32 ORIndex = 0xFFFFFFFF, ORType = 0xFFFFFFFF;
    UINT32 ORProtocol = 0xFFFFFFFF;
    if (OK != m_pDisplay->GetORProtocol(displayID,
                                      &ORIndex, &ORType,
                                      &ORProtocol))
    {
        return false;
    }

    vector <Display::ORInfo>::iterator it = ORCombos.begin();
    for ( ; it != ORCombos.end(); it++)
    {
        // Auto-skip OR/protocol combos that are redundant with
        // the non-split SOR mode *if* the non-split SOR mode is
        // not being already skipped
        if ((it->IsRedundant) && !(m_SkipSplitSORMask & 0x1))
            continue;

        if (ORIndex == it->ORIndex &&
            ORType == it->ORType)
        {
            if ((ORProtocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS) &&
                (it->Protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A ||
                 it->Protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B))
            {
                // If DUAL_TMDS is being tested, and pixel clock <= 165 MHz,
                // TMDS_A or TMDS_B may also be possible on the split SOR combos
                break;
            }
            else if (ORProtocol == it->Protocol)
            {
                // Found a perfect OR/protocol match
                break;
            }
        }
    }

    return (it == ORCombos.end());
}

UINT32 EvoDisplaySor::GetRequiredLoopsCount()
{
    return m_NumSplitSorCombos;
}

RC EvoDisplaySor::ConfigureForLoop(UINT32 index)
{
    RC rc;

    if (m_TestSplitSors)
    {
        // Skip split SOR configs based on the skip mask
        if (m_SkipSplitSORMask & (1 << index))
                return OK;

        // Switching between split SOR modes can hang display
        // Ensure that displays are not being driven from a
        // previous split SOR config
        m_pDisplay->DetachAllDisplays();

        Printf(m_VerbosePrintPriority, "Testing Split SOR mode %s \n",
           Display::GetSplitSorModeString(m_SplitSorConfig[index]).c_str());

        CHECK_RC(m_pDisplay->ConfigureSplitSor(m_SplitSorConfig[index]));
    }

    return OK;
}

bool EvoDisplaySor::IsTestRequiredForDisplay(UINT32 loopIndex, DisplayID displayID)
{
    if (m_TestSplitSors)
    {
        // Skip invalid display IDs based on current split SOR config
        // Also assume the m_SkipDisplayMaskBySplitSORList[0]
        // is the default non-split SOR mode
        if (IsDisplayIdSkipped(displayID) ||
            (m_SkipDisplayMaskBySplitSORList[loopIndex] &
                    displayID))
        {
            Printf(m_VerbosePrintPriority, "Skipping 0x%08x in split SOR mode %s... \n",
                    displayID.Get(),
                    Display::GetSplitSorModeString(m_SplitSorConfig[loopIndex]).c_str());
            return false;
        }
    }

    return true;
}

void EvoDisplaySor::GetTMDSLinkSupport
(
    DisplayID displayID,
    bool *pLinkA,
    bool *pLinkB
)
{
    if (m_TestSplitSors)
    {
        *pLinkA = IsTmdsSingleLinkCapable(displayID, TMDS_LINK_A);
        *pLinkB = IsTmdsSingleLinkCapable(displayID, TMDS_LINK_B);
    }
    else
    {
        DisplaySor::GetTMDSLinkSupport(displayID, pLinkA, pLinkB);
    }
}

bool EvoDisplaySor::IsTmdsSingleLinkCapable(UINT32 displayID, TmdsLinkType LinkType)
{
    MASSERT(m_TestSplitSors);
    Display::ORCombinations ORCombos;

    // Check if the current OR/protocol needs to be skipped
    ORCombos.clear();
    if (OK != m_pDisplay->GetValidORCombinations(&ORCombos,
                                               false))
    {
        Printf(Tee::PriNormal, "SorLoopback: GetValidORCombinations failure for "
            "0x%x, returning TRUE from IsTmdsSingleLinkCapable.\n", displayID);
        return true;
    }

    UINT32 ORIndex = 0xFFFFFFFF, ORType = 0xFFFFFFFF;
    UINT32 ORProtocol = 0xFFFFFFFF;
    if (OK != m_pDisplay->GetORProtocol(displayID,
                                      &ORIndex, &ORType,
                                      &ORProtocol))
    {
        Printf(Tee::PriNormal, "SorLoopback: GetORProtocol failure for 0x%x, "
            "returning TRUE from IsTmdsSingleLinkCapable.\n", displayID);
        return true;
    }

    vector <Display::ORInfo>::iterator it = ORCombos.begin();
    for ( ; it != ORCombos.end(); it++)
    {
        if (ORIndex != it->ORIndex || ORType != it->ORType)
        {
            continue;
        }

        if ((LinkType == TMDS_LINK_A) &&
            it->Protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A)
        {
            break;
        }

        if ((LinkType == TMDS_LINK_B) &&
            it->Protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B)
        {
            break;
        }
    }

    return (it != ORCombos.end());
}

RC EvoDisplaySor::CacheEdid(DisplayID displayID, UINT32 protocol)
{
    RC rc;

    if (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM)
    {
        // If the EDID was already faked on the display, then get the
        // faked EDID so that it can be restored after the EDID is
        // faked for this test
        if (m_pDisplay->IsEdidFaked(displayID))
        {
            CHECK_RC(m_pDisplay->GetEdid(displayID, &m_CachedEdid, true));
        }
        else
        {
            // Clear the OldEdid so that when the EDID is restored it
            // sets NULL as the EDID (which reverts to the panel EDID)
            m_CachedEdid.clear();
        }

        UINT32 LVDSEdid8bpcSize = 0;
        const UINT08 *LVDSEdid8bpc = Edid::GetDefaultLVDSEdid(&LVDSEdid8bpcSize);

        CHECK_RC(m_pDisplay->SetEdid(displayID, LVDSEdid8bpc, LVDSEdid8bpcSize));
    }

    return rc;
}

RC EvoDisplaySor::RestoreEdid(DisplayID displayID, UINT32 protocol)
{
    RC rc;

    if (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM)
    {
        bool SetNullEdid = true;
        for (UINT32 idx = 0; idx < m_CachedEdid.size(); idx++)
        {
            if (m_CachedEdid[idx] != 0)
            {
                SetNullEdid = false;
                break;
            }
        }

        if (SetNullEdid)
        {
            CHECK_RC(m_pDisplay->SetEdid(displayID, NULL, 0));
        }
        else
        {
            CHECK_RC(m_pDisplay->SetEdid(displayID, &m_CachedEdid[0],
                static_cast<UINT32>(m_CachedEdid.size())));
        }
    }

    return rc;
}

RC EvoDisplaySor::AssignSOR
(
    DisplayID displayID,
    UINT32 requestedSor,
    UINT32 *pAssignedSor,
    UINT32 *pOrType,
    UINT32 *pProtocol
)
{
    RC rc;

    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
    {
        CHECK_RC(m_pDisplay->DetachAllDisplays());
    }
    CHECK_RC(DisplaySor::AssignSOR(displayID, requestedSor, pAssignedSor, pOrType, pProtocol));

    return rc;
}

RC EvoDisplaySor::PerformPassingSetMode
(
    DisplayID displayID,
    Display::Mode mode,
    UINT32 assignedSor,
    UINT32 protocol,
    UINT32 requestedPixelClockHz,
    UINT32 *pPassingPixelClockHz
)
{
    RC rc;

    *pPassingPixelClockHz = requestedPixelClockHz;

    CHECK_RC(CacheEdid(displayID, protocol));

    CHECK_RC(Select(displayID));
    CHECK_RC(SetTimings(mode.width, mode.height, requestedPixelClockHz));

    // The detach is needed so that IMP below is performed against
    // current pstate Vmin and not some random dispclk Vmin.
    CHECK_RC(m_pDisplay->DetachAllDisplays());
    RC rc_sm = SetMode(
            mode.width,
            mode.height,
            mode.depth,
            mode.refreshRate,
            assignedSor);

    // Bug 372353:
    if (rc_sm == RC::MODE_IS_NOT_POSSIBLE)
    {
        rc_sm.Clear();

        // Retry setmode by lowering down pclk
        CHECK_RC(RetrySetMode(
                    mode.width,
                    mode.height,
                    mode.depth,
                    mode.refreshRate,
                    m_pSubdev,
                    pPassingPixelClockHz));
    }

    Display::UpdateMode OrigUpdateMode = m_pDisplay->GetUpdateMode();
    m_pDisplay->SetUpdateMode(Display::ManualUpdate);

    // Explicitly set the clock in case the SetMode earlier has divided
    // it by 2 for dual link mode
    CHECK_RC(SetPixelClock(*pPassingPixelClockHz));
    CHECK_RC(SendORSettings(assignedSor, protocol));
    CHECK_RC(SendUpdates());

    m_pDisplay->SetUpdateMode(OrigUpdateMode);

    return rc;
}

RC EvoDisplaySor::PostSetModeCleanup(const DisplayID& displayID, UINT32 protocol)
{
    return RestoreEdid(displayID, protocol);
}

RC EvoDisplaySor::DoWARs
(
    UINT32 sorNumber,
    bool isProtocolTMDS,
    bool isLinkATestNeeded,
    bool isLinkBTestNeeded
)
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();

    //Sanity check for DsiVpllLockDelayOverride.
    if (m_pParent->GetDsiVpllLockDelayOverride())
    {
        if (regs.Test32(MODS_PDISP_SOR_PLL2_DCIR_PLL_RESET_ALLOW, sorNumber))
        {
            // Program the PLL delays if an override has been provided
            regs.Write32(MODS_PDISP_DSI_VPLL, m_pParent->GetDsiVpllLockDelay());
        }
        else
        {
            Printf(Tee::PriError, "LW_PDISP_SOR_PLL2_DCIR_PLL_RESET_ALLOW "
                   "needs to be set (in vbios) for DsiVpllLockDelayOverride to "
                   "actually take effect\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    if (m_pParent->GetWaitForEDATA() && m_pDisplay->GetClass() < LW9470_DISPLAY)
    {
        // This is needed to cooperate with the DisplayPort Library.
        // Force power up of all lanes with the lane sequencer and
        // make sure that the DP mode is disabled on both sublinks:
        CHECK_RC(DisableDPModeForced(sorNumber, isLinkATestNeeded, isLinkBTestNeeded));
    }

    if (m_pDisplay->GetClass() == LW9270_DISPLAY) // Kepler
    {
        // Bug 1009581:
        UINT32 RemSor = regs.Read32(MODS_PDISP_CLK_REM_SOR, 3);
        if (isProtocolTMDS)
        {
            regs.SetField(&RemSor, MODS_PDISP_CLK_REM_SOR_LINK_SPEED_TMDS);
        }
        else
        {
            regs.SetField(&RemSor, MODS_PDISP_CLK_REM_SOR_LINK_SPEED_LVDS);
        }
        regs.Write32(MODS_PDISP_CLK_REM_SOR, 3, RemSor);
    }

    return rc;
}

RC EvoDisplaySor::DisableDPModeForced
(
    UINT32 SorNumber,
    bool TestLinkA,
    bool TestLinkB
)
{
    // This is needed to cooperate with the DisplayPort Library.
    // Force power up of all lanes with the lane sequencer and
    // make sure that the DP mode is disabled on both sublinks
    RegHal &regs = m_pSubdev->Regs();
    UINT32 DPLinkCtl;
    // Force DP mode just for use of the sequencer, it will be
    // disabled again shortly

    RegHal::ArrayIndexes arrIdx0 = { SorNumber, 0 };
    RegHal::ArrayIndexes arrIdx1 = { SorNumber, 1 };
    DPLinkCtl = regs.Read32(MODS_PDISP_SOR_DP_LINKCTL, arrIdx0);
    regs.SetField(&DPLinkCtl, MODS_PDISP_SOR_DP_LINKCTL_ENABLE_YES);
    regs.Write32(MODS_PDISP_SOR_DP_LINKCTL, arrIdx0, DPLinkCtl);
    // Bug 888882:
    // SetMode in test 4 was leaving SOR in DP mode in second sublink
    DPLinkCtl = regs.Read32(MODS_PDISP_SOR_DP_LINKCTL, arrIdx1);
    regs.SetField(&DPLinkCtl, MODS_PDISP_SOR_DP_LINKCTL_ENABLE_NO);
    regs.Write32(MODS_PDISP_SOR_DP_LINKCTL, arrIdx1, DPLinkCtl);

    if (TestLinkA)
    {
        UINT32 DPPadCtl = regs.Read32(MODS_PDISP_SOR_DP_PADCTL, arrIdx0);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_0_NO);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_1_NO);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_2_NO);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_3_NO);
        regs.Write32(MODS_PDISP_SOR_DP_PADCTL, arrIdx0, DPPadCtl);
    }
    if (TestLinkB)
    {
        UINT32 DPPadCtl = regs.Read32(MODS_PDISP_SOR_DP_PADCTL, arrIdx1);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_0_NO);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_1_NO);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_2_NO);
        regs.SetField(&DPPadCtl, MODS_PDISP_SOR_DP_PADCTL_PD_TXD_3_NO);
        regs.Write32(MODS_PDISP_SOR_DP_PADCTL, arrIdx1, DPPadCtl);
    }

    // Run the lane sequencer
    UINT32 SeqCtl = regs.Read32(MODS_PDISP_SOR_LANE_SEQ_CTL, SorNumber);
    regs.SetField(&SeqCtl, MODS_PDISP_SOR_LANE_SEQ_CTL_DELAY, 0);
    regs.SetField(&SeqCtl, MODS_PDISP_SOR_LANE_SEQ_CTL_NEW_POWER_STATE_PU);
    regs.SetField(&SeqCtl, MODS_PDISP_SOR_LANE_SEQ_CTL_SETTING_NEW_TRIGGER);
    regs.Write32(MODS_PDISP_SOR_LANE_SEQ_CTL, SorNumber, SeqCtl);
    
    // Poll for the lane sequencer to be done
    Printf(Tee::PriLow, "SorLoopback: Waiting for the lane sequencer\n");
    // Allow test to proceed even for failure - so as to analyze the complete flow
    m_pParent->PollRegValue(m_RegAddSorBasedRegLookup[SorNumber],
            regs.SetField(MODS_PDISP_SOR_LANE_SEQ_CTL_SETTING_NEW_DONE),
            regs.LookupMask(MODS_PDISP_SOR_LANE_SEQ_CTL_SETTING_NEW),
            m_pParent->GetTimeoutMs());

    // Disable DP mode
    DPLinkCtl = regs.Read32(MODS_PDISP_SOR_DP_LINKCTL, arrIdx0);
    regs.SetField(&DPLinkCtl, MODS_PDISP_SOR_DP_LINKCTL_ENABLE_NO);
    regs.Write32(MODS_PDISP_SOR_DP_LINKCTL, arrIdx0, DPLinkCtl);

    return OK;
}

void EvoDisplaySor::SaveRegisters()
{
    m_SavedDsiVpllValue = m_pParent->GetDsiVpllLockDelayOverride() ?
            (m_pSubdev->Regs()).Read32(MODS_PDISP_DSI_VPLL) : 0;
}

void EvoDisplaySor::RestoreRegisters()
{
    if (m_pParent->GetDsiVpllLockDelayOverride())
    {
        (m_pSubdev->Regs()).Write32(MODS_PDISP_DSI_VPLL, m_SavedDsiVpllValue);
    }
}

RC EvoDisplaySor::RegisterCallbackArgs(LoopbackTester* pLoopbacktester)
{
    pLoopbacktester->RegisterDoWars(
                    std::bind(&EvoDisplaySor::DoWARs, this,
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4));
    return RC::OK;
}
//----------------------------------------------------------------------------
// Script interface.

JS_CLASS_INHERIT(SorLoopback, GpuTest,
                 "Display SorLoopback test.");

CLASS_PROP_READWRITE(SorLoopback, TMDSPixelClock, UINT32,
                     "TMDS Pixel clock in Hz");

CLASS_PROP_READWRITE(SorLoopback, LVDSPixelClock, UINT32,
                     "LVDS Pixel clock in Hz");

CLASS_PROP_READWRITE(SorLoopback, PllLoadAdj, UINT32,
                     "LW_PDISP_SOR_PLL1_LOADADJ value");

CLASS_PROP_READWRITE(SorLoopback, PllLoadAdjOverride, bool,
                     "Enable LW_PDISP_SOR_PLL1_LOADADJ override");

CLASS_PROP_READWRITE(SorLoopback, DsiVpllLockDelay, UINT32,
                     "Sets the PLL lock delay override in LW_PDISP_DSI_VPLL");

CLASS_PROP_READWRITE(SorLoopback, DsiVpllLockDelayOverride, bool,
                     "Enables PLL lock delay override in LW_PDISP_DSI_VPLL");

CLASS_PROP_READWRITE(SorLoopback, ForcedPatternIdx, UINT32,
                     "Selects which pattern to run, others are skipped");

CLASS_PROP_READWRITE(SorLoopback, ForcePattern, bool,
                     "Enables single user selected pattern runs");

CLASS_PROP_READWRITE(SorLoopback, CRCSleepMS, UINT32,
                     "Sleep in MS before reading CRC values from registers");

CLASS_PROP_READWRITE(SorLoopback, UseSmallRaster, bool,
                     "Execute SetMode on a smaller raster for sims");

CLASS_PROP_READWRITE(SorLoopback, WaitForEDATA, bool,
                     "Wait for results in EDATA registers");

CLASS_PROP_READWRITE(SorLoopback, IgnoreCrcMiscompares, bool,
                     "Don't return with CRC miscompare error");

CLASS_PROP_READWRITE(SorLoopback, SkipDisplayMask, UINT32,
                     "Do not run SorLoopback on these display masks");

CLASS_PROP_READWRITE(SorLoopback, SleepMSOnError, UINT32,
                     "Sleep in MS after determining an error");

CLASS_PROP_READWRITE(SorLoopback, PrintErrorDetails, bool,
                     "Print detailed info on failure");

CLASS_PROP_READWRITE(SorLoopback, PrintModifiedCRC, bool,
                     "Print modified CRC values after lane remapping");

CLASS_PROP_READWRITE(SorLoopback, EnableDisplayRegistersDump, bool,
                     "bool: enable dumping display register just before CRC check");

CLASS_PROP_READWRITE(SorLoopback, EnableDisplayRegistersDumpOnCrcMiscompare, bool,
                     "bool: enable dumping display register after negative CRC check");

CLASS_PROP_READWRITE(SorLoopback, NumRetries, UINT32,
                     "Number of attempts to reread test results");

CLASS_PROP_READWRITE(SorLoopback, InitialZeroPatternMS, UINT32,
                     "Sleep in MS with zero output before sending the patterns");

CLASS_PROP_READWRITE(SorLoopback, FinalZeroPatternMS, UINT32,
                     "Sleep in MS with zero output before after sending the patterns");

CLASS_PROP_READWRITE(SorLoopback, TestOnlySor, UINT32,
                     "Limits the test to single selected SOR");

CLASS_PROP_READWRITE(SorLoopback, LVDSA, bool,
                     "Enables testing of first sublink on LVDS outputs");

CLASS_PROP_READWRITE(SorLoopback, LVDSB, bool,
                     "Enables testing of second sublink on LVDS outputs");

CLASS_PROP_READWRITE(SorLoopback, EnableSplitSOR, bool,
                     "Enable testing of all possible split SOR combinations");

CLASS_PROP_READWRITE(SorLoopback, SkipSplitSORMask, UINT32,
                     "Do not run SorLoopback on these split SOR combinations");

CLASS_PROP_READWRITE(SorLoopback, SkipDisplayMaskBySplitSOR, string,
                     "Do not run SorLoopback for specified split SOR configs");

CLASS_PROP_READWRITE(SorLoopback, SkipLVDS, bool,
                     "Don't test LVDS outputs");

CLASS_PROP_READWRITE(SorLoopback, SkipUPhy, bool,
                     "Don't test UPhy outputs");

CLASS_PROP_READWRITE(SorLoopback, SkipLegacyTmds, bool,
                     "Don't test Legacy TMDS protocols");

CLASS_PROP_READWRITE(SorLoopback, SkipFrl, bool,
                     "Don't test FRL protocol");

CLASS_PROP_READWRITE(SorLoopback, IobistLoopCount, UINT32,
                     "Number of cycles to run during IOBIST test. "
                     "2^IobistLoopCount cycles will be run");

CLASS_PROP_READWRITE(SorLoopback, IobistCaptureBufferForSecLink, bool,
                     "Capture buffer should hold data for secondary link in case of failure");

struct PollRegValue_Args
{
    UINT32 RegAdr;
    UINT32 ExpectedData;
    UINT32 DataMask;
    GpuSubdevice *Subdev;
};

static bool GpuPollRegValue
(
   void * Args
)
{
    PollRegValue_Args * pArgs = static_cast<PollRegValue_Args*>(Args);

    UINT32 RegValue = pArgs->Subdev->RegRd32(pArgs->RegAdr);
    RegValue &= pArgs->DataMask;
    return (RegValue == pArgs->ExpectedData);
}

RC SorLoopback::PollPairRegValue
(
    UINT32 regAddr,
    UINT64 expectedData,
    UINT64 availableBits,
    FLOAT64 timeoutMs
)
{
    RC rc;

    UINT32 lowerRegExpectedData = LwU64_LO32(expectedData);
    CHECK_RC(PollRegValue(regAddr, lowerRegExpectedData, 0xFFFFFFFF, timeoutMs));

    UINT32 upperRegExpectedData = LwU64_HI32(expectedData);
    CHECK_RC(PollRegValue(regAddr + 4, upperRegExpectedData,
        ~(1 << (availableBits-32)), timeoutMs));

    return rc;
}

RC SorLoopback::PollRegValue
(
    UINT32 regAddr,
    UINT32 expectedData,
    UINT32 dataMask,
    FLOAT64 timeoutMs
)
{
    RC rc;

    PollRegValue_Args Args =
    {
        regAddr,
        expectedData & dataMask,
        dataMask,
        m_pSubdev
    };

    rc = POLLWRAP_HW(GpuPollRegValue, &Args, timeoutMs);

    if (rc != OK)
        VerbosePrintf(
            "SorLoopback: PollRegValue error %d,"
            " Register 0x%08x: 0x%08x != expected 0x%08x.\n",
            rc.Get(), regAddr,
            m_pSubdev->RegRd32(regAddr) & dataMask, expectedData & dataMask);

    return rc;
}

SorLoopback::SorLoopback()
{
    SetName("SorLoopback");
}

RC SorLoopback::Setup()
{
    RC rc;

    m_pDisplay = GetDisplay();
    m_pSubdev = GetBoundGpuSubdevice();

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(InitProperties());

    // Reserve the real display for this test.
    // Also disables user interface and sets default mode on default display
    // selection mask (probably 1024x768x32bppx60hz).
    CHECK_RC(GpuTest::AllocDisplay());

    if (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
        m_pDispSor = make_unique<LwDisplaySor>(m_pSubdev, m_pDisplay);
    }
    else
    {
        m_pDispSor = make_unique<EvoDisplaySor>(m_pSubdev, m_pDisplay, this);
    }
    CHECK_RC(m_pDispSor->Setup(GetVerbosePrintPri(), m_CleanupMode));

    CHECK_RC(m_pDisplay->GetVirtualDisplays(&m_VirtualDisplayMask));

    LoopbackTester::SorLoopbackTestArgs sorLoopbackTestArgs;
    CHECK_RC(SetSorLoopbackTestArgs(&sorLoopbackTestArgs));

    m_pLoopBackTesterLegacy = make_unique<LegacyLoopbackTester>(m_pSubdev, m_pDisplay,
            sorLoopbackTestArgs, GetVerbosePrintPri());
    m_pLoopBackTesterUPhy = make_unique<UPhyLoopbackTester>(m_pSubdev, m_pDisplay,
            sorLoopbackTestArgs, GetVerbosePrintPri());
    m_pLoopBackTesterIobist = make_unique<IobistLoopbackTester>(m_pSubdev, m_pDisplay,
            sorLoopbackTestArgs, GetVerbosePrintPri());

    return rc;
}

RC SorLoopback::Run()
{
    RC rc;

    VerbosePrintf("Set graphics mode to %dx%dx%dx%d.\n",
       m_Mode.width, m_Mode.height, m_Mode.depth, m_Mode.refreshRate);

    for (UINT32 i = 0; i < m_pDispSor->GetRequiredLoopsCount(); i++)
    {
        CHECK_RC(RunForAllProtocols(i));
    }

    // Error out if no SORs were tested
    if (m_NumSorsTested == 0)
    {
        Printf(Tee::PriError, "SorLoopback did NOT test any SORs.\n");
        return RC::NO_TESTS_RUN;
    }

    return rc;
}

RC SorLoopback::RunForAllProtocols(UINT32 loopIndex)
{
    RC rc;

    auto RunTest = [&](bool frlMode) -> RC
    {
        CHECK_RC(m_pDispSor->ConfigureForLoop(loopIndex));
        CHECK_RC(RunOnAllConnectors(loopIndex, frlMode));
        return rc;
    };

    if (!m_SkipLegacyTmds)
    {
        VerbosePrintf("Running test with legacy TMDS protocols\n");
        CHECK_RC(RunTest(false));
    }

    if (!m_SkipFrl && m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_HDMI21_FRL))
    {
        VerbosePrintf("Running test with FRL protocol\n");
        CHECK_RC(RunTest(true));
    }

    return rc;
}

RC SorLoopback::RunOnAllConnectors(UINT32 loopIndex, bool frlMode)
{
    RC rc;
    set<UINT32> TMDS_SorsTested;

    UINT32 Connectors = 0;

    CHECK_RC(m_pDispSor->SetFrlMode(frlMode));
    CHECK_RC(SetFrlMode(frlMode));
    CHECK_RC(m_pDisplay->GetConnectors(&Connectors));

    while (Connectors)
    {
        DisplayID displayIDToTest = Connectors & ~(Connectors - 1);
        Connectors ^= displayIDToTest;

        bool isRequired = true;
        CHECK_RC(IsTestRequiredForDisplay(loopIndex, displayIDToTest, &isRequired));
        if (!isRequired)
            continue;

        Utility::CleanupOnReturnArg<SorLoopback, RC, UINT32> RestoreSORExcludeMask(
            this, &SorLoopback::CleanSORExcludeMask, displayIDToTest);

        for (UINT32 sorIndex = 0; sorIndex < GetMaxSORCountPerDisplayID(); sorIndex++)
        {
            UINT32 assignedSor = 0xBAD;
            UINT32 orType = 0xBAD;
            UINT32 protocol = 0xBAD;

            RC assignRc = m_pDispSor->AssignSOR(displayIDToTest, sorIndex,
                            &assignedSor, &orType, &protocol);
            if (assignRc == RC::LWRM_NOT_SUPPORTED)
                continue;

            CHECK_RC(assignRc);

            if ((protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS) &&
                 !m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR) &&
                 (TMDS_SorsTested.find(assignedSor) != TMDS_SorsTested.end()))
            {
                // Bug 352650:
                // Don't test the same TMDS SOR again. We force dual link TMDS the first time
                // we test a SOR, if we try to test it again under different displayIDToTest
                // value. RM can treat it as the same display mask as in previous run.
                break;
            }

            if (frlMode
                && (protocol != LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A))
            {
                // FRL is supported only on TMDS_A
                break;
            }

            if (orType != LW0073_CTRL_SPECIFIC_OR_TYPE_SOR)
            {
                VerbosePrintf("Display 0x%.8x is not of supported OR type, "
                    "skipping.\n", displayIDToTest.Get());
                continue;
            }

            if (m_TestOnlySor != 0xFFFFFFFF && assignedSor != m_TestOnlySor)
                continue;

            VerbosePrintf("Starting SorLoopback %s test on display %08x, SOR%d.\n",
                protocol != LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM ? "TMDS" : "LVDS",
                displayIDToTest.Get(), assignedSor);

            // Indicate that we've tested at least one SOR
            m_NumSorsTested++;

            CHECK_RC(RunDisplay(displayIDToTest, assignedSor, protocol));

            if ((protocol != LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM) &&
                !m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
            {
                TMDS_SorsTested.insert(assignedSor);
            }
        } /* loops for each crossbar selection */
    } /* loops for each display ID */
    return rc;
}

RC SorLoopback::IsTestRequiredForDisplay
(
    UINT32 loopIndex,
    DisplayID displayID,
    bool *pIsRequired
)
{
    RC rc;

    *pIsRequired = false;
    do
    {
        if (displayID & m_SkipDisplayMask)
        {
            VerbosePrintf("SorLoopback: Skipping display 0x%x (user override)\n",
                displayID.Get());
            break;
        }

        if (m_VirtualDisplayMask & displayID)
        {
            VerbosePrintf("SorLoopback: Skipping display 0x%x (virtual)\n",
                displayID.Get());
            break;
        }

        if (!m_pDispSor->IsTestRequiredForDisplay(loopIndex, displayID))
        {
            break;
        }

        *pIsRequired = true;
        UINT32 orType = LW0073_CTRL_SPECIFIC_OR_TYPE_NONE;
        if ((m_pDisplay->GetORProtocol(displayID, nullptr, &orType, nullptr) == OK) &&
            (orType != LW0073_CTRL_SPECIFIC_OR_TYPE_SOR))
        {
            *pIsRequired = false;
        }
        else if (m_pDisplay->GetDisplayType(displayID) == Display::DFP)
        {
            CHECK_RC(IsSupportedSignalType(displayID, pIsRequired));
        }

        if (!*pIsRequired)
        {
            VerbosePrintf("Display 0x%.8x is of unsupported kind, skipping.\n", displayID.Get());
            break;
        }
    } while(0);

    return OK;
}

RC SorLoopback::IsSupportedSignalType(DisplayID displayID, bool *pIsSupported)
{
    RC rc;

    *pIsSupported = true;

    LW0073_CTRL_DFP_GET_INFO_PARAMS dfpInfo = {0};
    dfpInfo.subDeviceInstance = m_pSubdev->GetSubdeviceInst();
    dfpInfo.displayId = displayID;
    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_INFO,
        &dfpInfo, sizeof(dfpInfo)));

    switch (REF_VAL(LW0073_CTRL_DFP_FLAGS_SIGNAL, dfpInfo.flags))
    {
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_LVDS:
            if (m_SkipLVDS)
            {
                Printf(Tee::PriWarn, "Skipping LVDS.\n");
                *pIsSupported = false;
            }
            break;
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_SDI:
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_DISPLAYPORT:
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_DSI:
            *pIsSupported = false;
            break;
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_TMDS:
            break;
        default:
            Printf(Tee::PriError, "Sorloopback: Invalid Signal type.\n");
            return RC::HW_STATUS_ERROR;
    }

    return OK;
}

RC SorLoopback::DetermineValidPixelClockHz
(
    UINT32 protocol,
    const Display::Encoder encoderInfo,
    UINT32 *pixelClockHz
)
{
    if (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM)
    {
        if (m_LVDSPixelClock > 0)
        {
            *pixelClockHz = m_LVDSPixelClock;
        }
    }
    else
    {
        if (m_TMDSPixelClock > 0)
        {
            *pixelClockHz = m_TMDSPixelClock;
            m_pDispSor->SetMaximizePclk(false);
        }
    }

    if (*pixelClockHz == 0)
    {
        if (encoderInfo.MaxLinkClockHz > 0)
        {
            *pixelClockHz = static_cast<UINT32>(encoderInfo.MaxLinkClockHz);
        }
        else
        {
            // On older GPUs the capabilities notifier doesn't report
            // the max values
            *pixelClockHz = 165000000;
        }

        if ((protocol != LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM) &&
              m_pSubdev->HasBug(932163))
        {
            if (*pixelClockHz > 300000000)
                *pixelClockHz = 300000000;
        }
    }

    if (m_pSubdev->IsDFPGA())
    {
        // To prevent underflows:
        *pixelClockHz = 25200000;
    }

    return OK;
}

UINT32 SorLoopback::GetMaxSORCountPerDisplayID()
{
    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
    {
        return LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS;
    }
    return 1;
}

RC SorLoopback::Cleanup()
{
    StickyRC rc;

    if (m_pDispSor)
    {
        rc = m_pDispSor->Cleanup();
    }
    rc = GpuTest::Cleanup();

    return rc;
}

bool SorLoopback::IsSupported()
{
    GpuDevice * pGpuDev = GetBoundGpuDevice();
    GpuSubdevice * pGpuSubdev = GetBoundGpuSubdevice();

    if (GetDisplay()->GetDisplayClassFamily() != Display::EVO &&
        GetDisplay()->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
        // Need an EVO or LW display.
        return false;
    }

    if ((pGpuDev->GetNumSubdevices() > 1) &&
        pGpuSubdev->GetSubdeviceInst() != pGpuDev->GetDisplaySubdeviceInst())
    {
        // Lwrrently only run on the "master" (display-owning) subdevice.
        return false;
    }

    return true;
}

RC SorLoopback::InitProperties()
{
    if (m_UseSmallRaster)
    {
        m_Mode.width       = 160;
        m_Mode.height      = 120;
        m_Mode.refreshRate = 600;

        m_CleanupMode = m_Mode;
    }
    else
    {
        m_Mode.width       = 1600;
        m_Mode.height      = 1200;
        m_Mode.refreshRate = 60;

        m_CleanupMode.width = 800;
        m_CleanupMode.height = 600;
        m_CleanupMode.refreshRate = 60;
    }

    m_Mode.depth = 32;
    m_CleanupMode.depth = 32;

    if (m_pSubdev->IsDFPGA())
    {
        // To prevent underflows:
        m_Mode.width = 640;
        m_Mode.height = 480;
    }

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    if (GetTestConfiguration()->DisableCrt())
    {
        VerbosePrintf("NOTE: ignoring TestConfiguration.DisableCrt.\n");
    }

    return OK;
}

RC SorLoopback::RunDisplay
(
    const DisplayID& displayID,
    UINT32 assignedSor,
    UINT32 protocol
)
{
    RC rc;

    Display::Encoder encoderInfo;
    CHECK_RC(m_pDisplay->GetEncoder(displayID, &encoderInfo));
    VerbosePrintf("Links to test: %s\n",
        Display::Encoder::GetLinksString(encoderInfo.ActiveLink).c_str());

    UINT32 pixelClockHz = 0;
    CHECK_RC(DetermineValidPixelClockHz(protocol, encoderInfo, &pixelClockHz));

    string Fmt = Utility::StrPrintf(
                    "Completed SorLoopback test on display %08x, SOR%d, link(s) %s . rc = %%d\n",
                    displayID.Get(), assignedSor, Display::Encoder::GetLinksString(encoderInfo.ActiveLink).c_str());
    DisplayCleanup::PrintRcErrMsg PrintErr(&rc, GetVerbosePrintPri(), (const char *)&Fmt[0]);

    if (m_pDispSor->IsLinkUPhy(encoderInfo.ActiveLink))
    {
       if (!m_SkipUPhy)
       {
           CHECK_RC(RunDisplayWithMappedProtocol(encoderInfo.ActiveLink, displayID, assignedSor,
               protocol, UPHY_LANES::ZeroOne, pixelClockHz));

           CHECK_RC(ReassignSorForDualLink(displayID, assignedSor));

           CHECK_RC(RunDisplayWithMappedProtocol(encoderInfo.ActiveLink, displayID, assignedSor,
               protocol, UPHY_LANES::TwoThree, pixelClockHz));
       }
    }
    else if (m_pDisplay->GetClass() >= LW9070_DISPLAY &&
        (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS)) // >= GF119
    {
        bool IsTMDSLinkASupported = true;
        bool IsTMDSLinkBSupported = true;

        m_pDispSor->GetTMDSLinkSupport(displayID, &IsTMDSLinkASupported, &IsTMDSLinkBSupported);

        // Bug 733188:
        if (IsTMDSLinkASupported)
        {
            CHECK_RC(RunDisplayWithMappedProtocol(encoderInfo.ActiveLink, displayID, assignedSor,
                LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A,
                UPHY_LANES::NONE, pixelClockHz));
        }
        if (IsTMDSLinkBSupported)
        {
            // Cleanup of primary link in dual tmds will clear sor mapping
            // Hence it needs to be reassigned
            CHECK_RC(ReassignSorForDualLink(displayID, assignedSor));

            CHECK_RC(RunDisplayWithMappedProtocol(encoderInfo.ActiveLink, displayID, assignedSor,
                LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B,
                UPHY_LANES::NONE, pixelClockHz));
        }
    }
    else
    {
        CHECK_RC(RunDisplayWithMappedProtocol(encoderInfo.ActiveLink, displayID,
            assignedSor, protocol, UPHY_LANES::NONE, pixelClockHz));
    }

    return rc;
}

RC SorLoopback::RunDisplayWithMappedProtocol
(
    Display::Encoder::Links link,
    DisplayID displayID,
    UINT32 assignedSor,
    UINT32 mappedProtocol,
    UPHY_LANES lane,
    UINT32 pixelClockHz
)
{
    RC rc;
    {
        DEFER
        {
            rc = m_pDispSor->PostSetModeCleanup(displayID, mappedProtocol);
            m_pDispSor->RestoreRegisters();
        };

        UINT32 passingPixelClockHz = 0;
        RegHal &regs = m_pSubdev->Regs();

        CHECK_RC(m_pDispSor->PerformPassingSetMode(displayID, m_Mode,
            assignedSor, mappedProtocol, pixelClockHz, &passingPixelClockHz));

        VerbosePrintf("Pixel clock = %uHz%s\n", passingPixelClockHz,
            (passingPixelClockHz > pixelClockHz) ? " (max allowed by IMP)" : "");

        VerbosePrintf("SorPwr  = 0x%08x.\n",
            regs.Read32(MODS_PDISP_SOR_PWR, assignedSor));
        VerbosePrintf("SorPll0 = 0x%08x.\n",
            regs.Read32(MODS_PDISP_SOR_PLL0, assignedSor));

        LoopbackTester *pTester = GetLoopbackTester(link);
        CHECK_RC(pTester->SetFrlMode(m_FrlMode));
        CHECK_RC(m_pDispSor->RegisterCallbackArgs(pTester));
        m_pDispSor->SaveRegisters();
        CHECK_RC(pTester->RunLoopbackTest(displayID,
            assignedSor, mappedProtocol, link, lane));
    }
    return rc;
}

RC SorLoopback::CleanSORExcludeMask(UINT32 displayID)
{
    StickyRC rc;
    rc = m_pDisplay->DetachAllDisplays();
    rc = m_pDisplay->SetSORExcludeMask(displayID, 0);

    return rc;
}

RC SorLoopback::ReassignSorForDualLink(const DisplayID &displayID, UINT32 sorIdx)
{
    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
    {
        SorInfoList assignSorListWithTag;
        UINT32 excludeMask = ~(1 << sorIdx);
        return m_pDisplay->AssignSOR(DisplayIDs(1, displayID),
            excludeMask, &assignSorListWithTag, Display::AssignSorSetting::ONE_HEAD_MODE);
    }

    return RC::OK;
}

LoopbackTester* SorLoopback::GetLoopbackTester(Display::Encoder::Links links)
{
    if (m_pDispSor->IsLinkUPhy(links))
    {
        return m_pLoopBackTesterUPhy.get();
    }

    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_IOBIST))
    {
        return m_pLoopBackTesterIobist.get();
    }

    return m_pLoopBackTesterLegacy.get();
}

RC SorLoopback::SetSorLoopbackTestArgs(LoopbackTester::SorLoopbackTestArgs* pSorLoopbackTestArgs)
{
    pSorLoopbackTestArgs->tmdsPixelClock = m_TMDSPixelClock;
    pSorLoopbackTestArgs->lvdsPixelClock = m_LVDSPixelClock;
    pSorLoopbackTestArgs->pllLoadAdj = m_PllLoadAdj;
    pSorLoopbackTestArgs->pllLoadAdjOverride = m_PllLoadAdjOverride;
    pSorLoopbackTestArgs->dsiVpllLockDelay = m_DsiVpllLockDelay;
    pSorLoopbackTestArgs->dsiVpllLockDelayOverride = m_DsiVpllLockDelayOverride;
    pSorLoopbackTestArgs->forcedPatternIdx = m_ForcedPatternIdx;
    pSorLoopbackTestArgs->forcePattern = m_ForcePattern;
    pSorLoopbackTestArgs->crcSleepMS = m_CRCSleepMS;
    pSorLoopbackTestArgs->useSmallRaster = m_UseSmallRaster;
    pSorLoopbackTestArgs->waitForEDATA = m_WaitForEDATA;
    pSorLoopbackTestArgs->ignoreCrcMiscompares = m_IgnoreCrcMiscompares;
    pSorLoopbackTestArgs->skipDisplayMask = m_SkipDisplayMask;
    pSorLoopbackTestArgs->sleepMSOnError = m_SleepMSOnError;
    pSorLoopbackTestArgs->printErrorDetails = m_PrintErrorDetails;
    pSorLoopbackTestArgs->printModifiedCRC = m_PrintModifiedCRC;
    pSorLoopbackTestArgs->enableDisplayRegistersDump = m_EnableDisplayRegistersDump;
    pSorLoopbackTestArgs->enableDisplayRegistersDumpOnCrcMiscompare
                            = m_EnableDisplayRegistersDumpOnCrcMiscompare;
    pSorLoopbackTestArgs->numRetries = m_NumRetries;
    pSorLoopbackTestArgs->initialZeroPatternMS = m_InitialZeroPatternMS;
    pSorLoopbackTestArgs->finalZeroPatternMS = m_FinalZeroPatternMS;
    pSorLoopbackTestArgs->testOnlySor = m_TestOnlySor;
    pSorLoopbackTestArgs->lvdsA = m_LVDSA;
    pSorLoopbackTestArgs->lvdsB = m_LVDSB;
    pSorLoopbackTestArgs->enableSplitSOR = m_EnableSplitSOR;
    pSorLoopbackTestArgs->skipSplitSORMask = m_SkipSplitSORMask;
    pSorLoopbackTestArgs->skipDisplayMaskBySplitSOR = m_SkipDisplayMaskBySplitSOR;
    pSorLoopbackTestArgs->skipLVDS = m_SkipLVDS;
    pSorLoopbackTestArgs->skipUPhy = m_SkipUPhy;
    pSorLoopbackTestArgs->skipLegacyTmds = m_SkipLegacyTmds;
    pSorLoopbackTestArgs->skipFrl = m_SkipFrl;
    pSorLoopbackTestArgs->iobistLoopCount = m_IobistLoopCount;
    pSorLoopbackTestArgs->iobistCaptureBufferForSecLink = m_IobistCaptureBufferForSecLink;
    pSorLoopbackTestArgs->timeoutMs = m_TimeoutMs;

    return RC::OK;
}
