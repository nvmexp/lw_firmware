/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include <vector>

class LwLinkLatencyBinCounters
{
public:

    // Bin thresholds used for latency collection
    struct LatencyBins
    {
        UINT32 lowThresholdNs;
        UINT32 midThresholdNs;
        UINT32 highThresholdNs;
    };

    // Counts of the latency in each of the possible bins
    struct LatencyCounts
    {
        UINT32 virtualChannel;
        UINT64 elapsedTimeMs;
        UINT64 low;
        UINT64 mid;
        UINT64 high;
        UINT64 panic;
    };

    //! \brief Setup the latency bins
    //!
    //! \param latencyBins        : Latency bin configuration
    //!
    //! \return OK if successful, not OK otherwise
    //!
    RC SetupLatencyBins(const LatencyBins &latencyBins) const
    {
        return DoSetupLatencyBins(latencyBins);
    }

    //! \brief Clear the latency bins on the virtual channels specified in the
    //!        mask
    //!
    //! \return OK if successful, not OK otherwise
    //!
    RC ClearLatencyBinCounts() const
    {
        return DoClearLatencyBinCounts();
    }

    //! \brief Get the latency bin counts
    //!
    //! \param pLatencyCounts     : Per virtual channel, per port latency counts
    //!
    //! \return OK if successful, not OK otherwise
    //!
    RC GetLatencyBinCounts(vector<vector<LatencyCounts>> *pLatencyCounts) const
    {
        return DoGetLatencyBinCounts(pLatencyCounts);
    }

private:
    virtual RC DoSetupLatencyBins(const LatencyBins &latencyBins) const = 0;
    virtual RC DoClearLatencyBinCounts() const = 0;
    virtual RC DoGetLatencyBinCounts(vector<vector<LatencyCounts>> *pLatencyCounts) const = 0;
};
