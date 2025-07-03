/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/lwdisplay/lw_disp_utils.h"
#if !defined(TEGRA_MODS)
#include "gpu/display/evo_cctx.h"
#endif
#include "gpu/include/gsyncdev.h"
#include "gpu/include/gsyncdisp.h"
#include "Lwcm.h" //LW_CFGEX_GET_FLATPANEL_INFO_SCALER_NATIVE

namespace
{
struct GsyncAttachedDispPtrContainer
{
    union
    {
        LwDisplay* pLwDisplay;
        Display* pEvoDisplay;
    };
    Display::ClassFamily displayFamily = Display::NULLDISP;
    GsyncAttachedDispPtrContainer(Display* pDisplay);
};
//to cache the OrIndices and Displays connected through the edp connector
std::vector<UINT32> edpOrCache;
std::vector<UINT32> edpDispIdCache;

GsyncAttachedDispPtrContainer::GsyncAttachedDispPtrContainer(Display* pDisplay)
    : displayFamily(pDisplay->GetDisplayClassFamily())
{
#if !defined(TEGRA_MODS)
    switch (displayFamily)
    {
        case Display::LWDISPLAY:
            pLwDisplay = reinterpret_cast<LwDisplay*> (pDisplay);
            break;

        case Display::EVO:
            pEvoDisplay = pDisplay; // No need to use EvoDisplay directly
            break;

        default:
            Printf(Tee::PriError, "GsyncSink is attached to an unsupported "
                    "display of familyId %d.\n",
                    static_cast<INT32>(displayFamily));
            MASSERT(0);
    }
#else
        pEvoDisplay = pDisplay; // CheetAh
#endif
}
}

#if !defined(TEGRA_MODS)
RC EvoDispSetProcampSettings
(
    const GsyncDevice& gsyncDev, 
    Display::ORColorSpace cs
)
{
    RC rc;
    GsyncAttachedDispPtrContainer gsyncDisp(gsyncDev.GetPDisplay());
    EvoProcampSettings pcs;
    auto* pEvoDisplay = gsyncDisp.pEvoDisplay;
    //We want to override color space only
    pcs.ColorSpace = cs;
    if (cs != Display::ORColorSpace::RGB)
    {
        //These are default settings for YUV_609 and are tested on DP and HDMI
        pcs.ChromaLpf = EvoProcampSettings::CHROMA_LPF::ON;
        pcs.SatCos = 1024;
        pcs.SatSine = 0;
    }
    pcs.Dirty = true;
    CHECK_RC(pEvoDisplay->SetProcampSettings(&pcs));
    return rc;
}
#endif

RC GsyncDisplayHelper::SetMode
(
    const GsyncDevices& gsyncDevs,
    UINT32 width,
    UINT32 height,
    UINT32 refRate,
    UINT32 depth,
    Display::Display::ORColorSpace cs
)
{
    RC rc;
    for (const GsyncDevice& gsyncDev : gsyncDevs)
    {
        CHECK_RC(GsyncDisplayHelper::SetMode(gsyncDev, width, height, refRate,
                    depth, cs));
    }
    return rc;
}

