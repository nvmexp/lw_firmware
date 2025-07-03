/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Headers from outside mods
#include "ctrl/ctrl2080.h"

// Mods headers
#include "gpu/perf/avfssub.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/avfssub.h"
#include "core/include/rc.h"
#include "core/include/utility.h"

Avfs::Avfs(GpuSubdevice *pSubdevice) :
    m_pSubdevice(pSubdevice)
   ,m_IsInitialized(false)
   ,m_CalibrationStateHasChanged(false)
   ,m_AdcDevMask(0)
   ,m_NafllDevMask(0)
   ,m_LutNumEntries(0)
   ,m_LutStepSizeuV(0)
   ,m_LutMilwoltageuV(0)
   ,m_AdcInfoPrintPri(Tee::PriSecret)
{
}

Avfs::~Avfs() {}

RC Avfs::Initialize()
{
    Printf(Tee::PriDebug, "Beginning mods AVFS initialization\n");
    RC rc;
    if (m_IsInitialized)
    {
        return OK;
    }

    // Get device mask for ADCs on chip
    LW2080_CTRL_CLK_ADC_DEVICES_INFO_PARAMS adcInfoParams = {};
    rc = LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_ADC_DEVICES_GET_INFO
       ,&adcInfoParams
       ,sizeof(adcInfoParams));

    if ((rc == RC::LWRM_NOT_SUPPORTED) ||
        (adcInfoParams.super.objMask.super.pData[0] == 0))
    {
        // Don't set m_Initialized to true to prevent restoration of ADCs
        Printf(Tee::PriLow, "This GPU does not support AVFS\n");
        return OK;
    }
    CHECK_RC(rc);

    m_AdcDevMask = adcInfoParams.super.objMask.super.pData[0];

    Printf(Tee::PriDebug, "Global ADC info:\n");
    Printf(Tee::PriDebug, " bAdcIsDisableAllowed = %s\n",
           adcInfoParams.bAdcIsDisableAllowed ? "true" : "false");
    switch (adcInfoParams.version)
    {
        case LW2080_CTRL_CLK_ADC_DEVICES_V10:
        {
            const auto& data = adcInfoParams.data.adcsV10;
            Printf(Tee::PriDebug, " calibrationRevVbios = %u\n", data.calibrationRevVbios);
            Printf(Tee::PriDebug, " calibrationRevFused = %u\n", data.calibrationRevFused);
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICES_V20:
        {
            const auto& data = adcInfoParams.data.adcsV20;
            Printf(Tee::PriDebug, " calIlwalidFuseRevMask = %u\n", data.calIlwalidFuseRevMask);
            Printf(Tee::PriDebug, " lowTempErrIlwalidFuseRevMask = %u\n",
                data.lowTempErrIlwalidFuseRevMask);
            Printf(Tee::PriDebug, " highTempErrIlwalidFuseRevMask = %u\n",
                data.highTempErrIlwalidFuseRevMask);
            Printf(Tee::PriDebug, " lowTemp = %d\n", data.lowTemp);
            Printf(Tee::PriDebug, " highTemp = %d\n", data.highTemp);
            Printf(Tee::PriDebug, " refTemp = %d\n", data.refTemp);
            Printf(Tee::PriDebug, " adcCodeCorrectionOffsetMin = %d\n",
                data.adcCodeCorrectionOffsetMin);
            Printf(Tee::PriDebug, " adcCodeCorrectionOffsetMax = %d\n",
                data.adcCodeCorrectionOffsetMax);
            Printf(Tee::PriDebug, " lowVoltuV = %u\n", data.lowVoltuV);
            Printf(Tee::PriDebug, " highVoltuV = %u\n", data.highVoltuV);
            break;
        }
    }

    for (UINT32 i = 0; i < LW2080_CTRL_CLK_ADC_MAX_DEVICES; ++i)
    {
        UINT32 mask = 1 << i;
        if (!(mask & m_AdcDevMask))
        {
            continue;
        }
        const LW2080_CTRL_CLK_ADC_DEVICE_INFO &adcDevInfo = adcInfoParams.devices[i];
        Printf(Tee::PriDebug, "ADC index = %d\n", i);
        Gpu::SplitVoltageDomain voltDom =
            m_pSubdevice->GetVolt3x()->RmVoltDomToGpuSplitVoltageDomain(adcDevInfo.voltDomain);
        PrintSingleAdcInfo(m_AdcInfoPrintPri, voltDom, adcDevInfo);
        AdcId id;
        CHECK_RC(TranslateLw2080AdcIdToAdcId(adcDevInfo.id, &id));
        m_adcIdToDevInd[id] = i;

        m_VoltDomToAdcDevMask[voltDom] |= mask;
    }

    Printf(Tee::PriDebug, "ADC dev mask = 0x%x\n", m_AdcDevMask);

    CHECK_RC(GetOriginalAdcCalibrationState(&m_OriginalCalibrationState));

    // Get device mask for NAFLLs on chip
    LW2080_CTRL_CLK_NAFLL_DEVICES_INFO_PARAMS nafllInfoParams = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_INFO
       ,&nafllInfoParams
       ,sizeof(nafllInfoParams)));

    m_NafllDevMask = nafllInfoParams.super.objMask.super.pData[0];
    m_LutNumEntries = nafllInfoParams.lutNumEntries;
    m_LutStepSizeuV = nafllInfoParams.lutStepSizeuV;
    m_LutMilwoltageuV = nafllInfoParams.lutMilwoltageuV;

    Printf(Tee::PriDebug, "Global NAFLL info:\n");
    Printf(Tee::PriDebug, " maxDvcoMinFreqMHZ = %u\n", nafllInfoParams.maxDvcoMinFreqMHz);
    Printf(Tee::PriDebug, " lutNumEntries = %d\n", nafllInfoParams.lutNumEntries);
    Printf(Tee::PriDebug, " lutStepSizeuV = %d\n", nafllInfoParams.lutStepSizeuV);
    Printf(Tee::PriDebug, " lutMilwoltageuV = %d\n", nafllInfoParams.lutMilwoltageuV);

    for (UINT32 i = 0; i < LW2080_CTRL_CLK_NAFLL_MAX_DEVICES; ++i)
    {
        UINT32 mask = 1 << i;
        if (!(m_NafllDevMask & mask))
        {
            continue;
        }
        LW2080_CTRL_CLK_NAFLL_DEVICE_INFO &nafllDevInfo = nafllInfoParams.devices[i];
        Printf(Tee::PriDebug, "NAFLL index = %d\n", i);
        PrintSingleNafllInfo(nafllDevInfo);

        NafllInfo newNafllInfo = {};
        newNafllInfo.devIndex = i;
        newNafllInfo.clkDomain = PerfUtil::ClkCtrl2080BitToGpuClkDomain(nafllDevInfo.clkDomain);

        NafllId id;
        CHECK_RC(TranslateLw2080NafllIdToNafllId(nafllDevInfo.id, &id));
        m_nafllIdToInfo[id] = newNafllInfo;

        m_clkDomainToNafllIdSet[newNafllInfo.clkDomain].insert(id);

        m_ClkDomainToFFRLimitMHz[newNafllInfo.clkDomain] =
            static_cast<UINT32>(nafllInfoParams.devices[i].fixedFreqRegimeLimitMHz);
    }

    Printf(Tee::PriDebug, "NAFLL dev mask = 0x%x\n", m_NafllDevMask);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_CLK_VOLT_CONTROLLERS_GET_INFO
       ,&m_ClvcInfo
       ,sizeof(m_ClvcInfo)));
    CHECK_RC(GetEnabledVoltControllers(&m_ClvcVoltDoms));

    m_IsInitialized = true;
    m_CalibrationStateHasChanged = false;
    Printf(Tee::PriDebug, "Avfs initialized\n");
    return OK;
}

