/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwldt_error_injector.h"

#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "device/interface/lwlink.h"
#include "gpu/js/fpk_comm.h"

namespace
{
    //! Structure for printing injection / mismatch data
    struct ErrorPrintData
    {
        string name;
        UINT32 actual;
        UINT32 expected;
    };

    const char * s_ReplayOnlyStr = "Replay Only";
    const char * s_HwRecoveryStr = "HW Recovery";
    const char * s_SwRecoveryStr = "SW Recovery";

#define ADD_ERROR_DATA(arr, s, a, e)        \
    {                                       \
        ErrorPrintData epd = { s, a, e };   \
        if (epd.name.size() > maxSize)      \
            maxSize = epd.name.size();      \
        arr.push_back(epd);                 \
    }

    LwLinkErrorInject::ErrorInjectLevel PickInjectionLevel
    (
        FancyPicker::FpContext *pFpCtx,
        UINT32                  possibleMask
    )
    {
        const UINT32 injTypeCount = Utility::CountBits(possibleMask);
        const UINT32 injTypeBit = pFpCtx->Rand.GetRandom(0, injTypeCount - 1);
        const UINT32 injType = Utility::FindNthSetBit(possibleMask, injTypeBit);
        return static_cast<LwLinkErrorInject::ErrorInjectLevel>(injType);
    }

