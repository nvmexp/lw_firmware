/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/platform.h"
#include "device/interface/i2c.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/avfssub.h"
#include "gpu/perf/voltsub.h"
#include "gpu/tests/gputest.h"

// RapidJson includes
#include "document.h"
#include "error/en.h"
#include "filereadstream.h"

namespace Json = rapidjson;

#include <sstream>

class PwrRegCheck : public GpuTest
{
public:
    PwrRegCheck()
    {
        SetName("PwrRegCheck");
    }

    bool IsSupported();

    RC Setup();
    RC Run();
    RC Cleanup();

    SETGET_PROP(BoardName, string);
    SETGET_PROP(SkipI2cWriteLockCheck, bool);
    SETGET_PROP(SkipSwRevCheck, bool);
    SETGET_PROP(SkipVoltageCheck, bool);
    SETGET_PROP(HbmExpectedVOut, UINT32);

    GET_PROP(VOutTolFixedmV, UINT32);
    RC SetVOutTolFixedmV(UINT32 val)
    {
        m_VOutTolFixedmV = val;
        m_UseFixedTol = true;
        return OK;
    }

    FLOAT32 GetVOutTolPercent()
    {
        // Colwert to percentage
        return m_VOutTolPercent * 100;
    }
    RC SetVOutTolPercent(FLOAT32 val)
    {
        // Colwert from percentage
        m_VOutTolPercent = val / 100;
        m_UsePercentTol = true;
        return OK;
    }

    GET_PROP(ConfigFile, string);
    RC SetConfigFile(string configFile)
    {
        m_ConfigFile = configFile;
        m_UseLwstomConfig = true;
        return OK;
    }

    string GetSwRevExpectedValue();
    RC SetSwRevExpectedValue(string val);

private:

    // Struct to hold the device-specific addresses
    // for the values on a power regulator
    struct PwrRegData
    {
        UINT32 pageReg           = 0x0;
        UINT32 pageMsgLen        = 1;
        UINT32 devidReg          = 0x28;
        UINT32 devidPage         = ~0U;
        UINT32 devidMsgLen       = 1;
        UINT32 devidVal          = 0x88;
        UINT32 configCodeReg     = 0x46;
        UINT32 configCodePage    = ~0U;
        UINT32 configCodeMsgLen  = 2;
        UINT32 swRevReg          = 0x29;
        UINT32 swRevPage         = ~0U;
        UINT32 swRevMsgLen       = 1;
        UINT32 statusCmlReg      = 0x7E;
        UINT32 statusCmlPage     = ~0U;
        UINT32 statusCmlMsgLen   = 1;
        UINT32 statusCmlMask     = 0x8;
        UINT32 readVOutReg       = 0x8B;
        UINT32 readVOutPage      = ~0U;
        UINT32 readVOutMsgLen    = 2;
        UINT32 fwVerReg          = 0x47;
        UINT32 fwVerPage         = ~0U;
        UINT32 fwVerMsgLen       = 2;
    };

    // Struct to hold the I2C communication address and
    // expected configuration for the values lwrrently checked
    struct PwrRegConfig
    {
        string name           = "";
        string regulator      = "MP2888A";
        UINT32 i2cPort        = 0x2;
        UINT32 i2cAddr        = 0x0;
        UINT32 configCodeVal   = 0x0008;
        UINT32 swRevVal       = 0x1;
        UINT32 statusCmlVal   = 0x0;
        UINT32 expectedMv     = 0;
        double voutScale      = 1.0;
        UINT32 voutTolMv      = ~0U;
    };

    bool m_UseLwstomConfig = false;
    string m_ConfigFile = "pwrregcheck_cfg.json";
    string m_BoardName;

    // WAR 1920931
    // I2c Writes will eventually be locked, but until
    // then we can't fail if they aren't locked yet
    bool m_SkipI2cWriteLockCheck = false;

    // WAR 1920931
    // LwVdd's and HbmVdd's swRevVals are dependent on board and VBIOS
    // version. Using a config file to match a board/VBIOS to a SW Revision
    // would get far too lwmbersome, and would probably cause a lot of
    // failures if/when the config file wasn't updated immediately.
    // Until we have a way to query VBIOS for what the expected values
    // should be for SW Revision, we're skipping the SW Rev check.
    bool m_SkipSwRevCheck = false;

    // WAR bug 200348479 and 200407176
    bool m_SkipVoltageCheck = false;

