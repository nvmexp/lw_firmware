/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "pmtrigg.h"
#include "core/include/platform.h"
#include "pmchan.h"
#include "pmchwrap.h"
#include "pmevent.h"
#include "pmsurf.h"
#include "rmpmucmdif.h"
#include "core/include/utility.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "lwerror.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/sysspec.h"

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger::PmTrigger() :
    m_pEventManager(NULL)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmTrigger::~PmTrigger()
{
}

//--------------------------------------------------------------------
//! \brief Return the PolicyManager
//!
PolicyManager *PmTrigger::GetPolicyManager() const
{
    return m_pEventManager->GetPolicyManager();
}

//--------------------------------------------------------------------
//! \fn RC PmTrigger::IsSupported() const
//! \brief Check whether the trigger can be exelwted
//!
//! This is called at parse-time to determine whether the trigger is
//! supported.
//!
/* virtual RC PmTrigger::IsSupported() const = 0 */

//--------------------------------------------------------------------
//! \fn bool PmTrigger::CouldMatch(const PmEvent *pEvent) const
//! \brief "Quick and dirty" check whether the event might match the trigger
//!
//! This method does a "quick and dirty" comparison of the trigger and
//! event, and returns true if they might match.  CouldMatch() is not
//! as thorough as DoMatch(), which does a full comparison of the
//! trigger and event.  CouldMatch() has several restrictions that
//! DoMatch() does not:
//!
//! - CouldMatch() is called with the PolicyManager mutex unlocked,
//!   and must not call any methods that lock the mutex.  See
//!   "Rationale" below.
//! - CouldMatch may be called before the surfaces are initialized, so
//!   SurfaceDesc and PageDesc operations are not allowed.  Other
//!   PolicyManager xxxDesc classes, such as ChannelDesc and GpuDesc,
//!   may work for now... but it's probably best to avoid them anyway,
//!   since they may lock the mutex someday.
//! - CouldMatch() is const, and may be called more than once per
//!   event.  DoMatch() is non-const, and will be called at most
//!   once per event.  So any side-effects should go in DoMatch().
//! - It is permissible for CouldMatch() to return true and DoMatch
//!   to return false on the same event.  The reverse is forbidden:
//!   CouldMatch() must not return false for any event that would
//!   return true from DoMatch().
//!
//! Many implementations of CouldMatch() merely check whether
//! pEvent->GetType() matches the trigger.  Most implementations of
//! DoMatch() start by calling CouldMatch(), and then check
//! everything else.
//!
//! Rationale:
//!
//! This method primarily exists so that PmEventManager::HandleEvent()
//! can compare an event to all the triggers before it locks the
//! PolicyManager mutex, and abort if the event cannot match any
//! trigger.  This helps prevent deadlocks in cases such as:
//!
//! - Thread A is running Action.WaitForSemaphoreRelease(), which
//!   locks the mutex to prevent overlapping actions.
//! - Thread B is supposed to release the semaphore, but generates a
//!   WaitForIdle event first.
//! - Thread B blocks on the mutex in HandleEvent().
//! - Deadlock: Thread A is blocked on the GPU semaphore, and thread B
//!   is blocked on the mutex.
//!
//! This method prevents the above deadlock in the 95% case in which
//! the second event is a no-op, because the policyFile didn't have an
//! OnWaitForIdle trigger.  (For the remaining 5% case, the policyFile
//! writer needs to use AllowOverlappingTriggers to deliberately allow
//! the two actionBlocks to overlap.)
//!
/* virtual bool PmTrigger::CouldMatch(const PmEvent *pEvent) const = 0 */

//--------------------------------------------------------------------
//! \fn bool PmTrigger::DoMatch(const PmEvent *pEvent)
//! \brief Return true if the trigger matches the event.
//!
//! This method returns true if the trigger should fire when the event
//! happens.
//!
//! This method may also have side-effects, such as setting up the
//! trigger for the next event.  For example, if the trigger contains
//! a fancyPicker that makes it accept a different event each time.
//!
//! \sa CouldMatch()
//!
/* virtual bool PmTrigger::DoMatch(const PmEvent *pEvent) = 0 */

//--------------------------------------------------------------------
//! \brief Return the surfaceDecs for this trigger, or NULL if none
//!
//! This method is called by PmSurfaceDesc::GetMatchingXXX and
//! PmPageDesc::GetMatchingXXX to find which surfaces/pages match this
//! trigger.
//!
/* virtual */ const PmSurfaceDesc *PmTrigger::GetSurfaceDesc() const
{
    return NULL;
}

//--------------------------------------------------------------------
//! \brief Return the pageDesc for this trigger, or NULL if none
//!
//! This method is called by PmSurfaceDesc::GetMatchingXXX and
//! PmPageDesc::GetMatchingXXX to find which surfaces/pages match this
//! trigger.
//!
/* virtual */ const PmPageDesc *PmTrigger::GetPageDesc() const
{
    return NULL;
}

//--------------------------------------------------------------------
//! \brief Private method to hook any events required by the trigger
//!
//! This method is called by PmEventManager::StartTest to hook any
//! events required to generate the trigger
//!
/* virtual */ RC PmTrigger::StartTest()
{
    return OK;
}

//--------------------------------------------------------------------
//! \fn UINT32 PmTrigger::GetCaps() const;
//! \brief Return a mask of the objects that the matching PmEvent should have
//!
//! This method is called at parse-time by PmAction::IsSupported(), by
//! any action that requires a faulting object.  Ideally, we should
//! query the PmEvent, but the event isn't known at parse-time.
//!
//! This method is normally called via colwenience methods
//! HasSurfaces(), HasPage(), etc.
//!
//! For example, if HAS_SURFACES is set, then all matching events
//! should return a surface(s) from PmEvent::GetSurfaces().  Similar
//! logic applies to:
//!
//! - HAS_SURFACES: PmEvent::GetSurfaces()
//! - HAS_PAGE: PmEvent::GetPage() and PmEvent::GetGpuAddr()
//! - HAS_CHANNEL: PmEvent::GetChannel()
//! - HAS_GPU_SUBDEVICE: PmEvent::GetGpuSubdevice()
//! - HAS_GPU_DEVICE: PmEvent::GetGpuDevice()
//!
/* virtual UINT32 PmTrigger::GetCaps() const; */

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnChannelReset
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnChannelReset::PmTrigger_OnChannelReset()
{
}

