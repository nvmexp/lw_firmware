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

#include "core/include/mgrmgr.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwlink/lwlinktest.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"

#include "core/include/js_uint64.h"

class GpuTestConfiguration;
class Goldelwalues;
class GpuSubdevice;

//-----------------------------------------------------------------------------
class LwLinkState : public LwLinkTest
{
public:
    LwLinkState();
    virtual ~LwLinkState() { }
    virtual RC Setup();
    virtual RC Cleanup();
    virtual bool IsSupported();

    virtual RC RunTest();

    void PrintJsProperties(Tee::Priority pri);

    void SetChangeStateLinkMask(UINT64 linkMask) { m_ChangeStateLinkMask = linkMask; }
    void SetForceHsLinkMask(UINT64 linkMask)     { m_ForceHsLinkMask     = linkMask; }
    void SetForceSafeLinkMask(UINT64 linkMask)   { m_ForceSafeLinkMask   = linkMask; }
    void SetForceOffLinkMask(UINT64 linkMask)       { m_ForceSafeLinkMask   = linkMask; }


    SETGET_PROP(EnableSafeTransitions,     bool);
    GET_PROP(EnableOffTransitions,         bool);
    SET_PROP_LWSTOM(EnableOffTransitions,  bool);
    SETGET_PROP(SafeSleepMs,               UINT32);
    SETGET_PROP(OffSleepMs,                UINT32);
    SETGET_PROP(HsSleepMs,                 UINT32);

private:
    //! JS variables (see JS interfaces for description)
    UINT64  m_ChangeStateLinkMask         = ~0ULL;
    UINT64  m_ForceHsLinkMask             = 0ULL;
    UINT64  m_ForceSafeLinkMask           = 0ULL;
    UINT64  m_ForceOffLinkMask            = 0ULL;
    bool    m_EnableSafeTransitions       = true;
    bool    m_EnableOffTransitions        = true;
    bool    m_EnableOffTransitionsFromJs  = false;
    UINT32  m_SafeSleepMs                 = 0U;
    UINT32  m_OffSleepMs                  = 0U;
    UINT32  m_HsSleepMs                   = 0U;

    vector<LwLink::SubLinkState> m_LinkStateChanges;
    vector<LwLink::LinkStatus>   m_OrigLinkStatus;
    LwLinkPowerState::Owner      m_PowerStateOwner;

    RC SetStateOnLinks(UINT64 linkMask, LwLink::SubLinkState state);
};

//-----------------------------------------------------------------------------
LwLinkState::LwLinkState()
{
    SetName("LwLinkState");
}

