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

// PerfUtil is intended to be used for utility code that is common
// between Perf, Volt3x, Power, and Thermal

#pragma once
#ifndef INCLUDED_PERFUTIL_H
#define INCLUDED_PERFUTIL_H

#include "core/include/types.h"
#include "lwtypes.h"
#include "ctrl/ctrl2080/ctrl2080boardobj.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "core/include/tee.h"
#include "lwmisc.h"
#include "core/include/gpu.h"
#include "gpu/perf/voltsub.h"
#include "gpu/perf/perfsub.h" // needed for Perf::RegimeSetting, remove in C++11
#include "gpu/perf/avfssub.h"
#include "core/include/pci.h"
#include <vector>

class RC;
class GpuSubdevice;

namespace PerfUtil
{
    // Used to specify which limits to print on a PerfPoint switch in PStates 3.0
    enum LimitVerbosity
    {
        NO_LIMITS = 0,
        MODS_LIMITS = BIT(0),
        RM_LIMITS = BIT(1),
        ONLY_ENABLED_LIMITS = BIT(2),
        VERBOSE_LIMITS = BIT(3)
    };

    struct ClockDomainInfo
    {
        Gpu::ClkDomain      modsId;
        UINT32              lw2080Id;
        const char *        name;
    };
    // 2x Clocks must come before 1x clocks so that ClkCtrl2080BitToGpuClkDomain()
    // works correctly. RM stores gpc/sys/xbar/ltc as 2x and there is no DOMAIN_LTCCLK.
    // NOTE: Starting GV100, HUB2CLK has been deprecated  and there is no SW
    // programmable LTCCLK (It has been tied to XBARCLK internally in hardware).
    static const ClockDomainInfo s_ClockDomainInfos[Gpu::ClkDomain_NUM] =
    {  // modsId            RM id for get/set, 0 if not gettable   Name
    // ===============   =====================================  =======
        { Gpu::ClkUnkUnsp,  0,                                    ENCJSENT("ClkUnkUnsp") }
        ,{ Gpu::ClkM,        LW2080_CTRL_CLK_DOMAIN_MCLK,          ENCJSENT("ClkM") }
        ,{ Gpu::ClkHost,     LW2080_CTRL_CLK_DOMAIN_HOSTCLK,       ENCJSENT("ClkHost") }
        ,{ Gpu::ClkDisp,     LW2080_CTRL_CLK_DOMAIN_DISPCLK,       ENCJSENT("ClkDisp") }
        ,{ Gpu::ClkPA,       LW2080_CTRL_CLK_DOMAIN_PCLK0,         ENCJSENT("ClkPA") }
        ,{ Gpu::ClkPB,       LW2080_CTRL_CLK_DOMAIN_PCLK1,         ENCJSENT("ClkPB") }
        ,{ Gpu::ClkX,        LW2080_CTRL_CLK_DOMAIN_XCLK,          ENCJSENT("ClkX") }
        ,{ Gpu::ClkGpc2,     LW2080_CTRL_CLK_DOMAIN_GPC2CLK,       ENCJSENT("ClkGpc2") }
        ,{ Gpu::ClkLtc2,     LW2080_CTRL_CLK_DOMAIN_LTC2CLK,       ENCJSENT("ClkLtc2") }
        ,{ Gpu::ClkXbar2,    LW2080_CTRL_CLK_DOMAIN_XBAR2CLK,      ENCJSENT("ClkXbar2") }
        ,{ Gpu::ClkSys2,     LW2080_CTRL_CLK_DOMAIN_SYS2CLK,       ENCJSENT("ClkSys2") }
        ,{ Gpu::ClkHub2,     LW2080_CTRL_CLK_DOMAIN_HUB2CLK,       ENCJSENT("ClkHub2") }
        ,{ Gpu::ClkGpc,      LW2080_CTRL_CLK_DOMAIN_GPCCLK,        ENCJSENT("ClkGpc") }
        ,{ Gpu::ClkLtc,      LW2080_CTRL_CLK_DOMAIN_LTC2CLK,       ENCJSENT("ClkLtc") }
        ,{ Gpu::ClkXbar,     LW2080_CTRL_CLK_DOMAIN_XBARCLK,       ENCJSENT("ClkXbar") }
        ,{ Gpu::ClkSys,      LW2080_CTRL_CLK_DOMAIN_SYSCLK,        ENCJSENT("ClkSys") }
        ,{ Gpu::ClkHub,      LW2080_CTRL_CLK_DOMAIN_HUBCLK,        ENCJSENT("ClkHub") }
        ,{ Gpu::ClkLeg,      LW2080_CTRL_CLK_DOMAIN_LEGCLK,        ENCJSENT("ClkLeg") }
        ,{ Gpu::ClkUtilS,    LW2080_CTRL_CLK_DOMAIN_UTILSCLK,      ENCJSENT("ClkUtilS") }
        ,{ Gpu::ClkPwr,      LW2080_CTRL_CLK_DOMAIN_PWRCLK,        ENCJSENT("ClkPwr") }
        ,{ Gpu::ClkLwd,      LW2080_CTRL_CLK_DOMAIN_LWDCLK,        ENCJSENT("ClkLwd") }
        ,{ Gpu::ClkPexGen,   LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK,    ENCJSENT("ClkPexGen") }
        ,{ Gpu::ClkPexRef,   0,                                    ENCJSENT("ClkPexRef") }
        ,{ Gpu::ClkLwlCom,   0,                                    ENCJSENT("ClkLwlCom") }
    };
    extern UINT32 s_LimitVerbosity;