//--------------------------------------------------------------------
//! \brief Return OK.
//!
/* virtual */ RC PmTrigger_OnChannelReset::IsSupported() const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a START event.
//!
/* virtual */ bool PmTrigger_OnChannelReset::CouldMatch(const PmEvent *pEvent) const
{
    return pEvent->GetType() == PmEvent::CHANNEL_RESET;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a START event.
//!
/* virtual */ bool PmTrigger_OnChannelReset::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_Start
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_Start::PmTrigger_Start()
{
}

//--------------------------------------------------------------------
//! \brief Return OK.
//!
/* virtual */ RC PmTrigger_Start::IsSupported() const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a START event.
//!
/* virtual */ bool PmTrigger_Start::CouldMatch(const PmEvent *pEvent) const
{
    return pEvent->GetType() == PmEvent::START;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a START event.
//!
/* virtual */ bool PmTrigger_Start::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_End
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_End::PmTrigger_End()
{
}

//--------------------------------------------------------------------
//! \brief Return OK.
//!
/* virtual */ RC PmTrigger_End::IsSupported() const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a END event.
//!
/* virtual */ bool PmTrigger_End::CouldMatch(const PmEvent *pEvent) const
{
    return pEvent->GetType() == PmEvent::END;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a END event.
//!
/* virtual */ bool PmTrigger_End::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_RobustChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_RobustChannel::PmTrigger_RobustChannel
(
    const PmChannelDesc *pChannelDesc,
    RC rc
) :
    m_pChannelDesc(pChannelDesc),
    m_rc(rc)
{
    MASSERT(pChannelDesc != NULL);
    MASSERT(rc != OK);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_RobustChannel::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a robust-channel event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_RobustChannel::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const PmEvent_RobustChannel * pRobustChannelEvent =
        static_cast<const PmEvent_RobustChannel *>(pEvent);

    return (((pEvent->GetType() == PmEvent::ROBUST_CHANNEL &&
            pRobustChannelEvent->GetRC() == m_rc)) ||
            (pEvent->GetType() == PmEvent::PHYSICAL_PAGE_FAULT));
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching robust-channel event
//!
/* virtual */ bool PmTrigger_RobustChannel::DoMatch(const PmEvent *pEvent)
{
    if(pEvent->GetType() == PmEvent::PHYSICAL_PAGE_FAULT)
        return CouldMatch(pEvent);
    else
        return CouldMatch(pEvent) && m_pChannelDesc->Matches(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_NonStallInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_NonStallInt::PmTrigger_NonStallInt
(
    const RegExp &intNames,
    const PmChannelDesc *pChannelDesc
) :
    m_IntNames(intNames),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_NonStallInt::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a NonStallInt event with the right name.
//! Does not check whether it oclwrred on the right channel.
//!
/* virtual */ bool PmTrigger_NonStallInt::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const PmEvent_NonStallInt *pNsiEvent =
        static_cast<const PmEvent_NonStallInt*>(pEvent);

    return (pEvent->GetType() == PmEvent::NON_STALL_INT &&
            m_IntNames.Matches(pNsiEvent->GetNonStallInt()->GetName()));
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching NonStallInt event
//!
/* virtual */ bool PmTrigger_NonStallInt::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent) && m_pChannelDesc->Matches(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_SemaphoreRelease
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_SemaphoreRelease::PmTrigger_SemaphoreRelease
(
    const PmSurfaceDesc *pSurfaceDesc,
    const FancyPicker& offsetPicker,
    const FancyPicker& payloadPicker,
    bool   bUseSeedString,
    UINT32 randomSeed
) :
    m_PageDesc(pSurfaceDesc, 0, sizeof(m_Payload)),
    m_OffsetPicker(offsetPicker),
    m_PayloadPicker(payloadPicker)
{
    MASSERT(pSurfaceDesc != NULL);

    // Initialize fancy picker context
    m_FpContext.LoopNum    = 0;
    m_FpContext.RestartNum = 0;
    m_FpContext.pJSObj     = 0;
    if(!bUseSeedString)
    {
        m_FpContext.Rand.SeedRandom(randomSeed);
    }
    else
    {
        const string seedString = pSurfaceDesc->GetName();
        UINT64 BIGGEST_32_BIT_PRIME = 0x00000000fffffffbUL;
        UINT64 strSeed = 0;
        for (size_t i = 0; i < seedString.length(); ++i)
        {
            strSeed = (strSeed << 8) + (seedString[i] & 0x00ff);
            strSeed %= BIGGEST_32_BIT_PRIME;
        }
        m_FpContext.Rand.SeedRandom(static_cast<UINT32>(strSeed));
    }

    m_OffsetPicker.SetContext(&m_FpContext);
    m_PayloadPicker.SetContext(&m_FpContext);

    // Set initial page descriptor and payload
    m_PageDesc = PmPageDesc(pSurfaceDesc, m_OffsetPicker.Pick(),
                            GetPayloadSize());
    m_Payload = m_PayloadPicker.Pick64();
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmTrigger_SemaphoreRelease::~PmTrigger_SemaphoreRelease()
{
    PmTrigger_SemaphoreRelease *pThis = this;
    GetEventManager()->UnhookSurfaceEvent(
        PmTrigger_SemaphoreRelease::SemReleaseCallback, &pThis, sizeof(pThis));
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_SemaphoreRelease::IsSupported() const
{
    return m_PageDesc.IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a SemaphoreRelease event with the right
//! payload.  Does not check whether the surface or offset is right.
//!
/* virtual */ bool PmTrigger_SemaphoreRelease::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const PmEvent_SemaphoreRelease *pSemaphoreReleaseEvent =
        static_cast<const PmEvent_SemaphoreRelease*>(pEvent);

    return (pEvent->GetType() == PmEvent::SEMAPHORE_RELEASE &&
            m_Payload == pSemaphoreReleaseEvent->GetPayload());
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching SemaphoreRelease event
//!
//! Return true if pEvent is a matching SemaphoreRelease event, and
//! update m_PageDesc and m_Payload for the next SemaphoreRelease
//! event chosen by the fancyPickers.
//!
/* virtual */ bool PmTrigger_SemaphoreRelease::DoMatch
(
    const PmEvent *pEvent
)
{
    if (CouldMatch(pEvent) && m_PageDesc.Matches(pEvent))
    {
        // set page_desc and payload to the next values in the pickers
        m_PageDesc = PmPageDesc(m_PageDesc.GetSurfaceDesc(),
                                m_OffsetPicker.Pick(),
                                GetPayloadSize());
        m_Payload = m_PayloadPicker.Pick64();
        return true;
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Override the base-class method
//!
/* virtual */ const PmPageDesc *PmTrigger_SemaphoreRelease::GetPageDesc() const
{
    return &m_PageDesc;
}

//--------------------------------------------------------------------
//! \brief Check whether the release has oclwrred; launch PmEvent(s) if so.
//!
RC PmTrigger_SemaphoreRelease::SemReleaseCallback(void *ppThis,
                                                  const PmMemRanges &matchingRanges,
                                                  PmPageDesc* nextPageDesc,
                                                  vector<UINT08>* nextData)
{
    PmTrigger_SemaphoreRelease *pThis = *static_cast<PmTrigger_SemaphoreRelease**>(ppThis);
    RC rc;

    // Increment loop num for both pickers
    pThis->m_FpContext.LoopNum++;
    // Setup the next page descriptor
    UINT64 payloadSize = pThis->GetPayloadSize();
    UINT64 nextPayload = pThis->m_PayloadPicker.Pick64();
    *nextPageDesc = PmPageDesc(pThis->m_PageDesc.GetSurfaceDesc(),
                               pThis->m_OffsetPicker.Pick(), payloadSize);
    // Setup the next payload vector
    nextData->resize(payloadSize, 0);
    memcpy(&nextData->front(), &nextPayload, payloadSize);

    // Call event handler for each matching range
    for (UINT32 ii = 0; ii < matchingRanges.size(); ii++)
    {
        PmEvent_SemaphoreRelease semRelease(matchingRanges[ii],
                                            pThis->m_Payload);
        CHECK_RC(pThis->GetEventManager()->HandleEvent(&semRelease));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Hook the surface event so the semaphore release may be triggered.
//!
RC PmTrigger_SemaphoreRelease::StartTest()
{
    // Sets up the hook with the initial pageDesc & payload
    PmTrigger_SemaphoreRelease *pThis = this;

    // Setup the payload vector
    UINT64 payloadSize = GetPayloadSize();
    vector<UINT08> payload(payloadSize, 0);
    memcpy(&(payload[0]), &m_Payload, payloadSize);

    return GetEventManager()->HookSurfaceEvent(&m_PageDesc, payload,
                                PmTrigger_SemaphoreRelease::SemReleaseCallback,
                                &pThis, sizeof(pThis));
}

//--------------------------------------------------------------------
//! \brief Private function to return payload size according to FancyPicker type
//!
UINT64 PmTrigger_SemaphoreRelease::GetPayloadSize() const
{
    return m_PayloadPicker.Type() == FancyPicker::T_INT64?
           sizeof(UINT64) : sizeof(UINT32);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnWaitForChipIdle
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnWaitForChipIdle::PmTrigger_OnWaitForChipIdle()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmTrigger_OnWaitForChipIdle::IsSupported() const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a matching WAIT_FOR_IDLE event.
//!
/* virtual */
bool PmTrigger_OnWaitForChipIdle::CouldMatch(const PmEvent *pEvent) const
{
    return (pEvent->GetType() == PmEvent::WAIT_FOR_IDLE &&
            pEvent->GetChannels().empty());
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a matching WAIT_FOR_IDLE event.
//!
/* virtual */
bool PmTrigger_OnWaitForChipIdle::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnWaitForIdle
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnWaitForIdle::PmTrigger_OnWaitForIdle
(
    const PmChannelDesc *pChannelDesc
) :
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(m_pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnWaitForIdle::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if the event is a WAIT_FOR_IDLE event.  This method
//! makes sure the WAIT_FOR_IDLE is on a non-NULL channel -- otherwise
//! it would be a WaitForChipIdle -- but it doesn't check whether it's
//! the correct channel.
//!
/* virtual */ bool PmTrigger_OnWaitForIdle::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return (pEvent->GetType() == PmEvent::WAIT_FOR_IDLE &&
            !pEvent->GetChannels().empty());
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a matching WAIT_FOR_IDLE event.
//!
/* virtual */ bool PmTrigger_OnWaitForIdle::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent) && (m_pChannelDesc->Matches(pEvent));
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnMethodWrite
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnMethodWrite::PmTrigger_OnMethodWrite
(
    const PmChannelDesc *pChannelDesc,
    const FancyPicker&   fancyPicker,
    bool                 bUseSeedString,
    UINT32               randomSeed
) :
    m_pChannelDesc(pChannelDesc),
    m_FancyPicker(fancyPicker),
    m_bUseSeedString(bUseSeedString),
    m_RandomSeed(randomSeed)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmTrigger_OnMethodWrite::~PmTrigger_OnMethodWrite()
{
    for (PickCountMap::iterator iter = m_PickCountMap.begin();
         iter != m_PickCountMap.end(); iter++)
    {
        delete iter->second;
    }
    m_PickCountMap.clear();
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnMethodWrite::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_WRITE event.
//!
/* virtual */ bool PmTrigger_OnMethodWrite::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return pEvent->GetType() == PmEvent::METHOD_WRITE;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_WRITE event, the
//! channel matches, and the count is part of the PickCounter.
//!
/* virtual */ bool PmTrigger_OnMethodWrite::DoMatch(const PmEvent *pEvent)
{
    if (CouldMatch(pEvent) && m_pChannelDesc->Matches(pEvent))
    {
        PmChannels channels = pEvent->GetChannels();
        MASSERT(channels.size() == 1);
        const PmChannel *pChannel = channels[0];
        PmChannelWrapper *pChannelWrapper =
            static_cast<const PmEvent_MethodWrite *>(pEvent)->GetPmChannelWrapper();

        // Get a pointer to the PmPickCounter class for this channel/trigger
        // pair.  Create one if it doesn't already exist.
        //
        PmPickCounter *pPickCounter;
        PickCountMap::iterator iter = m_PickCountMap.find(pChannel);
        if (iter != m_PickCountMap.end())
        {
            pPickCounter = iter->second;
        }
        else
        {
            pPickCounter = new PmPickCounter(&m_FancyPicker,
                                             m_bUseSeedString,
                                             m_RandomSeed,
                                             pChannel->GetName());
            m_PickCountMap[pChannel] = pPickCounter;
        }

        // Increment the counter by the number of methods written since
        // the last time the trigger was checked
        pPickCounter->IncrCounter(pEvent->GetMethodCount() -
                                  pPickCounter->GetCounter());
        pChannelWrapper->SetNextOnWrite(
                (UINT32)pPickCounter->GetCountRemaining());

        return pPickCounter->CheckCounter();
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnPercentMethods
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnPercentMethods::PmTrigger_OnPercentMethods
(
    const PmChannelDesc *pChannelDesc,
    const FancyPicker&   fancyPicker,
    bool                 bUseSeedString,
    UINT32               randomSeed
) :
    m_pChannelDesc(pChannelDesc),
    m_FancyPicker(fancyPicker),
    m_bUseSeedString(bUseSeedString),
    m_RandomSeed(randomSeed)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmTrigger_OnPercentMethods::~PmTrigger_OnPercentMethods()
{
    m_PercentMap.clear();
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnPercentMethods::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! \brief Return true if the channel matches the event channel matches
//! a channel in the channel descriptor.
//!
bool PmTrigger_OnPercentMethods::ChannelMatches(const PmEvent *pEvent)
{
    PmChannels channels = pEvent->GetChannels();
    MASSERT(channels.size() == 1);
    const PmChannel *pChannel = channels[0];

    if (pChannel->GetMethodCount() == 0)
        return false;

    return m_pChannelDesc->Matches(pEvent);
}

//--------------------------------------------------------------------
//! \brief Return true if the current method count in the channel matches
//! a desired count in the method list.  If the FancyPicer context has not
//! yet been created for the event channel, then creates and initialize
//! the necessary structure prior to checking the method count
//!
bool PmTrigger_OnPercentMethods::MethodCountMatches(const PmEvent *pEvent)
{
    PmChannels channels = pEvent->GetChannels();
    MASSERT(channels.size() == 1);
    const PmChannel *pChannel = channels[0];

    if (m_PercentMap.count(pChannel) == 0)
    {
        PmPercentMethods percentMethods;

        percentMethods.fpContext.LoopNum    = 0;
        percentMethods.fpContext.RestartNum = 0;
        percentMethods.fpContext.pJSObj     = 0;

        // Seed the random-number generator
        //
        if (!m_bUseSeedString)
        {
            percentMethods.fpContext.Rand.SeedRandom(m_RandomSeed);
        }
        else if (pChannel->GetName().length() > 0)
        {
            string seedString = pChannel->GetName();
            UINT64 BIGGEST_32_BIT_PRIME = 0x00000000fffffffbUL;
            UINT64 strSeed = 0;
            for (size_t ii = 0; ii < pChannel->GetName().length(); ++ii)
            {
                strSeed = (strSeed << 8) + (seedString[ii] & 0x00ff);
                strSeed %= BIGGEST_32_BIT_PRIME;
            }
            percentMethods.fpContext.Rand.SeedRandom(static_cast<UINT32>(strSeed));
        }

        m_FancyPicker.SetContext(&percentMethods.fpContext);
        m_PercentMap[pChannel] = percentMethods;

        if (m_FancyPicker.Type() == FancyPicker::T_INT)
        {
            m_PercentMap[pChannel].nextTriggerPercent = (FLOAT32)m_FancyPicker.Pick();
        }
        else
        {
            m_PercentMap[pChannel].nextTriggerPercent = m_FancyPicker.FPick();
        }
        ++ m_PercentMap[pChannel].fpContext.LoopNum;
    }

    return ((m_PercentMap[pChannel].nextTriggerPercent < ILWALID_METHOD_PERCENT) &&
            (CalcMethods(pChannel) == pEvent->GetMethodCount()));
}

//--------------------------------------------------------------------
//! \brief Update the next method count where the trigger should fire
//!
void PmTrigger_OnPercentMethods::UpdateNextMethodCount(const PmEvent *pEvent)
{
    PmChannels channels = pEvent->GetChannels();
    MASSERT(channels.size() == 1);
    const PmChannel *pChannel = channels[0];
    UINT32 methodCount = pEvent->GetMethodCount();

    MASSERT(m_PercentMap.count(pChannel) > 0);

    FLOAT32 incr;
    m_FancyPicker.SetContext(&m_PercentMap[pChannel].fpContext);

    do
    {
        if (m_FancyPicker.Type() == FancyPicker::T_INT)
        {
            incr = (FLOAT32)m_FancyPicker.Pick();
        }
        else
        {
            incr = m_FancyPicker.FPick();
        }

        ++m_PercentMap[pChannel].fpContext.LoopNum;

        if (incr > 0.0)
        {
            m_PercentMap[pChannel].nextTriggerPercent += incr;
        }
        else
        {
            m_PercentMap[pChannel].nextTriggerPercent = ILWALID_METHOD_PERCENT;
        }

        if (m_PercentMap[pChannel].nextTriggerPercent > MAX_VALID_METHOD_PERCENT)
        {
            m_PercentMap[pChannel].nextTriggerPercent = ILWALID_METHOD_PERCENT;
        }
    } while ((m_PercentMap[pChannel].nextTriggerPercent <= MAX_VALID_METHOD_PERCENT) &&
             (CalcMethods(pChannel) <= methodCount));
}

//--------------------------------------------------------------------
//! \brief Get the next method count where the trigger should fire
//!
UINT32 PmTrigger_OnPercentMethods::GetNextMethodCount(const PmEvent *pEvent)
{
    PmChannels channels = pEvent->GetChannels();
    MASSERT(channels.size() == 1);
    const PmChannel *pChannel = channels[0];
    MASSERT(m_PercentMap.count(pChannel) > 0);
    return  CalcMethods(pChannel);
}

//--------------------------------------------------------------------
//! \brief Get the next method percentage where the trigger should fire
//!
FLOAT32 PmTrigger_OnPercentMethods::GetNextMethodPercent(const PmEvent *pEvent)
{
    PmChannels channels = pEvent->GetChannels();
    MASSERT(channels.size() == 1);
    const PmChannel *pChannel = channels[0];
    MASSERT(m_PercentMap.count(pChannel) > 0);
    return  m_PercentMap[pChannel].nextTriggerPercent;
}

//--------------------------------------------------------------------
//! \brief Callwlate the next method count where the trigger should fire
//! based on the next percentage
//!
UINT32 PmTrigger_OnPercentMethods::CalcMethods(const PmChannel *pChannel)
{
    MASSERT(m_PercentMap.count(pChannel) > 0);
    UINT32 initializedMethodCount = pChannel->GetChannelInitMethodCount();

    // MAX_VALID_METHOD_PERCENT values 100.0 is a specify value, for pre-write strategy
    // it means the trigger will happen after last trace method.
    // And 99.0 means the trigger will happend before the last trace method.
    //
    // For example, total 100 trace method, 100% means it will be triggered before the
    // injected method NOP which method index is 100 and the start index is 0. 99% means
    // it will be triggered before the last method which method index is 99.
    if(m_PercentMap[pChannel].nextTriggerPercent >= MAX_VALID_METHOD_PERCENT)
        return (UINT32)(pChannel->GetMethodCount() - initializedMethodCount);
    else
        return  (UINT32)((m_PercentMap[pChannel].nextTriggerPercent *
                    ((FLOAT32)pChannel->GetMethodCount() - initializedMethodCount - 1) + 50.0) / 100.0);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnPercentMethodsWritten
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnPercentMethodsWritten::PmTrigger_OnPercentMethodsWritten
(
    const PmChannelDesc *pChannelDesc,
    const FancyPicker&   fancyPicker,
    bool                 bUseSeedString,
    UINT32               randomSeed
) :  PmTrigger_OnPercentMethods(pChannelDesc, fancyPicker,
                                bUseSeedString, randomSeed)
{
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_WRITE event.
//!
/* virtual */ bool PmTrigger_OnPercentMethodsWritten::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return pEvent->GetType() == PmEvent::METHOD_WRITE;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_WRITE event, the
//! channel matches, and the count matches a desired percentage of
//! methods from the FancyPicker
//!
/* virtual */ bool PmTrigger_OnPercentMethodsWritten::DoMatch
(
    const PmEvent *pEvent
)
{
    if (CouldMatch(pEvent) && ChannelMatches(pEvent))
    {
        PmChannelWrapper *pChannelWrapper =
            static_cast<const PmEvent_MethodWrite *>(pEvent)->GetPmChannelWrapper();
        bool   bReturn = false;

        if (MethodCountMatches(pEvent))
        {
            UpdateNextMethodCount(pEvent);
            bReturn = true;
        }
        UINT32 nextMethodCount = 0;
        if (GetNextMethodPercent(pEvent) <= MAX_VALID_METHOD_PERCENT)
        {
            MASSERT(GetNextMethodCount(pEvent) >= pEvent->GetMethodCount());
            nextMethodCount = GetNextMethodCount(pEvent) - pEvent->GetMethodCount();
        }
        pChannelWrapper->SetNextOnWrite(nextMethodCount);
        return bReturn;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnMethodExelwte
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnMethodExelwte::PmTrigger_OnMethodExelwte
(
    const PmChannelDesc *pChannelDesc,
    const FancyPicker&   fancyPicker,
    bool                 bUseSeedString,
    UINT32               randomSeed
) :
    m_pChannelDesc(pChannelDesc),
    m_FancyPicker(fancyPicker),
    m_bUseSeedString(bUseSeedString),
    m_RandomSeed(randomSeed)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmTrigger_OnMethodExelwte::~PmTrigger_OnMethodExelwte()
{
    for (PickCounterMap::iterator iter = m_PickCountMap.begin();
         iter != m_PickCountMap.end(); iter++)
    {
        delete iter->second;
    }
    m_PickCountMap.clear();
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnMethodExelwte::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_EXELWTE event.
//!
/* virtual */ bool PmTrigger_OnMethodExelwte::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return pEvent->GetType() == PmEvent::METHOD_EXELWTE;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_EXELWTE event, the
//! channel matches, and the count is part of the PickCounter.
//!
/* virtual */ bool PmTrigger_OnMethodExelwte::DoMatch(const PmEvent *pEvent)
{
    if (CouldMatch(pEvent) && m_pChannelDesc->Matches(pEvent))
    {
        PmChannels channels = pEvent->GetChannels();
        MASSERT(channels.size() == 1);
        PmChannel *pChannel = channels[0];

        // Get a pointer to the PmPickCounter class for this channel/trigger
        // pair.  Create one if it doesn't already exist.
        //
        PmPickCounter *pPickCounter;
        PickCounterMap::iterator iter = m_PickCountMap.find(pChannel);
        if (iter != m_PickCountMap.end())
        {
            pPickCounter = iter->second;
        }
        else
        {
            pPickCounter = new PmPickCounter(&m_FancyPicker,
                                             m_bUseSeedString,
                                             m_RandomSeed,
                                             pChannel->GetName());
            m_PickCountMap[pChannel] = pPickCounter;
        }

        // Increment the pick counter so that the count matches the
        // number of methods exelwted
        pPickCounter->IncrCounter(pEvent->GetMethodCount() -
                                  pPickCounter->GetCounter());
        return pPickCounter->CheckCounter();
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnPercentMethodsExelwted
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnPercentMethodsExelwted::PmTrigger_OnPercentMethodsExelwted
(
    const PmChannelDesc *pChannelDesc,
    const FancyPicker&   fancyPicker,
    bool                 bUseSeedString,
    UINT32               randomSeed
) : PmTrigger_OnPercentMethods(pChannelDesc, fancyPicker,
                               bUseSeedString, randomSeed)
{
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_EXELWTE event.
//!
/* virtual */ bool PmTrigger_OnPercentMethodsExelwted::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return pEvent->GetType() == PmEvent::METHOD_EXELWTE;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a METHOD_EXELWTE event, the
//! channel matches, and the percentage of methods exelwted matches
//! the fancy picker.
//!
/* virtual */ bool PmTrigger_OnPercentMethodsExelwted::DoMatch
(
    const PmEvent *pEvent
)
{
    if (CouldMatch(pEvent) &&
        ChannelMatches(pEvent) && MethodCountMatches(pEvent))
    {
        UpdateNextMethodCount(pEvent);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnMethodIdWrite
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnMethodIdWrite::PmTrigger_OnMethodIdWrite
(
    const PmChannelDesc *pChannelDesc,
    vector<UINT32> &ClassIds,
    string classType,
    vector<UINT32> &Methods,
    bool AfterWrite
) :
    m_pChannelDesc(pChannelDesc),
    m_ClassType(classType),
    m_AfterWrite(AfterWrite)
{
    MASSERT(m_pChannelDesc != NULL);
    m_ClassIds.insert(ClassIds.begin(), ClassIds.end());
    m_Methods.insert(Methods.begin(), Methods.end());
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnMethodIdWrite::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if the event is a METHOD_ID_WRITE event with the
//! correct classId, method, and AfterWrite flag.  This function does
//! not check whether the channel is correct.
//!
/* virtual */ bool PmTrigger_OnMethodIdWrite::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const PmEvent_MethodIdWrite *pMethodIdWriteEvent =
        static_cast<const PmEvent_MethodIdWrite*>(pEvent);

    return (pEvent->GetType() == PmEvent::METHOD_ID_WRITE &&
            m_Methods.count(pMethodIdWriteEvent->GetMethod()) &&
            m_AfterWrite == pMethodIdWriteEvent->GetAfterWrite() &&
            m_ClassIds.count(pMethodIdWriteEvent->GetClassId()));
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a matching METHOD_ID_WRITE event.
//!
/* virtual */ bool PmTrigger_OnMethodIdWrite::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent) && m_pChannelDesc->Matches(pEvent);
}

//--------------------------------------------------------------------
//! \brief Tell EventManager to generate METHOD_ID_WRITE events.
//!
/* virtual */ RC PmTrigger_OnMethodIdWrite::StartTest()
{
    RC rc;

    if (!m_ClassType.empty())
    {
        UINT32 supportedClass = 0;
        GpuDevices gpuDevices = GetPolicyManager()->GetGpuDevices();
        CHECK_RC(EngineClasses::GetFirstSupportedClass(
                    LwRmPtr().Get(),
                    gpuDevices[0],
                    m_ClassType,
                    &supportedClass));
        m_ClassIds.insert(supportedClass);
    }

    if (GetPolicyManager()->GetStrictMode())
    {
        GpuDevices gpuDevices = GetPolicyManager()->GetGpuDevices();
        vector<UINT32> classes(m_ClassIds.begin(), m_ClassIds.end());
        bool bTriggerPossible = false;
        UINT32 supportedClass = 0;

        for (GpuDevices::iterator ppGpuDev = gpuDevices.begin();
             (ppGpuDev != gpuDevices.end()) && !bTriggerPossible; ppGpuDev++)
        {
            rc = LwRmPtr()->GetFirstSupportedClass((UINT32)classes.size(),
                                                   &classes[0],
                                                   &supportedClass,
                                                   *ppGpuDev);
            if (rc == OK)
            {
                bTriggerPossible = true;
            }
            else if (rc != RC::UNSUPPORTED_FUNCTION)
            {
                CHECK_RC(rc);
            }
            rc.Clear();
        }

        if (!bTriggerPossible)
        {
            ErrPrintf("Trigger.OnMethodIdWrite : None of the following classes are supported on any GpuDevice\n");
            ErrPrintf("                          ");
            for (UINT32 ii = 0; ii < (UINT32)classes.size(); ii++)
            {
                ErrPrintf("0x%04x ", classes[ii]);
            }
            ErrPrintf("\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    for (set<UINT32>::iterator pClassId = m_ClassIds.begin();
         pClassId != m_ClassIds.end(); ++pClassId)
    {
        for (auto const &method : m_Methods)
        {
            GetEventManager()->HookMethodIdEvent(m_pChannelDesc, *pClassId,
                method, m_AfterWrite);
        }
    }
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnTimeUs
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
//! \param pHwPicker FancyPicker that tells when the trigger should
//!     fire.  Uses incremental counting, as per PmPickCounter.
//! \param pModelPicker Same as pHwPicker, but for amodel/fmodel.
//! \param pRtlPicker Same as pHwPicker, but for RTL.
//! \param RandomSeed Passed to PmPickCounter to seed the
//!     random-number generator.
//!
PmTrigger_OnTimeUs::PmTrigger_OnTimeUs
(
    const string &TimerName,
    const FancyPicker& hwPicker,
    const FancyPicker& modelPicker,
    const FancyPicker& rtlPicker,
    bool         bUseSeedString,
    UINT32       RandomSeed
) :
    m_TimerName(TimerName),
    m_pPickCounter(NULL),
    m_HwPicker(hwPicker),
    m_ModelPicker(modelPicker),
    m_RtlPicker(rtlPicker),
    m_bUseSeedString(bUseSeedString),
    m_RandomSeed(RandomSeed)
{
    MASSERT(TimerName != "");
}

//--------------------------------------------------------------------
//! Return true if the event is a matching TIMER event, or a
//! START_TIMER event on the matching timer.
//!
//! Note: The reason we return true on a START_TIMER is because
//! DoMatch() sets up the timer on a START_TIMER event.  So we need
//! to make sure that DoMatch() gets called, even though it'll
//! return false.
//!
/* virtual */ bool PmTrigger_OnTimeUs::CouldMatch(const PmEvent *pEvent) const
{
    if (pEvent->GetType() == PmEvent::TIMER)
    {
        const PmEvent_Timer *pTimerEvent =
            static_cast<const PmEvent_Timer*>(pEvent);
        return (m_pPickCounter != NULL &&
                pTimerEvent->GetTimerName() == m_TimerName &&
                !m_pPickCounter->Done() &&
                pTimerEvent->GetTimeUS() >= m_pPickCounter->GetNextCount());
    }
    else if (pEvent->GetType() == PmEvent::START_TIMER)
    {
        const PmEvent_StartTimer *pStartTimerEvent =
            static_cast<const PmEvent_StartTimer*>(pEvent);
        return pStartTimerEvent->GetTimerName() == m_TimerName;
    }

    return false;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a matching TIMER event.
//!
//! Return true if the event is a matching TIMER event.  Also, on a
//! matching TIMER or START_TIMER event, set up the pickCounter and
//! timer thread so that the next timer event fires.
//!
/* virtual */ bool PmTrigger_OnTimeUs::DoMatch(const PmEvent *pEvent)
{
    // Abort if this is not a matching TIMER or START_TIMER event.
    //
    if (!CouldMatch(pEvent))
    {
        return false;
    }

    // Process the event
    //
    if (pEvent->GetType() == PmEvent::TIMER)
    {
        // On a timer event, increment the counter
        //
        m_pPickCounter->IncrCounter(m_pPickCounter->GetCountRemaining());
    }
    else if (m_pPickCounter == NULL)
    {
        // On the first Action.StartTimer, create the counter.
        //
        MASSERT(pEvent->GetType() == PmEvent::START_TIMER);
        FancyPicker *pFancyPicker = NULL;
        switch (Platform::GetSimulationMode())
        {
            case Platform::Hardware:
                pFancyPicker = &m_HwPicker;
                break;
            case Platform::Fmodel:
            case Platform::Amodel:
                pFancyPicker = &m_ModelPicker;
                break;
            case Platform::RTL:
                pFancyPicker = &m_RtlPicker;
                break;
        }
        MASSERT(pFancyPicker != NULL);
        m_pPickCounter = new PmPickCounter(pFancyPicker, m_bUseSeedString,
                                           m_RandomSeed,
                                           "Trigger.OnTimeUs");
    }
    else
    {
        // On subsequent Action.StartTimers, cancel the current
        // timeout and restart the counter.
        //
        MASSERT(pEvent->GetType() == PmEvent::START_TIMER);
        TimerData timerData = { this, m_pPickCounter->GetNextCount() };
        RC rc = GetEventManager()->UnhookTimerEvent(TimerHandler,
                                                    &timerData,
                                                    sizeof(timerData));
        if (rc != OK)
        {
            ErrPrintf("Error calling UnhookTimerEvent in PmTrigger_OnTimeUs\n");
        }
        MASSERT(rc == OK);

        m_pPickCounter->Restart();
    }

    // Schedule the next event
    //
    if (!m_pPickCounter->Done())
    {
        UINT64 TimerStart = GetPolicyManager()->GetTimerStartUS(m_TimerName);
        UINT64 ElapsedTime = m_pPickCounter->GetNextCount();
        TimerData timerData = { this, ElapsedTime };

        RC rc = GetEventManager()->HookTimerEvent(TimerStart + ElapsedTime,
                                                  TimerHandler,
                                                  &timerData,
                                                  sizeof(timerData));
        if (rc != OK)
        {
            ErrPrintf("Error calling HookTimerEvent in PmTrigger_OnTimeUs\n");
        }
        MASSERT(rc == OK);
    }

    return pEvent->GetType() == PmEvent::TIMER;
}

//--------------------------------------------------------------------
//! \brief Handler that gets called on timeout to fire the next event
//!
/* static */ RC PmTrigger_OnTimeUs::TimerHandler(void *pTimerData)
{
    MASSERT(pTimerData != NULL);
    PmTrigger_OnTimeUs *pTrigger = ((TimerData*)pTimerData)->pTrigger;
    UINT64 ElapsedTimeUs = ((TimerData*)pTimerData)->ElapsedTimeUs;
    RC rc;

    PmEvent_Timer event(pTrigger->m_TimerName, ElapsedTimeUs);
    CHECK_RC(pTrigger->GetEventManager()->HandleEvent(&event));
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnPmuElpgEvent
//////////////////////////////////////////////////////////////////////
PmTrigger_OnPmuElpgEvent::PmTrigger_OnPmuElpgEvent
(
    const PmGpuDesc *pGpuDesc,
    UINT32           engineId,
    UINT32           interruptStatus
) :
    m_pGpuDesc(pGpuDesc),
    m_EngineId(engineId),
    m_InterruptStatus(interruptStatus)
{
}

/* virtual */ RC PmTrigger_OnPmuElpgEvent::IsSupported() const
{
    return m_pGpuDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true on a PMU_EVENT with the correct engine id, interrupt
//! status, unit id, and flags.  This method does not check whether it
//! came from the correct GPU.
//!
/* virtual */ bool PmTrigger_OnPmuElpgEvent::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const PmEvent_PmuEvent::PmuEvent *pPmuEvent =
        static_cast<const PmEvent_PmuEvent*>(pEvent)->GetPmuEvent();

    return ((pEvent->GetType() == PmEvent::PMU_EVENT) &&
            (pPmuEvent->format == PMU::PMU_MESSAGE) &&
            (pPmuEvent->data.msg.hdr.unitId == RM_PMU_UNIT_LPWR) &&
            ((pPmuEvent->data.msg.hdr.ctrlFlags & RM_FLCN_QUEUE_HDR_FLAGS_EVENT) != 0) &&
            0);
}

//--------------------------------------------------------------------
//! \brief Return true on a matching PMU_EVENT
//!
/* virtual */ bool PmTrigger_OnPmuElpgEvent::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent) && m_pGpuDesc->Matches(pEvent);
}

////////////////////////////////////////////////////////////////////
// PmTrigger_OnRmEvent
//////////////////////////////////////////////////////////////////////
PmTrigger_OnRmEvent::PmTrigger_OnRmEvent
(
    const PmGpuDesc *pGpuDesc,
    UINT32           EventId
) :
    m_pGpuDesc(pGpuDesc),
    m_EventId(EventId)
{
}

/* virtual */ RC PmTrigger_OnRmEvent::IsSupported() const
{
    return m_pGpuDesc->IsSupportedInTrigger();
}

/* virtual */ bool PmTrigger_OnRmEvent::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const PmEvent_RmEvent *pRmEvent = static_cast<const PmEvent_RmEvent*>(pEvent);

    return ((pEvent->GetType() == PmEvent::RM_EVENT) &&
            (pRmEvent->GetRmEventId() == m_EventId));
}

//--------------------------------------------------------------------
//! \brief Return true on a matching PMU_EVENT
//!
/* virtual */ bool PmTrigger_OnRmEvent::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent) && m_pGpuDesc->Matches(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnTraceEventCpu
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnTraceEventCpu::PmTrigger_OnTraceEventCpu
(
    const string &traceEventName,
    bool afterTraceEvent
) :
    m_TraceEventName(traceEventName),
    m_AfterTraceEvent(afterTraceEvent)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnTraceEventCpu::IsSupported() const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a TRACE_EVENT_CPU event.
//!
/* virtual */ bool PmTrigger_OnTraceEventCpu::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return pEvent->GetType() == PmEvent::TRACE_EVENT_CPU;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a matching TRACE_EVENT_CPU event.
//!
/* virtual */ bool PmTrigger_OnTraceEventCpu::DoMatch(const PmEvent *pEvent)
{
    const PmEvent_TraceEventCpu *pTraceEventCpu =
            static_cast<const PmEvent_TraceEventCpu *>(pEvent);
    return CouldMatch(pEvent) &&
        pTraceEventCpu->GetTraceEventName() == m_TraceEventName &&
        pTraceEventCpu->IsAfterTraceEvent() == m_AfterTraceEvent;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_ReplayablePageFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_ReplayablePageFault::PmTrigger_ReplayablePageFault
(
    const PmGpuDesc *pGpuDesc
) :
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_ReplayablePageFault::IsSupported() const
{
    return m_pGpuDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a robust-channel event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_ReplayablePageFault::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return (pEvent->GetType() == PmEvent::REPLAYABLE_FAULT);
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching robust-channel event
//!
/* virtual */ bool PmTrigger_ReplayablePageFault::DoMatch
(
    const PmEvent *pEvent
)
{
    return CouldMatch(pEvent) && m_pGpuDesc->Matches(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_CeRecoverableFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_CeRecoverableFault::PmTrigger_CeRecoverableFault
(
    const PmGpuDesc *pGpuDesc
) :
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_CeRecoverableFault::IsSupported() const
{
    return m_pGpuDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a fault buffer event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_CeRecoverableFault::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return (pEvent->GetType() == PmEvent::CE_RECOVERABLE_FAULT);
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching CE fault event
//!
/* virtual */ bool PmTrigger_CeRecoverableFault::DoMatch
(
    const PmEvent *pEvent
)
{
    return CouldMatch(pEvent) && m_pGpuDesc->Matches(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_NonReplayableFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_NonReplayableFault::PmTrigger_NonReplayableFault
(
    const PmChannelDesc *pChannelDesc
) :
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(m_pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_NonReplayableFault::IsSupported() const
{
    return m_pChannelDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a fault buffer event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_NonReplayableFault::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return (pEvent->GetType() == PmEvent::NON_REPLAYABLE_FAULT);
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching non-replayable fault event
//!
/* virtual */ bool PmTrigger_NonReplayableFault::DoMatch
(
    const PmEvent *pEvent
)
{
    if (pEvent->GetChannels().empty())
    {
        return CouldMatch(pEvent);
    }
    else
    {
        return CouldMatch(pEvent) && m_pChannelDesc->Matches(pEvent);
    }
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_PageFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_PageFault::PmTrigger_PageFault
(
    const PmChannelDesc *pChannelDesc
) :
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_PageFault::IsSupported() const
{
    RC rc;
    vector< shared_ptr<PmTrigger> >::const_iterator iter = m_Triggers.begin();
    for (; iter != m_Triggers.end(); ++iter)
    {
        CHECK_RC((*iter)->IsSupported());
    }
    return rc;
}

//--------------------------------------------------------------------
//! Return true if pEvent is a fault buffer event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_PageFault::CouldMatch
(
    const PmEvent *pEvent
) const
{
    vector< shared_ptr<PmTrigger> >::const_iterator iter = m_Triggers.begin();
    for (; iter != m_Triggers.end(); ++iter)
    {
        if ((*iter)->CouldMatch(pEvent))
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching non-replayable fault event
//!
/* virtual */ bool PmTrigger_PageFault::DoMatch
(
    const PmEvent *pEvent
)
{
    vector< shared_ptr<PmTrigger> >::const_iterator iter = m_Triggers.begin();
    for (; iter != m_Triggers.end(); ++iter)
    {
        if ((*iter)->DoMatch(pEvent))
        {
            return true;
        }
    }
    return false;
}

namespace PmFaulting
{
    bool IsSupportMMUFaultBuffer();
}

//--------------------------------------------------------------------
//! \brief Check if MMU_FAULT_BUFFER is supported then it can determine
//! detect page fault either via channel RC or non-replayable notifier.
//!
/* virtual */ RC PmTrigger_PageFault::StartTest()
{
    RC rc;

    m_Triggers.push_back(shared_ptr<PmTrigger>(
                         new PmTrigger_RobustChannel(m_pChannelDesc,
                                                     RC::RM_RCH_FIFO_ERROR_MMU_ERR_FLT)));
    if (PmFaulting::IsSupportMMUFaultBuffer())
    {
        GetPolicyManager()->SetNonReplayableFaultBufferNeeded();
        m_Triggers.push_back(shared_ptr<PmTrigger>(
                             new PmTrigger_NonReplayableFault(m_pChannelDesc)));
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_FaultBufferOverflow
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_FaultBufferOverflow::PmTrigger_FaultBufferOverflow
(
    const PmGpuDesc *pGpuDesc
) :
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_FaultBufferOverflow::IsSupported() const
{
    return m_pGpuDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a robust-channel event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_FaultBufferOverflow::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return (pEvent->GetType() == PmEvent::FAULT_BUFFER_OVERFLOW);
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching robust-channel event
//!
/* virtual */ bool PmTrigger_FaultBufferOverflow::DoMatch
(
    const PmEvent *pEvent
)
{
    return CouldMatch(pEvent) && m_pGpuDesc->Matches(pEvent);
}

//--------------------------------------------------------------------
//! \brief Check if MMU_FAULT_BUFFER is supported then it can determine
//! how to prepare for capturing fault buffer overflow
//!
/* virtual */ RC PmTrigger_FaultBufferOverflow::StartTest()
{
    RC rc;

    if (PmFaulting::IsSupportMMUFaultBuffer())
    {
        GetPolicyManager()->SetNonReplayableFaultBufferNeeded();
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_AccessCounterNotification
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_AccessCounterNotification::PmTrigger_AccessCounterNotification
(
    const PmGpuDesc *pGpuDesc
) :
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_AccessCounterNotification::IsSupported() const
{
    return m_pGpuDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a robust-channel event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_AccessCounterNotification::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return (pEvent->GetType() == PmEvent::ACCESS_COUNTER_NOTIFICATION);
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching robust-channel event
//!
/* virtual */ bool PmTrigger_AccessCounterNotification::DoMatch
(
    const PmEvent *pEvent
)
{
    return CouldMatch(pEvent) && m_pGpuDesc->Matches(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_PluginEventTrigger
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_PluginEventTrigger::PmTrigger_PluginEventTrigger
(
    const string &eventName
) :
    m_EventName(eventName)
{
}

//--------------------------------------------------------------------
//! \brief Return OK.
//!
/* virtual */ RC PmTrigger_PluginEventTrigger::IsSupported() const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is from trace_3d plugin && matches the event name.
//!
/* virtual */ bool PmTrigger_PluginEventTrigger::CouldMatch(const PmEvent *pEvent) const
{
    const PmEvent_Plugin* pEvent_Plugin = dynamic_cast<const PmEvent_Plugin *>(pEvent);
    if(pEvent_Plugin)
        return pEvent_Plugin->GetType() == PmEvent::PLUGIN && pEvent_Plugin->GetEventName() == m_EventName;
    else
        return false;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is from trace_3d plugin && matches the event name.
//!
/* virtual */ bool PmTrigger_PluginEventTrigger::DoMatch(const PmEvent *pEvent)
{
    return CouldMatch(pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnErrorLoggerInterrupt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnErrorLoggerInterrupt::PmTrigger_OnErrorLoggerInterrupt
(
    const PmGpuDesc *pGpuDesc,
    const std::string &interruptString
) :
    m_pGpuDesc(pGpuDesc),
    m_InterruptString(interruptString)
{
    MASSERT(pGpuDesc != nullptr);
}


//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnErrorLoggerInterrupt::IsSupported() const
{
    return m_pGpuDesc->IsSupportedInTrigger();
}

//--------------------------------------------------------------------
//! Return true if pEvent is a error logger event with a matching string.
//!
/* virtual */ bool PmTrigger_OnErrorLoggerInterrupt::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const PmEvent_OnErrorLoggerInterrupt *pOnErrorLoggerInterruptEvent =
        static_cast<const PmEvent_OnErrorLoggerInterrupt*>(pEvent);

    return pEvent->GetType() == PmEvent::ERROR_LOGGER_INTERRUPT &&
        pOnErrorLoggerInterruptEvent->GetInterruptString() == m_InterruptString;
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching error logger event
//!
/* virtual */ bool PmTrigger_OnErrorLoggerInterrupt::DoMatch
(
    const PmEvent *pEvent
)
{
    return CouldMatch(pEvent) && m_pGpuDesc->Matches(pEvent);
}

//--------------------------------------------------------------------
//! \brief Check if error logger trigger is supported
//!
/* virtual */ RC PmTrigger_OnErrorLoggerInterrupt::StartTest()
{
    RC rc;

    auto pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr != nullptr);
    if (pGpuDevMgr->NumDevices() > 1 || pGpuDevMgr->NumGpus() > 1)
    {
        ErrPrintf("Multi-GPU not supported by OnErrorLoggerInterrupt.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmTrigger_OnT3dPluginEvent
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmTrigger_OnT3dPluginEvent::PmTrigger_OnT3dPluginEvent
(
    const string &traceEventName
) :
    m_TraceEventName(traceEventName)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_OnT3dPluginEvent::IsSupported() const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a T3D_PLUGIN_EVENT event.
//!
/* virtual */ bool PmTrigger_OnT3dPluginEvent::CouldMatch
(
    const PmEvent *pEvent
) const
{
    return pEvent->GetType() == PmEvent::T3D_PLUGIN_EVENT;
}

//--------------------------------------------------------------------
//! \brief Return true if the event is a matching T3D_PLUGIN_EVENT event.
//!
/* virtual */ bool PmTrigger_OnT3dPluginEvent::DoMatch(const PmEvent *pEvent)
{
    const PmEvent_OnT3dPluginEvent *pT3dPluginEvent =
            static_cast<const PmEvent_OnT3dPluginEvent *>(pEvent);
    return CouldMatch(pEvent) &&
        pT3dPluginEvent->GetTraceEventName() == m_TraceEventName;
}
// PmTrigger_NonfatalPoisonError
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!

PmTrigger_NonfatalPoisonError::PmTrigger_NonfatalPoisonError
(
    const PmChannelDesc *pChannelDesc,
    string type
) :
    m_pChannelDesc(pChannelDesc),
    m_poisonType(type)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_NonfatalPoisonError::IsSupported() const
{
    RC rc;
    vector< shared_ptr<PmTrigger> >::const_iterator iter = m_Triggers.begin();
    for (; iter != m_Triggers.end(); ++iter)
    {
        CHECK_RC((*iter)->IsSupported());
    }
    return rc;
}

//--------------------------------------------------------------------
//! Return true if pEvent is a fault buffer event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_NonfatalPoisonError::CouldMatch
(
    const PmEvent *pEvent
) const
{
    const UINT32 CENumMax = 9; // 9 for GA100

    if ( pEvent->GetType() == PmEvent::NONE_FATAL_POISON_ERROR )
    {
        // Lwrrently, the info16 in notification data returned from
        // LW2080_NOTIFIERS_POISON_ERROR_NON_FATAL represents different poison error
        // Reference: https://confluence.lwpu.com/display/ASRC/RM+Event+Notifiers+for+Error+Containment+Errors
        // //hw/doc/gpu/ampere/ampere/design/Functional_Descriptions/Ampere_GPU_Fault_Containment_Functional_Description.docx

        // E10: SM poison HWW
        if ( m_poisonType == "E10" && pEvent->GetInfo16() == ROBUST_CHANNEL_GR_EXCEPTION )
        {
            return true;
        }

        // E12: CE poison error
        if ( m_poisonType == "E12" )
        {
            for ( unsigned int i = 0; i <= CENumMax; ++i )
            {
                if ( pEvent->GetInfo16() == ROBUST_CHANNEL_CEn_ERROR(i) )
                {
                    return true;
                }
            }
        }

        // E13: MMU poisoned fault
        if ( m_poisonType == "E13" && pEvent->GetInfo16() == ROBUST_CHANNEL_FIFO_ERROR_MMU_ERR_FLT)
        {
            return true;
        }

        // E16: GCC poison HWW
        if ( m_poisonType == "E16" && pEvent->GetInfo16() == ROBUST_CHANNEL_GR_EXCEPTION )
        {
            return true;
        }

        // E17: CTXSW (GPCCS/TPCCS) poison HWW
        if ( m_poisonType == "E17" && pEvent->GetInfo16() == ROBUST_CHANNEL_GR_EXCEPTION )
        {
            return true;
        }

        // poisonType is optional argument
        // return true directly if it is not specified and event type matches
        if (m_poisonType == "")
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching NONE_FATAL_POISON_ERROR event
//!
/* virtual */ bool PmTrigger_NonfatalPoisonError::DoMatch
(
    const PmEvent *pEvent
)
{
    return CouldMatch(pEvent);
}

/* virtual */ RC PmTrigger_NonfatalPoisonError::StartTest()
{
    RC rc;

    return rc;

}

//////////////////////////////////////////////////////////////////////
// PmTrigger_FatalPoisonError
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!

PmTrigger_FatalPoisonError::PmTrigger_FatalPoisonError
(
    const PmChannelDesc *pChannelDesc,
    string type
) :
    m_pChannelDesc(pChannelDesc),
    m_poisonType(type)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if this trigger can be exelwted.
//!
/* virtual */ RC PmTrigger_FatalPoisonError::IsSupported() const
{
    RC rc;
    vector< shared_ptr<PmTrigger> >::const_iterator iter = m_Triggers.begin();
    for (; iter != m_Triggers.end(); ++iter)
    {
        CHECK_RC((*iter)->IsSupported());
    }
    return rc;
}

//--------------------------------------------------------------------
//! Return true if pEvent is a fault buffer event with the correct
//! error code.  Does not check whether it happened on the right
//! channel.
//!
/* virtual */ bool PmTrigger_FatalPoisonError::CouldMatch
(
    const PmEvent *pEvent
) const
{
    if ( pEvent->GetType() == PmEvent::FATAL_POISON_ERROR )
    {
        // Lwrrently, the info16 in notification data returned from
        // LW2080_NOTIFIERS_POISON_ERROR_FATAL represents different poison error
        // Reference: https://confluence.lwpu.com/display/ASRC/RM+Event+Notifiers+for+Error+Containment+Errors
        // //hw/doc/gpu/ampere/ampere/design/Functional_Descriptions/Ampere_GPU_Fault_Containment_Functional_Description.docx

        // E06: L2 unsupported client poison
        if ( m_poisonType == "E06" && pEvent->GetInfo16() == LTC_ERROR )
        {
            return true;
        }

        // E09: FBHUB poison interrupt
        if ( m_poisonType == "E09" && pEvent->GetInfo16() == FB_MEMORY_ERROR )
        {
            return true;
        }

        // poisonType is optional argument
        // return true directly if it is not specified and event type matches
        if (m_poisonType == "")
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------
//! \brief Return true if pEvent is a matching FATAL_POISON_ERROR event
//!
/* virtual */ bool PmTrigger_FatalPoisonError::DoMatch
(
    const PmEvent *pEvent
)
{
    return CouldMatch(pEvent); // && m_pChannelDesc->Matches(pEvent);
}

/* virtual */ RC PmTrigger_FatalPoisonError::StartTest()
{
    RC rc;

    return rc;
}
