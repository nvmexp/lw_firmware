/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */


#include "core/include/lwrm.h"
#include "core/include/jsonlog.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/pcie.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/pwrsub.h"
#include "gpu/tests/gputest.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/lwdisplay/lw_disp_crc_handler_c3.h"
#include "cheetah/include/sysutil.h"

#ifdef INCLUDE_AZALIA
    #include "device/azalia/azactrl.h"
#endif

// RapidJson includes
#include "document.h"
#include "error/en.h"
#include "filereadstream.h"

#include <cstdlib>
#include <algorithm>

namespace Json = rapidjson;

#define TOUPPER(str) std::transform(str.begin(), str.end(), str.begin(), ::toupper)

class LWSpecs : public GpuTest
{
public:
    RC Run() override;
    RC Setup() override;
    bool IsSupported() override;
    RC Cleanup() override;

    RC SetJavaScriptObject(JSContext * cx, JSObject *obj) override;
    RC SetupJsonItems() override;

    SETGET_PROP(Filename, string);
    SETGET_PROP(Override, bool);

private: // testargs
    string m_Filename;
    bool m_Override = false;

private:
    static const string empty;
    template <typename T>
    RC CompareField(const string& name, T val, const string& units = empty);
    template <typename T>
    T ReadField(Json::Value& value);
    template <typename T>
    void PrintField(Tee::Priority pri, const string& name, T val, const string& units);

    static const map<UINT32, string> s_GpuFamilyNames;

    Json::Document m_Doc;
    Json::Value m_JsonFields;
    JsonItem* m_pJsonUpload = nullptr;
    RC LoadJsonFile();
    RC CheckHasMember(const Json::Value& jsonObj, const string& memberName);

    PStateOwner m_PStateOwner;
    UINT32 m_MaxSpeedPState = 0;
    Perf::PerfPoint m_OrigPerfPoint;
    RC CalcMaxPexSpeedPState(UINT32* pPState);
};

/*static*/ const string LWSpecs::empty = string();

