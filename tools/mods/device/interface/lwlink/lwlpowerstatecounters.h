/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <utility>

#include "core/include/types.h"
#include "core/include/rc.h"

class LwLinkPowerStateCounters
{
public:
    // For GPUs we do double virtual dispatch, i.e. we have a GPU object with a
    // virtual interface, which aggregates another "implementation" object, with
    // its own virtual interface. A correct aggregation would forward interface
    // queries from the outer object to the inner. Unfortunately we don't have
    // this mechanism, therefore just querying for an interface via dynamic_cast
    // is not enough to determine if the interface is supported. We need this
    // additional function that will forward the question to the aggregated
    // "implementation" object.
    bool IsSupported() const
    {
        return LwlPowerStateCountersAreSupported();
    }

    //! \brief Clear the count of number of times the transmitter's sublink went to
    //! the low power (LP) mode
    //!
    //! \param linkId : Link ID on this device to query
    //!
    //! \return OK if successful, not OK otherwise
    RC ClearLPCounts(UINT32 linkId)
    {
        return DoClearLPCounts(linkId);
    }

    //! \brief Get the number of times the transmitter's sublink went to the lower
    //! power (LP) mode
    //!
    //! \param linkId : Link ID on this device to query
    //! \param pCount : Pointer to the variable to write the counter to
    //!
    //! \return OK if successful, not OK otherwise
    RC GetLPEntryOrExitCount(UINT32 linkId, bool entry, UINT32 *pCount, bool *pbOverflow)
    {
        return DoGetLPEntryOrExitCount(linkId, entry, pCount, pbOverflow);
    }

private:
    virtual bool LwlPowerStateCountersAreSupported() const = 0;
    virtual RC DoClearLPCounts(UINT32 linkId) = 0;
    virtual RC DoGetLPEntryOrExitCount(UINT32 linkId, bool entry, UINT32 *pCount, bool *pbOverflow) = 0;
};
