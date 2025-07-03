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

#include <iostream>
#include <fstream>
#include <string>

#include "gmock/gmock.h"
using namespace std;

#include "gpu/display/lwdisplay/iot_config_parser.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
using namespace testing;
using namespace rapidjson;

#include "gmock/gmock.h"

// --------------------------------------------------------------
class ICPTester : public Test
{
public:
    IotConfigParser parser;
};

TEST_F(ICPTester, ReturnsErrorIfJsonConfigLoadFails)
{
    ASSERT_THAT(parser.Load("randomPath.json"), RC::CANNOT_OPEN_FILE);
}

TEST_F(ICPTester, ParseFailsIfFileNotLoaded)
{
    IotConfigurationExt iotConfig;
    ASSERT_THAT(parser.Parse(&iotConfig), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ICPRootParseTester, ReturnsErrorOnUnsupportedField)
{
    string missingVersionJson = R"(
    {
        "UNSUPPORTED": "JUNK",
        "superSwitchVersion" : 1,
        "ttyPort": "/dev/ttyUSB0",
        "portMap": {
            "0x1000": {
                "port": "i02",
                "stressModes": ["1600x1200x24x60, 0xf36144ec,random.png",
                                "800x600x32x60, 0xf2345AB,random.png"]
            }
        },
        "pollTimeoutMs": 2000,
        "handShakeComand": "sw i00",
        "handShakeResponse": "sw i00 Command incorrect"
    })";
    Document root;
    root.Parse(missingVersionJson.c_str());

    IotConfigParser parser;
    IotConfigurationExt iotConfig;
    ASSERT_THAT(parser.ParseRootConfig(root, &iotConfig), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ICPRootParseTester, ReturnsErrorOnMissinglMandatoryFields)
{
    string missingVersionJson = R"(
    {
        "ttyPort": "/dev/ttyUSB0",
        "portMap": {
            "0x1000": {
                "port": "i02",
                "stressModes": ["1600x1200x24x60, 0xf36144ec,random.png",
                                "800x600x32x60, 0xf2345AB,random.png"]
            }
        },
        "pollTimeoutMs": 2000,
        "handShakeComand": "sw i00",
        "handShakeResponse": "sw i00 Command incorrect"
    })";
    Document root;
    root.Parse(missingVersionJson.c_str());

    IotConfigParser parser;
    IotConfigurationExt iotConfig;
    ASSERT_THAT(parser.ParseRootConfig(root, &iotConfig), RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(ICPRootParseTester, ReturnsErrorOnDuplicateField)
{
    string missingVersionJson = R"(
    {
        "superSwitchVersion" : 1,
        "superSwitchVersion" : 1,
        "ttyPort": "/dev/ttyUSB0",
        "portMap": {
            "0x1000": {
                "port": "i02",
                "stressModes": ["1600x1200x24x60, 0xf36144ec,random.png",
                                "800x600x32x60, 0xf2345AB,random.png"]
            }
        },
        "pollTimeoutMs": 2000,
        "handShakeComand": "sw i00",
        "handShakeResponse": "sw i00 Command incorrect"
    })";
    Document root;
    root.Parse(missingVersionJson.c_str());

    IotConfigParser parser;
    IotConfigurationExt iotConfig;
    ASSERT_THAT(parser.ParseRootConfig(root, &iotConfig), RC::CANNOT_ENUMERATE_OBJECT);
}

class ICPTopLevelFieldTester : public Test
{
public:
    IotConfigParser parser;
    Document root;
    IotConfigurationExt iotConfig;

    void CheckForValid(const string& jsonString, const string& key)
    {
        ASSERT_THAT(Parse(jsonString, key), true);
        ASSERT_THAT(parser.ParseTopLevelField(key, root[key.c_str()], &iotConfig), RC::OK);
    }

    void CheckForIlwalid(const string& jsonString, const string& key)
    {
        ASSERT_THAT(Parse(jsonString, key), true);
        ASSERT_THAT(parser.ParseTopLevelField(key, root[key.c_str()], &iotConfig),
            RC::CANNOT_ENUMERATE_OBJECT);
    }

    bool Parse(const string& jsonString, const string& key)
    {
        root.Parse(jsonString.c_str());
        if (root.HasParseError())
        {
            Printf(Tee::PriNormal, "Error in parsing\n");
            return false;
        }
        auto it = root.FindMember(key.c_str());
        if (it == root.MemberEnd())
        {
            Printf(Tee::PriNormal, "Member not found\n");
            return false;
        }

        return true;
    }

};

TEST_F(ICPTopLevelFieldTester, ValidatesSuperSwitchVersion)
{
    string validVersionJson = R"(
    {
        "superSwitchVersion" : 1
    })";

    string ilwalidVersionJson = R"(
    {
        "superSwitchVersion" : "1"
    })";

    string unsupportedVersionJson = R"(
    {
        "superSwitchVersion" : 3
    })";

    CheckForValid(validVersionJson, IotConfigParser::JSON_PARAM_SUPERSWITCH_VERSION);
    CheckForIlwalid(ilwalidVersionJson, IotConfigParser::JSON_PARAM_SUPERSWITCH_VERSION);
    CheckForIlwalid(unsupportedVersionJson, IotConfigParser::JSON_PARAM_SUPERSWITCH_VERSION);
}

