/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlbws_trex_test_route.h"

#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "gpu/include/testdevice.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/utility/lwlink/lwlinkdev.h"

#include <cmath>
#include <algorithm>

using namespace LwLinkDataTestHelper;

RC TestRouteTrex::CheckLinkState()
{
    LwLinkRoute::PathIterator pPath = GetRoute()->PathsBegin(GetLocalLwLinkDev(), LwLink::DT_REQUEST);
    LwLinkRoute::PathIterator pathEnd = GetRoute()->PathsEnd(GetLocalLwLinkDev(), LwLink::DT_REQUEST);

    StickyRC rc;
    UINT32 pathIdx = 0;
    for (; pPath != pathEnd; ++pPath, pathIdx++)
    {
        LwLinkPath::PathEntryIterator pPe = pPath->PathEntryBegin(GetLocalLwLinkDev());
        LwLinkPath::PathEntryIterator peEnd = LwLinkPath::PathEntryIterator();
        UINT32 depth = 0;
        for (; pPe != peEnd; ++pPe, depth++)
        {
            vector<LwLink::LinkStatus> ls;
            CHECK_RC(pPe->pSrcDev->GetInterface<LwLink>()->GetLinkStatus(&ls));

            string s = Utility::StrPrintf("Link status on Path %u, Depth %u, %s\n",
                                          pathIdx,
                                          depth,
                                          pPe->pSrcDev->GetName().c_str());

            TestDevicePtr pRemoteDev = pPe->pCon->GetRemoteDevice(pPe->pSrcDev);
            s += "---------------------------------------------------\n";
            s += Utility::StrPrintf("  Remote Device  = %s\n",
                                    pRemoteDev->GetName().c_str());

            if (pRemoteDev->GetType() == TestDevice::TYPE_TREX)
                continue;

            vector<LwLinkConnection::EndpointIds> eids = pPe->pCon->GetLinks(pPe->pSrcDev);
            for (size_t ii = 0; ii < eids.size(); ii++)
            {
                MASSERT(eids[ii].first < ls.size());
                LwLinkPowerState::LinkPowerStateStatus lpws;
                if (
                    pPe->pSrcDev->SupportsInterface<LwLinkPowerState>() &&
                    LwLink::LS_ILWALID != ls[eids[ii].first].linkState
                )
                {
                    auto powerState = pPe->pSrcDev->GetInterface<LwLinkPowerState>();
                    CHECK_RC(powerState->GetLinkPowerStateStatus(eids[ii].first, &lpws));
                }
                s +=
                    Utility::StrPrintf("  Link %2u        = %s (RX : (%s, %s), TX : (%s, %s), "
                                       "Remote Link %u\n",
                        eids[ii].first,
                        LwLink::LinkStateToString(ls[eids[ii].first].linkState).c_str(),
                        LwLink::SubLinkStateToString(ls[eids[ii].first].rxSubLinkState).c_str(),
                        LwLinkPowerState::SubLinkPowerStateToString(lpws.rxSubLinkLwrrentPowerState).c_str(), //$
                        LwLink::SubLinkStateToString(ls[eids[ii].first].txSubLinkState).c_str(),
                        LwLinkPowerState::SubLinkPowerStateToString(lpws.txSubLinkLwrrentPowerState).c_str(), //$
                        eids[ii].second);

                // High speed, single lane, and sleep are all acceptable since single lane or sleep indicate
                // that the link trained to HS mode but went back to low power due to no traffic
                const bool validLS = ls[eids[ii].first].linkState == LwLink::LS_ACTIVE ||
                                     ls[eids[ii].first].linkState == LwLink::LS_SLEEP;
                const bool validRxSLS = ls[eids[ii].first].rxSubLinkState == LwLink::SLS_HIGH_SPEED ||
                                        ls[eids[ii].first].rxSubLinkState == LwLink::SLS_SINGLE_LANE ||
                                        (ls[eids[ii].first].rxSubLinkState == LwLink::SLS_OFF &&
                                         ls[eids[ii].first].linkState == LwLink::LS_SLEEP);
                const bool validTxSLS = ls[eids[ii].first].txSubLinkState == LwLink::SLS_HIGH_SPEED ||
                                        ls[eids[ii].first].txSubLinkState == LwLink::SLS_SINGLE_LANE ||
                                        (ls[eids[ii].first].txSubLinkState == LwLink::SLS_OFF &&
                                         ls[eids[ii].first].linkState == LwLink::LS_SLEEP);
                if (!validLS || !validRxSLS || !validTxSLS)
                {
                    rc = RC::LWLINK_BUS_ERROR;
                }
            }

            if (OK != rc)
                s += "******* Invalid link state found! *******\n";

            Printf((OK != rc) ? Tee::PriError : GetPrintPri(), "%s\n", s.c_str());
        }
    }
    return rc;
}
