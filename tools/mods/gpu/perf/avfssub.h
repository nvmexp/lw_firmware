/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017, 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_AVFS_H
#define INCLUDED_AVFS_H

#include <map>
#include <set>
#include <vector>

#include "core/include/gpu.h"
#include "gpu/perf/perfsub.h"
#include "core/include/types.h"
#include "core/include/setget.h"

class GpuSubdevice;
class RC;

//! Class which provides APIs to get info, status or control to many
//! of the hardware elements ilwolved in AVFS
//! Relies heavily on RMCTRL calls found within ctrl2080clkavfs.h
//! For more background on AVFS (including overview, terms and
//! definitions) see the below design guide:
//! //sw/docs/resman/components/avfs/AVFS_Software_Design_Guide.docx
class Avfs
{
public:
    Avfs(GpuSubdevice *pSubdevice);
    ~Avfs();

    RC Initialize();
    RC Cleanup();

    enum AdcId {
        ADC_SYS
       ,ADC_LTC
       ,ADC_XBAR
       ,ADC_LWD
       ,ADC_HOST
       ,ADC_GPC0
       ,ADC_GPC1
       ,ADC_GPC2
       ,ADC_GPC3
       ,ADC_GPC4
       ,ADC_GPC5
       ,ADC_GPC6
       ,ADC_GPC7
       ,ADC_GPC8
       ,ADC_GPC9
       ,ADC_GPC10
       ,ADC_GPC11
       ,ADC_GPCS
       ,ADC_SRAM
       ,ADC_SYS_ISINK
       ,AdcId_NUM
    };

    enum NafllId {
         NAFLL_SYS
        ,NAFLL_LTC
        ,NAFLL_XBAR
        ,NAFLL_LWD
        ,NAFLL_HOST
        ,NAFLL_GPC0
        ,NAFLL_GPC1
        ,NAFLL_GPC2
        ,NAFLL_GPC3
        ,NAFLL_GPC4
        ,NAFLL_GPC5
        ,NAFLL_GPC6
        ,NAFLL_GPC7
        ,NAFLL_GPC8
        ,NAFLL_GPC9
        ,NAFLL_GPC10
        ,NAFLL_GPC11
        ,NAFLL_GPCS
        ,NafllId_NUM
    };

    RC GetAdcDeviceStatusViaMask
    (
        UINT32 adcDevMask,
        LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS *adcStatusParams
    );
    //! Call GetAdcDeviceStatus with m_AdcDevMask as its argument
    RC GetAdcDeviceStatus
    (
        LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS *adcStatusParams
    );

    //! Given a single AdcId, get the status for that id
    RC GetAdcDeviceStatusViaId
    (
        AdcId id,
        LW2080_CTRL_CLK_ADC_DEVICE_STATUS *adcStatus
    );

    //! Retrieve actual and corrected voltage from the specified ADC
    RC GetAdcVoltageViaId
    (
        AdcId id,
        UINT32 *actualVoltageuV,
        UINT32 *correctedVoltageuV
    );

    //! Get the status for the NAFLLs specified by nafllDevMask
    RC GetNafllDeviceStatusViaMask
    (
        UINT32 nafllDevMask,
        LW2080_CTRL_CLK_NAFLL_DEVICES_STATUS_PARAMS *nafllStatusParams
    );

    //! Get the status for all NAFLLs
    RC GetNafllDeviceStatus
    (
        LW2080_CTRL_CLK_NAFLL_DEVICES_STATUS_PARAMS *nafllStatusParams
    );

    //! Get the status information for the NAFLL specified by id
    RC GetNafllDeviceStatusViaId
    (
        NafllId id,
        LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS *nafllStatus
    );

    RC GetNafllDeviceRegimeViaId
    (
        NafllId id,
        Perf::RegimeSetting* pRegime
    );

    //! Prints information in LW2080_CTRL_CLK_ADC_DEVICE_INFO
    static void PrintSingleAdcInfo
    (
        Tee::Priority pri,
        Gpu::SplitVoltageDomain voltDom,
        const LW2080_CTRL_CLK_ADC_DEVICE_INFO &adcInfo
    );

    //! Prints LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_V30
    static void PrintSingleAdcInfoV30
    (
        Tee::Priority pri,
        const LW2080_CTRL_CLK_ADC_DEVICE_INFO_DATA_V30 &adcInfo
    );

    //! Prints the information in LW2080_CTRL_CLK_NAFLL_DEVICE_INFO
    static void PrintSingleNafllInfo
    (
        const LW2080_CTRL_CLK_NAFLL_DEVICE_INFO &nafllInfo
    );

    //! Prints information in LW2080_CTRL_CLK_ADC_DEVICE_STATUS
    //! More information will be added as the structure is filled out
    static void PrintSingleAdcStatus
    (
        Tee::Priority pri,
        const LW2080_CTRL_CLK_ADC_DEVICE_STATUS &adcStatus
    );

