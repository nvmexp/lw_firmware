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

#include "gpu/perf/perfutil.h"
#include "lwmisc.h"
#include "core/include/rc.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrl2080.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/voltsub.h"

namespace PerfUtil
{
    UINT32 s_LimitVerbosity = NO_LIMITS;
    bool clkDomainMapsInited = false;

    // Should probably use boost::bimap instead...
    map<Gpu::ClkDomain, Gpu::ClkDomain> clkDomains1To2;
    map<Gpu::ClkDomain, Gpu::ClkDomain> clkDomains2To1;

    void InitClockDomainMaps();
    bool IsClkDomainInMap
    (
        const map<Gpu::ClkDomain, Gpu::ClkDomain>& domMap,
        Gpu::ClkDomain inputDom
    );
    Gpu::ClkDomain GetColwertedDomain
    (
        const map<Gpu::ClkDomain, Gpu::ClkDomain>& domMap,
        Gpu::ClkDomain inputDom
    );
}

void PerfUtil::BoardObjGrpMaskBitSet(LW2080_CTRL_BOARDOBJGRP_MASK *pMask, UINT32 bitIdx)
{
    UINT32 elementIndex = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdx);
    UINT32 elementOffset = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdx);

    pMask->pData[elementIndex] |= BIT(elementOffset);
}

void PerfUtil::BoardObjGrpMaskBitClear(LW2080_CTRL_BOARDOBJGRP_MASK *pMask, UINT32 bitIdx)
{
    UINT32 elementIndex = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitIdx);
    UINT32 elementOffset = LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitIdx);

    pMask->pData[elementIndex] &= ~(BIT(elementOffset));
}

// We can get rid of this function in C++11
void PerfUtil::InitClockDomainMaps()
{
    // 1x to 2x clock domains
    clkDomains1To2[Gpu::ClkGpc]  = Gpu::ClkGpc2;
    clkDomains1To2[Gpu::ClkSys]  = Gpu::ClkSys2;
    clkDomains1To2[Gpu::ClkLtc]  = Gpu::ClkLtc2;
    clkDomains1To2[Gpu::ClkXbar] = Gpu::ClkXbar2;
    clkDomains1To2[Gpu::ClkHub]  = Gpu::ClkHub2;

    // 2x to 1x clock domains
    clkDomains2To1[Gpu::ClkGpc2]  = Gpu::ClkGpc;
    clkDomains2To1[Gpu::ClkSys2]  = Gpu::ClkSys;
    clkDomains2To1[Gpu::ClkLtc2]  = Gpu::ClkLtc;
    clkDomains2To1[Gpu::ClkXbar2] = Gpu::ClkXbar;
    clkDomains2To1[Gpu::ClkHub2]  = Gpu::ClkHub;

    clkDomainMapsInited = true;
}

bool PerfUtil::IsClkDomainInMap
(
    const map<Gpu::ClkDomain, Gpu::ClkDomain>& domMap,
    Gpu::ClkDomain inputDom
)
{
    if (!clkDomainMapsInited)
        InitClockDomainMaps();

    return domMap.find(inputDom) != domMap.end();
}

Gpu::ClkDomain PerfUtil::GetColwertedDomain
(
    const map<Gpu::ClkDomain, Gpu::ClkDomain>& domMap,
    Gpu::ClkDomain inputDom
)
{
    if (!clkDomainMapsInited)
        InitClockDomainMaps();

    map<Gpu::ClkDomain, Gpu::ClkDomain>::const_iterator clkMapItr = domMap.find(inputDom);
    if (clkMapItr != domMap.end())
        return clkMapItr->second;
    else
        return Gpu::ClkUnkUnsp;
}

void PerfUtil::Warn2xClock(Gpu::ClkDomain dom)
{
    Printf(Tee::PriWarn,
           "%s is deprecated. Use %s instead\n",
           ClkDomainToStr(dom),
           ClkDomainToStr(ColwertTo1xClockDomain(dom)));
}

bool PerfUtil::Is1xClockDomain(Gpu::ClkDomain dom)
{
    return IsClkDomainInMap(clkDomains1To2, dom);
}

bool PerfUtil::Is2xClockDomain(Gpu::ClkDomain dom)
{
    return IsClkDomainInMap(clkDomains2To1, dom);
}

Gpu::ClkDomain PerfUtil::ColwertTo1xClockDomain(Gpu::ClkDomain dom)
{
    return GetColwertedDomain(clkDomains2To1, dom);
}

Gpu::ClkDomain PerfUtil::ColwertTo2xClockDomain(Gpu::ClkDomain dom)
{
    return GetColwertedDomain(clkDomains1To2, dom);
}

Gpu::ClkDomain PerfUtil::ColwertToOppositeClockDomain(Gpu::ClkDomain dom)
{
    if (Is1xClockDomain(dom))
        return ColwertTo2xClockDomain(dom);
    else if (Is2xClockDomain(dom))
        return ColwertTo1xClockDomain(dom);
    return Gpu::ClkUnkUnsp;
}

// Note: This function MUST only be called for the clock domains
// which are not supported by the RM.
FLOAT32 PerfUtil::GetModsToRmFreqRatio(Gpu::ClkDomain dom)
{
    if (Is1xClockDomain(dom))
        return 2.0f;
    else if (Is2xClockDomain(dom))
        return 0.5f;
    return 1.0f;
}

