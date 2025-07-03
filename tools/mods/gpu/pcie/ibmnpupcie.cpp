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

#include "ibmnpupcie.h"

#include "device/interface/lwlink.h"

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
IbmNpuPcie::IbmNpuPcie()
{
}

//-----------------------------------------------------------------------------
RC IbmNpuPcie::DoGetErrorCounts
(
    PexErrorCounts* pLocCounts,
    PexErrorCounts* pHostCounts,
    PexDevice::PexCounterType countType
)
{
    MASSERT(pLocCounts);
    MASSERT(pHostCounts);
    pLocCounts->Reset();
    pHostCounts->Reset();
    return OK;
}