    //! Prints LW2080_CTRL_CLK_ADC_DEVICE_STATUS_DATA_V30
    static void PrintSingleAdcStatusV30
    (
        Tee::Priority pri,
        const LW2080_CTRL_CLK_ADC_DEVICE_STATUS_DATA_V30& adcStatus
    );

    static void PrintSingleNafllStatus
    (
        Tee::Priority pri,
        const LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS &nafllStatus
    );

    void GetAvailableAdcIds(vector<AdcId> *availableAdcIds);
    void GetAvailableNafllIds(vector<NafllId> *availableNafllIds);

    RC TranslateAdcIdToDevIndex(AdcId id, UINT32 *adcIndex);
    AdcId TranslateDevIndexToAdcId(UINT32 devIdx) const;
    RC TranslateNafllIdToDevIndex(NafllId id, UINT32 *pNafllIndex);
    RC TranslateNafllIdToClkDomain(NafllId id, Gpu::ClkDomain *dom);

    //! Return true iff input clock domain is an NAFLL domain
    bool IsNafllClkDomain(Gpu::ClkDomain dom) const;

    //! Return all active NAFLL domains on this GpuSubdevice
    void GetNafllClkDomains(vector<Gpu::ClkDomain>* doms) const;

    //! \brief Contains all the state related to NAFLL ADC calibration
    typedef struct
    {
        UINT32 adcCalType;
        union
        {
            LW2080_CTRL_CLK_ADC_CAL_V10 v10; // Pascal
            LW2080_CTRL_CLK_ADC_CAL_V20 v20; // Volta/Turing
            LW2080_CTRL_CLK_ADC_DEVICE_CONTROL_DATA_V30 v30; // Ampere+
        };
    } AdcCalibrationState;

    // In Ampere RM decided to discontinue the ADC_CAL portion of the
    // ADC_DEVICE struct. They moved the calibration information directly
    // into the ADC_DEVICE struct itself. In order to make life easier
    // for our JS users, create a fake V30 calibration type. This allows
    // us to not have to rewrite most of the code that handles
    // AdcCalibrationState.
    static constexpr LwU8 MODS_SHIM_ADC_CAL_TYPE_V30 = 2;

    //! Get the at-init calibration state (slope/intercept) from all ADCs
    RC GetOriginalAdcCalibrationState
    (
        vector<AdcCalibrationState> *adcCalStates
    );

    //! Get the current calibration state (slope/intercept) from all ADCs
    RC GetLwrrentAdcCalibrationState
    (
        vector<AdcCalibrationState> *adcCalStates
    );

    //! Given a single ADC id, return that ADC's calibration settings
    RC GetAdcCalibrationStateViaId
    (
        AdcId id,
        bool bUseDefault,
        AdcCalibrationState *adcCalState
    );

    //! Set the calibration state of the ADC devices specified by adcDevMask
    //! to the calibration states supplied in adcCalStates.
    //! This is done in sequence (eg adcCalStates[0] corresponds to the first
    //! bit set in adcDevMask, adcCalStates[0] to second bit set, and so on)
    RC SetAdcCalibrationStateViaMask
    (
        UINT32 adcDevMask,
        const vector<AdcCalibrationState> &adcCalStates
    );

    //! Set the ADC calibration state of the ADC specified by id
    RC SetAdcCalibrationStateViaId
    (
        AdcId id,
        const AdcCalibrationState &adcCalState
    );

    GET_PROP(AdcDevMask, UINT32);
    GET_PROP(NafllDevMask, UINT32);
    GET_PROP(LutNumEntries, UINT32);
    GET_PROP(LutStepSizeuV, UINT32);
    GET_PROP(LutMilwoltageuV, UINT32);

    //! Enable or disable the PMU frequency controller for one or more NAFLL domains
    RC SetFreqControllerEnable(const vector<Gpu::ClkDomain>& clkDoms, bool bEnable) const;

    //! Enable or disable the PMU frequency controller for a single NAFLL domain
    RC SetFreqControllerEnable(Gpu::ClkDomain dom, bool bEnable) const;

    //! Enable or disable the PMU frequency controller for all NAFLL domains
    RC SetFreqControllerEnable(bool bEnable) const;

    //! Get the enable/disable status for a frequency controller on an
    //! NAFLL domain
    RC IsFreqControllerEnabled(Gpu::ClkDomain dom, bool* pbEnable) const;

    //! Get the list of clock domains that lwrrently have CLFC enabled
    RC GetEnabledFreqControllers(vector<Gpu::ClkDomain>* pClkDoms) const;

    //! Returns all voltage domains that lwrrently have CLVC enabled
    RC GetEnabledVoltControllers(set<Gpu::SplitVoltageDomain>* pVoltDoms) const;