//-----------------------------------------------------------------------------
RC LwLinkState::SetEnableOffTransitions(bool bEnable)
{
    m_EnableOffTransitions = bEnable;
    m_EnableOffTransitionsFromJs = true;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkState::Setup()
{
    RC rc;

    CHECK_RC(LwLinkTest::Setup());

    if ((m_ChangeStateLinkMask & m_ForceHsLinkMask) ||
        (m_ChangeStateLinkMask & m_ForceSafeLinkMask) ||
        (m_ChangeStateLinkMask & m_ForceOffLinkMask) ||
        (m_ForceHsLinkMask & m_ForceSafeLinkMask) ||
        (m_ForceHsLinkMask & m_ForceOffLinkMask) ||
        (m_ForceSafeLinkMask & m_ForceOffLinkMask))
    {
        Printf(Tee::PriError, "%s : Links masks overlap!\n", GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    TestDevicePtr pDev = GetBoundTestDevice();

    if (!pDev->HasFeature(Device::GPUSUB_SUPPORTS_LWLINK_OFF_TRANSITION))
    {
        if (m_ForceOffLinkMask || (m_EnableOffTransitionsFromJs && m_EnableOffTransitions))
        {
            Printf(Tee::PriError, "%s : %s does not support off transitions!\n",
                   GetName().c_str(),
                   pDev->GetName().c_str());
            return RC::BAD_PARAMETER;
        }

        if (m_EnableOffTransitions)
        {
            VerbosePrintf("%s : %s does not support off transitions, disabling\n",
                          GetName().c_str(), pDev->GetName().c_str());
            m_EnableOffTransitions = false;
        }
    }

    const bool bRequiresOffAfterHsSafe =
        !GetBoundTestDevice()->HasFeature(Device::GPUSUB_SUPPORTS_LWLINK_HS_SAFE_CYCLING);
    if ((!m_EnableSafeTransitions || bRequiresOffAfterHsSafe) && !m_EnableOffTransitions)
    {
        Printf(Tee::PriError, "%s : No link state transitions possible!\n", GetName().c_str());
        return RC::NO_TESTS_RUN;
    }

    if (m_EnableOffTransitions && GetBoundGpuSubdevice())
    {
        // Transitioning to OFF ilwalidates all peer mappings, so remove all peer mappings
        // associated with the GPU under test
        CHECK_RC(DevMgrMgr::d_GraphDevMgr->RemoveAllPeerMappings(GetBoundGpuSubdevice()));
    }

    UINT64 sysmemLinkMask = 0;
    vector<LwLinkRoutePtr> routes;
    CHECK_RC(LwLinkDevIf::TopologyManager::GetRoutesOnDevice(GetBoundTestDevice(), &routes));
    for (auto& route : routes)
    {
        if (route->IsSysmem())
        {
            vector<LwLinkConnection::EndpointIds> endpointIds =
                route->GetEndpointLinks(pDev, LwLink::DT_ALL);
            for (auto lwrEpId : endpointIds)
                sysmemLinkMask |= 1ULL << lwrEpId.first;
        }
    }

    auto pLwLink = pDev->GetInterface<LwLink>();
    CHECK_RC(pLwLink->GetLinkStatus(&m_OrigLinkStatus));

    UINT64 activeLinkMask = 0;
    for (size_t lwrId = 0; lwrId < m_OrigLinkStatus.size(); lwrId++)
    {
        if (pLwLink->IsLinkActive(lwrId) && !(sysmemLinkMask & (1ULL << lwrId)))
        {
            activeLinkMask |= (1ULL << lwrId);

            LwLink::EndpointInfo epInfo;

            // Loopout links require that the bits for both ends of the links
            // be set or clear in the same mask together.  When forcing topolo
            if ((OK == pLwLink->GetRemoteEndpoint(lwrId, &epInfo)) &&
                (epInfo.id == pDev->GetId()) && (epInfo.linkNumber != lwrId))
            {
                if (((m_ChangeStateLinkMask & (1ULL << lwrId)) &&
                     !(m_ChangeStateLinkMask & (1ULL << epInfo.linkNumber))) ||
                    ((m_ForceHsLinkMask & (1ULL << lwrId)) &&
                     !(m_ForceHsLinkMask & (1ULL << epInfo.linkNumber))) ||
                    ((m_ForceSafeLinkMask & (1ULL << lwrId)) &&
                     !(m_ForceSafeLinkMask & (1ULL << epInfo.linkNumber))) ||
                    ((m_ForceOffLinkMask & (1ULL << lwrId)) &&
                     !(m_ForceOffLinkMask & (1ULL << epInfo.linkNumber))))
                {
                    Printf(Tee::PriError,
                           "%s : Loopout link %u not exercised the same as its remote link %u\n",
                           GetName().c_str(),
                           static_cast<UINT32>(lwrId),
                           epInfo.linkNumber);
                    return RC::BAD_PARAMETER;
                }
            }
        }
    }
    if (!(m_ChangeStateLinkMask & activeLinkMask))
    {
        Printf(Tee::PriError, "%s : Links masks overlap!\n", GetName().c_str());
        return RC::NO_TESTS_RUN;
    }

    m_ChangeStateLinkMask &= activeLinkMask;
    m_ForceHsLinkMask     &= activeLinkMask;
    m_ForceSafeLinkMask   &= activeLinkMask;
    m_ForceOffLinkMask    &= activeLinkMask;

    if (pDev->SupportsInterface<LwLinkPowerState>())
    {
        m_PowerStateOwner = LwLinkPowerState::Owner(pDev->GetInterface<LwLinkPowerState>());
        CHECK_RC(m_PowerStateOwner.Claim(LwLinkPowerState::ClaimState::FULL_BANDWIDTH));
    }

    CHECK_RC(SetStateOnLinks(m_ForceHsLinkMask,   LwLink::SLS_HIGH_SPEED));
    CHECK_RC(SetStateOnLinks(m_ForceSafeLinkMask, LwLink::SLS_SAFE_MODE));
    CHECK_RC(SetStateOnLinks(m_ForceOffLinkMask,  LwLink::SLS_OFF));

    VerbosePrintf("%s : Actual link masks tested on %s\n"
                  "    ChangeStateLinkMask : 0x%llx\n"
                  "    SysmemLinkMask      : 0x%llx (ignored links)\n"
                  "    ForceHsLinkMask     : 0x%llx\n"
                  "    ForceSafeLinkMask   : 0x%llx\n"
                  "    ForceOffLinkMask    : 0x%llx\n",
                  GetName().c_str(),
                  pDev->GetName().c_str(),
                  m_ChangeStateLinkMask,
                  sysmemLinkMask,
                  m_ForceHsLinkMask,
                  m_ForceSafeLinkMask,
                  m_ForceOffLinkMask);


    const UINT32 totalLoops = GetTestConfiguration()->Loops();
    LwLink::SubLinkState prevState  = LwLink::SLS_HIGH_SPEED;
    LwLink::SubLinkState lwrState  = LwLink::SLS_HIGH_SPEED;
    LwLink::SubLinkState nextState = LwLink::SLS_ILWALID;
    Random & rand = GetFpContext()->Rand;
    for (UINT32 loop = 0; loop < totalLoops; loop++)
    {
        if (!m_EnableOffTransitions)
        {
            nextState = (lwrState == LwLink::SLS_HIGH_SPEED) ? LwLink::SLS_SAFE_MODE :
                                                               LwLink::SLS_HIGH_SPEED;
        }
        else if (!m_EnableSafeTransitions)
        {
            nextState = (lwrState == LwLink::SLS_HIGH_SPEED) ? LwLink::SLS_OFF :
                                                               LwLink::SLS_HIGH_SPEED;
        }
        else
        {
            // Current random generator exhibits very poor bit 0 randomness
            const bool bRandBool = ((rand.GetRandom() >> 16) % 2) == 1;
            switch (lwrState)
            {
                case LwLink::SLS_HIGH_SPEED:
                    nextState =  bRandBool ? LwLink::SLS_SAFE_MODE : LwLink::SLS_OFF;
                    break;
                case LwLink::SLS_SAFE_MODE:
                    if ((prevState == LwLink::SLS_HIGH_SPEED) && bRequiresOffAfterHsSafe)
                    {
                        nextState = LwLink::SLS_OFF;
                    }
                    else
                    {
                        nextState =  bRandBool ? LwLink::SLS_HIGH_SPEED : LwLink::SLS_OFF;
                    }
                    break;
                case LwLink::SLS_OFF:
                    nextState =  bRandBool ? LwLink::SLS_SAFE_MODE : LwLink::SLS_HIGH_SPEED;
                    break;
                default:
                    Printf(Tee::PriError, "%s : Invalid link state %d!\n",
                           GetName().c_str(), lwrState);
                    return RC::SOFTWARE_ERROR;
            }
        }
        m_LinkStateChanges.push_back(nextState);
        prevState = lwrState;
        lwrState  = nextState;
    }

    // Insert an OFF transition if the link would not be able to get back to HS mode in
    // Cleanup with a simple directed change to HS mode
    if (bRequiresOffAfterHsSafe &&
        (prevState == LwLink::SLS_HIGH_SPEED) &&
        (lwrState == LwLink::SLS_SAFE_MODE))
    {
        m_LinkStateChanges.push_back(LwLink::SLS_OFF);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkState::RunTest()
{
    RC rc;
    const bool bStopOnError = GetGoldelwalues()->GetStopOnError();

    for (size_t loop = 0; loop < m_LinkStateChanges.size(); loop++)
    {
        rc = SetStateOnLinks(m_ChangeStateLinkMask, m_LinkStateChanges[loop]);
        if ((rc != OK) && !bStopOnError)
        {
            SetDeferredRc(rc);
            rc.Clear();
        }
        CHECK_RC(rc);

        switch (m_LinkStateChanges[loop])
        {
            case LwLink::SLS_HIGH_SPEED:
                Tasker::Sleep(m_HsSleepMs);
                break;
            case LwLink::SLS_SAFE_MODE:
                Tasker::Sleep(m_SafeSleepMs);
                break;
            case LwLink::SLS_OFF:
                Tasker::Sleep(m_OffSleepMs);
                break;
            default:
                Printf(Tee::PriError, "%s : Invalid link state %d!\n",
                       GetName().c_str(), m_LinkStateChanges[loop]);
                return RC::SOFTWARE_ERROR;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkState::Cleanup()
{
    StickyRC rc;

    TestDevicePtr pDev = GetBoundTestDevice();
    for (size_t lwrId = 0; lwrId < m_OrigLinkStatus.size(); lwrId++)
    {
        LwLink::SubLinkState sls = m_OrigLinkStatus[lwrId].rxSubLinkState;
        if ((sls == LwLink::SLS_ILWALID) || (sls == LwLink::SLS_TRAINING))
            continue;

        if (sls == LwLink::SLS_SINGLE_LANE || m_OrigLinkStatus[lwrId].linkState == LwLink::LS_SLEEP)
            sls = LwLink::SLS_HIGH_SPEED;

        rc = pDev->GetInterface<LwLink>()->SetLinkState(lwrId, sls);
    }

    // Restore the original power state configuration
    if (pDev->SupportsInterface<LwLinkPowerState>())
    {
        CHECK_RC(m_PowerStateOwner.Release());
    }

    return rc;
}

//-----------------------------------------------------------------------------
bool LwLinkState::IsSupported()
{
    if (!LwLinkTest::IsSupported())
        return false;

    vector<LwLinkRoutePtr> routes;

    // Allow test to run if this fails - which will subsequently fail
    if (OK != LwLinkDevIf::TopologyManager::GetRoutesOnDevice(GetBoundTestDevice(), &routes))
        return true;

    bool bAnyNonSysmemRoute = false;
    // Taking down sysmem links at runtime is a recipe for failure
    for (auto& route : routes)
    {
        if (!route->IsSysmem())
        {
            bAnyNonSysmemRoute = true;
            break;
        }
    }

    if (routes.size() == 0)
        bAnyNonSysmemRoute = true;

    if (!bAnyNonSysmemRoute)
    {
        Printf(Tee::PriLow, "Skipping %s, all connections are sysmem links\n", GetName().c_str());
    }
    return bAnyNonSysmemRoute;
}

void LwLinkState::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tChangeStateLinkMask   :  0x%llx\n", m_ChangeStateLinkMask);
    Printf(pri, "\tForceHsLinkMask       :  0x%llx\n", m_ForceHsLinkMask);
    Printf(pri, "\tForceSafeLinkMask     :  0x%llx\n", m_ForceSafeLinkMask);
    Printf(pri, "\tForceOffLinkMask      :  0x%llx\n", m_ForceOffLinkMask);
    Printf(pri, "\tEnableSafeTransitions :  0x%s\n",
           m_EnableSafeTransitions ? "true" : "false");
    Printf(pri, "\tEnableOffTransitions  :  0x%s\n",
           m_EnableOffTransitions ? "true" : "false");
    Printf(pri, "\tSafeSleepMs           :  %u\n",     m_SafeSleepMs);
    Printf(pri, "\tOffSleepMs            :  %u\n",     m_OffSleepMs);
    Printf(pri, "\tHsSleepMs             :  %u\n",     m_HsSleepMs);
    LwLinkTest::PrintJsProperties(pri);
}

//-----------------------------------------------------------------------------
RC LwLinkState::SetStateOnLinks(UINT64 linkMask, LwLink::SubLinkState state)
{
    StickyRC rc;

    TestDevicePtr pDev = GetBoundTestDevice();
    auto pLwLink = pDev->GetInterface<LwLink>();

    VerbosePrintf("%s : Changing link mask 0x%llx to %s mode on %s\n",
                  GetName().c_str(),
                  linkMask,
                  LwLink::SubLinkStateToString(state).c_str(),
                  pDev->GetName().c_str());
    for (size_t lwrId = 0; lwrId < m_OrigLinkStatus.size(); lwrId++)
    {
        if (linkMask & (1ULL << lwrId))
        {
            RC setRc = pLwLink->SetLinkState(lwrId, state);
            if (RC::OK != setRc)
            {
                Printf(Tee::PriError, "%s : Failed to train link %u to %s on %s!\n",
                       GetName().c_str(),
                       static_cast<UINT32>(lwrId),
                       LwLink::SubLinkStateToString(state).c_str(),
                       pDev->GetName().c_str());
            }
            rc = setRc;
        }
    }

    if ((state == LwLink::SLS_HIGH_SPEED) &&
        (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::PostLinkTraining))
    {
        CHECK_RC(UphyRegLogger::LogRegs(GetBoundTestDevice(),
                                        UphyRegLogger::UphyInterface::LwLink,
                                        UphyRegLogger::LogPoint::PostLinkTraining,
                                        rc));
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(LwLinkState, LwLinkTest, "LwLink link state toggling test");

#define JS_SET_LINK_MASK(className, linkMaskFunc, desc)                                         \
    JS_STEST_LWSTOM(className, linkMaskFunc, 1, desc)                                           \
    {                                                                                           \
        STEST_HEADER(1, 1, "Usage: LwLinkState." #linkMaskFunc "(linkMask)");                   \
        STEST_PRIVATE(className, pObj, nullptr);                                                \
        STEST_ARG(0, JSObject *, pJsLinkMaskObj);                                               \
        JSClass* pJsClass = JS_GetClass(pJsLinkMaskObj);                                        \
        if (!(pJsClass->flags & JSCLASS_HAS_PRIVATE) || strcmp(pJsClass->name, "UINT64") != 0)  \
        {                                                                                       \
            Printf(Tee::PriError, #linkMaskFunc "expects a UINT64 JS object as a parameter\n"); \
            RETURN_RC(RC::BAD_PARAMETER);                                                       \
        }                                                                                       \
        JsUINT64* pJsUINT64 = static_cast<JsUINT64*>(JS_GetPrivate(pContext, pJsLinkMaskObj));  \
        pObj->linkMaskFunc(pJsUINT64->GetValue());                                              \
        RETURN_RC(RC::OK);                                                                      \
    }
JS_SET_LINK_MASK(LwLinkState, SetChangeStateLinkMask, "Set mask of links to change state (default = ~0ULL)");
JS_SET_LINK_MASK(LwLinkState, SetForceHsLinkMask,     "Set mask of links to force to HS mode (default = 0)");
JS_SET_LINK_MASK(LwLinkState, SetForceSafeLinkMask,   "Set mask of links to force to Safe mode (default = 0)");
JS_SET_LINK_MASK(LwLinkState, SetForceOffLinkMask,    "Set mask of links to force to Off mode (default = 0)");

CLASS_PROP_READWRITE(LwLinkState, EnableSafeTransitions, bool,
                     "Enable transitions to Safe mode on supported devices (default = true)");
CLASS_PROP_READWRITE(LwLinkState, EnableOffTransitions, bool,
                     "Enable transitions to Off mode on supported devices (default = true)");
CLASS_PROP_READWRITE(LwLinkState, SafeSleepMs, UINT32,
                     "Time in milliseconds to sleep when in safe mode (default = 0)");
CLASS_PROP_READWRITE(LwLinkState, OffSleepMs, UINT32,
                     "Time in milliseconds to sleep when in off mode (default = 0)");
CLASS_PROP_READWRITE(LwLinkState, HsSleepMs, UINT32,
                     "Time in milliseconds to sleep when in HS mode (default = 0)");