// Note: This function MUST only be called for the clock domains
// which are not supported by the RM.
FLOAT32 PerfUtil::GetRmToModsFreqRatio(Gpu::ClkDomain dom)
{
    if (Is1xClockDomain(dom))
        return 0.5f;
    else if (Is2xClockDomain(dom))
        return 2.0f;
    return 1.0f;
}

const char* PerfUtil::VoltageLimitToStr(Volt3x::VoltageLimit voltLimit)
{
    switch (voltLimit)
    {
        case Volt3x::RELIABILITY_LIMIT:
            return "reliability";
        case Volt3x::ALT_RELIABILITY_LIMIT:
            return "alternate reliability";
        case Volt3x::OVERVOLTAGE_LIMIT:
            return "overvoltage";
        case Volt3x::VMIN_LIMIT:
            return "vmin";
        case Volt3x::VCRIT_LOW_LIMIT:
            return "vcritLow";
        case Volt3x::VCRIT_HIGH_LIMIT:
            return "vcritHigh";
        default:
            return "UNKNOWN";
    }
}

const char* PerfUtil::RamAssistTypeToStr(Volt3x::RamAssistType ramAssistType)
{
    switch (ramAssistType)
    {
        case Volt3x::RAM_ASSIST_TYPE_DISABLED:
            return "Disabled";
        case Volt3x::RAM_ASSIST_TYPE_STATIC:
            return "Static";
        case Volt3x::RAM_ASSIST_TYPE_DYNAMIC_WITHOUT_LUT:
            return "DynamicWithoutLut";
        case Volt3x::RAM_ASSIST_TYPE_DYNAMIC_WITH_LUT:
            return "DynamicWithLut";
        default:
            return "UNKNOWN";
    }
}

struct RegimeInfo
{
    Perf::RegimeSetting  modsRegime;
    UINT32               lw2080Regime;
    const char *         regimeStr;
};

static const RegimeInfo s_RegimeInfos[Perf::RegimeSetting_NUM] =
{  // modsId                             RM ID                                                        Name
    { Perf::FIXED_FREQUENCY_REGIME,      LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR,                         "fixed frequency" }
   ,{ Perf::VOLTAGE_REGIME,              LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR,                          "voltage" }
   ,{ Perf::FREQUENCY_REGIME,            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR,                          "frequency" }
   ,{ Perf::VR_ABOVE_NOISE_UNAWARE_VMIN, LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_ABOVE_NOISE_UNAWARE_VMIN, "vr above vmin"}
   ,{ Perf::FFR_BELOW_DVCO_MIN,          LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN,          "ffr below dvco min"}
   ,{ Perf::VR_WITH_CPM,                 LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_WITH_CPM,                 "vr with cpm"}
   ,{ Perf::DEFAULT_REGIME,              LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID,                     "default" }
};

UINT32 PerfUtil::RegimeSettingToCtrl2080Bit(Perf::RegimeSetting regime)
{
    if (regime < Perf::RegimeSetting_NUM)
    {
        MASSERT(s_RegimeInfos[regime].modsRegime == regime);
        return s_RegimeInfos[regime].lw2080Regime;
    }
    return 0;
}

Perf::RegimeSetting PerfUtil::RegimeCtrl2080BitToRegimeSetting(UINT32 reg2080Bit)
{
    MASSERT(!(reg2080Bit & (reg2080Bit -1)));

    for (UINT32 ii = 0; ii < NUMELEMS(s_RegimeInfos); ii++)
    {
        if (s_RegimeInfos[ii].lw2080Regime == reg2080Bit)
            return s_RegimeInfos[ii].modsRegime;
    }
    return Perf::RegimeSetting_NUM;
}

const char* PerfUtil::RegimeSettingToStr(Perf::RegimeSetting regime)
{
    if (regime < Perf::RegimeSetting_NUM)
    {
        MASSERT(s_RegimeInfos[regime].modsRegime == regime);
        return s_RegimeInfos[regime].regimeStr;
    }

    return "unknown";
}

Perf::RegimeSetting PerfUtil::RegimeSettingFromStr(const char* pRegimeStr)
{
    MASSERT(pRegimeStr);
    for (UINT32 ii = 0; ii < NUMELEMS(s_RegimeInfos); ii++)
    {
        if (!stricmp(pRegimeStr, s_RegimeInfos[ii].regimeStr))
            return static_cast<Perf::RegimeSetting>(ii);
    }
    return Perf::RegimeSetting_NUM;
}

struct VoltageDomainInfo
{
    Gpu::VoltageDomain  modsId;
    UINT32              lw2080Id;
    const char *        name;
};
static const VoltageDomainInfo s_VoltageDomainInfos[Gpu::VoltageDomain_NUM + 1] =
{
     { Gpu::CoreV, LW2080_CTRL_PERF_VOLTAGE_DOMAINS_CORE, "CoreV" }
    ,{ Gpu::ILWALID_VoltageDomain, 0, "ILWALID_VoltageDomain" }
};

UINT32 PerfUtil::GpuVoltageDomainToCtrl2080Bit(Gpu::VoltageDomain vd)
{
    if (vd < Gpu::VoltageDomain_NUM)
    {
        MASSERT(s_VoltageDomainInfos[vd].modsId == vd);
        return s_VoltageDomainInfos[vd].lw2080Id;
    }
    return 0;
}