    RC GetRmPerfLimitsInfo
    (
        vector<LW2080_CTRL_PERF_LIMIT_INFO>& infos
       ,const GpuSubdevice * const pGpuSub
    );
    RC GetRmPerfLimitsStatus
    (
        vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS>& statuses
       ,const GpuSubdevice * const pGpuSub
    );

    RC GetRmPerfPerfLimitsInfo
    (
        LW2080_CTRL_PERF_PERF_LIMITS_INFO& infos
       ,GpuSubdevice *  pGpuSub
    );

    RC GetRmPerfPerfLimitsStatus
    (
        LW2080_CTRL_PERF_PERF_LIMITS_STATUS& statuses
       ,GpuSubdevice * pGpuSub
    );

    //! Print a single perf limit retrieved from RM
    void PrintRmPerfLimit
    (
        Tee::Priority pri
       ,const LW2080_CTRL_PERF_LIMIT_INFO& info
       ,const LW2080_CTRL_PERF_LIMIT_GET_STATUS& status
       ,GpuSubdevice * pGpuSub
    );

    // Overloaded PrintRmPerfLimit() to allow us to print limits
    // BEFORE we set them (useful for debugging)
    void PrintRmPerfLimit
    (
        Tee::Priority pri
       ,const LW2080_CTRL_PERF_LIMIT_INFO& info
       ,const LW2080_CTRL_PERF_LIMIT_SET_STATUS& status
       ,GpuSubdevice * pGpuSub
    );

    RC PrintRmPerfLimitInput
    (
        Tee::Priority pri
       ,const LW2080_CTRL_PERF_LIMIT_INPUT& input
       ,GpuSubdevice * pGpuSub
    );

    RC PrintVoltLimit
    (
        Tee::Priority pri
       ,const LW2080_CTRL_PERF_VOLT_DOM_INFO& vInfo
       ,GpuSubdevice* pGpuSub
    );

    RC PrintVPStateLimit(Tee::Priority pri, UINT32 vpstateIdx, GpuSubdevice* pGpuSub);

    void BoardObjGrpMaskBitSet(LW2080_CTRL_BOARDOBJGRP_MASK *pMask, UINT32 bitIdx);
    void BoardObjGrpMaskBitClear(LW2080_CTRL_BOARDOBJGRP_MASK *pMask, UINT32 bitIdx);

    // 1x/2x utility functions
    void Warn2xClock(Gpu::ClkDomain dom);
    bool Is2xClockDomain(Gpu::ClkDomain dom);
    bool Is1xClockDomain(Gpu::ClkDomain dom);
    Gpu::ClkDomain ColwertTo1xClockDomain(Gpu::ClkDomain dom);
    Gpu::ClkDomain ColwertTo2xClockDomain(Gpu::ClkDomain dom);
    Gpu::ClkDomain ColwertToOppositeClockDomain(Gpu::ClkDomain dom);
    FLOAT32 GetModsToRmFreqRatio(Gpu::ClkDomain dom);
    FLOAT32 GetRmToModsFreqRatio(Gpu::ClkDomain dom);

    const char* VoltageLimitToStr(Volt3x::VoltageLimit voltLimit);
    const char * RamAssistTypeToStr(Volt3x::RamAssistType ramAssist);
    // Gpu::ClkDomain utility functions
    UINT32 GpuClkDomainToCtrl2080Bit(Gpu::ClkDomain cd);
    Gpu::ClkDomain ClkCtrl2080BitToGpuClkDomain(UINT32 clk2080Bit);
    const char * ClkDomainToStr(Gpu::ClkDomain clkdmn);
    Gpu::ClkDomain ClkDomainFromStr(const char * clkdmnName);

    // Gpu::VoltageDomain utility functions
    UINT32 GpuVoltageDomainToCtrl2080Bit(Gpu::VoltageDomain Vd);
    Gpu::VoltageDomain VoltCtrl2080BitToGpuVoltDomain(UINT32 clk2080Bit);
    const char * VoltDomainToStr(Gpu::VoltageDomain VoltDomain);

    // Gpu::SplitVoltageDomain utility functions
    UINT32 GpuSplitVoltageDomainToCtrl2080Bit(Gpu::SplitVoltageDomain id);
    const char * SplitVoltDomainToStr(Gpu::SplitVoltageDomain dom);
    Gpu::SplitVoltageDomain SplitVoltDomainFromStr(const char * splitVoltDmnName);

    // Perf::RegimeSetting utility functions
    UINT32 RegimeSettingToCtrl2080Bit(Perf::RegimeSetting reg);
    Perf::RegimeSetting RegimeCtrl2080BitToRegimeSetting(UINT32 reg2080Bit);
    const char* RegimeSettingToStr(Perf::RegimeSetting reg);
    Perf::RegimeSetting RegimeSettingFromStr(const char* pRegimeStr);

    Perf::ClockDomainType RmClkDomTypeToModsClkDomType(UINT08 clockDomainType);

    // Colwersion functions for PCI gen speed
    Pci::PcieLinkSpeed PciGenCtrl2080BitToPciGen(LwU32 pci2080bit);
    UINT32 PciGenToCtrl2080Bit(Pci::PcieLinkSpeed linkSpeed);

    // Avfs utility functions
    const char* AdcIdToStr(Avfs::AdcId id);
    const char* NafllIdToStr(Avfs::NafllId id);

    const char* VFieldIdToStr(LwU8 vfieldId);
    const char* FuseIdToStr(LwU8 fuseId);
}

#endif
