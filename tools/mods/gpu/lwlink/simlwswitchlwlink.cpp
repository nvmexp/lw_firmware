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

#include "simlwswitchlwlink.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_impl.h"

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
/* virtual */ RC SimLwSwitchLwLink::DoGetFabricApertures(vector<FabricAperture> *pFas)
{
    MASSERT(pFas);
    // Return an empty set of fabric addresses.  This will allow switch only fabric
    // configurations
    pFas->clear();
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC SimLwSwitchLwLink::DoGetOutputLinks
(
    UINT32   inputLink
   ,UINT64   fabricBase
   ,DataType dt
   ,set<UINT32>* pPorts
) const
{
    MASSERT((dt == DT_REQUEST) || (dt == DT_RESPONSE));
    RC rc;

    FMLibInterface::PortInfo portInfo = {};
    portInfo.type = GetDevice()->GetType();
    portInfo.nodeId = 0;
    portInfo.physicalId = GetTopologyId();
    portInfo.portNum = inputLink;

    if (dt == DT_REQUEST)
    {
        UINT32 index = static_cast<UINT32>(fabricBase >> GetFabricRegionBits());
        CHECK_RC(TopologyManager::GetFMLibIf()->GetIngressRequestPorts(portInfo, index, pPorts));
    }
    else if (dt == DT_RESPONSE)
    {
        CHECK_RC(TopologyManager::GetFMLibIf()->GetIngressResponsePorts(portInfo, fabricBase, pPorts));
    }

    return rc;
}
