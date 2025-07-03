/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2003-2013,2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Filename: pgstats.cpp
// Helper class to get the current PG stats..Lwrrently supported engine is MSCG.
// More engines can be added as needed.

#include "pgstats.h"

//! getPGStats():
//! This method gets the current PG stats for the engine provided qas argument.
//! This method lwrrently supports only MSCG engine..Other engines can be added as needed.
//!
//! \param engId            [in] : The engine whose PGSTAT is to be returned.
//! \param gatingCount      [out]: The current gating count for engine passed as argument.
//! \param gatingUs         [out]: The current gating time in uSec for engine passed as argument.
//! \param ungatingCount    [out]: The current ungating count for engine passed as argument.
//! \param ungatingUs       [out]: The current ungating time in uSec for engine passed as argument.
//! \param avgEntryUs       [out]: The current avg entry gating time in uSec for engine passed as argument.
//! \param avgExitUs        [out]: The current avg exit gating time in uSec for engine passed as argument.
//!
RC PgStats::getPGStats(UINT32 engId,
                       UINT32 *gatingCount,   UINT32 *gatingUs,
                       UINT32 *ungatingCount, UINT32 *ungatingUs,
                       UINT32 *avgEntryUs,    UINT32 *avgExitUs)
{
    RC rc;

    switch(engId)
    {
    case LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS:
        {
            const UINT32 listSize = 6;
            LW2080_CTRL_POWERGATING_PARAMETER powerGatingParam[listSize] = {};

            for (UINT32 i = 0; i < listSize; i++)
            {
                powerGatingParam[i].parameterExtended = engId;
            }

            powerGatingParam[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGCOUNT;
            powerGatingParam[1].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGTIME_US;
            powerGatingParam[2].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGCOUNT;
            powerGatingParam[3].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGTIME_US;
            powerGatingParam[4].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_AVG_ENTRYTIME_US;
            powerGatingParam[5].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_AVG_EXITTIME_US;

            CHECK_RC(queryPGParameters(powerGatingParam, listSize));

            *gatingCount        = powerGatingParam[0].parameterValue;
            *gatingUs           = powerGatingParam[1].parameterValue;
            *ungatingCount      = powerGatingParam[2].parameterValue;
            *ungatingUs         = powerGatingParam[3].parameterValue;
            *avgEntryUs         = powerGatingParam[4].parameterValue;
            *avgExitUs          = powerGatingParam[5].parameterValue;
            break;
        }
    default:
        Printf(Tee::PriError,"Other engines not supported");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! getPGPercent():
//! This method gets the current PG stats for the engine provided qas argument.
//! This method callwlates the percent using the difference of lwrrently PG stat and the past PGStat
//!
//! \param engId            [in] : The engine whose PGSTAT is to be returned.
//! \param pct              [out]: The current gating percent for engine passed as argument.
//! \param gatingCount      [out]: The current gating count for engine passed as argument.
//! \param gatingUs         [out]: The current gating time in uSec for engine passed as argument.
//! \param avgEntryUs       [out]: The current avg entry gating time in uSec for engine passed as argument.
//! \param avgExitUs        [out]: The current avg exit gating time in uSec for engine passed as argument.
//!
RC PgStats::getPGPercent(UINT32 engId, LwBool runOnEmu, UINT32 *pct,
                         UINT32 *gatingCount, UINT32 *avgGatingUs,
                         UINT32 *avgEntryUs, UINT32 *avgExitUs)
{
    RC rc;
    UINT32 lwrrGatingCount, lwrrGatingUs, lwrrUngatingCount, lwrrUngatingUs;
    UINT32 totalTime;

    // must query one time first to get the reference stats data
    if ((engId != lastEngId) ||
        (lastGatingCount == 0 && lastGatingUs == 0 && lastUngatingCount == 0 && lastUngatingUs == 0))
    {
        rc = getPGStats(engId,
                        &lastGatingCount,   &lastGatingUs,
                        &lastUngatingCount, &lastUngatingUs,
                        avgEntryUs,          avgExitUs);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%s: getPGStats() failed with rc=%d\n",
                   __FUNCTION__, (UINT32)rc);
            return rc;
        }

        if(runOnEmu)
        {
            GpuUtility::SleepOnPTimer(10, pGpuSubDevice);
        }
        else
        {
            Tasker::Sleep(1000);
        }
    }

    rc = getPGStats(engId,
                    &lwrrGatingCount,   &lwrrGatingUs,
                    &lwrrUngatingCount, &lwrrUngatingUs,
                    avgEntryUs,          avgExitUs);
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: getPGStats() failed with rc=%d\n",
               __FUNCTION__, (UINT32)rc);
        return rc;
    }

    UINT32    diffGatingCount   = lwrrGatingCount        - lastGatingCount;
    UINT32    diffGatingUs      = lwrrGatingUs           - lastGatingUs;
    UINT32    diffUngatingUs    = lwrrUngatingUs         - lastUngatingUs;

    lastGatingCount   = lwrrGatingCount;
    lastGatingUs      = lwrrGatingUs;
    lastUngatingCount = lwrrUngatingCount;
    lastUngatingUs    = lwrrUngatingUs;
    lastEngId         = engId;

    totalTime = diffGatingUs + diffUngatingUs;
    if (totalTime)
    {
        *pct = diffGatingUs * 100 / totalTime;
    }
    else
    {
        //
        // We have 3 cases below may hit this condition (no MSCG cycles)
        //
        // 1.  MSCG is disabled
        // 2.  Statistics somehow are not working
        // 3.  MSCG is enabled but it can't work for special condition like under
        //     immediate flip mode so it won't generate MSCG events to make statistics get populated
        //

        *pct = 0;
    }

    *gatingCount = diffGatingCount;

    if (*gatingCount)
    {
        *avgGatingUs = diffGatingUs / *gatingCount;
    }
    else
    {
        *avgGatingUs = 0;
    }

    return OK;
}

