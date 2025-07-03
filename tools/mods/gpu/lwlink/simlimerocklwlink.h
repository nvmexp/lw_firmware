/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "simlwswitchlwlink.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_fabricmanager_libif.h"

//--------------------------------------------------------------------
//! \brief Implementation of LwLink used for simulated Limerock devices
//!
class SimLimerockLwLink : public SimLwSwitchLwLink
{
protected:
    SimLimerockLwLink() = default;
    virtual ~SimLimerockLwLink() = default;

    UINT32 DoGetFabricRegionBits() const override { return 36; }
    UINT32 DoGetLinksPerGroup() const override { return 4; }
    UINT32 DoGetMaxLinkGroups() const override { return 9; }
    UINT32 DoGetMaxLinks() const override { return 36; }
    RC DoGetOutputLinks
    (
        UINT32 inputLink
       ,UINT64 fabricBase
       ,DataType dt
       ,set<UINT32>* pPorts
    ) const override;
};
