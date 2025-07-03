/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2015-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "keplerpcie.h"
#include "ctrl/ctrl2080.h"
#include "core/include/lwrm.h"
#include "core/include/utility.h"
#include "core/include/massert.h"

namespace {
const UINT32 PEX_COUNTER_MASK = LW2080_CTRL_BUS_PEX_COUNTER_NAKS_SENT_COUNT |
                                LW2080_CTRL_BUS_PEX_COUNTER_REPLAY_COUNT |
                                LW2080_CTRL_BUS_PEX_COUNTER_RECEIVER_ERRORS |
                                LW2080_CTRL_BUS_PEX_COUNTER_LANE_ERRORS |
                                LW2080_CTRL_BUS_PEX_COUNTER_BAD_DLLP_COUNT |
                                LW2080_CTRL_BUS_PEX_COUNTER_REPLAY_ROLLOVER_COUNT |
                                LW2080_CTRL_BUS_PEX_COUNTER_8B10B_ERRORS_COUNT |
                                LW2080_CTRL_BUS_PEX_COUNTER_SYNC_HEADER_ERRORS_COUNT |
                                LW2080_CTRL_BUS_PEX_COUNTER_BAD_TLP_COUNT;
}

//-----------------------------------------------------------------------------
//! \brief Reset the PCIE error counts on the GPU
//!
//! \return OK if successful, not OK otherwise
RC KeplerPcie::DoResetErrorCounters()
{
    RC rc;

    CHECK_RC(FermiPcie::DoResetErrorCounters());

    // TODO : Kepler+ (including GF117) should be using this API for all
    // counters.  For now just use it to reset NAKS sent which are not
    // present in the Fermi API
    LW2080_CTRL_BUS_CLEAR_PEX_COUNTERS_PARAMS pexCounters;
    memset(&pexCounters, 0, sizeof(pexCounters));
    pexCounters.pexCounterMask = PEX_COUNTER_MASK;

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(GetSubdevice());
    MASSERT(pRmHandle);
    CHECK_RC(LwRmPtr()->Control(pRmHandle, LW2080_CTRL_CMD_BUS_CLEAR_PEX_COUNTERS,
                                &pexCounters, sizeof(pexCounters)));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the PCIE error counts on the GPU via hardware counters
//!
//! \param pCounts  : Pointer to GPU counts.
//!
//! \return OK if successful, not OK otherwise
RC KeplerPcie::DoGetErrorCounters(PexErrorCounts *pCounts)
{
    RC rc;

    CHECK_RC(FermiPcie::DoGetErrorCounters(pCounts));

    // TODO : Kepler+ (including GF117) should be using this API for all
    // counters.  For now just use it to retrieve NAKS sent which are not
    // present in the Fermi API
    LW2080_CTRL_BUS_GET_PEX_COUNTERS_PARAMS pexCounters;
    memset(&pexCounters, 0, sizeof(pexCounters));
    pexCounters.pexCounterMask = PEX_COUNTER_MASK;

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(GetSubdevice());
    MASSERT(pRmHandle);
    CHECK_RC(LwRmPtr()->Control(pRmHandle, LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                                &pexCounters, sizeof(pexCounters)));

    for (UINT32 i = 0; i < LW2080_CTRL_PEX_MAX_COUNTER_TYPES; ++i)
    {
        UINT32 lw2080PexCounter = PEX_COUNTER_MASK & (1U << i);
        if (lw2080PexCounter)
        {
            PexErrorCounts::PexErrCountIdx errIdx;
            CHECK_RC(PexErrorCounts::TranslatePexCounterTypeToPexErrCountIdx(
                         lw2080PexCounter, &errIdx));
            CHECK_RC(pCounts->SetCount(errIdx, true, pexCounters.pexCounters[i]));
        }
    }

    LW2080_CTRL_CMD_BUS_GET_PEX_LANE_COUNTERS_PARAMS perLaneParams = {};
    CHECK_RC(LwRmPtr()->Control(pRmHandle, LW2080_CTRL_CMD_BUS_GET_PEX_LANE_COUNTERS,
                                &perLaneParams, sizeof(perLaneParams)));

    pCounts->SetPerLaneCounts(&perLaneParams.pexLaneCounter[0]);

    return rc;
}