RC Avfs::Cleanup()
{
    if (!m_IsInitialized)
    {
        return OK;
    }

    RC rc;

    // Only reset the ADC calibration if it has changed
    if (m_CalibrationStateHasChanged)
    {
        CHECK_RC(SetAdcCalibrationStateViaMask(
            m_AdcDevMask, m_OriginalCalibrationState));
    }

    m_OriginalCalibrationState.clear();
    m_adcIdToDevInd.clear();
    m_IsInitialized = false;
    return rc;
}

RC Avfs::GetAdcDeviceStatusViaMask
(
    UINT32 adcDevMask,
    LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS *adcStatusParams
)
{
    MASSERT(adcDevMask);
    MASSERT(IS_MASK_SUBSET(adcDevMask, GetAdcDevMask()));
    MASSERT(adcStatusParams);

    RC rc;
    memset(adcStatusParams, 0, sizeof(*adcStatusParams));
    adcStatusParams->super.objMask.super.pData[0] = adcDevMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_ADC_DEVICES_GET_STATUS
       ,adcStatusParams
       ,sizeof(*adcStatusParams)));

    return OK;
}

RC Avfs::GetAdcDeviceStatus
(
    LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS *adcStatusParams
)
{
    return GetAdcDeviceStatusViaMask(GetAdcDevMask(), adcStatusParams);
}

void Avfs::GetNafllClockDomainFFRLimitMHz
(
    map<Gpu::ClkDomain, UINT32> *pClkDomainToFFRLimitMHz
)
{
    for (map<Gpu::ClkDomain, UINT32>::const_iterator domItr = m_ClkDomainToFFRLimitMHz.begin();
         domItr != m_ClkDomainToFFRLimitMHz.end();
         ++domItr)
    {
        (*pClkDomainToFFRLimitMHz)[domItr->first] = domItr->second;
    }
}

RC Avfs::GetAdcDeviceStatusViaId
(
    AdcId id,
    LW2080_CTRL_CLK_ADC_DEVICE_STATUS *adcStatus
)
{
    MASSERT(adcStatus);
    RC rc;
    UINT32 adcIndex;
    CHECK_RC(TranslateAdcIdToDevIndex(id, &adcIndex));
    UINT32 devMask = 1 << adcIndex;
    LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS adcStatusParams = {};
    CHECK_RC(GetAdcDeviceStatusViaMask(devMask, &adcStatusParams));
    // POD copy here
    *adcStatus = adcStatusParams.devices[adcIndex];
    return rc;
}

RC Avfs::GetAdcVoltageViaId
(
    AdcId id,
    UINT32 *actualVoltageuV,
    UINT32 *correctedVoltageuV
)
{
    RC rc;
    LW2080_CTRL_CLK_ADC_DEVICE_STATUS adcDevStatus = {};
    CHECK_RC(GetAdcDeviceStatusViaId(id, &adcDevStatus));
    if (actualVoltageuV)
    {
        *actualVoltageuV = adcDevStatus.correctedVoltageuV;
    }
    if (correctedVoltageuV)
    {
        *correctedVoltageuV = adcDevStatus.correctedVoltageuV;
    }
    return rc;
}

RC Avfs::GetNafllDeviceStatusViaMask
(
    UINT32 nafllDevMask,
    LW2080_CTRL_CLK_NAFLL_DEVICES_STATUS_PARAMS *nafllStatusParams
)
{
    MASSERT(nafllDevMask);
    MASSERT(IS_MASK_SUBSET(nafllDevMask, GetNafllDevMask()));
    MASSERT(nafllStatusParams);

    RC rc;
    memset(nafllStatusParams, 0, sizeof(*nafllStatusParams));
    nafllStatusParams->super.objMask.super.pData[0] = nafllDevMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_STATUS
       ,nafllStatusParams
       ,sizeof(*nafllStatusParams)));

    return rc;
}

RC Avfs::GetNafllDeviceStatus
(
    LW2080_CTRL_CLK_NAFLL_DEVICES_STATUS_PARAMS *nafllStatusParams
)
{
    return GetNafllDeviceStatusViaMask(GetNafllDevMask(), nafllStatusParams);
}

RC Avfs::GetNafllDeviceStatusViaId
(
    Avfs::NafllId id,
    LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS *nafllStatus
)
{
    RC rc;
    MASSERT(nafllStatus);
    UINT32 nafllDevIndex;
    CHECK_RC(TranslateNafllIdToDevIndex(id, &nafllDevIndex));

    UINT32 nafllDevMask = 1 << nafllDevIndex;

    LW2080_CTRL_CLK_NAFLL_DEVICES_STATUS_PARAMS nafllStatusParams = {};
    CHECK_RC(GetNafllDeviceStatusViaMask(nafllDevMask, &nafllStatusParams));

    // POD copy here
    *nafllStatus = nafllStatusParams.devices[nafllDevIndex];
    return rc;
}

RC Avfs::GetNafllDeviceRegimeViaId
(
    NafllId id,
    Perf::RegimeSetting* pRegime
)
{
    MASSERT(pRegime);
    RC rc;

    LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS nafllStatus;
    CHECK_RC(GetNafllDeviceStatusViaId(id, &nafllStatus));
    *pRegime = Perf::Regime2080CtrlBitToRegimeSetting(nafllStatus.lwrrentRegimeId);

    return rc;
}