Gpu::VoltageDomain PerfUtil::VoltCtrl2080BitToGpuVoltDomain
(
    UINT32 clk2080Bit
)
{
    MASSERT(!(clk2080Bit & (clk2080Bit - 1)));
    if (!(clk2080Bit & (clk2080Bit - 1)))
    {
        for (UINT32 idx = 0;
        idx < NUMELEMS(s_VoltageDomainInfos);
            idx++)
        {
            if (s_VoltageDomainInfos[idx].lw2080Id == clk2080Bit)
            {
                return s_VoltageDomainInfos[idx].modsId;
            }
        }
    }
    return Gpu::ILWALID_VoltageDomain;
}

UINT32 PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkDomain cd)
{
    if (cd < Gpu::ClkDomain_NUM)
    {
        MASSERT(PerfUtil::s_ClockDomainInfos[cd].modsId == cd);
        return PerfUtil::s_ClockDomainInfos[cd].lw2080Id;
    }
    return 0;
}

Gpu::ClkDomain PerfUtil::ClkCtrl2080BitToGpuClkDomain(UINT32 clk2080Bit)
{
    MASSERT(!(clk2080Bit & (clk2080Bit - 1)));

    // If only one bit is set
    if (!(clk2080Bit & (clk2080Bit - 1)))
    {
        for (UINT32 idx = 0; idx < NUMELEMS(PerfUtil::s_ClockDomainInfos); idx++)
        {
            if (PerfUtil::s_ClockDomainInfos[idx].lw2080Id == clk2080Bit)
            {
                return PerfUtil::s_ClockDomainInfos[idx].modsId;
            }
        }
    }
    return Gpu::ClkUnkUnsp;
}

struct SplitVoltageDomainInfo
{
    Gpu::SplitVoltageDomain modsId;
    UINT32                  lw2080Id;
    const char *            name;
};

static const SplitVoltageDomainInfo s_SplitVoltageDomainInfos[Gpu::SplitVoltageDomain_NUM] =
{    // modsId               RM id for get/set                     Name
     { Gpu::VOLTAGE_LOGIC,   LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC, "lwvdd" }
    ,{ Gpu::VOLTAGE_SRAM,    LW2080_CTRL_VOLT_VOLT_DOMAIN_SRAM,  "lwvdds" }
    ,{ Gpu::VOLTAGE_MSVDD,   LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD, "msvdd" }
};

UINT32 PerfUtil::GpuSplitVoltageDomainToCtrl2080Bit(Gpu::SplitVoltageDomain id)
{
    if (id < Gpu::SplitVoltageDomain_NUM)
    {
        MASSERT(s_SplitVoltageDomainInfos[id].modsId == id);
        return s_SplitVoltageDomainInfos[id].lw2080Id;
    }
    return LW2080_CTRL_VOLT_VOLT_DOMAIN_ILWALID;
}

// !Return the name of a clock domain
const char * PerfUtil::ClkDomainToStr(Gpu::ClkDomain cd)
{
    if (cd < Gpu::ClkDomain_NUM)
    {
        MASSERT(PerfUtil::s_ClockDomainInfos[cd].modsId == cd);
        return PerfUtil::s_ClockDomainInfos[cd].name;
    }
    return PerfUtil::s_ClockDomainInfos[Gpu::ClkUnkUnsp].name;
}

// !Return the name of a clock domain
const char * PerfUtil::VoltDomainToStr(Gpu::VoltageDomain voltDomain)
{
    if (voltDomain < Gpu::VoltageDomain_NUM)
    {
        MASSERT(s_VoltageDomainInfos[voltDomain].modsId == voltDomain);
        return s_VoltageDomainInfos[voltDomain].name;
    }
    return s_VoltageDomainInfos[Gpu::ILWALID_VoltageDomain].name;
}

const char * PerfUtil::SplitVoltDomainToStr
(
    Gpu::SplitVoltageDomain dom
)
{
    if (dom < Gpu::SplitVoltageDomain_NUM)
    {
        MASSERT(s_SplitVoltageDomainInfos[dom].modsId == dom);
        return s_SplitVoltageDomainInfos[dom].name;
    }
    return "unknown split voltage domain";
}

// !Return the clock domain specified by name
Gpu::ClkDomain PerfUtil::ClkDomainFromStr
(
    const char * clkdmnName
)
{
    if (clkdmnName)
    {
        for (UINT32 i = Gpu::ClkFirst; i < Gpu::ClkDomain_NUM; i++)
            if (!stricmp(clkdmnName, PerfUtil::s_ClockDomainInfos[i].name))
                return (Gpu::ClkDomain)i;
    }
    return Gpu::ClkUnkUnsp;
}

Gpu::SplitVoltageDomain PerfUtil::SplitVoltDomainFromStr
(
    const char * splitVoltDmnName
)
{
    if (splitVoltDmnName)
    {
        for (UINT32 i = 0; i < Gpu::SplitVoltageDomain_NUM; i++)
        {
            if (!stricmp(splitVoltDmnName, s_SplitVoltageDomainInfos[i].name))
                return (Gpu::SplitVoltageDomain)i;
        }

        // Bug 1722546 - preserve legacy behavior with "logic" and "sram" rail names
        if (!strcmp(splitVoltDmnName, "logic"))
        {
            Printf(Tee::PriWarn, "\"logic\" voltage domain name is deprecated. Use lwvdd instead\n");
            return Gpu::VOLTAGE_LOGIC;
        }
        else if (!strcmp(splitVoltDmnName, "sram"))
        {
            Printf(Tee::PriWarn, "\"sram\" voltage domain name is deprecated. Use lwvdds instead\n");
            return Gpu::VOLTAGE_SRAM;
        }
    }
    return Gpu::SplitVoltageDomain_NUM;
}

