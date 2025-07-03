/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "gpu/utility/eclwtil.h"

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief Abstraction layer HSHUB ECC queries
//!
//! This class is normally instantiated from a GpuSubdevice, and
//! accessed via GpuSubdevice::GetHsHubEcc().
//!
//! Unless otherwise indicated, the methods automatically clear the
//! pParams arguments so that the caller doesn't have to.
//!
class HsHubEcc
{
public:
    explicit HsHubEcc(GpuSubdevice *pGpuSubdevice) : m_pGpuSubdevice(pGpuSubdevice) { }
    ~HsHubEcc() = default;
    GpuSubdevice *GetGpuSubdevice() const { return m_pGpuSubdevice; }

    RC GetHsHubDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_HSHUB); }

    RC GetHsHubDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_HSHUB); }

private:
    RC GetDetailedEccCounts(Ecc::DetailedErrCounts * pParams, LW2080_ECC_UNITS unit);
    RC GetDetailedAggregateEccCounts(Ecc::DetailedErrCounts * pParams, LW2080_ECC_UNITS unit);

    GpuSubdevice *m_pGpuSubdevice; //!< Set by constructor
};