//! isPGSupported():
//! This method check if PG for the engine is supported on the system.
//!
//! \param engId            [in] : The engine whose PGSTAT is to be returned.
//! \param isSupported      [out]: The boolean value to indicate if system supports the engine or not.
//!
RC PgStats::isPGSupported(UINT32 engId, bool *isSupported)
{
    MASSERT(isSupported);

    RC rc;
    LW2080_CTRL_POWERGATING_PARAMETER powerGatingParam = {0};

    powerGatingParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;
    powerGatingParam.parameterExtended = engId;

    CHECK_RC(queryPGParameters(&powerGatingParam, 1));

    *isSupported = !!powerGatingParam.parameterValue;
    return OK;
}

//! isPGEnabled():
//! This method check if PG for the engine is enabled in HW.
//!
//! \param engId            [in] : The engine whose PGSTAT is to be returned.
//! \param isEnabled        [out]: The boolean value to indicate if the engine is enabled or not.
//!
RC PgStats::isPGEnabled(UINT32 engId, bool *isEnabled)
{
    MASSERT(isEnabled);

    RC rc;
    LW2080_CTRL_POWERGATING_PARAMETER powerGatingParam = {0};

    powerGatingParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
    powerGatingParam.parameterExtended = engId;

    CHECK_RC(queryPGParameters(&powerGatingParam, 1));

    *isEnabled = !!powerGatingParam.parameterValue;
    return OK;
}

//! queryPGParameters():
//! This method query parameters of PG.
//!
//! \param powerGatingParams   [in/out] : PG query parameters list.
//! \param listSize            [in]     : size of parameters list.
//!
RC PgStats::queryPGParameters(LW2080_CTRL_POWERGATING_PARAMETER* powerGatingParams, LwU32 listSize)
{
    MASSERT(powerGatingParams);

    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS queryPowerGatingParam = {0};

    queryPowerGatingParam.listSize = listSize;
    queryPowerGatingParam.parameterList = LW_PTR_TO_LwP64(powerGatingParams);

    rc = pLwRm->ControlBySubdevice(pGpuSubDevice,
                                   LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                   &queryPowerGatingParam,
                                   sizeof(queryPowerGatingParam));
    if (rc != OK)
    {
        Printf(Tee::PriHigh,"\n %s: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER Query failed with rc=%d\n",
               __FUNCTION__, (UINT32)rc);
        return rc;
    }

    return OK;
}