void Avfs::PrintSingleAdcInfo
(
    Tee::Priority pri,
    Gpu::SplitVoltageDomain voltDom,
    const LW2080_CTRL_CLK_ADC_DEVICE_INFO &adcInfo
)
{
    AdcId id;
    TranslateLw2080AdcIdToAdcId(adcInfo.id, &id);

    Printf(pri, "ADC info:\n");
    Printf(pri, " id = %s\n", PerfUtil::AdcIdToStr(id));
    Printf(pri, " voltDomain = %s\n", PerfUtil::SplitVoltDomainToStr(voltDom));
    Printf(pri, " porOverrideMode = %u\n", adcInfo.porOverrideMode);
    Printf(pri, " nafllsSharedMask = 0x%08x\n", adcInfo.nafllsSharedMask);
    Printf(pri, " bDynCal = %s\n", (adcInfo.bDynCal == LW_TRUE) ? "true" : "false");

    switch (adcInfo.super.type)
    {
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10:
            Printf(pri,
                " type = v10\n"
                " calibration type = v10\n"
                " slope = %u\n"
                " intercept = %u\n",
                adcInfo.data.adcV10.adcCal.slope,
                adcInfo.data.adcV10.adcCal.intercept);
            break;
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20:
            Printf(pri, " type = v20\n");
            switch (adcInfo.data.adcV20.calType)
            {
                case LW2080_CTRL_CLK_ADC_CAL_TYPE_V10:
                    Printf(pri,
                        " calibration type = v10\n"
                        " slope = %u\n"
                        " intercept = %u\n",
                        adcInfo.data.adcV20.adcCal.calV10.slope,
                        adcInfo.data.adcV20.adcCal.calV10.intercept);
                    break;
                case LW2080_CTRL_CLK_ADC_CAL_TYPE_V20:
                    Printf(pri,
                        " calibration type = v20\n"
                        " offset = %u\n"
                        " gain = %u\n"
                        " offsetVfeIdx = %u\n"
                        " coarseControl = %u\n",
                        adcInfo.data.adcV20.adcCal.calV20.offset,
                        adcInfo.data.adcV20.adcCal.calV20.gain,
                        adcInfo.data.adcV20.adcCal.calV20.offsetVfeIdx,
                        adcInfo.data.adcV20.adcCal.calV20.coarseControl);
                    break;
            }
            break;
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
        {
            Printf(pri, " type = v30\n");
            PrintSingleAdcInfoV30(pri, adcInfo.data.adcV30);
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
        {
            const auto& data = adcInfo.data.adcV30IsinkV10.data;
            Printf(pri, " type = v30IsinkV10\n");
            PrintSingleAdcInfoV30(pri, adcInfo.data.adcV30IsinkV10.super);
            Printf(pri, " adcCodeCorrectionOffsetMin = %d\n", data.adcCodeCorrectionOffsetMin);
            Printf(pri, " adcCodeCorrectionOffsetMax = %d\n", data.adcCodeCorrectionOffsetMax);
            Printf(pri, " calIlwalidFuseRevMask = 0x%08x\n", data.calIlwalidFuseRevMask);
            Printf(pri, " lowTempErrIlwalidFuseRevMask = 0x%08x\n", data.lowTempErrIlwalidFuseRevMask); //$
            Printf(pri, " highTempErrIlwalidFuseRevMask = 0x%08x\n", data.highTempErrIlwalidFuseRevMask); //$
            Printf(pri, " adcCalRevFused = %u\n", data.adcCalRevFused);
            Printf(pri, " adcCodeErrHtRevFused = %u\n", data.adcCodeErrHtRevFused);
            Printf(pri, " adcCodeErrLtRevFused = %u\n", data.adcCodeErrLtRevFused);
            Printf(pri, " lowTemp = %d\n", data.lowTemp);
            Printf(pri, " highTemp = %d\n", data.highTemp);
            Printf(pri, " refTemp = %d\n", data.refTemp);
            Printf(pri, " lowVoltuV = %u\n", data.lowVoltuV);
            Printf(pri, " highVoltuV = %u\n", data.highVoltuV);
            Printf(pri, " refVoltuV = %u\n", data.refVoltuV);
            break;
        }
        default:
        {
            MASSERT(!"Unknown ADC type");
            break;
        }
    }
}

void Avfs::PrintSingleAdcInfoV30
(
    Tee::Priority pri,
    const LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_V30 &adcInfo
)
{
    Printf(pri, " offset = %d\n", adcInfo.offset);
    Printf(pri, " gain = %d\n", adcInfo.gain);
    Printf(pri, " coarseOffset = %d\n", adcInfo.coarseOffset);
    Printf(pri, " coarseGain = %d\n", adcInfo.coarseGain);
    Printf(pri, " lowTempLowVoltErr = %d\n", adcInfo.lowTempLowVoltErr);
    Printf(pri, " lowTempHighVoltErr = %d\n", adcInfo.lowTempHighVoltErr);
    Printf(pri, " highTempLowVoltErr = %d\n", adcInfo.highTempLowVoltErr);
    Printf(pri, " highTempHighVoltErr = %d\n", adcInfo.highTempHighVoltErr);
}

void Avfs::PrintSingleAdcStatus
(
    Tee::Priority pri,
    const LW2080_CTRL_CLK_ADC_DEVICE_STATUS &adcStatus
)
{
    Printf(pri, "actualVoltageuV = %u\n", adcStatus.actualVoltageuV);
    Printf(pri, "correctedVoltageuV = %u\n", adcStatus.correctedVoltageuV);
    Printf(pri, "sampledCode = 0x%hhx\n", adcStatus.sampledCode);
    Printf(pri, "overrideCode = 0x%hhx\n", adcStatus.overrideCode);
    Printf(pri, "instCode = 0x%hhx\n", adcStatus.instCode);

    switch (adcStatus.type)
    {
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10:
            break;
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20:
            Printf(pri, "offset = %d\n", adcStatus.statusData.adcV20.offset);
            break;
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
            PrintSingleAdcStatusV30(pri, adcStatus.statusData.adcV30);
            break;
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
            PrintSingleAdcStatusV30(pri, adcStatus.statusData.adcV30IsinkV10.super);
            break;
        default:
            MASSERT(!"Unknown ADC type");
            break;
    }
}

void Avfs::PrintSingleAdcStatusV30
(
    Tee::Priority pri,
    const LW2080_CTRL_CLK_ADC_DEVICE_STATUS_DATA_V30& adcStatus
)
{
    Printf(pri, "adcCodeCorrectionOffset = %d\n", adcStatus.adcCodeCorrectionOffset);
    Printf(pri, "tempCoefficient = %.3f\n", adcStatus.tempCoefficient);
    Printf(pri, "voltTempCoefficient = %.3f\n", adcStatus.voltTempCoefficient);
    Printf(pri, "bRamAssistEngaged = %s\n",
        (adcStatus.bRamAssistEngaged == LW_TRUE) ? "true" : "false");
}

void Avfs::PrintSingleNafllInfo
(
    const LW2080_CTRL_CLK_NAFLL_DEVICE_INFO &nafllInfo
)
{
    constexpr auto pri = Tee::PriDebug;
    Printf(pri, " type = 0x%x\n", nafllInfo.type);
    AdcId adcId;
    Avfs::TranslateLw2080AdcIdToAdcId(nafllInfo.id, &adcId);
    Printf(pri, " id = 0x%x (%s)\n", nafllInfo.id, PerfUtil::AdcIdToStr(adcId));
    Printf(pri, " mdiv = %u\n", nafllInfo.mdiv);
    Printf(pri, " bSkipPldivBelowDvcoMin = %s\n",
        nafllInfo.bSkipPldivBelowDvcoMin == LW_TRUE ? "true" : "false");
    Printf(pri, " vselectMode = 0x%x\n", nafllInfo.vselectMode);
    Printf(pri, " inputRefClkFreqMHz = %u\n", nafllInfo.inputRefClkFreqMHz);
    Printf(pri, " inputRefClkDivVal = %u\n", nafllInfo.inputRefClkDivVal);
    Printf(pri, " clkDomain = 0x%u\n", nafllInfo.clkDomain);
    Printf(pri, " adcIdxLogic = %u\n", nafllInfo.adcIdxLogic);
    Printf(pri, " adcIdxSram = %u\n", nafllInfo.adcIdxSram);
    Printf(pri, " dvcoMinFreqVFEIdx = %u\n", nafllInfo.dvcoMinFreqVFEIdx);
    Printf(pri, " freqCtrlIdx = %u\n", nafllInfo.freqCtrlIdx);
    Printf(pri, " hysteresisThreshold = %u\n", nafllInfo.hysteresisThreshold);
    Printf(pri, " fixedFreqRegimeLimitMHz = %u\n", nafllInfo.fixedFreqRegimeLimitMHz);
    Printf(pri, " bDvco1x = %s\n", nafllInfo.bDvco1x == LW_TRUE ? "true" : "false");
}

void Avfs::PrintSingleNafllStatus
(
    Tee::Priority pri,
    const LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS &nafllStatus
)
{
    // TODO: Maybe refactor "-dump_nafll_lut" and flesh this out?
    Printf(pri,
           "type = %d\n",
           nafllStatus.type);
}

void Avfs::GetAvailableAdcIds(vector<AdcId> *availableAdcIds)
{
    MASSERT(availableAdcIds);
    availableAdcIds->clear();
    for (map<AdcId, UINT32>::const_iterator it = m_adcIdToDevInd.begin();
         it != m_adcIdToDevInd.end();
         ++it)
    {
        availableAdcIds->push_back(it->first);
    }
}

void Avfs::GetAvailableNafllIds(vector<NafllId> *availableNafllIds)
{
    MASSERT(availableNafllIds);
    availableNafllIds->clear();
    for (map<NafllId, NafllInfo>::const_iterator it = m_nafllIdToInfo.begin();
         it != m_nafllIdToInfo.end();
         ++it)
    {
        availableNafllIds->push_back(it->first);
    }
}

RC Avfs::TranslateAdcIdToDevIndex(AdcId id, UINT32 *pAdcIndex)
{
    MASSERT(pAdcIndex);
    RC rc;
    if (m_adcIdToDevInd.find(id) == m_adcIdToDevInd.end())
    {
        Printf(Tee::PriError,
               "Avfs: ADC ID (%d) not present on this chip\n", (UINT32)id);
        return RC::SOFTWARE_ERROR;
    }
    *pAdcIndex = m_adcIdToDevInd[id];
    return rc;
}

Avfs::AdcId Avfs::TranslateDevIndexToAdcId(UINT32 devIdx) const
{
    for (const auto& entry : m_adcIdToDevInd)
    {
        if (entry.second == devIdx)
            return entry.first;
    }
    return AdcId_NUM;
}

RC Avfs::TranslateNafllIdToDevIndex(NafllId id, UINT32 *pNafllIndex)
{
    MASSERT(pNafllIndex);
    if (m_nafllIdToInfo.find(id) == m_nafllIdToInfo.end())
    {
        Printf(Tee::PriError, "No NAFLL index (%d) found\n", id);
        return RC::SOFTWARE_ERROR;
    }
    *pNafllIndex = m_nafllIdToInfo[id].devIndex;
    return OK;
}

RC Avfs::TranslateNafllIdToClkDomain(NafllId id, Gpu::ClkDomain *dom)
{
    MASSERT(dom);
    if (m_nafllIdToInfo.find(id) == m_nafllIdToInfo.end())
    {
        Printf(Tee::PriError, "No NAFLL index (%d) found\n", id);
        return RC::SOFTWARE_ERROR;
    }
    *dom = m_nafllIdToInfo[id].clkDomain;
    return OK;
}

bool Avfs::IsNafllClkDomain(Gpu::ClkDomain dom) const
{
    return m_clkDomainToNafllIdSet.find(dom) != m_clkDomainToNafllIdSet.end();
}

void Avfs::GetNafllClkDomains(vector<Gpu::ClkDomain>* doms) const
{
    for (map<Gpu::ClkDomain, set<NafllId> >::const_iterator nafllDomItr =
            m_clkDomainToNafllIdSet.begin();
         nafllDomItr != m_clkDomainToNafllIdSet.end();
         ++nafllDomItr)
    {
        doms->push_back(nafllDomItr->first);
    }
}

RC Avfs::GetAdcCalibrationStateViaMask
(
    UINT32 adcDevMask,
    bool bUseDefault,
    vector<Avfs::AdcCalibrationState> *adcCalStates
)
{
    RC rc;

    MASSERT(adcDevMask);
    MASSERT(IS_MASK_SUBSET(adcDevMask, GetAdcDevMask()));
    MASSERT(adcCalStates);

    adcCalStates->clear();
    adcCalStates->reserve(Utility::CountBits(adcDevMask));
    LW2080_CTRL_CLK_ADC_DEVICES_CONTROL_PARAMS adcCtrlParams = {};
    adcCtrlParams.super.objMask.super.pData[0] = adcDevMask;
    adcCtrlParams.bDefault = bUseDefault ? LW_TRUE : LW_FALSE;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_ADC_DEVICES_GET_CONTROL
       ,&adcCtrlParams
       ,sizeof(adcCtrlParams)));

    for (UINT32 i = 0; i < LW2080_CTRL_CLK_ADC_MAX_DEVICES; ++i)
    {
        UINT32 mask = 1 << i;
        if (!(mask & adcDevMask))
        {
            continue;
        }

        AdcCalibrationState calState;

        switch (adcCtrlParams.devices[i].super.type)
        {
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10:
                calState.adcCalType = LW2080_CTRL_CLK_ADC_CAL_TYPE_V10;
                calState.v10.slope = adcCtrlParams.devices[i].data.adcV10.adcCal.slope;
                calState.v10.intercept = adcCtrlParams.devices[i].data.adcV10.adcCal.intercept;
                break;
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20:
                // Volta supports both types of calibrations i.e. slope/intercept or gain/offset
                switch (adcCtrlParams.devices[i].data.adcV20.calType)
                {
                    case LW2080_CTRL_CLK_ADC_CAL_TYPE_V10:
                        calState.adcCalType = LW2080_CTRL_CLK_ADC_CAL_TYPE_V10;
                        calState.v10.slope = adcCtrlParams.devices[i].data.adcV20.adcCal.calV10.slope;
                        calState.v10.intercept = adcCtrlParams.devices[i].data.adcV20.adcCal.calV10.intercept;
                        break;
                    case LW2080_CTRL_CLK_ADC_CAL_TYPE_V20:
                        calState.adcCalType = LW2080_CTRL_CLK_ADC_CAL_TYPE_V20;
                        calState.v20.offset = adcCtrlParams.devices[i].data.adcV20.adcCal.calV20.offset;
                        calState.v20.gain = adcCtrlParams.devices[i].data.adcV20.adcCal.calV20.gain;
                        break;
                    default:
                        Printf(Tee::PriError, "Invalid ADC calibration type\n");
                        return RC::AVFS_ILWALID_ADC_CAL_TYPE;
                }
                break;
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
                calState.adcCalType = Avfs::MODS_SHIM_ADC_CAL_TYPE_V30;
                calState.v30 = adcCtrlParams.devices[i].data.adcV30;
                break;
            default:
                Printf(Tee::PriError, "Invalid ADC type\n");
                return RC::AVFS_ILWALID_ADC_TYPE;
        }
        adcCalStates->push_back(calState);
    }
    return rc;
}

