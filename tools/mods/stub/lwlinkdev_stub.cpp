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

#include "core/include/script.h"
#include "gpu/utility/lwlink/lwlerrorcollector.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlthroughputcounters.h"
#include "device/interface/lwlink/lwltrex.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"

//-----------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::GetRoutesOnDevice
(
    TestDevicePtr pDevice,
    vector<LwLinkRoutePtr> *pRoutes
)
{
    return RC::OK;
}

RC LwLinkDevIf::TopologyManager::Initialize()
{
    return RC::OK;
}

RC LwLinkDevIf::TopologyManager::Print(Tee::Priority pri)
{
    return RC::OK;
}

RC LwLinkDevIf::TopologyManager::Setup()
{
    return RC::OK;
}

RC LwLinkDevIf::TopologyManager::Shutdown()
{
    return RC::OK;
}

RC LwLinkErrorCollector::Stop()
{
    return RC::OK;
}

string LwLinkThroughputCount::GetString(const Config &tc, string prefix)
{
    return "";
}

RC LwLinkThroughputCounters::ClearThroughputCounters(UINT64 linkMask)
{
    return RC::OK;
}

RC LwLinkThroughputCounters::GetThroughputCounters
(
    UINT64 linkMask
  , map<UINT32, vector<LwLinkThroughputCount>> *pCounts
)
{
    return RC::OK;
}

RC LwLinkThroughputCounters::SetupThroughputCounters
(
    UINT64 linkMask
  , const vector<LwLinkThroughputCount::Config> &configs
)
{
    return RC::OK;
}

RC LwLinkThroughputCounters::StartThroughputCounters(UINT64 linkMask)
{
    return RC::OK;
}

RC LwLinkThroughputCounters::StopThroughputCounters(UINT64 linkMask)
{
    return RC::OK;
}

LwLinkTrex::ReductionOp LwLinkTrex::StringToReductionOp(const string& str)
{
    return RO_ILWALID;
}

vector<LwLinkConnection::EndpointIds> LwLinkRoute::GetEndpointLinks
(
    TestDevicePtr pDev
   ,LwLink::DataType dt
) const
{
    return vector<LwLinkConnection::EndpointIds>();
}

TestDevicePtr LwLinkRoute::GetRemoteEndpoint(TestDevicePtr pLocEndpoint) const
{
    return TestDevicePtr();
}

bool LwLinkRoute::IsLoop() const
{
    return false;
}

bool LwLinkRoute::IsP2P() const
{
    return false;
}

bool LwLinkRoute::UsesEndpoints(LwLinkConnection::EndpointDevices endpoints) const
{
    return false;
}

//------------------------------------------------------------------------------
// LwLinkDev constants
//-----------------------------------------------------------------------------
JS_CLASS(LwLinkDevConst);
static SObject LwLinkDevConst_Object
(
    "LwLinkDevConst",
    LwLinkDevConstClass,
    0,
    0,
    "LwLinkDevConst JS Object"
);

PROP_CONST(LwLinkDevConst, TYPE_EBRIDGE, TestDevice::TYPE_EBRIDGE);
PROP_CONST(LwLinkDevConst, TYPE_IBM_NPU, TestDevice::TYPE_IBM_NPU);
PROP_CONST(LwLinkDevConst, TYPE_LWIDIA_GPU, TestDevice::TYPE_LWIDIA_GPU);
PROP_CONST(LwLinkDevConst, TYPE_LWIDIA_LWSWITCH, TestDevice::TYPE_LWIDIA_LWSWITCH);
PROP_CONST(LwLinkDevConst, TYPE_UNKNOWN, TestDevice::TYPE_UNKNOWN);
PROP_CONST(LwLinkDevConst, HWD_NONE,            0);
PROP_CONST(LwLinkDevConst, HWD_EXPLORER_NORMAL, 0);
PROP_CONST(LwLinkDevConst, HWD_EXPLORER_ILWERT, 0);
PROP_CONST(LwLinkDevConst, TM_DEFAULT, LwLinkDevIf::TM_DEFAULT);
PROP_CONST(LwLinkDevConst, TM_MIN,     LwLinkDevIf::TM_MIN);
PROP_CONST(LwLinkDevConst, TM_FILE,    LwLinkDevIf::TM_FILE);
PROP_CONST(LwLinkDevConst, TREX_DATA_ALL_ZEROS, LwLinkTrex::DP_ALL_ZEROS);
PROP_CONST(LwLinkDevConst, TREX_DATA_ALL_ONES,  LwLinkTrex::DP_ALL_ONES);
PROP_CONST(LwLinkDevConst, TREX_DATA_0_F,       LwLinkTrex::DP_0_F);
PROP_CONST(LwLinkDevConst, TREX_DATA_A_5,       LwLinkTrex::DP_A_5);
PROP_CONST(LwLinkDevConst, TREX_SRC_EGRESS,  LwLinkTrex::SRC_EGRESS);
PROP_CONST(LwLinkDevConst, TREX_SRC_RX,      LwLinkTrex::SRC_RX);
PROP_CONST(LwLinkDevConst, TREX_DST_INGRESS, LwLinkTrex::DST_INGRESS);
PROP_CONST(LwLinkDevConst, TREX_DST_TX,      LwLinkTrex::DST_TX);
