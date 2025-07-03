/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SUBDEVFB_H
#define INCLUDED_SUBDEVFB_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#include "gpu/utility/eclwtil.h"

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief Abstraction layer around SUBDEVICE_FB classes
//!
//! This class contains wrappers around RmControl calls for the
//! SUBDEVICE_FB classes, GF100_SUBDEVICE_FB and GK110_SUBDEVICE_FB,
//! so that the caller doesn't have to use a switch statement.  Each
//! wrapper function copies the data into a class-independant data
//! structure.
//!
//! This class is normally instantiated from a GpuSubdevice, and
//! accessed via GpuSubdevice::GetSubdeviceFb().
//!
//! Unless otherwise indicated, the methods automatically clear the
//! pParams arguments so that the caller doesn't have to.
//!
//! \see SubdeviceGraphics Description of the modifiers that can be applied to
//! the count information.
//!
class SubdeviceFb
{
public:
    static SubdeviceFb *Alloc(GpuSubdevice *pGpuSubdevice);
    SubdeviceFb(GpuSubdevice *pGpuSubdevice, UINT32 myClass);
    virtual ~SubdeviceFb() = default;
    GpuSubdevice *GetGpuSubdevice() const { return m_pGpuSubdevice; }
    UINT32 GetClass() const { return m_Class; }

    virtual RC GetDramDetailedEccCounts(Ecc::DetailedErrCounts *pParams) = 0;
    virtual RC GetLtcDetailedEccCounts(Ecc::DetailedErrCounts *pParams) = 0;
    struct GetEdcCountsParams
    {
        vector<UINT64> edcCounts; // Indexed by [virtualFbio]
    };
    virtual RC GetEdcCounts(GetEdcCountsParams *pParams) = 0;

    virtual RC GetDramDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) = 0;
    virtual RC GetLtcDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) = 0;

    // Wrapper around LW**E1_CTRL_CMD_FB_GET_EDC_CRC_DATA
    //
    struct GetEdcCrcDataParams
    {
        struct CrcData
        {
            UINT32 expected;
            UINT32 actual;
        };
        vector<CrcData> crcData; // Indexed by [virtualFbio]
    };
    virtual RC GetEdcCrcData(GetEdcCrcDataParams *pParams) = 0;

    // Wrapper around LW**E1_CTRL_CMD_FB_GET_LATEST_ECC_ADDRESSES
    //
    // NOTE: assuming the FB has an ECC unit (uncorr=DBE), not parity (uncorr=SBE)
    struct GetLatestEccAddressesParams
    {
        struct Address
        {
            UINT64      physAddress;
            bool        isSbe;           // true = SBE, false = DBE
            UINT32      sbeBitPosition;  // _UINT32_MAX = unknown
            UINT32      rbcAddress;
            Ecc::Source eccSource;
        };
        vector<Address> addresses;
        UINT32 totalAddressCount;
    };
    virtual RC GetLatestEccAddresses(GetLatestEccAddressesParams *pParams) = 0;

private:
    GpuSubdevice *m_pGpuSubdevice; //!< Set by constructor
    UINT32 m_Class; //!< Set by constructor
};

#endif
