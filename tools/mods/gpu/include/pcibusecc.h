/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
//! \brief Abstraction layer PCI BUS ECC queries
//!
//! This class is normally instantiated from a GpuSubdevice, and
//! accessed via GpuSubdevice::GetPciBusEcc().
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
class PciBusEcc
{
public:
    explicit PciBusEcc(GpuSubdevice *pGpuSubdevice) : m_pGpuSubdevice(pGpuSubdevice) { }
    ~PciBusEcc() = default;
    GpuSubdevice *GetGpuSubdevice() const { return m_pGpuSubdevice; }

    RC GetDetailedReorderEccCounts(Ecc::ErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_PCIE_REORDER); }

    RC GetDetailedP2PEccCounts(Ecc::ErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_PCIE_P2PREQ); }

    RC GetDetailedAggregateReorderEccCounts(Ecc::ErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_PCIE_REORDER); }

    RC GetDetailedAggregateP2PEccCounts(Ecc::ErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_PCIE_P2PREQ); }

private:
    RC GetDetailedEccCounts(Ecc::ErrCounts* pParams, LW2080_ECC_UNITS unit);
    RC GetDetailedAggregateEccCounts(Ecc::ErrCounts* pParams, LW2080_ECC_UNITS unit);

    GpuSubdevice *m_pGpuSubdevice; //!< Set by constructor
};
