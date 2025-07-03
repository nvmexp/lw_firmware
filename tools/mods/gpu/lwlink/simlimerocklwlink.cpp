/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "simlimerocklwlink.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_impl.h"

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
/* virtual */ RC SimLimerockLwLink::DoGetOutputLinks
(
    UINT32   inputLink
   ,UINT64   fabricBase
   ,DataType dt
   ,set<UINT32>* pPorts
) const
{
    RC rc;

    FMLibInterface::PortInfo portInfo = {};
    portInfo.type = GetDevice()->GetType();
    portInfo.nodeId = 0;
    portInfo.physicalId = GetTopologyId();
    portInfo.portNum = inputLink;

    UINT32 index = static_cast<UINT32>(fabricBase >> GetFabricRegionBits());

    CHECK_RC(TopologyManager::GetFMLibIf()->GetRidPorts(portInfo, index, pPorts));

    return rc;
}
