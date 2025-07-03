/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2007,2014-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CHIPSET_H
#define INCLUDED_CHIPSET_H

#ifndef INCLUDED_RC_H
    #include "core/include/rc.h"
#endif
#ifndef INCLUDED_FRAMEBUF_H
    #include "core/include/framebuf.h"
#endif

#define MAX_NUM_CHIPS 2

namespace Chipset
{
    enum ChipVersion
    {
        INVALID = 0
        ,FIRST_MCP
        ,MCP1
        ,MCP2
        ,CK8
        ,MCP2S
        ,CK8S
        ,MCP04
        ,CK804
        ,MCP51
        ,MCP51W
        ,MCP55
        ,MCP61
        ,MCP65
        ,MCP67
        ,MCP73
        ,MCP77
        ,MCP79
        ,MCP89
        ,MCP8D
        ,LAST_MCP
        ,INTEL
        ,AMD
        ,FIRST_CRUSH
        ,C11
        ,C12
        ,C17
        ,C18
        ,C19
        ,C51
        ,C55
        ,T65
        ,C73
        ,LAST_CRUSH
        ,ANY_CRUSH
    };

    // These should be alphabetical (except for "NONE")
    enum ChipSku
    {
        NONE
        ,D
        ,G
        ,M
        ,MV
        ,PRO
        ,PV
        ,PVG
        ,SLV
        ,V
        ,UNKNOWN
    };

     static const string McpChipName [] =
     {
             "INVALID"
            ,"FIRST_MCP"
            ,"MCP1"
            ,"MCP2"
            ,"CK8"
            ,"MCP2S"
            ,"CK8S"
            ,"MCP04"
            ,"CK804"
            ,"MCP51"
            ,"MCP51W"
            ,"MCP55"
            ,"MCP61"
            ,"MCP65"
            ,"MCP67"
            ,"MCP73"
            ,"MCP77"
            ,"MCP79"
            ,"MCP8AD"
            ,"MCP8AS"
            ,"LAST_MCP"

            ,"FIRST_CRUSH"
            ,"C11"
            ,"C12"
            ,"C17"
            ,"C18"
            ,"C19"
            ,"C51"
            ,"C55"
            ,"T65"
            ,"C73"
            ,"LAST_CRUSH"
            ,"ANY_CRUSH"

     };

    RC Initialize();
    RC Uninitialize();
    RC GetChipInfo(UINT32 index, ChipVersion* version = NULL, ChipSku* sku = NULL,
                   INT32* pDomain = NULL, INT32* pBus = NULL, INT32* pDev = NULL);
    RC SetChipInfo(UINT32 index, ChipVersion version, ChipSku sku);
    ChipVersion GetChipVersion(UINT32 index);
    ChipSku GetChipSku(UINT32 index);
    RC GetNumberOfChips(UINT32* nChips);
    RC ReadChipId   (UINT32 * fuseVal240, UINT32 * fuseVal244);

    RC ReadMCP89ChipId(UINT32* FabCode,
                       UINT32* LotCode0,
                       UINT32* LotCode1,
                       INT32*  XCord,
                       UINT32* YCord,
                       UINT32* WaferId);

    RC GetCookedLotCode(UINT32 LotCode, string& CookedLotCode);
    INT32 GetXCoordCooked(INT32 XCoord);
};

#endif // INCLUDED_CHIPSET_H