Pci::PcieLinkSpeed PerfUtil::PciGenCtrl2080BitToPciGen(LwU32 pci2080bit)
{
    switch (pci2080bit)
    {
        case LW2080_CTRL_BUS_SET_PCIE_SPEED_32000MBPS:
            return Pci::Speed32000MBPS;
        case LW2080_CTRL_BUS_SET_PCIE_SPEED_16000MBPS:
            return Pci::Speed16000MBPS;
        case LW2080_CTRL_BUS_SET_PCIE_SPEED_8000MBPS:
            return Pci::Speed8000MBPS;
        case LW2080_CTRL_BUS_SET_PCIE_SPEED_5000MBPS:
            return Pci::Speed5000MBPS;
        case LW2080_CTRL_BUS_SET_PCIE_SPEED_2500MBPS:
            return Pci::Speed2500MBPS;
        default:
            return Pci::SpeedUnknown;
    }
}

UINT32 PerfUtil::PciGenToCtrl2080Bit(Pci::PcieLinkSpeed linkSpeed)
{
    switch (linkSpeed)
    {
        case Pci::Speed32000MBPS:
            return LW2080_CTRL_BUS_SET_PCIE_SPEED_32000MBPS;
        case Pci::Speed16000MBPS:
            return LW2080_CTRL_BUS_SET_PCIE_SPEED_16000MBPS;
        case Pci::Speed8000MBPS:
            return LW2080_CTRL_BUS_SET_PCIE_SPEED_8000MBPS;
        case Pci::Speed5000MBPS:
            return LW2080_CTRL_BUS_SET_PCIE_SPEED_5000MBPS;
        case Pci::Speed2500MBPS:
            return LW2080_CTRL_BUS_SET_PCIE_SPEED_2500MBPS;
        default:
            return 0;
    }
}

Perf::ClockDomainType PerfUtil::RmClkDomTypeToModsClkDomType(UINT08 clockDomainType)
{
    switch(clockDomainType)
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY:
            return Perf::Master;
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY:
            return Perf::Slave;
        default:
            return Perf::None;
    }
}

struct AdcIdInfo
{
    Avfs::AdcId          adcId;
    const char *         adcIdStr;
};

static const AdcIdInfo s_AdcIdInfo[Avfs::AdcId_NUM] =
{   // ADC ID               // Name
     { Avfs::ADC_SYS,       "ADC_SYS"       }
    ,{ Avfs::ADC_LTC,       "ADC_LTC"       }
    ,{ Avfs::ADC_XBAR,      "ADC_XBAR"      }
    ,{ Avfs::ADC_LWD,       "ADC_LWD"       }
    ,{ Avfs::ADC_HOST,      "ADC_HOST"      }
    ,{ Avfs::ADC_GPC0,      "ADC_GPC0"      }
    ,{ Avfs::ADC_GPC1,      "ADC_GPC1"      }
    ,{ Avfs::ADC_GPC2,      "ADC_GPC2"      }
    ,{ Avfs::ADC_GPC3,      "ADC_GPC3"      }
    ,{ Avfs::ADC_GPC4,      "ADC_GPC4"      }
    ,{ Avfs::ADC_GPC5,      "ADC_GPC5"      }
    ,{ Avfs::ADC_GPC6,      "ADC_GPC6"      }
    ,{ Avfs::ADC_GPC7,      "ADC_GPC7"      }
    ,{ Avfs::ADC_GPC8,      "ADC_GPC8"      }
    ,{ Avfs::ADC_GPC9,      "ADC_GPC9"      }
    ,{ Avfs::ADC_GPC10,     "ADC_GPC10"     }
    ,{ Avfs::ADC_GPC11,     "ADC_GPC11"     }
    ,{ Avfs::ADC_GPCS,      "ADC_GPCS"      }
    ,{ Avfs::ADC_SRAM,      "ADC_SRAM"      }
    ,{ Avfs::ADC_SYS_ISINK, "ADC_SYS_ISINK" }
};

const char* PerfUtil::AdcIdToStr(Avfs::AdcId id)
{
    if (id < Avfs::AdcId_NUM)
    {
        MASSERT(s_AdcIdInfo[id].adcId == id);
        return s_AdcIdInfo[id].adcIdStr;
    }
    return "unknown ADC Id";
}

struct NafllIdInfo
{
    Avfs::NafllId        nafllId;
    const char *         nafllIdStr;
};

