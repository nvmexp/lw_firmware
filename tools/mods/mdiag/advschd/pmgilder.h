/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2012 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Defines a PmGilder class to compare the Potential Events
//! with the actual ones

#ifndef INCLUDED_PMGILDER_H
#define INCLUDED_PMGILDER_H

#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

//!---------------------------------------------------------------
//! \brief Class to compare potential Events with actual ones
//!
//! The gilder keeps a log of the potential & actual events that occur
//! during the test, and uses them to validate the results and/or
//! write an output file (sometimes called the "gild file") at the end
//! of the test.
//!
//! The gilder has 4 modes that it uses to validate the results:
//!
//! - REQUIRED: The actual events may be a superset of the potential
//!   events.  All potential events must have oclwrred, but any
//!   extra (either unexpected or overoclwrred) events are ignored
//! - INCLUDE: The actual events must be a subset of the potential
//!   events.  For example, if you unmap a page (a potential page
//!   fault), then you may or may not get an actual page fault,
//!   because the test might not access that page.  But it's an error
//!   to get a page fault on any page that wasn't unmapped.
//! - EQUAL: The actual events must be the same as the potential
//!   events.  For example, if you unmap a page, you *must* get a page
//!   fault on that page, and vice-versa.
//! - NONE: Do not compare the potential and actual events.  This mode
//!   is mostly for users that prefer to analyze the output file via a
//!   perl script.
//!
//! You can also supply an input file, which should be the output file
//! from a previous run of mods with the same test(s).  If so, then
//! the gilder ignores the potential events, and just compares the
//! actual events to the input file:
//!
//! - REQUIRED: The actual events may be a superset of the events in the
//!   input file, possibly in a different order, but must include all
//!   events from the input file.
//! - INCLUDE: The actual events must be a subset of the events in the
//!   input file, possibly in a different order.
//! - EQUAL: The actual events must be the same as the events in the
//!   input file, in the same order.
//! - NONE: Do not compare the events with the input file.
//!
//! In all gilding modes, when an event line is preceeded with a '-' it
//! marks the event as forbidden (i.e. if a matching event oclwrs, it
//! will cause the test to fail).
//!
//! When policy manager matches events, normally each field in the event
//! line is checked for an exact string match, however if a field is
//! preceeded by a '/' it indicates that the field should be considered
//! a regular expression when matching instead.  For all event types, the
//! first field on the line may never be a regular expression and for PMU
//! events the first 3 fields may not be a regular expression.
//!
class PmGilder
{
public:
    enum Mode
    {
        REQUIRED_MODE,
        INCLUDE_MODE,
        EQUAL_MODE,
        NONE_MODE
    };

    PmGilder(PolicyManager *pPolicyManager);
    ~PmGilder();

    void SetMode(Mode mode)                    { m_Mode = mode; }
    void SetEventsMin(UINT32 eventsMin)        { m_EventsMin = eventsMin; }
    void SetInputFile(const string &filename)  { m_InputFile = filename; }
    void SetOutputFile(const string &filename) { m_OutputFile = filename; }
    void SetReqEventTimeout(UINT32 timoutMs)   { m_ReqEventTimeoutMs = timoutMs; }
    void SetReqEventEnableSimTimeout(bool bEnable);

    RC StartTest();
    void StartPotentialEvent(PmPotentialEvent *pPotEvent);
    void EndPotentialEvent(PmPotentialEvent *pPotEvent);
    void EventOclwrred(PmEvent *pEvent);
    void DumpRange(const PmMemRange &range);
    void CrcRange(const PmMemRange &range);
    void KeepRange(const PmMemRange &range);
    RC WaitForRequiredEvents();
    RC EndTest();

private:
    //! Used by LogEntry.  Each log entry is one of these three types.
    enum LogEntryType
    {
        START_POTENTIAL, //!< Start of a potential event.
        END_POTENTIAL,   //!< End of a potential event.
        EVENT_OCLWRRED   //!< Event oclwrred.
    };

    //! Private data type used to record the potential & actual events
    //! that oclwrred during the test.
    struct LogEntry
    {
        LogEntry(LogEntryType aType, void *ptr) : m_Type(aType), m_Ptr(ptr) {}
        LogEntryType m_Type;
        union
        {
            void             *m_Ptr;       //!< For constructor
            PmPotentialEvent *m_pPotEvent; //!< For START_POTENTIAL &
                                           //!< END_POTENTIAL entries
            PmEvent          *m_pEvent;    //!< For EVENT_OCLWRRED entries
        };
        string GetGildString() const;
        bool Matches(const string &gildString) const;
        bool MatchesRequiredEvent(const string &gildString) const;
    };

    RC WriteOutputFile();
    RC CheckEventsMin();
    RC CreateDumpEvents();
    RC CheckLog();
    RC LoadInputFile();
    RC CheckInputFile();
    void FreeLog();

    bool AreRequiredEventsPresent();
    static bool PollReqEvents(void * vpthis);

    PolicyManager *m_pPolicyManager; //!< PolicyManager that owns this object
    Mode           m_Mode;           //!< INCLUDE, EQUAL, or NONE mode
    UINT32         m_EventsMin;      //!< Minimum # of events that must happen
    string         m_InputFile;      //!< Name of the input file
    string         m_OutputFile;     //!< Name of the output file
    list<LogEntry> m_Log;            //!< Log of events & potential events
                                     //!< that oclwrred during test
    PmMemRanges    m_DumpRanges;     //!< Memory ranges to dump at end of test
    PmMemRanges    m_CrcRanges;      //!< Memory ranges to CRC at end of test
    PmMemRanges    m_KeepRanges;     //!<
    vector<string> m_RequiredEvents; //!< Required events.  Only used with an
                                     //!< input gild file.  All events are
                                     //!< required in EQUAL mode, only events
                                     //!< preceeded by '+' are required in
                                     //!< INCLUDE mode
    vector<string> m_ForbiddenEvents; //!< Forbidden events.  Only used with an
                                      //!< input gild file.  Any events flagged
                                      //!< with "-" at the start of the line
                                      //!< must not be present in the test
    vector<string> m_OptionalEvents; //!< Optional events.  Only used with an
                                     //!< input gild file.  No events are
                                     //!< optional in EQUAL mode, events not
                                     //!< preceeded by '+' are optional in
                                     //!< INCLUDE mode
    UINT32         m_ReqEventLogSize;//!< Size of the log the last time
                                     //!< AreRequiredEventsPresent was called
                                     //!< Used to reduce overhead during
                                     //!< polling
    UINT32         m_ReqEventTimeoutMs; //!< Timeout to wait for required
                                        //!< events when gilding using a file
    bool           m_ReqEventEnableSimTimeout; //!< true if required event
                                               //!< timeouts are enabled on sim
};

#endif // INCLUDED_PMGILDER_H
