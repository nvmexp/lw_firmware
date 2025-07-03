/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "gpu/utility/eclwtil.h"
#include <vector>

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief Abstraction layer around SUBDEVICE_GRAPHICS classes
//!
//! This class contains wrappers around RmControl calls for the
//! SUBDEVICE_GRAPHICS classes, GF100_SUBDEVICE_GRAPHICS and
//! GK110_SUBDEVICE_GRAPHICS, so that the caller doesn't have to use a
//! switch statement.  Each wrapper function copies the data into a
//! class-independant data structure.
//!
//! This class is normally instantiated from a GpuSubdevice, and
//! accessed via GpuSubdevice::GetSubdeviceGraphics().
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
class SubdeviceGraphics
{
public:
    static SubdeviceGraphics *Alloc(GpuSubdevice *pGpuSubdevice);
    SubdeviceGraphics(GpuSubdevice *pGpuSubdevice, UINT32 myClass);
    virtual ~SubdeviceGraphics() = default;
    GpuSubdevice *GetGpuSubdevice() const { return m_pGpuSubdevice; }
    UINT32 GetClass() const { return m_Class; }

    virtual RC GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams);

    // Wrapper around LW**E0_CTRL_CMD_GR_GET_TEX_COUNTS
    //
    struct GetTexDetailedEccCountsParams
    {
        // Indexed by [gpc][tpc][tex]
        vector<vector<vector<Ecc::ErrCounts>>> eccCounts;

    };
    virtual RC GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams);
    virtual RC GetL1DataDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetL1TagDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetCbuDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetSmIcacheDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetGccL15DetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetGpcMmuDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetHubMmuL2TlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetHubMmuHubTlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetHubMmuFillUnitDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetGpccsDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetFecsDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetSmRamsDetailedEccCounts(Ecc::DetailedErrCounts *pParams);

    virtual RC GetL1cDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetSHMDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetTexDetailedAggregateEccCounts(GetTexDetailedEccCountsParams *pParams);
    virtual RC GetL1DataDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetL1TagDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetCbuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetSmIcacheDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetGccL15DetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetGpcMmuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetHubMmuL2TlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetHubMmuHubTlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetHubMmuFillUnitDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetGpccsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetFecsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetSmRamsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams);

private:
    GpuSubdevice *m_pGpuSubdevice; //!< Set by constructor
    UINT32 m_Class; //!< Set by constructor
};
