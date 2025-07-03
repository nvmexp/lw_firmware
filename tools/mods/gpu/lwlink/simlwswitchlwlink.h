/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "simlwlink.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_fabricmanager_libif.h"

//--------------------------------------------------------------------
//! \brief Implementation of LwLink used for simulated Willow devices
//!
class SimLwSwitchLwLink : public SimLwLink
{
protected:
    SimLwSwitchLwLink() {}
    virtual ~SimLwSwitchLwLink() {}

    RC DoGetFabricApertures(vector<FabricAperture> *pFas) override;
    UINT32 DoGetFabricRegionBits() const override { return 34; }
    string DoGetLinkGroupName() const override { return "LWLW"; }
    UINT32 DoGetMaxLinks() const override { return 18; }
    RC DoGetOutputLinks
    (
        UINT32 inputLink
       ,UINT64 fabricBase
       ,DataType dt
       ,set<UINT32>* pPorts
    ) const override;
    RC DoSetFabricApertures
    (
        const vector<FabricAperture> &fas
    ) override { return RC::UNSUPPORTED_FUNCTION; }
};