static const NafllIdInfo s_NafllIdInfo[Avfs::NafllId_NUM] =
{   // NAFLL ID               // Name
     { Avfs::NAFLL_SYS,       "NAFLL_SYS"   }
    ,{ Avfs::NAFLL_LTC,       "NAFLL_LTC"   }
    ,{ Avfs::NAFLL_XBAR,      "NAFLL_XBAR"  }
    ,{ Avfs::NAFLL_LWD,       "NAFLL_LWD"   }
    ,{ Avfs::NAFLL_HOST,      "NAFLL_HOST"  }
    ,{ Avfs::NAFLL_GPC0,      "NAFLL_GPC0"  }
    ,{ Avfs::NAFLL_GPC1,      "NAFLL_GPC1"  }
    ,{ Avfs::NAFLL_GPC2,      "NAFLL_GPC2"  }
    ,{ Avfs::NAFLL_GPC3,      "NAFLL_GPC3"  }
    ,{ Avfs::NAFLL_GPC4,      "NAFLL_GPC4"  }
    ,{ Avfs::NAFLL_GPC5,      "NAFLL_GPC5"  }
    ,{ Avfs::NAFLL_GPC6,      "NAFLL_GPC6"  }
    ,{ Avfs::NAFLL_GPC7,      "NAFLL_GPC7"  }
    ,{ Avfs::NAFLL_GPC8,      "NAFLL_GPC8"  }
    ,{ Avfs::NAFLL_GPC9,      "NAFLL_GPC9"  }
    ,{ Avfs::NAFLL_GPC10,     "NAFLL_GPC10" }
    ,{ Avfs::NAFLL_GPC11,     "NAFLL_GPC11" }
    ,{ Avfs::NAFLL_GPCS,      "NAFLL_GPCS"  }
};

const char* PerfUtil::NafllIdToStr(Avfs::NafllId id)
{
    if (id < Avfs::NafllId_NUM)
    {
        MASSERT(s_NafllIdInfo[id].nafllId == id);
        return s_NafllIdInfo[id].nafllIdStr;
    }
    return "unknown nafll Id";
}

RC PerfUtil::GetRmPerfLimitsInfo
(
    vector<LW2080_CTRL_PERF_LIMIT_INFO>& infos
   ,const GpuSubdevice * const pGpuSub
)
{
    RC rc;
    LW2080_CTRL_PERF_LIMITS_INFO_V2_PARAMS infoParam = {0};
    if (infos.size() > NUMELEMS(infoParam.limitsList)) {
        Printf(Tee::PriError, "info length %zu exceeds max %zu\n",
               infos.size(), NUMELEMS(infoParam.limitsList));
    }
    infoParam.numLimits = static_cast<LwU32>(infos.size());
    memcpy(&infoParam.limitsList, &infos[0], infos.size() * sizeof(infos[0]));
    rc = LwRmPtr()->ControlBySubdevice(pGpuSub,
                                       LW2080_CTRL_CMD_PERF_LIMITS_GET_INFO_V2,
                                       &infoParam, sizeof(infoParam));
    memcpy(&infos[0], &infoParam.limitsList, infos.size() * sizeof(infos[0]));

    return rc;
}

RC PerfUtil::GetRmPerfLimitsStatus
(
    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS>& statuses
   ,const GpuSubdevice * const pGpuSub
)
{
    RC rc;

    LW2080_CTRL_PERF_LIMITS_GET_STATUS_V2_PARAMS statusParam = {0};
    statusParam.numLimits = static_cast<LwU32>(statuses.size());
    // Set limitIDs as inputs in the array
    for (UINT32 limitIdx = 0; limitIdx < statuses.size(); limitIdx++)
    {
        statusParam.limitsList[limitIdx].limitId = statuses[limitIdx].limitId;
    }
    CHECK_RC(LwRmPtr()->ControlBySubdevice(pGpuSub,
                                           LW2080_CTRL_CMD_PERF_LIMITS_GET_STATUS_V2,
                                           &statusParam, sizeof(statusParam)));

    // Copy the output array
    for (UINT32 limitIdx = 0; limitIdx < statuses.size(); limitIdx++)
    {
        statuses[limitIdx] = statusParam.limitsList[limitIdx];
    }

    return rc;
}

RC PerfUtil::GetRmPerfPerfLimitsInfo
(
    LW2080_CTRL_PERF_PERF_LIMITS_INFO& infos
    ,GpuSubdevice *  pGpuSub
)
{
    return LwRmPtr()->ControlBySubdevice(
        pGpuSub
       ,LW2080_CTRL_CMD_PERF_PERF_LIMITS_GET_INFO
       ,&infos
       ,sizeof(infos));
}

RC PerfUtil::GetRmPerfPerfLimitsStatus
(
    LW2080_CTRL_PERF_PERF_LIMITS_STATUS& statuses
    ,GpuSubdevice *  pGpuSub
)
{
    return LwRmPtr()->ControlBySubdevice(
        pGpuSub
       ,LW2080_CTRL_CMD_PERF_PERF_LIMITS_GET_STATUS
       ,&statuses
       ,sizeof(statuses));
}

void PerfUtil::PrintRmPerfLimit
(
    Tee::Priority pri
   ,const LW2080_CTRL_PERF_LIMIT_INFO& info
   ,const LW2080_CTRL_PERF_LIMIT_GET_STATUS& status
   ,GpuSubdevice * pGpuSub
)
{
    const LW2080_CTRL_PERF_LIMIT_INPUT & input = status.input;
    const LW2080_CTRL_PERF_LIMIT_OUTPUT & output = status.output;
    MASSERT(status.limitId == info.limitId);

    if (info.szName[0] == '\0')
        return;

    const bool inOutDisabled = input.type == LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED &&
        !output.bEnabled;

    if (inOutDisabled)
    {
        if (s_LimitVerbosity & ONLY_ENABLED_LIMITS)
            return;
    }

    Printf(pri, " %s priority=%d %s%s",
           info.szName,
           info.priority,
           info.flags & 1 ? "min" : "",
           info.flags & 2 ? "max" : "");
    if (inOutDisabled)
        Printf(pri, " disabled");
    Printf(pri, "\n");

    if (inOutDisabled)
        return;

    PrintRmPerfLimitInput(pri, input, pGpuSub);
}