TEST_F(ICPTopLevelFieldTester, ValidatesTtyPortAsSupportedString)
{
    string validTtyPort = R"(
    {
        "ttyPort" : "/dev/tty0"
    })";

    string ilwalidTtyPort = R"(
    {
        "ttyPort" : 0
    })";

    CheckForValid(validTtyPort, IotConfigParser::JSON_PARAM_TTYPORT);
    CheckForIlwalid(ilwalidTtyPort, IotConfigParser::JSON_PARAM_TTYPORT);
}

TEST_F(ICPTopLevelFieldTester, ValidatesPollTimeoutMs)
{
    string validPollTimeoutMsJson = R"(
    {
        "pollTimeoutMs" : 2000
    })";

    string ilwalidPollTimeoutMsJson = R"(
    {
        "pollTimeoutMs" : "1"
    })";

    CheckForValid(validPollTimeoutMsJson, IotConfigParser::JSON_PARAM_POLL_TIMEOUTMS);
    CheckForIlwalid(ilwalidPollTimeoutMsJson, IotConfigParser::JSON_PARAM_POLL_TIMEOUTMS);
}

TEST_F(ICPTopLevelFieldTester, ValidatesHandshakeCommand)
{
    string validHandshakeCommandJson = R"(
    {
        "handShakeComand" : "sw i00"
    })";

    string ilwalidHandshakeCommandJson = R"(
    {
        "handShakeComand" : []
    })";

    CheckForValid(validHandshakeCommandJson, IotConfigParser::JSON_PARAM_HANDSHAKE_COMAND);
    CheckForIlwalid(ilwalidHandshakeCommandJson, IotConfigParser::JSON_PARAM_HANDSHAKE_COMAND);
}

TEST_F(ICPTopLevelFieldTester, ValidatesHandshakeResponse)
{
    string validHandshakeResponseJson = R"(
    {
        "handShakeResponse" : "sw i00 Command incorrect"
    })";

    string ilwalidHandshakeResponseJson = R"(
    {
        "handShakeResponse" : []
    })";

    CheckForValid(validHandshakeResponseJson, IotConfigParser::JSON_PARAM_HANDSHAKE_RESPONSE);
    CheckForIlwalid(ilwalidHandshakeResponseJson, IotConfigParser::JSON_PARAM_HANDSHAKE_RESPONSE);
}

TEST_F(ICPTopLevelFieldTester, ValidatesMutePort)
{
    string validJson = R"(
    {
        "mutePort" : "i20"
    })";

    string ilwalidJson = R"(
    {
        "mutePort" : []
    })";

    CheckForValid(validJson, IotConfigParser::JSON_PARAM_MUTE_PORT);
    CheckForIlwalid(ilwalidJson, IotConfigParser::JSON_PARAM_MUTE_PORT);
}
TEST_F(ICPTopLevelFieldTester, ValidatesSuperSwitchDebugPrintPriority)
{
    string validSuperSwitchDebugPrintPriorityJson = R"(
    {
        "superSwitchDebugPrintPriority" : "low"
    })";

    string ilwalidSuperSwitchDebugPrintPriorityJson = R"(
    {
        "superSwitchDebugPrintPriority" : false
    })";

    string unsupportedSuperSwitchDebugPrintPriorityJson = R"(
    {
        "superSwitchDebugPrintPriority" : "medium"
    })";

    CheckForValid(validSuperSwitchDebugPrintPriorityJson,
        IotConfigParser::JSON_PARAM_SUPERSWITCH_DEBUG_PRINT_PRIORITY);
    CheckForIlwalid(ilwalidSuperSwitchDebugPrintPriorityJson,
        IotConfigParser::JSON_PARAM_SUPERSWITCH_DEBUG_PRINT_PRIORITY);
    CheckForIlwalid(unsupportedSuperSwitchDebugPrintPriorityJson,
        IotConfigParser::JSON_PARAM_SUPERSWITCH_DEBUG_PRINT_PRIORITY);
}

