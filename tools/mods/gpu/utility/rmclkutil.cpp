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
//! \file rmclkutil.cpp
//! \brief Utility used by number of clk realted RM tests
//!

#include <vector>
#include <string>
#include <cctype>
#include <algorithm>

#include "rmclkutil.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"

//! \brief dumpTableInfo Function.
//!
//! Dumps the perf table information.
//!
//! \sa Run
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC RmtClockUtil::dumpTableInfo(UINT32 hClient,
                     UINT32 hSubDev,
                     LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo)
{
    UINT32 status;
    RC rc;

    // get perf table header
    status = LwRmControl(hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_TABLE_INFO,
                         pTableInfo, sizeof (*pTableInfo));
    if (status != LW_OK)
    {
        Printf(Tee::PriLow,"GET_TABLE_INFO failed: 0x%x, %d \n",
               status, __LINE__);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow,"flags         : 0x%x\n",
           (UINT32)(pTableInfo->flags));
    Printf(Tee::PriLow,"numLevels     : 0x%x\n",
           (UINT32)(pTableInfo->numLevels));
    Printf(Tee::PriLow,"numClkDomains: 0x%x\n",
           (UINT32)(pTableInfo->numPerfClkDomains));
    Printf(Tee::PriLow,"clkDomains    : 0x%x\n",
           (UINT32) (pTableInfo->perfClkDomains));

    return rc;
}

//! \brief dumpLevels Function.
//!
//! Dumps the each level information from the perf table.
//!
//! \sa Run
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC RmtClockUtil::dumpLevels(UINT32 hClient, UINT32 hSubDev,
           LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo,
           LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS *pLevelInfo)
{
    LW2080_CTRL_PERF_GET_CLK_INFO *pGetClkInfos;
    UINT32 j, k;
    UINT32 status;
    RC rc;

    // get levels
    for (j = 0; j < pTableInfo->numLevels; j++)
    {
        pLevelInfo->level = j;
        status = LwRmControl(hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_GET_LEVEL_INFO,
                             pLevelInfo, sizeof (*pLevelInfo));
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                  "PerfTableTest:GET_LEVEL_INFO failed:0x%x,  %d\n",
                   status,__LINE__);
            return RmApiStatusToModsRC(status);
        }

        Printf(Tee::PriLow,"level %d: flags 0x%x\n",
               j, (UINT32)(pLevelInfo->flags));

        pGetClkInfos =
            (LW2080_CTRL_PERF_GET_CLK_INFO *)pLevelInfo->perfGetClkInfoList;

        for (k = 0; k < pTableInfo->numPerfClkDomains; k++)
        {
         Printf(Tee::PriLow,
         "level%d:%-8s current%-4dMHz default%-4dMHz min%-4dMHz max%-4dMHz\n",
                   j,
                   GetClkDomainName(pGetClkInfos[k].domain),
                   (INT32) (pGetClkInfos[k].lwrrentFreq / 1000),
                   (INT32) (pGetClkInfos[k].defaultFreq / 1000),
                   (INT32) (pGetClkInfos[k].minFreq / 1000),
                   (INT32) (pGetClkInfos[k].maxFreq / 1000));
        }
    }

    return rc;
}