void PerfUtil::PrintRmPerfLimit
(
    Tee::Priority pri
   ,const LW2080_CTRL_PERF_LIMIT_INFO& info
   ,const LW2080_CTRL_PERF_LIMIT_SET_STATUS& status
   ,GpuSubdevice * pGpuSub
)
{
    const LW2080_CTRL_PERF_LIMIT_INPUT &input = status.input;
    MASSERT(status.limitId == info.limitId);

    if (info.szName[0] == '\0')
        return;

    const bool disabled = (input.type == LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED);
    if (disabled)
    {
        if (s_LimitVerbosity & ONLY_ENABLED_LIMITS)
            return;
    }

    Printf(pri, " %s priority=%d",
           info.szName,
           info.priority);

    if (disabled)
        Printf(pri, " disabled");
    Printf(pri, "\n");

    if (disabled)
        return;

    PrintRmPerfLimitInput(pri, input, pGpuSub);
}

RC PerfUtil::PrintRmPerfLimitInput
(
    Tee::Priority pri
   ,const LW2080_CTRL_PERF_LIMIT_INPUT& input
   ,GpuSubdevice * pGpuSub
)
{
    RC rc;
    Printf(pri, "  type=");
    switch (input.type)
    {
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED:
            Printf(pri, "disabled\n");
            break;
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE:
            Printf(pri, "pstate %d ",
                   Utility::BitScanForward(input.data.pstate.pstateId));
            if (input.data.pstate.point == LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_LAST)
            {
                Printf(pri, "\n");
                break;
            }
            Printf(pri, "point=");
            switch (input.data.pstate.point)
            {
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MIN:
                    Printf(pri, "min\n");
                    break;
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MID:
                    Printf(pri, "mid\n");
                    break;
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_NOM:
                    Printf(pri, "nom\n");
                    break;
                case LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_MAX:
                    Printf(pri, "max\n");
                    break;
                default:
                    Printf(pri, "unknown\n");
            }
            break;
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ:
        {
            Gpu::ClkDomain dom =
                ClkCtrl2080BitToGpuClkDomain(input.data.freq.domain);
            Printf(pri, "frequency domain=%s freqKHz=%d\n", ClkDomainToStr(dom),
                   input.data.freq.freqKHz);
            break;
        }
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE:
        {
            const LW2080_CTRL_PERF_VOLT_DOM_INFO & vInfo = input.data.volt.info;
            Printf(pri, "voltage domain=");
            Gpu::SplitVoltageDomain voltDomain =
                pGpuSub->GetVolt3x()->RmVoltDomToGpuSplitVoltageDomain(vInfo.voltageDomain);
            Printf(pri, "%s\n", SplitVoltDomainToStr(voltDomain));
            CHECK_RC(PrintVoltLimit(pri, vInfo, pGpuSub));
            break;

        }
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VOLTAGE_3X:
        {
            Printf(pri, "voltage domain=");
            Gpu::SplitVoltageDomain voltDomain =
                pGpuSub->GetVolt3x()->RmVoltDomToGpuSplitVoltageDomain(
                                                            input.data.volt3x.voltDomain);
            Printf(pri, "%s\n", SplitVoltDomainToStr(voltDomain));
            for (UINT32 ii = 0; ii < input.data.volt3x.numElements; ii++)
            {
                CHECK_RC(PrintVoltLimit(pri, input.data.volt3x.info[ii], pGpuSub));
            }
            break;
        }
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VPSTATE:
        case LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VPSTATE_IDX:
        {
            const UINT32& vpstateIdx = input.data.vpstate.vpstateIdx;
            Printf(pri, "vpstate vpstate=%d vpstateIndex=%d\n",
                   input.data.vpstate.vpstate, vpstateIdx);
            CHECK_RC(PrintVPStateLimit(pri, vpstateIdx, pGpuSub));
            break;
        }
        default:
            Printf(pri, "unknown\n");
            break;
    }
    return OK;
}

RC PerfUtil::PrintVoltLimit
(
    Tee::Priority pri
   ,const LW2080_CTRL_PERF_VOLT_DOM_INFO& vInfo
   ,GpuSubdevice* pGpuSub
)
{
    RC rc;

    Printf(pri, "   subType=");
    switch (vInfo.type)
    {
        case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL:
            Printf(pri, "actualVoltage voltageuV=%d\n", vInfo.data.logical.logicalVoltageuV);
            break;
        case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VDT:
            Printf(pri, "VDT vdtIndex=%d\n", vInfo.data.vdt.vdtIndex);
            break;
        case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_DELTA_ONLY:
            Printf(pri, "delta deltauV=%d\n", vInfo.voltageDeltauV.value);
            break;
        case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VFE:
        {
            Printf(pri, "VFE vfeIndex=%d targetVoltageuV=%d\n",
                   vInfo.data.vfe.vfeEquIndex,
                   vInfo.lwrrTargetVoltageuV);
            break;
        }
        case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_PSTATE:
            Printf(pri, "pstate pstateIdx=%d\n", vInfo.data.pstate.pstateIndex);
            break;
        case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VPSTATE:
        {
            Printf(pri, "vpstate vpstateIdx=%d\n", vInfo.data.vpstate.vpstateIndex);
            CHECK_RC(PrintVPStateLimit(pri, vInfo.data.vpstate.vpstateIndex, pGpuSub));
            break;
        }
        case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_FREQUENCY:
        {
            Gpu::ClkDomain dom =
                ClkCtrl2080BitToGpuClkDomain(vInfo.data.freq.clkDomain);
            Printf(pri, "freq domain=%s freqKHz=%d\n",
                   ClkDomainToStr(dom),
                   vInfo.data.freq.freqKHz);
            break;
        }
        default:
            Printf(pri, "unknown\n");
            break;
    }
    return rc;
}

