/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <iostream>
#include <assert.h>
#include <algorithm>

#include "parsejsonconfig.h"

#include "gpu/include/dispchan.h"
#include "core/include/platform.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "lwMultiMon.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "ctrl/ctrl0073.h"
#include "gpu/tests/gputest.h"
#include "gpu/js/fpk_comm.h"
#include "core/include/rc.h"
#ifndef INCLUDED_MODESET_UTILS_H
#include "gpu/display/modeset_utils.h"
#endif
#include "lwdispjsoncfg_helper.h"

#ifndef CHECK_RC
#define CHECK_RC(f)             \
    do {                        \
    if (OK != (rc = (f)))   \
    return rc;            \
    } while (0)
#endif

#define MAX_WINDOWS 8

LwDispCompFactor::MatchSelect CParseJsonDisplaySettings::colwertWindowMatchSelect
(
    std::string matchSelect
)
{
    LwDispCompFactor::MatchSelect mselect = LwDispCompFactor::MatchSelect::ZERO;
    if (matchSelect == "ZERO")
        mselect = LwDispCompFactor::MatchSelect::ZERO;
    else if (matchSelect == "ONE")
        mselect = LwDispCompFactor::MatchSelect::ONE;
    else if (matchSelect == "K1")
        mselect = LwDispCompFactor::MatchSelect::K1;
    else if (matchSelect == "K2")
        mselect = LwDispCompFactor::MatchSelect::K2;
    else if (matchSelect == "NEG_K1")
        mselect = LwDispCompFactor::MatchSelect::NEG_K1;
    else if (matchSelect == "K1_TIMES_SRC")
        mselect = LwDispCompFactor::MatchSelect::K1_TIMES_SRC;
    else if (matchSelect == "K1_TIMES_DST")
        mselect = LwDispCompFactor::MatchSelect::K1_TIMES_DST;
    else if (matchSelect == "NEG_K1_TIMES_SRC")
        mselect = LwDispCompFactor::MatchSelect::ZERO;
    else if (matchSelect == "NEG_K1_TIMES_SRC")
        mselect = LwDispCompFactor::MatchSelect::ZERO;
    else
    {
        //TODO: assert here
        mselect = LwDispCompFactor::MatchSelect::ZERO;
    }
    return mselect;
}

