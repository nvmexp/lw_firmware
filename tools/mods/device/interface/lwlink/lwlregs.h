/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file lwlink.h -- LwLink interface defintion

#pragma once

#include "core/include/rc.h"
#include "gpu/reghal/reghaltable.h"

class RegHal;

class LwLinkRegs
{
public:
    RegHal& Regs()
        { return DoRegs(); }
    const RegHal& Regs() const
        { return DoRegs(); }
    RC RegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT32 *pData)
        {
            MASSERT(pData);
            RC rc;
            UINT64 data64 = 0;
            CHECK_RC(DoRegRd(linkId, domain, offset, &data64));
            MASSERT(data64 <= _UINT32_MAX);
            *pData = static_cast<UINT32>(data64);
            return OK;
        }
    RC RegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData)
        { return DoRegRd(linkId, domain, offset, pData); }
    RC RegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT32 data)
        { return DoRegWr(linkId, domain, offset, static_cast<UINT64>(data)); }
    RC RegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data)
        { return DoRegWr(linkId, domain, offset, data); }
    RC RegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT32 data)
        { return DoRegWrBroadcast(linkId, domain, offset, static_cast<UINT64>(data)); }
    RC RegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data)
        { return DoRegWrBroadcast(linkId, domain, offset, data); }

protected:
    virtual RegHal& DoRegs() = 0;
    virtual const RegHal& DoRegs() const = 0;
    virtual RC DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData) = 0;
    virtual RC DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) = 0;
    virtual RC DoRegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) = 0;
};
