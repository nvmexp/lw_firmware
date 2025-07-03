/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gputest.h"
#include "core/include/utility.h"
#include "device/interface/c2c.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pwrsub.h"
#include "vbiosdcb4x.h"

class CheckConfig2 : public GpuTest
{
public:
    CheckConfig2()
    {
        SetName("CheckConfig2");
    }

    SETGET_PROP(SkipHulkCheck, bool);
    SETGET_PROP(RamCfgStrap, string);
    SETGET_PROP(ExpVbiosChipSku, string);
    SETGET_PROP(SkipIsenseAdcCalCheck, bool);
    SETGET_PROP(SkipPwrSensorCheck, bool);
    SETGET_PROP(C2CFOMFailThresh, UINT32)
    SETGET_PROP(C2CFOMPassThresh, UINT32)
    SETGET_PROP(C2CWindowsPassThresh, UINT32)
    SETGET_PROP(C2CWindowsFailThresh, UINT32)
    SETGET_PROP(C2CPrintMarginal, bool);


    bool IsSupported() override
    {
        return GetBoundGpuSubdevice()->GetParentDevice()->GetFamily() >= GpuDevice::Turing;
    }

    RC Setup() override;

    RC Run() override
    {
        RC rc;
        CHECK_RC(CheckRamCfgStrap());
        CHECK_RC(CheckVbiosChipSku());
        CHECK_RC(CheckHulk());
        CHECK_RC(CheckIsenseAdcCal());
        CHECK_RC(CheckPwrINA3221());
        CHECK_RC(CheckOCBin());
        CHECK_RC(CheckC2CFOM());
        return rc;
    }

    void PrintJsProperties(Tee::Priority pri) override;

private:
    bool m_SkipHulkCheck = false;
    string m_RamCfgStrap;
    string m_ExpVbiosChipSku;
    bool m_SkipIsenseAdcCalCheck = false;
    bool m_SkipPwrSensorCheck = false;

    UINT32 m_C2CFOMFailThresh     = 0;
    UINT32 m_C2CFOMPassThresh     = 0;
    UINT32 m_C2CWindowsPassThresh = ~0U;
    UINT32 m_C2CWindowsFailThresh = ~0U;
    bool   m_C2CPrintMarginal     = false;

    RC CheckRamCfgStrap();
    RC CheckVbiosChipSku();
    RC CheckHulk();
    RC CheckIsenseAdcCal();
    RC CheckPwrINA3221();
    RC CheckOCBin();
    RC CheckC2CFOM();
    bool AreOffsetsEnabled();
};

/**
 * \brief Check whether either of the LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_OVRM_NUM
 *        offset(s) is enabled.
 */
