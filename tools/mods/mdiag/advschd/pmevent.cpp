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

#include "pmevent.h"
#include "gpu/include/gpudev.h"
#include "pmchan.h"
#include "pmgilder.h"
#include "pmpotevt.h"
#include "pmsurf.h"
#include "mdiag/utils/mdiagsurf.h"
#include "pmtest.h"
#include "pmtrigg.h"
#include "pmutils.h"
#include "core/include/utility.h"
#include "rmflcncmdif.h"
#include "rmpmucmdif.h"
#include "rmpolicy.h"
#include <algorithm>
#include <ctype.h>
#include <iomanip>
#include "gpu/utility/tsg.h"
#include "pmsmcengine.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/utils.h"
#include <sstream>

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent::PmEvent
(
    Type type
) :
    m_Type(type)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmEvent::~PmEvent()
{
}

//--------------------------------------------------------------------
//! \brief Notify the gilder that the event oclwrred
//!
//! The base-class implementation of this method simply passes the
//! event to PmGilder::EventOclwrred().  A subclass may override it in
//! order to send additional methods to the gilder.
//!
//! A typical override is to call PmGilder::EndPotentialEvent() right
//! after EventOclwrred().  For example, when you push a Trap on the
//! pushbuffer, it will only happen once.  The potential event should
//! end immediately after the trap oclwrs.
//!
/* virtual */ void PmEvent::NotifyGilder(PmGilder *pGilder) const
{
    pGilder->EventOclwrred(Clone());
}

//--------------------------------------------------------------------
//! \brief Return true if this event matches the gildfile entry.
//!
//! This method is used to compare this run of mods to the previous
//! run of mods, so this method should ignore any fields that change
//! from run to run.
//!
/* virtual */ bool PmEvent::Matches(const string &gildString) const
{
    RC rc;
    bool bMatches;

    rc = InnerMatches(gildString, &bMatches);
    if (rc != OK)
    {
        ErrPrintf("PmEvent::Matches failed (%s)\n", rc.Message());
        MASSERT(!"Should never get here in PmEvent::Matches()");
        return false;
    }

    return bMatches;
}

//--------------------------------------------------------------------
//! \brief Return true if this event is close enough to the gildfile
//!        description to match a required gildfile entry.
//!
//! This function is used by the gilder when waiting for required
//! events to be received to match required events in the gildfile.
//! Some events have success/failure indications embedded in their
//! gildstrings which should be ignored when checking if required
//! events have been received to avoid waiting forever (in case the
//! desired type of event oclwrs but indicates a failure instead of
//! success).  Since gilding uses the more strict Matches() API,
//! these types of failures will be caught at that time.
/* virtual */ bool PmEvent::MatchesRequiredEvent
(
    const string &gildString
) const
{
    return Matches(gildString);
}

//--------------------------------------------------------------------
//! \brief Indicates whether this event can occur without a potential event
//!
//! Some events can only occur when there is a pending "potential
//! event", and fail gilding otherwise.  Unless this method is
//! overridden in the subclass, the default assumption is that this
//! event can occur spontaneously, without a pending potential event.
//!
/* virtual */ bool PmEvent::RequiresPotentialEvent() const
{
    return false;
}

//--------------------------------------------------------------------
//! \brief Return the channel associated with the event, or NULL if none
//!
/* virtual */ PmChannels PmEvent::GetChannels() const
{
    PmChannels channels;
    return channels;
}

//--------------------------------------------------------------------
//! \brief Return the memory range associated with the event, if any.
//!
/* virtual */ const PmMemRange *PmEvent::GetMemRange() const
{
    return NULL;
}

//--------------------------------------------------------------------
//! \brief Return the subsurfaces associated with the event, if any.
//!
//! Unless overridden by a subclass, this simply calls GetMemRange()
//! and returns all subsurfaces that overlap the range.
//!
/* virtual */ PmSubsurfaces PmEvent::GetSubsurfaces() const
{
    const PmMemRange *pMemRange = GetMemRange();
    PmSubsurfaces matchingSubsurfaces;

    if (pMemRange != NULL)
    {
        PmSubsurfaces subsurfacesInSurf =
            pMemRange->GetSurface()->GetSubsurfaces();
        for (PmSubsurfaces::iterator ppSubsurface = subsurfacesInSurf.begin();
             ppSubsurface != subsurfacesInSurf.end(); ++ppSubsurface)
        {
            if (pMemRange->Overlaps(*ppSubsurface))
            {
                matchingSubsurfaces.push_back(*ppSubsurface);
            }
        }
    }

    return matchingSubsurfaces;
}

//--------------------------------------------------------------------
//! \brief Return the faulting GpuSubdevice associated with the event, if any.
//!
//! Return NULL if there is no associated subdevice, or if the concept
//! of a "faulting subdevice" doesn't apply for the event type.
//!
/* virtual */ GpuSubdevice *PmEvent::GetGpuSubdevice() const
{
    return NULL;
}

//--------------------------------------------------------------------
//! \brief Return the faulting GpuDevice associated with the event, if any.
//!
//! Return NULL if there is no associated subdevice, or if the concept
//! of a "faulting subdevice" doesn't apply for the event type.
//!
//! The default base-class method tries to extract a GpuDevice from
//! GetGpuSubdevice() and GetChannel(), in that order.  Override this
//! method in any subclass in which that won't work.
//!
/* virtual */ GpuDevice *PmEvent::GetGpuDevice() const
{
    GpuSubdevice *pGpuSubdevice;

    if ((pGpuSubdevice = GetGpuSubdevice()) != NULL)
    {
        return pGpuSubdevice->GetParentDevice();
    }
    else if (!GetChannels().empty())
    {
        // Assumption: all the channels should have gpudevice
        return GetChannels()[0]->GetGpuDevice();
    }
    else
    {
        return NULL;
    }
}

//--------------------------------------------------------------------
//! \brief Returns the method count associated with the event, 0 if none
//!
/* virtual */ UINT32 PmEvent::GetMethodCount() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the timestamp associated with the event, 0 if none
//!
/* virtual */const UINT64 PmEvent::GetTimeStamp() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the GPC ID associated with the event, 0 if none
//!
/* virtual */const UINT32 PmEvent::GetGPCID() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the Client ID associated with the event, 0 if none
//!
/* virtual */const UINT32 PmEvent::GetClientID() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the tsg name associated with the event, 0 if none
//!
/* virtual */const string PmEvent::GetTsgName() const
{
    return "";
}

//--------------------------------------------------------------------
//! \brief Returns the VEID associated with the event, 0 if none
//!
/* virtual */const UINT32 PmEvent::GetVEID() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the fault type associated with the event, 0 if none
//!
/* virtual */const UINT32 PmEvent::GetFaultType() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the access type associated with the event, 0 if none
//!
/* virtual */const UINT32 PmEvent::GetAccessType() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the address type associated with the event, 0 if none
//!
/* virtual */const UINT32 PmEvent::GetAddressType() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the MMU engine id associated with the event, 0 if none
//!
/* virtual */const UINT32 PmEvent::GetMMUEngineID() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Returns the info16 associated with the event, 0 if none
//!
/* virtual */const UINT16 PmEvent::GetInfo16() const
{
    return 0;
}

