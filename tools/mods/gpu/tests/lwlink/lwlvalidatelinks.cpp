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

#include "core/include/script.h"
#include "core/include/mle_protobuf.h"
#include "device/interface/lwlink/lwlcci.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "lwlinktest.h"


class LwlValidateLinks : public LwLinkTest
{
public:
    LwlValidateLinks();
    virtual ~LwlValidateLinks() {}

    RC Setup() override;
    RC RunTest() override;
    RC InitFromJs() override;
    void PrintJsProperties(Tee::Priority pri) override;

private:
    UINT64 m_LinkMaskOverride = 0;

    enum
    {
        LINK_ERR_NONE          = 0
       ,LINK_ERR_NOSTATUS      = (1 << 0)
       ,LINK_ERR_LINK_STATE    = (1 << 1)
       ,LINK_ERR_RX_SLS        = (1 << 2)
       ,LINK_ERR_TX_SLS        = (1 << 3)
       ,LINK_ERR_LINK_NOT_OFF  = (1 << 4)
    };

    void PrintLinkState(TestDevicePtr pTestDevice, const vector<LwLink::LinkStatus> & linkStatus);
    RC ValidateLinks(TestDevicePtr pTestDevice, UINT64 expectedHslinkMask);
};

// -----------------------------------------------------------------------------
LwlValidateLinks::LwlValidateLinks()
{
    SetName("LwlValidateLinks");
}

// -----------------------------------------------------------------------------
RC LwlValidateLinks::Setup()
{
    RC rc;
    CHECK_RC(LwLinkTest::Setup());
    return rc;
}

// -----------------------------------------------------------------------------
RC LwlValidateLinks::RunTest()
{
    RC rc;
    UINT64 linksToValidate = m_LinkMaskOverride;
    if (!linksToValidate)
    {
        // Use topology manager to determine the links to test.  This will result in
        // non-switch topologies expecting that all discovered links are trained
        // (i.e. all links not disabled by fusing, floorsweeping or vbios link disable)
        CHECK_RC(LwLinkDevIf::TopologyManager::GetExpectedLinkMask(GetBoundTestDevice(),
                                                                   &linksToValidate));
    }

    auto * pLwLink = GetBoundTestDevice()->GetInterface<LwLink>();
    const UINT64 maxLinkMask = pLwLink->GetMaxLinkMask();
    if ((maxLinkMask & linksToValidate) != linksToValidate)
    {
        Printf(Tee::PriError, "Link mask to test = 0x%llx is invalid. Max link mask supported is 0x%llx\n",
            linksToValidate, maxLinkMask);
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(ValidateLinks(GetBoundTestDevice(), linksToValidate));

    if (pLwLink->SupportsInterface<LwLinkCableController>())
    {
        auto * pLwlCC = pLwLink->GetInterface<LwLinkCableController>();
        UINT64 ccLinkMask = 0;
        for (UINT32 lwrCC = 0; lwrCC < pLwlCC->GetCount(); lwrCC++)
        {
            ccLinkMask |= pLwlCC->GetLinkMask(lwrCC);
        }
        if (ccLinkMask != pLwlCC->GetBiosLinkMask())
        {
            Printf(Tee::PriError,
                   "Driver CCI link mask (0x%llx) doesnt match BIOS CCI link mask (0x%llx)\n",
                   ccLinkMask, pLwlCC->GetBiosLinkMask());
            return RC::LWLINK_BUS_ERROR;
        }
    }
    return rc;
}

// -----------------------------------------------------------------------------
RC LwlValidateLinks::InitFromJs()
{
    RC rc;
    CHECK_RC(LwLinkTest::InitFromJs());

    JavaScriptPtr pJs;
    string sLinkMaskOverride;

    rc = pJs->GetProperty(GetJSObject(), "LinkMaskOverride", &sLinkMaskOverride);
    if (rc == RC::ILWALID_OBJECT_PROPERTY)
        return RC::OK;

    char * pEndPtr;
    m_LinkMaskOverride = Utility::Strtoull(sLinkMaskOverride.c_str(), &pEndPtr, 0);
    if (*pEndPtr != '\0')
    {
        Printf(Tee::PriError,
               "%s : Invalid link mask string %s\n",
               GetName().c_str(), sLinkMaskOverride.c_str());
        return RC::ILWALID_ARGUMENT;
    }

    return rc;
}

// -----------------------------------------------------------------------------
void LwlValidateLinks::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tLinkMaskOverride:   0x%llx\n", m_LinkMaskOverride);
    GpuTest::PrintJsProperties(pri);
}