RC GsyncDisplayHelper::SetMode
(
    const GsyncDevice& gsyncDev,
    UINT32 width,
    UINT32 height,
    UINT32 refRate,
    UINT32 depth, 
    Display::Display::ORColorSpace cs
)
{
    RC rc;
    DisplayID dispId = gsyncDev.GetDisplayID();
    GsyncAttachedDispPtrContainer gsyncDisp(gsyncDev.GetPDisplay());
#if !defined(TEGRA_MODS)
    if (gsyncDisp.displayFamily == Display::LWDISPLAY)
    {
        auto* pLwDisplay = gsyncDisp.pLwDisplay;
        // Use Single window set mode function to avoid complications
        const Display::Mode resolutionInfo(width, height, depth, refRate);
        HeadIndex head = Display::ILWALID_HEAD;
        CHECK_RC(pLwDisplay->GetFirstSupportedHead(dispId, &head));
        Printf(Tee::PriDebug, "%s: SetMode on disp 0x%x, head %u\n",
                __FUNCTION__, dispId.Get(), head);
        if (head == Display::ILWALID_HEAD)
        {
            Printf(Tee::PriError,
                    "No head is free for running setmode on display 0x%x\n",
                    dispId.Get());
            return RC::SOFTWARE_ERROR;
        }
    
        GsyncDisplayHelper::CBArgs cbArgs;
        cbArgs.cs = cs;
        cbArgs.headIdx = head;
        if (cs != Display::ORColorSpace::DEFAULT_CS)
        {    
            std::function <RC(void *)>CallBack 
                = [=] (void * pCbArgs) 
                {
                    RC rc;
                    CBArgs cbArgs = *static_cast<CBArgs*>(pCbArgs);
                    HeadIndex headIdx = cbArgs.headIdx;
                    Display::ORColorSpace proCampCS = cbArgs.cs;
                    LwDispHeadSettings *pHeadSettings = nullptr;
                    pHeadSettings = pLwDisplay->GetHeadSettings(headIdx);
                    pHeadSettings->procampSettings.ColorSpace = proCampCS;
                    pHeadSettings->procampSettings.dirty = true;
                    if (proCampCS != Display::ORColorSpace::RGB)
                    {
                        //These are default settings for YUV_609 and are tested on DP and HDMI
                        pHeadSettings->procampSettings.ChromaLpf 
                            = LwDispProcampSettings::CHROMA_LPF::ENABLE;
                        pHeadSettings->procampSettings.satCos = 1024;
                        pHeadSettings->procampSettings.satSine = 0;
                    }
                    return rc;
                };
            return pLwDisplay->SetModeWithSingleWindow(dispId,
                    resolutionInfo,
                    head,
                    CallBack, 
                    &cbArgs);
        }
        else
        {

            return pLwDisplay->SetModeWithSingleWindow(dispId,
                    resolutionInfo,
                    head);
        }
    }
    else
#endif
    {
        auto* pEvoDisplay = gsyncDisp.pEvoDisplay;
        CHECK_RC(pEvoDisplay->Select(dispId.Get()));
#if !defined(TEGRA_MODS)
        CHECK_RC(EvoDispSetProcampSettings(gsyncDev, cs));
#endif
        return (pEvoDisplay->SetMode(dispId.Get(),
                    width,
                    height,
                    depth,
                    refRate,
                    Display::FilterTapsNone,
                    Display::ColorCompressionNone,
                    LW_CFGEX_GET_FLATPANEL_INFO_SCALER_NATIVE
                    ));
    }
    return rc;
}

RC GsyncDisplayHelper::DisplayImage
(
    const GsyncDevice& gsyncDev,
    Surface2D *pSurf
)
{
    RC rc;
    DisplayID dispId = gsyncDev.GetDisplayID();
    GsyncAttachedDispPtrContainer gsyncDisp(gsyncDev.GetPDisplay());
#if !defined(TEGRA_MODS)
    if (gsyncDisp.displayFamily == Display::LWDISPLAY)
    {
        return gsyncDisp.pLwDisplay->DisplayImage(
                gsyncDev.GetDisplayID(), pSurf, nullptr);
    }
    else
#endif
    {
        return gsyncDisp.pEvoDisplay->SetImage(dispId.Get(), pSurf);
    }
    return rc;
}

RC GsyncDisplayHelper::DetachAllDisplays(const GsyncDevices& gsyncDevs)
{
    RC rc;
    for (const GsyncDevice& gsyncDev : gsyncDevs)
    {
        CHECK_RC(GsyncDisplayHelper::DetachAllDisplays(gsyncDev));
    }
    return rc;
}

