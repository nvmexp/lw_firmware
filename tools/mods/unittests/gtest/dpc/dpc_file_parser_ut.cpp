/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>

#include "gmock/gmock.h"

#include "dpc_file_parser_internal.h"
#include "displayport.h"

using namespace std;
using namespace DispPipeConfig;
using namespace rapidjson;
using namespace testing;
using namespace DisplayPort;
//------------------------------------------------------------------------------
// High level tests
//------------------------------------------------------------------------------
class DPCTester : public Test
{
public:
    DPCFileParser dispPipeConfigParser;

    string validJson = R"(
    {
        "rasterLockHeads": {
            "0":[1,2],
            "3":[]
        },
        "templates": {},
        "cfgs": [{
            "heads": [1, 2],
            "orIndex": 2,
            "passThrough": false,
            "crcs": [{
                    "dual_tmds": ["a", "b"],
                    "primary_crc": "0xABCD"
                },
                {
                    "dp_sst": ["c"],
                    "primary_crc": "0xABCD"
                },
                {
                    "rg_crc": "0xABCD"
                },
                {
                    "comp_crc": "0xABCD"
                }
            ],
            "oRPixelDepth" : "BPP_36_444",
            "colorSpace"   : "RGB",
            "timings": {
                "horizontal": {
                    "hActive": 100,
                    "hFrontPorch": 4,
                    "hSync": 3,
                    "hBackPorch": 2,
                    "hSyncPolarityNegative" : false
                },
                "vertical": {
                    "vActive": 100,
                    "vFrontPorch": 4,
                    "vSync": 3,
                    "vBackPorch": 2,
                    "vSyncPolarityNegative" : false
                },
                "pclkHz": 10000000
            },
            "viewPortIn": {
                "xPos": 0,
                "yPos": 0,
                "xSize": 100,
                "ySize": 100
            },
            "viewPortOut": {
                "xPos": 0,
                "yPos": 0,
                "xSize": 100,
                "ySize": 100
            },
            "lwrsorSlaveHead": {
                "xPos": 3,
                "yPos": 4,
                "size": 32,
                "format": "A8R8G8B8",
                "pattern": "box crosshatched unused"
            },
            "cursor": {
                "xPos": 3,
                "yPos": 4,
                "size": 32,
                "format": "A8R8G8B8",
                "pattern": "box crosshatched unused"
            },
            "windows": {
                "0": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    },
                    "compositionRectOut": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    },
                    "rotate:flip": "180:hor_ver"
                },
                "1": {
                    "width": 100,
                    "height": 300,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    },
                    "compositionRectOut": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    }
                }
            }
        }]
    }
    )";

    string validJsonMinimal = R"(
    {
        "templates": {},
        "cfgs": [{
            "heads": [0],
            "crcs": [{
                    "dual_tmds": []
                }
            ],
            "timings": {
                "horizontal": {
                    "hActive": 100,
                    "hFrontPorch": 4,
                    "hSync": 3,
                    "hBackPorch": 2
                },
                "vertical": {
                    "vActive": 200,
                    "vFrontPorch": 4,
                    "vSync": 3,
                    "vBackPorch": 2
                },
                "pclkHz": 10000000
            },
            "cursor": {
                "size": 32,
                "format": "A8R8G8B8",
                "pattern": "box crosshatched unused"
            },
            "windows": {
                "0": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                },
                "1": {
                    "width": 100,
                    "height": 200,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                }
            }
        }]
    }
    )";

    string validJsonMinimalWithResolution = R"(
    {
        "cfgs": [{
            "heads": [0],
            "crcs": [{
                    "dual_tmds": []
                }
            ],
            "timings": {
                "resolution": "1600x1200x24x60"
            },
            "windows": {
                "0": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                }
            }
        }]
    }
    )";
    RC ValidateFile(const string& fileName);
};

RC DPCTester::ValidateFile(const string& fileName)
{
    RC rc;
    DPCFileParser dispPipeConfigParser;

    CHECK_RC(dispPipeConfigParser.Load(fileName));
    if (!dispPipeConfigParser.IsLoadedConfigValid())
        return RC::CANNOT_ENUMERATE_OBJECT;

    CHECK_RC(dispPipeConfigParser.Parse());

    return OK;
}

TEST_F (DPCTester, ReturnsErrorIfJsonConfigLoadFails)
{
    std::string filePath = "./not_available.dpc";

    ASSERT_THAT(dispPipeConfigParser.Load(filePath), RC::CANNOT_OPEN_FILE);
}

TEST_F (DPCTester, ValidatesDpcFilesInPerforce)
{
    vector<string> dpcFilesInPerforce = {
        "200x80_fmodel.dpc",
        "256x48_minimal.dpc",
        "400x80_dsc_fmodel.dpc",
        "5120x2880_5K_Dual_SST.dpc",
        "Test46_StressModes_TMDS_A.dpc",
        "Test46_StressModes_DP_SST.dpc",
        "1920x1080_2Head1Or.dpc"
    };

    const char *runspacePath = getelw("MODS_RUNSPACE");
    for (const auto &fileName: dpcFilesInPerforce)
    {
        string filePath = runspacePath != nullptr ?
            Utility::StrPrintf("%s/%s", runspacePath, fileName.c_str()) : fileName;
        EXPECT_THAT(ValidateFile(filePath), OK);
    }
}

TEST_F (DPCTester, ParseFailsIfFileNotLoaded)
{
    ASSERT_THAT(dispPipeConfigParser.Parse(), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST_F (DPCTester, OnParseValidatesJsonHasCfgEntry)
{
    dispPipeConfigParser.SetJsonDolwment(validJson);

    ASSERT_THAT(dispPipeConfigParser.Parse(), OK);

    auto cfgEntriesMap = dispPipeConfigParser.GetCfgEntriesMap();
    ASSERT_THAT(cfgEntriesMap.size(), 1);
    auto it = cfgEntriesMap.find(1);
    ASSERT_NE(it, cfgEntriesMap.end());
}

TEST_F (DPCTester, OnParseValidatesJsonHasOptionalKeyTemplates)
{
    dispPipeConfigParser.SetJsonDolwment(validJson);

    ASSERT_THAT(dispPipeConfigParser.Parse(), OK);
}

TEST_F (DPCTester, OnParseValidatesJsonTopLevelHasNoUnsupportedKey)
{
    std::string ilwalidTopLevelJson =
        R"({"cfgs" : [{}], "templates" : {}, "dpSinksCaps": {}, "unsupported" : {}})";
    dispPipeConfigParser.SetJsonDolwment(ilwalidTopLevelJson);

    ASSERT_THAT(dispPipeConfigParser.Parse(), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST_F (DPCTester, ParseFailsOnEmptyCfgs)
{
    std::string ilwalidTopLevelJson = R"({"cfgs" : [], "templates" : {}})";
    dispPipeConfigParser.SetJsonDolwment(ilwalidTopLevelJson);

    ASSERT_THAT(dispPipeConfigParser.Parse(), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST_F (DPCTester, OnParseSuccessFillsCfgEntries)
{
    dispPipeConfigParser.SetJsonDolwment(validJson);

    ASSERT_THAT(dispPipeConfigParser.Parse(), OK);
    ASSERT_THAT(dispPipeConfigParser.GetCfgEntriesMap().size(), 1);

    auto cfgEntriesMap = dispPipeConfigParser.GetCfgEntriesMap();
    auto cfgEntriesIt = cfgEntriesMap.find(1);
    ASSERT_NE(cfgEntriesIt, cfgEntriesMap.end());
    if (cfgEntriesIt != cfgEntriesMap.end())
    {
        ASSERT_THAT(cfgEntriesIt->second.size(), 4);
    }
}

TEST_F (DPCTester, ValidateRasterLockMasterHead)
{
    string rasterLockHeads = R"({
        "rasterLockHeadsNonArray": 0,
        "rasterLockIlwalidMasterHead": {
            "4":[1,2]
        },
        "rasterLockSlaveHeadsNonArray": {
            "0":2
        },
        "rasterLockIlwalidSlaveHead": {
            "0":[5]
        },
        "rasterLockSameHeadMultiEntries1": {
            "0":[1],
            "0":[2]
        },
        "rasterLockSameHeadMultiEntries2": {
            "0":[1],
            "2":[1]
        },
        "rasterLockSameHeadMultiEntries3": {
            "0":[1],
            "2":[0]
        },
        "rasterLockSameHeadMultiEntries4": {
            "0":[0]
        },
        "rasterLockSameHeadMultiEntries5": {
            "0":[1, 1]
        },
        "rasterLockHeadsValid": {
            "0":[1,2],
            "3":[]
        }
    })";

    Document root;
    root.Parse(rasterLockHeads.c_str());

    JsonParserRasterLockHeads rasterLockHeadsParser;
    RasterLockHeadsMap rasterLockHeadsMap;

    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockHeadsNonArray"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockIlwalidMasterHead"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockSlaveHeadsNonArray"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockIlwalidSlaveHead"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockSameHeadMultiEntries1"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockSameHeadMultiEntries2"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockSameHeadMultiEntries3"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockSameHeadMultiEntries4"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockSameHeadMultiEntries5"],
        &rasterLockHeadsMap), RC::CANNOT_ENUMERATE_OBJECT);

    rasterLockHeadsMap = { };
    ASSERT_THAT(rasterLockHeadsParser.Parse(root["rasterLockHeadsValid"],
        &rasterLockHeadsMap), RC::OK);

    dispPipeConfigParser.SetJsonDolwment(validJson);
    ASSERT_THAT(dispPipeConfigParser.Parse(), RC::OK);
    rasterLockHeadsMap = dispPipeConfigParser.GetRasterLockHeadsMap();

    ASSERT_THAT(rasterLockHeadsMap.size(), 2);
    ASSERT_THAT(rasterLockHeadsMap.count(0), 1);
    ASSERT_THAT(rasterLockHeadsMap.count(1), 0);
    ASSERT_THAT(rasterLockHeadsMap.count(2), 0);
    ASSERT_THAT(rasterLockHeadsMap.count(3), 1);
}

//------------------------------------------------------------------------------
// CfgFieldParserTester
//------------------------------------------------------------------------------
TEST (CfgFieldParserTester, ValidateFullParsing)
{
    string ilwalidCfgEntriesJson =
            R"({"cfgs" : [{"protocol" : "tmds_a", "padlinks" : [], "crc" : "0xABCD"},
                          {"protocol" : "tmds_a", "padlinks" : [], "crc" : "0xABCD", "extra": 0}],
                "templates" : {}})";

    Document root;
    root.Parse(ilwalidCfgEntriesJson.c_str());

    JsonParserCfgs cfgFieldTester;

    DPCCfgEntry entry;
    ASSERT_THAT(cfgFieldTester.ParseSingleCfg(0, root["cfgs"][0], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseSingleCfg(1, root["cfgs"][1], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST_F (DPCTester, ParseForSuccessWithoutViewPorts)
{
    dispPipeConfigParser.SetJsonDolwment(validJsonMinimal);

    ASSERT_THAT(dispPipeConfigParser.Parse(), OK);
    ASSERT_THAT(dispPipeConfigParser.GetCfgEntriesMap().size(), 1);

    auto cfgEntriesMap = dispPipeConfigParser.GetCfgEntriesMap();
    auto cfgEntriesIt = cfgEntriesMap.find(0);
    ASSERT_NE(cfgEntriesIt, cfgEntriesMap.end());
    if (cfgEntriesIt != cfgEntriesMap.end())
    {
        ASSERT_THAT(cfgEntriesIt->second.size(), 1);
    }

    DPCCfgEntry cfgEntry = cfgEntriesIt->second[0];

    // Parsing should succeed. Values will be updated by DpcConfigValidator
    LwDispViewPortSettings::ViewPort expectedViewPort(0, 0, 0, 0);

    EXPECT_THAT(cfgEntry.m_ViewPortIn, expectedViewPort);
    EXPECT_THAT(cfgEntry.m_ViewPortOut, expectedViewPort);
}

TEST_F (DPCTester, ParseForSuccessWithoutViewPortsWithResolution)
{
    dispPipeConfigParser.SetJsonDolwment(validJsonMinimalWithResolution);

    ASSERT_THAT(dispPipeConfigParser.Parse(), OK);
    ASSERT_THAT(dispPipeConfigParser.GetCfgEntriesMap().size(), 1);

    auto cfgEntriesMap = dispPipeConfigParser.GetCfgEntriesMap();
    auto cfgEntriesIt = cfgEntriesMap.find(0);
    ASSERT_NE(cfgEntriesIt, cfgEntriesMap.end());
    if (cfgEntriesIt != cfgEntriesMap.end())
    {
        ASSERT_THAT(cfgEntriesIt->second.size(), 1);
    }

    DPCCfgEntry cfgEntry = cfgEntriesIt->second[0];

    // Parsing should succeed. Values will be updated by DpcConfigValidator
    LwDispViewPortSettings::ViewPort expectedViewPort(0, 0, 0, 0);

    EXPECT_THAT(cfgEntry.m_ViewPortIn, expectedViewPort);
    EXPECT_THAT(cfgEntry.m_ViewPortOut, expectedViewPort);
}


TEST (CfgFieldParserTester, ValidatesFieldHeads)
{
    string headsJsonStr = R"(
        {"headsNonArray" : 1234,
         "headsEmptyArray":[],
         "headsIlwalidEntries":[2, "a"],
         "headsIlwalidHeadIndex":[4],
         "headsValid":[1, 2]})";

    Document root;
    root.Parse(headsJsonStr.c_str());

    JsonParserCfgs cfgFieldTester;
    DPCCfgEntry entry;
    ASSERT_THAT(cfgFieldTester.ParseHeads(root["headsNonArray"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseHeads(root["headsEmptyArray"], &entry),
         RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseHeads(root["headsIlwalidEntries"], &entry),
         RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseHeads(root["headsIlwalidHeadIndex"], &entry),
         RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(cfgFieldTester.ParseHeads(root["headsValid"], &entry),
         OK);
    ASSERT_THAT(entry.m_Heads, (1<<1 | 1<<2));
}

TEST (CfgFieldParserTester, ValidatesFieldOrIndex)
{
    string orIndexJsonStr = R"({"orIndexNonNumeric":[], "orIndexOOR":4, "orIndexValid":2})";

    Document root;
    root.Parse(orIndexJsonStr.c_str());

    JsonParserCfgs cfgFieldTester;
    DPCCfgEntry entry;
    ASSERT_THAT(cfgFieldTester.ParseOrIndex(root["orIndexNonNumeric"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseOrIndex(root["orIndexOOR"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseOrIndex(root["orIndexValid"], &entry),
        OK);
    ASSERT_THAT(entry.m_OrIndex, 2);
}

TEST (CfgFieldParserTester, ValidatesFieldsOutputScalerTaps)
{
    string outputScalerTapsJsonStr = R"(
        {"outputScalerTapsNonNumeric":[],
         "outputScalerTapsOOR":1,
         "outputScalerTapsValid2":2,
         "outputScalerTapsValid5":5
        })";

    Document root;
    root.Parse(outputScalerTapsJsonStr.c_str());

    JsonParserCfgs cfgFieldTester;
    DPCCfgEntry entry;
    ASSERT_THAT(cfgFieldTester.ParseOutputScalerVTaps(root["outputScalerTapsNonNumeric"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseOutputScalerHTaps(root["outputScalerTapsNonNumeric"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseOutputScalerVTaps(root["outputScalerTapsOOR"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseOutputScalerHTaps(root["outputScalerTapsOOR"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(cfgFieldTester.ParseOutputScalerVTaps(root["outputScalerTapsValid2"], &entry),
        OK);
    ASSERT_THAT(entry.m_VTaps, LwDispScalerSettings::VTAPS::TAPS_2);

    ASSERT_THAT(cfgFieldTester.ParseOutputScalerHTaps(root["outputScalerTapsValid2"], &entry),
        OK);
    ASSERT_THAT(entry.m_HTaps, LwDispScalerSettings::HTAPS::TAPS_2);

    ASSERT_THAT(cfgFieldTester.ParseOutputScalerVTaps(root["outputScalerTapsValid5"], &entry),
        OK);
    ASSERT_THAT(entry.m_VTaps, LwDispScalerSettings::VTAPS::TAPS_5);

    ASSERT_THAT(cfgFieldTester.ParseOutputScalerHTaps(root["outputScalerTapsValid5"], &entry),
        OK);
    ASSERT_THAT(entry.m_HTaps, LwDispScalerSettings::HTAPS::TAPS_5);
}

TEST (CfgFieldParserTester, ValidatesFieldPassThrough)
{
    string passthroughJsonStr = R"({"passThroughNonBool":[], "passThroughValid" : true})";

    Document root;
    root.Parse(passthroughJsonStr.c_str());

    JsonParserCfgs cfgFieldTester;
    DPCCfgEntry entry;
    ASSERT_THAT(cfgFieldTester.ParsePassthrough(root["passThroughNonBool"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParsePassthrough(root["passThroughValid"], &entry),
        OK);
    ASSERT_THAT(entry.m_IsPassThroughMode, true);
}

TEST (CfgFieldParserTester, ValidatesORColorSpace)
{
    string colorSpaceJsonStr = R"(
        {"csIlwalid" : 1234,
         "csEmptyArray" : [],
         "csIlwalidString": "yvu601",
         "csValid": "yuv709"})";

    Document root;
    root.Parse(colorSpaceJsonStr.c_str());

    JsonParserCfgs cfgFieldTester;
    DPCCfgEntry entry;
    ASSERT_THAT(cfgFieldTester.ParseColorSpace(root["csIlwalid"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseColorSpace(root["csEmptyArray"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseColorSpace(root["csIlwalidString"], &entry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseColorSpace(root["csValid"], &entry),
        OK);
}

TEST (CfgFieldParserTester, ValidatesORPixelDepth)
{
    string oRPixelDepthJsonStr = R"(
        { "depthIlwalid" : 777,
          "depthEmptyArray" : [],
          "depthIlwalidString" : "BPP_777_444",
          "depthValid" : "BPP_36_444"})";
    Document root;
    root.Parse(oRPixelDepthJsonStr.c_str());

    JsonParserCfgs cfgFieldTester;
    DPCCfgEntry entry;
    ASSERT_THAT(cfgFieldTester.ParseORPixelDepth(root["depthIlwalid"], &entry),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseORPixelDepth(root["depthEmptyArray"], &entry),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseORPixelDepth(root["depthIlwalidString"], &entry),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldTester.ParseORPixelDepth(root["depthValid"], &entry),
            RC::OK);
}

TEST(CfgFieldParserTest, ValidatesFieldWindows)
{
    string windowsJsonStr = R"({"windowsNonObject": 1234, "windowsEmptyObject": {},
            "windowsNonNumericKey": {"A": {}}})";
    Document root;
    root.Parse(windowsJsonStr.c_str());

    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windowsNonObject"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windowsEmptyObject"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windowsNonNumericKey"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);

    string validWindowsJsonStr = R"(
        {
            "windows": {
                "0": {
                    "ownerHead" : 0,
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "inputScalerVTaps" : 5,
                    "inputScalerHTaps" : 5,
                    "displayGamutTypeForTMOLut" : "SDR",
                    "surfaceRectIn": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    },
                    "compositionRectOut": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    }
                },
                "1": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    },
                    "compositionRectOut": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 100,
                        "ySize": 100
                    }
                }
            }
        }
    )";
    root.Parse(validWindowsJsonStr.c_str());

    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
        OK);
    ASSERT_THAT(dispCfgEntry.m_Windows.size(), 2);
}

TEST(WindowWithFrames, MismatchedFrameCount)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string missingFrameJsonStr = R"(
        {
            "windows": {
                "0:0": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                },
                "0:2": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                },
                "1:0": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                },
                "1:1": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                }
            },
            "windowsAnotherSet": {
                "0:0": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                },
                "0:2": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                }
            }
        }
    )";
    root.Parse(missingFrameJsonStr.c_str());

    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windowsAnotherSet"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);

}

TEST(WindowWithFrames, IlwalidFrameIndex)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string missingFrameJsonStr = R"(
        {
            "windows": {
                "0:K": {
                    "width": 10,
                    "height": 3,
                    "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical"
                }
            }
        }
    )";
    root.Parse(missingFrameJsonStr.c_str());

    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ImmediateFlip, ValidatesStringForFrame)
{
    JsonParserWindows windowsObjectParser;

    string windowsJsonStr = R"(
           {
            "windowValidImmediateFlip" : "immediateFlip",
            "windowIlwalidImmediateFlip" : "anything"
            }
    )";
    Document root;
    root.Parse(windowsJsonStr.c_str());

    Window windowDefinition;

    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidImmediateFlip"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidImmediateFlip"],
        &windowDefinition), RC::OK);
}