// -----------------------------------------------------------------------------
void LwlValidateLinks::PrintLinkState
(
    TestDevicePtr pTestDevice,
    const vector<LwLink::LinkStatus> & linkStatus
)
{
    string linkStatusStr = Utility::StrPrintf("%s : Link status for %s\n",
                                              GetName().c_str(),
                                              pTestDevice->GetName().c_str());
    auto pLwLink = pTestDevice->GetInterface<LwLink>();

    for (size_t lwrLink = 0; lwrLink < linkStatus.size(); ++lwrLink)
    {
        linkStatusStr += Utility::StrPrintf("  Link %2u : ", static_cast<UINT32>(lwrLink));
        if (!pLwLink->IsLinkActive(lwrLink))
        {
            linkStatusStr += "Inactive\n";
            continue;
        }

        linkStatusStr += Utility::StrPrintf("Link State = %s, RX SLS = %s, TX SLS = %s\n",
            LwLink::LinkStateToString(linkStatus[lwrLink].linkState).c_str(),
            LwLink::SubLinkStateToString(linkStatus[lwrLink].rxSubLinkState).c_str(),
            LwLink::SubLinkStateToString(linkStatus[lwrLink].txSubLinkState).c_str());
    }
    VerbosePrintf("%s\n", linkStatusStr.c_str());
}

