/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Filename: pgstats.h
// Helper class to get the current PG stats..Lwrrently supported engine is MSCG.
// More engines can be added as needed.

#ifndef _PGSTATS_H_
#define _PGSTATS_H_
#include "core/include/display.h"
#include "gpu/tests/rm/utility/dtiutils.h"

class PgStats
{
private:
    GpuSubdevice *pGpuSubDevice;
    UINT32 lastEngId;
    UINT32 lastGatingCount;
    UINT32 lastGatingUs;
    UINT32 lastUngatingCount;
    UINT32 lastUngatingUs;

public:
    PgStats(GpuSubdevice *pGpuSubDev)
    {
        pGpuSubDevice     = pGpuSubDev;
        lastEngId         = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_GR_RG;
        lastGatingCount   = 0;
        lastGatingUs      = 0;
        lastUngatingCount = 0;
        lastUngatingUs    = 0;
    }

    ~PgStats()
    {
        pGpuSubDevice = NULL;
    }

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
    RC getPGStats(UINT32  engId,         UINT32 *gatingCount, UINT32 *gatingUs,
                  UINT32 *ungatingCount, UINT32 *ungatingUs,
                  UINT32 *avgEntryUs,    UINT32 *avgExitUs);

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
    RC getPGPercent(UINT32 engId,  LwBool runOnEmu,
                    UINT32 *pct,   UINT32 *gatingCount,
                    UINT32 *avgUs, UINT32 *avgEntryUs,
                    UINT32 *avgExitUs);

    //! isPGSupported():
    //! This method check if PG for the engine is supported on the system.
    //!
    //! \param engId            [in] : The engine whose PGSTAT is to be returned.
    //! \param isSupported      [out]: The boolean value to indicate if system supports the engine or not.
    //!
    RC isPGSupported(UINT32 engId, bool *isSupported);

    //! isPGEnabled():
    //! This method check if PG for the engine is enabled in HW.
    //!
    //! \param engId            [in] : The engine whose PGSTAT is to be returned.
    //! \param isEnabled        [out]: The boolean value to indicate if the engine is enabled or not.
    //!
    RC isPGEnabled(UINT32 engId, bool *isEnabled);

    //! queryPGParameters():
    //! This method query parameters of PG.
    //!
    //! \param powerGatingParams   [in/out] : PG query parameters list.
    //! \param listSize            [in]     : size of parameters list.
    //!
    RC queryPGParameters(LW2080_CTRL_POWERGATING_PARAMETER* powerGatingParams, LwU32 listSize);
};

#endif

