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

#include "lwlinkreghal.h"
#include "device/interface/lwlink/lwlregs.h"

#ifdef MATS_STANDALONE
#include "fakemods.h"
#else
#include "gpu/include/testdevice.h"
#endif

//-----------------------------------------------------------------------------
LwLinkRegHal::LwLinkRegHal(TestDevice* pTestDevice)
: RegHal(pTestDevice)
{
}

//--------------------------------------------------------------------
UINT32 LwLinkRegHal::Read32
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();
    UINT32 val = 0;
    if (OK != pLwLinkRegs->RegRd(domainIndex,
                                 LookupDomain(address, arrayIndexes),
                                 LookupAddress(address, arrayIndexes),
                                 &val))
    {
        Printf(Tee::PriError, "Failed to read lwlink register %s\n", ColwertToString(address));
        MASSERT(!"Failed to read lwlink register");
    }
    return val;
}

//--------------------------------------------------------------------
//! \brief Write a register
void LwLinkRegHal::Write32
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT32            value
)
{
    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    if (OK != pLwLinkRegs->RegWr(domainIndex,
                                 regDomain,
                                 LookupAddress(address, arrayIndexes),
                                 value))
    {
        Printf(Tee::PriError, "Failed to write lwlink register %s\n", ColwertToString(address));
        MASSERT(!"Failed to write lwlink register");
    }
}

//--------------------------------------------------------------------
//! \brief Write a register
void LwLinkRegHal::Write32Broadcast
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT32            value
)
{
    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    if (OK != pLwLinkRegs->RegWrBroadcast(domainIndex,
                                          regDomain,
                                          LookupAddress(address, arrayIndexes),
                                          value))
    {
        Printf(Tee::PriError, "Failed to write lwlink broadcast register %s\n", ColwertToString(address));
        MASSERT(!"Failed to write lwlink register");
    }
}

//--------------------------------------------------------------------
//! \brief Write a register
void LwLinkRegHal::Write32Sync
(
    UINT32             domainIndex,
    ModsGpuRegAddress  address,
    ArrayIndexes       arrayIndexes,
    UINT32             value
)
{
    Printf(Tee::PriError, "Write32Sync not supported for lwlink registers\n");
    MASSERT(!"Write32Sync not supported for lwlink registers");
}

//--------------------------------------------------------------------
//! \brief Write a register field
void LwLinkRegHal::Write32
(
    UINT32          domainIndex,
    RegSpace        space,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
)
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    const UINT32 regAddress = LookupAddress(ColwertToAddress(field), addressIndexes);
    RegHalDomain regDomain = LookupDomain(ColwertToAddress(field), addressIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();

    UINT32 regValue = 0;
    if (OK != pLwLinkRegs->RegRd(domainIndex,
                                 regDomain,
                                 regAddress,
                                 &regValue))
    {
        Printf(Tee::PriError, "Failed to read lwlink field %s\n", ColwertToString(field));
        MASSERT(!"Failed to read lwlink field");
    }
    SetField(&regValue, field, fieldIndexes, value);
    if (OK != pLwLinkRegs->RegWr(domainIndex,
                                 regDomain,
                                 regAddress,
                                 regValue))
    {
        Printf(Tee::PriError, "Failed to write lwlink field %s\n", ColwertToString(field));
        MASSERT(!"Failed to write lwlink field");
    }
}

//--------------------------------------------------------------------
//! \brief Write a register field
void LwLinkRegHal::Write32Sync
(
    UINT32          domainIndex,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
)
{
    Printf(Tee::PriError, "Write32Sync not supported for lwlink registers\n");
    MASSERT(!"Write32Sync not supported for lwlink registers");
}

//--------------------------------------------------------------------
UINT64 LwLinkRegHal::Read64
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();
    UINT64 val = 0;
    if (OK != pLwLinkRegs->RegRd(domainIndex,
                                 LookupDomain(address, arrayIndexes),
                                 LookupAddress(address, arrayIndexes),
                                 &val))
    {
        Printf(Tee::PriError, "Failed to read lwlink register %s\n", ColwertToString(address));
        MASSERT(!"Failed to read lwlink register");
    }
    return val;
}

//--------------------------------------------------------------------
//! \brief Write a register
void LwLinkRegHal::Write64
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT64            value
)
{
    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    if (OK != pLwLinkRegs->RegWr(domainIndex,
                                 regDomain,
                                 LookupAddress(address, arrayIndexes),
                                 value))
    {
        Printf(Tee::PriError, "Failed to write lwlink register %s\n", ColwertToString(address));
        MASSERT(!"Failed to write lwlink register");
    }
}

//--------------------------------------------------------------------
//! \brief Write a register
void LwLinkRegHal::Write64Broadcast
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT64            value
)
{
    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    if (OK != pLwLinkRegs->RegWrBroadcast(domainIndex,
                                          regDomain,
                                          LookupAddress(address, arrayIndexes),
                                          value))
    {
        Printf(Tee::PriError, "Failed to write lwlink broadcast register %s\n", ColwertToString(address));
        MASSERT(!"Failed to write lwlink register");
    }
}

//--------------------------------------------------------------------
//! \brief Write a register
void LwLinkRegHal::Write64Sync
(
    UINT32             domainIndex,
    ModsGpuRegAddress  address,
    ArrayIndexes       arrayIndexes,
    UINT64             value
)
{
    Printf(Tee::PriError, "Write64Sync not supported for lwlink registers\n");
    MASSERT(!"Write64Sync not supported for lwlink registers");
}

//--------------------------------------------------------------------
//! \brief Write a register field
void LwLinkRegHal::Write64
(
    UINT32          domainIndex,
    RegSpace        space,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT64          value
)
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    const UINT32 regAddress = LookupAddress(ColwertToAddress(field), addressIndexes);
    RegHalDomain regDomain = LookupDomain(ColwertToAddress(field), addressIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    auto pLwLinkRegs = GetTestDevice()->GetInterface<LwLinkRegs>();

    UINT64 regValue = 0;
    if (OK != pLwLinkRegs->RegRd(domainIndex,
                                 regDomain,
                                 regAddress,
                                 &regValue))
    {
        Printf(Tee::PriError, "Failed to read lwlink field %s\n", ColwertToString(field));
        MASSERT(!"Failed to read lwlink field");
    }
    SetField64(&regValue, field, fieldIndexes, value);
    if (OK != pLwLinkRegs->RegWr(domainIndex,
                                 regDomain,
                                 regAddress,
                                 regValue))
    {
        Printf(Tee::PriError, "Failed to write lwlink field %s\n", ColwertToString(field));
        MASSERT(!"Failed to write lwlink field");
    }
}

//--------------------------------------------------------------------
//! \brief Write a register field
void LwLinkRegHal::Write64Sync
(
    UINT32          domainIndex,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT64          value
)
{
    Printf(Tee::PriError, "Write64Sync not supported for lwlink registers\n");
    MASSERT(!"Write64Sync not supported for lwlink registers");
}