//! \brief dumpLevels Function.
//!
//! Dumps the each level information from the perf table.
//!
//! \sa Run
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC RmtClockUtil::LwrrentLevelInfo(UINT32 hClient,
                               UINT32 hSubDev,
                               UINT32 noOfClkDomains,
                               LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                                 *getLevelInfoParams,
                               LW2080_CTRL_PERF_SET_CLK_INFO *pSetClkInfos)
{
    UINT32 k, iLoopVar;
    UINT32 status;
    RC rc;
    LW2080_CTRL_PERF_GET_CLK_INFO *pGetClkInfos;

    //
    // Used for reading from actual clk domain rather than
    // from Perf Table struct
    //
    LW2080_CTRL_CLK_GET_INFO_PARAMS getDomainInfoParams = {0};
    vector<LW2080_CTRL_CLK_INFO> pClkDomainInfos(
            noOfClkDomains * sizeof (LW2080_CTRL_CLK_INFO));

    status = LwRmControl(hClient,
                         hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_LEVEL_INFO,
                         getLevelInfoParams,
                         sizeof (*getLevelInfoParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
              "PerfTableTest: GET_LEVEL_INFO failed: 0x%x,  %d\n",
               status, __LINE__);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow,"level %d: flags 0x%x\n",
     (INT32)(getLevelInfoParams->level), (INT32)(getLevelInfoParams->flags));

    pGetClkInfos =
       (LW2080_CTRL_PERF_GET_CLK_INFO *)getLevelInfoParams->perfGetClkInfoList;

    for (iLoopVar = 0; iLoopVar < noOfClkDomains; iLoopVar++)
    {
        pClkDomainInfos[iLoopVar].clkDomain = pGetClkInfos[iLoopVar].domain;
    }

    // hook up clk info table
    getDomainInfoParams.flags = 0;
    getDomainInfoParams.clkInfoListSize = noOfClkDomains;
    getDomainInfoParams.clkInfoList = LW_PTR_TO_LwP64(&pClkDomainInfos[0]);

    // get clocks
    status = LwRmControl(hClient,
                         hSubDev,
                         LW2080_CTRL_CMD_CLK_GET_INFO,
                         &getDomainInfoParams,
                         sizeof (getDomainInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "LwrrentLevelInfo:Get DomainsInfo fail: 0x%x, %d\n",
               status,__LINE__);
        return RmApiStatusToModsRC(status);
    }

    for (k = 0; k < noOfClkDomains; k++)
    {
     Printf(Tee::PriLow,
      "level%d:%-8s current%-4dMHz default%-4dMHz min%-4dMHz max%-4dMHz\n",
               (INT32) (getLevelInfoParams->level),
               GetClkDomainName(pGetClkInfos[k].domain),
               (INT32) (pGetClkInfos[k].lwrrentFreq / 1000),
               (INT32) (pGetClkInfos[k].defaultFreq / 1000),
               (INT32) (pGetClkInfos[k].minFreq / 1000),
               (INT32) (pGetClkInfos[k].maxFreq / 1000));

        Printf(Tee::PriLow,"Freq got SET is %-4dMHz \n",
                (INT32) (pClkDomainInfos[k].targetFreq / 1000));

        if (pGetClkInfos[k].domain == LW2080_CTRL_CLK_DOMAIN_MCLK)
        {
            //
            // Bug 384097 : For mGpus with no dedicated VideoRAM,
            // the value of mclk in perf table is always 0. We should
            // not consider 0 mclks while checking if the target
            // frequencies got set (which will never be set to 0)
            //
            if( (( pClkDomainInfos[k].targetFreq )  !=
                 ( pSetClkInfos[k].targetFreq    )) &&
                ( pSetClkInfos[k].targetFreq     )
              )
            {
                Printf(Tee::PriHigh,"Freq SET for MCLK Failed :%-4dMHz \n",
                        (INT32)( (pClkDomainInfos[k].targetFreq) ));

                return RC::LWRM_ERROR;
            }
            else
            {
                Printf(Tee::PriLow,"Freq got SET for MCLK is :%-4dMHz \n",
                        (INT32)( (pClkDomainInfos[k].targetFreq / 1000)));
            }
        }
        else
        {
              if ( (pClkDomainInfos[k].targetFreq) !=
                   (pSetClkInfos[k].targetFreq)
                 )
                  return RC::LWRM_ERROR;
        }
    }

    return rc;
}