bool  CParseJsonDisplaySettings::parseWindowCompositionSettings(const rapidjson::Value & window, LwDispWindowSettings *pWin)
{
    auto& compSettings = pWin->compositionSettings;

    Value::ConstMemberIterator depItr, ckItr, tmpItr, tmpItr2;

    depItr = window.FindMember("wCompCtrlDepth");
    if (depItr != window.MemberEnd())
    {
        compSettings.compCtrlDepth = depItr->value.GetInt64();
    }
    ckItr = window.FindMember("wColorKeySelect");
    if (ckItr != window.MemberEnd())
    {
        switch (ckItr->value.GetInt64())
        {
        case 0:
            compSettings.colorKeySelect = LwDispWindowCompositionSettings::ColorKeySelect::DISABLE;
            break;
        case 1:
            compSettings.colorKeySelect = LwDispWindowCompositionSettings::ColorKeySelect::SRC;
            break;
        case 2:
            compSettings.colorKeySelect = LwDispWindowCompositionSettings::ColorKeySelect::DST;
            break;
        default:
            //TODO
            //assert(0)
            compSettings.colorKeySelect = LwDispWindowCompositionSettings::ColorKeySelect::DISABLE;
            break;
        }
    }

    depItr = window.FindMember("wCompCtrlDepth");
    if (depItr != window.MemberEnd())
    {
        compSettings.compCtrlDepth = depItr->value.GetInt64();
    }

    tmpItr = window.FindMember("wSrcColorFactorMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.first.color.matchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());
    tmpItr = window.FindMember("wSrcColorFactorNoMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.first.color.noMatchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());
    tmpItr = window.FindMember("wDstColorFactorMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.second.color.matchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());
    tmpItr = window.FindMember("wDstColorFactorNoMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.second.color.noMatchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());
    tmpItr = window.FindMember("wSrcAlphaFactorMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.first.alpha.matchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());
    tmpItr = window.FindMember("wSrcAlphaFactorNoMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.first.alpha.noMatchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());
    tmpItr = window.FindMember("wDstAlphaFactorMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.second.alpha.matchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());
    tmpItr = window.FindMember("wDstAlphaFactorNoMatchSelect");
    if (tmpItr != window.MemberEnd())
        compSettings.matchSelectFactors.second.alpha.noMatchSelect =
            colwertWindowMatchSelect(tmpItr->value.GetString());

    tmpItr  = window.FindMember("wCompConsAlphaK1");
    tmpItr2 = window.FindMember("wCompConsAlphaK2");
    if (tmpItr != window.MemberEnd())
        compSettings.compConsAlphaK1 = tmpItr->value.GetInt64();
    if (tmpItr2 != window.MemberEnd())
        compSettings.compConsAlphaK2 = tmpItr2->value.GetInt64();

    tmpItr  = window.FindMember("wKeyAlphaMin");
    tmpItr2 = window.FindMember("wKeyAlphaMax");
    if (tmpItr != window.MemberEnd() && tmpItr2 != window.MemberEnd())
    {
        compSettings.keyAlpha.min = tmpItr->value.GetInt64();
        compSettings.keyAlpha.max = tmpItr2->value.GetInt64();
    }
    tmpItr  = window.FindMember("wKeyRedCrMin");
    tmpItr2 = window.FindMember("wKeyRedCrMax");
    if (tmpItr != window.MemberEnd() && tmpItr2 != window.MemberEnd())
    {
        compSettings.keyRedCr.min = tmpItr->value.GetInt64();
        compSettings.keyRedCr.max = tmpItr2->value.GetInt64();
    }
    tmpItr  = window.FindMember("wKeyGreenYMin");
    tmpItr2 = window.FindMember("wKeyGreenYMax");
    if (tmpItr != window.MemberEnd() && tmpItr2 != window.MemberEnd())
    {
        compSettings.keyGreenY.min = tmpItr->value.GetInt64();
        compSettings.keyGreenY.max = tmpItr2->value.GetInt64();
    }
    tmpItr  = window.FindMember("wKeyBlueCbMin");
    tmpItr2 = window.FindMember("wKeyBlueCbMax");
    if (tmpItr != window.MemberEnd() && tmpItr2 != window.MemberEnd())
    {
        compSettings.keyBlueCb.min = tmpItr->value.GetInt64();
        compSettings.keyBlueCb.max = tmpItr2->value.GetInt64();
    }

    return true;

}
bool CParseJsonDisplaySettings::parseWindowSurface(const rapidjson::Value & window, LwDispSurfaceParams *pWinSurface, LwU32 defaultWidth, LwU32 defaultHeight)
{
    Value::ConstMemberIterator imItr, hItr, wItr, xItr, yItr;

    imItr = window.FindMember("wImageFile");
    if (imItr != window.MemberEnd())
    {
        pWinSurface->imageFileName = std::string(imItr->value.GetString());
    }
    else
    {
        Printf(Tee::PriDebug,"No image specified, using default color\n");
        pWinSurface->bFillFBWithColor = true;
        pWinSurface->fbColor = 0;
        imItr = window.FindMember("wDefaultColor");
        if (imItr != window.MemberEnd())
        {
            pWinSurface->fbColor = imItr->value.GetInt64(); //TODO: Fix appropriately
        }
    }

    hItr = window.FindMember("wSurfaceFormat");
    if (hItr != window.MemberEnd())
    {
        pWinSurface->colorFormat = colwertSurfaceFormat(hItr->value.GetString());
        if (pWinSurface->colorFormat == ColorUtils::Format::LWFMT_NONE)
        {
            Printf(Tee::PriError, "ERROR: Invalid color format specified for window %s", hItr->value.GetString() );
            return false;
        }
    }

    hItr = window.FindMember("wSurfaceLayout");
    pWinSurface->layout = Surface2D::Layout::Pitch;
    if (hItr != window.MemberEnd())
    {
        pWinSurface->layout = colwertSurfaceLayout(hItr->value.GetString());
    }

    hItr = window.FindMember("wSurfaceInputRange");
    pWinSurface->inputRange = Surface2D::InputRange::FULL;
    if (hItr != window.MemberEnd())
    {
        pWinSurface->inputRange = colwertSurfaceInputRange(hItr->value.GetString());
    }

    wItr = window.FindMember("wImageWidth");
    hItr = window.FindMember("wImageHeight");
    if (hItr != window.MemberEnd() &&  wItr != window.MemberEnd())
    {
        pWinSurface->imageWidth  = wItr->value.GetInt64();
        pWinSurface->imageHeight = wItr->value.GetInt64();
    }
    else
    {
        pWinSurface->imageWidth = defaultWidth;
        pWinSurface->imageHeight = defaultHeight;
    }

    return true;
}

bool  CParseJsonDisplaySettings::parseWindowSettings(const rapidjson::Value & window, LwDispWindowSettings *pWin, LwDispSurfaceParams *pWinSurface, LwU32 defaultWidth, LwU32 defaultHeight)
{
    LwU32 width_in  = defaultWidth;
    LwU32 height_in = defaultHeight;
    LwU32 width = width_in;
    LwU32 height = height_in;
    LwU32 width_out = width;
    LwU32 height_out = height;
    LwU32 x = 0;
    LwU32 y = 0;
    Value::ConstMemberIterator hItr, wItr, xItr, yItr;
    // Set defaults
    pWin->SetPointSizeIlwalues(x, y, width, height);
    hItr = window.FindMember("wViewPortInHeight");
    wItr = window.FindMember("wViewPortInWidth");
    xItr = window.FindMember("wViewPortPointInX");
    yItr = window.FindMember("wViewPortPointInY");
    if (hItr != window.MemberEnd() && wItr != window.MemberEnd())
    {
        width = width_in = wItr->value.GetInt64();
        height = height_in = hItr->value.GetInt64();
    }
    if (xItr != window.MemberEnd() && yItr != window.MemberEnd())
    {
        x = xItr->value.GetInt64();
        y = yItr->value.GetInt64();
    }
    pWin->SetPointSizeIlwalues(x, y, width_in, height_in);

    // Set defaults to viewport in
    hItr = window.FindMember("wViewPortValidInHeight");
    wItr = window.FindMember("wViewPortValidInWidth");
    xItr = window.FindMember("wViewPortValidPointInX");
    yItr = window.FindMember("wViewPortValidPointInY");
    if (hItr != window.MemberEnd() && wItr != window.MemberEnd()
        && xItr != window.MemberEnd() && yItr != window.MemberEnd())
    {
        width = wItr->value.GetInt64();
        height = hItr->value.GetInt64();
        x = xItr->value.GetInt64();
        y = yItr->value.GetInt64();
        pWin->SetValidPointSizeIlwalues(x, y, width, height);
    }

    hItr = window.FindMember("wViewPortOutHeight");
    wItr = window.FindMember("wViewPortOutWidth");
    // set defaults
    pWin->SetSizeOut(width_out, height_out);
    if (hItr != window.MemberEnd() && wItr != window.MemberEnd())
    {
        width = wItr->value.GetInt64();
        height = hItr->value.GetInt64();
        pWin->SetSizeOut(width, height);
    }

    hItr = window.FindMember("wOpaqueInHeight");
    wItr = window.FindMember("wOpaqueInWidth");
    xItr = window.FindMember("wOpaquePointInX");
    yItr = window.FindMember("wOpaquePointInY");
    if (hItr != window.MemberEnd() && wItr != window.MemberEnd() &&
        xItr != window.MemberEnd() && yItr != window.MemberEnd())
    {
        Rect rect;
        rect.width = wItr->value.GetInt64();
        rect.height = hItr->value.GetInt64();
        rect.xPos = xItr->value.GetInt64();
        rect.yPos = yItr->value.GetInt64();

        pWin->AddOpaqueRect(rect);
    }

    hItr = window.FindMember("wBufSizeHeight");
    wItr = window.FindMember("wBufSizeWidth");
    if (hItr != window.MemberEnd() && wItr != window.MemberEnd())
    {
        width  = pWin->sizeOut.width  = wItr->value.GetInt64();
        height = pWin->sizeOut.height = hItr->value.GetInt64();
    }

    hItr = window.FindMember("wScalarVtaps");
    wItr = window.FindMember("wScalarHtaps");
    if (hItr != window.MemberEnd() && wItr != window.MemberEnd())
    {
        LwU32 vtaps = hItr->value.GetInt64();
        LwU32 htaps = wItr->value.GetInt64();

        LwDispScalerSettings::VTAPS verTaps;
        LwDispScalerSettings::HTAPS horTaps;

        switch(vtaps)
        {
        case 2:
            verTaps = LwDispScalerSettings::VTAPS::TAPS_2;
            break;
        case 5:
            verTaps = LwDispScalerSettings::VTAPS::TAPS_5;
            break;
        default:
            //TODO
            assert(0);
            verTaps = LwDispScalerSettings::VTAPS::TAPS_2;
            break;
        }

        switch(htaps)
        {
        case 2:
            horTaps = LwDispScalerSettings::HTAPS::TAPS_2;
            break;
        case 5:
            horTaps = LwDispScalerSettings::HTAPS::TAPS_5;
            break;
        default:
            //TODO
            assert(0);
            horTaps = LwDispScalerSettings::HTAPS::TAPS_2;
            break;
        }

        pWin->inputWindowScalerSettings.verticalTaps   = verTaps;
        pWin->inputWindowScalerSettings.horizontalTaps = horTaps;
        pWin->inputWindowScalerSettings.dirty = true;
    }

    // Window composition settings
    parseWindowCompositionSettings(window, pWin);

    if ( window.HasMember("wImageSurfaceParams"))
    {
        const Value & winImgSettings = window["wImageSurfaceParams"];
        parseWindowSurface(winImgSettings, pWinSurface, width_in, width_out);
    }
    else
    {
        // TODO: no window image settings are provided
        assert(0);
    }

    pWin->dirty = true;

    return true;
}

Surface2D::InputRange CParseJsonDisplaySettings::colwertSurfaceInputRange(std::string fmt)
{
    LwU32 i;
    for (i = 0; i < (sizeof(Surface2dInputRangeStr)/sizeof(*Surface2dInputRangeStr)); i++)
    {
        if (fmt == Surface2dInputRangeStr[i].inputRangeStr)
            return Surface2dInputRangeStr[i].inputRange;
    }
    return Surface2D::FULL;
}

ColorUtils::Format CParseJsonDisplaySettings::colwertSurfaceFormat(std::string fmt)
{
    LwU32 i;
    for (i = 0; i < (sizeof(ClrUtilStr)/sizeof(*ClrUtilStr)); i++)
    {
        if (fmt == ClrUtilStr[i].fmtStr)
            return ClrUtilStr[i].clrFmt;
    }
    return ColorUtils::Format::LWFMT_NONE;
}

Surface2D::Layout CParseJsonDisplaySettings::colwertSurfaceLayout(std::string fmt)
{
    LwU32 i;
    for (i = 0; i < (sizeof(Surf2dLayoutStr)/sizeof(*Surf2dLayoutStr)); i++)
    {
        if (fmt == Surf2dLayoutStr[i].layoutStr)
            return Surf2dLayoutStr[i].layout;
    }
    //TODO: add assert here
    return Surface2D::Layout::Pitch;
}

LwDispORSettings::ORType CParseJsonDisplaySettings::colwertOrType(std::string fmt)
{
    LwU32 i;
    for (i = 0; i < (sizeof(orTypeStr)/sizeof(*orTypeStr)); i++)
    {
        if (fmt == orTypeStr[i].orTypeStr)
            return orTypeStr[i].orType;
    }
    //TODO: add assert here
    return LwDispORSettings::ORType::SOR;
}

// FIX this... make this an intermediate data-structure instead of the class
LwU32 CParseJsonDisplaySettings::colwertOrProtocol(std::string fmt)
{
    LwU32 i;
    for (i = 0; i < (sizeof(orProtocolStr)/sizeof(*orProtocolStr)); i++)
    {
        if (fmt == orProtocolStr[i].orProtocolStr)
            return orProtocolStr[i].orProtocol;
    }
    //TODO: add assert here
    return LWC37D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A;
}

LwDispORSettings::PixelDepth CParseJsonDisplaySettings::colwertOrPixelDepth(std::string fmt)
{
    LwU32 i;
    for (i = 0; i < (sizeof(orPixelDepthStr)/sizeof(*orPixelDepthStr)); i++)
    {
        if (fmt == orPixelDepthStr[i].pixelDepthStr)
            return orPixelDepthStr[i].pixelDepth;
    }
    //TODO: add assert here
    return LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_ILWALID;
}

bool CParseJsonDisplaySettings::parseRasterSettings
(
 const rapidjson::Value &     raster,
 LwDispRasterSettings *        pRasterSettings,
 LwDispPixelClockSettings *    pPixelClkSettings
 )
{
    Value::ConstMemberIterator widxItr = raster.FindMember("rasterWidth");
    Value::ConstMemberIterator actxItr = raster.FindMember("activeX");
    Value::ConstMemberIterator sexItr = raster.FindMember("syncEndX");
    Value::ConstMemberIterator bsxItr = raster.FindMember("blankStartX");
    Value::ConstMemberIterator bexItr = raster.FindMember("blankEndX");
    Value::ConstMemberIterator polxItr = raster.FindMember("polarityX");

    Value::ConstMemberIterator heiItr = raster.FindMember("rasterHeight");
    Value::ConstMemberIterator actyItr = raster.FindMember("activeY");
    Value::ConstMemberIterator seyItr = raster.FindMember("syncEndY");
    Value::ConstMemberIterator bsyItr = raster.FindMember("blankStartY");
    Value::ConstMemberIterator beyItr = raster.FindMember("blankEndY");
    Value::ConstMemberIterator polyItr = raster.FindMember("polarityY");
    Value::ConstMemberIterator bsy2Itr = raster.FindMember("blank2StartY");
    Value::ConstMemberIterator bey2Itr = raster.FindMember("blank2EndY");

    Value::ConstMemberIterator plItr = raster.FindMember("pixelClockHz");
    Value::ConstMemberIterator pladj1000divItr = raster.FindMember("pixelClockAdj1000Div1001");
    Value::ConstMemberIterator plNotDriver = raster.FindMember("pixelClkNotDriver");
    Value::ConstMemberIterator plhoppItr = raster.FindMember("pixelClockHopping");
    // Value::ConstMemberIterator lcItr = raster.FindMember("laneCount");
    // Value::ConstMemberIterator lrItr = raster.FindMember("linkRate");
    Value::ConstMemberIterator intrItr = raster.FindMember("interlaced");

    if (actxItr == raster.MemberEnd() ||
        actyItr == raster.MemberEnd())
    {
        // Failed to parse the configuration, atleast need to have activex/activey settings
        Printf(Tee::PriDebug,"Insufficient configuration parameters at a minimum need activeX, activeY and refreshrate \n");
        return false;
    }

    // unless all remaining fields are described, use custom-generated timings
    if (widxItr == raster.MemberEnd() ||
        sexItr == raster.MemberEnd() ||
        bsxItr == raster.MemberEnd() ||
        bexItr == raster.MemberEnd() ||
        polxItr == raster.MemberEnd()||
        heiItr == raster.MemberEnd() ||
        actyItr == raster.MemberEnd() ||
        seyItr == raster.MemberEnd() ||
        beyItr == raster.MemberEnd() ||
        bsyItr == raster.MemberEnd() ||
        polyItr == raster.MemberEnd() ||
        plItr == raster.MemberEnd() )
    {
        Printf(Tee::PriDebug,"Not all json raster-settings are sepecified, using custom timings with 60Hz RR\n");
#if 0// TODO: Generate timings with minimal raster settings
        LwU32 refreshRate = 60; // by default assume 60Hz RR
        if (ModesetUtils::GetDispLwstomTiming(this, display, actxItr->value.GetInt64(),
            actyItr->value.GetInt64(), refreshRate,
            AUTO, pRasterSettings, 0, NULL) != OK)
        {
            Printf(Tee::PriDebug,"Edid Timings Not avilable callwlate timings based on CVT_RB \n");
            CHECK_RC(ModesetUtils::GetDispLwstomTiming(this, display, width, height, refreshRate,
                CVT_RB, pRasterSettings, 0, NULL));
        }
#endif
        pRasterSettings->Dirty = true;
    }
    else
    {
        pRasterSettings->RasterWidth  = widxItr->value.GetInt64();
        pRasterSettings->ActiveX      = actxItr->value.GetInt64();
        pRasterSettings->SyncEndX     = sexItr->value.GetInt64();
        pRasterSettings->BlankStartX  = bsxItr->value.GetInt64();
        pRasterSettings->BlankEndX    = bexItr->value.GetInt64();
        pRasterSettings->PolarityX    = !!polxItr->value.GetInt64();
        pRasterSettings->RasterHeight = heiItr->value.GetInt64();
        pRasterSettings->ActiveY      = actyItr->value.GetInt64();
        pRasterSettings->SyncEndY     = seyItr->value.GetInt64();
        pRasterSettings->BlankStartY  = bsyItr->value.GetInt64();
        pRasterSettings->BlankEndY    = beyItr->value.GetInt64();
        pRasterSettings->PolarityY    = !!polyItr->value.GetInt64();
        if (bsy2Itr != raster.MemberEnd() && bey2Itr != raster.MemberEnd())
        {
            pRasterSettings->Blank2StartY = bsy2Itr->value.GetInt64();
            pRasterSettings->Blank2EndY   = bey2Itr->value.GetInt64();
        }
        pRasterSettings->PixelClockHz = plItr->value.GetInt64();
    }

    pPixelClkSettings->pixelClkFreqHz = pRasterSettings->PixelClockHz;
    pPixelClkSettings->bAdj1000Div1001 = false;
    if (pladj1000divItr != raster.MemberEnd())
        pPixelClkSettings->bAdj1000Div1001 = !!pladj1000divItr->value.GetInt64();
    pPixelClkSettings->bNotDriver = false;
    if (plNotDriver != raster.MemberEnd() && plNotDriver->value.GetInt64())
        pPixelClkSettings->bNotDriver = true;
    pPixelClkSettings->bHopping = false;
    pPixelClkSettings->HoppingMode = LwDispPixelClockSettings::VBLANK;
    if (plhoppItr != raster.MemberEnd())
    {
        // TODO: Fixit
        pPixelClkSettings->HoppingMode = LwDispPixelClockSettings::VBLANK;
        pPixelClkSettings->bHopping    = true;
    }

    pRasterSettings->Interlaced   = false;
    if (intrItr != raster.MemberEnd() && intrItr->value.GetInt64())
        pRasterSettings->Interlaced   = true;

    pRasterSettings->Dirty = true;
    pPixelClkSettings->dirty = true;

    return true;
}

bool CParseJsonDisplaySettings::parseHeadViewPortSettings
(
    const rapidjson::Value & hViewPort,
    LwDispViewPortSettings *pHeadVP,
    LwU32                  defaultWidth,
    LwU32                  defaultHeight
)
{
    Value::ConstMemberIterator hItr = hViewPort.FindMember("hViewPortInHeight");
    Value::ConstMemberIterator wItr = hViewPort.FindMember("hViewPortInWidth");
    Value::ConstMemberIterator xItr = hViewPort.FindMember("hViewPortPointInX");
    Value::ConstMemberIterator yItr = hViewPort.FindMember("hViewPortPointInY");
    if (hItr != hViewPort.MemberEnd() && wItr != hViewPort.MemberEnd())
    {
        pHeadVP->ViewportIn.width  = wItr->value.GetInt64();;
        pHeadVP->ViewportIn.height = hItr->value.GetInt64();;
    }
    else
    {
        pHeadVP->ViewportIn.width  = defaultWidth;
        pHeadVP->ViewportIn.height = defaultHeight;
    }

    pHeadVP->ViewportIn.xPos = 0;
    pHeadVP->ViewportIn.yPos = 0;
    if (xItr != hViewPort.MemberEnd() && yItr != hViewPort.MemberEnd())
    {
        pHeadVP->ViewportIn.xPos = xItr->value.GetInt64();
        pHeadVP->ViewportIn.yPos = yItr->value.GetInt64();
    }
    // Fill in the defaults first
    pHeadVP->ViewportValidIn.width = pHeadVP->ViewportIn.width;
    pHeadVP->ViewportValidIn.height = pHeadVP->ViewportIn.height;
    pHeadVP->ViewportValidIn.xPos = pHeadVP->ViewportIn.xPos;
    pHeadVP->ViewportValidIn.yPos = pHeadVP->ViewportIn.yPos;

    hItr = hViewPort.FindMember("hViewPortValidInHeight");
    wItr = hViewPort.FindMember("hViewPortValidInWidth");
    if (hItr != hViewPort.MemberEnd() && wItr != hViewPort.MemberEnd())
    {
        pHeadVP->ViewportValidIn.width  = wItr->value.GetInt64();;
        pHeadVP->ViewportValidIn.height = hItr->value.GetInt64();;
    }
    xItr = hViewPort.FindMember("hViewPortValidPointInX");
    yItr = hViewPort.FindMember("hViewPortValidPointInY");
    if (xItr != hViewPort.MemberEnd() && yItr != hViewPort.MemberEnd())
    {
        pHeadVP->ViewportValidIn.xPos = xItr->value.GetInt64();
        pHeadVP->ViewportValidIn.yPos = yItr->value.GetInt64();
    }

    // Set defaults
    pHeadVP->ViewportOut.width = pHeadVP->ViewportIn.width;
    pHeadVP->ViewportOut.height = pHeadVP->ViewportIn.height;
    hItr = hViewPort.FindMember("hViewPortOutHeight");
    wItr = hViewPort.FindMember("hViewPortOutWidth");
    if (hItr != hViewPort.MemberEnd() && wItr != hViewPort.MemberEnd())
    {
        pHeadVP->ViewportOut.width = wItr->value.GetInt64();
        pHeadVP->ViewportOut.height = hItr->value.GetInt64();
    }

    // Set defaults
    pHeadVP->ViewportOut.xPos = 0;
    pHeadVP->ViewportOut.yPos = 0;
    xItr = hViewPort.FindMember("hViewPortPointOutAdjustX");
    yItr = hViewPort.FindMember("hViewPortPointOutAdjustY");
    if (xItr != hViewPort.MemberEnd() && yItr != hViewPort.MemberEnd())
    {
        pHeadVP->ViewportOut.xPos = xItr->value.GetInt64();
        pHeadVP->ViewportOut.yPos = yItr->value.GetInt64();
    }

    // Mark all the fields as dirty
    pHeadVP->ViewportOut.dirty  = true;
    pHeadVP->ViewportValidIn.dirty = true;
    pHeadVP->ViewportIn.dirty = true;

    return true;
}

bool CParseJsonDisplaySettings::parseLwrsorSettings(const rapidjson::Value & cursor, LwDispLwrsorSettings *pLwrsor, LwDispSurfaceParams *pLwrsorSurface)
{
    Value::ConstMemberIterator szItr   = cursor.FindMember("cSize");
    Value::ConstMemberIterator imItr   = cursor.FindMember("cImageFile");
    Value::ConstMemberIterator colrItr = cursor.FindMember("cDefaultColor");
    Value::ConstMemberIterator forItr  = cursor.FindMember("cSurfaceFormat");
    Value::ConstMemberIterator layItr  = cursor.FindMember("cSurfaceLayout");
    Value::ConstMemberIterator inrngItr = cursor.FindMember("cSurfaceInputRange");
    Value::ConstMemberIterator wItr    = cursor.FindMember("cImageWidth");
    Value::ConstMemberIterator hItr    = cursor.FindMember("cImageHeight");
    Value::ConstMemberIterator xItr    = cursor.FindMember("cHotSpotX");
    Value::ConstMemberIterator yItr    = cursor.FindMember("cHotSpotY");
    Value::ConstMemberIterator xPtOItr = cursor.FindMember("cHotSpotPointOutX");
    Value::ConstMemberIterator yPtOItr = cursor.FindMember("cHotSpotPointOutY");

    // Setup the defaults
    LwU32 defaultWidthHeight = 32;

    pLwrsorSurface->imageWidth = pLwrsorSurface->imageHeight = 32;
    pLwrsorSurface->layout = Surface2D::Pitch; // safe to always force pitch for cursor
    if (szItr != cursor.MemberEnd())
    {
        if (pLwrsorSurface->imageWidth == 32  ||
            pLwrsorSurface->imageWidth == 64  ||
            pLwrsorSurface->imageWidth == 128 ||
            pLwrsorSurface->imageWidth == 256)
        {
            defaultWidthHeight = pLwrsorSurface->imageWidth = pLwrsorSurface->imageHeight = szItr->value.GetInt64();
        }
        else
        {
            assert(0);
        }
    }
    pLwrsorSurface->fbColor = 0;
    if (colrItr != cursor.MemberEnd())
    {
        pLwrsorSurface->fbColor = colrItr->value.GetInt64();
    }

    if (imItr != cursor.MemberEnd())
    {
        pLwrsorSurface->imageFileName = imItr->value.GetString();
        pLwrsorSurface->bFillFBWithColor = false;
    }
    else
    {
        Printf(Tee::PriDebug,"No image specified, using default color\n");
        pLwrsorSurface->bFillFBWithColor = true;
    }

    pLwrsor->hotSpotPointOutX = pLwrsor->hotSpotPointOutY = 0;
    if (xPtOItr != cursor.MemberEnd() && yPtOItr != cursor.MemberEnd())
    {
        pLwrsor->hotSpotPointOutX = xPtOItr->value.GetInt64();
        pLwrsor->hotSpotPointOutY = yPtOItr->value.GetInt64();
    }

    pLwrsor->SetLwrsorImmSettings(0, 0);
    if (xItr != cursor.MemberEnd() && yItr != cursor.MemberEnd())
    {
        pLwrsor->SetLwrsorImmSettings(xItr->value.GetInt64(),
            yItr->value.GetInt64());
    }

    if (forItr != cursor.MemberEnd())
    {
        pLwrsorSurface->colorFormat = colwertSurfaceFormat(forItr->value.GetString());
        if (pLwrsorSurface->colorFormat == ColorUtils::Format::LWFMT_NONE)
        {
            Printf(Tee::PriError, "ERROR: Invalid color format specified for window %s" , forItr->value.GetString() );
            return false;
        }
    }

    pLwrsorSurface->layout = Surface2D::Layout::Pitch;
    if (layItr != cursor.MemberEnd())
    {
        pLwrsorSurface->layout = colwertSurfaceLayout(layItr->value.GetString());
    }

    pLwrsorSurface->inputRange = Surface2D::InputRange::FULL;
    if (inrngItr != cursor.MemberEnd())
    {
        pLwrsorSurface->inputRange = colwertSurfaceInputRange(inrngItr->value.GetString());
    }

    if (hItr != cursor.MemberEnd() &&  wItr != cursor.MemberEnd())
    {
        pLwrsorSurface->imageWidth  = wItr->value.GetInt64();
        pLwrsorSurface->imageHeight = hItr->value.GetInt64();
    }
    else
    {
        pLwrsorSurface->imageWidth = defaultWidthHeight;
        pLwrsorSurface->imageHeight = defaultWidthHeight;
    }

    return true;
}

bool CParseJsonDisplaySettings::parseOrSettings(const rapidjson::Value & orsetting, LwDispORSettings *pOr, std::string &orProtocolStr, LwU32 orNum)
{
    Value::ConstMemberIterator typeItr = orsetting.FindMember("orType");
    Value::ConstMemberIterator protItr = orsetting.FindMember("orProtocol");
    Value::ConstMemberIterator secItr = orsetting.FindMember("orPixelDepth");
    Value::ConstMemberIterator polItr = orsetting.FindMember("orDePolarity");

    if (orNum > 7) //FIXIT
        return false;

    pOr->Reset();

    pOr->pixelDepth = LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444;
    pOr->protocol = ILWALID_VALUE;
    pOr->ORNumber = orNum;
    //pOr->ORType = LwDispORSettings::ORType::SOR; //Fix this -- assuming a sor here

    if (typeItr != orsetting.MemberEnd())
    {
        assert (std::string(typeItr->value.GetString())==std::string("SOR"));
    //FIXIT: Add support for instantiating other objects later
    }

    if (secItr != orsetting.MemberEnd())
        pOr->pixelDepth = colwertOrPixelDepth(secItr->value.GetString());

    if (polItr != orsetting.MemberEnd())
        pOr->VSyncPolarityNegative = !!polItr->value.GetInt64();

    if (protItr != orsetting.MemberEnd())
        pOr->protocol = colwertOrProtocol(protItr->value.GetString());

    if (pOr->protocol == (LwU32)ILWALID_VALUE)
        return false;

    orProtocolStr = std::string(protItr->value.GetString());

    pOr->Dirty =  true;

    return true;
}

DisplayPanel* CParseJsonDisplaySettings::getDisplayPanelForProtocol(const vector<DisplayPanel*> &pDisplayPanels, std::string &reqProtocolStr, LwU32 displayIdExcludeMask)
{
    std::size_t found;
    for (auto &pDisplayPanel :  pDisplayPanels)
    {
        if (pDisplayPanel->displayId.Get() & displayIdExcludeMask)
            continue;

        found = pDisplayPanel->orProtocol.find(reqProtocolStr);
        if (found != std::string::npos)
        {
            // The DCB entries have a perfect match for the requested protocol
            Printf(Tee::PriError, "%s Assigning displayId: %08x",__FUNCTION__, pDisplayPanel->displayId.Get());
            return pDisplayPanel;
        }
    }

    for (auto &pDisplayPanel :  pDisplayPanels)
    {
        if (pDisplayPanel->displayId.Get() & displayIdExcludeMask)
            continue;

        if ((reqProtocolStr == "TMDS_A" || reqProtocolStr == "TMDS_B") &&
            pDisplayPanel->orProtocol == "DUAL_TMDS")
        {
            return pDisplayPanel;
        }
    }
    return NULL;
}

bool CParseJsonDisplaySettings::parseAndApplyDisplaySettings
(
   const std::string              &configFile,
   const LwDisplay               *pLwDisplay,
   const vector<DisplayPanel*>    &displayPanels,
   vector<DisplayPanel*>          &validDisplayPanels
)
{
    char buffer[64*1024];
    DisplayPanel *pLwrDisplayPanel;
    LwU32  assignedDisplayIds = 0;
    LwDispCoreDisplaySettings *pDispCoreSettings = NULL;

    m_pFp = fopen(configFile.c_str(), "r");
    if (!m_pFp)
    {
        Printf(Tee::PriError, "ERROR: Failed to open file: %s", configFile.c_str()  );
        return false;
    }
    rapidjson::FileReadStream fStream(m_pFp, buffer, sizeof(buffer));
    rapidjson::Document d;
    if (d.ParseStream<0>(fStream).HasParseError())
    {
        Printf(Tee::PriError, "Error parsing the file, makesure config file passes jsonLint" );
        return false;
    }

    assert(d.IsObject());

    if ( !d.HasMember("dispcfgid") ||
        (std::string(d["dispcfgid"].GetString()) != "displaySettingsSection"))
    {
        Printf(Tee::PriError, "ERROR: Incorrect ID value : %s",  d["id"].GetString() );
        return false;
    }

    if (!d.HasMember("headSettingsSection"))
    {
        Printf(Tee::PriError, "ERROR: Head Settings are mandatory and not specified in the configuration file" );
        return false;
    }

    const Value & headSettings = d["headSettingsSection"];
    if (!headSettings.IsArray())
    {
        Printf(Tee::PriError, "ERROR: headSettings has to be an array" );
        return false;
    }

    LwDispCoreChnContext  *pCoreDispContext = NULL;
    if (OK != pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
        (LwDispChnContext**)&pCoreDispContext,
        0,
        Display::ILWALID_HEAD) ||
        !pCoreDispContext)
    {
        Printf(Tee::PriError, "ERROR: Cannot obtain core display context" );
        return false;
    }
    pDispCoreSettings = &pCoreDispContext->DispLwstomSettings;
    validDisplayPanels.clear(); // first clear the results

    for (rapidjson::SizeType i = 0; i < headSettings.Size(); i++)
    {
        const Value & h = headSettings[i];
        pLwrDisplayPanel = NULL;
        LwU32  head = 0;
        if(!h.HasMember("dispcfgtype") || (std::string(h["dispcfgtype"].GetString())) != std::string("head"))
        {
            Printf(Tee::PriError, "ERROR: dispcfgtype needs to be specified and should be set to \"head\"" );
            return false;
        }

        if (h.HasMember("headNumber"))
        {
            head = h["headNumber"].GetInt64();
        }
        else
        {
            Printf(Tee::PriError, "ERROR: headNumber is not specified" );
            return false;
        }
        if (!h.HasMember("headRasterSettingsSection"))
        {
            Printf(Tee::PriError, "ERROR: Raster settings are mandatory" );
            return false;
        }

        std::string crcFileName = "lwdisp_json_cfg_head_";
        crcFileName += std::to_string(head);
        crcFileName += ".xml";
        std::string crcGoldenFileDir="dispinfradata/testgoldens/jsoncfggoldens/";
        if (h.HasMember("crcFileName"))
        {
            crcFileName = h["crcFileName"].GetString();
        }

        if (h.HasMember("crcGoldenFileDir"))
        {
            crcGoldenFileDir = h["crcGoldenFileDir"].GetString();
        }

        const Value & raster = h["headRasterSettingsSection"];
        //Reset the head settings first before populating
        pDispCoreSettings->HeadSettings[head].Reset();

        // Parse the raster settings for the head first
        LwDispRasterSettings *pRasterSettings = &pDispCoreSettings->HeadSettings[head].rasterSettings;
        LwDispPixelClockSettings *pPclkSettings = &pDispCoreSettings->HeadSettings[head].pixelClkSettings;
        if (false == parseRasterSettings(raster,  pRasterSettings, pPclkSettings))
        {
            Printf(Tee::PriError, "ERROR: Failed to parse Raster Settings" );
            return false;
        }

        LwDispViewPortSettings *pHeadVp = &pDispCoreSettings->HeadSettings[head].viewPortSettings;
        LwU32 defaultWidth = pRasterSettings->ActiveX;
        LwU32 defaultHeight = pRasterSettings->ActiveY;
        // Now update the viewport settings with custom values
        if (false == parseHeadViewPortSettings(h, pHeadVp, defaultWidth, defaultHeight))
        {
            Printf(Tee::PriError, "ERROR: Failed to parse head ViewPort Settings" );
            return false;
        }

        if (h.HasMember("orSettingsSection"))
        {
            const Value & orsettings = h["orSettingsSection"];
            std::string orProtocolStr;
            LwU32 orNumber   = 0;

            Value::ConstMemberIterator priItr = orsettings.FindMember("orNumber");
            if (priItr != orsettings.MemberEnd())
                orNumber = priItr->value.GetInt64();
            else
                return false;

            if(!orsettings.HasMember("dispcfgtype") || (std::string(orsettings["dispcfgtype"].GetString())) != std::string("or"))
            {
                Printf(Tee::PriError, "ERROR: dispcfgtype needs to be specified and should be set to \"or\"" );
                return false;
        }

            if (orNumber > 7) // FIXIT
                return false;

            LwDispORSettings *pORSettings = &(pDispCoreSettings->SorSettings[orNumber]);
            if (false == parseOrSettings(orsettings, pORSettings, orProtocolStr, orNumber))
            {
                Printf(Tee::PriError, "ERROR: Failed to parse OR settings" );
                return false;
            }
            else
            {
                pDispCoreSettings->HeadSettings[head].pORSettings = pORSettings;
            }

            //
            // Now that the orSettings are parsed, try to obtain a displayID associated with
            // this protocol
            //
            pLwrDisplayPanel = getDisplayPanelForProtocol(displayPanels, orProtocolStr, assignedDisplayIds);
            if (pLwrDisplayPanel)
            {
                pLwrDisplayPanel->head = head;
                pLwrDisplayPanel->sor = orNumber;
                pLwrDisplayPanel->m_crcFileName = crcFileName;
                pLwrDisplayPanel->m_goldenCrcDir = crcGoldenFileDir;
                pLwrDisplayPanel->resolution.pixelClockHz = pPclkSettings->pixelClkFreqHz;
                pLwrDisplayPanel->pixelDepth = pORSettings->pixelDepth;
                // These are not used during custom modeset but filling them with default values nonetheless
                pLwrDisplayPanel->resolution.width = defaultWidth;
                pLwrDisplayPanel->resolution.height = defaultHeight;
                pLwrDisplayPanel->resolution.depth = 24;
                pLwrDisplayPanel->resolution.refreshrate = 60;

                assignedDisplayIds |= pLwrDisplayPanel->displayId.Get();
                assignedDisplayIds |= pLwrDisplayPanel->secDisplayId.Get();

                validDisplayPanels.push_back(pLwrDisplayPanel);
            }
            else
            {
                Printf(Tee::PriError, "ERROR: Failed to find a corresponding displayId for protocol: %s" , orProtocolStr.c_str() );
                return false;
            }
        }
        else
        {
            Printf(Tee::PriError, "ERROR: Failed to parse OR settings, \"orSettingsSection\" section missing" );
            return false;
        }

        // update the cursor settings
        pLwrDisplayPanel->bValidLwrsorSettings = false;
        if (h.HasMember("lwrsorSettingsSection"))
        {
            const Value & cursor = h["lwrsorSettingsSection"];
            LwDispLwrsorSettings    *pLwrsor = &pLwrDisplayPanel->lwrsorSettings;
            LwDispSurfaceParams     *pLwrsorSurfParams = &pLwrDisplayPanel->lwrsorSurfParams;

            if(!cursor.HasMember("dispcfgtype") || (std::string(cursor["dispcfgtype"].GetString())) != std::string("cursor"))
            {
                Printf(Tee::PriError, "ERROR: dispcfgtype needs to be specified and should be set to \"cursor\"" );
                return false;
            }

            if (false == parseLwrsorSettings(cursor, pLwrsor, pLwrsorSurfParams))
            {
                Printf(Tee::PriError, "ERROR: Failed to parse cursor settings" );
                return false;
            }
            pLwrDisplayPanel->bValidLwrsorSettings = true;
        }

        // Now look for all the windows assigned to this head
        if (h.HasMember("windowSettingsSection"))
        {
            const Value & windowSettings = h["windowSettingsSection"];
            for (rapidjson::SizeType j = 0; j < windowSettings.Size(); j++)
            {
                const Value & window = windowSettings[j];
                LwU32 winId = 32; //TODO: Fixit

                if(!window.HasMember("dispcfgtype") || (std::string(window["dispcfgtype"].GetString())) != std::string("window"))
                {
                    Printf(Tee::PriError, "ERROR: dispcfgtype needs to be specified and should be set to \"window\"" );
                    return false;
                }

                if (window.HasMember("windowNumber"))
                {
                    winId = window["windowNumber"].GetInt64();
                    if (winId > MAX_WINDOWS)
                    {
                        Printf(Tee::PriError, "ERROR: Incorrect windowId %d", winId );
                        return false;
                    }

                    if (!(1<<winId & m_allocatedWindows))
                    {
                        m_allocatedWindows |= (1<<winId);
                    }
                    else
                    {
                        Printf(Tee::PriError, "ERROR: windowNumber is already used, should be unique (%d)", winId );
                        return false;
                    }
                }
                else
                {
                    Printf(Tee::PriError, "ERROR: windowNumber is not specified" );
                    return false;
                }

                assert(pLwrDisplayPanel);
                WindowSettings *pWinSettings = pLwrDisplayPanel->allocWindowSettings();
                if (!pWinSettings)
                {
                    Printf(Tee::PriError, "ERROR: Failed to allocate memory for window settings" );
                    return false;
                }
                LwDispWindowSettings *pWin = &pWinSettings->winSettings;
                LwDispSurfaceParams  *pSurfParams = &pWinSettings->surfSettings;
                pWin->Reset();
                // Using winIndex as a placeholder for actual window instance
                pWin->winIndex = winId;
                if (false == parseWindowSettings(window,
                                                 pWin,
                                                 pSurfParams,
                                                 defaultWidth,
                                                 defaultHeight))
                {
                    Printf(Tee::PriError, "ERROR: Failed to parse window settings " );
                    return false;
                }

            }
        }
    }
    return true;
}

CParseJsonDisplaySettings::~CParseJsonDisplaySettings()
{
    if (m_pFp)
        fclose(m_pFp);
}
