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

#pragma once

#include "core/include/display.h"
#include <vector>

class GsyncDevice;
using GsyncDevices = std::vector<GsyncDevice>;

RC EvoDispSetProcampSettings(Display::ORColorSpace cs);

namespace GsyncDisplayHelper
{
typedef struct 
{
    Display::ORColorSpace cs;
    HeadIndex headIdx;
}CBArgs;
RC SetMode
(
    const GsyncDevice& gsyncDev,
    UINT32 width,
    UINT32 height,
    UINT32 refRate,
    UINT32 depth,
    Display::ORColorSpace cs 
);
RC SetMode
(
    const GsyncDevices& gsyncDevs,
    UINT32 width,
    UINT32 height,
    UINT32 refRate,
    UINT32 depth,
    Display::ORColorSpace cs
);
RC DetachAllDisplays(const GsyncDevices& gsyncDevs);
RC DetachAllDisplays(const GsyncDevice& gsyncDev);
RC DisplayImage(const GsyncDevice& gsyncDev, Surface2D *pSurf);
RC AssignSor
(
    const GsyncDevice& gsyncDev,
    const OrIndex orIndex,
    Display::AssignSorSetting headMode,
    vector<UINT32>* pOutSorList,
    OrIndex* pOutSorIdx
);
};