TEST(ImmediateFlip, FirstFrameAsImmediateFlipIsIlwalid)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string jsonStr = R"(
        {
            "windows": {
                "0:0": "immediateFlip"
            }
        }
    )";
    root.Parse(jsonStr.c_str());

    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ImmediateFlip, NonRGBPrevFrameIsIlwalid)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string jsonStr = R"(
        {
            "windows": {
                "0:0": {
                     "colorSpace" : "YUV601",
                     "format": "Y8___U8V8_N444",
                     "pattern": "gradient color_palette vertical"
                },
                "0:1": "immediateFlip"
            }
        }
    )";
    root.Parse(jsonStr.c_str());

    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ImmediateFlip, RGBPrevFrameIsValid)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string jsonStrValid = R"(
           {
               "windows": {
                   "0:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "0:1": "immediateFlip"
               }
           }
       )";
    root.Parse(jsonStrValid.c_str());
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
           RC::OK);
}

TEST(ImmediateFlipWithMultipleWindows, IlwalidBeginModeSpecified)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string jsonStrIlwalid = R"(
           {
               "windows": {
                   "0:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "0:1": "immediateFlip",
                   "0:2": "immediateFlip",
                   "1:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "1:1": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "1:2": "immediateFlip"
               }
           }
       )";
    root.Parse(jsonStrIlwalid.c_str());
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
           RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ImmediateFlipWithMultipleWindows, IlwalidBeginModeSpecified1)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string jsonStrIlwalid = R"(
           {
               "windows": {
                   "0:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "0:1": "immediateFlip",
                   "1:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "1:1": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   }
               }
           }
       )";
    root.Parse(jsonStrIlwalid.c_str());
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
           RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ImmediateFlipWithMultipleWindows, ValidBeginModeSpecified)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string jsonStrValid = R"(
           {
               "windows": {
                   "0:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "0:1": "immediateFlip",
                   "0:2": "immediateFlip",
                   "1:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "1:1": "immediateFlip",
                   "1:2": "immediateFlip"
               }
           }
       )";
    root.Parse(jsonStrValid.c_str());
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windows"], &dispCfgEntry),
           RC::OK);
}


TEST(Frame0SpecificKeys, ShouldNotBePresentOnOtherFrames)
{
    Document root;
    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;

    string jsonStrIlwalid = R"(
           {
               "windowsIlwalidKeyOwnerHead": {
                   "0:0": {
                        "colorSpace" : "RGB",
                        "format": "A8R8G8B8",
                        "pattern": "gradient color_palette vertical"
                   },
                   "0:1": {
                        "ownerHead" : 2,
                        "pattern": "gradient color_palette vertical"
                   }
               }
           }
       )";
    root.Parse(jsonStrIlwalid.c_str());
    ASSERT_THAT(cfgFieldParser.ParseWindows(root["windowsIlwalidKeyOwnerHead"], &dispCfgEntry),
           RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(CfgFieldParserTest, ValidatesTimingsField)
{
    string timingJsonStr = R"(
        {
        "timingsNotObject": [],
        "timingsValid":
            {"vertical": {"vActive": 10, "vFrontPorch": 4, "vSync": 3, "vBackPorch": 8, 
                          "vSyncPolarityNegative" : true},
             "pclkHz" : 111,
             "horizontal": {"hActive": 10, "hFrontPorch": 4, "hSync": 3, "hBackPorch": 8,
                            "hSyncPolarityNegative" : true}},
        "timingsVerticalMissing":
        {"horizontal": {"hActive": 10, "hFrontPorch": 4, "hSync": 3, "hBackPorch": 8},
             "pclkHz" : 111},
        "timingsHorizontalMissing":
            {"vertical": {"vActive": 10, "vFrontPorch": 4, "vSync": 3, "vBackPorch": 8},
             "pclkHz" : 111},
        "timingsVerticalNotObject":
            {"vertical": [],
             "horizontal": {"hActive": 10, "hFrontPorch": 4, "hSync": 3, "hBackPorch": 8},
              "pclkHz" : 111},
        "timingsHorizontalNotObject":
            {"vertical": {"vActive": 10, "vFrontPorch": 4, "vSync": 3, "vBackPorch": 8},
             "pclkHz" : 111,
             "horizontal": []},
        "timingsExtraField":
            {"vertical": {"vActive": 10, "vFrontPorch": 4, "vSync": 3, "vBackPorch": 8},
             "pclkHz" : 111,
             "extra" : [],
             "horizontal": {"hActive": 10, "hFrontPorch": 4, "hSync": 3, "hBackPorch": 8}}}
    )";

    Document root;
    root.Parse(timingJsonStr.c_str());

    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsNotObject"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsVerticalMissing"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsHorizontalMissing"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsExtraField"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsVerticalNotObject"], &dispCfgEntry),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsHorizontalNotObject"], &dispCfgEntry),
            RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsValid"], &dispCfgEntry), OK);
    ASSERT_THAT(dispCfgEntry.m_Timings.Dirty, true);
}

TEST(CfgFieldParserTest, ValidatesTimingsFieldForResolution)
{
    string timingJsonStr = R"(
        {
        "timingsResolutionIlwalid":
            {"resolution": "xxx"},
        "timingsVerticalNotAllowedWithResolution" : 
            {"vertical": {},  "resolution": ""},
        "timingsHorizontalNotAllowedWithResolution" : 
            {"horizontal": {},  "resolution": ""},
        "timingsPclkHzNotAllowedWithResolution" : 
            {"pclkHz": {},  "resolution": ""},
        "timingsResolutionNonNumericWidth":
            {"resolution": "abcx768x32x60"},
        "timingsResolutionNonNumericHeight":
            {"resolution": "1024xabcx32x60"},
        "timingsResolutionNonNumericDepth":
            {"resolution": "1024x768xabcx60"},
        "timingsResolutionNonNumericRR":
            {"resolution": "1024x768x32xabc"},
        "timingsResolutiolwalid":
            {"resolution": "1024x768x32x60", "dpLaneCount": 2, "dpLinkRate": 810000000},
        "timingsResolutiolwalidFrl":
            {"resolution": "1024x768x32x60", "frlLaneCount": 4, "frlLinkRate": 10},
        "timingsResolutionIlwalidFrl":
            {"resolution": "1024x768x32x60", "frlLaneCount": 6, "frlLinkRate": 9},
        "timingsResolutionIlwalidFrl1":
            {"resolution": "1024x768x32x60", "frlLaneCount": 4, "dpLinkRate": 810000000},
        "timingsResolutionIlwalidFrl2":
            {"resolution": "1024x768x32x60", "dpLaneCount": 2, "frlLinkRate": 12},
        "timingsResolutionIlwalidFrl3":
            {"resolution": "1024x768x32x60", "dpLaneCount": 2, "dpLinkRate": 810000000, "frlLaneCount": 4, "frlLinkRate": 10},
        "timingsResolutionIlwalidFrl4":
            {"resolution": "1024x768x32x60", "dpLaneCount": 2, "dpLinkRate": 540000000, "frlLaneCount": 3},
        "timingsResolutionIlwalidFrl5":
            {"resolution": "1024x768x32x60", "dpLaneCount": 8, "frlLaneCount": 4, "frlLinkRate": 10},
        "timingsResolutiolwalidMax": 
            {"resolution": "max"},
        "timingsResolutiolwalidNative": 
            {"resolution": "native"}
        }    
    )";

    Document root;
    root.Parse(timingJsonStr.c_str());

    JsonParserCfgs cfgFieldParser;
    DPCCfgEntry dispCfgEntry;
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsVerticalNotAllowedWithResolution"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsHorizontalNotAllowedWithResolution"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsPclkHzNotAllowedWithResolution"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionIlwalid"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionNonNumericWidth"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionNonNumericHeight"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionNonNumericDepth"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionNonNumericRR"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutiolwalid"],
        &dispCfgEntry), RC::OK);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutiolwalidFrl"],
        &dispCfgEntry), RC::OK);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionIlwalidFrl"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionIlwalidFrl1"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionIlwalidFrl2"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionIlwalidFrl3"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionIlwalidFrl4"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutionIlwalidFrl5"],
        &dispCfgEntry), RC::CANNOT_ENUMERATE_OBJECT);

    AlternateTimings altTimings(1024, 768, 32, 60, 2, 810000000);
    ASSERT_THAT(altTimings, dispCfgEntry.m_AlternateTimings);

    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutiolwalidMax"],
        &dispCfgEntry), RC::OK);
    ASSERT_THAT(AlternateTimings::ResolutionDetectionMode::MAX,
        dispCfgEntry.m_AlternateTimings.resolutionDetectionMode);

    ASSERT_THAT(cfgFieldParser.ParseTimings(root["timingsResolutiolwalidNative"],
        &dispCfgEntry), RC::OK);
    ASSERT_THAT(AlternateTimings::ResolutionDetectionMode::NATIVE,
        dispCfgEntry.m_AlternateTimings.resolutionDetectionMode);
}