    UINT32 PickSingleLink(FancyPicker::FpContext *pFpCtx, UINT32 linkMask)
    {
        const INT32  injLinks = Utility::CountBits(linkMask);
        const UINT32 injLinkIdx = pFpCtx->Rand.GetRandom(0, static_cast<UINT32>(injLinks) - 1);
        const UINT32 injLink = Utility::FindNthSetBit(linkMask, injLinkIdx);
        return 1 << injLink;
    }
};

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
ErrorInjector::ErrorInjector
(
    UINT32                  errInjTypeMask,
    FancyPicker::FpContext *pFpCtx,
    Tee::Priority           pri,
    FLOAT64                 timeoutMs
)
:  m_InjectTypeMask(errInjTypeMask)
  ,m_pFpCtx(pFpCtx)
  ,m_Priority(pri)
  ,m_TimeoutMs(timeoutMs)
  ,m_bRunning(false)
  ,m_bPerInjPointLevel(true)
  ,m_PossibleInjMask(~0U)
{
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
ErrorInjector::~ErrorInjector()
{
    m_ExpectedErrors.clear();
    m_ActualErrors.clear();
    m_InjectionData.clear();
}

//------------------------------------------------------------------------------
//! \brief Check the error injection results
//!
//! \return OK if actual errors match expected errors, not OK otherwise
RC ErrorInjector::CheckResults(UINT32 replayTolerance) const
{
    RC rc;

    for (auto const &actError : m_ActualErrors)
    {
        if (!m_ExpectedErrors.count(actError.first))
        {
            Printf(Tee::PriError, "Expected errors missing data for %s link %u\n",
                   get<0>(actError.first)->GetName().c_str(),
                   get<1>(actError.first));
            return RC::SOFTWARE_ERROR;
        }
        PrintExpectedErrors(actError.first, m_ExpectedErrors.at(actError.first));

        if (!actError.second.Matches(m_ExpectedErrors.at(actError.first), replayTolerance))
        {
            Printf(Tee::PriError, "Error injection mismatch on %s, Link %u\n",
                   get<0>(actError.first)->GetName().c_str(),
                   get<1>(actError.first));
            PrintMismatch(actError.second, m_ExpectedErrors.at(actError.first));
            rc = RC::LWLINK_BUS_ERROR;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
void ErrorInjector::Initialize(const set<LwLinkPath::PathEntry> &injectionPoints)
{
    m_bPerInjPointLevel = true;
    m_PossibleInjMask   = ~0U;
    m_MultiLinkSuppMask = ~0U;

    for (auto const &lwrInjectPoint : injectionPoints)
    {
        vector<LwLinkConnection::EndpointIds> epIds = GetLinks(lwrInjectPoint);
        TestDevicePtr pRemDev = lwrInjectPoint.pCon->GetRemoteDevice(lwrInjectPoint.pSrcDev);

        for (auto const & eid : epIds)
        {
            m_ExpectedErrors[make_tuple(lwrInjectPoint.pSrcDev, eid.first)] = ErrorCounts();
            m_ActualErrors[make_tuple(lwrInjectPoint.pSrcDev, eid.first)]   = ErrorCounts();
            m_ExpectedErrors[make_tuple(pRemDev, eid.second)]               = ErrorCounts();
            m_ActualErrors[make_tuple(pRemDev, eid.second)]                 = ErrorCounts();
        }

        auto errInj = lwrInjectPoint.pSrcDev->GetInterface<LwLinkErrorInject>();
        auto expInj = pRemDev->GetInterface<LwLinkErrorInject>();

        const UINT32 possibleInj = errInj->SupportedInjectLevelMask() &
                                   expInj->SupportedInjectLevelMask() &
                                   m_InjectTypeMask;
        m_InjectionData.emplace_back(lwrInjectPoint, pRemDev, possibleInj);

        if (!lwrInjectPoint.pSrcDev->IsEndpoint() || !pRemDev->IsEndpoint())
            m_bPerInjPointLevel = false;

        m_PossibleInjMask = errInj->SupportedInjectLevelMask() &
                            expInj->SupportedInjectLevelMask() &
                            m_InjectTypeMask &
                            m_PossibleInjMask;

        m_MultiLinkSuppMask = errInj->SupportedMultiLinkInjectionMask() &
                              expInj->SupportedMultiLinkInjectionMask() &
                              m_MultiLinkSuppMask;
    }
}

//------------------------------------------------------------------------------
//! \brief Reset the error injector
//!
//! \return OK if successful, not OK otherwise
RC ErrorInjector::Reset()
{
    StickyRC rc;
    LwLink::ErrorCounts errorCounts;

    for (auto & expError : m_ExpectedErrors)
    {
        auto pLwLink = get<0>(expError.first)->GetInterface<LwLink>();
        rc = pLwLink->GetErrorCounts(get<1>(expError.first), &errorCounts);
        rc = pLwLink->ClearErrorCounts(get<1>(expError.first));
        expError.second = ErrorCounts();
    }
    for (auto & actError : m_ActualErrors)
    {
        actError.second = ErrorCounts();
    }

    for (auto & lwrInjectData : m_InjectionData)
    {
        lwrInjectData.lwrInjectionLinkMask = 0;
        lwrInjectData.lwrInjectionLevel    = LwLinkErrorInject::EIL_OFF;
        lwrInjectData.remExpectedLinkMask  = 0;
    }

    m_bRunning = false;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Start error injection
//!
//! \return OK if successful, not OK otherwise
RC ErrorInjector::Setup()
{
    RC rc;
    if (m_bPerInjPointLevel)
    {
        CHECK_RC(SetupInjectionPerPoint());
    }
    else
    {
        SetupInjectionGlobal();
    }
    CHECK_RC(SetupExpectedErrors());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Start error injection
//!
//! \return OK if successful, not OK otherwise
RC ErrorInjector::Start()
{
    RC rc;

    Utility::CleanupOnReturn<ErrorInjector, RC> stopErrorInjection(this, &ErrorInjector::Stop);

    for (auto const & lwrInjectData : m_InjectionData)
    {
        if (lwrInjectData.lwrInjectionLevel == LwLinkErrorInject::EIL_OFF)
            continue;

        CHECK_RC(StartExpectingErrors(lwrInjectData));

        auto errInj = lwrInjectData.injectionPoint.pSrcDev->GetInterface<LwLinkErrorInject>();

        rc = errInj->InjectErrors(lwrInjectData.lwrInjectionLinkMask,
                                  lwrInjectData.lwrInjectionLevel);
        if (rc != OK)
        {
            errInj->InjectErrors(lwrInjectData.lwrInjectionLinkMask, LwLinkErrorInject::EIL_OFF);
            return rc;
        }
    }

    stopErrorInjection.Cancel();
    m_bRunning = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Stop error injection
//!
//! \return OK if successful, not OK otherwise
RC ErrorInjector::Stop()
{
    if (!m_bRunning)
        return OK;

    StickyRC rc;

    map<TestDevicePtr, set<UINT32>> countedLinks;

    for (auto & lwrInjectData : m_InjectionData)
    {
        if (lwrInjectData.lwrInjectionLevel == LwLinkErrorInject::EIL_OFF)
            continue;

        TestDevicePtr pInjDev = lwrInjectData.injectionPoint.pSrcDev;
        TestDevicePtr pExpDev = lwrInjectData.pRemDev;
        LwLinkConnectionPtr pInjCon = lwrInjectData.injectionPoint.pCon;

        if (!countedLinks.count(pInjDev))
            countedLinks[pInjDev] = { };
        if (!countedLinks.count(pExpDev))
            countedLinks[pExpDev] = { };

        vector<LwLinkConnection::EndpointIds> epIds = GetLinks(lwrInjectData.injectionPoint);
        for (auto const & lwrEp : epIds)
        {
            if (!(lwrInjectData.lwrInjectionLinkMask & (1 << lwrEp.first)))
                continue;

            if (!countedLinks[pInjDev].count(lwrEp.first))
            {
                rc = GetActualErrors(pInjDev, lwrEp.first);
                rc = pInjDev->GetInterface<LwLink>()->ClearErrorCounts(lwrEp.first);
                countedLinks[pInjDev].insert(lwrEp.first);
            }

            if (!countedLinks[pExpDev].count(lwrEp.second))
            {
                rc = GetActualErrors(pExpDev, lwrEp.second);
                rc = pExpDev->GetInterface<LwLink>()->ClearErrorCounts(lwrEp.second);
                countedLinks[pExpDev].insert(lwrEp.second);
            }
        }
        auto errInj = pInjDev->GetInterface<LwLinkErrorInject>();

        // Dont stop injecting errors until after all error counts have been retrieved
        // otherwise disabling injection will cause failures since it also enables
        // some interrupt based errors in RM that need to be disabled during error
        // injection
        rc = errInj->InjectErrors(lwrInjectData.lwrInjectionLinkMask, LwLinkErrorInject::EIL_OFF);

        lwrInjectData.lwrInjectionLinkMask = 0;
        lwrInjectData.lwrInjectionLevel    = LwLinkErrorInject::EIL_OFF;
        lwrInjectData.remExpectedLinkMask  = 0;
    }

    rc = StopExpectingErrors();
    m_bRunning = false;

    return rc;
}

//------------------------------------------------------------------------------
ErrorInjector::InjectionData * ErrorInjector::FindRemInjData
(
    const InjectionData & injData
)
{
    for (auto & lwrInjectData : m_InjectionData)
    {
        // If the connections are the same but the injection points are different then this would
        // be the injection site on the opposite end of the connection
        if ((lwrInjectData.injectionPoint.pCon == injData.injectionPoint.pCon) &&
            (lwrInjectData.injectionPoint != injData.injectionPoint))
        {
             return &lwrInjectData;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
RC ErrorInjector::GetActualErrors(TestDevicePtr pDev, UINT32 linkId)
{
    RC rc;
    LwLink::ErrorCounts errorCounts;
    CHECK_RC(pDev->GetInterface<LwLink>()->GetErrorCounts(linkId, &errorCounts));

    const UINT32 txReplays =
        errorCounts.GetCount(LwLink::ErrorCounts::LWL_TX_REPLAY_ID);
    const UINT32 txRecoveries =
        errorCounts.GetCount(LwLink::ErrorCounts::LWL_TX_RECOVERY_ID);
    const UINT32 swRecoveries =
        errorCounts.GetCount(LwLink::ErrorCounts::LWL_SW_RECOVERY_ID);

    ErrorCounts &actErrors = m_ActualErrors[make_tuple(pDev, linkId)];
    if (swRecoveries)
    {
        actErrors.swRecoveryCount++;
    }
    else if (txRecoveries)
    {
        actErrors.hwRecoveryCount++;
    }
    else if (txReplays)
    {
        actErrors.replayOnlyCount++;
    }

    return rc;
}

//------------------------------------------------------------------------------
vector<LwLinkConnection::EndpointIds> ErrorInjector::GetLinks
(
    const LwLinkPath::PathEntry & injPoint
) const
{
    if (injPoint.bRemote)
        return injPoint.pCon->GetRemoteLinks(injPoint.pSrcDev);
    return injPoint.pCon->GetLocalLinks(injPoint.pSrcDev);
}

//------------------------------------------------------------------------------
RC ErrorInjector::SetupExpectedErrors()
{
    for (auto & lwrInjectData : m_InjectionData)
    {
        if (lwrInjectData.lwrInjectionLevel == LwLinkErrorInject::EIL_OFF)
            continue;

        vector<LwLinkConnection::EndpointIds> epIds = GetLinks(lwrInjectData.injectionPoint);
        for (auto const & lwrEp : epIds)
        {
            if (((1 << lwrEp.first) & lwrInjectData.lwrInjectionLinkMask) == 0)
                continue;

            ErrorCounts &injErrors =
                m_ExpectedErrors[make_tuple(lwrInjectData.injectionPoint.pSrcDev, lwrEp.first)];
            ErrorCounts &expErrors =
                m_ExpectedErrors[make_tuple(lwrInjectData.pRemDev, lwrEp.second)];
            switch (lwrInjectData.lwrInjectionLevel)
            {
                case LwLinkErrorInject::EIL_REPLAYS_ONLY:
                    injErrors.replayOnlyCount++;
                    break;
                case LwLinkErrorInject::EIL_HW_RECOVERY:
                    injErrors.hwRecoveryCount++;

                    // HW and SWRecoveries occur on both ends of the loop so for non-loopback or
                    // loopout connections, increment the count on the other end
                    if (!lwrInjectData.injectionPoint.pCon->IsLoop() ||
                        (lwrEp.second != lwrEp.first))
                    {
                        expErrors.hwRecoveryCount++;
                    }
                    break;
                case LwLinkErrorInject::EIL_SW_RECOVERY:
                    injErrors.swRecoveryCount++;

                    // HW and SWRecoveries occur on both ends of the loop so for non-loopback or
                    // loopout connections, increment the count on the other end
                    if (!lwrInjectData.injectionPoint.pCon->IsLoop() ||
                        (lwrEp.second != lwrEp.first))
                    {
                        expErrors.swRecoveryCount++;
                    }
                    break;
                case LwLinkErrorInject::EIL_OFF:
                    break;
                default:
                    Printf(Tee::PriError, "%s : Unknown error injection level %d\n",
                           __FUNCTION__, lwrInjectData.lwrInjectionLevel);
                    return RC::SOFTWARE_ERROR;
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
void ErrorInjector::SetupInjectionGlobal()
{
    const LwLinkErrorInject::ErrorInjectLevel injLevel =
        PickInjectionLevel(m_pFpCtx, m_PossibleInjMask);
    for (auto & lwrInjectData : m_InjectionData)
    {
        lwrInjectData.lwrInjectionLevel    = LwLinkErrorInject::EIL_OFF;
        lwrInjectData.lwrInjectionLinkMask = 0;
        lwrInjectData.remExpectedLinkMask  = 0;

        TestDevicePtr pInjDev = lwrInjectData.injectionPoint.pSrcDev;
        LwLinkConnectionPtr pInjCon = lwrInjectData.injectionPoint.pCon;
        UINT32 injMask = lwrInjectData.injectionPoint.bRemote ?
            pInjCon->GetRemoteLinkMask(pInjDev) : pInjCon->GetLocalLinkMask(pInjDev);
        InjectionData * pRemInjData = FindRemInjData(lwrInjectData);
        if (pRemInjData)
        {
            if ((injLevel == LwLinkErrorInject::EIL_HW_RECOVERY) ||
                (injLevel == LwLinkErrorInject::EIL_SW_RECOVERY))
            {
                injMask &= ~pRemInjData->remExpectedLinkMask;
            }
        }

        if (!injMask)
            continue;

        lwrInjectData.lwrInjectionLevel    = injLevel;

        // Only allow recovery on one link of a gang at a time (per end) if multi link
        // injection is not supported
        if (!(m_MultiLinkSuppMask & (1 << injLevel)))
            lwrInjectData.lwrInjectionLinkMask = PickSingleLink(m_pFpCtx, injMask);
        else
            lwrInjectData.lwrInjectionLinkMask = injMask;

        SetupRemoteExpectedMask(&lwrInjectData);
    }
}

//------------------------------------------------------------------------------
RC ErrorInjector::SetupInjectionPerPoint()
{
    for (auto & lwrInjectData : m_InjectionData)
    {
        TestDevicePtr pInjDev = lwrInjectData.injectionPoint.pSrcDev;
        TestDevicePtr pExpDev = lwrInjectData.pRemDev;
        LwLinkConnectionPtr pInjCon = lwrInjectData.injectionPoint.pCon;

        UINT32 possibleInjMask  = lwrInjectData.possibleInjMask;
        UINT32 possibleLinkMask = 0;

        if (lwrInjectData.injectionPoint.bRemote)
            possibleLinkMask = pInjCon->GetRemoteLinkMask(pInjDev);
        else
            possibleLinkMask = pInjCon->GetLocalLinkMask(pInjDev);

        InjectionData * pRemInjData = FindRemInjData(lwrInjectData);
        if (pRemInjData)
        {
            switch (pRemInjData->lwrInjectionLevel)
            {
                case LwLinkErrorInject::EIL_REPLAYS_ONLY:
                    // If the remote side is injecting replays then only allow replays on the
                    // current side to avoid recovery injection from defeating replays.  This
                    // can occur even if replays are injected on a link that is not being
                    // recovered since recovery will stall link traffic and injecting replays
                    // while traffic is stalled will cause the replays to not occur
                    possibleInjMask = (1 << LwLinkErrorInject::EIL_REPLAYS_ONLY);
                    break;
                case LwLinkErrorInject::EIL_HW_RECOVERY:
                case LwLinkErrorInject::EIL_SW_RECOVERY:
                    // If injecting recovery on the remote end, do not allow replays on this
                    // end of the connection (see above).  In addition remove the link on this
                    // end of the remote connection from the possible links that can have an
                    // injection since recoveries will already occur on both ends of the link
                    possibleInjMask  &= ~(1 << LwLinkErrorInject::EIL_REPLAYS_ONLY);
                    possibleLinkMask &= ~pRemInjData->remExpectedLinkMask;
                    break;
                case LwLinkErrorInject::EIL_OFF:
                    break;
                default:
                    Printf(Tee::PriError, "%s : Unknown error injection level %d\n",
                           __FUNCTION__, pRemInjData->lwrInjectionLevel);
                    return RC::SOFTWARE_ERROR;
            }
        }

        if (!possibleInjMask || !possibleLinkMask)
            continue;

        lwrInjectData.lwrInjectionLevel = PickInjectionLevel(m_pFpCtx, possibleInjMask);

        // Only allow recovery on one link of a gang at a time (per end) if multi link
        // injection is not supported
        if (!(m_MultiLinkSuppMask & (1 << lwrInjectData.lwrInjectionLevel)))
            lwrInjectData.lwrInjectionLinkMask = PickSingleLink(m_pFpCtx, possibleLinkMask);
        else
            lwrInjectData.lwrInjectionLinkMask = possibleLinkMask;

        SetupRemoteExpectedMask(&lwrInjectData);
    }
    return OK;
}

//------------------------------------------------------------------------------
void ErrorInjector::SetupRemoteExpectedMask(InjectionData *pInjData)
{
    vector<LwLinkConnection::EndpointIds> epIds = GetLinks(pInjData->injectionPoint);
    pInjData->remExpectedLinkMask = 0;
    for (auto const & lwrEp : epIds)
    {
        if (!(pInjData->lwrInjectionLinkMask & (1 << lwrEp.first)))
            continue;
        pInjData->remExpectedLinkMask |= (1 << lwrEp.second);
    }
}

//------------------------------------------------------------------------------
//! \brief Notify devices to expect errors
//!
//! \param pInjDev   : Device injecting errors
//! \param injMask   : Link mask where errors are injected
//! \param injLevel  : What level of errors are being injected
//!
//! \return OK if successful, not OK otherwise
RC ErrorInjector::StartExpectingErrors(const InjectionData &injData) const
{
    if (injData.lwrInjectionLevel == LwLinkErrorInject::EIL_OFF)
        return OK;

    RC rc;

    TestDevicePtr pInjDev = injData.injectionPoint.pSrcDev;
    auto expDevErrInj = injData.pRemDev->GetInterface<LwLinkErrorInject>();
    auto injDevErrInj = pInjDev->GetInterface<LwLinkErrorInject>();

    // Injecting errors can generate errors on both ends of the link so
    // let both devices know to expect errors
    CHECK_RC(expDevErrInj->ExpectErrors(injData.remExpectedLinkMask, injData.lwrInjectionLevel));
    CHECK_RC(injDevErrInj->ExpectErrors(injData.lwrInjectionLinkMask, injData.lwrInjectionLevel));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Notify devices to stop expecting errors on all links
//!
//! \return OK if successful, not OK otherwise
RC ErrorInjector::StopExpectingErrors() const
{
    StickyRC rc;

    for (auto const & lwrInjectData : m_InjectionData)
    {
        if (lwrInjectData.lwrInjectionLevel == LwLinkErrorInject::EIL_OFF)
            continue;

        TestDevicePtr pInjDev = lwrInjectData.injectionPoint.pSrcDev;
        auto expDevErrInj = lwrInjectData.pRemDev->GetInterface<LwLinkErrorInject>();
        auto injDevErrInj = pInjDev->GetInterface<LwLinkErrorInject>();

        rc = expDevErrInj->ExpectErrors(lwrInjectData.remExpectedLinkMask,
                                        LwLinkErrorInject::EIL_OFF);
        rc = injDevErrInj->ExpectErrors(lwrInjectData.lwrInjectionLinkMask,
                                        LwLinkErrorInject::EIL_OFF);
    }
    return rc;
}

//------------------------------------------------------------------------------
void ErrorInjector::PrintExpectedErrors
(
    tuple<TestDevicePtr, UINT32> devLinkId,
    const ErrorCounts &expected
) const
{
    Printf(m_Priority,
           "Errors injected on %s, Link %u\n",
           get<0>(devLinkId)->GetName().c_str(),
           get<1>(devLinkId));

    size_t maxSize = 0;
    vector<ErrorPrintData> printData;
    ADD_ERROR_DATA(printData,
                   s_ReplayOnlyStr,
                   0,
                   expected.replayOnlyCount);
    ADD_ERROR_DATA(printData,
                   s_HwRecoveryStr,
                   0,
                   expected.hwRecoveryCount);
    ADD_ERROR_DATA(printData,
                   s_SwRecoveryStr,
                   0,
                   expected.swRecoveryCount);

    for (size_t ii = 0; ii < printData.size(); ii++)
    {
        Printf(m_Priority, "    %s counts injected %s: %u\n",
               printData[ii].name.c_str(),
               string(maxSize - printData[ii].name.size(), ' ').c_str(),
               printData[ii].expected);
    }
}

//------------------------------------------------------------------------------
//! \brief Print mismatch data
//!
//! \param actual   : Actual error counts
//! \param expected : Expected error counts
void ErrorInjector::PrintMismatch
(
    const ErrorCounts &actual,
    const ErrorCounts &expected
) const
{
    vector<ErrorPrintData> mismatchData;
    size_t maxSize = 0;

    if (actual.replayOnlyCount != expected.replayOnlyCount)
    {
        ADD_ERROR_DATA(mismatchData,
                       s_ReplayOnlyStr,
                       actual.replayOnlyCount,
                       expected.replayOnlyCount);
    }
    if (actual.hwRecoveryCount != expected.hwRecoveryCount)
    {
        ADD_ERROR_DATA(mismatchData,
                       s_HwRecoveryStr,
                       actual.hwRecoveryCount,
                       expected.hwRecoveryCount);
    }
    if (actual.swRecoveryCount != expected.swRecoveryCount)
    {
        ADD_ERROR_DATA(mismatchData,
                       s_SwRecoveryStr,
                       actual.swRecoveryCount,
                       expected.swRecoveryCount);
    }

    for (size_t ii = 0; ii < mismatchData.size(); ii++)
    {
        Printf(Tee::PriError, "    %s counts actual (expected) %s: %u (%u)\n",
               mismatchData[ii].name.c_str(),
               string(maxSize - mismatchData[ii].name.size(), ' ').c_str(),
               mismatchData[ii].actual,
               mismatchData[ii].expected);
    }
}
