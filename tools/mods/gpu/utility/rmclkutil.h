/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmclkutil.h
//! \brief Clk Utility header used by many number of RM tests
//!

#ifndef INCLUDED_RMT_CLKUTIL_H
#define INCLUDED_RMT_CLKUTIL_H

#include "core/include/channel.h"
#include "lwRmApi.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"

#include "Lwcm.h"

#include "core/include/utility.h"
#include "random.h"

typedef struct
{
    UINT32 lwclk_desktop;
    UINT32 lwclk_desktop_MAX;
    UINT32 mclk_desktop;

    UINT32 lwclk_maxperf;
    UINT32 lwclk_maxperf_MAX;
    UINT32 mclk_maxperf;
} MYPERFINFO;

struct clkDomainStruct
{
    UINT32 clkDomain;
    const char *clkDomainName;
} const clkDomainArray[] =
{
    { LW2080_CTRL_CLK_DOMAIN_GPCCLK,        "GPCCLK"},
    { LW2080_CTRL_CLK_DOMAIN_XBARCLK,       "XBARCLK"},
    { LW2080_CTRL_CLK_DOMAIN_SYSCLK,        "SYSCLK"},
    { LW2080_CTRL_CLK_DOMAIN_HUBCLK,        "HUBCLK"},
    { LW2080_CTRL_CLK_DOMAIN_MCLK,          "MCLK" } ,
    { LW2080_CTRL_CLK_DOMAIN_HOSTCLK,       "HOSTCLK"},
    { LW2080_CTRL_CLK_DOMAIN_DISPCLK,       "DISPCLK"},
    { LW2080_CTRL_CLK_DOMAIN_PCLK0,         "PCLK0"},
    { LW2080_CTRL_CLK_DOMAIN_PCLK1,         "PCLK1"},
    { LW2080_CTRL_CLK_DOMAIN_XCLK,          "XCLK"},
    { LW2080_CTRL_CLK_DOMAIN_GPC2CLK,       "GPC2CLK"},
    { LW2080_CTRL_CLK_DOMAIN_LTC2CLK,       "LTC2CLK"},
    { LW2080_CTRL_CLK_DOMAIN_XBAR2CLK,      "XBAR2CLK"},
    { LW2080_CTRL_CLK_DOMAIN_SYS2CLK,       "SYS2CLK"},
    { LW2080_CTRL_CLK_DOMAIN_HUB2CLK,       "HUB2CLK"},
    { LW2080_CTRL_CLK_DOMAIN_LEGCLK,        "LEGCLK"},
    { LW2080_CTRL_CLK_DOMAIN_UTILSCLK,      "UTILSCLK"},
    { LW2080_CTRL_CLK_DOMAIN_PWRCLK,        "PWRCLK"},
    { LW2080_CTRL_CLK_DOMAIN_MSDCLK,        "MSDCLK"},
    { LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK,    "PCIEGENCLK"},
    { 0,                                    "UNKNOWN"}
};

struct clkSourceStruct
{
    UINT32 clkSource;
    const char *clkSourceName;
} const clkSourceArray[] =
{
    { LW2080_CTRL_CLK_SOURCE_DEFAULT,       "DEFAULT" },
    { LW2080_CTRL_CLK_SOURCE_MPLL,          "MPLL" },
    { LW2080_CTRL_CLK_SOURCE_VPLL0,         "VPLL0" },
    { LW2080_CTRL_CLK_SOURCE_VPLL1,         "VPLL1" },
    { LW2080_CTRL_CLK_SOURCE_SPPLL0,        "SPPLL0" },
    { LW2080_CTRL_CLK_SOURCE_SPPLL1,        "SPPLL1" },
    { LW2080_CTRL_CLK_SOURCE_XCLK,          "XCLK" },
    { LW2080_CTRL_CLK_SOURCE_PEXREFCLK,     "PEXREFCLK"},
    { LW2080_CTRL_CLK_SOURCE_XTAL,          "XTAL"},
    { LW2080_CTRL_CLK_SOURCE_3XXCLKDIV2,    "3XXCLKDIV2"},
    { LW2080_CTRL_CLK_SOURCE_GPCPLL,        "GPCPLL"},
    { LW2080_CTRL_CLK_SOURCE_LTCPLL,        "LTCPLL"},
    { LW2080_CTRL_CLK_SOURCE_XBARPLL,       "XBARPLL"},
    { LW2080_CTRL_CLK_SOURCE_SYSPLL,        "SYSPLL"},
    { LW2080_CTRL_CLK_SOURCE_XTAL4X,        "XTAL4X"},
    { LW2080_CTRL_CLK_SOURCE_REFMPLL,       "REFMPLL"},
    { LW2080_CTRL_CLK_SOURCE_HOSTCLK ,      "HOSTCLK" },
    { LW2080_CTRL_CLK_SOURCE_XCLK500,       "XCLK500" },
    { LW2080_CTRL_CLK_SOURCE_XCLKGEN3,      "XCLKGEN3" },
    { 0,                                    "UNKNOWN" }
};

struct clkAdcStruct
{
    UINT32 adcId;
    const char *adcName;
} const clkAdcArray[] =
{
    { LW2080_CTRL_CLK_ADC_ID_SYS ,      "SYS_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_LTC,       "LTC_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_XBAR,      "XBAR_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPC0,      "GPC0_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPC1,      "GPC1_ADC"} ,
    { LW2080_CTRL_CLK_ADC_ID_GPC2,      "GPC2_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPC3,      "GPC3_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPC4,      "GPC4_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPC5,      "GPC5_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPC6,      "GPC6_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPC7,      "GPC7_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_GPCS,      "GPCS_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_LWD,       "LWD_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_HOST,      "HOST_ADC"},
    { LW2080_CTRL_CLK_ADC_ID_MAX,       "UNKNOWN"}
};

class RmtClockUtil
{
public:

    void DumpDomains(UINT32 domains);

    RC dumpTableInfo(UINT32 m_hClient,
                     UINT32 hSubDev,
                     LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo);

    RC
    dumpLevels(UINT32 m_hClient,
               UINT32 hSubDev,
               LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo,
               LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS *pLevelInfo);

    RC
    LwrrentLevelInfo(UINT32 m_hClient,
                     UINT32 hSubDev,
                     UINT32 noOfClkDomains,
                    LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS *getLevelInfoParams,
                    LW2080_CTRL_PERF_SET_CLK_INFO *pSetClkInfos);

    RC
    LwrrentLevelClks(UINT32 m_hClient,
                     UINT32 hSubDev,
                     UINT32 noOfClkDomains,
                     LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                      *getLevelInfoParams,
                     bool activeClk);

    const char *GetClkDomainName(UINT32 clkDomain);
    const char *GetClkSourceName(UINT32 clkSource);
    const char *GetAdcName(UINT32 adcId);

    const UINT32 GetClkSourceValue(const char * clkSourceName) const;

    RC ChangeModeForced(UINT32 m_hClient,
                        UINT32 hDev,
                        UINT32 hSubDev,
                        UINT32 perfmode,
                        bool resetMode);

    RC getLevels(UINT32 hClient,
                 UINT32 hSubDev,
                 LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo,
                 LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS *pLevelInfo,
                 MYPERFINFO *pPerfInfo);

    RC ModsEnDisAble(UINT32 hClient,
                     UINT32 hSubDev,
                     bool enable);

    bool clkIsFreqWithinRange(UINT32 targetFreq,
                              UINT32 actualFreq,
                              UINT32 rangeIn100XPercent);

};

#endif // INCLUDED_RMT_CLKUTIL_H