//--------------------------------------------------------------------
//! \brief Return the test associated with the event, or NULL if none.
//!
//! The default implementation extracts the test from GetChannel() and
//! GetMemRange().
//!
/* virtual */ PmTest *PmEvent::GetTest() const
{
    // Get the tests associated with the channel & surface for this
    // event, if any.
    //
    PmChannels channels = GetChannels();
    PmTest *pChannelTest = !channels.empty() ? channels[0]->GetTest() : NULL;

    const PmMemRange *pMemRange = GetMemRange();
    PmSurface *pSurface = pMemRange ? pMemRange->GetSurface() : NULL;
    PmTest *pSurfaceTest = pSurface ? pSurface->GetTest() : NULL;

    // If all non-NULL tests returned by the channel and surface all
    // the same, then return that test.  Otherwise, return NULL.
    //
    if (pChannelTest == NULL)
        return pSurfaceTest;
    else if (pSurfaceTest == NULL)
        return pChannelTest;
    else
        return (pChannelTest == pSurfaceTest) ? pChannelTest : NULL;
}

//--------------------------------------------------------------------
//! \brief Returns the trace event name associated with the event, 0 if none
//!
/* virtual */string PmEvent::GetTraceEventName() const
{
    return "";
}

/*virtual*/PmSmcEngine *PmEvent::GetSmcEngine() const
{
    PmTest * pPmTest = GetTest();
    if (pPmTest != nullptr)
    {
        PmSmcEngine * pSmcEngine = pPmTest->GetSmcEngine();
        return pSmcEngine;
    }

    return nullptr;
}
//--------------------------------------------------------------------
//! \brief Return the part of a gild string associated with a memRange
//!
//! Return a string that represents the gpuAddr at the start of the
//! memRange as both a UINT64 and a subsurface-name/offset pair.
//!
string PmEvent::GetGildStringForMemRange(const PmMemRange *pMemRange)
{
    MASSERT(pMemRange != NULL);
    UINT64 gpuAddr = pMemRange->GetGpuAddr();

    // Find the subsurface that contains gpuAddr.  In case more than
    // one matches (probably impossible), choose the smaller one, or
    // the alphabetically-lower one in case of a tie.
    //
    PmSubsurface *pBestSubsurface = NULL;
    string SubsurfaceName = "UNKNOWN";
    UINT64 SubsurfaceOffset = gpuAddr;

    PmSubsurfaces subsurfaces = pMemRange->GetSurface()->GetSubsurfaces();
    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        if (gpuAddr >= (*ppSubsurface)->GetGpuAddr() &&
            gpuAddr < (*ppSubsurface)->GetGpuAddr() +
                      (*ppSubsurface)->GetSize())
        {
            if (pBestSubsurface == NULL ||
                pBestSubsurface->GetSize() > (*ppSubsurface)->GetSize() ||
                (pBestSubsurface->GetSize() == (*ppSubsurface)->GetSize() &&
                 pBestSubsurface->GetName() > (*ppSubsurface)->GetName()))
            {
                pBestSubsurface = *ppSubsurface;
                SubsurfaceName = pBestSubsurface->GetName();
                SubsurfaceOffset = gpuAddr - pBestSubsurface->GetGpuAddr();
            }
        }
    }

    // Return the string
    //
    return Utility::StrPrintf(
        "0x%llx %s 0x%llx",
        gpuAddr, SubsurfaceName.c_str(), SubsurfaceOffset);
}

