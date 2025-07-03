/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
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
//! \brief Abstraction layer falcon ECC queries
//!
//! This class is normally instantiated from a GpuSubdevice, and
//! accessed via GpuSubdevice::GetFalconEcc().
//!
//! Unless otherwise indicated, the methods automatically clear the
//! pParams arguments so that the caller doesn't have to.
//!
//! There are two modifiers that are used by RM to describe the information
//! format: detail level and lifetime.
//! - detail level:
//!   - non-detailed: Total counts for each ECC unit. Implied if detail is not
//!     specified.
//!   - detailed: Counts for each ECC unit location. Slow to run.
//! - lifetime:
//!   - volatile: Counts since last RM init. Implied if aggregate is not
//!     specified.
//!   - aggregate: Counts for the life of the board.
//!
class FalconEcc
{
public:
    explicit FalconEcc(GpuSubdevice *pGpuSubdevice) : m_pGpuSubdevice(pGpuSubdevice) { }
    virtual ~FalconEcc() = default;
    GpuSubdevice *GetGpuSubdevice() const { return m_pGpuSubdevice; }

    virtual RC GetPmuDetailedEccCounts(Ecc::ErrCounts *pParams) = 0;
    virtual RC GetPmuDetailedAggregateEccCounts(Ecc::ErrCounts *pParams) = 0;

private:
    GpuSubdevice *m_pGpuSubdevice; //!< Set by constructor
};

class RmFalconEcc : public FalconEcc
{
public:
    explicit RmFalconEcc(GpuSubdevice *pGpuSubdevice) : FalconEcc(pGpuSubdevice) { }
    virtual ~RmFalconEcc() = default;

    RC GetPmuDetailedEccCounts(Ecc::ErrCounts *pParams) override
        { return GetDetailedEccCounts(pParams, ECC_UNIT_PMU); }

    RC GetPmuDetailedAggregateEccCounts(Ecc::ErrCounts *pParams) override
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_PMU); }

private:
    RC GetDetailedEccCounts(Ecc::ErrCounts* pParams, LW2080_ECC_UNITS unit);
    RC GetDetailedAggregateEccCounts(Ecc::ErrCounts* pParams, LW2080_ECC_UNITS unit);
};

class TegraFalconEcc : public FalconEcc
{
public:
    explicit TegraFalconEcc(GpuSubdevice *pGpuSubdevice) : FalconEcc(pGpuSubdevice) { }
    virtual ~TegraFalconEcc() = default;

    RC GetPmuDetailedEccCounts(Ecc::ErrCounts *pParams) override;
    RC GetPmuDetailedAggregateEccCounts(Ecc::ErrCounts *pParams) override;
};