// -----------------------------------------------------------------------------
RC LwlValidateLinks::ValidateLinks(TestDevicePtr pTestDevice, UINT64 expectedHslinkMask)
{
    RC rc;
    const FLOAT64 timeoutMs = GetTestConfiguration()->TimeoutMs();

    vector<LwLink::LinkStatus> origLinkStatus;

    auto pLwLink = pTestDevice->GetInterface<LwLink>();
    CHECK_RC(pLwLink->GetLinkStatus(&origLinkStatus));

    LwLinkPowerState::Owner psOwner(pTestDevice->GetInterface<LwLinkPowerState>());
    if (pTestDevice->SupportsInterface<LwLinkPowerState>())
    {
        CHECK_RC(psOwner.Claim(LwLinkPowerState::ClaimState::FULL_BANDWIDTH));
    }

    auto restoreLinkStatus = [&]() -> RC
    {
        return psOwner.Release();
    };

    DEFERNAME(deferRestoreLinkStatus)
    {
        restoreLinkStatus();
    };

    // Force all links to HS mode for the purpose of this test (i.e. do not allow
    // them to be in single lane mode
    for (UINT32 link = 0; link < pLwLink->GetMaxLinks(); link++)
    {
        if (!pLwLink->IsLinkActive(link) || !pTestDevice->SupportsInterface<LwLinkPowerState>())
            continue;
        auto powerState = pTestDevice->GetInterface<LwLinkPowerState>();
        Tasker::PollHw(timeoutMs, [&]() -> bool
        {
            LwLinkPowerState::LinkPowerStateStatus ps;
            powerState->GetLinkPowerStateStatus(link, &ps);
            return ps.rxSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB &&
                   ps.txSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB;
        }, __FUNCTION__);
    }

    vector<LwLink::LinkStatus> linkStatus;
    CHECK_RC(pLwLink->GetLinkStatus(&linkStatus));
    INT32 lwrLink = Utility::BitScanForward(expectedHslinkMask);

    PrintLinkState(pTestDevice, linkStatus);

    // Verify that any links expected to be in HS mode are in HS mode
    vector<UINT32> linkErrs(pLwLink->GetMaxLinks(), 0);
    while (lwrLink != -1)
    {
        if (static_cast<size_t>(lwrLink) >= linkStatus.size())
        {
            linkErrs[lwrLink] = LINK_ERR_NOSTATUS;
            break;
        }

        if (linkStatus[lwrLink].linkState != LwLink::LS_ACTIVE)
            linkErrs[lwrLink] |= LINK_ERR_LINK_STATE;

        if (linkStatus[lwrLink].rxSubLinkState != LwLink::SLS_HIGH_SPEED)
            linkErrs[lwrLink] |= LINK_ERR_RX_SLS;

        if (linkStatus[lwrLink].txSubLinkState != LwLink::SLS_HIGH_SPEED)
            linkErrs[lwrLink] |= LINK_ERR_TX_SLS;
        lwrLink = Utility::BitScanForward(expectedHslinkMask, lwrLink + 1);
    }

    // Verify that any links not expected to be in HS mode are actually inactive
    const UINT64 inactiveLinks = ~expectedHslinkMask & pLwLink->GetMaxLinkMask();
    lwrLink = Utility::BitScanForward(inactiveLinks);
    while (lwrLink != -1)
    {
        if (static_cast<size_t>(lwrLink) >= linkStatus.size())
        {
            linkErrs[lwrLink] = LINK_ERR_NOSTATUS;
            break;
        }

        if (pLwLink->IsLinkActive(lwrLink))
            linkErrs[lwrLink] |= LINK_ERR_LINK_NOT_OFF;
        lwrLink = Utility::BitScanForward(inactiveLinks, lwrLink + 1);
    }

    string linkErrStr = "";
    for (size_t lwrLink = 0; lwrLink < linkErrs.size(); ++lwrLink)
    {
        if (linkErrs[lwrLink] != 0)
        {
            if (linkErrStr == "")
            {
                linkErrStr += Utility::StrPrintf("%s : LwLink validation failed on %s\n",
                                                 GetName().c_str(),
                                                 pTestDevice->GetName().c_str());
            }
            if (linkErrs[lwrLink] & LINK_ERR_NOSTATUS)
            {
                linkErrStr += Utility::StrPrintf("  Link %u : No link status found\n",
                                                 static_cast<UINT32>(lwrLink));
            }
            if (linkErrs[lwrLink] & LINK_ERR_LINK_STATE)
            {
                linkErrStr +=
                    Utility::StrPrintf("  Link %u : Invalid link state, expected Active, "
                                       "found %s\n",
                        static_cast<UINT32>(lwrLink),
                        LwLink::LinkStateToString(linkStatus[lwrLink].linkState).c_str());
            }
            if (linkErrs[lwrLink] & LINK_ERR_RX_SLS)
            {
                linkErrStr +=
                    Utility::StrPrintf("  Link %u : Invalid receive sublink state, expected "
                                       "High Speed, found %s\n",
                        static_cast<UINT32>(lwrLink),
                        LwLink::SubLinkStateToString(linkStatus[lwrLink].rxSubLinkState).c_str());
            }
            if (linkErrs[lwrLink] & LINK_ERR_TX_SLS)
            {
                linkErrStr +=
                    Utility::StrPrintf("  Link %u : Invalid transmit sublink state, expected "
                                       "High Speed, found %s\n",
                        static_cast<UINT32>(lwrLink),
                        LwLink::SubLinkStateToString(linkStatus[lwrLink].txSubLinkState).c_str());
            }

            if (linkErrs[lwrLink] & LINK_ERR_LINK_NOT_OFF)
            {
                linkErrStr +=
                    Utility::StrPrintf("  Link %u : Expected link to be disabled, but link was active\n",
                        static_cast<UINT32>(lwrLink));
            }
            else if (pLwLink->SupportsInterface<LwLinkCableController>())
            {
                auto pLwlCC = pLwLink->GetInterface<LwLinkCableController>();
                const UINT32 cciId = pLwlCC->GetCciIdFromLink(lwrLink);
                if (cciId != LwLinkCableController::ILWALID_CCI_ID)
                {
                    vector<LwLinkCableController::GradingValues> gradingVals;
                    CHECK_RC(pLwlCC->GetGradingValues(cciId, &gradingVals));
                    linkErrStr += 
                        LwLinkCableController::LinkGradingValuesToString(lwrLink, gradingVals);
                    
                    SCOPED_DEV_INST(pTestDevice.get());
                    ByteStream bs;
                    auto entry = Mle::Entry::lwlink_mismatch(&bs);
                    
                    {
                        // todo: using the port mismatch to unblock CR
                        // The actual issue here is failing to go to HS
                        auto portMismatch = entry.lwswitch_port_mismatch();
                        portMismatch.reason(Mle::LwlinkMismatch::MismatchReason::port_mismatch)
                                    .lwlink_id(lwrLink);

                        for (auto const & lwrGrading : gradingVals)
                        {
                            if ((lwrGrading.linkId == lwrLink) && (lwrGrading.laneMask != 0))
                            {
                                const INT32 firstLane =
                                    Utility::BitScanForward(lwrGrading.laneMask);
                                const INT32 lastLane =
                                    Utility::BitScanReverse(lwrGrading.laneMask);
                                for (INT32 lane = firstLane; lane <= lastLane; ++lane)
                                {
                                    portMismatch.cc_lane_gradings()
                                                .lane_id(lane)
                                                .rx_init(lwrGrading.rxInit[lane])
                                                .tx_init(lwrGrading.txInit[lane])
                                                .rx_maint(lwrGrading.rxMaint[lane])
                                                .tx_maint(lwrGrading.txMaint[lane]);
                                }                                
                            }
                        }
                    }

                    entry.Finish();
                    Tee::PrintMle(&bs);
                } 
            }
        }
        else
        {
            VerbosePrintf("%s : LwLink validation succeeded on on %s, link %zu\n",
                          GetName().c_str(),
                          pTestDevice->GetName().c_str(),
                          lwrLink);
        }
    }
    if (linkErrStr != "")
    {
        Printf(Tee::PriError, "%s", linkErrStr.c_str());
    }

    deferRestoreLinkStatus.Cancel();
    CHECK_RC(restoreLinkStatus());

    return (rc == RC::OK) && (linkErrStr != "") ? RC::LWLINK_BUS_ERROR : rc;
}

JS_CLASS_INHERIT(LwlValidateLinks, LwLinkTest, "LwlValidateLinks");