//------------------------------------------------------------------------------
// JsonParserRegionalCrcsTester
//------------------------------------------------------------------------------
TEST (JsonParserRegionalCrcsTester, MustBeAnObject)
{
    string jsonStr = R"(
            {
                "regionalCrcsIlwalid": "unknown",
                "regionalCrcsValid": {
                    "0": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x2020202","0x2020202","0x2020202"]
                    }
                }
            }
            )";
    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    RegionalCrcsRegions regionalCrcsRegions;
    RegionalCrcsGoldens regionalCrcsGoldens;

    ASSERT_THAT(parser.Parse(root["regionalCrcsIlwalid"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionalCrcsValid"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::OK);
}

TEST (JsonParserRegionalCrcsTester, MemberKeysMustBeInt0to8)
{
    string jsonStr = R"(
            {
                "regionalCrcsIlwalid": {
                    "V": {
                        
                    }
                },
                "regionalCrcsIlwalidRange": {
                    "10": {
                        
                    }
                },
                "regionalCrcsValid": {
                    "0": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x2020202","0x2020202","0x2020202"]
                    },
                    "1": {
                        "xPos": 780,
                        "yPos": 250,
                        "xSize": 300,
                        "ySize": 150,
                        "golden": ["0x2020202","0x2020202","0x2020202"]
                    }
                }
            }
            )";
    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    RegionalCrcsRegions regionalCrcsRegions;
    RegionalCrcsGoldens regionalCrcsGoldens;

    ASSERT_THAT(parser.Parse(root["regionalCrcsIlwalid"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionalCrcsIlwalidRange"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionalCrcsValid"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::OK);
}

TEST (JsonParserRegionalCrcsTester, RegionMustBeValidObject)
{
    string jsonStr = R"(
        {
            "regionIlwalid": {
                
            },
            "regionUnexpectedField": {
                "XPos": "A"
            },
            "regionXPosNotInt": {
                "xPos": "A",
                "yPos": 0,
                "xSize": 600,
                "ySize": 300,
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regionYPosNotInt": {
                "xPos": 0,
                "yPos": "A",
                "xSize": 600,
                "ySize": 300,
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regionXSizeNotInt": {
                "xPos": 0,
                "yPos": 0,
                "xSize": "A",
                "ySize": 300,
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regionYSizeNotInt": {
                "xPos": 0,
                "yPos": 0,
                "xSize": 600,
                "ySize": "A",
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regiolwalid": {
                "xPos": 0,
                "yPos": 0,
                "xSize": 600,
                "ySize": 300,
                "golden": ["0x2020202","0x2020202","0x2020202"]
            }
        }
        )";

    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    LwDisplay::RegionalCrcRegion regionalCrc;
    RegionGoldenCrcs regionalCrcsGolden;

    ASSERT_THAT(parser.Parse(root["regionIlwalid"], &regionalCrc, &regionalCrcsGolden),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionUnexpectedField"], &regionalCrc, &regionalCrcsGolden),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionXPosNotInt"], &regionalCrc, &regionalCrcsGolden),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionYPosNotInt"], &regionalCrc, &regionalCrcsGolden),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionXSizeNotInt"], &regionalCrc, &regionalCrcsGolden),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionYSizeNotInt"], &regionalCrc, &regionalCrcsGolden),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regiolwalid"], &regionalCrc, &regionalCrcsGolden),
        RC::OK);
}

TEST (JsonParserRegionalCrcsTester, RegionFieldGoldelwalidation)
{
    string jsonStr = R"(
            {
                "goldenNotAnArray": {},
                "goldenEmpty": [],
                "goldenNonString": [2020202,"0x2020202","0x2020202"],
                "goldenIlwalidNumberInString": ["0x2020202","KLMN","0x2020202"],
                "goldelwalid": ["0x2020202","0x2020202","0x2020202"]
            }
            )";

    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    RegionGoldenCrcs goldenCrcs;

    ASSERT_THAT(parser.ParseGoldens(root["goldenNotAnArray"], &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.ParseGoldens(root["goldenEmpty"], &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.ParseGoldens(root["goldenNonString"], &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.ParseGoldens(root["goldenIlwalidNumberInString"], &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.ParseGoldens(root["goldelwalid"], &goldenCrcs),
        RC::OK);
}

TEST (JsonParserRegionalCrcsTester, RegionMustHaveMandatoryFields)
{
    string jsonStr = R"(
        {
            "regionXPosMissing": {
                "yPos": 0,
                "xSize": 600,
                "ySize": 300,
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regionYPosMissing": {
                "xPos": 0,
                "xSize": 600,
                "ySize": 300,
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regionXSizeMissing": {
                "xPos": 0,
                "yPos": 0,
                "ySize": 300,
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regionYSizeMissing": {
                "xPos": 0,
                "yPos": 0,
                "xSize": 600,
                "ySize": "A",
                "golden": ["0x2020202","0x2020202","0x2020202"]
            },
            "regionGoldenMissing": {
                "xPos": 0,
                "yPos": 0,
                "xSize": 600,
                "ySize": 300
            }
        }
        )";

    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    LwDisplay::RegionalCrcRegion regionalCrc;
    RegionGoldenCrcs goldenCrcs;

    ASSERT_THAT(parser.Parse(root["regionXPosMissing"], &regionalCrc, &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionYPosMissing"], &regionalCrc, &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionXSizeMissing"], &regionalCrc, &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionYSizeMissing"], &regionalCrc, &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionGoldenMissing"], &regionalCrc, &goldenCrcs),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST (JsonParserRegionalCrcsTester, MemberKeysOptionalHead)
{
    string jsonStr = R"(
            {
                "regionalCrcsRegionIlwalid": {
                    "0:K": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x2020202","0x2020202","0x2020202"]
                    }
                },
                "regionalCrcsHeadIlwalid": {
                    "K:0": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x2020202","0x2020202","0x2020202"]
                    }
                },
                "regionalCrcsValid": {
                    "0:0": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x2020202","0x2020202","0x2020202"]
                    },
                    "0:1": {
                        "xPos": 780,
                        "yPos": 250,
                        "xSize": 300,
                        "ySize": 150,
                        "golden": ["0x2020202","0x2020202","0x2020202"]
                    }
                }
            }
            )";
    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    RegionalCrcsRegions regionalCrcsRegions;
    RegionalCrcsGoldens regionalCrcsGoldens;

    ASSERT_THAT(parser.Parse(root["regionalCrcsHeadIlwalid"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionalCrcsRegionIlwalid"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(parser.Parse(root["regionalCrcsValid"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::OK);
}

TEST (JsonParserRegionalCrcsTester, FailsForOOBHeadIndex)
{
    string jsonStr = R"(
            {
                "IlwalidHead": {
                    "5:0": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x2020201","0x2020202","0x2020203"]
                    }
                }
            }
            )";
    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    RegionalCrcsRegions regionalCrcsRegions;
    RegionalCrcsGoldens regionalCrcsGoldens;

    ASSERT_THAT(parser.Parse(root["IlwalidHead"], &regionalCrcsRegions, &regionalCrcsGoldens),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST (JsonParserRegionalCrcsTester, ValidateFilledData)
{
    string jsonStr = R"(
            {
                "regionalCrcs": {
                    "0:0": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x2020201","0x2020202","0x2020203"]
                    },
                    "0:1": {
                        "xPos": 780,
                        "yPos": 250,
                        "xSize": 300,
                        "ySize": 150,
                        "golden": ["0x2020204","0x2020205"]
                    },
                    "1:0": {
                        "xPos": 0,
                        "yPos": 0,
                        "xSize": 600,
                        "ySize": 300,
                        "golden": ["0x1020201","0x1020202","0x1020203"]
                    }
                }
            }
            )";
    Document root;
    root.Parse(jsonStr.c_str());

    JsonParserRegionalCrcs parser;
    RegionalCrcsRegions regionalCrcsRegions;
    RegionalCrcsGoldens regionalCrcsGoldens;

    ASSERT_THAT(parser.Parse(root["regionalCrcs"], &regionalCrcsRegions, &regionalCrcsGoldens),
            RC::OK);
    ASSERT_THAT(regionalCrcsRegions.size(), 2);

    // H0RO
    auto it0 = regionalCrcsRegions.find(0); // Head0
    ASSERT_NE(it0, regionalCrcsRegions.end());
    auto itCrcH0R0 = it0->second.find(0); // H0:R0
    ASSERT_NE(itCrcH0R0, it0->second.end());
    auto crcH0R0 = itCrcH0R0->second;
    ASSERT_EQ(crcH0R0.xPos, 0U);
    ASSERT_EQ(crcH0R0.yPos, 0U);
    ASSERT_EQ(crcH0R0.width, 600U);
    ASSERT_EQ(crcH0R0.height, 300U);
    auto itGolden0 = regionalCrcsGoldens.find(0); // Head0
    ASSERT_NE(itGolden0, regionalCrcsGoldens.end());
    auto itH0R0 = itGolden0->second.find(0); // H0:R0
    RegionGoldenCrcs goldenCrcsH0R0 = itH0R0->second;
    ASSERT_EQ(goldenCrcsH0R0.size(), static_cast<size_t>(3));
    ASSERT_EQ(goldenCrcsH0R0[0], 0x2020201U);
    ASSERT_EQ(goldenCrcsH0R0[1], 0x2020202U);
    ASSERT_EQ(goldenCrcsH0R0[2], 0x2020203U);

    // H1RO
    auto it1 = regionalCrcsRegions.find(1); // Head1
    ASSERT_NE(it1, regionalCrcsRegions.end());
    auto itCrcH1R0 = it0->second.find(0); // H1:R0
    ASSERT_NE(itCrcH1R0, it1->second.end());
    auto crcH1R0 = itCrcH1R0->second;
    ASSERT_EQ(crcH1R0.xPos, 0U);
    ASSERT_EQ(crcH1R0.yPos, 0U);
    ASSERT_EQ(crcH1R0.width, 600U);
    ASSERT_EQ(crcH1R0.height, 300U);
    auto itGolden1 = regionalCrcsGoldens.find(1); // Head1
    ASSERT_NE(itGolden1, regionalCrcsGoldens.end());
    auto itH1R0 = itGolden1->second.find(0); // H0:R0
    RegionGoldenCrcs goldenCrcsH1R0 = itH1R0->second;
    ASSERT_EQ(goldenCrcsH1R0.size(), static_cast<size_t>(3));
    ASSERT_EQ(goldenCrcsH1R0[0], 0x1020201U);
    ASSERT_EQ(goldenCrcsH1R0[1], 0x1020202U);
    ASSERT_EQ(goldenCrcsH1R0[2], 0x1020203U);
}

//------------------------------------------------------------------------------
// JsonParserCrcsTester
//------------------------------------------------------------------------------
TEST (JsonParserCrcsTester, ValidateFieldCrcs)
{
    string protocolJsonStr = R"(
        {
            "crcsNotAnArray": "unknown",
            "crcsEmptyArray": [],
            "crcsNotAnArrayOfObject": ["key", "key1", 4456],
            "crcsObjectInArrayHasIlwalidMember": [{
                "invalid": 1234
            }],
            "crcsObjectHasIlwalidPadlinksAssignedToProtocol": [{
                "dp_sst": 1234
            }],
            "crcsObjectHasNonEmptyPadlinksForSkip": [{
                "skip": ["a", "b"]
            }],
            "crcsObjectHasDscWithProtocolTmds": [{
                "dual_tmds": ["a", "b"],
                "dsc" : {}
            }],
            "crcsValid": [{
                    "dual_tmds": ["a", "b"],
                    "primary_crc": "0xABCD",
                    "rg_crc": "0xDEAD",
                    "comp_crc": "0xBABE"
                },
                {
                    "dp_sst": ["c"],
                    "primary_crc": "0xABCD"
                },
                {
                    "rg_crc": "0xABCD"
                },
                {
                    "comp_crc": "0xABCD"
                },
                {
                    "skip" : []
                }
            ]
        }
    )";

    Document root;
    root.Parse(protocolJsonStr.c_str());

    JsonParserCrcs crcsTester;

    vector<CrcsInfo> crcs;
    ASSERT_THAT(crcsTester.Parse(root["crcsNotAnArray"], &crcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsEmptyArray"], &crcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsNotAnArrayOfObject"], &crcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsObjectInArrayHasIlwalidMember"], &crcs),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsObjectHasIlwalidPadlinksAssignedToProtocol"], &crcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsObjectHasDscWithProtocolTmds"], &crcs),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsObjectHasNonEmptyPadlinksForSkip"], &crcs),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsValid"], &crcs),
        OK);
    ASSERT_THAT(crcs.size(), 5);

    ASSERT_THAT(crcs[0].crcValues[0].first.compCrc, 0xBABE);
    ASSERT_THAT(crcs[0].crcValues[0].first.rgCrc, 0xDEAD);
    ASSERT_THAT(crcs[0].crcValues[0].first.primaryCrc, 0xABCD);
    ASSERT_THAT(crcs[0].protocol, CrcHandler::CRCProtocol::CRC_TMDS_DUAL);
    ASSERT_THAT(crcs[0].padLinksMask, (1 << 0) | (1 << 1));
    ASSERT_THAT(crcs[4].protocol, CrcHandler::CRCProtocol::CRC_ILWALID);
}