//! \brief LwrrentLevelClks Function.
//!
//! Dumps and check for current level clock information. This is used during
//! verification path for clock actually got set to the expected perf level.
//!
//! \sa Run
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC RmtClockUtil::LwrrentLevelClks(UINT32 hClient,
                               UINT32 hSubDev,
                               UINT32 noOfClkDomains,
                               LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                                 *getLevelInfoParams,
                               bool activeClk)
{

    //
    // Here: pGetClkInfos is Clk infos from current level taken from Perf table
    //       pClkDomainInfos is from actual clock domain
    //

    UINT32 k, iLoopVar;
    UINT32 status;
    RC rc;
    LW2080_CTRL_PERF_GET_CLK_INFO *pGetClkInfos;

    //
    // Used for reading from actual clk domain rather than
    // from Perf Table struct
    //
    LW2080_CTRL_CLK_GET_INFO_PARAMS getDomainInfoParams = {0};
    vector<LW2080_CTRL_CLK_INFO> pClkDomainInfos(
            noOfClkDomains * sizeof (LW2080_CTRL_CLK_INFO));

    status = LwRmControl(hClient,
                         hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_LEVEL_INFO,
                         getLevelInfoParams,
                         sizeof (*getLevelInfoParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "PerfTableTest: GET_LEVEL_INFO failed: 0x%x, %d\n",
               status, __LINE__);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow,"level %d: flags 0x%x\n",
       (INT32)(getLevelInfoParams->level), (INT32)(getLevelInfoParams->flags));

    pGetClkInfos =
       (LW2080_CTRL_PERF_GET_CLK_INFO *)getLevelInfoParams->perfGetClkInfoList;

    for (iLoopVar = 0; iLoopVar < noOfClkDomains; iLoopVar++)
    {
        pClkDomainInfos[iLoopVar].clkDomain = pGetClkInfos[iLoopVar].domain;
    }

    // hook up clk info table
    getDomainInfoParams.flags = 0;
    getDomainInfoParams.clkInfoListSize = noOfClkDomains;
    getDomainInfoParams.clkInfoList = LW_PTR_TO_LwP64(&pClkDomainInfos[0]);

    // get clocks
    status = LwRmControl(hClient,
                         hSubDev,
                         LW2080_CTRL_CMD_CLK_GET_INFO,
                         &getDomainInfoParams,
                         sizeof (getDomainInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "LwrrentLevelInfo:Get DomainsInfo failed: 0x%x, %d\n",
               status,__LINE__);
        return RmApiStatusToModsRC(status);
    }

    if(activeClk)
    {

        for (k = 0; k < noOfClkDomains; k++)
        {
         Printf
         (Tee::PriLow,
         "level %d:%-8s current%-4dMHz default%-4dMHz min%-4dMHz max%-4dMHz\n",
                   (INT32)(getLevelInfoParams->level),
                   GetClkDomainName(pGetClkInfos[k].domain),
                   (INT32)(pGetClkInfos[k].lwrrentFreq / 1000),
                   (INT32)(pGetClkInfos[k].defaultFreq / 1000),
                   (INT32)(pGetClkInfos[k].minFreq / 1000),
                   (INT32)(pGetClkInfos[k].maxFreq / 1000)
          );

            if (pGetClkInfos[k].domain == LW2080_CTRL_CLK_DOMAIN_MCLK)
            {
                //
                // Bug 384097 : For mGpus with no dedicated VideoRAM,
                // the value of mclk in perf table is always 0. We should
                // not consider 0 mclks while checking if the target
                // frequencies got set (which will never be set to 0)
                //
                if( (( pClkDomainInfos[k].targetFreq ) !=
                     ( pGetClkInfos[k].lwrrentFreq   )) &&
                    ( pGetClkInfos[k].lwrrentFreq    )
                  )
                {
                    Printf(Tee::PriError,"Freq SET MCLK Failed:%-4dMHz \n",
                            (INT32)( (pClkDomainInfos[k].targetFreq)));
                    return RC::LWRM_ERROR;
                }
                else
                {
                    Printf(Tee::PriLow,"Freq got SET MCLK is :%-4dMHz \n",
                            (INT32)( (pClkDomainInfos[k].targetFreq / 1000)));
                }
            }
            else
            {
                if ( (pClkDomainInfos[k].targetFreq) <
                     (pGetClkInfos[k].defaultFreq)
                   )
                    return RC::LWRM_ERROR;
            }

        }

    }
    else
    {
        for (k = 0; k < noOfClkDomains; k++)
        {
         Printf(Tee::PriLow,
         "level%d:%-8s current%-4dMHz default%-4dMHz min%-4dMHz max%-4dMHz\n",
                   (INT32)(getLevelInfoParams->level),
                   GetClkDomainName(pGetClkInfos[k].domain),
                   (INT32)(pGetClkInfos[k].lwrrentFreq / 1000),
                   (INT32)(pGetClkInfos[k].defaultFreq / 1000),
                   (INT32)(pGetClkInfos[k].minFreq / 1000),
                   (INT32)(pGetClkInfos[k].maxFreq / 1000));

            Printf(Tee::PriLow,"Freq got SET is %-4dMHz \n",
                    (INT32) (pClkDomainInfos[k].targetFreq / 1000));

            if (pGetClkInfos[k].domain == LW2080_CTRL_CLK_DOMAIN_MCLK)
            {
                //
                // Bug 384097 : For mGpus with no dedicated VideoRAM,
                // the value of mclk in perf table is always 0. We should
                // not consider 0 mclks while checking if the target
                // frequencies got set (which will never be set to 0)
                //
                if( (( pClkDomainInfos[k].targetFreq ) !=
                    ( pGetClkInfos[k].lwrrentFreq    )) &&
                    ( pGetClkInfos[k].lwrrentFreq     )
                  )
                {
                    Printf(Tee::PriError,"Freq SET MCLK failed:%-4dMHz \n",
                            (INT32)( (pClkDomainInfos[k].targetFreq)));
                    return RC::LWRM_ERROR;
                }
                else
                {
                    Printf(Tee::PriLow,"Freq got SET MCLK is:%-4dMHz \n",
                            (INT32)( (pClkDomainInfos[k].targetFreq / 1000)));
                }
            }
            else
            {
                if ( (pClkDomainInfos[k].targetFreq) !=
                     (pGetClkInfos[k].lwrrentFreq)
                   )
                    return RC::LWRM_ERROR;
            }

        }
    }

    return rc;

}