TEST_F(ICPTopLevelFieldTester, ValidatesHpdDebugPrintPriority)
{
    string validHpdDebugPrintPriorityJson = R"(
    {
        "hpdDebugPrintPriority" : "low"
    })";

    string ilwalidHpdDebugPrintPriorityJson = R"(
    {
        "hpdDebugPrintPriority" : false
    })";

    string unsupportedHpdDebugPrintPriorityJson = R"(
    {
        "hpdDebugPrintPriority" : "medium"
    })";

    CheckForValid(validHpdDebugPrintPriorityJson,
        IotConfigParser::JSON_PARAM_HPD_DEBUG_PRINT_PRIORITY);
    CheckForIlwalid(ilwalidHpdDebugPrintPriorityJson,
        IotConfigParser::JSON_PARAM_HPD_DEBUG_PRINT_PRIORITY);
    CheckForIlwalid(unsupportedHpdDebugPrintPriorityJson,
        IotConfigParser::JSON_PARAM_HPD_DEBUG_PRINT_PRIORITY);
}

TEST(PortMapTester, MustBeANonEmptyObjectWithValidMembers)
{
    string ilwalidPortMapJson = R"(
    {
        "portMap" : "low"
    })";
    string ilwalidPortMapJson1 = R"(
    {
        "portMap" : {}
    })";
    string ilwalidPortMapJson2 = R"(
    {
        "portMap" : {"Hello": {}}
    })";
    string ilwalidPortMapJson3 = R"(
    {
        "portMap" : {"0x100": {}}
    })";

    IotConfigParser parser;
    Document root;
    IotConfigurationExt iotConfig;

    root.Parse(ilwalidPortMapJson.c_str());
    ASSERT_THAT(parser.ParsePortMap(root[IotConfigParser::JSON_PARAM_PORTMAP], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidPortMapJson1.c_str());
    ASSERT_THAT(parser.ParsePortMap(root[IotConfigParser::JSON_PARAM_PORTMAP], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidPortMapJson2.c_str());
    ASSERT_THAT(parser.ParsePortMap(root[IotConfigParser::JSON_PARAM_PORTMAP], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidPortMapJson3.c_str());
    ASSERT_THAT(parser.ParsePortMap(root[IotConfigParser::JSON_PARAM_PORTMAP], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);
}

TEST(PortMapMemberTester, MustBeANonEmptyObjectWithValidMembers)
{
    string ilwalidJson = R"(
    {
       "0x1000" : "low"
    })";
    string ilwalidJson1 = R"(
    {
       "0x1000" : {}
    })";
    string ilwalidJson2 = R"(
    {
       "0x1000" : {"Hello": {}}
    })";
    string ilwalidJson3 = R"(
    {
       "0x1000" : {
           "port": 2,
           "stressModes": ["1600x1200x24x60, 0xf36144ec,random.png",
                           "1600x1200x24x60, 0xf36144ec,random.png"]
       }
    })";
    string ilwalidJson4 = R"(
    {
       "0x1000" : {
           "port": "i02",
           "stressModes": ""
       }
    })";
    string ilwalidJson5 = R"(
    {
       "0x1000" : {
           "port": "i02",
           "stressModes": "",
           "skip" : ""
       }
    })";
    string validJson = R"(
    {
       "0x1000" : {
           "port": "i02",
           "stressModes": ["1600x1200x24x60, 0xf36144ec,random.png",
                           "1600x1200x24x60, 0xf36144ec,random.png"],
           "skip" : true
       }
    })";

    IotConfigParser parser;
    Document root;

    DisplayID displayID = 0x1000;

    IotConfigurationExt iotConfig;

    root.Parse(ilwalidJson.c_str());
    ASSERT_THAT(parser.ParsePortMapMember(displayID, root["0x1000"], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson1.c_str());
    ASSERT_THAT(parser.ParsePortMapMember(displayID, root["0x1000"], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson2.c_str());
    ASSERT_THAT(parser.ParsePortMapMember(displayID, root["0x1000"], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson3.c_str());
    ASSERT_THAT(parser.ParsePortMapMember(displayID, root["0x1000"], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson4.c_str());
    ASSERT_THAT(parser.ParsePortMapMember(displayID, root["0x1000"], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson5.c_str());
    ASSERT_THAT(parser.ParsePortMapMember(displayID, root["0x1000"], &iotConfig),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(validJson.c_str());
    ASSERT_THAT(parser.ParsePortMapMember(displayID, root["0x1000"], &iotConfig),
        RC::OK);
}

TEST(StessModeTester, MustBeANonEmptyStringWithValidMembers)
{
    string ilwalidJson1 = R"(
    {
       "stressModes0": {}
    })";

    string ilwalidJson2 = R"(
    {
       "stressModes0": ""
    })";

    string ilwalidJson3 = R"(
    {
       "stressModes0": "1600x1200x24x60"
    })";

    string ilwalidJson4 = R"(
    {
       "stressModes0": "1600x1200x24x60, CRC, iamge.png"
    })";

    string ilwalidJson5 = R"(
    {
       "stressModes0": "1600x24x60, 0xf36144ec, iamge.png"
    })";

    string ilwalidJson6 = R"(
    {
       "stressModes0": "1600x24xMx60, 0xf36144ec, iamge.png"
    })";

    string validJson = R"(
    {
       "stressModes0": "1600x1200x24x60, 0xf36144ec, random.png"
    })";

    IotConfigParser parser;
    Document root;
    IotConfigurationExt::StressMode stressMode;

    root.Parse(ilwalidJson1.c_str());
    ASSERT_THAT(parser.ParseStressMode(root["stressModes0"], &stressMode),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson2.c_str());
    ASSERT_THAT(parser.ParseStressMode(root["stressModes0"], &stressMode),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson3.c_str());
    ASSERT_THAT(parser.ParseStressMode(root["stressModes0"], &stressMode),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson4.c_str());
    ASSERT_THAT(parser.ParseStressMode(root["stressModes0"], &stressMode),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson5.c_str());
    ASSERT_THAT(parser.ParseStressMode(root["stressModes0"], &stressMode),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(ilwalidJson6.c_str());
    ASSERT_THAT(parser.ParseStressMode(root["stressModes0"], &stressMode),
        RC::CANNOT_ENUMERATE_OBJECT);

    root.Parse(validJson.c_str());
    ASSERT_THAT(parser.ParseStressMode(root["stressModes0"], &stressMode),
        RC::OK);
}