RC Avfs::GetOriginalAdcCalibrationState
(
    vector<Avfs::AdcCalibrationState> *adcCalStates
)
{
    return GetAdcCalibrationStateViaMask(m_AdcDevMask, true, adcCalStates);
}

RC Avfs::GetLwrrentAdcCalibrationState
(
    vector<Avfs::AdcCalibrationState> *adcCalStates
)
{
    return GetAdcCalibrationStateViaMask(m_AdcDevMask, false, adcCalStates);
}

RC Avfs::GetAdcCalibrationStateViaId
(
    AdcId id,
    bool bUseDefault,
    Avfs::AdcCalibrationState *adcCalState
)
{
    RC rc;
    UINT32 adcDevInd;
    CHECK_RC(TranslateAdcIdToDevIndex(id, &adcDevInd));

    const UINT32 adcDevMask = 1 << adcDevInd;

    vector<Avfs::AdcCalibrationState> adcDevices;
    CHECK_RC(GetAdcCalibrationStateViaMask(
        adcDevMask, bUseDefault, &adcDevices));
    *adcCalState = adcDevices[0];

    return rc;
}

RC Avfs::SetAdcCalibrationStateViaMask
(
    UINT32 adcDevMask,
    const vector<Avfs::AdcCalibrationState> &adcCalStates
)
{
    RC rc;
    MASSERT(adcDevMask);
    if (!IS_MASK_SUBSET(adcDevMask, GetAdcDevMask()))
    {
        Printf(Tee::PriError,
               "Avfs: Invalid dev mask (0x%x), must be a subset "
               "of the devices defined by the VBIOS (0x%x)\n",
               adcDevMask, GetAdcDevMask());
        return RC::SOFTWARE_ERROR;
    }

    if ((UINT32)Utility::CountBits(adcDevMask) != adcCalStates.size())
    {
        Printf(Tee::PriError,
               "Avfs: ADC dev mask has incorrect number of bits set\n"
               "bits set/value = %d (0x%x), calibration array size = %u\n",
               Utility::CountBits(adcDevMask), adcDevMask,
               (UINT32)adcCalStates.size());
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_CLK_ADC_DEVICES_CONTROL_PARAMS adcCtrlParams = {};
    adcCtrlParams.super.objMask.super.pData[0] = adcDevMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_ADC_DEVICES_GET_CONTROL
       ,&adcCtrlParams
       ,sizeof(adcCtrlParams)));

    for (vector<Avfs::AdcCalibrationState>::const_iterator it = adcCalStates.begin();
         it != adcCalStates.end();
         ++it)
    {
        UINT32 lowestBitInd = static_cast<UINT32>(Utility::BitScanForward(adcDevMask));

        if (lowestBitInd >= LW2080_CTRL_CLK_ADC_MAX_DEVICES)
            continue;

        LW2080_CTRL_CLK_ADC_DEVICE_CONTROL &devCtrl = adcCtrlParams.devices[lowestBitInd];

        Printf(Tee::PriDebug, "Setting ADC ind = %d with calibration: ", lowestBitInd);

        switch (devCtrl.super.type)
        {
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10:
                if (it->adcCalType != LW2080_CTRL_CLK_ADC_CAL_TYPE_V10)
                {
                    Printf(Tee::PriError,
                        "Invalid calibration data passed for device %u\n",
                        lowestBitInd);
                    return RC::AVFS_ILWALID_ADC_CAL_TYPE;
                }
                devCtrl.data.adcV10.adcCal.slope = it->v10.slope;
                devCtrl.data.adcV10.adcCal.intercept = it->v10.intercept;

                Printf(Tee::PriDebug,
                    "slope = %d and intercept = %d\n",
                    it->v10.slope, it->v10.intercept);
                break;
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20:
            {
                if (it->adcCalType != devCtrl.data.adcV20.calType)
                {
                    Printf(Tee::PriError,
                        "Invalid calibration data passed for device %u\n",
                        lowestBitInd);
                    return RC::AVFS_ILWALID_ADC_CAL_TYPE;
                }
                switch (devCtrl.data.adcV20.calType)
                {
                    case LW2080_CTRL_CLK_ADC_CAL_TYPE_V10:
                        devCtrl.data.adcV20.adcCal.calV10.slope = it->v10.slope;
                        devCtrl.data.adcV20.adcCal.calV10.intercept = it->v10.intercept;

                        Printf(Tee::PriDebug,
                            "slope = %d and intercept = %d\n",
                            it->v10.slope, it->v10.intercept);
                        break;
                    case LW2080_CTRL_CLK_ADC_CAL_TYPE_V20:
                        devCtrl.data.adcV20.adcCal.calV20.offset = it->v20.offset;
                        devCtrl.data.adcV20.adcCal.calV20.gain = it->v20.gain;

                        Printf(Tee::PriDebug,
                            "offset = %d and gain = %d\n",
                            it->v20.offset, it->v20.gain);
                        break;
                    default:
                        Printf(Tee::PriError, "Invalid ADC calibration type\n");
                        return RC::AVFS_ILWALID_ADC_CAL_TYPE;
                }
                break;
            }
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
            case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
                devCtrl.data.adcV30 = it->v30;
                break;
            default:
                Printf(Tee::PriError, "Invalid ADC type\n");
                return RC::AVFS_ILWALID_ADC_TYPE;
        }
        adcDevMask &= adcDevMask - 1;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_ADC_DEVICES_SET_CONTROL
       ,&adcCtrlParams
       ,sizeof(adcCtrlParams)));

    m_CalibrationStateHasChanged = true;

    return rc;
}