TEST (JsonParserCrcsTester, ValidateFieldCrcs2Head1Or)
{
    string protocolJsonStr = R"(
        {
            "crcsValid": [{
                    "dp_sst": ["a", "b"],
                    "primary_crc": "0xABCD",
                    "rg_crc": "0xDEAD:0xBEEF",
                    "comp_crc": "0xBABE:0xBABA"
                }
            ],
            "crcsIlwalidPrimaryCRC": [{
                    "dp_sst": ["a", "b"],
                    "primary_crc": "0xABCD:0x1234",
                    "rg_crc": "0xDEAD:0xBEEF",
                    "comp_crc": "0xBABE:0xBABA"
                }
            ],
            "crcsIlwalidRgCRC": [{
                    "dp_sst": ["a", "b"],
                    "primary_crc": "0xABCD",
                    "rg_crc": "0xDEAD:fgfg",
                    "comp_crc": "0xBABE:0xBABA"
                }
            ],
            "crcsIlwalidCompCRC": [{
                    "dp_sst": ["a", "b"],
                    "primary_crc": "0xABCD",
                    "rg_crc": "0xDEAD:0xBEEF",
                    "comp_crc": "0xBABE:fgfg"
                }
            ]
        }
    )";

    Document root;
    root.Parse(protocolJsonStr.c_str());

    JsonParserCrcs crcsTester;

    vector<CrcsInfo> crcs;
    ASSERT_THAT(crcsTester.Parse(root["crcsValid"], &crcs),
        OK);
    ASSERT_THAT(crcs.size(), 1);

    ASSERT_THAT(crcs[0].crcValues[0].second.compCrc, 0xBABA);
    ASSERT_THAT(crcs[0].crcValues[0].second.rgCrc, 0xBEEF);
    ASSERT_THAT(crcs[0].crcValues[0].first.compCrc, 0xBABE);
    ASSERT_THAT(crcs[0].crcValues[0].first.rgCrc, 0xDEAD);
    ASSERT_THAT(crcs[0].crcValues[0].first.primaryCrc, 0xABCD);
    ASSERT_THAT(crcs[0].protocol, CrcHandler::CRCProtocol::CRC_DP_SST);

    ASSERT_THAT(crcsTester.Parse(root["crcsIlwalidPrimaryCRC"], &crcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsIlwalidRgCRC"], &crcs),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsTester.Parse(root["crcsIlwalidCompCRC"], &crcs),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST (JsonParserCrcsTester, ValidateMultiFieldCrcs)
{
    string protocolJsonStr = R"(
        {
            "crcsValid": [
                {
                    "dp_sst": ["a", "b"],
                    "primary_crc": "0xABCD,0xCDEF",
                    "rg_crc": "0xDEAD,0xBEAF",
                    "comp_crc": "0xBABE,0xBABA"
                }, 
                {
                    "dp_sst": ["a", "b"],
                    "primary_crc": "0xABCD,0xCDEF",
                    "rg_crc": "0xDEAD:0xBEEF,0xBABE:0xBABA",
                    "comp_crc": "0xBABE:0xBABA,0xDEAD:0xBEEF"
                }
            ]       
        }
    )";

    Document root;
    root.Parse(protocolJsonStr.c_str());

    JsonParserCrcs crcsTester;

    vector<CrcsInfo> crcs;
    ASSERT_THAT(crcsTester.Parse(root["crcsValid"], &crcs), RC::OK);
    ASSERT_THAT(crcs.size(), 2);

    ASSERT_THAT(crcs[0].crcValues[0].first.compCrc, 0xBABE);
    ASSERT_THAT(crcs[0].crcValues[1].first.compCrc, 0xBABA);
    ASSERT_THAT(crcs[0].crcValues[0].first.rgCrc, 0xDEAD);
    ASSERT_THAT(crcs[0].crcValues[1].first.rgCrc, 0xBEAF);
    ASSERT_THAT(crcs[0].crcValues[0].first.primaryCrc, 0xABCD);
    ASSERT_THAT(crcs[0].crcValues[1].first.primaryCrc, 0xCDEF);

    ASSERT_THAT(crcs[1].crcValues[0].first.rgCrc, 0xDEAD);
    ASSERT_THAT(crcs[1].crcValues[0].second.rgCrc, 0xBEEF);
    ASSERT_THAT(crcs[1].crcValues[1].first.rgCrc, 0xBABE);
    ASSERT_THAT(crcs[1].crcValues[1].second.rgCrc, 0xBABA);

    ASSERT_THAT(crcs[1].crcValues[0].first.primaryCrc, 0xABCD);
    ASSERT_THAT(crcs[1].crcValues[0].second.primaryCrc, LwDispCRCSettings::CRC_NONE);
    ASSERT_THAT(crcs[1].crcValues[1].first.primaryCrc, 0xCDEF);
    ASSERT_THAT(crcs[1].crcValues[1].second.primaryCrc, LwDispCRCSettings::CRC_NONE);
}

TEST (JsonParserCrcsTester, ValidateFieldPadlinks)
{
    string padlinksArrayJsonStr = R"(
        {"padlinksNonArray":12,
         "padlinksIlwalidEntriesInArray": ["X"],
         "padlinksValid":["a", "b"],
         "padlinksIlwalidMulitMstEntries":["a:0", "b:0"],
         "padlinksIlwalidMultiEntries":["a", "b:0"],
         "padlinksValidMstPanelId":["d:1"]})";

    Document root;
    root.Parse(padlinksArrayJsonStr.c_str());

    JsonParserCrcs crcsParser;
    PadLinksMask padlinksMask;
    UINT32 dispIndexInMstChain;
    ASSERT_THAT(crcsParser.ParsePadlinks(CrcHandler::CRCProtocol::CRC_DP_SST,
        root["padlinksNonArray"], &padlinksMask, &dispIndexInMstChain),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParsePadlinks(CrcHandler::CRCProtocol::CRC_DP_SST,
        root["padlinksIlwalidEntriesInArray"], &padlinksMask, &dispIndexInMstChain),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParsePadlinks(CrcHandler::CRCProtocol::CRC_DP_SST,
        root["padlinksValid"], &padlinksMask, &dispIndexInMstChain),
        RC::OK);
    ASSERT_THAT(padlinksMask, (1<<0 | 1<<1));

    ASSERT_THAT(crcsParser.ParsePadlinks(CrcHandler::CRCProtocol::CRC_DP_MST,
        root["padlinksIlwalidMulitMstEntries"], &padlinksMask, &dispIndexInMstChain),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParsePadlinks(CrcHandler::CRCProtocol::CRC_DP_MST,
        root["padlinksIlwalidMultiEntries"], &padlinksMask, &dispIndexInMstChain),
        RC::CANNOT_ENUMERATE_OBJECT);
    dispIndexInMstChain = UINT_MAX;
    ASSERT_THAT(crcsParser.ParsePadlinks(CrcHandler::CRCProtocol::CRC_DP_MST,
        root["padlinksValidMstPanelId"], &padlinksMask, &dispIndexInMstChain),
        RC::OK);
    ASSERT_THAT(padlinksMask, 1 << 3);
    ASSERT_THAT(dispIndexInMstChain, 1);

}

//------------------------------------------------------------------------------
// JsonParserDscTester
//------------------------------------------------------------------------------
class JsonParserDscTester : public Test
{
public:
    string dscJsonString = R"({
        "dscNonObject" : [],
        "dslwnsupported" : {"extra" : []},
        "dscEmpty" : {},
        "dscAllFieldsValid" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false
        },
        "dscAllOptionalFieldsValid1" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "sliceWidth" : 40,
            "sliceHeight" : 10
        },
        "dscAllOptionalFieldsValid2" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "sliceCount" : 2,
            "sliceHeight" : 50,
            "algorithmRevision" : 1.2
        },
        "dscNonStringDpSink" : {
            "dpSink" : [],
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 0,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false
        },
        "dscFecValid" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "fecEnable" : true
        },
        "dscFecIlwalid" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 0,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "fecEnable" : "NotBoolean"
        },
        "dscNonStringMode" : {
            "dpSink" : "dpMon1",
            "mode" : [],
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 0,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false
        },
       "dscNonStringColorFormat" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : [],
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 0,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false
        },
        "dscNonNumericBitsPerComponent" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerPixelX16" : 0,
            "bitsPerComponent" : [],
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false
        },
        "dscNonNumericBitsPerPixelX16" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : [],
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false
        },
        "dscNonBooleanAutoReset" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 0,
            "autoReset" : [],
            "fullIchErrPrecision" : true,
            "forceIchReset" : false
        },
        "dscNonBooleanFullIchErrPrecision" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 0,
            "autoReset" : true,
            "fullIchErrPrecision" : [],
            "forceIchReset" : false
        },
        "dscNonBooleanForceIchReset" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 0,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : []
        },
        "dscNonNumericSliceWidth" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "sliceWidth" : [] 
        },
        "dscNonNumericSliceHeight" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "sliceHeight" : [] 
        },
        "dscNonNumericSliceCount" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "sliceCount" : [] 
        },
        "dscIlwalidCombinationSliceCountAndWidth" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "sliceCount" : 1,
            "sliceWidth" : 50
        },
        "dscNonNumericDscRev" : {
            "dpSink" : "dpMon1",
            "mode" : "single",
            "colorFormat" : "RGB",
            "bitsPerComponent" : 10,
            "bitsPerPixelX16" : 266,
            "autoReset" : true,
            "fullIchErrPrecision" : true,
            "forceIchReset" : false,
            "algorithmRevision" : []
        }
    })";
};

TEST_F (JsonParserDscTester, ValidateFieldDsc)
{
    Document root;
    root.Parse(dscJsonString.c_str());

    JsonParserCrcs crcsParser;
    DscSettings dscSettings;

    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonObject"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dslwnsupported"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscEmpty"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonStringDpSink"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonStringMode"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonStringColorFormat"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonNumericBitsPerComponent"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonNumericBitsPerPixelX16"], &dscSettings),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonBooleanAutoReset"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonBooleanFullIchErrPrecision"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonBooleanForceIchReset"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonNumericSliceWidth"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonNumericSliceHeight"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonNumericSliceCount"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscIlwalidCombinationSliceCountAndWidth"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscNonNumericDscRev"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscFecIlwalid"], &dscSettings),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(crcsParser.ParseDsc(root["dscAllFieldsValid"], &dscSettings),
        OK);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscAllOptionalFieldsValid1"], &dscSettings),
        OK);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscAllOptionalFieldsValid2"], &dscSettings),
        OK);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscAllFieldsValid"], &dscSettings),
        OK);
    ASSERT_THAT(crcsParser.ParseDsc(root["dscFecValid"], &dscSettings),
        OK);
}

TEST_F (JsonParserDscTester, ValidateFieldDscParsingResult)
{
    Document root;
    root.Parse(dscJsonString.c_str());

    JsonParserCrcs crcsParser;
    DscSettings dscSettings;

    ASSERT_THAT(crcsParser.ParseDsc(root["dscAllFieldsValid"], &dscSettings),
        OK);
    ASSERT_THAT(dscSettings.dpSink, "dpMon1");
    ASSERT_THAT(dscSettings.mode, Display::DSCMode::SINGLE);
    ASSERT_THAT(dscSettings.colorFormat, LWT_COLOR_FORMAT::LWT_COLOR_FORMAT_RGB);
    ASSERT_THAT(dscSettings.bitsPerComponent, 10);
    ASSERT_THAT(dscSettings.bitsPerPixelX16, 266);
    ASSERT_THAT(dscSettings.autoReset, true);
    ASSERT_THAT(dscSettings.fullIchErrPrecision, true);
    ASSERT_THAT(dscSettings.forceIchReset, false);

    ASSERT_THAT(crcsParser.ParseDsc(root["dscAllOptionalFieldsValid1"], &dscSettings),
        OK);
    ASSERT_THAT(dscSettings.sliceWidth, 40);
    ASSERT_THAT(dscSettings.sliceHeight, 10);

    ASSERT_THAT(crcsParser.ParseDsc(root["dscAllOptionalFieldsValid2"], &dscSettings),
        OK);
    ASSERT_THAT(dscSettings.sliceCount, LwDispDSCSliceCount::COUNT_2);
    ASSERT_THAT(dscSettings.sliceHeight, 50);

    ASSERT_THAT(dscSettings.algorithmRevision.versionMajor, 1);
    ASSERT_THAT(dscSettings.algorithmRevision.versionMinor, 2);

    ASSERT_THAT(crcsParser.ParseDsc(root["dscFecValid"], &dscSettings),
        OK);
    ASSERT_THAT(dscSettings.fecEnable, true);
}