TEST(IotConfigFullTester, ValidatesParsedValues)
{

    string validPortMapJson = R"(
    {
        "superSwitchVersion" : 1,
        "ttyPort": "/dev/ttyUSB0",
        "mutePort" : "i20",
        "portMap" : {
            "0x1000" : {
                "port": "i02",
                "stressModes": ["1600x1200x24x60, 0xf36144ec,random.png",
                                "800x600x32x60, 0xf2345AB,another.png"]
            },
            "0x2000" : {
                "port": "i01",
                "stressModes": ["1600x1200x24x60, 0xf36ec,random"]
            },
            "0x200" : {
                "port": "i21",
                "stressModes": ["1600x1200x24x60, 0xf36ec,random"],
                "skip" : true
            }
        },
        "pollTimeoutMs": 2000,
        "handShakeComand": "sw i00",
        "handShakeResponse": "sw i00 Command incorrect",
        "superSwitchDebugPrintPriority": "low",
        "hpdDebugPrintPriority": "high"
    })";

    IotConfigParser parser;
    Document root;

    root.Parse(validPortMapJson.c_str());
    IotConfigurationExt iotConfig;

    ASSERT_THAT(parser.ParseRootConfig(root, &iotConfig), RC::OK);

    ASSERT_THAT(iotConfig.portMap.size(), 2);
    auto it1 = iotConfig.portMap.find(0x1000);
    ASSERT_THAT(it1->second, "i02");
    auto it2 = iotConfig.portMap.find(0x2000);
    ASSERT_THAT(it2->second, "i01");

    ASSERT_THAT(iotConfig.stressModesInfo.size(), 2);
    ASSERT_THAT(iotConfig.stressModesInfo[0].first, 0x1000);
    ASSERT_THAT(iotConfig.stressModesInfo[0].second.size(), 2);
    ASSERT_THAT(iotConfig.stressModesInfo[1].first, 0x2000);
    ASSERT_THAT(iotConfig.stressModesInfo[1].second.size(), 1);

    IotConfigurationExt::StressModes stressModes0 = iotConfig.stressModesInfo[0].second;
    IotConfigurationExt::StressModes stressModes1 = iotConfig.stressModesInfo[1].second;

    ASSERT_THAT(stressModes0.size(), 2);
    ASSERT_THAT(stressModes1.size(), 1);

    IotConfigurationExt::StressMode mode(1600, 1200, 24, 60, 0xf36144ec, "random.png");
    ASSERT_THAT(stressModes0[0], mode);

    IotConfigurationExt::StressMode mode1(800, 600, 32, 60, 0xf2345AB, "another.png");
    ASSERT_THAT(stressModes0[1], mode1);

    IotConfigurationExt::StressMode mode2(1600, 1200, 24, 60, 0xf36ec, "random");
    ASSERT_THAT(stressModes1[0], mode2);

    ASSERT_THAT(iotConfig.superSwitchVersion, 1);
    ASSERT_THAT(iotConfig.ttyPort, "/dev/ttyUSB0");
    ASSERT_THAT(iotConfig.pollTimeoutMs, 2000);
    ASSERT_THAT(iotConfig.handShakeComand, "sw i00");
    ASSERT_THAT(iotConfig.handShakeResponse, "sw i00 Command incorrect");
    ASSERT_THAT(iotConfig.printPriority, Tee::PriLow);
    ASSERT_THAT(iotConfig.hpdDebugPrintPriority, Tee::PriNormal);
}