RC Avfs::SetAdcCalibrationStateViaId
(
    AdcId id,
    const Avfs::AdcCalibrationState &adcCalState
)
{
    RC rc;
    UINT32 adcDevInd;
    CHECK_RC(TranslateAdcIdToDevIndex(id, &adcDevInd));

    UINT32 adcDevMask = 1 << adcDevInd;

    vector<Avfs::AdcCalibrationState> calStates;
    calStates.push_back(adcCalState);
    CHECK_RC(SetAdcCalibrationStateViaMask(adcDevMask, calStates));
    return rc;
}

map<UINT32, Avfs::AdcId> Avfs::CreateLw2080AdcIdToAdcIdMap()
{
    map<UINT32, AdcId> translationMap;
    translationMap[LW2080_CTRL_CLK_ADC_ID_SYS]   =     ADC_SYS;
    translationMap[LW2080_CTRL_CLK_ADC_ID_LTC]   =     ADC_LTC;
    translationMap[LW2080_CTRL_CLK_ADC_ID_XBAR]  =     ADC_XBAR;
    translationMap[LW2080_CTRL_CLK_ADC_ID_LWD]   =     ADC_LWD;
    translationMap[LW2080_CTRL_CLK_ADC_ID_HOST]  =     ADC_HOST;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC0]  =     ADC_GPC0;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC1]  =     ADC_GPC1;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC2]  =     ADC_GPC2;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC3]  =     ADC_GPC3;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC4]  =     ADC_GPC4;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC5]  =     ADC_GPC5;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC6]  =     ADC_GPC6;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC7]  =     ADC_GPC7;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC8]  =     ADC_GPC8;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC9]  =     ADC_GPC9;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC10] =     ADC_GPC10;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPC11] =     ADC_GPC11;
    translationMap[LW2080_CTRL_CLK_ADC_ID_GPCS]  =     ADC_GPCS;
    translationMap[LW2080_CTRL_CLK_ADC_ID_SRAM]  =     ADC_SRAM;
    translationMap[LW2080_CTRL_CLK_ADC_ID_SYS_ISINK] = ADC_SYS_ISINK;
    return translationMap;
}