//------------------------------------------------------------------------------
// JsonParserViewPort
//------------------------------------------------------------------------------
TEST(JsonParserViewPortTester, ValidatesFieldViewport)
{
    string viewPortJsonStr = R"(
        {
            "viewPortValid": {"xPos": 5, "yPos": 10, "xSize": 110, "ySize": 120},
            "viewPortMissingField": {"xPos": 0, "yPos": 0, "xSize": 100},
            "viewPortNonNumericXPos": {"xPos": {}, "yPos": 0, "xSize": 100, "ySize": 100},
            "viewPortNonNumericYPos": {"xPos": 0, "yPos": {}, "xSize": 100, "ySize": 100},
            "viewPortNonNumericXSize": {"xPos": 0, "yPos": 0, "xSize": {}, "ySize": 100},
            "viewPortNonNumericYSize": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": {}},
            "viewPortExtraField": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100,
                                   "extra": 100},
            "viewPortOnlyMandatoryFields": {"xSize": 110, "ySize": 120}
        }
    )";

    JsonParserViewPort viewportObjectParser;

    Document root;
    root.Parse(viewPortJsonStr.c_str());

    LwDispViewPortSettings::ViewPort viewPort;
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortMissingField"], &viewPort),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortNonNumericXPos"], &viewPort),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortNonNumericYPos"], &viewPort),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortNonNumericXSize"], &viewPort),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortNonNumericYSize"], &viewPort),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortExtraField"], &viewPort),
        RC::CANNOT_ENUMERATE_OBJECT);

    LwDispViewPortSettings::ViewPort validViewPort;
    LwDispViewPortSettings::ViewPort expectedViewPort(5, 10, 110, 120);
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortValid"], &validViewPort),
        OK);
    ASSERT_THAT(validViewPort, expectedViewPort);

    LwDispViewPortSettings::ViewPort validViewPortWithOnlyMandatoryFields;
    LwDispViewPortSettings::ViewPort expectedViewPortWithOnlyMandatoryFields(0, 0, 110, 120);
    ASSERT_THAT(viewportObjectParser.Parse(root["viewPortOnlyMandatoryFields"],
        &validViewPortWithOnlyMandatoryFields), OK);
    ASSERT_THAT(validViewPortWithOnlyMandatoryFields, expectedViewPortWithOnlyMandatoryFields);
}

//------------------------------------------------------------------------------
// JsonParserBlend
//------------------------------------------------------------------------------
TEST (BlendTester, OnParseSuccessFillsCompFields)
{
    JsonParserBlend parser;
    LwDispWindowCompositionSettings compositionSettings;
    string blendStr = R"(
    {
        "blend": {
            "colorKeyingRange": {
                "min": [1, 2, 3, 4],
                "max": [100, 200, 300, 400]
            },
            "depth": 3,
            "colorKeyingSelect": "dst",
            "color": {
                "matchSelectFactor": {
                    "src": "K1",
                    "dst": "NEG_K1"
                },
                "noMatchSelectFactor": {
                    "src": "ZERO",
                    "dst": "ONE"
                }
            },
            "alpha": {
                "matchSelectFactor": {
                    "src": "NEG_K1_TIMES_DST",
                    "dst": "NEG_K1_TIMES_SRC"
                },
                "noMatchSelectFactor": {
                    "src": "K1",
                    "dst": "K2"
                }
            },
            "constantAlpha": [240, 55]
        }
    }
    )";
    Document root;
    root.Parse(blendStr.c_str());
    parser.Parse(root["blend"], &compositionSettings);
    ASSERT_EQ(compositionSettings.keyAlpha.min, 1);
    ASSERT_EQ(compositionSettings.keyRedCr.min, 2);
    ASSERT_EQ(compositionSettings.keyGreenY.min, 3);
    ASSERT_EQ(compositionSettings.keyBlueCb.min, 4);
    ASSERT_EQ(compositionSettings.keyAlpha.max, 100);
    ASSERT_EQ(compositionSettings.keyRedCr.max, 200);
    ASSERT_EQ(compositionSettings.keyGreenY.max, 300);
    ASSERT_EQ(compositionSettings.keyBlueCb.max, 400);
    ASSERT_EQ(compositionSettings.compCtrlDepth, 3);
    ASSERT_EQ(compositionSettings.colorKeySelect, 2);
    ASSERT_EQ(compositionSettings.matchSelectFactors.first.color.matchSelect, 2);
    ASSERT_EQ(compositionSettings.matchSelectFactors.first.color.noMatchSelect, 0);
    ASSERT_EQ(compositionSettings.matchSelectFactors.second.color.matchSelect, 4);
    ASSERT_EQ(compositionSettings.matchSelectFactors.second.color.noMatchSelect, 1);
    ASSERT_EQ(compositionSettings.matchSelectFactors.first.alpha.matchSelect, 8);
    ASSERT_EQ(compositionSettings.matchSelectFactors.first.alpha.noMatchSelect, 2);
    ASSERT_EQ(compositionSettings.matchSelectFactors.second.alpha.matchSelect, 7);
    ASSERT_EQ(compositionSettings.matchSelectFactors.second.alpha.noMatchSelect, 3);
    ASSERT_EQ(compositionSettings.compConsAlphaK1, 240);
    ASSERT_EQ(compositionSettings.compConsAlphaK2, 55);
}
TEST (BlendTester, MustBeAnObject)
{
    JsonParserBlend blendParser;
    LwDispWindowCompositionSettings compositionSettings;
    string blendStr = R"(
    {
        "compNonObject": 1234,
        "compObject": {
            "colorKeyingRange": {
                "min": [1, 2, 3, 4],
                "max": [100, 200, 300, 400]
            },
            "depth": 3,
            "colorKeyingSelect": "dst",
            "color": {
                "matchSelectFactor": {
                    "src": "K1",
                    "dst": "NEG_K1"
                },
                "noMatchSelectFactor": {
                    "src": "ZERO",
                    "dst": "ONE"
                }
            },
            "alpha": {
                "matchSelectFactor": {
                    "src": "NEG_K1_TIMES_DST",
                    "dst": "NEG_K1_TIMES_SRC"
                },
                "noMatchSelectFactor": {
                    "src": "K1",
                    "dst": "K2"
                }
            },
            "constantAlpha": [240, 55]
        }
    }
    )";
    Document root;
    root.Parse(blendStr.c_str());
    ASSERT_EQ(blendParser.Parse(root["compNonObject"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(blendParser.Parse(root["compObject"], &compositionSettings), RC::OK);
}
TEST (BlendTester, ValidatesMandatoryFields)
{
    JsonParserBlend blendParser;
    LwDispWindowCompositionSettings compositionSettings;
    string blendStr = R"(
    {
        "compWithIlwalidEntries": {
            "colorKeyingRange1": {},
            "depth1": 4
        },
        "compWithLessEntries": {
            "colorKeyingRange": {
                "min": [1, 2, 3, 4],
                "max": [100, 200, 300, 400]
            },
            "depth": 3,
            "colorKeyingSelect": "dst",
            "color": {
                "matchSelectFactor": {
                    "src": "K1",
                    "dst": "NEG_K1"
                },
                "noMatchSelectFactor": {
                    "src": "ZERO",
                    "dst": "ONE"
                }
            },
            "alpha": {
                "matchSelectFactor": {
                    "src": "NEG_K1_TIMES_DST",
                    "dst": "NEG_K1_TIMES_SRC"
                },
                "noMatchSelectFactor": {
                    "src": "K1",
                    "dst": "K2"
                }
            }
        },
        "compWithValidEntries": {
            "colorKeyingRange": {
                "min": [1, 2, 3, 4],
                "max": [100, 200, 300, 400]
            },
            "depth": 3,
            "colorKeyingSelect": "dst",
            "color": {
                "matchSelectFactor": {
                    "src": "K1",
                    "dst": "NEG_K1"
                },
                "noMatchSelectFactor": {
                    "src": "ZERO",
                    "dst": "ONE"
                }
            },
            "alpha": {
                "matchSelectFactor": {
                    "src": "NEG_K1_TIMES_DST",
                    "dst": "NEG_K1_TIMES_SRC"
                },
                "noMatchSelectFactor": {
                    "src": "K1",
                    "dst": "K2"
                }
            },
            "constantAlpha": [240, 55]
        }
    }
    )";
    Document root;
    root.Parse(blendStr.c_str());
    ASSERT_EQ(blendParser.Parse(root["compWithIlwalidEntries"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(blendParser.Parse(root["compWithLessEntries"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(blendParser.Parse(root["compWithValidEntries"], &compositionSettings), RC::OK);
}
TEST (BlendTester, DepthField_RangeCheck)
{
    LwDispWindowCompositionSettings compositionSettings;
    string depthStr = R"(
    {
        "depthNegative": -10,
        "depthIlwalid": 300,
        "depthValid": 230
    }
    )";
    Document root;
    root.Parse(depthStr.c_str());
    ASSERT_EQ(SimpleStructsParser::ParseUINT<UINT08>(root["depthNegative"],
        &compositionSettings.compCtrlDepth), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(SimpleStructsParser::ParseUINT<UINT08>(root["depthIlwalid"],
        &compositionSettings.compCtrlDepth), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(SimpleStructsParser::ParseUINT<UINT08>(root["depthValid"],
        &compositionSettings.compCtrlDepth), RC::OK);
}
TEST (BlendTester, DepthField_MustBeAnUnsignedInt)
{
    LwDispWindowCompositionSettings compositionSettings;
    string depthStr = R"(
    {
        "depthNonInteger": "hello",
        "depthNegative": -5,
        "depth": 230
    }
    )";
    Document root;
    root.Parse(depthStr.c_str());
    ASSERT_EQ(SimpleStructsParser::ParseUINT<UINT08>(root["depthNonInteger"],
        &compositionSettings.compCtrlDepth), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(SimpleStructsParser::ParseUINT<UINT08>(root["depthNegative"],
        &compositionSettings.compCtrlDepth), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(SimpleStructsParser::ParseUINT<UINT08>(root["depth"],
        &compositionSettings.compCtrlDepth), RC::OK);
}
TEST (BlendTester, ConstantAlpha_MustBeAnArray)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend alphaParser;
    string alphaStr = R"(
    {
        "constantAlphaNonArray": {},
        "constantAlpha": [1, 2]
    }
    )";
    Document root;
    root.Parse(alphaStr.c_str());
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaNonArray"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlpha"], &compositionSettings), RC::OK);
}
TEST (BlendTester, ConstantAlpha_MustContainTwoValues)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend alphaParser;
    string alphaStr = R"(
    {
        "constantAlphaEmptyArray": [],
        "constantAlphaLessValues": [1],
        "constantAlphaMoreValues": [1, 2, 3],
        "constantAlphaTwoValues": [1, 2]
    }
    )";
    Document root;
    root.Parse(alphaStr.c_str());
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaEmptyArray"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaLessValues"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaMoreValues"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaTwoValues"],
        &compositionSettings), RC::OK);
}
TEST (BlendTester, ConstantAlpha_BothElementsMustBeUINT08)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend alphaParser;
    string alphaStr = R"(
    {
        "constantAlphaIlwalidK1": [-4, 10],
        "constantAlphaIlwalidK2": [250, 500],
        "constantAlphaIlwalidK1K2": [300, -10],
        "constantAlphaValid": [1, 2]
    }
    )";
    Document root;
    root.Parse(alphaStr.c_str());
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaIlwalidK1"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaIlwalidK2"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaIlwalidK1K2"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(alphaParser.ParseConstantAlpha(root["constantAlphaValid"],
        &compositionSettings), RC::OK);
}
TEST (BlendTester, ColorKeyingRange_MustBeAnObject)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend colorKeyingRangeParser;
    string colorKeyingRangeStr = R"(
    {
        "colorKeyingRangeNonObject": "1234",
        "colorKeyingRange": {
            "min": [1, 2, 3, 4],
            "max": [100, 200, 300, 400]
        }
    }
    )";
    Document root;
    root.Parse(colorKeyingRangeStr.c_str());
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeNonObject"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRange"],
        &compositionSettings), RC::OK);
}
TEST (BlendTester, ColorKeyingRange_MustContainMinMax)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend colorKeyingRangeParser;
    string colorKeyingRangeStr = R"(
    {
        "colorKeyingRangeEmptyObject": {},
        "colorKeyingRangeOnlyMin": {
            "min": [1, 2, 3, 4]
        },
        "colorKeyingRangeOnlyMax": {
            "max": [1, 2, 3, 4]
        },
        "colorKeyingRangeIlwalidFields": {
            "mean": [1, 2, 3, 4],
            "median": []
        },
        "colorKeyingRange": {
            "min": [1, 2, 3, 4],
            "max": [100, 200, 300, 400]
        }
    }
    )";
    Document root;
    root.Parse(colorKeyingRangeStr.c_str());
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeEmptyObject"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeOnlyMin"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeOnlyMax"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeIlwalidFields"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRange"],
        &compositionSettings), RC::OK);
}
TEST (BlendTester, ColorKeyingRange_MinMaxMustBeAnArrayOfSizeFour)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend colorKeyingRangeParser;
    string colorKeyingRangeStr = R"(
    {
        "colorKeyingRangeEmptyMin": {
            "min": [],
            "max": [100, 200, 300, 400]
        },
        "colorKeyingRangeEmptyMax": {
            "min": [1, 2, 3, 4],
            "max": []
        },
        "colorKeyingRangeIlwalid": {
            "min": [1, 2, 3, 4, 5],
            "max": [300, 400]
        },
        "colorKeyingRangeValid": {
            "min": [1, 2, 3, 4],
            "max": [100, 200, 300, 400]
        }
    }
    )";
    Document root;
    root.Parse(colorKeyingRangeStr.c_str());
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeEmptyMin"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeEmptyMax"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeIlwalid"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeValid"],
        &compositionSettings), RC::OK);
}
TEST (BlendTester, ColorKeyingRange_MinMaxArrayElementsMustBeOfTypeUINT16)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend colorKeyingRangeParser;
    string colorKeyingRangeStr = R"(
    {
        "colorKeyingRangeMinIlwalid": {
            "min": [400000, -2, 3, 5],
            "max": [100, 200, 300, 400]
        },
        "colorKeyingRangeMaxIlwalid": {
            "min": [1, 2, 3, 4],
            "max": [-5, 10, 20, 300000]
        },
        "colorKeyingRangeIlwalid": {
            "min": [-1, -2, 3, 4],
            "max": [300000, 40, 50, 100]
        },
        "colorKeyingRangeValid": {
            "min": [1, 2, 3, 4],
            "max": [100, 200, 300, 65535]
        }
    }
    )";
    Document root;
    root.Parse(colorKeyingRangeStr.c_str());
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeMinIlwalid"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeMaxIlwalid"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeIlwalid"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingRangeParser.ParseColorKeyingRange(root["colorKeyingRangeValid"],
        &compositionSettings), RC::OK);
}
TEST (BlendTester, ColorKeyingSelect_MustBeString)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend colorKeyingSelectParser;
    string colorKeyingSelectStr = R"(
    {
        "colorKeyingSelectIlwalid": [],
        "colorKeyingSelectValid": "src"
    }
    )";
    Document root;
    root.Parse(colorKeyingSelectStr.c_str());
    ASSERT_EQ(colorKeyingSelectParser.ParseColorKeyingSelect(root["colorKeyingSelectIlwalid"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingSelectParser.ParseColorKeyingSelect(root["colorKeyingSelectValid"],
        &compositionSettings), RC::OK);
}
TEST (BlendTester, ColorKeyingSelect_StringMustBeSrcOrDst)
{
    LwDispWindowCompositionSettings compositionSettings;
    JsonParserBlend colorKeyingSelectParser;
    string colorKeyingSelectStr = R"(
    {
        "colorKeyingSelectIlwalid": "hello",
        "colorKeyingSelectSrcValid": "src",
        "colorKeyingSelectDstValid": "dst"
    }
    )";
    Document root;
    root.Parse(colorKeyingSelectStr.c_str());
    ASSERT_EQ(colorKeyingSelectParser.ParseColorKeyingSelect(root["colorKeyingSelectIlwalid"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(colorKeyingSelectParser.ParseColorKeyingSelect(root["colorKeyingSelectSrcValid"],
        &compositionSettings), RC::OK);
    ASSERT_EQ(colorKeyingSelectParser.ParseColorKeyingSelect(root["colorKeyingSelectDstValid"],
        &compositionSettings), RC::OK);
}
//------------------------------------------------------------------------------
// CompositionFactorParser
//------------------------------------------------------------------------------
TEST (BlendTester, CompositionField_MustBeAnObject)
{
    LwDispWindowCompositionSettings compositionSettings;
    CompositionFactorParser compositionFactorParser(true);
    string CompositionFactorStr = R"(
    {
        "compositionNonObject": "hello",
        "composition": {
            "matchSelectFactor": {
                "src": "K1",
                "dst": "K2"
            },
            "noMatchSelectFactor": {
                "src": "ZERO",
                "dst": "NEG_K1_TIMES_SRC"
            }
        }
    }
    )";
    Document root;
    root.Parse(CompositionFactorStr.c_str());
    ASSERT_EQ(compositionFactorParser.Parse(root["compositionNonObject"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(compositionFactorParser.Parse(root["composition"], &compositionSettings), RC::OK);
}
TEST (BlendTester, CompositionField_MustContainOnlyTwoFields)
{
    LwDispWindowCompositionSettings compositionSettings;
    CompositionFactorParser compositionFactorParser(false);
    string CompositionFactorStr = R"(
    {
        "compositionEmptyObject": {},
        "compositionIlwalid": {
            "matchSelectFactor": {}
        },
        "composition": {
            "matchSelectFactor": {
                "src": "K1",
                "dst": "K2"
            },
            "noMatchSelectFactor": {
                "src": "K2",
                "dst": "ONE"
            }
        }
    }
    )";
    Document root;
    root.Parse(CompositionFactorStr.c_str());
    ASSERT_EQ(compositionFactorParser.Parse(root["compositionEmptyObject"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(compositionFactorParser.Parse(root["compositionIlwalid"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(compositionFactorParser.Parse(root["composition"], &compositionSettings), RC::OK);
}
TEST (BlendTester, CompositionField_MustContailwalidFields)
{
    LwDispWindowCompositionSettings compositionSettings;
    CompositionFactorParser compositionFactorParser(true);
    string CompositionFactorStr = R"(
    {
        "compositionNoValidFields": {
            "hello": {},
            "world": {}
        },
        "compositionIlwalid": {
            "matchSelectFactor": {
                "src": {}
            },
            "noMatchSelectFactor": {
                "hello": "world",
                "world": "hello"
            }
        },
        "composition": {
            "matchSelectFactor": {
                "src": "K1",
                "dst": "NEG_K1"
            },
            "noMatchSelectFactor": {
                "src": "K1",
                "dst": "NEG_K1"
            }
        }
    }
    )";
    Document root;
    root.Parse(CompositionFactorStr.c_str());
    ASSERT_EQ(compositionFactorParser.Parse(root["compositionNoValidFields"],
        &compositionSettings), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(compositionFactorParser.Parse(root["compositionIlwalid"], &compositionSettings),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(compositionFactorParser.Parse(root["composition"], &compositionSettings), RC::OK);
}
TEST (BlendTester, SelectFactor_MustBeAnObject)
{
    LwDispWindowCompositionSettings compositionSettings;
    CompositionFactorParser selectFactorParser(true);
    string selectFactorStr = R"(
    {
        "selectFactorNonObject": "hello",
        "selectFactor": {
            "src": "K1",
            "dst": "NEG_K1"
        }
    }
    )";
    Document root;
    root.Parse(selectFactorStr.c_str());
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorNonObject"], true),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactor"], true), RC::OK);
}
TEST (BlendTester, SelectFactor_MustContainOnlyTwoFieldsSrcAndDst)
{
    LwDispWindowCompositionSettings compositionSettings;
    CompositionFactorParser selectFactorParser(false);
    string selectFactorStr = R"(
    {
        "selectFactorEmptyObject": {},
        "selectFactorLessFields": {
            "src": "K1"
        },
        "selectFactorMoreFields": {
            "src": "K1",
            "dst": "NEG_K1",
            "hello": "world"
        },
        "selectFactorIlwalid": {
            "src": "K1",
            "hello": "NEG_K1"
        },
        "selectFactorValid": {
            "src": "K1",
            "dst": "ONE"
        }
    }
    )";
    Document root;
    root.Parse(selectFactorStr.c_str());
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorEmptyObject"], false),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorLessFields"], false),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorMoreFields"], false),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorIlwalid"], false),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorValid"], false),
        RC::OK);
}
TEST (BlendTester, SelectFactor_SrcAndDstMustBeString)
{
    LwDispWindowCompositionSettings compositionSettings;
    CompositionFactorParser selectFactorParser(false);
    string selectFactorStr = R"(
    {
        "selectFactorSrcNonString": {
            "src": [],
            "dst": "NEG_K1"
        },
        "selectFactorDstNonString": {
            "src": "NEG_K1",
            "dst": {}
        },
        "selectFactorValid": {
            "src": "K1",
            "dst": "K2"
        }
    }
    )";
    Document root;
    root.Parse(selectFactorStr.c_str());
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorSrcNonString"], false),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorDstNonString"], false),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["selectFactorValid"], false), RC::OK);
}
TEST (BlendTester, SelectFactor_SrcAndDstMustContailwalidAlgorithm)
{
    LwDispWindowCompositionSettings compositionSettings;
    CompositionFactorParser selectFactorParser(true);
    string selectFactorStr = R"(
    {
        "colorSrcIlwalid": {
            "src": "K2",
            "dst": "ONE"
        },
        "colorDstIlwalid": {
            "src": "K1",
            "dst": "K1_TIMES_SRC"
        },
        "alphaSrcIlwalid": {
            "src": "ONE",
            "dst": "ONE"
        },
        "alphaDstIlwalid": {
            "src": "ZERO",
            "dst": "K1_TIMES_DST"
        },
        "colorSrcDstValid": {
            "src": "ZERO",
            "dst": "K1_TIMES_DST"
        },
        "alphaSrcDstValid": {
            "src": "ZERO",
            "dst": "K2"
        }
    }
    )";
    Document root;
    root.Parse(selectFactorStr.c_str());
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["colorSrcIlwalid"], true),
              RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["colorDstIlwalid"], true),
              RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["alphaSrcIlwalid"], false),
              RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["alphaDstIlwalid"], false),
              RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["colorSrcDstValid"], true),
            RC::OK);
  ASSERT_EQ(selectFactorParser.ValidateSelectFactor(root["alphaSrcDstValid"], false),
            RC::OK);
}

