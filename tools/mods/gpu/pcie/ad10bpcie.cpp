/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ad10bpcie.h"
#include "core/include/platform.h"

//-----------------------------------------------------------------------------
//! \brief Get the current Secure Canary Path Monitor status of the GPU(Copy from hopperpcie.cpp).
//!
//! \return The current SCPM status of the GPU
/* virtual */ RC AD10BPcie::DoGetScpmStatus(UINT32 * pStatus) const
{
    RC rc;
    if (!pStatus)
    {
        return RC::BAD_PARAMETER;
    }
    *pStatus = 0;
    UINT16 extCapOffset = 0;
    UINT16 extCapSize   = 0;
    // Get the generic header
    // The only different between Hopper is we allow RC error for Gen4.
    if (RC::OK != Pci::GetExtendedCapInfo(DomainNumber(), BusNumber(),
                                          DeviceNumber(), FunctionNumber(),
                                          Pci::VSEC_ECI, 0,
                                          &extCapOffset, &extCapSize))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }   

    // VSEC_DEBUG_SEC is at offet 0x10 from the generic header.
    CHECK_RC(Platform::PciRead32(DomainNumber(), BusNumber(),
                                 DeviceNumber(), FunctionNumber(),
                                 extCapOffset + 0x10,
                                 pStatus));

    return RC::OK;
}
