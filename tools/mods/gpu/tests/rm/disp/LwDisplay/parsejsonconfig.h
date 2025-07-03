/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _PARSE_DISPSETTINGS_H_
#define _PARSE_DISPSETTINGS_H_

#include "document.h"
#include "filereadstream.h"
#include "modesetlib.h"
#ifndef INCLUDED_LWDISP_CCTX_H
#include "lwdisp_cctx.h"
#endif

using namespace rapidjson;
using namespace Modesetlib;
using namespace std;

class CParseJsonDisplaySettings
{
private:
    std::string configFile;
    FILE *m_pFp;
    LwU32 m_availableWindows;
    LwU32 m_allocatedWindows;

private:
    bool parseRasterSettings(const rapidjson::Value & raster, LwDispRasterSettings *, LwDispPixelClockSettings *);
    bool parseWindowCompositionSettings(const rapidjson::Value & window, LwDispWindowSettings *pWin);
    //bool parseWindowSurface(const rapidjson::Value & window, LwDispSurfaceParams *pWinSurface, LwU32 defaultWidth, LwU32 defaultHeight);
    bool  parseWindowSettings(const rapidjson::Value & window, LwDispWindowSettings *pWin, LwDispSurfaceParams *pWinSurface, LwU32 defaultWidth, LwU32 defaultHeight);
    LwDispCompFactor::MatchSelect colwertWindowMatchSelect(std::string matchSelect);
    ColorUtils::Format colwertSurfaceFormat(std::string fmt);
    Surface2D::Layout colwertSurfaceLayout(std::string fmt);
    bool parseWindowSurface(const rapidjson::Value & window, LwDispSurfaceParams *pWinSurface, LwU32 defaultWidth, LwU32 defaultHeight);
    bool parseHeadViewPortSettings(const rapidjson::Value & hViewPort, LwDispViewPortSettings *pHeadVP, LwU32 defaultWidth, LwU32 defaultHeight);
    bool parseLwrsorSettings(const rapidjson::Value & cursor, LwDispLwrsorSettings *pLwrsor, LwDispSurfaceParams *pLwrsorSurface);
    LwDispORSettings::ORType colwertOrType(std::string fmt);
    LwU32 colwertOrProtocol(std::string fmt);
    LwDispORSettings::PixelDepth colwertOrPixelDepth(std::string fmt);
    Surface2D::InputRange colwertSurfaceInputRange(std::string fmt);
    bool parseOrSettings(const rapidjson::Value & orsetting, LwDispORSettings *pOr, std::string &orProtocolStr, LwU32 orNum);
    DisplayPanel* getDisplayPanelForProtocol(const vector<DisplayPanel*> &pDisplayPanels, std::string &reqProtocolStr, LwU32 displayIdExcludeMask);

public:
    CParseJsonDisplaySettings():
        m_pFp(nullptr), m_availableWindows(0xff), m_allocatedWindows(0) {}
    bool parseAndApplyDisplaySettings(const std::string &configFile, const LwDisplay *pLwDisplay, const vector<DisplayPanel*> &displayPanels, vector<DisplayPanel*>&validDisplayPanels);
    //bool parseDisplaySettings(DISPLAYSETTINGS *dSettings);
    ~CParseJsonDisplaySettings();
};

#ifndef ILWALID_VALUE
#define ILWALID_VALUE ~0
#endif

#endif //_PARSE_DISPSETTINGS_H_