//------------------------------------------------------------------------------
// JsonParserLwrsor
//------------------------------------------------------------------------------
TEST(JsonParserLwrsorTester, ValidatesFieldLwrsor)
{
    string lwrsorJsonStr = R"(
        {"lwrsorValid": {"size": 32, "format": "A8R8G8B8", "xPos": 3, "yPos": 4,
                           "pattern": "box crosshatched unused"},
         "lwrsorValidMandatoryFields": {"size": 32, "format": "A8R8G8B8",
                           "pattern": "box crosshatched unused"},
         "lwrsorNonNumericSize": {"size": {}, "format": "A8R8G8B8", "xPos": 3, "yPos": 4,
                          "pattern": "box crosshatched unused"},
         "lwrsorNonStringFormat": {"size": 32, "format": {}, "xPos": 3, "yPos": 4,
                          "pattern": "box crosshatched unused"},
         "lwrsorNonNumericXPos": {"size": 32, "format": "A8R8G8B8", "xPos": {}, "yPos": 4,
                          "pattern": "box crosshatched unused"},
         "lwrsorNonNumericYPos": {"size": 32, "format": "A8R8G8B8", "xPos": 3, "yPos": {},
                          "pattern": "box crosshatched unused"},
         "lwrsorNonStringPattern": {"size": 32, "format": "A8R8G8B8", "xPos": 3, "yPos": 4,
                           "pattern": []}}
    )";

    JsonParserLwrsor lwrsorObjectParser;

    Document root;
    root.Parse(lwrsorJsonStr.c_str());
    ASSERT_TRUE(root.IsObject());

    Cursor cursor;
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorNonNumericSize"], &cursor),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorNonStringFormat"], &cursor),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorNonNumericXPos"], &cursor),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorNonNumericYPos"], &cursor),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorNonNumericYPos"], &cursor),
        RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorNonStringPattern"], &cursor),
        RC::CANNOT_ENUMERATE_OBJECT);

    Cursor validLwrsor;
    vector<string> tokenizedSurfacePattern = {"box", "crosshatched", "unused"};
    Cursor expectedLwrsor(3, 4, 32, ColorUtils::A8R8G8B8, tokenizedSurfacePattern);
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorValid"], &validLwrsor),
        OK);
    ASSERT_THAT(validLwrsor, expectedLwrsor);

    Cursor validLwrsorMandatoryFields;
    Cursor expectedLwrsorValidMandatoryFields(0, 0, 32, ColorUtils::A8R8G8B8,
            tokenizedSurfacePattern);
    ASSERT_THAT(lwrsorObjectParser.Parse(root["lwrsorValidMandatoryFields"],
            &validLwrsorMandatoryFields), OK);
    ASSERT_THAT(validLwrsorMandatoryFields, validLwrsorMandatoryFields);
}