RC PerfUtil::PrintVPStateLimit
(
    Tee::Priority pri,
    UINT32 vpstateIdx,
    GpuSubdevice* pGpuSub
)
{
    RC rc;

    Perf* const pPerf = pGpuSub->GetPerf();
    MASSERT(pPerf);

    if (!pPerf->IsPState30Supported())
        return rc;

    LW2080_CTRL_PERF_VPSTATES_INFO vpstatesInfo;
    memset(&vpstatesInfo, 0, sizeof(vpstatesInfo));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
             pGpuSub
            ,LW2080_CTRL_CMD_PERF_VPSTATES_GET_INFO
            ,&vpstatesInfo
            ,sizeof(vpstatesInfo)));

    for (UINT32 ii = 0; ii < LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS; ii++)
    {
        const Gpu::ClkDomain& domain = pPerf->GetDomainFromClockProgrammingOrder(ii);
        const UINT32& targetFreqMHz =
            vpstatesInfo.vpstates[vpstateIdx].data.v3x.clocks[ii].targetFreqMHz;
        if (domain != Gpu::ClkUnkUnsp && targetFreqMHz != 0)
        {
            Printf(pri, "    domain=%s targetFreqkHz=%u\n",
                ClkDomainToStr(domain),
                targetFreqMHz * 1000);
        }
    }

    return rc;
}

const char* PerfUtil::FuseIdToStr(LwU8 fuseId)
{
    switch (fuseId)
    {
        case LW2080_CTRL_FUSE_ID_STRAP_SPEEDO:
            return "FUSE_SPEEDO";
        case LW2080_CTRL_FUSE_ID_STRAP_SPEEDO_VERSION:
            return "FUSE_SPEEDO_VERSION";
        case LW2080_CTRL_FUSE_ID_STRAP_IDDQ:
            return "FUSE_IDDQ";
        case LW2080_CTRL_FUSE_ID_STRAP_IDDQ_VERSION:
            return "FUSE_IDDQ_VERSION";
        case LW2080_CTRL_FUSE_ID_STRAP_IDDQ_1:
            return "FUSE_IDDQ_1";
        case LW2080_CTRL_FUSE_ID_STRAP_BOARD_BINNING:
            return "FUSE_BOARD_BINNING";
        case LW2080_CTRL_FUSE_ID_STRAP_BOARD_BINNING_VERSION:
            return "FUSE_BOARD_BINNING_VERSION";
        case LW2080_CTRL_FUSE_ID_STRAP_SRAM_VMIN:
            return "FUSE_SRAM_VMIN";
        case LW2080_CTRL_FUSE_ID_STRAP_SRAM_VMIN_VERSION:
            return "FUSE_VMIN_VERSION";
        case LW2080_CTRL_FUSE_ID_ISENSE_VCM_OFFSET:
            return "FUSE_ISENSE_VCM_OFFSET";
        case LW2080_CTRL_FUSE_ID_ISENSE_DIFF_GAIN:
            return "FUSE_ISENSE_DIFF_GAIN";
        case LW2080_CTRL_FUSE_ID_ISENSE_DIFF_OFFSET:
            return "FUSE_ISENSE_DIFF_OFFSET";
        case LW2080_CTRL_FUSE_ID_ISENSE_CALIBRATION_VERSION:
            return "FUSE_ISENSE_CALIBRATION_VERSION";
        case LW2080_CTRL_FUSE_ID_ISENSE_DIFFERENTIAL_COARSE_GAIN:
            return "FUSE_ISENSE_DIFF_COARSE_GAIN";
        case LW2080_CTRL_FUSE_ID_KAPPA:
            return "FUSE_KAPPA(2x actual kappa value)";
        case LW2080_CTRL_FUSE_ID_KAPPA_VERSION:
            return "FUSE_KAPPA_VERSION";
        case LW2080_CTRL_FUSE_ID_STRAP_SPEEDO_1:
            return "FUSE_SPEEDO_1";
        case LW2080_CTRL_FUSE_ID_CPM_VERSION:
            return "FUSE_CPM_VERSION";
        case LW2080_CTRL_FUSE_ID_CPM_0:
            return "FUSE_CPM_0";
        case LW2080_CTRL_FUSE_ID_CPM_1:
            return "FUSE_CPM_1";
        case LW2080_CTRL_FUSE_ID_CPM_2:
            return "FUSE_CPM_2";
        case LW2080_CTRL_FUSE_ID_ISENSE_VCM_COARSE_OFFSET:
            return "FUSE_ISENSE_VCM_COARSE_OFFSET";
        case LW2080_CTRL_FUSE_ID_STRAP_BOOT_VMIN_MSVDD:
            return "FUSE_STRAP_BOOT_VMIN_MSVDD";
        case LW2080_CTRL_FUSE_ID_KAPPA_VALID:
            return "FUSE_KAPPA_VALID";
        case LW2080_CTRL_FUSE_ID_IDDQ_LWVDD:
            return "FUSE_IDDQ_LWVDD";
        case LW2080_CTRL_FUSE_ID_IDDQ_MSVDD:
            return "FUSE_IDDQ_MSVDD";
        case LW2080_CTRL_FUSE_ID_STRAP_SPEEDO_2:
            return "FUSE_STRAP_SPEEDO_2";
        case LW2080_CTRL_FUSE_ID_OC_BIN:
            return "FUSE_OC_BIN";
        case LW2080_CTRL_FUSE_ID_LV_FMAX_KNOB:
            return "FUSE_LV_FMAX_KNOB";
        case LW2080_CTRL_FUSE_ID_MV_FMAX_KNOB:
            return "FUSE_MV_FMAX_KNOB";
        case LW2080_CTRL_FUSE_ID_HV_FMAX_KNOB:
            return "FUSE_HV_FMAX_KNOB";
        case LW2080_CTRL_FUSE_ID_PSTATE_VMIN_KNOB:
            return "FUSE_PSTATE_VMIN_KNOB";
        case LW2080_CTRL_FUSE_ID_ILWALID:
            return "FUSE_ILWALID";
        default:
            MASSERT(!"Unknown FUSE ID");
            return "FUSE_UNKNOWN";
    }
}