map<UINT32, Avfs::NafllId> Avfs::CreateLw2080NafllIdToNafllIdMap()
{
    map<UINT32, NafllId> translationMap;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_SYS]   = NAFLL_SYS;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_LTC]   = NAFLL_LTC;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_XBAR]  = NAFLL_XBAR;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_LWD]   = NAFLL_LWD;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_HOST]  = NAFLL_HOST;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC0]  = NAFLL_GPC0;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC1]  = NAFLL_GPC1;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC2]  = NAFLL_GPC2;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC3]  = NAFLL_GPC3;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC4]  = NAFLL_GPC4;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC5]  = NAFLL_GPC5;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC6]  = NAFLL_GPC6;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC7]  = NAFLL_GPC7;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC8]  = NAFLL_GPC8;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC9]  = NAFLL_GPC9;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC10] = NAFLL_GPC10;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPC11] = NAFLL_GPC11;
    translationMap[LW2080_CTRL_CLK_NAFLL_ID_GPCS]  = NAFLL_GPCS;
    return translationMap;
}

/* static */ const map<UINT32, Avfs::AdcId> Avfs::s_lw2080AdcIdToAdcIdMap(
    Avfs::CreateLw2080AdcIdToAdcIdMap());

/* static */ const map<UINT32, Avfs::NafllId> Avfs::s_lw2080NafllIdToNafllIdMap(
    Avfs::CreateLw2080NafllIdToNafllIdMap());

RC Avfs::TranslateLw2080AdcIdToAdcId(UINT32 lw2080AdcId, AdcId *pId)
{
    MASSERT(pId);
    RC rc;
    map<UINT32, Avfs::AdcId>::const_iterator it = s_lw2080AdcIdToAdcIdMap.find(lw2080AdcId);
    if (s_lw2080AdcIdToAdcIdMap.end() == it)
    {
        Printf(Tee::PriError,
               "Avfs: Invalid LW2080_CTRL_CLK_ADC_ID: 0x%x\n",
               (UINT32)lw2080AdcId);
        return RC::SOFTWARE_ERROR;
    }
    *pId = it->second;
    return rc;
}

RC Avfs::TranslateLw2080NafllIdToNafllId(UINT32 lw2080NafllId, Avfs::NafllId *pId)
{
    MASSERT(pId);
    map<UINT32, Avfs::NafllId>::const_iterator it = s_lw2080NafllIdToNafllIdMap.find(lw2080NafllId);
    if (s_lw2080NafllIdToNafllIdMap.end() == it)
    {
        Printf(Tee::PriError, "Could not find NAFLL id = 0x%x\n", lw2080NafllId);
        return RC::SOFTWARE_ERROR;
    }
    *pId = it->second;
    return OK;
}

RC Avfs::SetFreqControllerEnable(Gpu::ClkDomain dom, bool bEnable) const
{
    const vector<Gpu::ClkDomain> clkDoms = { dom };
    return SetFreqControllerEnable(clkDoms, bEnable);
}

RC Avfs::SetFreqControllerEnable(bool bEnable) const
{
    RC rc;

    vector<Gpu::ClkDomain> clfcClkDoms;
    CHECK_RC(GetEnabledFreqControllers(&clfcClkDoms));
    CHECK_RC(SetFreqControllerEnable(clfcClkDoms, bEnable));

    return rc;
}

RC Avfs::SetFreqControllerEnable(const vector<Gpu::ClkDomain>& clkDoms, bool bEnable) const
{
    if (clkDoms.empty())
        return OK;

    RC rc;

    LW2080_CTRL_CLK_CLK_FREQ_CONTROLLERS_CONTROL_PARAMS freqCtrlParams;
    CHECK_RC(FreqControllerGetControl(clkDoms, &freqCtrlParams));

    // Make sure each every frequency controller for a domain has the same
    // enable/disable value
    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &freqCtrlParams.super.objMask.super)
        freqCtrlParams.freqControllers[ii].bDisable = !bEnable;
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
             m_pSubdevice
            ,LW2080_CTRL_CMD_CLK_CLK_FREQ_CONTROLLERS_SET_CONTROL
            ,&freqCtrlParams
            ,sizeof(freqCtrlParams)));

    return rc;
}

RC Avfs::IsFreqControllerEnabled(Gpu::ClkDomain dom, bool* pbEnable) const
{
    RC rc;

    *pbEnable = false;
    LW2080_CTRL_CLK_CLK_FREQ_CONTROLLERS_CONTROL_PARAMS freqCtrlParams;
    rc = FreqControllerGetControl(dom, &freqCtrlParams);
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        return OK;
    }
    CHECK_RC(rc);

    // If any frequency controller is enabled, mark the entire domain as enabled
    // as per CS request
    UINT32 ii = 0;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &freqCtrlParams.super.objMask.super)
        if (!freqCtrlParams.freqControllers[ii].bDisable)
        {
            *pbEnable = true;
            return rc;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Avfs::GetEnabledFreqControllers(vector<Gpu::ClkDomain>* pClkDoms) const
{
    MASSERT(pClkDoms);
    pClkDoms->clear();

    RC rc;

    vector<Gpu::ClkDomain> clkDoms;
    GetNafllClkDomains(&clkDoms);

    // Common case: All NAFLL domains will support CLFC
    pClkDoms->reserve(clkDoms.size());

    for (const auto clkDom : clkDoms)
    {
        bool bEnable;
        CHECK_RC(IsFreqControllerEnabled(clkDom, &bEnable));
        if (bEnable)
        {
            pClkDoms->push_back(clkDom);
        }
    }

    return rc;
}

RC Avfs::FreqControllerGetControl
(
    Gpu::ClkDomain dom,
    LW2080_CTRL_CLK_CLK_FREQ_CONTROLLERS_CONTROL_PARAMS* pCtrlParams
) const
{
    const vector<Gpu::ClkDomain> clkDoms = { dom };
    return FreqControllerGetControl(clkDoms, pCtrlParams);
}

RC Avfs::FreqControllerGetControl
(
    const vector<Gpu::ClkDomain>& clkDoms,
    LW2080_CTRL_CLK_CLK_FREQ_CONTROLLERS_CONTROL_PARAMS* pCtrlParams
) const
{
    RC rc;

    LW2080_CTRL_CLK_CLK_FREQ_CONTROLLERS_GET_INFO_PARAMS freqCtrlInfoParams;
    rc = LwRmPtr()->ControlBySubdevice(
             m_pSubdevice
            ,LW2080_CTRL_CMD_CLK_CLK_FREQ_CONTROLLERS_GET_INFO
            ,&freqCtrlInfoParams
            ,sizeof(freqCtrlInfoParams));

    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        return OK;
    }
    UINT32 controlMask = 0;
    for (const auto dom : clkDoms)
    {
        UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
        UINT32 domControlMask = 0;
        INT32 bitIndex = 0;
        UINT32 infoMask = freqCtrlInfoParams.super.objMask.super.pData[0];

        while ((bitIndex = Utility::BitScanForward(infoMask)) != -1)
        {
            if (freqCtrlInfoParams.freqControllers[bitIndex].clkDomain == rmDomain)
            {
                domControlMask |= BIT(bitIndex);
            }
            infoMask ^= BIT(bitIndex);
        }

        if (!domControlMask)
        {
            Printf(Tee::PriLow,
                   "No frequency controller(s) for %s domain\n",
                   PerfUtil::ClkDomainToStr(dom));
            return RC::LWRM_NOT_SUPPORTED;
        }
        controlMask |= domControlMask;
    }

    pCtrlParams->super.objMask.super.pData[0] = controlMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
             m_pSubdevice
            ,LW2080_CTRL_CMD_CLK_CLK_FREQ_CONTROLLERS_GET_CONTROL
            ,pCtrlParams
            ,sizeof(*pCtrlParams)));

    return rc;
}