//------------------------------------------------------------------------------
// JsonParserTimings
//------------------------------------------------------------------------------
TEST(JsonParserTimingsTester, ValidatesFieldTimings)
{
    string timingsJsonStr = R"(
       {"horizNonNumericVActive": {"hActive": {}, "hFrontPorch": 4, "hSync": 3, "hBackPorch": 2},
        "horizNonNumericVFrontPorch": {"hActive": 10, "hFrontPorch": {}, "hSync": 3,
                                       "hBackPorch": 2},
        "horizNonNumericVSync": {"hActive": 10, "hFrontPorch": 4, "hSync": {}, "hBackPorch": 2},
        "horizNonNumericVBackPorch": {"hActive": 10, "hFrontPorch": 4, "hSync": 3,
                                      "hBackPorch": {}},
        "horizNonBooleanSyncPolarity": {"hActive": 10, "hFrontPorch": 4, "hSync": 3,
                                      "hBackPorch": 5, "hSyncPolarityNegative": {}},
        "horizTimingExtra": {"hActive": 10, "hFrontPorch": 4, "hSync": 3, "hBackPorch": 8,
                             "extra": []}
       }
    )";

    JsonParserTimings timingsParser;

    Document root;
    root.Parse(timingsJsonStr.c_str());

    UINT32 active = 0;
    UINT32 hFrontPorch = 0;
    UINT32 hSync = 0;
    UINT32 hBackPorch = 0;
    bool hSyncPolarityNeg = false;

    ASSERT_THAT(timingsParser.ParseVORHTimings
        (false, root["horizNonNumericVActive"], &active,
        &hFrontPorch, &hSync, &hBackPorch, &hSyncPolarityNeg), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(timingsParser.ParseVORHTimings
        (false, root["horizNonNumericVFrontPorch"], &active,
        &hFrontPorch, &hSync, &hBackPorch, &hSyncPolarityNeg), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(timingsParser.ParseVORHTimings
        (false, root["horizNonNumericVSync"], &active,
        &hFrontPorch, &hSync, &hBackPorch, &hSyncPolarityNeg), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(timingsParser.ParseVORHTimings
        (false, root["horizNonNumericVBackPorch"], &active,
        &hFrontPorch, &hSync, &hBackPorch, &hSyncPolarityNeg), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(timingsParser.ParseVORHTimings
        (false, root["horizTimingExtra"], &active,
        &hFrontPorch, &hSync, &hBackPorch, &hSyncPolarityNeg), RC::CANNOT_ENUMERATE_OBJECT);

    string validTimingsJsonStr = R"(
        {
         "horizTimingValid": {"hActive": 5, "hFrontPorch": 3, "hSync": 2, "hBackPorch": 1, 
                             "hSyncPolarityNegative": true},
         "vertTimingValid": {"vActive": 2, "vFrontPorch": 4, "vSync": 5, "vBackPorch": 6,
                             "vSyncPolarityNegative": true}
        }
    )";

    Document rootValid;
    rootValid.Parse(validTimingsJsonStr.c_str());

    active = hFrontPorch = hSync = hBackPorch = 0;
    ASSERT_THAT(timingsParser.ParseVORHTimings
        (false, rootValid["horizTimingValid"], &active,
        &hFrontPorch, &hSync, &hBackPorch, &hSyncPolarityNeg), OK);
    ASSERT_THAT(active, 5);
    ASSERT_THAT(hFrontPorch, 3);
    ASSERT_THAT(hSync, 2);
    ASSERT_THAT(hBackPorch, 1);
    ASSERT_THAT(hSyncPolarityNeg, true);

    active = hFrontPorch = hSync = hBackPorch = 0; hSyncPolarityNeg = false;
    ASSERT_THAT(timingsParser.ParseVORHTimings
        (true, rootValid["vertTimingValid"], &active,
        &hFrontPorch, &hSync, &hBackPorch, &hSyncPolarityNeg), OK);
    ASSERT_THAT(active, 2);
    ASSERT_THAT(hFrontPorch, 4);
    ASSERT_THAT(hSync, 5);
    ASSERT_THAT(hBackPorch, 6);
    ASSERT_THAT(hSyncPolarityNeg, true);
}

//------------------------------------------------------------------------------
// JsonParserWindowsTester
//------------------------------------------------------------------------------
TEST(JsonParserWindowsTester, ValidatesWindowsFields)
{
    JsonParserWindows windowsObjectParser;

    string windowsJsonStr = R"(
           {"windowNonObject" : 1234, "windowEmptyObject":{},
            "windowExtraField": {"width": 10, "height": 3, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {}, "compositionRectOut": {}, "extra" : 1234},
            "windowNonNumericWidth": {"width": [], "height": 3, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {}, "compositionRectOut": {}},
            "windowNonNumericHeight": {"width": 12, "height": [], "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {}, "compositionRectOut": {}},
            "windowNonStringFormat": {"width": 12, "height": 130, "format": 1234,
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {}, "compositionRectOut": {}},
            "windowNonStringPattern": {"width": 12, "height": 130, "format": "A8R8G8B8",
                    "pattern": 1234,
                    "surfaceRectIn": {}, "compositionRectOut": {}},
            "windowNonNumericInputScalerVTaps": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "inputScalerVTaps" : [],
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110}},
            "windowNonNumericInputScalerHTaps": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "inputScalerHTaps" : [],
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110}},
            "windowValid": {"ownerHead": 1, "width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "layout" : "pitch",
                    "displayGamutTypeForTMOLut" : "SDR", "colorSpace" : "RGB",
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110}},
            "windowValidWithAllFields": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "inputScalerVTaps" : 5,
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110},
                    "inputScalerHTaps" : 5, "displayGamutTypeForTMOLut" : "SDR",
                    "rotate:flip" : "180:hor"},
            "windowValidWithMandatoryParams": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {"xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xSize": 110, "ySize": 110}},
            "windowWithoutCompositionRectOut": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "surfaceRectIn": {"xSize": 100, "ySize": 100}},
            "windowWithoutSurfaceRectIn": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical",
                    "compositionRectOut": {"xSize": 110, "ySize": 130}},
            "windowNonStringLayout": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "layout" : []},
            "windowIlwalidLayout": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "layout" : "xyz"},
            "windowValidBLLayout": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "layout" : "blocklinear"},
            "windowValidBLLayoutWithParams": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "layout" : "blocklinear:4:4"},
            "windowIlwalidBlockWidth": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "layout" : "blocklinear:nan:4"},
            "windowIlwalidBlockHeight": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "layout" : "blocklinear:8:nan"},
            "windowIlwalidDisplayGamutType": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "displayGamutTypeForTMOLut" : []},
            "windowOnlyMandatoryFields": {"pattern": "gradient color_palette vertical"},
            "windowValidColorSpace": {"width": 120, "height": 130, "format": "Y12___U12V12_N422",
                    "pattern": "gradient color_palette vertical", "colorSpace" : "YUV2020"},
            "windowIlwalidColorSpace": {"width": 120, "height": 130, "format": "Y8___U8V8_N444",
                    "pattern": "gradient color_palette vertical", "colorSpace" : "YUV729"},
            "windowIlwalidColorSpaceFormat1": {"width": 120, "height": 130, "format": "Y8___U8V8_N444",
                    "pattern": "gradient color_palette vertical", "colorSpace" : "RGB"},
            "windowIlwalidColorSpaceFormat2": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "colorSpace" : "YUV601"}
            }
    )";

    Document root;
    root.Parse(windowsJsonStr.c_str());

    Window windowDefinition;
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowNonObject"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowEmptyObject"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowExtraField"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowNonNumericWidth"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowNonNumericHeight"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
            root["windowNonNumericInputScalerVTaps"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowNonStringFormat"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowNonStringPattern"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowNonStringLayout"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidLayout"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidBLLayout"],
            &windowDefinition), OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidBLLayoutWithParams"],
            &windowDefinition), OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidBlockWidth"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidBlockHeight"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidDisplayGamutType"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidColorSpace"],
            &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidColorSpace"],
            &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidColorSpaceFormat1"],
            &windowDefinition), RC::BAD_FORMAT_SPECIFICAION);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidColorSpaceFormat2"],
            &windowDefinition), RC::BAD_FORMAT_SPECIFICAION);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValid"],
            &windowDefinition), OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidWithAllFields"],
        &windowDefinition), OK);
    LwDispViewPortSettings::ViewPort viewPortIn(0, 0, 100, 100);
    LwDispViewPortSettings::ViewPort viewPortOut(10, 10, 110, 110);
    Window expectedWindowDefinition;
    expectedWindowDefinition.ownerHead = 1;
    expectedWindowDefinition.width = 120;
    expectedWindowDefinition.height = 130;
    expectedWindowDefinition.format = ColorUtils::A8R8G8B8;
    expectedWindowDefinition.tokenizedSurfacePattern = {"gradient", "color_palette", "vertical"};
    expectedWindowDefinition.displayGamutType =
            EetfLutGenerator::EetfLwrves::EETF_LWRVE_VIDEO_SDR;
    expectedWindowDefinition.surfaceRectIn = viewPortIn;
    expectedWindowDefinition.compositionRectOut = viewPortOut;
    expectedWindowDefinition.vTaps = LwDispScalerSettings::VTAPS::TAPS_5;
    expectedWindowDefinition.hTaps = LwDispScalerSettings::HTAPS::TAPS_5;
    expectedWindowDefinition.rotateDeg = 180;
    expectedWindowDefinition.flipType = Window::FlipType::HOR;
    ASSERT_THAT(windowDefinition, expectedWindowDefinition);

    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidWithMandatoryParams"],
        &windowDefinition), OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowWithoutCompositionRectOut"],
        &windowDefinition), OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowWithoutSurfaceRectIn"],
        &windowDefinition), OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowOnlyMandatoryFields"],
        &windowDefinition), OK);
}

TEST(JsonParserWindowsTester, ValidatesWindowsFieldsForYUVColorFormats)
{
    JsonParserWindows windowsObjectParser;

    string windowsJsonStr = R"(
            {"windowValidUVSwapParamWithAllFields": {"width": 120, "height": 130, 
                    "format": "Y8___U8V8_N444:swapUV",
                    "pattern": "gradient color_palette vertical", "inputScalerVTaps" : 5,
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110},
                    "inputScalerHTaps" : 5, "displayGamutTypeForTMOLut" : "SDR"},
            "windowIlwalidUVSwapParamWithAllFields": {"width": 120, "height": 130, 
                    "format": "Y8___U8V8_N444:abcd",
                    "pattern": "gradient color_palette vertical", "inputScalerVTaps" : 5,
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110},
                    "inputScalerHTaps" : 5, "displayGamutTypeForTMOLut" : "SDR"},
            "windowValidUVSwapParam": {"width": 120, "height": 130, "format": "Y8___U8V8_N444",
                    "pattern": "gradient color_palette vertical", "inputScalerVTaps" : 5,
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110},
                    "inputScalerHTaps" : 5, "displayGamutTypeForTMOLut" : "SDR"},
             "windowIlwalidFormatWithUvSwap": {"width": 120, "height": 130, "format": "A8R8G8B8:swapUV",
                    "pattern": "gradient color_palette vertical", "inputScalerVTaps" : 5,
                    "surfaceRectIn": {"xPos": 0, "yPos": 0, "xSize": 100, "ySize": 100},
                    "compositionRectOut": {"xPos": 10, "yPos": 10, "xSize": 110, "ySize": 110},
                    "inputScalerHTaps" : 5, "displayGamutTypeForTMOLut" : "SDR"}

            }
    )";

    Document root;
    root.Parse(windowsJsonStr.c_str());

    Window windowDefinition;

    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowValidUVSwapParamWithAllFields"], &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowValidUVSwapParam"], &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidUVSwapParamWithAllFields"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidFormatWithUvSwap"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);

}

TEST(JsonParserWindowsTester, ValidatesWindowsFieldsForRotate)
{
    JsonParserWindows windowsObjectParser;

    string windowsJsonStr = R"(
           {"windowValidRotate": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate": "90"},
            "windowValidRotateNeg": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate": "-180"},
            "windowIlwalidRotateEmptyArr": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate": []},
            "windowIlwalidRotateInt": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate": 90},
            "windowIlwalidRotateValue": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate": "500"},
            "windowIlwalidRotateFlipValue": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate": "180:hor"}
           })";

    Document root;
    root.Parse(windowsJsonStr.c_str());

    Window windowDefinition;

    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidRotate"],
        &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidRotateNeg"],
        &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidRotateEmptyArr"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidRotateInt"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidRotateValue"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidRotateFlipValue"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(JsonParserWindowsTester, ValidatesWindowsFieldsForFlip)
{
    JsonParserWindows windowsObjectParser;

    string windowsJsonStr = R"(
           {
            "windowValidFlip": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip": "hor"},
            "windowIlwalidFlipString": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip": "abcd"},
            "windowIlwalidFlipInt": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip": 123},
            "windowIlwalidFlipEmptyArr": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip": []},
            "windowIlwalidFlipRotateValue": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip": "hor:180"}
           })";

    Document root;
    root.Parse(windowsJsonStr.c_str());

    Window windowDefinition;

    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidFlip"],
        &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidFlipString"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidFlipInt"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidFlipEmptyArr"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidFlipRotateValue"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(JsonParserWindowsTester, ValidatesWindowsFieldsForRotateFlip)
{
    JsonParserWindows windowsObjectParser;

    string windowsJsonStr = R"(
           {"windowValidRotateFlip": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate:flip": "90:ver"},
            "windowIlwalidRotateFlipString": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate:flip": "abcd"},
            "windowIlwalidRotateFlipIlwalidRotateSize": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "rotate:flip": "90000:ver"},
            "windowIlwalidRotateFlipIlwalidFlip": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "rotate:flip": "90:abcd"},
            "windowIlwalidRotateFlipInt": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate:flip": 1234},
            "windowIlwalidRotateFlipEmptyArr": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate:flip": []},
            "windowIlwalidRotateFlipExchangedValue": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "rotate:flip": "ver:90"},
            "windowIlwalidRotateFlipEmptyRotate": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "rotate:flip": ":ver"},
            "windowValidRotateFlipEmptyFlip": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate:flip": "90:"}
           })";

    Document root;
    root.Parse(windowsJsonStr.c_str());

    Window windowDefinition;

    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidRotateFlip"],
        &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidRotateFlipString"], &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidRotateFlipIlwalidRotateSize"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidRotateFlipIlwalidFlip"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidRotateFlipInt"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidRotateFlipEmptyArr"], &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidRotateFlipExchangedValue"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidRotateFlipEmptyRotate"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowValidRotateFlipEmptyFlip"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(JsonParserWindowsTester, ValidatesWindowsFieldsForFlipRotate)
{
    JsonParserWindows windowsObjectParser;

    string windowsJsonStr = R"(
           {"windowValidFlipRotate": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip:rotate": "ver:270"},
            "windowIlwalidFlipRotateString": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip:rotate": "abcd"},
            "windowIlwalidFlipRotateIlwalidRotateSize": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "flip:rotate": "ver:90000"},
            "windowIlwalidFlipRotateIlwalidFlip": {"width": 120, "height": 130, 
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "flip:rotate": "abcd:90"},
            "windowIlwalidFlipRotateInt": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip:rotate": 1234},
            "windowIlwalidFlipRotateEmptyArr": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip:rotate": []},
            "windowIlwalidFlipRotateExchangedValue": {"width": 120, "height": 130, 
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical", 
                    "flip:rotate": "90:ver"},
            "windowIlwalidFlipRotateEmptyRotate": {"width": 120, "height": 130,
                    "format": "A8R8G8B8", "pattern": "gradient color_palette vertical",
                    "flip:rotate": "ver:"},
            "windowValidFlipRotateEmptyFlip": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "flip:rotate": ":90"},
            "windowMultipleRotateAndFlip": {"width": 120, "height": 130, "format": "A8R8G8B8",
                    "pattern": "gradient color_palette vertical", "rotate": 180, "flip": "hor",
                    "rotate:flip": "90:hor", "flip:rotate": "hor:90"}
           })";

    Document root;
    root.Parse(windowsJsonStr.c_str());

    Window windowDefinition;

    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowValidFlipRotate"],
        &windowDefinition), RC::OK);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidFlipRotateString"], &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidFlipRotateIlwalidRotateSize"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidFlipRotateIlwalidFlip"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowIlwalidFlipRotateInt"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidFlipRotateEmptyArr"], &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidFlipRotateExchangedValue"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowIlwalidFlipRotateEmptyRotate"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(
        root["windowValidFlipRotateEmptyFlip"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(windowsObjectParser.ParseSingleWindowFrame(root["windowMultipleRotateAndFlip"],
        &windowDefinition), RC::CANNOT_ENUMERATE_OBJECT);
}