//! \brief Helper function to get the clock domains' names.
//!
//! This is a helper function can be used in various tests to get what
//! clock domains' names.
//------------------------------------------------------------------------------
const char *RmtClockUtil::GetClkDomainName(UINT32 clkDomain)
{
    UINT32 i;

    for (i = 0;
         i < sizeof (clkDomainArray) / sizeof (struct clkDomainStruct);
         i++)
        if (clkDomainArray[i].clkDomain == clkDomain)
            return clkDomainArray[i].clkDomainName;

    // return last entry name: UNKNOWN
    return clkDomainArray[i-1].clkDomainName;
}

//! \brief Helper function to get the ADC name corresponding to the given ADC ID
//!
//! This is a helper function can be used in various tests to get ADC ID to
//! name mapping.
//------------------------------------------------------------------------------
const char *RmtClockUtil::GetAdcName(UINT32 adcId)
{
    UINT32 i;

    for (i = 0; i < sizeof (clkAdcArray) / sizeof (struct clkAdcStruct); i++)
    {
        if (clkAdcArray[i].adcId == adcId)
        {
            return clkAdcArray[i].adcName;
        }
    }

    // Return last entry name: UNKNOWN
    return clkAdcArray[i-1].adcName;
}

//! \brief Helper function to get the clock source names.
//!
//! This is a helper function can be used in various tests to get what
//! clock source names.
//------------------------------------------------------------------------------
const char *RmtClockUtil::GetClkSourceName(UINT32 clkSource)
{
    UINT32 i;

    for (i = 0;
         i < sizeof (clkSourceArray) / sizeof (struct clkSourceStruct);
         i++)
        if (clkSourceArray[i].clkSource == clkSource)
            return clkSourceArray[i].clkSourceName;

    // return last entry name: UNKNOWN
    return clkSourceArray[i-1].clkSourceName;
}

//! \brief Helper function to get the clock source value.
//!
//! This is a helper function can be used in various tests to get the
//! clock source value from its name.
//------------------------------------------------------------------------------
const UINT32 RmtClockUtil::GetClkSourceValue(const char *clkSourceName) const
{
    UINT32 i;
    std::string sourceName(clkSourceName);

    // Call the specific version of toupper from ctype instead of the locale ver.
    std::transform(sourceName.begin(), sourceName.end(), sourceName.begin(),
        (int (*)(int))std::toupper);

    for(i = 0;
        i < sizeof (clkSourceArray) / sizeof (struct clkSourceStruct);
        i++)
        if(sourceName.compare(clkSourceArray[i].clkSourceName) == 0)
            return clkSourceArray[i].clkSource;

    // return the last entry: UNKNOWN
    return clkSourceArray[i-1].clkSource;
}