RC Avfs::GetEnabledVoltControllers(set<Gpu::SplitVoltageDomain>* pVoltDoms) const
{
    MASSERT(pVoltDoms);
    pVoltDoms->clear();

    RC rc;

    // CLVC is not supported or it is disabled on all voltage domains
    if (rc != RC::OK || !m_ClvcInfo.super.objMask.super.pData[0])
    {
        return RC::OK;
    }

    Volt3x* pVolt3x = m_pSubdevice->GetVolt3x();
    CHECK_RC(pVolt3x->Initialize());

    LW2080_CTRL_CLK_CLK_VOLT_CONTROLLERS_CONTROL_PARAMS ctrlParams = {};
    CHECK_RC(VoltControllerGetControl({}, &ctrlParams));

    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &ctrlParams.super.objMask.super)
        if (ctrlParams.voltControllers[ii].voltOffsetMaxuV != 0 && ctrlParams.voltControllers[ii].voltOffsetMinuV != 0)
        {
            Gpu::SplitVoltageDomain voltDom = pVolt3x->RailIdxToGpuSplitVoltageDomain(
                m_ClvcInfo.voltControllers[ii].voltRailIdx);
            pVoltDoms->insert(voltDom);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    return rc;
}

RC Avfs::SetVoltControllerEnable
(
    const set<Gpu::SplitVoltageDomain>& voltDoms,
    bool bEnable
) const
{
    if (voltDoms.empty())
    {
        return RC::OK;
    }

    RC rc;

    LW2080_CTRL_CLK_CLK_VOLT_CONTROLLERS_CONTROL_PARAMS ctrlParams = {};
    CHECK_RC(VoltControllerGetControl(voltDoms, &ctrlParams));

    UINT32 ii;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &ctrlParams.super.objMask.super)
        ctrlParams.voltControllers[ii].voltOffsetMaxuV = bEnable ? m_ClvcInfo.voltControllers[ii].voltOffsetMaxuV : 0;
        ctrlParams.voltControllers[ii].voltOffsetMinuV = bEnable ? m_ClvcInfo.voltControllers[ii].voltOffsetMinuV : 0;
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
             m_pSubdevice
            ,LW2080_CTRL_CMD_CLK_CLK_VOLT_CONTROLLERS_SET_CONTROL
            ,&ctrlParams
            ,sizeof(ctrlParams)));

    return rc;
}

RC Avfs::VoltControllerGetControl
(
    const set<Gpu::SplitVoltageDomain>& voltDoms,
    LW2080_CTRL_CLK_CLK_VOLT_CONTROLLERS_CONTROL_PARAMS* pCtrlParams
) const
{
    RC rc;

    // CLVC is not supported or it is disabled on all voltage domains
    if (rc != RC::OK || !m_ClvcInfo.super.objMask.super.pData[0])
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 ctrlMask = voltDoms.empty() ? m_ClvcInfo.super.objMask.super.pData[0] : 0;
    for (const auto voltDom : voltDoms)
    {
        const UINT32 railIdx = m_pSubdevice->GetVolt3x()->GpuSplitVoltageDomainToRailIdx(voltDom);
        if (railIdx == LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS)
        {
            return RC::ILWALID_VOLTAGE_DOMAIN;
        }
        UINT32 infoMask = m_ClvcInfo.super.objMask.super.pData[0];

        INT32 bitIndex = 0;
        while ((bitIndex = Utility::BitScanForward(infoMask)) != -1)
        {
            const UINT32 bitMask = BIT(bitIndex);
            if (m_ClvcInfo.voltControllers[bitIndex].voltRailIdx == railIdx)
            {
                ctrlMask |= bitMask;
            }
            infoMask ^= bitMask;
        }
    }

    pCtrlParams->super.objMask.super.pData[0] = ctrlMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
             m_pSubdevice
            ,LW2080_CTRL_CMD_CLK_CLK_VOLT_CONTROLLERS_GET_CONTROL
            ,pCtrlParams
            ,sizeof(*pCtrlParams)));

    return rc;
}

RC Avfs::SetRegime(Gpu::ClkDomain dom, Perf::RegimeSetting regime) const
{
    RC rc;

    if (!IsNafllClkDomain(dom))
    {
        Printf(Tee::PriError,
               "Cannot set VR_ABOVE_NOISE_UNAWARE_VMIN on non-NAFLL domain %s\n",
               PerfUtil::ClkDomainToStr(dom));
        return RC::ILWALID_CLOCK_DOMAIN;
    }

    LW2080_CTRL_CLK_NAFLL_DEVICES_INFO_PARAMS nafllInfoParams = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_INFO
       ,&nafllInfoParams
       ,sizeof(nafllInfoParams)));

    UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(dom);
    UINT32 rmRegime = PerfUtil::RegimeSettingToCtrl2080Bit(regime);
    UINT32 nafllMask = nafllInfoParams.super.objMask.super.pData[0];
    UINT32 controlMask = 0;
    INT32 bitIndex = 0;
    while ((bitIndex = Utility::BitScanForward(nafllMask)) != -1)
    {
        if (nafllInfoParams.devices[bitIndex].clkDomain == rmDomain)
        {
            controlMask |= BIT(bitIndex);
        }
        nafllMask ^= BIT(bitIndex);
    }

    LW2080_CTRL_CLK_NAFLL_DEVICES_CONTROL_PARAMS nafllControlParams = {};
    nafllControlParams.super.objMask.super.pData[0] = controlMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_CONTROL
       ,&nafllControlParams
       ,sizeof(nafllControlParams)));

    while((bitIndex = Utility::BitScanForward(controlMask)) != -1)
    {
        nafllControlParams.devices[bitIndex].targetRegimeIdOverride = rmRegime;
        controlMask ^= BIT(bitIndex);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_SET_CONTROL
       ,&nafllControlParams
       ,sizeof(nafllControlParams)));

    return rc;
}