    // Tolerance between the expected voltage and the voltage
    // reported by the regulator. Can be specified as either a
    // percentage of expected voltage or a fixed difference.
    FLOAT32 m_VOutTolPercent = 0.0f;
    UINT32 m_VOutTolFixedmV = 13;
    bool m_UsePercentTol = false;
    bool m_UseFixedTol = false;

    UINT32 m_HbmExpectedVOut = 0;
    map<string, UINT32> m_SwRevExpectedValue;

    map<string, PwrRegData> m_PwrRegDataMap;
    vector<PwrRegConfig> m_BoardPwrRegs;

    // Need to disable/re-enable CLFC and CLVC for NAFLL clock domains
    vector<Gpu::ClkDomain> m_ClfcClkDoms;
    set<Gpu::SplitVoltageDomain> m_ClvcVoltDoms = {};

    Perf::PStateLockType m_OrigPerfLock = Perf::DefaultLock;

    RC GetPwrRegInfo
    (
        map<string, PwrRegData>* pPwrRegDataMap,
        vector<PwrRegConfig>* pBoardPwrRegs,
        string* pErrorMsg
    );

    RC CheckPwrReg(const PwrRegConfig& pwrRegConfig);
    RC LockSimulatedTemp(UINT32 degC);
    RC UnlockSimulatedTemp();

    static RC ParseJson(const string& filename, Json::Document* pDoc);

    static RC GetJsonString
    (
        const Json::Value& jsonObj,
        const string& valName,
        string* pRetVal
    );

    static RC GetJsonUintFromString
    (
        const Json::Value& jsonObj,
        const string& valName,
        bool  bRequired,
        UINT32* pRetVal
    );

    static RC GetJsonRegFromString
    (
        const Json::Value& jsonObj,
        const string& valName,
        bool  bRequired,
        UINT32* pRegPage,
        UINT32* pRegAddr
    );

    static RC CheckHasMember
    (
        const Json::Value& jsonObj,
        const string& memberName
    );
};