bool CheckConfig2::AreOffsetsEnabled()
{
    LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS devParams = {0};

    if (RC::OK != LwRmPtr()->ControlBySubdevice(GetBoundGpuSubdevice(),
            LW2080_CTRL_CMD_PMGR_PWR_DEVICES_GET_INFO,
            &devParams, sizeof(devParams)))
    {
        Printf(Tee::PriError, "Loading PWR DEVICES _INFO failed\n");
        return false;
    }

    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICES_MAX_DEVICES; i++)
    {
        if (devParams.devMask & (1 << i) &&
            devParams.devices[i].type == LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11)
        {
            LW2080_CTRL_PMGR_PWR_DEVICE_INFO_DATA_GPUADC11 &gpuAdc11 =
                devParams.devices[i].data.gpuAdc11;
            for (UINT32 j = 0; j < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_OVRM_NUM; j++)
            {
                if ((gpuAdc11.ovrm.device[j].gen ==
                            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_OVRM_GEN_1) &&
                    (gpuAdc11.offset.instance[j].provIdx !=
                         LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_PROV_IDX_NONE))
                {
                    return true;
                }
                else if (gpuAdc11.ovrm.device[j].gen ==
                        LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_OVRM_GEN_2)
                {
                    if (gpuAdc11.ovrm.device[j].data.gen2.bGround == LW_TRUE)
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

RC CheckConfig2::Setup()
{
    RC rc;

    Power* pPower = GetBoundGpuSubdevice()->GetPower();
    if (!pPower)
    {
        m_SkipIsenseAdcCalCheck = true;
        m_SkipPwrSensorCheck = true;
    }
    else if (!pPower->HasAdcPwrSensor())
    {
        m_SkipIsenseAdcCalCheck = true;
    }

    if (GetBoundTestDevice()->SupportsInterface<C2C>())
    {
        auto * pC2C = GetBoundTestDevice()->GetInterface<C2C>();
        const UINT32 activePartitionMask = pC2C->GetActivePartitionMask();
        if (activePartitionMask)
        {
            const UINT32 maxFOMWindows = pC2C->GetMaxFomWindows();
            if (m_C2CWindowsPassThresh == ~0U)
                m_C2CWindowsPassThresh = maxFOMWindows;
            if (m_C2CWindowsFailThresh == ~0U)
                m_C2CWindowsFailThresh = maxFOMWindows;

            if (m_C2CFOMFailThresh > m_C2CFOMPassThresh)
            {
                Printf(Tee::PriError,
                    "C2C FOM fail threshold (%u) may not be greater than pass threshold (%u)\n",
                    m_C2CFOMFailThresh, m_C2CFOMPassThresh);
                return RC::BAD_PARAMETER;
            }
            if (m_C2CWindowsFailThresh < m_C2CWindowsPassThresh)
            {
                Printf(Tee::PriError,
                    "C2C FOM windows fail threshold (%u) may not be less than pass threshold (%u)\n",
                    m_C2CWindowsFailThresh, m_C2CWindowsPassThresh);
                return RC::BAD_PARAMETER;
            }
        }
    }

    CHECK_RC(GpuTest::Setup());

    return rc;
}

void CheckConfig2::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "CheckConfig2 Js Properties:\n");
    Printf(pri, "\t%-31s %s\n", "ExpVbiosChipSku:", m_ExpVbiosChipSku.c_str());
    Printf(pri, "\t%-31s %s\n", "RamCfgStrap:", m_RamCfgStrap.c_str());
    Printf(pri, "\t%-31s %s\n", "SkipHulkCheck:", m_SkipHulkCheck ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "SkipIsenseAdcCalCheck:", m_SkipIsenseAdcCalCheck ? "true" : "false"); //$
    Printf(pri, "\t%-31s %s\n", "SkipPwrSensorCheck:", m_SkipPwrSensorCheck ? "true" : "false");
    Printf(pri, "\t%-31s %u\n", "C2CFOMFailThresh:", m_C2CFOMFailThresh);
    Printf(pri, "\t%-31s %u\n", "C2CFOMPassThresh:", m_C2CFOMPassThresh);
    Printf(pri, "\t%-31s %u\n", "C2CWindowsPassThresh:", m_C2CWindowsPassThresh);
    Printf(pri, "\t%-31s %u\n", "C2CWindowsFailThresh:", m_C2CWindowsFailThresh);
    Printf(pri, "\t%-31s %s\n", "C2CPrintMarginal:", m_C2CPrintMarginal ? "true" : "false");
}

// Check that the Ram Config Strap matches one of the given expected values
RC CheckConfig2::CheckRamCfgStrap()
{
    if (m_RamCfgStrap.empty())
    {
        return RC::OK;
    }

    VerbosePrintf("Checking RAM config straps\n");

    UINT32 strapValue = GetBoundGpuSubdevice()->RamConfigStrap();

    vector<string> strapExpectedValues;
    // Allow space- and/or comma- separated values
    Utility::Tokenizer(m_RamCfgStrap, ", ", &strapExpectedValues);
    for (string& strapExpectedVal : strapExpectedValues)
    {
        char* end = nullptr;
        UINT32 expectedVal = strtol(strapExpectedVal.c_str(), &end, 0);
        if (end != (strapExpectedVal.c_str() + strapExpectedVal.size()))
        {
            Printf(Tee::PriError, "Un-parsable value: \"%s\"\n", strapExpectedVal.c_str());
            return RC::BAD_PARAMETER;
        }
        if (expectedVal == strapValue)
        {
            // We matched one of the values, so we're done
            return RC::OK;
        }
    }

    Printf(Tee::PriError,
           "Strap value did not match expected values: 0x%x; Expected: %s\n",
           strapValue, m_RamCfgStrap.c_str());

    return RC::WRONG_MEMORY_STRAP_VALUE;
}

RC CheckConfig2::CheckHulk()
{
    if (m_SkipHulkCheck)
    {
        return RC::OK;
    }

    VerbosePrintf("Checking for HULK licenses\n");

    RC rc;
    bool hulkFlashed;
    CHECK_RC(GetBoundGpuSubdevice()->GetIsHulkFlashed(&hulkFlashed));

    if (hulkFlashed)
    {
        Printf(Tee::PriError, "Flashed HULK licenses are not allowed!\n");
        return RC::ILWALID_INFO_ROM;
    }
    return rc;
}

RC CheckConfig2::CheckVbiosChipSku()
{
    if (m_ExpVbiosChipSku.empty())
    {
        return RC::OK;
    }

    VerbosePrintf("Checking VBIOS chip SKU\n");

    RC rc;
    string sku;
    CHECK_RC(GetBoundGpuSubdevice()->GetChipSkuNumberModifier(&sku));

    // VBIOS forces its sku name to upper-case, so forcing the expected string to-upper for better
    // user-experience.
    const string exp = Utility::ToUpperCase(m_ExpVbiosChipSku);
    if (sku != exp)
    {
        Printf(Tee::PriError, "Incorrect vbios image flashed. Expected chip sku was %s, but found "
                              "%s!\n", exp.c_str(), sku.c_str());
        return RC::WRONG_BIOS;
    }

    return rc;
}

RC CheckConfig2::CheckIsenseAdcCal()
{
    if (m_SkipIsenseAdcCalCheck)
    {
        return RC::OK;
    }

    VerbosePrintf("Checking ISENSE ADC programming\n");

    GpuSubdevice::AdcCalibrationReg fusedRegister;
    GpuSubdevice::AdcCalibrationReg programmedRegister;
    GpuSubdevice::AdcRawCorrectionReg correctionRegister;
    GpuSubdevice::AdcMulSampReg mulSampRegister;

    if (GetBoundGpuSubdevice()->GetAdcCalFuseVals(&fusedRegister) != RC::OK)
    {
        Printf(Tee::PriLow, "LW_FUSE_OPT_ADC_CAL_SENSE is returning "
                            "unsupported\n");
        return RC::OK;
    }

    if (GetBoundGpuSubdevice()->GetAdcProgVals(&programmedRegister) != RC::OK)
    {
        Printf(Tee::PriLow, "LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL2 is "
                                   "returning unsupported\n");
        return RC::OK;
    }

    VerbosePrintf("Checking ISENSE ADC calibration registers against fuses\n");

    VerbosePrintf("Fused offset: 0x%04x, Programmed offset: 0x%04x\n",
        fusedRegister.fieldVcmOffset, programmedRegister.fieldVcmOffset);
    if (fusedRegister.fieldVcmOffset != programmedRegister.fieldVcmOffset)
    {
        Printf(Tee::PriError,
            "Programmed ADC_OFFSET (0x%04x) mismatches fuse value (0x%04x)\n",
            programmedRegister.fieldVcmOffset, fusedRegister.fieldVcmOffset);
        return RC::AVFS_ADC_CALIBRATION_ERROR;
    }

    VerbosePrintf("Fused gain: 0x%04x, Programmed gain: 0x%04x\n",
        fusedRegister.fieldDiffGain, programmedRegister.fieldDiffGain);
    if (fusedRegister.fieldDiffGain != programmedRegister.fieldDiffGain)
    {
        Printf(Tee::PriError,
            "Programmed ADC_GAIN (0x%04x) mismatches fuse value (0x%04x)\n",
            programmedRegister.fieldDiffGain, fusedRegister.fieldDiffGain);
        return RC::AVFS_ADC_CALIBRATION_ERROR;
    }

    VerbosePrintf("Fused Coarse offset: 0x%04x, Programmed Coarse offset: 0x%04x\n",
        fusedRegister.fieldVcmCoarse, programmedRegister.fieldVcmCoarse);
    if (fusedRegister.fieldVcmCoarse != programmedRegister.fieldVcmCoarse)
    {
        Printf(Tee::PriError,
            "Programmed ADC_COARSE (0x%04x) mismatches fuse value (0x%04x)\n",
            programmedRegister.fieldVcmCoarse, fusedRegister.fieldVcmCoarse);
        return RC::AVFS_ADC_CALIBRATION_ERROR;
    }

    // V_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_MULTISAMPLER only
    // supported in GA10x.
    if (GetBoundGpuSubdevice()->GetAdcRawCorrectVals(&correctionRegister) != RC::OK)
    {
        Printf(Tee::PriLow, "LW_THERM_ADC_RAW_CORRECTION is "
                               "returning unsupported\n");
        return RC::OK;

    }

    if (GetBoundGpuSubdevice()->GetAdcMulSampVals(&mulSampRegister) != RC::OK)
    {
        Printf(Tee::PriLow, "LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_MULTISAMPLER is "
                               "returning unsupported\n");
        return RC::OK;
    }

    if (AreOffsetsEnabled())
    {
        const UINT32 expectedValue = 0;
        if (correctionRegister.fieldValue != expectedValue)
        {
            Printf(Tee::PriError,
                "Offset not enabled ADC Raw correction should be (0x%011x) but is (0x%011x)\n",
                expectedValue, correctionRegister.fieldValue);
            return RC::AVFS_ADC_CALIBRATION_ERROR;
        }
    }
    else
    {
        const UINT32 expectedValue = ((mulSampRegister.fieldNumSamples + 1) *
                fusedRegister.fieldOffsetCal) >> 2;
        if (correctionRegister.fieldValue != expectedValue)
        {
            Printf(Tee::PriError,
                "ADC Raw Correction (0x%011x) mismatches expected value (0x%011x)\n",
                correctionRegister.fieldValue, expectedValue);
            return RC::AVFS_ADC_CALIBRATION_ERROR;
        }
    }

    return RC::OK;
}

RC CheckConfig2::CheckPwrINA3221()
{
    if (m_SkipPwrSensorCheck)
    {
        return RC::OK;
    }

    RC rc;

    VerbosePrintf("Checking INA3221 Power Sensor Configuration\n");

    I2c::I2cDcbDevInfo i2cDcbDevInfo;
    rc = GetBoundGpuSubdevice()->GetInterface<I2c>()->GetI2cDcbDevInfo(&i2cDcbDevInfo);
    if (rc == RC::LWRM_ERROR || rc == RC::GENERIC_I2C_ERROR)
    {
        Printf(Tee::PriError, "Failed to get I2C device information from DCB\n");
        return RC::I2C_DEVICE_NOT_FOUND;
    }

    VerbosePrintf("INA3221: DCB data found\n");

    // Fetch all INA3221 type devices from Power Sensor Table
    Power::PwrSensorCfg pwrSensorCfg = {};
    Power* const pPower = GetBoundGpuSubdevice()->GetPower();
    rc = pPower->GetPowerSensorCfgVals(LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221,
                                       &pwrSensorCfg);

    if (rc == RC::I2C_DEVICE_NOT_FOUND)
    {
        // Skip testing if Power Sensor is not configured in VBIOS
        Printf(Tee::PriNormal, "INA3221: Sensor not configured. Test Skipped.\n");
        return RC::OK;
    }

    VerbosePrintf("INA3221: Power Sensor Table data found\n");

    for (auto const& it : pwrSensorCfg)
    {
        UINT32 biosDcbTblVal = 0;
        I2c::I2cDcbDevInfoEntry i2cEntryInfo = {};
        bool bDeviceFound = false;
        for (UINT32 i = 0; i < i2cDcbDevInfo.size(); i++)
        {
            // Create a map of all devices matching i2cType
            if (i2cDcbDevInfo[i].Type == LW_DCB4X_I2C_DEVICE_TYPE_POWER_SENSOR_INA3221 &&
                i2cDcbDevInfo[i].I2CDeviceIdx == it.I2cIndex)
            {
                bDeviceFound = true;
                i2cEntryInfo = i2cDcbDevInfo[i];
                break;
            }
        }

        if (!bDeviceFound)
        {
            Printf(Tee::PriError, "Power Sensor and DCB tables data is not matching."
                                  "Check VBIOS\n");
            return RC::BOARD_TEST_ERROR;
        }

        I2c::Dev i2cDev = GetBoundGpuSubdevice()->GetInterface<I2c>()->I2cDev(i2cEntryInfo.I2CLogicalPort,
                                                                              i2cEntryInfo.I2CAddress);
        // Read Dword
        i2cDev.SetOffsetLength(1);
        i2cDev.SetMessageLength(2);

        // Read Config register 0x00
        rc = i2cDev.Read(0x00, &biosDcbTblVal);

        VerbosePrintf("INA3221: Verifying Sensor\n");
        VerbosePrintf("I2C config data = %04x\n", biosDcbTblVal);
        VerbosePrintf("Power Sensor Table(VBIOS) = %04x\n", it.Configuration);
        VerbosePrintf("I2C Port = %02x\n", i2cEntryInfo.I2CLogicalPort);
        VerbosePrintf("I2C Address = %02x\n", i2cEntryInfo.I2CAddress);

        if (rc != RC::OK ||
            biosDcbTblVal != it.Configuration)
        {
            Printf(Tee::PriError,
                    "INA3221 sensor configuration (%04x) mismatches expected value (%04x)\n",
                    biosDcbTblVal, it.Configuration);
            return RC::BOARD_TEST_ERROR;
        }
    }

    VerbosePrintf("Success: Checking INA3221 Power Sensor Configuration\n");

    return rc;
}

RC CheckConfig2::CheckOCBin()
{
    RC rc;
    VerbosePrintf("Checking OC Bin Configuration\n");

    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();

    UINT32 expectedOCBin = 0;
    UINT32 foundOCBin = 0;

    if (RC::OK != pPerf->GetOCBilwalue(&expectedOCBin))
    {
        Printf(Tee::PriLow, "Board does not support OC Bin\n");
        return RC::OK;
    }
    VerbosePrintf("Expected OC Bin Value: %u\n", expectedOCBin);

    if (RC::OK != pPerf->GetOCBinFuseValue(&foundOCBin))
    {
        Printf(Tee::PriLow, "Board does not support OC Bin fuse\n");
        return RC::OK;
    }
    VerbosePrintf("Found OC Bin Value: %u\n", foundOCBin);

    if (expectedOCBin > foundOCBin)
    {
        Printf(Tee::PriError,
            "OC Bin Configuration mismatch. Expected Value: %u. Found Value: %u\n",
            expectedOCBin, foundOCBin);

        return RC::BOARD_TEST_ERROR;
    }

    VerbosePrintf("Success: Checking OC Bin Configuration\n");
    return rc;
}

RC CheckConfig2::CheckC2CFOM()
{
    TestDevicePtr pTestDevice = GetBoundTestDevice();

    if (!pTestDevice->SupportsInterface<C2C>())
        return RC::OK;

    auto * pC2C = pTestDevice->GetInterface<C2C>();
    const UINT32 activePartitionMask = pC2C->GetActivePartitionMask();
    if (!activePartitionMask)
        return RC::OK;

    RC rc;
    StickyRC fomRc;

    const UINT32 maxLinks = pC2C->GetMaxLinks();
    const UINT32 maxFOMValue = pC2C->GetMaxFomValue();
    const UINT32 maxFOMWindows = pC2C->GetMaxFomWindows();
    for (UINT32 linkId = 0; linkId < maxLinks; ++linkId)
    {
        C2C::LinkStatus linkStatus = C2C::LinkStatus::Off;
        CHECK_RC(pC2C->GetLinkStatus(linkId, &linkStatus));

        if (linkStatus != C2C::LinkStatus::Active)
            continue;

        vector<C2C::FomData> fomData;
        string fomStr = Utility::StrPrintf("C2C Link %u Lanes (0-%u) FOM Values = ",
                                           linkId, static_cast<UINT32>(fomData.size()));
        string fomWinStr = Utility::StrPrintf("C2C Link %u Lanes (0-%u) FOM Windows = ",
                                              linkId, static_cast<UINT32>(fomData.size()));
        string comma = "";
        CHECK_RC(pC2C->GetFomData(linkId, &fomData));
        for (size_t lwrLane = 0; lwrLane < fomData.size(); lwrLane++)
        {
            const UINT16 fomVal = fomData[lwrLane].passingWindow;

            fomStr += Utility::StrPrintf("%s%u", comma.c_str(), fomVal);
            if (fomVal > maxFOMValue)
            {
                Printf(Tee::PriError,
                       "%s : C2C Link %u Lane(%u) FOM = %u is above"
                       " maximum FOM value = %u\n",
                       pTestDevice->GetName().c_str(),
                       linkId, static_cast<UINT32>(lwrLane), fomVal,
                       maxFOMValue);
                fomRc = RC::UNEXPECTED_RESULT;
            }
            else if (fomVal == 0)
            {
                Printf(Tee::PriError,
                       "%s : C2C Link %u Lane(%u) FOM = 0\n",
                       pTestDevice->GetName().c_str(),
                       linkId, static_cast<UINT32>(lwrLane));
                fomRc = RC::UNEXPECTED_RESULT;
            }
            else if (fomVal < m_C2CFOMPassThresh)
            {
                if (fomVal <= m_C2CFOMFailThresh)
                {
                    Printf(Tee::PriError,
                           "%s : C2C Link %u Lane(%u) FOM = %u is %s"
                           " Fail threshold = %u\n",
                           pTestDevice->GetName().c_str(),
                           linkId, static_cast<UINT32>(lwrLane), fomVal,
                           (fomVal == m_C2CFOMFailThresh) ? "at" : "below",
                           m_C2CFOMFailThresh);
                    fomRc = RC::UNEXPECTED_RESULT;
                }
                else if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) ||
                         m_C2CPrintMarginal)
                {
                    Printf(Tee::PriWarn,
                           "%s : C2C Link %u Lane(%u) could be potentially marginal. FOM"
                           " = %u is above Fail threshold = %u but below Pass threshold = %u\n",
                           pTestDevice->GetName().c_str(),
                           linkId, static_cast<UINT32>(lwrLane), fomVal, m_C2CFOMFailThresh,
                           m_C2CFOMPassThresh);
                }
            }

            const UINT16 windowsVal = fomData[lwrLane].numWindows;
            fomWinStr += Utility::StrPrintf("%s%u", comma.c_str(), windowsVal);
            if (windowsVal < 2)
            {
                Printf(Tee::PriError,
                       "%s : C2C Link %u Lane(%u) FOM windows = %u, minimum FOM windows = 2\n",
                       pTestDevice->GetName().c_str(),
                       linkId, static_cast<UINT32>(lwrLane), windowsVal);
                fomRc = RC::UNEXPECTED_RESULT;
            }
            else if (windowsVal > maxFOMWindows)
            {
                Printf(Tee::PriError,
                       "%s : C2C Link %u Lane(%u) FOM windows = %u, maximum FOM windows = %u\n",
                       pTestDevice->GetName().c_str(),
                       linkId, static_cast<UINT32>(lwrLane), windowsVal, maxFOMWindows);
                fomRc = RC::UNEXPECTED_RESULT;
            }
            else if (windowsVal > m_C2CWindowsPassThresh)
            {
                if (windowsVal >= m_C2CWindowsFailThresh)
                {
                    Printf(Tee::PriError,
                           "%s : C2C Link %u Lane(%u) FOM windows = %u is %s"
                           " Fail threshold = %u\n",
                           pTestDevice->GetName().c_str(),
                           linkId, static_cast<UINT32>(lwrLane), windowsVal,
                           (windowsVal == m_C2CWindowsFailThresh) ? "at" : "above",
                           m_C2CWindowsFailThresh);
                    fomRc = RC::UNEXPECTED_RESULT;
                }
                else if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) ||
                         m_C2CPrintMarginal)
                {
                    Printf(Tee::PriWarn,
                           "%s : C2C Link %u Lane(%u) could be potentially marginal. FOM"
                           "windows = %u is below Fail threshold = %u but above"
                           " Pass threshold = %u\n",
                           pTestDevice->GetName().c_str(),
                           linkId, static_cast<UINT32>(lwrLane), windowsVal, m_C2CWindowsFailThresh,
                           m_C2CWindowsFailThresh);
                }
            }
            if (comma.empty())
                comma = ", ";
        }
        VerbosePrintf("%s\n", fomStr.c_str());
        VerbosePrintf("%s\n", fomWinStr.c_str());
    }
    return (rc == RC::OK) ? fomRc : rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