RC Avfs::SetQuickSlowdown(UINT32 vfLwrveIdx, UINT32 gpcSWMask, bool enable)
{
    RC rc;
    RegHal& regs = m_pSubdevice->Regs();
    constexpr UINT32 ilwalidGPCMask = 0xFFFF'FFFF;

    if (vfLwrveIdx >= LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_MAX)
    {
        Printf(Tee::PriError, "VF lwrve Idx = {%d} is greater than the max supported VF lwrve Idx = %d \n",
            vfLwrveIdx, LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_MAX);
        return RC::BAD_PARAMETER;
    }

    // If slowdown isn't supported, fail
    if (!regs.IsSupported(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL))
    {
        Printf(Tee::PriError,
               "quick_slowdown module is not present on this GPU!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    const UINT32 maxGpcCount = m_pSubdevice->GetMaxGpcCount();
    const UINT32 gpcMask     = m_pSubdevice->GetFsImpl()->GpcMask();
    for (UINT32 hwGpc = 0; hwGpc < maxGpcCount; ++hwGpc)
    {
        if (((1 << hwGpc) & gpcMask & (gpcSWMask)) == 0)
        {
            continue;
        }

        // If a valid GPC doesn't have slowdown enabled, fail
        if (!regs.Test32(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_ENABLE_YES,
                         hwGpc))
        {
            Printf(Tee::PriError,
                   "quick_slowdown module is not enabled on phys GPC %d!\n",
                   hwGpc);
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    LW2080_CTRL_CLK_NAFLL_DEVICES_CONTROL_PARAMS nafllControl = {};
    // Assuming that the Maximum NAFLL Device ID is not 31 because
    // the ilwalidGPCMask would not work and we would need to use a different one
    // RM lwrrently states that the maximum NAFLL Devices are 32, so the ilwalidGPCMask
    // is set as 0xFFFF'FFFF
    static_assert(LW2080_CTRL_CLK_NAFLL_ID_MAX < 31, "LW2080_CTRL_CLK_NAFLL_ID_MAX has to be lesser than 31");
    const UINT32 requestedMask = (gpcSWMask == ilwalidGPCMask) 
                                 ? m_NafllDevMask : ((gpcSWMask & gpcMask) << NAFLL_GPC0);
    nafllControl.super.objMask.super.pData[0] = requestedMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_CONTROL
       ,&nafllControl
       ,sizeof(nafllControl)));

    INT32 bitIndex = -1;
    while ((bitIndex = Utility::BitScanForward(requestedMask)) != -1)
    {
        nafllControl.devices[bitIndex].data.v30.bQuickSlowdownForceEngage[vfLwrveIdx] = enable;
        gpcSWMask ^= BIT(bitIndex);
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_SET_CONTROL
       ,&nafllControl
       ,sizeof(nafllControl)));

    return rc;
}

RC Avfs::SetRegime(const RegimeOverrides& overrides)
{
    RC rc;

    for (const auto& ov : overrides)
    {
        if (!IsNafllClkDomain(ov.ClkDomain))
        {
            Printf(Tee::PriError, "Cannot override regime for non-NAFLL clock domain %s\n",
                   PerfUtil::ClkDomainToStr(ov.ClkDomain));
            return RC::ILWALID_CLOCK_DOMAIN;
        }
    }

    LW2080_CTRL_CLK_NAFLL_DEVICES_INFO_PARAMS nafllInfo = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_INFO
       ,&nafllInfo
       ,sizeof(nafllInfo)));

    // Create the BOARDOBJGRP mask for the GET_CONTROL call
    UINT32 controlMask = 0;
    for (const auto& ov : overrides)
    {
        const UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(ov.ClkDomain);
        INT32 bitIndex = 0;
        UINT32 nafllMask = nafllInfo.super.objMask.super.pData[0];
        while ((bitIndex = Utility::BitScanForward(nafllMask)) != -1)
        {
            if (nafllInfo.devices[bitIndex].clkDomain == rmDomain)
            {
                controlMask |= BIT(bitIndex);
            }
            nafllMask ^= BIT(bitIndex);
        }
    }

    LW2080_CTRL_CLK_NAFLL_DEVICES_CONTROL_PARAMS nafllControl = {};
    nafllControl.super.objMask.super.pData[0] = controlMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_CONTROL
       ,&nafllControl
       ,sizeof(nafllControl)));

    for (const auto& ov : overrides)
    {
        const UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(ov.ClkDomain);
        const UINT32 rmRegime = PerfUtil::RegimeSettingToCtrl2080Bit(ov.Regime);
        INT32 bitIndex = 0;
        UINT32 nafllMask = nafllControl.super.objMask.super.pData[0];
        while ((bitIndex = Utility::BitScanForward(nafllMask)) != -1)
        {
            if (nafllInfo.devices[bitIndex].clkDomain == rmDomain)
            {
                nafllControl.devices[bitIndex].targetRegimeIdOverride = rmRegime;
            }
            nafllMask ^= BIT(bitIndex);
        }
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_SET_CONTROL
       ,&nafllControl
       ,sizeof(nafllControl)));

    return rc;
}

Avfs::NafllId Avfs::ClkDomToNafllId(Gpu::ClkDomain dom) const
{
    map<Gpu::ClkDomain, set<NafllId>>::const_iterator domItr = m_clkDomainToNafllIdSet.find(dom);
    if (domItr == m_clkDomainToNafllIdSet.end())
    {
        Printf(Tee::PriError, "%s: Invalid clock domain\n", __FUNCTION__);
        return NafllId_NUM;
    }
    return *(domItr->second.begin());
}

RC Avfs::GetAdcInfoPrintPri(UINT32* pri)
{
    *pri = static_cast<UINT32>(m_AdcInfoPrintPri);
    return OK;
}

RC Avfs::SetAdcInfoPrintPri(UINT32 pri)
{
    m_AdcInfoPrintPri = static_cast<Tee::Priority>(pri);
    return OK;
}

UINT32 Avfs::GetAdcDevMask(Gpu::SplitVoltageDomain voltDom) const
{
    const auto& entry = m_VoltDomToAdcDevMask.find(voltDom);
    if (entry != m_VoltDomToAdcDevMask.end())
    {
        return entry->second;
    }

    return 0;
}

RC Avfs::DisableClvc()
{
    return SetVoltControllerEnable(m_ClvcVoltDoms, false);
}

RC Avfs::EnableClvc()
{
    return SetVoltControllerEnable(m_ClvcVoltDoms, true);
}