//------------------------------------------------------------------------------
// JsonParserDpSinkCapsTester
//------------------------------------------------------------------------------
TEST(JsonParserDpSinkCapsTester, ParseFieldDpSinkCaps)
{
    string dpSinkCapsJsonStr = R"({
        "dpSinkCapsNonObject" : 12,
        "dpSinkCapsAllFieldsValid" : {"maxSliceWidth": 2560, "maxNumHorizontalSlices" : 24,
            "lineBufferBitDepth" : 13, "bitsPerColor" : 12, "bitsPerPixelPrecisionX16" : 8,
            "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsAllFieldsValid1" : {"maxSliceWidth": 2560, "maxNumHorizontalSlices" : 24,
            "lineBufferBitDepth" : 13, "colorDepthMask" : 4, "bitsPerPixelPrecisionX16" : 8,
            "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsExtraField" : {"maxSliceWidth": 2560, "maxNumHorizontalSlices" : 24,
            "lineBufferBitDepth" : 13, "bitsPerColor" : 12, "bitsPerPixelPrecisionX16" : 8,
            "blockPrediction" : true, "algorithmRevision" : 1.2, "extra" : true
        },
        "dpSinkCapsMissingField" : {},
        "dpSinkCapsNonNumericMaxSliceWidth" : {"maxSliceWidth": [], "maxNumHorizontalSlices" : 24,
            "lineBufferBitDepth" : 13, "bitsPerColor" : 12, "bitsPerPixelPrecisionX16" : 8,
            "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsNonNumericMaxHorizontalSlices" : {"maxSliceWidth": 2560,
            "maxNumHorizontalSlices" : [], "lineBufferBitDepth" : 13, "bitsPerColor" : 12,
            "bitsPerPixelPrecisionX16" : 8, "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsNonNumericLineBufferBitDepth" : {"maxSliceWidth": 2560,
            "maxNumHorizontalSlices" : 24, "lineBufferBitDepth" : [], "bitsPerColor" : 12,
            "bitsPerPixelPrecisionX16" : 8, "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsNonNumericBitsPerColor" : {"maxSliceWidth": 2560,
            "maxNumHorizontalSlices" : 24, "lineBufferBitDepth" : 13, "bitsPerColor" : [],
            "bitsPerPixelPrecisionX16" : 8, "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsNonNumericBitsPerPixelPrecisionX16" : {"maxSliceWidth": 2560,
            "maxNumHorizontalSlices" : 24, "lineBufferBitDepth" : 13, "bitsPerColor" : 12,
            "bitsPerPixelPrecisionX16" : [], "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsNonBooleanBlockPrediction" : {"maxSliceWidth": 2560,
            "maxNumHorizontalSlices" : 24, "lineBufferBitDepth" : 13, "bitsPerColor" : 12,
            "bitsPerPixelPrecisionX16" : 8, "blockPrediction" : {}, "algorithmRevision" : 1.2
        },
        "dpSinkCapsNonNumericAlgorithmRevision" : {"maxSliceWidth": 2560,
            "maxNumHorizontalSlices" : 24, "lineBufferBitDepth" : 13, "bitsPerColor" : 12,
            "bitsPerPixelPrecisionX16" : 8, "blockPrediction" : true, "algorithmRevision" : []
        },
        "dpSinkCapsNonNumericColorDepthMask" : {"maxSliceWidth": 2560,
            "maxNumHorizontalSlices" : 24, "lineBufferBitDepth" : 13, "colorDepthMask" : "",
            "bitsPerPixelPrecisionX16" : 8, "blockPrediction" : true, "algorithmRevision" : 1.2
        },
        "dpSinkCapsBothColorDepthAndBitsPerColor" : {"maxSliceWidth": 2560, "maxNumHorizontalSlices" : 24, 
            "lineBufferBitDepth" : 13, "colorDepthMask" : 4, "bitsPerColor" : 12, 
            "bitsPerPixelPrecisionX16" : 8, "blockPrediction" : true, "algorithmRevision" : 1.2
        }
    })";

    Document root;
    root.Parse(dpSinkCapsJsonStr.c_str());

    JsonParserDpSinkCaps dpSinkParserTester;
    DpSinkCaps dpSinkCaps;;
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonObject"], &dpSinkCaps),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsExtraField"], &dpSinkCaps),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsMissingField"], &dpSinkCaps),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonNumericMaxSliceWidth"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonNumericMaxHorizontalSlices"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonNumericLineBufferBitDepth"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonNumericBitsPerColor"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(
            root["dpSinkCapsNonNumericBitsPerPixelPrecisionX16"], &dpSinkCaps),
            RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonBooleanBlockPrediction"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonNumericAlgorithmRevision"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsNonNumericColorDepthMask"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsBothColorDepthAndBitsPerColor"],
            &dpSinkCaps), RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsAllFieldsValid"], &dpSinkCaps),
            RC::OK);
    EXPECT_THAT(dpSinkCaps.maxSliceWidth, 2560);
    EXPECT_THAT(dpSinkCaps.maxNumHztSlices, DscSliceCount::DSC_SLICES_PER_SINK_24);
    EXPECT_THAT(dpSinkCaps.lineBufferBitDepth, 13);
    EXPECT_THAT(dpSinkCaps.decoderColorDepthCaps, 4); // In form of colorDepthMask
    EXPECT_THAT(dpSinkCaps.bitsPerPixelPrecision, DSC_BITS_PER_PIXEL_PRECISION_1_2);
    EXPECT_THAT(dpSinkCaps.bBlockPrediction, true);
    EXPECT_THAT(dpSinkCaps.algorithmRevision.versionMajor, 1);
    EXPECT_THAT(dpSinkCaps.algorithmRevision.versionMinor, 2);

    ASSERT_THAT(dpSinkParserTester.ParseSinkCaps(root["dpSinkCapsAllFieldsValid1"], &dpSinkCaps),
           RC::OK);
    EXPECT_THAT(dpSinkCaps.decoderColorDepthCaps, 4);
}

TEST(JsonParserDpSinkCapsTester, ValidateValues)
{
    DpSinkCaps dpSinkCaps;
    JsonParserDpSinkCaps dpSinkParserTester;

    ASSERT_THAT(dpSinkParserTester.UpdateDpSinkCapsPostValidation(
        8, DscSliceCount::DSC_SLICES_PER_SINK_24, 13,
        12, DSC_BITS_PER_PIXEL_PRECISION_1_2,
        true, 1.2, &dpSinkCaps), OK);

    ASSERT_NE(dpSinkParserTester.UpdateDpSinkCapsPostValidation(
        2800, DscSliceCount::DSC_SLICES_PER_SINK_24, 13,
        12, DSC_BITS_PER_PIXEL_PRECISION_1_2,
        true, 1.1, &dpSinkCaps), OK);
    ASSERT_NE(dpSinkParserTester.UpdateDpSinkCapsPostValidation(
        8, 11, 13,
        12, DSC_BITS_PER_PIXEL_PRECISION_1_2,
        true, 1.1, &dpSinkCaps), OK);
    ASSERT_NE(dpSinkParserTester.UpdateDpSinkCapsPostValidation(
        8, DscSliceCount::DSC_SLICES_PER_SINK_24, 14,
        12, DSC_BITS_PER_PIXEL_PRECISION_1_2,
        true, 1.1, &dpSinkCaps), OK);
    ASSERT_NE(dpSinkParserTester.UpdateDpSinkCapsPostValidation(
        8, DscSliceCount::DSC_SLICES_PER_SINK_24, 13,
        11, DSC_BITS_PER_PIXEL_PRECISION_1_2,
        true, 1.1, &dpSinkCaps), OK);
    ASSERT_NE(dpSinkParserTester.UpdateDpSinkCapsPostValidation(
        8, DscSliceCount::DSC_SLICES_PER_SINK_24, 13,
        12, 5,
        true, 1.1, &dpSinkCaps), OK);
    ASSERT_NE(dpSinkParserTester.UpdateDpSinkCapsPostValidation(
        8, DscSliceCount::DSC_SLICES_PER_SINK_24, 13,
        12, DSC_BITS_PER_PIXEL_PRECISION_1_2,
        true, 1.3, &dpSinkCaps), OK);
}

TEST(JsonParserDpSinkCapsTester, ParseDpSinksCaps)
{
    string dpSinksCaps = R"({
        "dpSinksCaps" : {
            "dpSink1" : {"maxSliceWidth": 2560, "maxNumHorizontalSlices" : 24,
                "lineBufferBitDepth" : 13, "bitsPerColor" : 12, "bitsPerPixelPrecisionX16" : 8,
                "blockPrediction" : true, "algorithmRevision" : 1.2
            },
            "dpSink2" : {"maxSliceWidth": 2560, "maxNumHorizontalSlices" : 24,
                "lineBufferBitDepth" : 13, "bitsPerColor" : 12, "bitsPerPixelPrecisionX16" : 8,
                "blockPrediction" : true, "algorithmRevision" : 1.2
            },
            "dpSink3" : {"maxSliceWidth": 2560, "maxNumHorizontalSlices" : 24,
                "lineBufferBitDepth" : 13, "bitsPerColor" : 12, "bitsPerPixelPrecisionX16" : 8,
                "blockPrediction" : true, "algorithmRevision" : 1.2
            }
        }
    })";

    Document root;
    root.Parse(dpSinksCaps.c_str());

    DpSinksCapsMap dpSinkCapsMap;
    JsonParserDpSinkCaps dpSinkParserTester;

    ASSERT_THAT(dpSinkParserTester.Parse(root["dpSinksCaps"], &dpSinkCapsMap), OK);
    ASSERT_THAT(dpSinkCapsMap.size(), 3);
}

TEST(JsonParserPattern, ParseLwstomPattern)
{
    string lwstomPattern = R"({
            "patterlwalid": "custom:0xffaaaaaa,0xffaaaaaa,0xff555555,0xff555555",
            "patternMissingColon": "custom,0xffaaaaaa,0xffaaaaaa,0xff555555,0xff555555",
            "patternNoNumbers": "custom:",
            "patternIlwalidNumbers": "custom:UYT,0xABCD"
        })";

    Document root;
    root.Parse(lwstomPattern.c_str());

    vector<string> tokenizedSurfacePattern;
    vector<UINT32> lwstomPatternData;
    vector<string> tokenizedRightSurfacePattern;

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternMissingColon"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternNoNumbers"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternIlwalidNumbers"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window", root["patterlwalid"].GetString(),
        &tokenizedSurfacePattern, &tokenizedRightSurfacePattern, &lwstomPatternData), RC::OK);
    ASSERT_THAT(tokenizedSurfacePattern.size(), 2);
    ASSERT_THAT(lwstomPatternData.size(), 4);
}

TEST(JsonParserPattern, ParseFlipPrev)
{
    string lwstomPattern = R"({
            "patterlwalid": "flipPrev",
            "patternIlwalid": "flipPrev:0x10"
        })";

    Document root;
    root.Parse(lwstomPattern.c_str());

    vector<string> tokenizedSurfacePattern;
    vector<UINT32> lwstomPatternData;
    vector<string> tokenizedRightSurfacePattern;

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window", root["patternIlwalid"].GetString(),
        &tokenizedSurfacePattern, &tokenizedRightSurfacePattern, &lwstomPatternData),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window", root["patterlwalid"].GetString(),
        &tokenizedSurfacePattern, &tokenizedRightSurfacePattern, &lwstomPatternData), RC::OK);
    ASSERT_THAT(tokenizedSurfacePattern.size(), 3); // "na" "na" is appended

}

TEST (JsonParserPattern, ValueMayHaveEitherPipeOrColonButNotBoth)
{
    string pattern = R"({
        "patterlwalid1": "gradient color_palette vertical|fp_gray na na",
        "patterlwalid2": "custom:0xffaaaaaa,0xffaaaaaa,0xff555555,0xff555555",
        "patternIlwalid":
            "gradient color_palette vertical|custom:0xffaaaaaa,0xffaaaaaa,0xff555555,0xff555555"
        })";

    Document root;
    root.Parse(pattern.c_str());

    vector<string> tokenizedSurfacePattern;
    vector<UINT32> lwstomPatternData;
    vector<string> tokenizedRightSurfacePattern;

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window", root["patternIlwalid"].GetString(),
        &tokenizedSurfacePattern, &tokenizedRightSurfacePattern, &lwstomPatternData),
        RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window", root["patterlwalid1"].GetString(),
        &tokenizedSurfacePattern, &tokenizedRightSurfacePattern, &lwstomPatternData), RC::OK);
    ASSERT_THAT(tokenizedSurfacePattern.size(), 1);
    ASSERT_THAT(tokenizedRightSurfacePattern.size(), 1);

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window", root["patterlwalid2"].GetString(),
        &tokenizedSurfacePattern, &tokenizedRightSurfacePattern, &lwstomPatternData), RC::OK);
    ASSERT_THAT(tokenizedSurfacePattern.size(), 2);
    ASSERT_THAT(lwstomPatternData.size(), 4);

}

TEST (JsonParserPattern, ParseStereoPattern)
{
    string stereoPattern = R"({
            "patterlwalid": "gradient color_palette vertical|fp_gray na na",
            "patternMissingAfterPipe": "gradient color_palette vertical|",
            "patternMissingBeforePipe": "|gradient color_palette vertical",
            "patternIlwalidAfterPipe": "gradient color_palette vertical|123AB",
            "patternIlwalidBeforePipe": "123.png|gradient color_palette vertical",
            "patternWithMoreThanOnePipe":
                "gradient color_palette vertical|gradient color_palette vertical|fp_gray na na"
        })";

    Document root;
    root.Parse(stereoPattern.c_str());

    vector<string> tokenizedSurfacePattern;
    vector<UINT32> lwstomPatternData;
    vector<string> tokenizedRightSurfacePattern;

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternMissingAfterPipe"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternMissingBeforePipe"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternIlwalidAfterPipe"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternIlwalidBeforePipe"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);
    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window",
        root["patternWithMoreThanOnePipe"].GetString(), &tokenizedSurfacePattern,
        &tokenizedRightSurfacePattern, &lwstomPatternData), RC::CANNOT_ENUMERATE_OBJECT);

    ASSERT_THAT(SimpleStructsParser::ParsePattern("Window", root["patterlwalid"].GetString(),
        &tokenizedSurfacePattern, &tokenizedRightSurfacePattern, &lwstomPatternData), RC::OK);
    ASSERT_THAT(tokenizedSurfacePattern.size(), 1);
    ASSERT_THAT(tokenizedRightSurfacePattern.size(), 1);
}