//! \brief Helper function to dump out the domains supported.
//!
//! This is a helper function can be used in various tests to get what
//! clock domains are supported for current settings
//------------------------------------------------------------------------------
void RmtClockUtil::DumpDomains(UINT32 domains)
{
    Printf(Tee::PriLow, " supported domains:\n");

    if (domains & LW2080_CTRL_CLK_DOMAIN_MCLK)
        Printf(Tee::PriLow, "  MCLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_HOSTCLK)
        Printf(Tee::PriLow, "  HOSTCLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_DISPCLK)
        Printf(Tee::PriLow, "  DISPCLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_PCLK0)
        Printf(Tee::PriLow, "  PCLK0\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_PCLK1)
        Printf(Tee::PriLow, "  PCLK1\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_XCLK)
        Printf(Tee::PriLow, "  XCLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_GPC2CLK)
        Printf(Tee::PriLow, "  GPC2CLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_LTC2CLK)
        Printf(Tee::PriLow, "  LTC2CLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_XBAR2CLK)
        Printf(Tee::PriLow, "  XBAR2CLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_SYS2CLK)
        Printf(Tee::PriLow, "  SYS2CLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_HUB2CLK)
        Printf(Tee::PriLow, "  HUB2CLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_LEGCLK)
        Printf(Tee::PriLow, "  LEGCLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_UTILSCLK)
        Printf(Tee::PriLow, "  UTILSCLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_PWRCLK)
        Printf(Tee::PriLow, "  PWRCLK\n");
    if (domains & LW2080_CTRL_CLK_DOMAIN_MSDCLK)
        Printf(Tee::PriLow, "  MSDCLK\n");
}

//! \brief Utility to change the perf mode forcibly.
//!
//! Used at various places to change the mode, last arg resetmode
//! is used if the change in perf mode needs to be reseted.
//!
//! \return OK if passed, specific RC if failed
//-----------------------------------------------------------------------------
RC RmtClockUtil::ChangeModeForced(UINT32 hClient,
                               UINT32 hDev,
                               UINT32 hSubDev,
                               UINT32 perfmode,
                               bool resetMode)
{
    RC rc;
    LW2080_CTRL_PERF_BOOST_PARAMS perfBoostParams = {0};
    LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS getLwrrLevelInfoParams = {0};
    UINT32 status;

    Printf(Tee::PriLow,"ChangeModeForced test STARTS for %d\n",perfmode);

// START: Before Forced mode change reset to LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR

    perfBoostParams.flags = LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR;

    status = LwRmControl(hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_BOOST,
                         &perfBoostParams,
                         sizeof(perfBoostParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,"SET_LEVEL_INFO failed: 0x%x, %d\n",
               status, __LINE__);
        return RmApiStatusToModsRC(status);
    }

    // get and dump active level
    Printf(Tee::PriLow,"Dumping current perf level...\n");
    getLwrrLevelInfoParams.lwrrLevel = 0xffffffff;
    status = LwRmControl(hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_LWRRENT_LEVEL,
                         &getLwrrLevelInfoParams,
                         sizeof (getLwrrLevelInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
              "CMD_PERF_GET_LWRRENT_LEVEL failed: 0x%x , %d\n",
               status, __LINE__);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow,"  current level: %d\n",
        (INT32)(getLwrrLevelInfoParams.lwrrLevel));
// END: Before Forced mode change reset to LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR

    perfBoostParams.flags = LW2080_CTRL_PERF_BOOST_FLAGS_CMD_BOOST_TO_MAX;
    perfBoostParams.duration = LW2080_CTRL_PERF_BOOST_DURATION_INFINITE;
    status = LwRmControl(hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_BOOST,
                         &perfBoostParams,
                         sizeof(perfBoostParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
          "SET MODE failed for mode: %d: with status 0x%x ,%d\n",
           perfmode, status, __LINE__);
        return RmApiStatusToModsRC(status);
    }

    // get and dump active level
    Printf(Tee::PriLow,"Dumping current perf level...\n");
    getLwrrLevelInfoParams.lwrrLevel = 0xffffffff;
    status = LwRmControl(hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_LWRRENT_LEVEL,
                         &getLwrrLevelInfoParams,
                         sizeof (getLwrrLevelInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "CTRL_CMD_PERF_GET_LWRRENT_LEVEL failed: 0x%x , %d\n",
            status, __LINE__);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow,"  current level: %d\n",
           (INT32)(getLwrrLevelInfoParams.lwrrLevel));

    switch(perfmode)
    {
        case LW_CFGEX_PERF_MODE_2D_ALWAYS:
            if (getLwrrLevelInfoParams.lwrrLevel != 0)
                return RC::LWRM_ERROR;
            break;

        case LW_CFGEX_PERF_MODE_3D_LP_ALWAYS:
            if (getLwrrLevelInfoParams.lwrrLevel != 1)
                return RC::LWRM_ERROR;
            break;

        case LW_CFGEX_PERF_MODE_3D_ALWAYS:
            if (getLwrrLevelInfoParams.lwrrLevel != 2)
                return RC::LWRM_ERROR;
            break;
    }

    if (resetMode)
    {
    // START: After Forced mode change reset to LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR

        perfBoostParams.flags = LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR;

        status = LwRmControl(hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_BOOST,
                             &perfBoostParams,
                             sizeof(perfBoostParams));

        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,"SET_LEVEL_INFO failed: 0x%x ,%d\n",
                   status, __LINE__);
            return RmApiStatusToModsRC(status);
        }

        // get and dump active level
        Printf(Tee::PriLow,"Dumping current perf level...\n");
        getLwrrLevelInfoParams.lwrrLevel = 0xffffffff;
        status = LwRmControl(hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_GET_LWRRENT_LEVEL,
                             &getLwrrLevelInfoParams,
                             sizeof (getLwrrLevelInfoParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                  "CMD_PERF_GET_LWRRENT_LEVEL failed: 0x%x , %d\n",
                  status, __LINE__);
            return RmApiStatusToModsRC(status);
        }

        Printf(Tee::PriLow,"  current level: %d\n",
            (INT32)(getLwrrLevelInfoParams.lwrrLevel));
    // END: After Forced mode change reset to LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR
    }

    Printf(Tee::PriLow,"ChangeModeForced test ENDS for %d\n",perfmode);
    return rc;
}

//! \brief Utility to get the levels from the perf table.
//!
//!
//! \return OK if passed, specific RC if failed
//-----------------------------------------------------------------------------
RC RmtClockUtil::getLevels(UINT32 hClient,
                 UINT32 hSubDev,
                 LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo,
                 LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS *pLevelInfo,
                 MYPERFINFO *pPerfInfo)
{
    LW2080_CTRL_PERF_GET_CLK_INFO *pGetClkInfos;
    UINT32 j, k;
    UINT32 status;
    RC rc;

    // get levels
    for (j = 0; j < pTableInfo->numLevels; j++)
    {
        // skip all desktop=0/maxperf=numlevels-1
        if (j != 0 && j != (pTableInfo->numLevels-1))
            continue;

        pLevelInfo->level = j;
        status = LwRmControl(hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_GET_LEVEL_INFO,
                             pLevelInfo, sizeof (*pLevelInfo));

        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,"GET_LEVEL_INFO failed: 0x%x , %d\n",
                   status, __LINE__);
            return RC::LWRM_ERROR;
        }

        pGetClkInfos =
            (LW2080_CTRL_PERF_GET_CLK_INFO *)pLevelInfo->perfGetClkInfoList;
        for (k = 0; k < pTableInfo->numPerfClkDomains; k++)
        {
            if (pGetClkInfos[k].domain == LW2080_CTRL_CLK_DOMAIN_GPC2CLK)
            {
                if (j == 0)
                {
                    pPerfInfo->lwclk_desktop = pGetClkInfos[k].lwrrentFreq;
                    pPerfInfo->lwclk_desktop_MAX = pGetClkInfos[k].maxFreq;
                }
                else if (j == (pTableInfo->numLevels-1))
                {
                    pPerfInfo->lwclk_maxperf = pGetClkInfos[k].lwrrentFreq;
                    pPerfInfo->lwclk_maxperf_MAX = pGetClkInfos[k].maxFreq;
                }
            }
            else if (pGetClkInfos[k].domain == LW2080_CTRL_CLK_DOMAIN_MCLK)
            {
                if (j == 0)
                    pPerfInfo->mclk_desktop = pGetClkInfos[k].lwrrentFreq;
                else if (j == (pTableInfo->numLevels-1))
                    pPerfInfo->mclk_maxperf = pGetClkInfos[k].lwrrentFreq;
            }
        }
    }

    return rc;
}

//! \brief ModsEnDisAble.
//!
//------------------------------------------------------------------------------
RC RmtClockUtil::ModsEnDisAble(UINT32 hClient,
                               UINT32 hSubDev,
                               bool enable)
{
    RC rc;

    UINT32 status;

    LW2080_CTRL_PERF_SET_CLK_CTRL_PARAMS setClkCtrlParams = {0};

    if (enable)
    {
        //
        // MODS disables the clk control and thermal behavior changes,
        // Re-enable it for this test only
        //
        setClkCtrlParams.clkCtrl = 0;

        setClkCtrlParams.clkCtrl |=
            DRF_DEF(2080, _CTRL_PERF_CLK_CTRL, _GRAPHICS, _ENABLED) |
            DRF_DEF(2080, _CTRL_PERF_CLK_CTRL, _MEMORY, _ENABLED) |
            DRF_DEF(2080, _CTRL_PERF_CLK_CTRL, _THERMAL, _ENABLED);

        status = LwRmControl(hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_SET_CLK_CTRL,
                             &setClkCtrlParams,
                             sizeof (setClkCtrlParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                  "CMD_PERF_SET_CLK_CTRL failed: 0x%x , %d\n",
                   status, __LINE__);
        }
    }
    else
    {
        //
        // Reset to MODS original
        //
        setClkCtrlParams.clkCtrl = 0;

        setClkCtrlParams.clkCtrl |=
            DRF_DEF(2080, _CTRL_PERF_CLK_CTRL, _GRAPHICS, _DISABLED) |
            DRF_DEF(2080, _CTRL_PERF_CLK_CTRL, _MEMORY, _DISABLED) |
            DRF_DEF(2080, _CTRL_PERF_CLK_CTRL, _THERMAL, _DISABLED);

        status = LwRmControl(hClient, hSubDev,
                             LW2080_CTRL_CMD_PERF_SET_CLK_CTRL,
                             &setClkCtrlParams,
                             sizeof (setClkCtrlParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                  "CMD_PERF_SET_CLK_CTRL failed: 0x%x , %d\n",
                   status, __LINE__);
        }
    }

    // Mask error in case feature isn't supported.
    if (status == LW_ERR_NOT_SUPPORTED)
    {
        status = LW_OK;
    }

    rc = RmApiStatusToModsRC(status);

    return rc;
}

//! \brief Helper function to verify if the programmed frequency lies
//!        within the specified range of the Target Freq.
//!
//! This is a helper function can be used in various tests to verify the
//! the programmed frequncy based on percentage error input .
//! Input percentage tolerance is 100 times the original percentage value.
//! \Return TRUE if lies within the specified percentage limit otherwise FALSE.
//------------------------------------------------------------------------------
bool RmtClockUtil::clkIsFreqWithinRange
(
    UINT32 targetFreq,
    UINT32 actualFreq,
    UINT32 rangeIn100XPercent
)
{
    UINT32 MAXTargetFreq = targetFreq + (targetFreq * rangeIn100XPercent)/10000;
    UINT32 MINTargetFreq = targetFreq - (targetFreq * rangeIn100XPercent)/10000;

    if ((actualFreq <= MAXTargetFreq) && (actualFreq >= MINTargetFreq))
        return true;
    else
        return false;
}