//-----------------------------------------------------------------------------
bool PwrRegCheck::IsSupported()
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return false;
    }

    UINT32 index;

    GpuSubdevice* pGpuSubdev = GetBoundGpuSubdevice();
    if (!pGpuSubdev->HasFeature(Device::GPUSUB_SUPPORTS_PWR_REG_CHECK))
    {
        return false;
    }

    if (!pGpuSubdev->SupportsInterface<I2c>())
    {
        return false;
    }

    RC rc;
    if (m_BoardName.empty())
    {
        rc = pGpuSubdev->GetBoardInfoFromBoardDB(&m_BoardName, &index, false);
        if (rc != OK)
        {
            VerbosePrintf("Problem getting board info in PwrRegCheck: %s\n", rc.Message());
            return false;
        }
    }

    if (Utility::FindPkgFile(m_ConfigFile) == Utility::NoSuchFile)
    {
        // If the test can't find the config file, it's not supported
        // Report an error if the user specified the file
        Printf(m_UseLwstomConfig ? Tee::PriError : Tee::PriLow,
            "File %s does not exist\n", m_ConfigFile.c_str());
        return false;
    }
    string err;
    rc = GetPwrRegInfo(&m_PwrRegDataMap, &m_BoardPwrRegs, &err);
    if (rc != OK)
    {
        VerbosePrintf("Problem getting board info from config file: %s\n", rc.Message());
        if (!err.empty())
        {
            VerbosePrintf("%s\n", err.c_str());
        }

        // Only silently skip for unknown board descriptions.  Any other failues shoudl proceed
        // to Run and generate a test failure, this prevents a malformed JSON file from being
        // submitted
        return (rc == RC::UNKNOWN_BOARD_DESCRIPTION) ? false : true;
    }

    if (m_BoardPwrRegs.size() == 0)
    {
        VerbosePrintf("Board %s not found in PwrRegConfig file\n", m_BoardName.c_str());
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
RC PwrRegCheck::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    if (m_UseFixedTol && m_UsePercentTol)
    {
        Printf(Tee::PriError, "Cannot specify both a fixed and a percent tolerance!\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Save the status of all CLFCs/CLVCs so that we can restore them correctly later
    Avfs* pAvfs = GetBoundGpuSubdevice()->GetAvfs();
    CHECK_RC(pAvfs->GetEnabledFreqControllers(&m_ClfcClkDoms));
    CHECK_RC(pAvfs->SetFreqControllerEnable(m_ClfcClkDoms, false));
    CHECK_RC(pAvfs->GetEnabledVoltControllers(&m_ClvcVoltDoms));
    CHECK_RC(pAvfs->SetVoltControllerEnable(m_ClvcVoltDoms, false));

    // Lock a simulated temperature of 70C to prevent voltage from changing at
    // intersect and also to get the lowest supported voltage.
    CHECK_RC(LockSimulatedTemp(70));

    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    m_OrigPerfLock = pPerf->GetPStateLockType();
    CHECK_RC(pPerf->SetForcedPStateLockType(Perf::HardLock));

    UINT32 index;

    if (m_BoardName.empty())
    {
        CHECK_RC(GetBoundGpuSubdevice()->GetBoardInfoFromBoardDB(&m_BoardName, &index, false));
    }
    string err;
    rc = GetPwrRegInfo(&m_PwrRegDataMap, &m_BoardPwrRegs, &err);
    if (rc != OK)
    {
        Printf(Tee::PriError,
            "Error getting power regulator information. %s\n", err.c_str());
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PwrRegCheck::Run()
{
    StickyRC rc;

    for (PwrRegConfig pwrReg : m_BoardPwrRegs)
    {
        rc = CheckPwrReg(pwrReg);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PwrRegCheck::Cleanup()
{
    StickyRC rc;
    rc = GetBoundGpuSubdevice()->GetPerf()->SetForcedPStateLockType(m_OrigPerfLock);
    rc = GetBoundGpuSubdevice()->GetAvfs()->SetVoltControllerEnable(m_ClvcVoltDoms, true);
    rc = GetBoundGpuSubdevice()->GetAvfs()->SetFreqControllerEnable(m_ClfcClkDoms, true);
    rc = UnlockSimulatedTemp();
    rc = GpuTest::Cleanup();
    return rc;
}

//-----------------------------------------------------------------------------
RC PwrRegCheck::CheckPwrReg(const PwrRegConfig& pwrRegConfig)
{
    RC rc;
    StickyRC stickyRc;
    UINT32 data = 0;

    const string& name = pwrRegConfig.name;

    if (m_PwrRegDataMap.count(pwrRegConfig.regulator) == 0)
    {
        Printf(Tee::PriError, "Power Regulator %s not defined!\n", pwrRegConfig.regulator.c_str());
        return RC::UNKNOWN_BOARD_DESCRIPTION;
    }

    PwrRegData pwrRegData = m_PwrRegDataMap[pwrRegConfig.regulator];

    I2c::Dev i2c = GetBoundGpuSubdevice()->GetInterface<I2c>()->I2cDev(pwrRegConfig.i2cPort, pwrRegConfig.i2cAddr);

    i2c.SetMsbFirst(false);

    if (pwrRegData.devidPage != ~0U)
    {
        i2c.SetMessageLength(pwrRegData.pageMsgLen);
        CHECK_RC(i2c.Write(pwrRegData.pageReg, pwrRegData.devidPage));
    }
    i2c.SetMessageLength(pwrRegData.devidMsgLen);
    CHECK_RC(i2c.Read(pwrRegData.devidReg, &data));
    VerbosePrintf("Data from %s %#x: %#x\n", name.c_str(), pwrRegData.devidReg, data);
    if (data != pwrRegData.devidVal)
    {
        Printf(Tee::PriError, "Invalid Devid for %s: Expected %#x Actual %#x\n",
            name.c_str(), pwrRegData.devidVal, data);
        stickyRc = RC::INCORRECT_FEATURE_SET;
    }

    if (pwrRegData.configCodePage != ~0U)
    {
        i2c.SetMessageLength(pwrRegData.pageMsgLen);
        CHECK_RC(i2c.Write(pwrRegData.pageReg, pwrRegData.configCodePage));
    }
    i2c.SetMessageLength(pwrRegData.configCodeMsgLen);
    CHECK_RC(i2c.Read(pwrRegData.configCodeReg, &data));
    VerbosePrintf("Data from %s %#x: %#x\n", name.c_str(), pwrRegData.configCodeReg, data);
    if (data != pwrRegConfig.configCodeVal)
    {
        Printf(Tee::PriError, "Invalid Config Code for %s: Expected %#x Actual %#x\n",
            name.c_str(), pwrRegConfig.configCodeVal, data);
        stickyRc = RC::INCORRECT_FEATURE_SET;
    }

    if (pwrRegData.swRevPage != ~0U)
    {
        i2c.SetMessageLength(pwrRegData.pageMsgLen);
        CHECK_RC(i2c.Write(pwrRegData.pageReg, pwrRegData.swRevPage));
    }
    i2c.SetMessageLength(pwrRegData.swRevMsgLen);
    CHECK_RC(i2c.Read(pwrRegData.swRevReg, &data));
    VerbosePrintf("Data from %s %#x: %#x\n", name.c_str(), pwrRegData.swRevReg, data);
    if (!m_SkipSwRevCheck)
    {
        UINT32 expectedVal = pwrRegConfig.swRevVal;
        if (m_SwRevExpectedValue.find(name) != m_SwRevExpectedValue.end())
        {
            expectedVal = m_SwRevExpectedValue[name];
        }
        if (data != expectedVal)
        {
            Printf(Tee::PriError, "Invalid SW Revision for %s: Expected %#x Actual %#x\n",
                name.c_str(), expectedVal, data);
            stickyRc = RC::INCORRECT_FEATURE_SET;
        }
    }
    else
    {
        Printf(Tee::PriNormal, "%s SW Revision: %#x\n", name.c_str(), data);
    }

    if (pwrRegData.statusCmlPage != ~0U)
    {
        i2c.SetMessageLength(pwrRegData.pageMsgLen);
        CHECK_RC(i2c.Write(pwrRegData.pageReg, pwrRegData.statusCmlPage));
    }
    i2c.SetMessageLength(pwrRegData.statusCmlMsgLen);
    CHECK_RC(i2c.Read(pwrRegData.statusCmlReg, &data));
    VerbosePrintf("Data from %s %#x: %#x\n", name.c_str(), pwrRegData.statusCmlReg, data);
    if (!m_SkipI2cWriteLockCheck)
    {
        if ((data & pwrRegData.statusCmlMask) != pwrRegConfig.statusCmlVal)
        {
            Printf(Tee::PriError, "I2C Writes not locked for %s\n", name.c_str());
            stickyRc = RC::INCORRECT_FEATURE_SET;
        }
    }

    if (!m_SkipVoltageCheck)
    {
        if (pwrRegData.readVOutPage != ~0U)
        {
            i2c.SetMessageLength(pwrRegData.pageMsgLen);
            CHECK_RC(i2c.Write(pwrRegData.pageReg, pwrRegData.readVOutPage));
        }
        i2c.SetMessageLength(pwrRegData.readVOutMsgLen);
        CHECK_RC(i2c.Read(pwrRegData.readVOutReg, &data));
        VerbosePrintf("Data from %s %#x: %#x\n", name.c_str(), pwrRegData.readVOutReg, data);

        const UINT32 mv = static_cast<UINT32>(pwrRegConfig.voutScale * data);
        const UINT32 expectedMv = pwrRegConfig.expectedMv;

        UINT32 toleranceMv = m_VOutTolFixedmV;
        if (pwrRegConfig.voutTolMv != ~0U)
        {
            toleranceMv = pwrRegConfig.voutTolMv;
            Printf(Tee::PriNormal, "%s specific tolerance specified: %umV\n", name.c_str(), toleranceMv);
        }

        UINT32 error = expectedMv > mv ? expectedMv - mv : mv - expectedMv;
        FLOAT32 tolerance = m_UsePercentTol ?
            expectedMv * m_VOutTolPercent :
            toleranceMv;

        Printf(Tee::PriNormal, "%s VOut: %umV\n", name.c_str(), mv);

        if (pwrRegConfig.expectedMv != 0)
        {
            if (error > tolerance)
            {
                Printf(Tee::PriError, "Read VOut (%umV) out of tolerance of expected (%umV) for %s\n",
                    mv, expectedMv, name.c_str());
                stickyRc = RC::INCORRECT_FEATURE_SET;
            }
        }
    }

    // No expected value for Firmware Version; they just want it printed.
    if (pwrRegData.fwVerPage != ~0U)
    {
        i2c.SetMessageLength(pwrRegData.pageMsgLen);
        CHECK_RC(i2c.Write(pwrRegData.pageReg, pwrRegData.fwVerPage));
    }
    i2c.SetMessageLength(pwrRegData.fwVerMsgLen);
    CHECK_RC(i2c.Read(pwrRegData.fwVerReg, &data));
    Printf(Tee::PriNormal, "%s Regulator FW version: 0x%08x\n", name.c_str(), data);

    return stickyRc;
}

//-----------------------------------------------------------------------------
string PwrRegCheck::GetSwRevExpectedValue()
{
    string rtn;
    for (const auto& ev : m_SwRevExpectedValue)
    {
        if (rtn.empty())
        {
            rtn += ';';
        }
        rtn += Utility::StrPrintf("%s:%u", ev.first.c_str(), ev.second);
    }
    return rtn;
}

//-----------------------------------------------------------------------------
// Expected format for this arg is "<PwrReg0 Name>:<VOut 0>[;<PwrReg1 Name>:<VOut 1>]"
RC PwrRegCheck::SetSwRevExpectedValue(string val)
{
    m_SwRevExpectedValue.clear();

    size_t strStart = 0;
    size_t scIndex = 0;

    do
    {
        scIndex = val.find(';', strStart);
        string entry = val.substr(strStart, scIndex - strStart);

        size_t cIndex = entry.find(':');
        if (cIndex == string::npos)
        {
            Printf(Tee::PriError,
                "Bad entry in T394's SwRevExpectedValue: %s\n", entry.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        string name = entry.substr(0, cIndex);
        string valStr = entry.substr(cIndex + 1);

        const char* start = valStr.c_str();
        char* temp = nullptr;
        UINT32 val = strtoul(valStr.c_str(), &temp, 0);
        if (val == (numeric_limits<UINT32>::max)() ||
            temp != start + valStr.size())
        {
            Printf(Tee::PriError, "Invalid number for %s SW Revision: %s\n",
                name.c_str(), valStr.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        m_SwRevExpectedValue[name] = val;

        strStart = scIndex + 1;
    }
    while (scIndex != string::npos);
    return OK;
}

//-----------------------------------------------------------------------------
RC PwrRegCheck::LockSimulatedTemp(UINT32 degC)
{
    return GetBoundGpuSubdevice()->GetPerf()->OverrideVfeVar(
        Perf::OVERRIDE_VALUE, Perf::CLASS_TEMP, -1, static_cast<FLOAT32>(degC));
}

//-----------------------------------------------------------------------------
RC PwrRegCheck::UnlockSimulatedTemp()
{
    return GetBoundGpuSubdevice()->GetPerf()->OverrideVfeVar(
        Perf::OVERRIDE_NONE, Perf::CLASS_TEMP, -1, 0.0f);
}

//-----------------------------------------------------------------------------
// Parse the power regulator configuration file for power regulator
// information for the board we're testing on.
//
// example format:
// {
//     "regulators": {
//         "MP2888A": {
//             "DevIdAddr": "0x28",
//             "DevId": "0x88",
//             "ConfigCodeAddr": "0x46",
//             "SwRevAddr": "0x29",
//             "StatusCmlAddr": "0x7E",
//             "StatusCmlMask": "0x8",
//             "VOutAddr": "0x8B",
//             "FwVerAddr": "0x47"
//         }, ...
//     },
//     "configurations": [
//         {
//             "boards": [
//                 "3820-0000",
//                 "PG500-0000",
//                 ...
//             ],
//             "regulators": {
//                 "LWVDD": {
//                     "regulator": "MP2888A",
//                     "I2cPort": "0x2",
//                     "I2cAddr": "0x1A",
//                     "ConfigCode": "0x0008",
//                     "SwRev": "0x1a",
//                     "StatusCml": "0x0",
//                     "VOut": "LWVDD"
//                 },
//                 "HBMVDD": {
//                     "regulator": "MP2888A",
//                     "I2cPort": "0x2",
//                     "I2cAddr": "0x38",
//                     "ConfigCode": "0x0002",
//                     "SwRev": "0x0",
//                     "StatusCml": "0x0",
//                     "VOut": "0x0552"
//                 }
//             }
//         }, ...
RC PwrRegCheck::GetPwrRegInfo
(
    map<string, PwrRegData>* pPwrRegDataMap,
    vector<PwrRegConfig>* pBoardPwrRegs,
    string* pErrorMsg
)
{
    RC rc;
    MASSERT(pPwrRegDataMap != nullptr);
    MASSERT(pBoardPwrRegs != nullptr);
    MASSERT(pErrorMsg != nullptr);

    pPwrRegDataMap->clear();
    pBoardPwrRegs->clear();

    // Parse JSON
    if (Utility::FindPkgFile(m_ConfigFile) == Utility::NoSuchFile)
    {
        *pErrorMsg = Utility::StrPrintf(
            "Cannot find config file: %s\n", m_ConfigFile.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    Json::Document doc;
    CHECK_RC(ParseJson(m_ConfigFile, &doc));

    // Get the general information (register offsets) for power regulators
    CHECK_RC(CheckHasMember(doc, "regulators"));
    Json::Value& regs = doc["regulators"];
    if (!regs.IsObject())
    {
        *pErrorMsg = "\"regulators\" entry must be a JSON object\n";
        return RC::CANNOT_PARSE_FILE;
    }

    for (auto jsonIter = regs.MemberBegin(); jsonIter != regs.MemberEnd(); jsonIter++)
    {
        string regName = jsonIter->name.GetString();
        Json::Value& regData = jsonIter->value;
        PwrRegData pwrRegData;
        CHECK_RC(GetJsonUintFromString(regData, "PageReg",          false, &pwrRegData.pageReg));
        CHECK_RC(GetJsonUintFromString(regData, "PageMsgLen",       false, &pwrRegData.pageMsgLen));
        CHECK_RC(GetJsonRegFromString(regData,  "DevIdAddr",        true,  &pwrRegData.devidPage, &pwrRegData.devidReg));
        CHECK_RC(GetJsonUintFromString(regData, "DevIdMsgLen",      false, &pwrRegData.devidMsgLen));
        CHECK_RC(GetJsonUintFromString(regData, "DevId",            true,  &pwrRegData.devidVal));
        CHECK_RC(GetJsonRegFromString(regData,  "ConfigCodeAddr",   true,  &pwrRegData.configCodePage, &pwrRegData.configCodeReg));
        CHECK_RC(GetJsonUintFromString(regData, "ConfigCodeMsgLen", false, &pwrRegData.configCodeMsgLen));
        CHECK_RC(GetJsonRegFromString(regData,  "SwRevAddr",        true,  &pwrRegData.swRevPage, &pwrRegData.swRevReg));
        CHECK_RC(GetJsonUintFromString(regData, "SwRevMsgLen",      false, &pwrRegData.swRevMsgLen));
        CHECK_RC(GetJsonRegFromString(regData,  "StatusCmlAddr",    true,  &pwrRegData.statusCmlPage, &pwrRegData.statusCmlReg));
        CHECK_RC(GetJsonUintFromString(regData, "StatusCmlMask",    true,  &pwrRegData.statusCmlMask));
        CHECK_RC(GetJsonUintFromString(regData, "StatusCmlMsgLen",  false, &pwrRegData.statusCmlMsgLen));
        CHECK_RC(GetJsonRegFromString(regData,  "VOutAddr",         true,  &pwrRegData.readVOutPage, &pwrRegData.readVOutReg));
        CHECK_RC(GetJsonUintFromString(regData, "VOutMsgLen",       false, &pwrRegData.readVOutMsgLen));
        CHECK_RC(GetJsonRegFromString(regData,  "FwVerAddr",        true,  &pwrRegData.fwVerPage, &pwrRegData.fwVerReg));
        CHECK_RC(GetJsonUintFromString(regData, "FwVerMsgLen",      false, &pwrRegData.fwVerMsgLen));
        (*pPwrRegDataMap)[regName] = pwrRegData;
    }

    // Search the different board configurations for our board
    CHECK_RC(CheckHasMember(doc, "configurations"));
    Json::Value& configs = doc["configurations"];
    if (!configs.IsArray())
    {
        *pErrorMsg = "\"configurations\" entry must be a JSON array\n";
        return RC::CANNOT_PARSE_FILE;
    }

    bool boardFound = false;
    auto configIter = configs.Begin();
    while (!boardFound && configIter != configs.End())
    {
        if (!configIter->IsObject())
        {
            *pErrorMsg = "\"configurations\" entries must be JSON objects\n";
            return RC::CANNOT_PARSE_FILE;
        }
        CHECK_RC(CheckHasMember(*configIter, "boards"));
        Json::Value& boardList = (*configIter)["boards"];
        if (!boardList.IsArray())
        {
            *pErrorMsg = "\"boards\" entry must be a JSON array of strings\n";
            return RC::CANNOT_PARSE_FILE;
        }
        for (auto boardIter = boardList.Begin(); boardIter != boardList.End(); boardIter++)
        {
            if (!boardIter->IsString())
            {
                *pErrorMsg = "\"boards\" entries must be JSON strings\n";
                return RC::CANNOT_PARSE_FILE;
            }
            if (m_BoardName == boardIter->GetString())
            {
                boardFound = true;
                break;
            }
        }
        if (!boardFound)
        {
            configIter++;
        }
    }

    if (!boardFound)
    {
        *pErrorMsg = Utility::StrPrintf("Board %s not found in %s file",
            m_BoardName.c_str(), m_ConfigFile.c_str());
        return RC::UNKNOWN_BOARD_DESCRIPTION;
    }

    // Parse out the power regulator configuration(s) for our board
    CHECK_RC(CheckHasMember(*configIter, "regulators"));
    Json::Value& regulators = (*configIter)["regulators"];
    if (!regulators.IsObject())
    {
        *pErrorMsg = "\"regulators\" entry must be a JSON object\n";
            return RC::CANNOT_PARSE_FILE;
    }
    for (auto regIter = regulators.MemberBegin(); regIter != regulators.MemberEnd(); regIter++)
    {
        PwrRegConfig pwrRegConfig;
        pwrRegConfig.name = regIter->name.GetString();
        Json::Value& regulator = regIter->value;
        CHECK_RC(GetJsonString(regulator, "regulator", &pwrRegConfig.regulator));
        CHECK_RC(GetJsonUintFromString(regulator, "I2cPort",    true,  &pwrRegConfig.i2cPort));
        CHECK_RC(GetJsonUintFromString(regulator, "I2cAddr",    true,  &pwrRegConfig.i2cAddr));
        CHECK_RC(GetJsonUintFromString(regulator, "ConfigCode", true,  &pwrRegConfig.configCodeVal));
        CHECK_RC(GetJsonUintFromString(regulator, "SwRev",      true,  &pwrRegConfig.swRevVal));
        CHECK_RC(GetJsonUintFromString(regulator, "StatusCml",  true,  &pwrRegConfig.statusCmlVal));
        if (regulator.HasMember("VOutTolMv"))
            pwrRegConfig.voutTolMv = regulator["VOutTolMv"].GetUint();
        if (regulator.HasMember("VOutScale"))
            pwrRegConfig.voutScale = regulator["VOutScale"].GetDouble();
        string vout;
        CHECK_RC(GetJsonString(regulator, "VOut", &vout));

        const UINT32 expectedVoutRegValue = strtoul(vout.c_str(), nullptr, 0);
        if (expectedVoutRegValue != 0)
        {
            pwrRegConfig.expectedMv = expectedVoutRegValue * pwrRegConfig.voutScale;
        }
        else
        {
            GpuSubdevice * pGpuSub = GetBoundGpuSubdevice();
            auto pVolt3x = pGpuSub->GetVolt3x();
            if (!pVolt3x || !pVolt3x->IsInitialized())
            {
                if (vout != "LWVDD")
                {
                    Printf(Tee::PriWarn,
                           "Unknown voltage domain %s, skipping voltage check\n",
                           vout.c_str());
                    pwrRegConfig.expectedMv = 0;
                }
                else
                {
                    CHECK_RC(pGpuSub->GetPerf()->GetCoreVoltageMv(&pwrRegConfig.expectedMv));
                }
            }
            else
            {
                Gpu::SplitVoltageDomain voltDom = Gpu::ILWALID_SplitVoltageDomain;
                if (vout == "LWVDD")
                    voltDom = Gpu::VOLTAGE_LOGIC;
                else if (vout == "LWVDDS")
                    voltDom = Gpu::VOLTAGE_SRAM;
                else if (vout == "MSVDD")
                    voltDom = Gpu::VOLTAGE_MSVDD;
                else
                {
                    Printf(Tee::PriWarn,
                           "Unknown voltage domain %s, skipping voltage check\n",
                           vout.c_str());
                    pwrRegConfig.expectedMv = 0;
                }

                if (voltDom != Gpu::ILWALID_SplitVoltageDomain)
                {
                    LW2080_CTRL_VOLT_VOLT_RAIL_STATUS railStatus = { };
                    CHECK_RC(pVolt3x->GetVoltRailStatusViaId(voltDom, &railStatus));
                    pwrRegConfig.expectedMv = railStatus.lwrrVoltDefaultuV / 1000;
                }
            }
        }
        
        pBoardPwrRegs->push_back(pwrRegConfig);
    }

    return rc;
}

//-----------------------------------------------------------------------------
// RapidJSON Helper functions
//-----------------------------------------------------------------------------
/* static */ RC PwrRegCheck::ParseJson(const string& filename, Json::Document* pDoc)
{
    RC rc;
    vector<char> jsonBuffer;
    CHECK_RC(Utility::ReadPossiblyEncryptedFile(filename, &jsonBuffer, nullptr));

    // This is necessary to avoid RapidJson accessing beyond JsonBuffer's bounds
    jsonBuffer.push_back('\0');

    pDoc->Parse(jsonBuffer.data());

    if (pDoc->HasParseError())
    {
        size_t line, column;

        CHECK_RC(Utility::GetLineAndColumnFromFileOffset(
            jsonBuffer, pDoc->GetErrorOffset(), &line, &column));

        Printf(Tee::PriError,
               "JSON syntax error in %s at line %zd column %zd\n",
               filename.c_str(), line, column);
        Printf(Tee::PriError, "%s\n", Json::GetParseError_En(pDoc->GetParseError()));

        return RC::CANNOT_PARSE_FILE;
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* static */ RC PwrRegCheck::GetJsonUintFromString
(
    const Json::Value& jsonObj,
    const string& valName,
    bool  bRequired,
    UINT32* pRetVal
)
{
    // If the value is optional and there is no member then just return, which
    // will use the default configuration
    if (!bRequired && !jsonObj.HasMember(valName.c_str()))
        return RC::OK;

    RC rc;
    string strVal;
    CHECK_RC(GetJsonString(jsonObj, valName, &strVal));
    *pRetVal = strtoul(strVal.c_str(), nullptr, 0);
    return rc;
}

//-----------------------------------------------------------------------------
/* static */ RC PwrRegCheck::GetJsonRegFromString
(
    const Json::Value& jsonObj,
    const string& valName,
    bool  bRequired,
    UINT32* pRegPage,
    UINT32* pRegAddr
)
{
    // If the value is optional and there is no member then just return, which
    // will use the default configuration
    if (!bRequired && !jsonObj.HasMember(valName.c_str()))
        return RC::OK;

    RC rc;
    string strVal;
    CHECK_RC(GetJsonString(jsonObj, valName, &strVal));
    const size_t pageSep = strVal.find_first_of(':');
    if (pageSep != string::npos)
    {
        *pRegPage = strtoul(strVal.substr(0, pageSep).c_str(), nullptr, 0);
        *pRegAddr = strtoul(strVal.substr(pageSep+1).c_str(), nullptr, 0);
    }
    else
    {   
        *pRegPage = ~0U;
        *pRegAddr = strtoul(strVal.c_str(), nullptr, 0);
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* static */ RC PwrRegCheck::GetJsonString
(
    const Json::Value& jsonObj,
    const string& valName,
    string* pRetVal
)
{
    RC rc;
    MASSERT(pRetVal != nullptr);

    CHECK_RC(CheckHasMember(jsonObj, valName));
    auto& member = jsonObj[valName.c_str()];
    if (!member.IsString())
    {
        Printf(Tee::PriError, "\"%s\" is not a string\n", valName.c_str());
        return RC::CANNOT_PARSE_FILE;
    }
    *pRetVal = member.GetString();
    return rc;
}

//-----------------------------------------------------------------------------
/* static */ RC PwrRegCheck::CheckHasMember
(
    const Json::Value& jsonObj,
    const string& memberName
)
{
    if (!jsonObj.HasMember(memberName.c_str()))
    {
        Printf(Tee::PriError,
            "Cannot find member \"%s\"\n", memberName.c_str());
        return RC::CANNOT_PARSE_FILE;
    }
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

JS_CLASS_INHERIT(PwrRegCheck, GpuTest,
                 "Power Regulator config test");

CLASS_PROP_READWRITE(PwrRegCheck, ConfigFile, string,
                     "Config file to use to get power regulator data/expected values.");

CLASS_PROP_READWRITE(PwrRegCheck, BoardName, string,
                     "Board name to use instead of checking boards.db.");

CLASS_PROP_READWRITE(PwrRegCheck, SkipI2cWriteLockCheck, bool,
                     "Skip checking whether or not I2C writes are locked.");

CLASS_PROP_READWRITE(PwrRegCheck, SkipSwRevCheck, bool,
                     "Skip checking whether or not SW Revision matches expected.");

CLASS_PROP_READWRITE(PwrRegCheck, SwRevExpectedValue, string,
                     "Set the expected SW Revision for the power regulators.");

CLASS_PROP_READWRITE(PwrRegCheck, HbmExpectedVOut, UINT32,
                     "Set the expected value for HBM's Regulator's VOut (in mV).");

CLASS_PROP_READWRITE(PwrRegCheck, VOutTolPercent, FLOAT32,
                     "Set the tolerance percentage for VOut comparison.");

CLASS_PROP_READWRITE(PwrRegCheck, VOutTolFixedmV, UINT32,
                     "Set a fixed tolerance in mV for VOut comparison (default 7mV).");

CLASS_PROP_READWRITE(PwrRegCheck, SkipVoltageCheck, bool,
                     "Skip checking lwvdd accuracy");