JS_CLASS_INHERIT(CheckConfig2, GpuTest,
                 "Configuration test");

CLASS_PROP_READWRITE(CheckConfig2, RamCfgStrap, string,
                     "Expected values for RamCfgStrap.");

CLASS_PROP_READWRITE(CheckConfig2, ExpVbiosChipSku, string,
                     "Expected values for Vbios Chip Sku.");

CLASS_PROP_READWRITE(CheckConfig2, SkipHulkCheck, bool,
                     "Skip the HULK check");

CLASS_PROP_READWRITE(CheckConfig2, SkipIsenseAdcCalCheck, bool,
                     "Skip the ISENSE ADC register vs. fuse check");

CLASS_PROP_READWRITE(CheckConfig2, SkipPwrSensorCheck, bool,
                     "Skip the Power Sensor vs. I2C device check");

CLASS_PROP_READWRITE(CheckConfig2, C2CFOMFailThresh, UINT32,
                     "Pass threshold for C2C FOM");
CLASS_PROP_READWRITE(CheckConfig2, C2CFOMPassThresh, UINT32,
                     "Fail threshold for C2C FOM");
CLASS_PROP_READWRITE(CheckConfig2, C2CWindowsPassThresh, UINT32,
                     "Pass threshold for C2C FOM Windows");
CLASS_PROP_READWRITE(CheckConfig2, C2CWindowsFailThresh, UINT32,
                     "Fail threshold for C2C FOM Windows");
CLASS_PROP_READWRITE(CheckConfig2, C2CPrintMarginal, bool,
                     "Print marginal FOM messages when value is between Pass/Fail");