/*static*/ const map<UINT32, string> LWSpecs::s_GpuFamilyNames =
{
#define DEFINE_NEW_GPU(DIDStart, DIDEnd, ChipId, LwId, Constant, Family, ...) \
    { GpuDevice::Family, #Family },
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(DIDStart, DIDEnd, ChipId, LwId, Constant, Family) \
    { GpuDevice::Family, #Family },
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
};

//-----------------------------------------------------------------------------
bool LWSpecs::IsSupported()
{
    if (GetBoundTestDevice()->GetType() != TestDevice::TYPE_LWIDIA_GPU)
        return false;

    return GpuTest::IsSupported();
}

//-----------------------------------------------------------------------------
RC LWSpecs::Setup()
{
    RC rc;


    CHECK_RC(GpuTest::Setup());

    if (m_Filename.empty() && !m_Override)
    {
        Printf(Tee::PriError, "LWSpecs requires the Filename argument unless you set Override=true.\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (!m_Filename.empty())
    {
        CHECK_RC(LoadJsonFile());
    }

    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    Perf *pPerf = GetBoundGpuSubdevice()->GetPerf();
    UINT32 numPStates = 0;
    CHECK_RC(pPerf->GetNumPStates(&numPStates));
    if (numPStates)
    {
        CHECK_RC(pPerf->GetLwrrentPerfPoint(&m_OrigPerfPoint));
        CHECK_RC(CalcMaxPexSpeedPState(&m_MaxSpeedPState));
        CHECK_RC(pPerf->SetInflectionPoint(m_MaxSpeedPState, Perf::GpcPerf_MAX));
    }
    else
    {
        Printf(Tee::PriWarn, "LWSPecs: No valid PState defined\n");
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LWSpecs::Cleanup()
{
    RC rc;

    Perf *pPerf = GetBoundGpuSubdevice()->GetPerf();
    UINT32 numPStates = 0;
    CHECK_RC(pPerf->GetNumPStates(&numPStates));
    if (numPStates)
    {
        // Restore original PerfPoint
        CHECK_RC(pPerf->SetPerfPoint(m_OrigPerfPoint));
    }

    m_PStateOwner.ReleasePStates();

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LWSpecs::SetJavaScriptObject(JSContext* cx, JSObject* obj)
{
    RC rc;

    CHECK_RC(GpuTest::SetJavaScriptObject(cx, obj));

    m_pJsonUpload = new JsonItem(cx, obj, ENCJSENT("JsonUpload"));

    return rc;
}

//-----------------------------------------------------------------------------
RC LWSpecs::SetupJsonItems()
{
    RC rc;

    CHECK_RC(GpuTest::SetupJsonItems());

    MASSERT(m_pJsonUpload);
    m_pJsonUpload->SetTag(ENCJSENT("upload"));
    m_pJsonUpload->RemoveAllFields();
    m_pJsonUpload->SetField(ENCJSENT("tname"), GetName().c_str());
    m_pJsonUpload->SetCategory(JsonItem::JSONLOG_DEFAULT);

    return rc;
}

//-----------------------------------------------------------------------------
RC LWSpecs::LoadJsonFile()
{
    RC rc;

    vector<char> jsonBuffer;
    CHECK_RC(Utility::ReadPossiblyEncryptedFile(m_Filename, &jsonBuffer, nullptr));

    // This is necessary to avoid RapidJson accessing beyond JsonBuffer's bounds
    jsonBuffer.push_back('\0');

    m_Doc.Parse(jsonBuffer.data());

    if (m_Doc.HasParseError())
    {
        size_t line, column;

        CHECK_RC(Utility::GetLineAndColumnFromFileOffset(
            jsonBuffer, m_Doc.GetErrorOffset(), &line, &column));

        Printf(Tee::PriError,
               "JSON syntax error in %s at line %zd column %zd\n",
               m_Filename.c_str(), line, column);
        Printf(Tee::PriError, "%s\n", Json::GetParseError_En(m_Doc.GetParseError()));

        return RC::CANNOT_PARSE_FILE;
    }

    CHECK_RC(CheckHasMember(m_Doc, "PullTimestamp"));
    Json::Value& pullTimestamp = m_Doc["PullTimestamp"];
    m_pJsonUpload->SetField("PullTimestamp", pullTimestamp.GetString());

    CHECK_RC(CheckHasMember(m_Doc, "ProductData"));
    Json::Value& productData = m_Doc["ProductData"];
    if (!productData.IsObject())
    {
        Printf(Tee::PriError, "\"ProductData\" entry must be a JSON object\n");
        return RC::CANNOT_PARSE_FILE;
    }

    CHECK_RC(CheckHasMember(productData, "LWSpecsTimestamp"));
    Json::Value& lwspecsTimestamp = productData["LWSpecsTimestamp"];
    m_pJsonUpload->SetField("LWSpecsTimestamp", lwspecsTimestamp.GetString());

    CHECK_RC(CheckHasMember(productData, "LWSpecsID"));
    Json::Value& lwspecsId = productData["LWSpecsID"];
    m_pJsonUpload->SetField("LWSpecsID", lwspecsId.GetUint());

    CHECK_RC(CheckHasMember(productData, "Properties"));
    m_JsonFields = productData["Properties"];
    if (!m_JsonFields.IsObject())
    {
        Printf(Tee::PriError, "\"Properties\" entry must be a JSON object\n");
        return RC::CANNOT_PARSE_FILE;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LWSpecs::CalcMaxPexSpeedPState(UINT32* pPState)
{
    MASSERT(pPState);
    RC rc;
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    Perf *pPerf = pGpuSubdev->GetPerf();
    vector<UINT32> availablePStates;
    UINT32 maxlnkSpeed = 0;
    UINT32 linkSpeed = 0;

    UINT32 lwrrPState = 0;
    CHECK_RC(pPerf->GetLwrrentPState(&lwrrPState));
    CHECK_RC(pPerf->GetAvailablePStates(&availablePStates));
    if (availablePStates.size() == 1)
    {
        *pPState = lwrrPState;
    }
    else
    {
        vector <UINT32>::const_reverse_iterator ii = availablePStates.rbegin();
        while (ii != availablePStates.rend())
        {
            // Read expected link speed for all PStates
            CHECK_RC(pPerf->GetPexSpeedAtPState((*ii), &linkSpeed));
            if (linkSpeed > maxlnkSpeed)
            {
                maxlnkSpeed = linkSpeed;
                *pPState = *ii;
            }
            ii++;
        }
    }
    return RC::OK;
}

namespace
{
    bool VerifyAerOffset
    (
        INT32 domainNumber,
        INT32 busNumber,
        INT32 deviceNumber,
        INT32 functionNumber,
        UINT16 expectedOffset
    )
    {
        UINT08 baseCapOffset;
        UINT16 extCapOffset;
        UINT16 extCapSize;

        Pci::FindCapBase(domainNumber,
                         busNumber,
                         deviceNumber,
                         functionNumber,
                         PCI_CAP_ID_PCIE,
                         &baseCapOffset);

        if (OK == Pci::GetExtendedCapInfo(domainNumber,
                                          busNumber,
                                          deviceNumber,
                                          functionNumber,
                                          Pci::AER_ECI,
                                          baseCapOffset,
                                          &extCapOffset,
                                          &extCapSize))
        {
            return extCapOffset == expectedOffset;
        }

        return false;
    }
}

//-----------------------------------------------------------------------------
RC LWSpecs::Run()
{
    StickyRC rc;

    GpuSubdevice* pSub = GetBoundGpuSubdevice();

    //-------------------------------------------------------------------------
    bool aerEnabled = false;
    bool gpuAerEnabled = pSub->GetInterface<Pcie>()->AerEnabled();
    bool gpuAerOffsetCorrect = false;
    if (gpuAerEnabled)
    {
        // Verify AER offset in PCIE Extended Capabilities list
        gpuAerOffsetCorrect = VerifyAerOffset(pSub->GetInterface<Pcie>()->DomainNumber(),
                                              pSub->GetInterface<Pcie>()->BusNumber(),
                                              pSub->GetInterface<Pcie>()->DeviceNumber(),
                                              pSub->GetInterface<Pcie>()->FunctionNumber(),
                                              pSub->GetInterface<Pcie>()->GetAerOffset());
    }

#ifdef INCLUDE_AZALIA
    // If we have a multifunctional SKU, both functions must support AER
    AzaliaController* const pAzalia = pSub->GetAzaliaController();
    if (pAzalia != nullptr)
    {
        bool azaAerEnabled = pAzalia->AerEnabled();
        bool azaAerOffsetCorrect = false;

        if (azaAerEnabled)
        {
            // Find AER offset in PCIE Extended Capabilities list
            SysUtil::PciInfo azaPciInfo = {};
            CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));

            azaAerOffsetCorrect = VerifyAerOffset(azaPciInfo.DomainNumber,
                                                  azaPciInfo.BusNumber,
                                                  azaPciInfo.DeviceNumber,
                                                  azaPciInfo.FunctionNumber,
                                                  pAzalia->GetAerOffset());
        }

        aerEnabled = azaAerEnabled &&
                     azaAerOffsetCorrect &&
                     gpuAerEnabled &&
                     gpuAerOffsetCorrect;
    }
    else
#endif
    {
        aerEnabled = gpuAerEnabled && gpuAerOffsetCorrect;
    }

    rc = CompareField("LWSPECS_AER_ENABLED", aerEnabled);

    //-------------------------------------------------------------------------
    string arch = "UNKNOWN";
    auto familyItr = s_GpuFamilyNames.find(static_cast<UINT32>(pSub->GetParentDevice()->GetFamily()));
    if (familyItr != s_GpuFamilyNames.end())
        arch = familyItr->second;
    else
        rc = RC::ILWALID_ARGUMENT;
    TOUPPER(arch);
    rc = CompareField("LWSPECS_ARCHITECTURE", arch);

    //-------------------------------------------------------------------------
    Perf::PerfPoint pp = Perf::PerfPoint(0, Perf::GpcPerf_TDP);
    Perf::PerfPoint standardPP = pSub->GetPerf()->GetMatchingStdPerfPoint(pp);
    if (standardPP.SpecialPt == Perf::NUM_GpcPerfMode)
        rc = RC::SOFTWARE_ERROR;
    UINT64 baseClk = 0;
    rc = pSub->GetPerf()->GetClockAtPerfPoint(standardPP, Gpu::ClkGpc, &baseClk);
    rc = CompareField("LWSPECS_BASE_CLOCK", round(baseClk / (1000.0f * 1000.0f)), "MHz");

    //-------------------------------------------------------------------------
    string boardNumber = pSub->GetBoardNumber();
    rc = CompareField("LWSPECS_BOARD_PROJECT_FOR_SW", boardNumber);
    if (boardNumber[0] == 'G')
        boardNumber.insert(boardNumber.begin(), 'P'); // Production board
    else
        boardNumber.insert(boardNumber.begin(), 'E'); // Engineering board
    rc = CompareField("LWSPECS_BOARD_PROJECT", boardNumber);

    //-------------------------------------------------------------------------
    string skuNumberStr =  pSub->GetBoardSkuNumber();
    UINT32 skuNumber = strtoul(skuNumberStr.c_str(), nullptr, 10);
    rc = CompareField("LWSPECS_BOARD_SKU", skuNumber);

    //-------------------------------------------------------------------------
    Perf::PerfPoint ppTurbo = Perf::PerfPoint(0, Perf::GpcPerf_TURBO);
    Perf::PerfPoint boostPP = pSub->GetPerf()->GetMatchingStdPerfPoint(ppTurbo);
    if (boostPP.SpecialPt == Perf::NUM_GpcPerfMode)
        rc = RC::SOFTWARE_ERROR;
    UINT64 boostClk = 0;
    rc = pSub->GetPerf()->GetClockAtPerfPoint(boostPP, Gpu::ClkGpc, &boostClk);
    rc = CompareField("LWSPECS_BOOST_CLOCK", round(boostClk / (1000.0f * 1000.0f)), "MHz");

    //-------------------------------------------------------------------------
    string coolingType = "ACTIVE";
    LW2080_CTRL_FAN_COOLER_INFO_PARAMS coolingParams = {};
    rc =  LwRmPtr()->ControlBySubdevice(pSub,
                                        LW2080_CTRL_CMD_FAN_COOLER_GET_INFO,
                                        &coolingParams, sizeof(coolingParams));
    if (coolingParams.coolerMask == 0x0)
    {
        coolingType = "PASSIVE";
    }
    else
    {
        for (INT32 idx = Utility::BitScanForward(coolingParams.coolerMask);
             idx != -1;
             idx = Utility::BitScanForward(coolingParams.coolerMask, idx+1))
        {
            if (coolingParams.coolers[idx].type == LW2080_CTRL_FAN_COOLER_TYPE_ILWALID
                || coolingParams.coolers[idx].type == LW2080_CTRL_FAN_COOLER_TYPE_UNKNOWN)
            {
                coolingType = "PASSIVE";
                break;
            }
        }
    }
    rc = CompareField("LWSPECS_COOLING_SOLUTION_TYPE", coolingType);

    //-------------------------------------------------------------------------
    string devIdStr = Utility::StrPrintf("0x%x", pSub->GetInterface<Pcie>()->DeviceId());
    TOUPPER(devIdStr);
    rc = CompareField("LWSPECS_DEVICE_ID", devIdStr);

    //-------------------------------------------------------------------------
    bool eccEnabled = false;
    if (pSub->HasFeature(Device::GPUSUB_SUPPORTS_ECC))
    {
        bool eccSupp = false;
        UINT32 eccMask = 0x0;
        rc = pSub->GetEccEnabled(&eccSupp, &eccMask);
        eccEnabled = eccSupp && eccMask;
    }
    rc = CompareField("LWSPECS_ECC", eccEnabled);

    //-------------------------------------------------------------------------
    bool eccSupported = pSub->HasFeature(Device::GPUSUB_SUPPORTS_ECC) && pSub->IsEccFuseEnabled();
    rc = CompareField("LWSPECS_ECC_SUPPORTED", eccSupported);

    //-------------------------------------------------------------------------
    UINT32 pin6Power = 0;
    UINT32 pin8Power = 0;
    LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS monParams = {};
    // Get power channels data from Power Topology Table in the VBIOS
    rc = LwRmPtr()->ControlBySubdevice(pSub,
                                       LW2080_CTRL_CMD_PMGR_PWR_MONITOR_GET_INFO,
                                       &monParams, sizeof(monParams));
    for (UINT32 channelIdx = 0;
         channelIdx < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; channelIdx++)
    {
        if (!(BIT(channelIdx) & monParams.channelMask))
            continue;

        LW2080_CTRL_PMGR_PWR_CHANNEL_INFO& ch = monParams.channels[channelIdx];
        switch (ch.pwrRail)
        {
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN0:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN1:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN2:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN3:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN4:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_8PIN5:
                pin8Power++;
                break;
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_6PIN0:
            case LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_EXT12V_6PIN1:
                pin6Power++;
                break;
        }
    }
    rc = CompareField("LWSPECS_EXTERNAL_POWER_NUM_6_PIN_CONNECTORS", pin6Power);
    rc = CompareField("LWSPECS_EXTERNAL_POWER_NUM_8_PIN_CONNECTORS", pin8Power);

    //-------------------------------------------------------------------------
    string strappedFanDebugPwm = "DISABLED";
    if (pSub->HasFeature(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_66) ||
        pSub->HasFeature(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_33))
    {
        if (pSub->IsFeatureEnabled(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_66))
        {
            strappedFanDebugPwm = "66";
        }
        else if (pSub->IsFeatureEnabled(Device::GPUSUB_STRAPS_FAN_DEBUG_PWM_33))
        {
            strappedFanDebugPwm = "33";
        }
    }
    rc = CompareField("LWSPECS_FAN_DEBUG_PWM", strappedFanDebugPwm);

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_FB_BAR_SIZE", pSub->FramebufferBarSize(), "MB");

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_FB_BUS_WIDTH", pSub->GetFB()->GetBusWidth(), "Bits");

    //-------------------------------------------------------------------------
    UINT32 fbColumns = (pSub->GetFB()->GetRowSize()
                        * pSub->GetFB()->GetPseudoChannelsPerChannel()
                        / pSub->GetFB()->GetAmapColumnSize());
    rc = CompareField("LWSPECS_FB_COLUMNS", fbColumns);

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_FB_EXT_BANKS", pSub->GetFB()->GetRankCount());

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_FB_INT_BANKS", pSub->GetFB()->GetBankCount());

    //-------------------------------------------------------------------------
    UINT32 fbRows = 1 << pSub->GetFB()->GetRowBits();
    rc = CompareField("LWSPECS_FB_ROWS", fbRows);

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_FBP_COUNT", pSub->GetFB()->GetFbpCount());

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_GPC_COUNT", pSub->GetGpcCount());

    //-------------------------------------------------------------------------
    string devidStr = Gpu::DeviceIdToString(pSub->DeviceId());
    TOUPPER(devidStr);
    rc = CompareField("LWSPECS_GPU_FAMILY", devidStr);

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_L2CACHE_CAPACITY", pSub->GetFB()->GetL2CacheSize() / 1024, "KB");

    //-------------------------------------------------------------------------
    UINT32 maxGpuTemp = 0;
    UINT32 rtpTargetTemp = 0;
    LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS thermInfoParams = {};
    rc = LwRmPtr()->ControlBySubdevice(pSub,
                                       LW2080_CTRL_CMD_THERMAL_POLICY_GET_INFO,
                                       &thermInfoParams, sizeof(thermInfoParams));
    if (thermInfoParams.policyMask > 0)
    {
        if (thermInfoParams.gpuSwSlowdownPolicyIdx < LW2080_CTRL_THERMAL_POLICY_MAX_POLICIES)
        {
            auto policy = thermInfoParams.policies[thermInfoParams.gpuSwSlowdownPolicyIdx];
            maxGpuTemp = static_cast<UINT32>(round(policy.limitMax / 256.0f));
        }
        if (thermInfoParams.acousticPolicyIdx < LW2080_CTRL_THERMAL_POLICY_MAX_POLICIES)
        {
            auto policy = thermInfoParams.policies[thermInfoParams.acousticPolicyIdx];
            rtpTargetTemp = static_cast<UINT32>(round(policy.limitRated / 256.0f));
        }
    }
    rc = CompareField("LWSPECS_MAX_GPU_TEMPERATURE", maxGpuTemp, "C");

    //-------------------------------------------------------------------------
    UINT64 memClk = 0;
    rc = pSub->GetPerf()->GetClockAtPerfPoint(boostPP, Gpu::ClkM, &memClk);
    rc = CompareField("LWSPECS_MEMORY_CLOCK_P0", round(memClk / (1000.0f * 1000.0f)), "MHz");

    //-------------------------------------------------------------------------
    UINT32 numSubPartitions = 0;
    for (UINT32 fbp = 0; fbp < pSub->GetFB()->GetMaxFbpCount(); fbp++)
    {
        // Half FBPA
        FLOAT32 ltcRatio = static_cast<FLOAT32>(Utility::CountBits(pSub->GetFsImpl()->L2Mask(fbp))) /
                           static_cast<FLOAT32>(pSub->GetFB()->GetMaxLtcsPerFbp());
        for (UINT32 fbio = 0; fbio < pSub->GetFB()->GetFbiosPerFbp(); fbio++)
        {
            numSubPartitions += static_cast<UINT32>(pSub->GetFB()->GetSubpartitions() * ltcRatio);
        }
    }
    const UINT32 memWidth = numSubPartitions * pSub->GetFB()->GetSubpartitionBusWidth();
    rc = CompareField("LWSPECS_MEMORY_INTERFACE_WIDTH", memWidth, "Bits");

    //-------------------------------------------------------------------------
    FLOAT32 fbSizeMb = round(pSub->GetFB()->GetGraphicsRamAmount() / (1024.0f * 1024.0f));
    rc = CompareField("LWSPECS_MEMORY_FB_SIZE", fbSizeMb, "MB"); //legacy
    rc = CompareField("LWSPECS_MEMORY_FB_SIZE_MB", fbSizeMb, "MB");

    //-------------------------------------------------------------------------
    string ramString = pSub->GetFB()->GetRamProtocolString();
    TOUPPER(ramString);
    rc = CompareField("LWSPECS_MEMORY_TYPE", ramString);

    //-------------------------------------------------------------------------
    LW2080_CTRL_GR_GET_INFO_V2_PARAMS params = {};
    params.grInfoListSize = 1;
    params.grInfoList[0].index = LW2080_CTRL_GR_INFO_INDEX_GPU_CORE_COUNT;
    rc = LwRmPtr()->ControlBySubdevice(pSub,
                                       LW2080_CTRL_CMD_GR_GET_INFO_V2,
                                       &params,
                                       sizeof(params));
    const UINT32 numCores = params.grInfoList[0].data;
    rc = CompareField("LWSPECS_NUM_LWDA_CORES", numCores);

    //-------------------------------------------------------------------------
    UINT32 dpConnectors = 0;
    UINT32 hdmiConnectors = 0;
    UINT32 dvidConnectors = 0;
    UINT32 usbConnectors = 0;

    if (GetDisplay()->GetDisplayClassFamily() != Display::NULLDISP)
    {
        LwDisplay* pDisplay = static_cast<LwDisplay*>(GetDisplay());
        DisplayIDs connectors;
        rc = pDisplay->GetConnectors(&connectors);
        set<UINT32> seenIndices;

        for (const DisplayID& conn : connectors)
        {
            RC dispRC;
            LW0073_CTRL_SPECIFIC_GET_CONNECTOR_DATA_PARAMS connParams = {};
            if (RC::OK != (dispRC = pDisplay->GetDisplaySubdeviceInstance(&connParams.subDeviceInstance)))
            {
                rc = dispRC;
                break;
            }
            connParams.displayId = conn.Get();

            if (RC::OK == (dispRC = pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_CONNECTOR_DATA,
                                                &connParams, sizeof(connParams))))
            {
                for (UINT32 i = 0; i < connParams.count; i++)
                {
                    auto& data = connParams.data[i];
                    if (!seenIndices.insert(data.index).second)
                        continue;

                    switch (data.type)
                    {
                        case LW0073_CTRL_SPECIFIC_CONNECTOR_DATA_TYPE_DVI_D:
                            dvidConnectors++;
                            break;
                        case LW0073_CTRL_SPECIFIC_CONNECTOR_DATA_TYPE_DP_EXT:
                            dpConnectors++;
                            break;
                        case LW0073_CTRL_SPECIFIC_CONNECTOR_DATA_TYPE_HDMI_A:
                            hdmiConnectors++;
                            break;
                        case LW0073_CTRL_SPECIFIC_CONNECTOR_DATA_TYPE_USB_C:
                            usbConnectors++;
                            break;
                    }
                }
            }
            rc = dispRC;
        }
    }
    rc = CompareField("LWSPECS_NUM_DP_CONNECTORS", dpConnectors);
    rc = CompareField("LWSPECS_NUM_DVID_CONNECTORS", dvidConnectors);
    rc = CompareField("LWSPECS_NUM_HDMI_CONNECTORS", hdmiConnectors);
    rc = CompareField("LWSPECS_NUM_USBC_CONNECTORS", usbConnectors);

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_NUM_INDEPENDENT_DISPLAY_HEADS", GetDisplay()->GetHeadCount());

    //-------------------------------------------------------------------------
    string classCodeStr = "";
    const UINT32 pciRevId =
        pSub->Regs().Read32(MODS_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE);
    const UINT32 pciClassCode = REF_VAL(LW_PCI_REV_ID_CLASS_CODE, pciRevId);
    switch (pciClassCode)
    {
        case 0x030000:
             classCodeStr = "VGA";
            break;
        case 0x030200:
            classCodeStr = "3D";
            break;
        default:
            classCodeStr = Utility::StrPrintf("0x%06x", pciClassCode);
            break;
    }
    TOUPPER(classCodeStr);
    rc = CompareField("LWSPECS_PCI_CLASS_CODE", classCodeStr);

    //-------------------------------------------------------------------------
    string lwrBusSpeed = "UNKNOWN";
    switch (pSub->GetInterface<Pcie>()->GetLinkSpeed(Pci::SpeedUnknown))
    {
        case Pci::Speed32000MBPS:
            lwrBusSpeed = "GEN5";
            break;
        case Pci::Speed16000MBPS:
            lwrBusSpeed = "GEN4";
            break;
        case Pci::Speed8000MBPS:
            lwrBusSpeed = "GEN3";
            break;
        case Pci::Speed5000MBPS:
            lwrBusSpeed = "GEN2";
            break;
        case Pci::Speed2500MBPS:
            lwrBusSpeed = "GEN1";
            break;
        default:
            break;
    }
    rc = CompareField("LWSPECS_PCIE_INIT_GEN", lwrBusSpeed);

    //-------------------------------------------------------------------------
    // "ROP Units" is another bogus marketing term. It's not the actual ROP count. It's more a measure of performance.
    rc = CompareField("LWSPECS_ROP_UNITS", pSub->RopCount() * pSub->RopPixelsPerClock());

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_RTP_TEMPERATURE", rtpTargetTemp, "C");

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_SM_COUNT", pSub->ShaderCount());

    //-------------------------------------------------------------------------
    string ssidStr = Utility::StrPrintf("0x%x", pSub->GetInterface<Pcie>()->SubsystemVendorId());
    TOUPPER(ssidStr);
    rc = CompareField("LWSPECS_SSID", ssidStr);

    //-------------------------------------------------------------------------
    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS thermParams = {};
    thermParams.eventId = LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_0;
    rc = LwRmPtr()->ControlBySubdevice(pSub,
                                       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_REPORTING_SETTINGS_GET,
                                       &thermParams,
                                       sizeof(thermParams));
    // round to the nearest 0.1C
    FLOAT32 thermalShutdown = round((thermParams.temperature * 10.0f)/256.0f) / 10.0f;
    rc = CompareField("LWSPECS_THERMAL_THRESHOLD_SHUTDOWN_TAVE", thermalShutdown);

    //-------------------------------------------------------------------------
    UINT32 tgpPower = 0;
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS powerPolicy = {};
    rc = LwRmPtr()->ControlBySubdevice(pSub,
                                       LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_INFO,
                                       &powerPolicy,
                                       sizeof(powerPolicy));
    auto policyIdx = static_cast<Power::PowerCapIdx>(powerPolicy.tgpPolicyIdx);
    if (policyIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES
        && (powerPolicy.policyMask & (1 << policyIdx)))
    {
        LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS pwrCtrl = {};
        pwrCtrl.policyMask = (1 << policyIdx);
        pwrCtrl.policyRelMask = 0;
        rc = LwRmPtr()->ControlBySubdevice(pSub,
                                           LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_CONTROL,
                                           &pwrCtrl, sizeof(pwrCtrl));
        tgpPower = pwrCtrl.policies[policyIdx].limitLwrr;
    }
    rc = CompareField("LWSPECS_TGP_SPEC_POWER", round(tgpPower / 1000.0f), "W");

    //-------------------------------------------------------------------------
    rc = CompareField("LWSPECS_TPC_COUNT", pSub->GetTpcCount());


    m_pJsonUpload->SetLwrDateTimeField(ENCJSENT("datetime"));

    return rc;
}

namespace
{
    template <typename T>
    struct ToJsonField
    {
        T m_Val;
        ToJsonField(T val) : m_Val(val) {}
        T operator*() { return m_Val; }
    };

    template <>
    struct ToJsonField<string>
    {
        string m_Val;
        ToJsonField(string val) : m_Val(val) {}
        const char* operator*() { return m_Val.c_str(); }
    };
}

//-----------------------------------------------------------------------------
template <typename T>
RC LWSpecs::CompareField(const string& name, T val, const string& units)
{
    RC rc;

    JsonItem jsi;
    jsi.SetField("PropertyValue", *ToJsonField<T>(val));

    DEFER
    {
        if (JsonLogStream::IsOpen())
            GetJsonExit()->SetField(name.c_str(), &jsi);
    };

    if (m_Filename.empty())
    {
        PrintField(Tee::PriNormal, name, val, units);
        return OK;
    }

    if (m_JsonFields.HasMember(name.c_str()))
    {
        DEFER
        {
            m_pJsonUpload->SetField(name.c_str(), &jsi);
        };

        PrintField(Tee::PriNormal, name, val, units);

        Json::Value& field = m_JsonFields[name.c_str()];

        CHECK_RC(CheckHasMember(field, "LWSpecsTimestamp"));
        Json::Value& lwspecsTimestamp = field["LWSpecsTimestamp"];
        jsi.SetField("LWSpecsTimestamp", lwspecsTimestamp.GetString());

        CHECK_RC(CheckHasMember(field, "LWSpecsID"));
        Json::Value& lwspecsId = field["LWSpecsID"];
        jsi.SetField("LWSpecsID", lwspecsId.GetUint());

        CHECK_RC(CheckHasMember(field, "PropertyValue"));
        Json::Value& property = field["PropertyValue"];

        T expected = ReadField<T>(property);
        jsi.SetField("ExpectedValue", *ToJsonField<T>(expected));

        if (val != expected)
        {
            jsi.SetField("Matches", false);
            Printf(Tee::PriError, "Miscompare: Expected Value = %s\n", property.GetString());
            return RC::DATA_MISMATCH;
        }
        else
        {
            jsi.SetField("Matches", true);
        }
    }
    else
    {
        PrintField(GetVerbosePrintPri(), name, val, units);

        Printf(GetTestConfiguration()->Verbose() ? Tee::PriWarn : Tee::PriLow,
               "No comparison field for %s\n", name.c_str());
    }

    return rc;
}

//-----------------------------------------------------------------------------
template <>
bool LWSpecs::ReadField(Json::Value& value)
{
    string s = value.GetString();
    return (s == "true") || (s == "True");
}

template <>
UINT32 LWSpecs::ReadField(Json::Value& value)
{
    return strtoul(value.GetString(), nullptr, 0);
}

template <>
UINT64 LWSpecs::ReadField(Json::Value& value)
{
    return strtoull(value.GetString(), nullptr, 0);
}

template <>
FLOAT32 LWSpecs::ReadField(Json::Value& value)
{
    return strtof(value.GetString(), nullptr);
}

template <>
string LWSpecs::ReadField(Json::Value& value)
{
    string s = value.GetString();
    TOUPPER(s);
    return s;
}

//-----------------------------------------------------------------------------
template <>
void LWSpecs::PrintField(Tee::Priority pri, const string& name, bool val, const string& units)
{
    Printf(pri, "%s = %s %s\n", name.c_str(), val ? "true" : "false", units.c_str());
}

template <>
void LWSpecs::PrintField(Tee::Priority pri, const string& name, UINT32 val, const string& units)
{
    Printf(pri, "%s = %d %s\n", name.c_str(), val, units.c_str());
}

template <>
void LWSpecs::PrintField(Tee::Priority pri, const string& name, UINT64 val, const string& units)
{
    Printf(pri, "%s = %llu %s\n", name.c_str(), val, units.c_str());
}

template <>
void LWSpecs::PrintField(Tee::Priority pri, const string& name, FLOAT32 val, const string& units)
{
    Printf(pri, "%s = %g %s\n", name.c_str(), val, units.c_str());
}

template <>
void LWSpecs::PrintField(Tee::Priority pri, const string& name, string val, const string& units)
{
    Printf(pri, "%s = %s %s\n", name.c_str(), val.c_str(), units.c_str());
}

//-----------------------------------------------------------------------------
RC LWSpecs::CheckHasMember
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

JS_CLASS_INHERIT(LWSpecs, GpuTest, "LWSpecs");
CLASS_PROP_READWRITE(LWSpecs, Filename, string, "Name of the json file to parse");
CLASS_PROP_READWRITE(LWSpecs, Override, bool, "bool: Run the test even without a Filename specified. It will print everything available.");