RC GsyncDisplayHelper::DetachAllDisplays(const GsyncDevice& gsyncDev)
{
    RC rc;
    DisplayID dispId = gsyncDev.GetDisplayID();
    GsyncAttachedDispPtrContainer gsyncDisp(gsyncDev.GetPDisplay());

#if !defined(TEGRA_MODS)
    if (gsyncDisp.displayFamily == Display::LWDISPLAY)
    {
        LwDisplay* pLwDisplay = gsyncDisp.pLwDisplay;
        UINT32 headIndex = pLwDisplay->GetAttachedHead(dispId);
        Printf(Tee::PriDebug, "DetachHeadIndex: 0x%x\n", headIndex);
        UINT32 detachWindowMask = 0;
        CHECK_RC(pLwDisplay->GetWindowsMaskHeadOwned(
                headIndex,
                &detachWindowMask));
        Printf(Tee::PriDebug, "DetachWinMask: 0x%x\n",
                detachWindowMask);
        CHECK_RC(pLwDisplay->SetWindowsOwnerDynamic(
                headIndex,
                detachWindowMask,
                LwDisplay::WIN_OWNER_MODE_DETACH,
                LwDisplay::SEND_UPDATE,
                LwDisplay::DONT_WAIT_FOR_NOTIFIER));
        pLwDisplay->DetachDisplay(DisplayIDs(1, dispId));
    }
    else
#endif
    {
        return gsyncDisp.pEvoDisplay->DetachDisplay(DisplayIDs(1, dispId));
    }
    return rc;
}

RC GsyncDisplayHelper::AssignSor
(
    const GsyncDevice& gsyncDev,
    const OrIndex orIndex,
    Display::AssignSorSetting headMode,
    vector<UINT32>* pOutSorList,
    OrIndex* pOutSorIdx
)
{
    RC rc;

    const UINT32 excludeMask = ~(1 << orIndex);
    DisplayID dispId = gsyncDev.GetDisplayID();
    Display* pDisplay = gsyncDev.GetPDisplay();
    Display::Encoder encoder;
    //Skipping Assign sor in case of edp connected Or's or Displays
    for (UINT32 sorIdx = 0; sorIdx < edpOrCache.size(); sorIdx++)
    {
        if (orIndex == edpOrCache[sorIdx])
        {
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
    }
    
    for (UINT32 dispIdx = 0; dispIdx < edpDispIdCache.size(); dispIdx++)
    {
        if (dispId == edpDispIdCache[dispIdx])
        {
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
    }

    CHECK_RC(pDisplay->GetEncoder(dispId.Get(), &encoder));
    if (encoder.EmbeddedDP)
    {
        edpOrCache.push_back(encoder.ORIndex);
        edpDispIdCache.push_back(dispId.Get());
        Printf(Tee::PriDebug, "Display: 0x%x, is connected through EmbeddedDp on" 
                "SorIndex: 0x%x. Skipping sor Assignment on this Display!\n",
                dispId.Get(), encoder.ORIndex);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    Printf(Tee::PriDebug, "AssignSor on display 0x%x, excludeMask 0x%x\n",
            dispId.Get(), excludeMask);
    // Call this function just to check if the assignment is
    // possible. The SOR will be assigned again in SetupXBar.
    CHECK_RC(pDisplay->AssignSOR(
                DisplayIDs(1, dispId), excludeMask, *pOutSorList,
                headMode));
    // Need to call SetSORExcludeMask here as GetORProtocol calls
    // SetupXBar now.
    CHECK_RC(pDisplay->SetSORExcludeMask(dispId.Get(),
                excludeMask));

    CHECK_RC(pDisplay->GetORProtocol(dispId.Get(),
                pOutSorIdx, nullptr, nullptr));
    if (*pOutSorIdx != orIndex)
    {
        Printf(Tee::PriError,
                "GetORProtocol returned unexpected "
                "SOR%d, when SOR%d was expected.\n",
                *pOutSorIdx, orIndex);
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}
