/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>

#include "core/include/display.h"
#include "gpu/display/lwdisplay/lwdisp_cctx.h"
#include "gpu/display/dpc/dpc_config_gen.h"
#include "gpu/display/dpc/dpc_file_parser.h"

#include "document.h"
#include "writer.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include "gmock/gmock.h"

using namespace std;
using namespace testing;
using namespace rapidjson;
using namespace DispPipeConfig;

using ResolutionString = string;

TEST(DpcConfigGeneratorTester, AddsDisplayId)
{
    DisplayIDs displayIDs;
    displayIDs.push_back(0x100);
    displayIDs.push_back(0x200);

    DisplayIDs displayIDsDualTmds;
    displayIDsDualTmds.push_back(0x800);

    HeadMask headMask = 0x1;

    Display::SorConnectorsDescription connectorsDescription;
    Display::SorConnectorDescription desc1;
    desc1.padlinks = 0x1;
    Display::SorConnectorDescription desc2;
    desc2.padlinks = 0x2;
    Display::SorConnectorDescription desc3;
    desc3.padlinks = 0x3;

    connectorsDescription[0x100] = desc1;
    connectorsDescription[0x200] = desc2;
    connectorsDescription[0x800] = desc3;

    DpcConfigGenerator generator(connectorsDescription);

    UINT32 configIndex = 0;
    generator.AddDisplayParallel(displayIDs, headMask, Display::ILWALID_OR_INDEX,
        CrcHandler::CRC_DP_DUAL_SST, "1024x768x32x60", &configIndex);
    generator.AddWindowFrame(configIndex, 0,
        0, 0, 128, 32, "random", ColorUtils::A8B8G8R8);
    generator.AddWindowFrame(configIndex, 0, 0, 1, "random");

    generator.AddDisplayParallel(displayIDsDualTmds, headMask, Display::ILWALID_OR_INDEX,
        CrcHandler::CRC_TMDS_DUAL, "1024x768x32x60", &configIndex);
    generator.AddWindowFrame(configIndex, 0, 1, 0, 128, 32, "random", ColorUtils::A8B8G8R8);
    generator.AddWindowFrame(configIndex, 0, 1, 1, "random");

    Printf(Tee::PriNormal, "%s\n", generator.GetDocString(true).c_str());

    Document d = std::move(generator.GetDpcDocRef()); // Copying Document is not allowed

    ASSERT_THAT(d[DPCFileParser::JSON_PARAM_CFGS].Size(), 2);
}