    //! Enable or disable CLVC for one or more voltage domains
    RC SetVoltControllerEnable
    (
        const set<Gpu::SplitVoltageDomain>& voltDoms,
        bool bEnable
    ) const;

    //! Tells RM to set VF points above the noise-unaware Vmin into the
    //! voltage regime. Noise-unaware Vmin is defined as the minimum voltage
    //! needed to support all non-NAFLL clock domains such as dramclk/dispclk.
    RC SetRegime
    (
        Gpu::ClkDomain dom, //! Must be a 2x clock domain
        Perf::RegimeSetting regime
    ) const;

    struct RegimeOverride
    {
        Gpu::ClkDomain ClkDomain;
        Perf::RegimeSetting Regime;
    };
    using RegimeOverrides = vector<RegimeOverride>;
    RC SetRegime(const RegimeOverrides& overrides);

    void GetNafllClockDomainFFRLimitMHz(map<Gpu::ClkDomain, UINT32> *pClkDomainFFRLimitMHz);

    NafllId ClkDomToNafllId(Gpu::ClkDomain dom) const;

    RC GetAdcInfoPrintPri(UINT32* pri);
    RC SetAdcInfoPrintPri(UINT32 pri);

    UINT32 GetAdcDevMask(Gpu::SplitVoltageDomain) const;

    RC DisableClvc();
    RC EnableClvc();

    // Enable/Disable slowing down to the secondary VF lwrve indices for the
    // Device whose bits are 1 in the Mask
    RC SetQuickSlowdown(UINT32 vfLwrveIdx, UINT32 nafllDevMask, bool enable);
protected:
    RC GetAdcCalibrationStateViaMask
    (
        UINT32 adcDevMask,
        bool bUseDefault,
        vector<AdcCalibrationState> *adcDevices
    );

private:
    typedef struct
    {
        UINT32 devIndex;
        Gpu::ClkDomain clkDomain;
    } NafllInfo;

    GpuSubdevice *m_pSubdevice;
    bool m_IsInitialized;
    bool m_CalibrationStateHasChanged;
    UINT32 m_AdcDevMask;
    UINT32 m_NafllDevMask;
    UINT32 m_LutNumEntries;
    UINT32 m_LutStepSizeuV;
    UINT32 m_LutMilwoltageuV;
    const static map<UINT32, AdcId> s_lw2080AdcIdToAdcIdMap;
    const static map<UINT32, NafllId> s_lw2080NafllIdToNafllIdMap;
    map<AdcId, UINT32> m_adcIdToDevInd;
    vector<AdcCalibrationState> m_OriginalCalibrationState;
    Tee::Priority m_AdcInfoPrintPri;

    map<NafllId, NafllInfo> m_nafllIdToInfo;
    map<Gpu::ClkDomain, set<NafllId> > m_clkDomainToNafllIdSet;
    map<Gpu::ClkDomain, UINT32> m_ClkDomainToFFRLimitMHz;

    static map<UINT32, AdcId> CreateLw2080AdcIdToAdcIdMap();
    static RC TranslateLw2080AdcIdToAdcId(UINT32 lw2080AdcId, AdcId *pId);

    static RC TranslateLw2080NafllIdToNafllId(UINT32 lw2080NafllId, NafllId *pId);
    static map<UINT32, NafllId> CreateLw2080NafllIdToNafllIdMap();
    map<Gpu::SplitVoltageDomain, UINT32> m_VoltDomToAdcDevMask;

    set<Gpu::SplitVoltageDomain> m_ClvcVoltDoms = {};
    LW2080_CTRL_CLK_CLK_VOLT_CONTROLLERS_GET_INFO_PARAMS m_ClvcInfo = {};

    //! Helper method to get the RMCTRL params for all frequency controller(s)
    //! on one or more NAFLL domains
    RC FreqControllerGetControl
    (
        const vector<Gpu::ClkDomain>& clkDoms,
        LW2080_CTRL_CLK_CLK_FREQ_CONTROLLERS_CONTROL_PARAMS* pCtrlParams
    ) const;

    RC FreqControllerGetControl
    (
        Gpu::ClkDomain clkDom,
        LW2080_CTRL_CLK_CLK_FREQ_CONTROLLERS_CONTROL_PARAMS* pCtrlParams
    ) const;

    //! Calls VOLT_CONTROLLERS_GET_INFO/GET_CONTROL for voltDoms
    //  If voltDoms is empty, returns all available controllers
    RC VoltControllerGetControl
    (
        const set<Gpu::SplitVoltageDomain>& voltDoms,
        LW2080_CTRL_CLK_CLK_VOLT_CONTROLLERS_CONTROL_PARAMS* pCtrlParams
    ) const;
};

#endif // INCLUDED_CLKAVFS_H