const char* PerfUtil::VFieldIdToStr(LwU8 vfieldId)
{
    switch (vfieldId)
    {
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_CRYSTAL:
            return "CRYSTAL";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_TV_MODE:
            return "TV_MODE";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_FP_IFACE:
            return "FP_IFACE";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_PANEL:
            return "PANEL";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_RAMCFG:
            return "RAMCFG";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_MEMSEL:
            return "MEMSEL";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_UNUSED_0:
            return "UNUSED_0";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SPEEDO:
            return "SPEEDO";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SPEEDO_VERSION:
            return "SPEEDO_VERSION";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_IDDQ:
            return "IDDQ";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_IDDQ_VERSION:
            return "IDDQ_VERSION";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_IDDQ_1:
            return "IDDQ_1";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_BOARD_BINNING:
            return "BOARD_BINNING";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_BOARD_BINNING_VERSION:
            return "BOARD_BINNING_VERSION";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SRAM_VMIN:
            return "SRAM_VMIN";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SRAM_VMIN_VERSION:
            return "VMIN_VERSION";
        case LW2080_CTRL_BIOS_VFIELD_ID_ISENSE_VCM_OFFSET:
            return "ISENSE_VCM_OFFSET";
        case LW2080_CTRL_BIOS_VFIELD_ID_ISENSE_DIFF_GAIN:
            return "ISENSE_DIFF_GAIN";
        case LW2080_CTRL_BIOS_VFIELD_ID_ISENSE_DIFF_OFFSET:
            return "ISENSE_DIFF_OFFSET";
        case LW2080_CTRL_BIOS_VFIELD_ID_ISENSE_CALIBRATION_VERSION:
            return "ISENSE_CALIBRATION_VERSION";
        case LW2080_CTRL_BIOS_VFIELD_ID_KAPPA:
            return "KAPPA(2x actual kappa value)";
        case LW2080_CTRL_BIOS_VFIELD_ID_KAPPA_VERSION:
            return "KAPPA_VERSION";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SPEEDO_1:
            return "SPEEDO_1";
        case LW2080_CTRL_BIOS_VFIELD_ID_CPM_VERSION:
            return "CPM_VERSION";
        case LW2080_CTRL_BIOS_VFIELD_ID_CPM_0:
            return "CPM_0";
        case LW2080_CTRL_BIOS_VFIELD_ID_CPM_1:
            return "CPM_1";
        case LW2080_CTRL_BIOS_VFIELD_ID_CPM_2:
            return "CPM_2";
        case LW2080_CTRL_BIOS_VFIELD_ID_ISENSE_VCM_COARSE_OFFSET:
            return "ISENSE_VCM_COARSE_OFFSET";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_BOOT_VMIN_MSVDD:
            return "STRAP_BOOT_VMIN_MSVDD";
        case LW2080_CTRL_BIOS_VFIELD_ID_KAPPA_VALID:
            return "KAPPA_VALID";
        case LW2080_CTRL_BIOS_VFIELD_ID_IDDQ_LWVDD:
            return "IDDQ_LWVDD";
        case LW2080_CTRL_BIOS_VFIELD_ID_IDDQ_MSVDD:
            return "IDDQ_MSVDD";
        case LW2080_CTRL_BIOS_VFIELD_ID_STRAP_SPEEDO_2:
            return "STRAP_SPEEDO_2";
        case LW2080_CTRL_BIOS_VFIELD_ID_OC_BIN:
            return "OC_BIN";
        case LW2080_CTRL_BIOS_VFIELD_ID_LV_FMAX_KNOB:
            return "LV_FMAX_KNOB";
        case LW2080_CTRL_BIOS_VFIELD_ID_MV_FMAX_KNOB:
            return "MV_FMAX_KNOB";
        case LW2080_CTRL_BIOS_VFIELD_ID_HV_FMAX_KNOB:
            return "HV_FMAX_KNOB";
        case LW2080_CTRL_BIOS_VFIELD_ID_PSTATE_VMIN_KNOB:
            return "PSTATE_VMIN_KNOB";
        case LW2080_CTRL_BIOS_VFIELD_ID_ILWALID:
            return "INVALID";
        default:
            MASSERT(!"Unknown VFIELD ID");
            return "UNKNOWN";
    }
}