//--------------------------------------------------------------------
//! \brief Determine whether a set of fields from within 2 sets of
//!        tokenized gild strings matches
//!
//! \param MyGildFields    : Tokenized gild string from the event
//! \param TheirGildFields : Tokenized gild string from the gild file
//! \param FieldStart      : Starting field to match
//! \param FieldCount      : Number of fields to match
//! \param bAllowRegex     : Allow the fields to be regular expressions
//!                          when matching
//! \param pbMatches       : Pointer to returned match status
//!
//! \return OK if successful, not OK otherwise
RC PmEvent::DoFieldsMatch
(
    const vector<string> &MyGildFields,
    const vector<string> &TheirGildFields,
    UINT32 FieldStart,
    UINT32 FieldCount,
    bool bAllowRegex,
    bool *pbMatches
) const
{
    RC rc;

    MASSERT(pbMatches);

    *pbMatches = false;
    if ((static_cast<UINT32>(MyGildFields.size()) < (FieldStart + FieldCount)) ||
        (static_cast<UINT32>(TheirGildFields.size()) < (FieldStart + FieldCount)))
    {
        ErrPrintf("PmEvent::DoFieldsMatch not enough fields for comparison\n");
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 i = FieldStart; i < (FieldStart + FieldCount); i++)
    {
        // Fields that start with '/' are treated as regular expressions
        if (TheirGildFields[i][0] == '/')
        {
            // Return an error if a regular expression field is encountered
            // and regular expressions are not allowed
            if (!bAllowRegex)
            {
                ErrPrintf("PmEvent::DoFieldsMatch regex encountered in invalid field %d\n", i);
                return RC::SOFTWARE_ERROR;
            }
            RegExp regexp;
            CHECK_RC(regexp.Set(TheirGildFields[i].substr(1),
                                RegExp::SUB | RegExp::ICASE));

            if (!regexp.Matches(MyGildFields[i]))
                return rc;
        }
        else if (TheirGildFields[i] != MyGildFields[i])
        {
            return rc;
        }
    }

    // If this point is reached then all fields match
    *pbMatches = true;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Determine whether the gildString matches the event
//!
//! \param gildString : Gild string from the gild file
//! \param pbMatches  : Pointer to returned match status
//!
//! \return OK if successful, not OK otherwise
RC PmEvent::InnerMatches(const string &gildString, bool *pbMatches) const
{
    vector<string> theirGildFields;
    vector<string> myGildFields;
    RC rc;

    *pbMatches = false;

    CHECK_RC(Utility::Tokenizer(GetGildString(), " ", &myGildFields));
    CHECK_RC(Utility::Tokenizer(gildString, " ", &theirGildFields));

    if (myGildFields.size() != theirGildFields.size())
    {
        return rc;
    }
    // Empty gild strings are valid and can match
    if (myGildFields.size() == 0)
    {
        *pbMatches = true;
        return rc;
    }

    // Check if the first field on the line matches without allowing regular
    // expressions (the type of event is represented by the first field on
    // the line)
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 0, 1,
                           false, pbMatches));

    // If the first field on the line does not match or there are no more
    // fields to process, the matching process is complete
    if ((!(*pbMatches)) || (static_cast<UINT32>(myGildFields.size()) == 1))
        return rc;

    // Check the remaining fields of the gild string, but allow regular
    // expressions for the remaining fields on the line
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 1,
                           static_cast<UINT32>(myGildFields.size()) - 1,
                           true, pbMatches));

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_OnChannelReset
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_OnChannelReset::PmEvent_OnChannelReset
(
    LwRm::Handle hObject,
    LwRm* pLwRm
) :
    PmEvent(CHANNEL_RESET)
{
    PolicyManager *pPM = PolicyManager::Instance();
    m_hChannel = hObject;
    m_pLwRm = pLwRm;
    MASSERT(m_pLwRm);

    LwRm::Handle hClient = m_pLwRm->GetClientHandle();
    PmChannels channels = pPM->GetActiveChannels();
    Tsg *pTsg = Tsg::GetTsgFromHandle(hClient, m_hChannel);
    if(pTsg)
    {
        Tsg::Channels tsgChannels = pTsg->GetChannels();
        for(Tsg::Channels:: iterator ppTsgChannel = tsgChannels.begin();
                ppTsgChannel != tsgChannels.end(); ++ppTsgChannel)
        {
            for(PmChannels::iterator ppChannel = channels.begin();
                    ppChannel != channels.end(); ++ppChannel)
            {
                if((*ppTsgChannel)->GetHandle() == (*ppChannel)->GetHandle())
                {
                    m_pChannels.push_back((*ppChannel));
                }
            }
        }
    }
    else
    {
        for(PmChannels::iterator ppChannel = channels.begin();
                ppChannel != channels.end(); ++ppChannel)
        {
            if((*ppChannel)->GetHandle() == m_hChannel)
            {
                m_pChannels.push_back((*ppChannel));
                break;
            }
        }
    }
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for Callback events
//!
/* virtual */ PmEvent *PmEvent_OnChannelReset::Clone() const
{
    return new PmEvent_OnChannelReset(m_hChannel, m_pLwRm);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_OnChannelReset::GetGildString() const
{
    return "CHANNEL_RESET";
}

//--------------------------------------------------------------------
//! \brief Returns the channel
//!
/* virtual */ PmChannels PmEvent_OnChannelReset::GetChannels() const
{
    return m_pChannels;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_End
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for Start events
//!
/* virtual */ PmEvent *PmEvent_End::Clone() const
{
    return new PmEvent_End(m_pTest);
}

//--------------------------------------------------------------------
//! Override the base method to suppress sending this event to the gilder
//!
/* virtual */ void PmEvent_End::NotifyGilder(PmGilder *pGilder) const
{
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_End::GetGildString() const
{
    return "END";
}

/* virtual */ PmTest * PmEvent_End::GetTest() const
{
    return m_pTest;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_Start
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for Start events
//!
/* virtual */ PmEvent *PmEvent_Start::Clone() const
{
    return new PmEvent_Start(m_pTest);
}

//--------------------------------------------------------------------
//! Override the base method to suppress sending this event to the gilder
//!
/* virtual */ void PmEvent_Start::NotifyGilder(PmGilder *pGilder) const
{
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_Start::GetGildString() const
{
    return "START";
}

/* virtual */ PmTest * PmEvent_Start::GetTest() const
{
    return m_pTest;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_RobustChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_RobustChannel::PmEvent_RobustChannel
(
    PmChannel *pChannel,
    RC rc
) :
    PmEvent(ROBUST_CHANNEL),
    m_pChannel(pChannel),
    m_RC(rc)
{
    MASSERT(m_pChannel);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for RobustChannel events
//!
/* virtual */ PmEvent *PmEvent_RobustChannel::Clone() const
{
    return new PmEvent_RobustChannel(m_pChannel, m_RC);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
//! To make it easier to parse via perl and such, this function takes
//! the robust-channel error description, and "tokenizes" it by
//! replacing spaces with underscores and discarding all other
//! non-alphanumeric characters.
//!
/* virtual */ string PmEvent_RobustChannel::GetGildString() const
{
    string rawError = m_RC.Message();
    string cookedError;
    for (string::iterator pRawError = rawError.begin();
         pRawError != rawError.end(); ++pRawError)
    {
        if (isalnum(*pRawError))
            cookedError += *pRawError;
        else if (*pRawError == ' ' || *pRawError == '_')
            cookedError += '_';
    }

    return Utility::StrPrintf(
        "ROBUST_CHANNEL_ERR %s %s",
        cookedError.c_str(), m_pChannel->GetName().c_str());
}

//--------------------------------------------------------------------
//! \brief Returns channels
//!
/* virtual */ PmChannels PmEvent_RobustChannel::GetChannels() const
{
    PmChannels pmChannles;
    pmChannles.push_back(m_pChannel);
    return pmChannles;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_NonStallInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_NonStallInt::PmEvent_NonStallInt
(
    PmNonStallInt *pNonStallInt
) :
    PmEvent(NON_STALL_INT),
    m_pNonStallInt(pNonStallInt)
{
    MASSERT(pNonStallInt != NULL);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for NonStallInt events
//!
/* virtual */ PmEvent *PmEvent_NonStallInt::Clone() const
{
    return new PmEvent_NonStallInt(m_pNonStallInt);
}

//--------------------------------------------------------------------
//! \brief Notify the gilder that this event oclwrred
//!
/* virtual */ void PmEvent_NonStallInt::NotifyGilder(PmGilder *pGilder) const
{
    PmEvent::NotifyGilder(pGilder);
    pGilder->EndPotentialEvent(new PmPotential_NonStallInt(m_pNonStallInt));
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_NonStallInt::GetGildString() const
{
    return "NON_STALL_INT " + m_pNonStallInt->ToString();
}

//--------------------------------------------------------------------
//! \brief This class requires a potential event
//!
/* virtual */ bool PmEvent_NonStallInt::RequiresPotentialEvent() const
{
    return true;
}

//--------------------------------------------------------------------
//! \brief Returns channels
//!
/* virtual */ PmChannels PmEvent_NonStallInt::GetChannels() const
{
    PmChannels pmChannles;
    pmChannles.push_back(m_pNonStallInt->GetChannel());
    return pmChannles;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_SemaphoreRelease
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_SemaphoreRelease::PmEvent_SemaphoreRelease
(
    const PmMemRange &MemRange,
    UINT64 payload
) :
    PmEvent(SEMAPHORE_RELEASE),
    m_MemRange(MemRange),
    m_Payload(payload)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for SemaphoreRelease events
//!
/* virtual */ PmEvent *PmEvent_SemaphoreRelease::Clone() const
{
    return new PmEvent_SemaphoreRelease(m_MemRange, m_Payload);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_SemaphoreRelease::GetGildString() const
{
    return "SEMAPHORE_RELEASE " + GetGildStringForMemRange(&m_MemRange);
}

//--------------------------------------------------------------------
//! \brief Determine whether the gildString matches the event
//!
//! \param gildString : Gild string from the gild file
//! \param pbMatches  : Pointer to returned match status
//!
//! \return OK if successful, not OK otherwise
RC PmEvent_SemaphoreRelease::InnerMatches(const string &gildString, bool *pbMatches) const
{
    vector<string> theirGildFields;
    vector<string> myGildFields;
    RC rc;

    *pbMatches = false;

    CHECK_RC(Utility::Tokenizer(GetGildString(), " ", &myGildFields));
    CHECK_RC(Utility::Tokenizer(gildString, " ", &theirGildFields));
    if (myGildFields.size() != theirGildFields.size())
        return rc;

    // Check if the first field on the line matches without allowing regular
    // expressions (the type of event is represented by the first field on
    // the line)
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 0, 1,
                           false, pbMatches));

    // If the first field on the line does not match or there are no more
    // fields to process, the matching process is complete
    if ((!(*pbMatches)) || (static_cast<UINT32>(myGildFields.size()) == 1))
        return rc;

    // For semaphore releases, remove the second field on the line before
    // gilding the remainder of the line (the second field on the line
    // contains the address
    myGildFields.erase(myGildFields.begin() + 1);
    theirGildFields.erase(theirGildFields.begin() + 1);

    // This gild string has already been determined to be a semaphore
    // release event and there should be at least 2 other fields to compare
    // after removing the address
    MASSERT(static_cast<UINT32>(myGildFields.size()) > 1);

    // Check the remaining fields of the gild string, but allow regular
    // expressions for the remaining fields on the line
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 1,
                           static_cast<UINT32>(myGildFields.size()) - 1,
                           true, pbMatches));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the memory range associated with the semaphore
//!
/* virtual */ const PmMemRange *PmEvent_SemaphoreRelease::GetMemRange() const
{
    return &m_MemRange;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_WaitForIdle
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_WaitForIdle::PmEvent_WaitForIdle(PmChannel *pChannel) :
    PmEvent(WAIT_FOR_IDLE),
    m_pChannel(pChannel)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for Wait For Idle events
//!
/* virtual */ PmEvent *PmEvent_WaitForIdle::Clone() const
{
    return new PmEvent_WaitForIdle(m_pChannel);
}

//--------------------------------------------------------------------
//! Override the base method to suppress sending this event to the gilder
//!
/* virtual */ void PmEvent_WaitForIdle::NotifyGilder(PmGilder *pGilder) const
{
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_WaitForIdle::GetGildString() const
{
    if (m_pChannel)
        return "WAIT_FOR_IDLE " + m_pChannel->GetName();
    else
        return "WAIT_FOR_CHIP_IDLE";
}

//--------------------------------------------------------------------
//! \brief Returns channels
//!
/* virtual */ PmChannels PmEvent_WaitForIdle::GetChannels() const
{
    PmChannels pmChannles;
    if (m_pChannel)
    {
        pmChannles.push_back(m_pChannel);
    }
    return pmChannles;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_MethodWrite
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_MethodWrite::PmEvent_MethodWrite
(
    PmChannel *pChannel,
    PmChannelWrapper *pPmChannelWrapper,
    UINT32 methodNum
) :
    PmEvent(METHOD_WRITE),
    m_pChannel(pChannel),
    m_pPmChannelWrapper(pPmChannelWrapper),
    m_MethodNum(methodNum)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for Method Write events
//!
/* virtual */ PmEvent *PmEvent_MethodWrite::Clone() const
{
    return new PmEvent_MethodWrite(m_pChannel,
                                   m_pPmChannelWrapper,
                                   m_MethodNum);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_MethodWrite::GetGildString() const
{
    char strData[11];

    sprintf(strData, "%d", m_MethodNum);
    return "METHOD_WRITE " + m_pChannel->GetName() + " " + strData;
}

//--------------------------------------------------------------------
//! \brief Returns the channels
//!
/* virtual */ PmChannels PmEvent_MethodWrite::GetChannels() const
{
    PmChannels pmChannles;
    pmChannles.push_back(m_pChannel);
    return pmChannles;
}

//--------------------------------------------------------------------
//! \brief Returns the method count
//!
/* virtual */ UINT32 PmEvent_MethodWrite::GetMethodCount() const
{
    return m_MethodNum;
}

//--------------------------------------------------------------------
//! \brief Returns the PmChannelWrapper that generated the event
//!
PmChannelWrapper *PmEvent_MethodWrite::GetPmChannelWrapper() const
{
    return m_pPmChannelWrapper;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_MethodExelwte
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_MethodExelwte::PmEvent_MethodExelwte
(
    PmChannel *pChannel,
    UINT32 methodNum
) :
    PmEvent(METHOD_EXELWTE),
    m_pChannel(pChannel),
    m_MethodNum(methodNum)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for Method Execute events
//!
/* virtual */ PmEvent *PmEvent_MethodExelwte::Clone() const
{
    return new PmEvent_MethodExelwte(m_pChannel, m_MethodNum);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_MethodExelwte::GetGildString() const
{
    char strData[11];

    sprintf(strData, "%d", m_MethodNum);
    return "METHOD_EXELWTE " + m_pChannel->GetName() + " " + strData;
}

//--------------------------------------------------------------------
//! \brief Returns channels
//!
/* virtual */ PmChannels PmEvent_MethodExelwte::GetChannels() const
{
    PmChannels pmChannles;
    pmChannles.push_back(m_pChannel);
    return pmChannles;
}

//--------------------------------------------------------------------
//! \brief Returns the method count
//!
/* virtual */ UINT32 PmEvent_MethodExelwte::GetMethodCount() const
{
    return m_MethodNum;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_MethodIdWrite
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_MethodIdWrite::PmEvent_MethodIdWrite
(
    PmChannel *pChannel,
    UINT32 ClassId,
    UINT32 Method,
    bool AfterWrite
) :
    PmEvent(METHOD_ID_WRITE),
    m_pChannel(pChannel),
    m_ClassId(ClassId),
    m_Method(Method),
    m_AfterWrite(AfterWrite)
{
    MASSERT(m_pChannel != NULL);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for MethodId Write events
//!
/* virtual */ PmEvent *PmEvent_MethodIdWrite::Clone() const
{
    return new PmEvent_MethodIdWrite(m_pChannel, m_ClassId,
                                     m_Method, m_AfterWrite);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_MethodIdWrite::GetGildString() const
{
    return Utility::StrPrintf("METHOD_ID_WRITE %s 0x%04x 0x%08x %s",
                              m_pChannel->GetName().c_str(), m_ClassId,
                              m_Method, m_AfterWrite ? "true" : "false");
}

//--------------------------------------------------------------------
//! \brief Returns channels
//!
/* virtual */ PmChannels PmEvent_MethodIdWrite::GetChannels() const
{
    PmChannels pmChannles;
    pmChannles.push_back(m_pChannel);
    return pmChannles;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_StartTimer
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_StartTimer::PmEvent_StartTimer
(
    const string &TimerName
) :
    PmEvent(START_TIMER),
    m_TimerName(TimerName)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for StartTimer events
//!
/* virtual */ PmEvent *PmEvent_StartTimer::Clone() const
{
    return new PmEvent_StartTimer(m_TimerName);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_StartTimer::GetGildString() const
{
    return Utility::StrPrintf("START_TIMER %s", m_TimerName.c_str());
}

//////////////////////////////////////////////////////////////////////
// PmEvent_Timer
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_Timer::PmEvent_Timer
(
    const string &TimerName,
    UINT64 TimeUS
) :
    PmEvent(TIMER),
    m_TimerName(TimerName),
    m_TimeUS(TimeUS)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for Timer events
//!
/* virtual */ PmEvent *PmEvent_Timer::Clone() const
{
    return new PmEvent_Timer(m_TimerName, m_TimeUS);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_Timer::GetGildString() const
{
    return Utility::StrPrintf("TIMER %s %llu", m_TimerName.c_str(), m_TimeUS);
}

//////////////////////////////////////////////////////////////////////
// PmEvent_GildString
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_GildString::PmEvent_GildString
(
    const string &str
) :
    PmEvent(GILD_STRING),
    m_String(str)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for PmEvent_GildString
//!
/* virtual */ PmEvent *PmEvent_GildString::Clone() const
{
    return new PmEvent_GildString(m_String);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_GildString::GetGildString() const
{
    return m_String;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_PmuEvent
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_PmuEvent::PmEvent_PmuEvent
(
    GpuSubdevice   *pGpuSubdevice,
    const PmuEvent &pmuEvent
) :
    PmEvent(PMU_EVENT),
    m_pGpuSubdevice(pGpuSubdevice),
    m_PmuEvent(pmuEvent)
{
    MASSERT(m_pGpuSubdevice);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for PMU Events
//!
/* virtual */ PmEvent *PmEvent_PmuEvent::Clone() const
{
    return new PmEvent_PmuEvent(m_pGpuSubdevice, m_PmuEvent);
}

//--------------------------------------------------------------------
//! \brief Get a description for the particular PMU event
//!
string PmEvent_PmuEvent::PmuEventDescription() const
{
    if (m_PmuEvent.format == PMU::PG_LOG_ENTRY)
    {
        // Go ahead and log the timestamp here.  It will be ignored in the
        // Matches() function
        return Utility::StrPrintf("PG_LOG %d %d 0x%08x 0x%08x",
                                  m_PmuEvent.data.pgLogEntry.eventType,
                                  m_PmuEvent.data.pgLogEntry.ctrlId,
                                  m_PmuEvent.data.pgLogEntry.status,
                                  m_PmuEvent.data.pgLogEntry.timeStamp);
    }
    const RM_FLCN_MSG_PMU *pMessage = &m_PmuEvent.data.msg;
    switch (pMessage->hdr.unitId)
    {
        case RM_PMU_UNIT_REWIND:
            return "REWIND";
        case RM_PMU_UNIT_SEQ:
            return "UNSUPPORTED";
        case RM_PMU_UNIT_LPWR:
            return "PG_UNKNOWN";
        case RM_PMU_UNIT_INIT:
            return "INIT";
        case RM_PMU_UNIT_GCX:
            return "GCX";
        default:
            break;
    }

    return "UNKNOWN";
}
//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_PmuEvent::GetGildString() const
{

    return Utility::StrPrintf("PMU_EVENT %d:%d %s",
                           m_pGpuSubdevice->GetParentDevice()->GetDeviceInst(),
                           m_pGpuSubdevice->GetSubdeviceInst(),
                           PmuEventDescription().c_str());
}

//--------------------------------------------------------------------
//! \brief Determine whether the gildString matches the event
//!
//! \param gildString : Gild string from the gild file
//! \param pbMatches  : Pointer to returned match status
//!
//! \return OK if successful, not OK otherwise
RC PmEvent_PmuEvent::InnerMatches(const string &gildString, bool *pbMatches) const
{
    vector<string> theirGildFields;
    vector<string> myGildFields;
    RC rc;

    *pbMatches = false;

    CHECK_RC(Utility::Tokenizer(GetGildString(), " ", &myGildFields));
    CHECK_RC(Utility::Tokenizer(gildString, " ", &theirGildFields));

    // There must be at least 3 fields on the line to have any potential of
    // matching a PMU event
    if ((myGildFields.size() != theirGildFields.size()) ||
        (theirGildFields.size() < 3))
        return rc;

    // Check if the first 3 fields on the line matches without allowing regular
    // expressions (the first 3 fields of a PMU event are necessary to
    // determine how the rest of the line should be gilded)
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 0, 3,
                           false, pbMatches));

    // If the first 3 fields on the line do not match or there are no more
    // fields to process, the matching process is complete
    if ((!(*pbMatches)) || (static_cast<UINT32>(myGildFields.size()) == 3))
        return rc;

    // At this point both the event being gilded and the event attached to
    // this object is a PMU Event
    if (myGildFields[2] == "PG_LOG")
    {
        // Remove the timestamp field from PG_LOG entries
        myGildFields.pop_back();
        theirGildFields.pop_back();
    }

    // Check the remaining fields of the gild string, but allow regular
    // expressions for the remaining fields on the line
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 3,
                           static_cast<UINT32>(myGildFields.size()) - 3,
                           true, pbMatches));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Determine whether the gildString matches the event
//!
//! \param gildString : Gild string from the gild file
//! \param pbMatches  : Pointer to returned match status
//!
//! \return OK if successful, not OK otherwise
RC PmEvent_PmuEvent::InnerMatchesRequiredEvent(const string &gildString, bool *pbMatches) const
{
    vector<string> theirGildFields;
    vector<string> myGildFields;
    RC rc;

    *pbMatches = false;

    CHECK_RC(Utility::Tokenizer(GetGildString(), " ", &myGildFields));
    CHECK_RC(Utility::Tokenizer(gildString, " ", &theirGildFields));

    // There must be at least 3 fields on the line to have any potential of
    // matching a PMU event
    if ((myGildFields.size() != theirGildFields.size()) ||
        (theirGildFields.size() < 3))
        return rc;

    // Check if the first 3 fields on the line matches without allowing regular
    // expressions (the first 3 fields of a PMU event are necessary to
    // determine how the rest of the line should be gilded)
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 0, 3,
                           false, pbMatches));

    // If the first 3 fields on the line do not match or there are no more
    // fields to process, the matching process is complete
    if ((!(*pbMatches)) || (static_cast<UINT32>(myGildFields.size()) == 3))
        return rc;

    // The following UNIT strings have pass/failure codes in their gild strings
    // These codes should be ignored when checking if required events have been
    // received.  A failure code will be caught at gild time
    if (myGildFields[2] == "SEQ")
    {
        // Only the first 3 fields need to be checked for these UNIT IDs to
        // indicate a possible match
        return rc;
    }
    else if (myGildFields[2] == "PG_LOG")
    {
        // Remove the timestamp field from PG_LOG entries
        myGildFields.pop_back();
        theirGildFields.pop_back();
    }

    // Check the remaining fields of the gild string, but allow regular
    // expressions for the remaining fields on the line
    CHECK_RC(DoFieldsMatch(myGildFields, theirGildFields, 3,
                           static_cast<UINT32>(myGildFields.size()) - 3,
                           true, pbMatches));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return true if this event is close enough to the gildfile
//!        description to match a required gildfile entry.
//!
/* virtual */ bool PmEvent_PmuEvent::MatchesRequiredEvent
(
    const string &gildString
) const
{
    RC rc;
    bool bMatches;

    rc = InnerMatchesRequiredEvent(gildString, &bMatches);
    if (rc != OK)
    {
        ErrPrintf("PmEvent_PmuEvent::MatchesRequiredEvent failed (%s)\n",
                  rc.Message());
        MASSERT(!"Should never get here in PmEvent_PmuEvent::MatchesRequiredEvent()");
        return false;
    }

    return bMatches;
}

//--------------------------------------------------------------------
//! \brief Return the GpuDevice where the PMU event oclwrred
//!
/* virtual */ GpuDevice *PmEvent_PmuEvent::GetGpuDevice() const
{
    return m_pGpuSubdevice->GetParentDevice();
}

//--------------------------------------------------------------------
//! \brief Return the GpuSubdevice where the PMU event oclwrred
//!
/* virtual */ GpuSubdevice *PmEvent_PmuEvent::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//--------------------------------------------------------------------
//! \brief Return the PMU event
//!
/* virtual */ const PmEvent_PmuEvent::PmuEvent *PmEvent_PmuEvent::GetPmuEvent() const
{
    return &m_PmuEvent;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_RmEvent
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_RmEvent::PmEvent_RmEvent(GpuSubdevice *pSubdev, UINT32 EventId)
 :
   PmEvent(RM_EVENT),
   m_pSubdev(pSubdev),
   m_EventId(EventId)
{
    MASSERT(pSubdev);
}
//--------------------------------------------------------------------
PmEvent* PmEvent_RmEvent::Clone() const
{
    return new PmEvent_RmEvent(m_pSubdev, m_EventId);
}
//--------------------------------------------------------------------
string PmEvent_RmEvent::GetGildString() const
{
    string GildStr;
    switch (m_EventId)
    {
        case ON_POWER_DOWN_GRAPHICS_ENTER:
            GildStr = "ON_POWER_DOWN_GRAPHICS_ENTER";
            break;
        case ON_POWER_DOWN_GRAPHICS_COMPLETE:
            GildStr = "ON_POWER_DOWN_GRAPHICS_COMPLETE";
            break;
        case ON_POWER_UP_GRAPHICS_ENTER:
            GildStr = "ON_POWER_UP_GRAPHICS_ENTER";
            break;
        case ON_POWER_UP_GRAPHICS_COMPLETE:
            GildStr = "ON_POWER_UP_GRAPHICS_COMPLETE";
            break;
        case ON_POWER_DOWN_VIDEO_ENTER:
            GildStr = "ON_POWER_DOWN_VIDEO_ENTER";
            break;
        case ON_POWER_DOWN_VIDEO_COMPLETE:
            GildStr = "ON_POWER_DOWN_VIDEO_COMPLETE";
            break;
        case ON_POWER_UP_VIDEO_ENTER:
            GildStr = "ON_POWER_UP_VIDEO_ENTER";
            break;
        case ON_POWER_UP_VIDEO_COMPLETE:
            GildStr = "ON_POWER_UP_VIDEO_COMPLETE";
            break;
        case ON_POWER_DOWN_VIC_ENTER:
            GildStr = "ON_POWER_DOWN_VIC_ENTER";
            break;
        case ON_POWER_DOWN_VIC_COMPLETE:
            GildStr = "ON_POWER_DOWN_VIC_COMPLETE";
            break;
        case ON_POWER_UP_VIC_ENTER:
            GildStr = "ON_POWER_UP_VIC_ENTER";
            break;
        case ON_POWER_UP_VIC_COMPLETE:
            GildStr = "ON_POWER_UP_VIC_COMPLETE";
            break;
        case ON_POWER_DOWN_MSPG_ENTER:
            GildStr = "ON_POWER_DOWN_MSPG_ENTER";
            break;
        case ON_POWER_DOWN_MSPG_COMPLETE:
            GildStr = "ON_POWER_DOWN_MSPG_COMPLETE";
            break;
        case ON_POWER_UP_MSPG_ENTER:
            GildStr = "ON_POWER_UP_MSPG_ENTER";
            break;
        case ON_POWER_UP_MSPG_COMPLETE:
            GildStr = "ON_POWER_UP_MSPG_COMPLETE";
            break;
        default:
            MASSERT(!"Unknown RmEvent");
            break;
    }
    return GildStr;
}
//--------------------------------------------------------------------
GpuDevice *PmEvent_RmEvent::GetGpuDevice() const
{
    return m_pSubdev->GetParentDevice();
}
//--------------------------------------------------------------------
GpuSubdevice *PmEvent_RmEvent::GetGpuSubdevice() const
{
    return m_pSubdev;
}
//--------------------------------------------------------------------
UINT32 PmEvent_RmEvent::GetRmEventId() const
{
    return m_EventId;
}
//--------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////
// PmEvent_TraceEventCpu
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_TraceEventCpu::PmEvent_TraceEventCpu
(
    const string &traceEventName,
    PmTest * pTest,
    LwRm::Handle chHandle,
    bool afterTraceEvent
) :
    PmEvent(TRACE_EVENT_CPU),
    m_TraceEventName(traceEventName),
    m_pTest(pTest),
    m_ChannelHandle(chHandle),
    m_AfterTraceEvent(afterTraceEvent)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for trace event cpu events
//!
/* virtual */ PmEvent *PmEvent_TraceEventCpu::Clone() const
{
    return new PmEvent_TraceEventCpu(m_TraceEventName, m_pTest,
                                     m_ChannelHandle, m_AfterTraceEvent);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_TraceEventCpu::GetGildString() const
{
    return "TRACE_EVENT_CPU " + m_TraceEventName;
}

//--------------------------------------------------------------------
/* virtual */ string PmEvent_TraceEventCpu::GetTraceEventName() const
{
    return m_TraceEventName;
}

//--------------------------------------------------------------------
/* virtual */ PmChannels PmEvent_TraceEventCpu::GetChannels() const
{
    PmChannels channels;
    PolicyManager *pPM = PolicyManager::Instance();
    PmChannel *pPmChannel = pPM->GetChannel(m_ChannelHandle);

    if (pPmChannel != nullptr)
    {
        channels.push_back(pPmChannel);
    }

    return channels;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_GMMUFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_GMMUFault::PmEvent_GMMUFault
(
    Type type,
    GpuSubdevice *pGpuSubdevice,
    const PmMemRange &memRange,
    UINT32 GPCID,
    UINT32 clientID,
    UINT32 faultType,
    UINT64 timestamp,
    const PmChannels &channels
) :
    PmEvent(type),
    m_pGpuSubdevice(pGpuSubdevice),
    m_MemRange(memRange),
    m_GPCID(GPCID),
    m_ClientID(clientID),
    m_FaultType(faultType),
    m_TimeStamp(timestamp),
    m_channels(channels)
{
    MASSERT(pGpuSubdevice != NULL);
}

//--------------------------------------------------------------------
/* virtual */ GpuSubdevice *PmEvent_GMMUFault::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//--------------------------------------------------------------------
//! \brief Return the fault type associated with the fault
//!
/* virtual */ const UINT32 PmEvent_GMMUFault::GetFaultType() const
{
    return m_FaultType;
}

//--------------------------------------------------------------------
//! \brief Return the client ID associated with the fault
//!
/* virtual */ const UINT32 PmEvent_GMMUFault::GetClientID() const
{
    return m_ClientID;
}

//--------------------------------------------------------------------
//! \brief Return the GPC ID associated with the fault
//!
/* virtual */ const UINT32 PmEvent_GMMUFault::GetGPCID() const
{
    return m_GPCID;
}

//--------------------------------------------------------------------
//! \brief Return the timestamp associated with the fault
//!
/* virtual */ const UINT64 PmEvent_GMMUFault::GetTimeStamp() const
{
    return m_TimeStamp;
}

//--------------------------------------------------------------------
//! \brief Return the tsg name associated with the fault
//!
/* virtual */ const string PmEvent_GMMUFault::GetTsgName() const
{
    if (m_channels.empty() || (m_channels[0]->GetTsg() == NULL))
    {
        Printf(Tee::PriWarn, "Warning: TSG is not available in PmEvent_ReplayableFault.\n");
        return "";
    }
    else
    {
        return m_channels[0]->GetTsg()->GetName();
    }
}

//--------------------------------------------------------------------
//! \brief Return the memory range associated with the fault
//!
/* virtual */ const PmMemRange *PmEvent_GMMUFault::GetMemRange() const
{
    return &m_MemRange;
}

//--------------------------------------------------------------------
//! \brief Return channels
//!
/* virtual */ PmChannels PmEvent_GMMUFault::GetChannels() const
{
    return m_channels;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_ReplayableFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_ReplayableFault::PmEvent_ReplayableFault
(
    GpuSubdevice *pGpuSubdevice,
    PmReplayableInt *pReplayableInt,
    const PmMemRange &memRange,
    UINT32 GPCID,
    UINT32 clientID,
    UINT32 faultType,
    UINT64 timestamp,
    UINT32 veid,
    const PmChannels &channels
) :
    PmEvent_GMMUFault(REPLAYABLE_FAULT, pGpuSubdevice,
        memRange, GPCID, clientID,
        faultType, timestamp, channels),
    m_pReplayableInt(pReplayableInt),
    m_VEID(veid)
{
    MASSERT(pReplayableInt != NULL);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for NonStallInt events
//!
/* virtual */ PmEvent *PmEvent_ReplayableFault::Clone() const
{
    return new PmEvent_ReplayableFault(m_pGpuSubdevice, m_pReplayableInt,
        m_MemRange, m_GPCID, m_ClientID, m_FaultType, m_TimeStamp, m_VEID, m_channels);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_ReplayableFault::GetGildString() const
{
    return "REPLAYABLE_FAULT " + m_pReplayableInt->ToString();
}

//--------------------------------------------------------------------
//! \brief Return the VEID associated with the fault
//!
/* virtual */ const UINT32 PmEvent_ReplayableFault::GetVEID() const
{
    return m_VEID;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_CeRecoverableFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_CeRecoverableFault::PmEvent_CeRecoverableFault
(
    GpuSubdevice *pGpuSubdevice,
    PmNonReplayableInt *pNonReplayableInt,
    const PmMemRange &memRange,
    UINT32 GPCID,
    UINT32 clientID,
    UINT32 faultType,
    UINT64 timestamp,
    const PmChannels &channels
) :
    PmEvent_GMMUFault(CE_RECOVERABLE_FAULT, pGpuSubdevice,
        memRange, GPCID, clientID,
        faultType, timestamp, channels),
    m_pNonReplayableInt(pNonReplayableInt)
{
    MASSERT(m_pNonReplayableInt != NULL);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for CE recoverable fault events
//!
/* virtual */ PmEvent *PmEvent_CeRecoverableFault::Clone() const
{
    return new PmEvent_CeRecoverableFault(m_pGpuSubdevice,
        m_pNonReplayableInt, m_MemRange, m_GPCID, m_ClientID, m_FaultType,
        m_TimeStamp, m_channels);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_CeRecoverableFault::GetGildString() const
{
    return "CE_RECOVERABLE_FAULT " + m_pNonReplayableInt->ToString();
}

//////////////////////////////////////////////////////////////////////
// PmEvent_NonReplayableFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_NonReplayableFault::PmEvent_NonReplayableFault
(
    GpuSubdevice *pGpuSubdevice,
    PmNonReplayableInt *pNonReplayableInt,
    const PmMemRange &memRange,
    UINT32 GPCID,
    UINT32 clientID,
    UINT32 faultType,
    UINT64 timestamp,
    const PmChannels &channels
) :
    PmEvent_GMMUFault(NON_REPLAYABLE_FAULT, pGpuSubdevice,
        memRange, GPCID, clientID,
        faultType, timestamp, channels),
    m_pNonReplayableInt(pNonReplayableInt)
{
    MASSERT(m_pNonReplayableInt != NULL);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for non-replayable fault events
//!
/* virtual */ PmEvent *PmEvent_NonReplayableFault::Clone() const
{
    return new PmEvent_NonReplayableFault(m_pGpuSubdevice,
        m_pNonReplayableInt, m_MemRange, m_GPCID, m_ClientID, m_FaultType,
        m_TimeStamp, m_channels);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_NonReplayableFault::GetGildString() const
{
    return "NON_REPLAYABLE_FAULT " + m_pNonReplayableInt->ToString();
}

//////////////////////////////////////////////////////////////////////
// PmEvent_FaultBufferOverflow
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_FaultBufferOverflow::PmEvent_FaultBufferOverflow
(
    GpuSubdevice *pGpuSubdevice
) :
    PmEvent(FAULT_BUFFER_OVERFLOW),
    m_pGpuSubdevice(pGpuSubdevice)
{
    MASSERT(pGpuSubdevice != NULL);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for FaultBufferOverflow events
//!
/* virtual */ PmEvent *PmEvent_FaultBufferOverflow::Clone() const
{
    return new PmEvent_FaultBufferOverflow(m_pGpuSubdevice);
}

//--------------------------------------------------------------------
/* virtual */ GpuSubdevice *PmEvent_FaultBufferOverflow::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_FaultBufferOverflow::GetGildString() const
{
    return "FAULT_BUFFER_OVERFLOW";
}

//////////////////////////////////////////////////////////////////////
// PmEvent_AccessCounterNotification
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_AccessCounterNotification::PmEvent_AccessCounterNotification
(
    GpuSubdevice *pGpuSubdevice,
    PmAccessCounterInt *pAccessCounterInt,
    const PmMemRange &memRange,
    const UINT64 instanceBlockPointer,
    const UINT32 aperture,
    const UINT32 accessType,
    const UINT32 addressType,
    const UINT32 counterValue,
    const UINT32 peerID,
    const UINT32 mmuEngineID,
    const UINT32 bank,
    const UINT32 notifyTag,
    const UINT32 subGranularity,
    const UINT32 instAperture,
    const bool   isVirtualAccess,
    const PmChannels &channels
) :
    PmEvent(ACCESS_COUNTER_NOTIFICATION),
    m_pGpuSubdevice(pGpuSubdevice),
    m_pAccessCounterInt(pAccessCounterInt),
    m_MemRange(memRange),
    m_InstanceBlockPointer(instanceBlockPointer),
    m_Aperture(aperture),
    m_AccessType(accessType),
    m_AddressType(addressType),
    m_CounterValue(counterValue),
    m_PeerID(peerID),
    m_MmuEngineID(mmuEngineID),
    m_Bank(bank),
    m_NotifyTag(notifyTag),
    m_SubGranularity(subGranularity),
    m_InstAperture(instAperture),
    m_IsVirtualAccess(isVirtualAccess),
    m_channels(channels)
{
    MASSERT(pGpuSubdevice != NULL);
    MASSERT(pAccessCounterInt != NULL);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for NonStallInt events
//!
/* virtual */ PmEvent *PmEvent_AccessCounterNotification::Clone() const
{
    return new PmEvent_AccessCounterNotification(
        m_pGpuSubdevice, m_pAccessCounterInt, m_MemRange, m_InstanceBlockPointer, m_Aperture,
        m_AccessType, m_AddressType, m_CounterValue, m_PeerID, m_MmuEngineID, m_Bank, m_NotifyTag,
        m_SubGranularity, m_InstAperture, m_IsVirtualAccess, m_channels);
}

//--------------------------------------------------------------------
/* virtual */ GpuSubdevice *PmEvent_AccessCounterNotification::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//--------------------------------------------------------------------
/* virtual */ const PmMemRange *PmEvent_AccessCounterNotification::GetMemRange() const
{
    return &m_MemRange;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetAperture() const
{
    return m_Aperture;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetAccessType() const
{
    return m_AccessType;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetAddressType() const
{
    return m_AddressType;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetPeerID() const
{
    return m_PeerID;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetMMUEngineID() const
{
    return m_MmuEngineID;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetBank() const
{
    return m_Bank;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetNotifyTag() const
{
    return m_NotifyTag;
}

//--------------------------------------------------------------------
/* virtual */ const UINT32 PmEvent_AccessCounterNotification::GetSubGranularity() const
{
    return m_SubGranularity;
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_AccessCounterNotification::GetGildString() const
{
    // Generating a string looks like:
    // ACCESS_COUNTER_NOTIFICATION 0 Type 0x1 AddrType 0x1 PeerId 0x0 TestID 0 SurfName chunk_0.tex SubGranularity 0x00000000
    stringstream desc;

    desc << "ACCESS_COUNTER_NOTIFICATION " << m_pAccessCounterInt->ToString();
    desc << " Type 0x" << hex << m_AccessType;
    desc << " AddrType 0x" << m_AddressType;
    desc << " InstAperture 0x" << m_InstAperture;
    desc << " PeerId 0x" << m_PeerID;
    desc << " TestID " << dec << m_MemRange.GetSurface()->GetTest()->GetTestId();
    desc << " SurfName " << m_MemRange.GetMdiagSurf()->GetName();
    desc << " SubGranularity 0x" << setw(sizeof(m_SubGranularity)*2) << setfill('0') << hex << m_SubGranularity;
    
    if (GetGpuSubdevice()->IsSMCEnabled() && m_IsVirtualAccess)
    {
        PmChannels pmChannels = GetChannels();
        MASSERT(pmChannels.size() != 0);
        LwRm *pLwRm = pmChannels[0]->GetLwRmPtr();
        UINT32 engineId = pmChannels[0]->GetEngineId();
        LWGpuResource* lwgpu = PolicyManager::Instance()->GetLWGpuResourceBySubdev(GetGpuSubdevice());
        UINT32 smcEngineId = MDiagUtils::GetGrEngineOffset(engineId);

        desc << " MMU_ENGINE_ID " << GetMMUEngineID();

        if (lwgpu->IsSmcMemApi())
        {
            string smcEngineName = lwgpu->GetSmcEngineName(pLwRm);
            desc << " [SmcEngineName " << smcEngineName << "]";
        }
        else
        {
            UINT32 swizzId = lwgpu->GetSwizzId(pLwRm);
            desc << " [Swizzid " << swizzId << " SMC_ID " << smcEngineId << "]";
        }
    }
   
    DebugPrintf("AccessCounter event gild string: %s\n", desc.str().c_str());
    return desc.str();
}

/* virtual */ bool PmEvent_AccessCounterNotification::GetIsVirtualAccess() const
{
    return m_IsVirtualAccess;
}

/* virtual */ PmChannels PmEvent_AccessCounterNotification::GetChannels() const
{
    return m_channels;
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for PluginEvent events
//!
/* virtual */ PmEvent *PmEvent_Plugin::Clone() const
{
    return new PmEvent_Plugin(m_EventName);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_Plugin::GetGildString() const
{
    return "PLUGIN_" + m_EventName;
}

//--------------------------------------------------------------------
//! \brief Return event name
//!
/* virtual */ string PmEvent_Plugin::GetEventName() const
{
    return m_EventName;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_OnPhysicalPageFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_OnPhysicalPageFault::PmEvent_OnPhysicalPageFault
(
    GpuSubdevice * pGpuSubdevice
) : PmEvent(PHYSICAL_PAGE_FAULT),
    m_pGpuSubdevice(pGpuSubdevice)
{
    MASSERT(pGpuSubdevice != 0);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for OnPhyscialPageFault events
//!
PmEvent * PmEvent_OnPhysicalPageFault::Clone() const
{
    return new PmEvent_OnPhysicalPageFault(m_pGpuSubdevice);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_OnPhysicalPageFault::GetGildString() const
{
    return "PHYSCIAL_PAGE_FAULT";
}

//--------------------------------------------------------------------
/* virtual */ GpuSubdevice *PmEvent_OnPhysicalPageFault::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_OnErrorLoggerInterrupt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_OnErrorLoggerInterrupt::PmEvent_OnErrorLoggerInterrupt
(
    GpuSubdevice * pGpuSubdevice,
    const std::string &interruptString
) : PmEvent(ERROR_LOGGER_INTERRUPT),
    m_pGpuSubdevice(pGpuSubdevice),
    m_InterruptString(interruptString)
{
    MASSERT(pGpuSubdevice != 0);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for OnPhyscialPageFault events
//!
PmEvent * PmEvent_OnErrorLoggerInterrupt::Clone() const
{
    return new PmEvent_OnErrorLoggerInterrupt(m_pGpuSubdevice, m_InterruptString);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_OnErrorLoggerInterrupt::GetGildString() const
{
    return "ERROR_LOGGER_INTERRUPT " + GetInterruptString();
}

//--------------------------------------------------------------------
/* virtual */ GpuSubdevice *PmEvent_OnErrorLoggerInterrupt::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//--------------------------------------------------------------------
/* virtual */ std::string PmEvent_OnErrorLoggerInterrupt::GetInterruptString() const
{
    return m_InterruptString;
}

//////////////////////////////////////////////////////////////////////
// PmEvent_OnT3dPluginEvent
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_OnT3dPluginEvent::PmEvent_OnT3dPluginEvent
(
    PmTest * pTest,
    const string& traceEventName
) : PmEvent(T3D_PLUGIN_EVENT),
    m_pTest(pTest),
    m_TraceEventName(traceEventName)
{
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for OnT3dPlugin events
//!
PmEvent * PmEvent_OnT3dPluginEvent::Clone() const
{
    return new PmEvent_OnT3dPluginEvent(m_pTest, m_TraceEventName);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_OnT3dPluginEvent::GetGildString() const
{
    return "T3D_PLUGIN_EVENT " + m_TraceEventName;
}

//--------------------------------------------------------------------
/* virtual */ string PmEvent_OnT3dPluginEvent::GetTraceEventName() const
{
    return m_TraceEventName;
}

//--------------------------------------------------------------------
/* virtual */ PmTest * PmEvent_OnT3dPluginEvent::GetTest() const
{
    return m_pTest;
}
// PmEvent_OnNonfatalPoisonError
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_OnNonfatalPoisonError::PmEvent_OnNonfatalPoisonError
(
    GpuSubdevice * pGpuSubdevice,
    UINT16 info16
) : PmEvent(NONE_FATAL_POISON_ERROR),
    m_pGpuSubdevice(pGpuSubdevice),
    m_info16(info16)
{
    MASSERT(pGpuSubdevice != 0);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for OnNonfatalPoisonError events
//!
PmEvent * PmEvent_OnNonfatalPoisonError::Clone() const
{
    return new PmEvent_OnNonfatalPoisonError(m_pGpuSubdevice, m_info16);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_OnNonfatalPoisonError::GetGildString() const
{
    return "NONE_FATAL_POISON_ERROR";
}

//--------------------------------------------------------------------
/* virtual */ GpuSubdevice *PmEvent_OnNonfatalPoisonError::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//--------------------------------------------------------------------
/* virtual */ const UINT16 PmEvent_OnNonfatalPoisonError::GetInfo16() const
{
    return m_info16;
}


//////////////////////////////////////////////////////////////////////
// PmEvent_OnFatalPoisonError
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEvent_OnFatalPoisonError::PmEvent_OnFatalPoisonError
(
    GpuSubdevice * pGpuSubdevice,
    UINT16 info16
) : PmEvent(FATAL_POISON_ERROR),
    m_pGpuSubdevice(pGpuSubdevice),
    m_info16(info16)
{
    MASSERT(pGpuSubdevice != 0);
}

//--------------------------------------------------------------------
//! \brief Implementation of Clone() for OnFatalPoisonError events
//!
PmEvent * PmEvent_OnFatalPoisonError::Clone() const
{
    return new PmEvent_OnFatalPoisonError(m_pGpuSubdevice, m_info16);
}

//--------------------------------------------------------------------
//! \brief Return the string to write to the gild file when the event oclwrs
//!
/* virtual */ string PmEvent_OnFatalPoisonError::GetGildString() const
{
    return "FATAL_POISON_ERROR";
}

//--------------------------------------------------------------------
/* virtual */ GpuSubdevice *PmEvent_OnFatalPoisonError::GetGpuSubdevice() const
{
    return m_pGpuSubdevice;
}

//--------------------------------------------------------------------
/* virtual */ const UINT16 PmEvent_OnFatalPoisonError::GetInfo16() const
{
    return m_info16;
}
