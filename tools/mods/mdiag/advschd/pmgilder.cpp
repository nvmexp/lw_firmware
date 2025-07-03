/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2017,2019,2021 by LWPU Corporation.  All rights reserved.
* All information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Implement a PmGilder class to compare the Potential Events
//! with the actual ones

#include "pmgilder.h"
#include "core/include/crc.h"
#include "gpu/include/gpudev.h"
#include "mdiag/utils/mdiagsurf.h"
#include "mdiag/sysspec.h"
#include "pmevent.h"
#include "pmpotevt.h"
#include "pmsurf.h"
#include "core/include/regexp.h"
#include "gpu/utility/surf2d.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include <ctype.h>

//--------------------------------------------------------------------
//! \brief PmGilder constructor
//!
PmGilder::PmGilder(PolicyManager *pPolicyManager) :
    m_pPolicyManager(pPolicyManager),
    m_Mode(INCLUDE_MODE),
    m_EventsMin(0),
    m_ReqEventLogSize(0),
    m_ReqEventTimeoutMs(0),
    m_ReqEventEnableSimTimeout(false)
{
    MASSERT(m_pPolicyManager != NULL);
}

//--------------------------------------------------------------------
//! \brief PmGilder destructor
//!
PmGilder::~PmGilder()
{
    FreeLog();
}

//--------------------------------------------------------------------
//! \brief Enable/disable required event timeouts on simulation
//!
//! \param bEnable : true to enable simulation timeouts, false otherwise
//!
void PmGilder::SetReqEventEnableSimTimeout(bool bEnable)
{
    m_ReqEventEnableSimTimeout = bEnable;
}

//--------------------------------------------------------------------
//! \brief This method should be called at the start of each test.
//!
//! This method just frees the log from the previous test.
//!
RC PmGilder::StartTest()
{
    FreeLog();
    if (!m_InputFile.empty())
        return LoadInputFile();

    return OK;
}

//--------------------------------------------------------------------
//! \brief This method should be called when a potential-event starts.
//!
//! \param pPotEvent The potential event that starts now.  The gilder
//! will assume ownership of the argument (deleting it at will), so it
//! should be created via new.
//!
void PmGilder::StartPotentialEvent(PmPotentialEvent *pPotEvent)
{
    m_Log.push_back(LogEntry(START_POTENTIAL, pPotEvent));
}

//--------------------------------------------------------------------
//! \brief This method should be called when a potential-event ends.
//!
//! \param pPotEvent The potential event that ends now.  The gilder
//! will assume ownership of the argument (deleting it at will), so it
//! should be created via new.
//!
void PmGilder::EndPotentialEvent(PmPotentialEvent *pPotEvent)
{
    m_Log.push_back(LogEntry(END_POTENTIAL, pPotEvent));
}

//--------------------------------------------------------------------
//! \brief This method should be called when an event oclwrs.
//!
//! \param pEvent The event that just oclwrred.  The gilder will
//! assume ownership of the argument (deleting it at will), so it
//! should be created via new or PmEvent::Clone().
//!
void PmGilder::EventOclwrred(PmEvent *pEvent)
{
    m_Log.push_back(LogEntry(EVENT_OCLWRRED, pEvent));
}

//--------------------------------------------------------------------
//! This method should be called when physical memory is moved, and we
//! want to dump the old memory to the gild file at the end of the test.
//!
//! \param range The memory that we want to dump.
//!
void PmGilder::DumpRange(const PmMemRange &range)
{
    m_DumpRanges.push_back(range);
}

//--------------------------------------------------------------------
//! This method should be called when physical memory is moved, and we
//! want to write a CRC of the old memory in the gild file at the end
//! of the test.
//!
//! \param range The memory that we want to CRC.
//!
void PmGilder::CrcRange(const PmMemRange &range)
{
    m_CrcRanges.push_back(range);
}

void PmGilder::KeepRange(const PmMemRange &range)
{
    m_KeepRanges.push_back(range);
}

//--------------------------------------------------------------------
//! \brief Wait for required events to be received if a timeout is set.
//!
//! \return OK if all required events have been received, or it is not
//!         necessary to wait for required events because the timeout
//!         is zero, not OK otherwise
//!
RC PmGilder::WaitForRequiredEvents()
{
    RC rc;

    if (m_ReqEventTimeoutMs)
    {
        UINT32 startTimeInSec = Utility::GetSystemTimeInSeconds();

        rc = Tasker::Poll(PmGilder::PollReqEvents,
                          (void *)this,
                          m_ReqEventEnableSimTimeout ?
                                m_ReqEventTimeoutMs :
                                Tasker::FixTimeout(m_ReqEventTimeoutMs),
                          __FUNCTION__,
                          "PmGilder::PollReqEvents");

        // Waiting for required events fails print a message and continue
        // on with gilding so that the required events are printed
        if (rc != OK)
        {
            ErrPrintf("Failure when waiting for required events : %s Polling Time(SysTimeInSec): TimeoutLimit =%d "
                      "StartTime = %d EndTime = %d\n",
                      rc.Message(), m_ReqEventTimeoutMs/1000,
                      startTimeInSec, Utility::GetSystemTimeInSeconds());

            ErrPrintf("There are %llu messages logged:\n", (UINT64)m_Log.size());
            for (list<LogEntry>::iterator pLog = m_Log.begin();
                 pLog != m_Log.end(); ++pLog)
            {
                string gildString = pLog->GetGildString();
                if (gildString != "")
                {
                    ErrPrintf("    %s\n", gildString.c_str());
                }
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief This method should be called at the end of each test.
//!
//! This method validates the log according to INCLUDE, EQUALS, or
//! NONE mode, as described in the class description.  It also writes
//! the output file (or appends, if this is not the 1st test) and
//! checks that at least m_EventsMin events oclwrred.
//!
RC PmGilder::EndTest()
{
    RC rc;

    // Cleanup
    Utility::CleanupOnReturn<PmGilder, void>
            FreePmGilderLog(this, &PmGilder::FreeLog);

    if (m_EventsMin > 0)
    {
        CHECK_RC(CheckEventsMin());
    }
    CHECK_RC(CreateDumpEvents());
    if (!m_OutputFile.empty())
    {
        CHECK_RC(WriteOutputFile());
    }
    switch (m_Mode)
    {
        case REQUIRED_MODE:
        case INCLUDE_MODE:
        case EQUAL_MODE:
            if (m_InputFile.empty())
                CHECK_RC(CheckLog());
            else
                CHECK_RC(CheckInputFile());
            break;
        case NONE_MODE:
            break;
    }

    return rc;
}

//--------------------------------------------------------------------
//! Return the string that should be printed in the gildfile for this
//! log entry, or "" if this entry shouldn't be logged.
//!
string PmGilder::LogEntry::GetGildString() const
{
    switch (m_Type)
    {
        case START_POTENTIAL:
            return m_pPotEvent->GetStartGildString();
        case END_POTENTIAL:
            return m_pPotEvent->GetEndGildString();
        case EVENT_OCLWRRED:
            return m_pEvent->GetGildString();
    }

    MASSERT(!"Should never get here in PmGilder::LogEntry::GetGildString");
    return "";
}

//--------------------------------------------------------------------
//! Return true if this log entry matches an gildfile entry from a
//! previous run of mods
//!
bool PmGilder::LogEntry::Matches(const string &gildString) const
{
    switch (m_Type)
    {
        case START_POTENTIAL:
            return m_pPotEvent->MatchesStart(gildString);
        case END_POTENTIAL:
            return m_pPotEvent->MatchesEnd(gildString);
        case EVENT_OCLWRRED:
            return m_pEvent->Matches(gildString);
    }

    MASSERT(!"Should never get here in PmGilder::LogEntry::Matches");
    return false;
}

//--------------------------------------------------------------------
//! Return true if this log entry  is close enough a gildfile
//! entry from a previous run of mods to match a required
//! gildfile entry.
//!
bool PmGilder::LogEntry::MatchesRequiredEvent(const string &gildString) const
{
    switch (m_Type)
    {
        case START_POTENTIAL:
            return m_pPotEvent->MatchesStart(gildString);
        case END_POTENTIAL:
            return m_pPotEvent->MatchesEnd(gildString);
        case EVENT_OCLWRRED:
            return m_pEvent->MatchesRequiredEvent(gildString);
    }

    MASSERT(!"Should never get here in PmGilder::LogEntry::MatchesRequiredEvent");
    return false;
}

//! Header of the gild file.  Used by PmGilder::WriteOutputFile().
//!
static const char *OutputFileHeader[] =
{
    "# This file contains potential events and actual events that",
    "# oclwrred while running MODS.",
    "#",
    "# Any line that starts with '#' is a comment.  Other lines indicate",
    "# events that oclwrred while mods ran, in a script-friendly format.",
    "#",
    // TODO: More details later when we're more sure which events are
    // still part of advanced scheduling.
    NULL
};

//--------------------------------------------------------------------
//! \brief Private function called by EndTest() to write the output file.
//!
RC PmGilder::WriteOutputFile()
{
    FileHolder file;
    RC rc;

    // Open the output file.
    //
    if (m_pPolicyManager->GetTestNum() == 0)
    {
        // If this is the first test, write the header
        //
        CHECK_RC(file.Open(m_OutputFile, "w"));
        for (int ii = 0; OutputFileHeader[ii] != NULL; ++ii)
        {
            fprintf(file.GetFile(), "%s\n", OutputFileHeader[ii]);
        }
    }
    else
    {
        // If this is not the first test, append to the existing file
        //
        CHECK_RC(file.Open(m_OutputFile, "a"));
    }

    // Write the test number
    //
    fprintf(file.GetFile(), "TEST_NUM %d\n", m_pPolicyManager->GetTestNum());

    // Write the log
    //
    for (list<LogEntry>::iterator pLog = m_Log.begin();
         pLog != m_Log.end(); ++pLog)
    {
        string gildString = pLog->GetGildString();
        if (gildString != "")
        {
            fprintf(file.GetFile(), "%s\n", gildString.c_str());
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! Private function called by EndTest() to verify that the minimum
//! number of events oclwrred.
//! \sa SetEventsMin()
//!
RC PmGilder::CheckEventsMin()
{
    UINT32 eventCount = 0;

    // Count the number of oclwrred events
    //
    for (list<LogEntry>::iterator pLog = m_Log.begin();
         pLog != m_Log.end(); ++pLog)
    {
        if(pLog->m_Type == EVENT_OCLWRRED)
        {
            // Check if minimum is reached
            if(++eventCount >= m_EventsMin)
                return OK;
        }
    }

    // minimum not reached
    ErrPrintf("Gilder Error: Minimum of %d events did not occur\n",
              m_EventsMin);
    return RC::DATA_MISMATCH;
}

//--------------------------------------------------------------------
//! Private function called by EndTest() to colwert m_DumpRanges and
//! m_CrcRanges into gildable events.
//!
RC PmGilder::CreateDumpEvents()
{
    RC rc;

    // Append "DUMP <binary data>" strings to the log for each entry
    // in m_DumpRanges
    //
    for (PmMemRanges::iterator pRange = m_DumpRanges.begin();
         pRange != m_DumpRanges.end(); ++pRange)
    {
        const MdiagSurf *pMdiagSurf = pRange->GetMdiagSurf();
        GpuDevice *pGpuDevice = pMdiagSurf->GetGpuDev();
        for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices();
             ++subdev)
        {
            vector<UINT08> bytes;
            CHECK_RC(pRange->ReadPhys(&bytes,
                                      pGpuDevice->GetSubdevice(subdev)));

            static const int BYTES_PER_LINE = 32;
            char dumpData[2 * BYTES_PER_LINE + 1];
            UINT32 lineCount = 0;

            for (size_t ii = 0; ii < bytes.size(); ++ii)
            {
                sprintf(&dumpData[2 * (ii % BYTES_PER_LINE)],
                        "%02x",
                        bytes[ii] & 0x00ff);
                if (((ii+1) % BYTES_PER_LINE) == 0 || ii == bytes.size() - 1)
                {
                    string header = Utility::StrPrintf(
                        "DUMP 0x%08x", pMdiagSurf->GetMemHandle());
                    if (pGpuDevice->GetNumSubdevices() > 1)
                        header += Utility::StrPrintf("[%d]", subdev);
                    header +=  Utility::StrPrintf(
                        "[%lld] ",
                        pRange->GetOffset() + lineCount * BYTES_PER_LINE);
                    lineCount++;
                    EventOclwrred(new PmEvent_GildString(header + dumpData));
                }
            }
        }
    }

    // Combine all entries in m_CrcRanges into a CRC, and append a
    // "CRC <32_bit_number>" string to the log.
    //
    if (!m_CrcRanges.empty())
    {
        UINT32 crc = 0;
        for (PmMemRanges::iterator pRange = m_CrcRanges.begin();
             pRange != m_CrcRanges.end(); ++pRange)
        {
            GpuDevice *pGpuDevice = pRange->GetMdiagSurf()->GetGpuDev();
            for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices();
                 ++subdev)
            {
                vector<UINT08> bytes;
                CHECK_RC(pRange->ReadPhys(&bytes,
                                          pGpuDevice->GetSubdevice(subdev)));
                crc = Crc::Append(&bytes[0], (UINT32)bytes.size(), crc);
            }
        }
        char crcString[20];
        sprintf(crcString, "CRC %08x", crc);
        EventOclwrred(new PmEvent_GildString(crcString));
    }

    m_DumpRanges.clear();
    m_CrcRanges.clear();
    m_KeepRanges.clear();

    return rc;
}

//--------------------------------------------------------------------
//! Private function called by EndTest() to do the INCLUDE or EQUALS
//! gilding, if there is no input file.
//!
RC PmGilder::CheckLog()
{
    // Note: The following containers contain a lot of PmEvent and
    // PmPotentialEvent pointers.  These pointers point to objects
    // that are owned by m_Log, and should not be deleted here.
    //
    list<PmPotentialEvent*>      potentialEvents;
    map<PmPotentialEvent*, bool> alreadyOclwrred;
    list<PmEvent*>               unexpectedEvents;
    list<PmEvent*>               overoclwrredEvents;
    list<PmPotentialEvent*>      unoclwrredEvents;
    RC rc;

    // Loop through the sorted log to fill the unexpectedEvents,
    // overoclwrredEvents, and unoclwrredEvents lists.  (We also keep
    // a running track of potentialEvents, but that's just a
    // means to an end.)
    //
    for (list<LogEntry>::iterator pLog = m_Log.begin();
         pLog != m_Log.end(); ++pLog)
    {
        switch (pLog->m_Type)
        {
            case START_POTENTIAL:
            {
                // Add a new potential event
                //
                alreadyOclwrred[pLog->m_pPotEvent] = false;
                potentialEvents.push_back(pLog->m_pPotEvent);
                break;
            }
            case END_POTENTIAL:
            {
                // Remove the oldest matching potential event, and
                // add it to unoclwrredEvents if there was no matching
                // actual event.
                //
                for (list<PmPotentialEvent*>::iterator ppPotEvent =
                         potentialEvents.begin();
                     ppPotEvent != potentialEvents.end(); ++ppPotEvent)
                {
                    if (pLog->m_pPotEvent->Matches(*ppPotEvent))
                    {
                        if (!alreadyOclwrred[*ppPotEvent])
                            unoclwrredEvents.push_back(*ppPotEvent);
                        alreadyOclwrred.erase(*ppPotEvent);
                        potentialEvents.erase(ppPotEvent);
                        break;
                    }
                }
                break;
            }
            case EVENT_OCLWRRED:
            {
                if (pLog->m_pEvent->RequiresPotentialEvent())
                {
                    // Compare this actual event to the current potential
                    // events.  If it matches any potential events, mark
                    // those potential events as having oclwrred
                    // (alreadyOclwrred).  If not, add the actual event to
                    // unexpectedEvents or overoclwrredEvents, depending
                    // on whether it matches a potential event that
                    // already oclwrred.
                    //
                    bool matchesPotEvent    = false;
                    bool matchesNewPotEvent = false;
                    for (list<PmPotentialEvent*>::iterator ppPotEvent =
                             potentialEvents.begin();
                         ppPotEvent != potentialEvents.end(); ++ppPotEvent)
                    {
                        if ((*ppPotEvent)->Matches(pLog->m_pEvent))
                        {
                            matchesPotEvent = true;
                            if (!alreadyOclwrred[*ppPotEvent])
                            {
                                alreadyOclwrred[*ppPotEvent] = true;
                                matchesNewPotEvent = true;
                                break;
                            }
                        }
                    }
                    if (!matchesPotEvent)
                        unexpectedEvents.push_back(pLog->m_pEvent);
                    else if (!matchesNewPotEvent)
                        overoclwrredEvents.push_back(pLog->m_pEvent);
                }
                break;
            }
        }
    }

    // If there are unexpected, overoclwrred, or unoclwrred Events
    // (depending on mode), return an RC::DATA_MISMATCH error and
    // record an error message.
    //
    if ((m_Mode != REQUIRED_MODE) && !unexpectedEvents.empty())
    {
        rc = RC::DATA_MISMATCH;
        ErrPrintf("There were unexpected events in test %d:\n",
                  m_pPolicyManager->GetTestNum());
        for (list<PmEvent*>::iterator ppEvent = unexpectedEvents.begin();
             ppEvent != unexpectedEvents.end(); ++ppEvent)
        {
            ErrPrintf("    %s\n", (*ppEvent)->GetGildString().c_str());
        }
    }
    if (m_Mode == EQUAL_MODE && !overoclwrredEvents.empty())
    {
        rc = RC::DATA_MISMATCH;
        if (overoclwrredEvents.size() == 1)
            ErrPrintf("There was an event that oclwrred more than once per potential event in test %d:\n",
                      m_pPolicyManager->GetTestNum());
        else
            ErrPrintf("There were events that oclwrred more than once per potential event in test %d:\n",
                      m_pPolicyManager->GetTestNum());
        for (list<PmEvent*>::iterator ppEvent = overoclwrredEvents.begin();
             ppEvent != overoclwrredEvents.end(); ++ppEvent)
        {
            ErrPrintf("    %s\n", (*ppEvent)->GetGildString().c_str());
        }
    }
    if ((m_Mode == EQUAL_MODE || m_Mode == REQUIRED_MODE) &&
        !unoclwrredEvents.empty())
    {
        rc = RC::DATA_MISMATCH;
        if (unoclwrredEvents.size() == 1)
            ErrPrintf("There was a potential event that did not occur in test %d:\n",
                      m_pPolicyManager->GetTestNum());
        else
            ErrPrintf("There were potential events that did not occur in test %d:\n",
                      m_pPolicyManager->GetTestNum());
        for (list<PmPotentialEvent*>::iterator ppPotEvent =
                 unoclwrredEvents.begin();
             ppPotEvent != unoclwrredEvents.end(); ++ppPotEvent)
        {
            ErrPrintf("    %s\n", (*ppPotEvent)->GetStartGildString().c_str());
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! Private function called by StartTest() to load the required and
//! optional events from the gild file.
//!
RC PmGilder::LoadInputFile()
{
    RC rc;

    // Parse the input file, and put the entries for the current test
    // in oldLogEntries.
    //
    RegExp testNumRegexp;
    CHECK_RC(testNumRegexp.Set("^TEST_NUM ([0-9]+)$", RegExp::SUB));
    RegExp commentRegexp;
    CHECK_RC(commentRegexp.Set("^#"));

    static const int MAX_INPUT_LINE = 256;
    char inputLine[MAX_INPUT_LINE];
    UINT32 testNum = 0;
    FileHolder file;

    // The input file is loaded once per test and the required/optional events
    // are specified per-test so clear them every time the file is loaded
    m_RequiredEvents.clear();
    m_OptionalEvents.clear();
    m_ForbiddenEvents.clear();

    CHECK_RC(file.Open(m_InputFile, "r"));
    while (fgets(inputLine, MAX_INPUT_LINE, file.GetFile()) != NULL)
    {
        size_t lineLen = strlen(inputLine);
        while (lineLen > 0 && isspace(inputLine[lineLen-1]))
            inputLine[--lineLen] = '\0';

        if (testNumRegexp.Matches(inputLine))
        {
            testNum = atoi(testNumRegexp.GetSubstring(1).c_str());
        }
        else if (testNum == m_pPolicyManager->GetTestNum() &&
                 !commentRegexp.Matches(inputLine))
        {
            if (inputLine[0] == '-')
            {
                m_ForbiddenEvents.push_back(&(inputLine[1]));
            }
            else if ((m_Mode == EQUAL_MODE) || (m_Mode == REQUIRED_MODE) ||
                ((m_Mode == INCLUDE_MODE) && (inputLine[0] == '+')))
            {
                if (inputLine[0] == '+')
                    m_RequiredEvents.push_back(&(inputLine[1]));
                else
                    m_RequiredEvents.push_back(inputLine);
            }
            else
            {
                m_OptionalEvents.push_back(inputLine);
            }
        }
    }

    if (ferror(file.GetFile()))
    {
        ErrPrintf("Error reading file %s", m_InputFile.c_str());
        rc = RC::FILE_READ_ERROR;
    }

    return rc;
}
//--------------------------------------------------------------------
//! Private function called by EndTest() to do the INCLUDE or EQUALS
//! gilding, if there is an input file.
//!
RC PmGilder::CheckInputFile()
{
    vector<LogEntry> newLogEntries;

    // Extract the log entries from m_Log that would be printed in a
    // log file, and put them in newLogEntries.
    //
    newLogEntries.reserve(m_Log.size());
    for (list<LogEntry>::iterator pLog = m_Log.begin();
         pLog != m_Log.end(); ++pLog)
    {
        if (!pLog->Matches(""))
        {
            newLogEntries.push_back(*pLog);
        }
    }

    // Compare oldLogEntries with newLogEntries.  In EQUAL mode, the
    // lists should match one-for-one.  In INCLUDE mode, newLogEntries
    // should be a subset of oldLogEntries, possibly in a different
    // order.
    //

    // First ensure that no forbidden entries have oclwrred
    vector<string>::iterator pForbiddenLogEntry;
    for (pForbiddenLogEntry = m_ForbiddenEvents.begin();
         pForbiddenLogEntry != m_ForbiddenEvents.end(); ++pForbiddenLogEntry)
    {
        for (vector<LogEntry>::iterator pNewLogEntry = newLogEntries.begin();
             pNewLogEntry != newLogEntries.end(); ++pNewLogEntry)
        {
            if (pNewLogEntry->Matches(*pForbiddenLogEntry))
            {
                ErrPrintf("Forbidden events oclwrred in test %d and file %s:\n",
                          m_pPolicyManager->GetTestNum(), m_InputFile.c_str());
                ErrPrintf("    Event in file: %s\n",
                          (*pForbiddenLogEntry).c_str());
                ErrPrintf("    Event in test: %s\n",
                          (*pNewLogEntry).GetGildString().c_str());
                return RC::DATA_MISMATCH;
            }
        }
    }

    if (m_Mode == EQUAL_MODE)
    {
        size_t numEntries = min(m_RequiredEvents.size(), newLogEntries.size());
        for (size_t ii = 0; ii < numEntries; ++ii)
        {
            if (!newLogEntries[ii].Matches(m_RequiredEvents[ii]))
            {
                ErrPrintf("Different events oclwrred in test %d and file %s:\n",
                          m_pPolicyManager->GetTestNum(), m_InputFile.c_str());
                ErrPrintf("    Event in file: %s\n",
                          m_RequiredEvents[ii].c_str());
                ErrPrintf("    Event in test: %s\n",
                          newLogEntries[ii].GetGildString().c_str());
                return RC::DATA_MISMATCH;
            }
        }
        if (m_RequiredEvents.size() > newLogEntries.size())
        {
            ErrPrintf("Event oclwrred in file %s but not in test %d:\n",
                      m_InputFile.c_str(), m_pPolicyManager->GetTestNum());
            ErrPrintf("    %s\n",
                      m_RequiredEvents[newLogEntries.size()].c_str());
            return RC::DATA_MISMATCH;
        }
        if (newLogEntries.size() > m_RequiredEvents.size())
        {
            ErrPrintf("Event oclwrred in test %d but not in file %s:\n",
                      m_pPolicyManager->GetTestNum(), m_InputFile.c_str());
            ErrPrintf("    %s\n",
                      newLogEntries[m_RequiredEvents.size()].GetGildString().c_str());
            return RC::DATA_MISMATCH;
        }
    }
    else // m_Mode == INCLUDE_MODE || m_Mode == REQUIRED_MODE
    {
        bool foundMismatch = false;
        vector<string>::iterator pOldLogEntry;
        for (vector<LogEntry>::iterator pNewLogEntry = newLogEntries.begin();
             pNewLogEntry != newLogEntries.end(); ++pNewLogEntry)
        {
            bool foundMatch = false;

            // Check against required events first
            for (pOldLogEntry = m_RequiredEvents.begin();
                 pOldLogEntry != m_RequiredEvents.end(); ++pOldLogEntry)
            {
                if (pNewLogEntry->Matches(*pOldLogEntry))
                {
                    foundMatch = true;
                    m_RequiredEvents.erase(pOldLogEntry);
                    break;
                }
            }

            // Skip checking optional events as well as ensuring a match when
            // using required mode as only required events are relevant and
            // any non-required events are ignored
            if  (m_Mode == REQUIRED_MODE)
                continue;

            // Now check against optional events
            for (pOldLogEntry = m_OptionalEvents.begin();
                 (pOldLogEntry != m_OptionalEvents.end()) && !foundMatch;
                 ++pOldLogEntry)
            {
                if (pNewLogEntry->Matches(*pOldLogEntry))
                {
                    foundMatch = true;
                    m_OptionalEvents.erase(pOldLogEntry);
                    break;
                }
            }
            if (!foundMatch)
            {
                if (!foundMismatch)
                {
                    ErrPrintf("Event(s) oclwrred in test %d but not in file %s:\n",
                              m_pPolicyManager->GetTestNum(), m_InputFile.c_str());
                }
                ErrPrintf("    %s\n", pNewLogEntry->GetGildString().c_str());
                foundMismatch = true;
            }
        }

        if (m_RequiredEvents.size())
        {
            ErrPrintf("Required event(s) in test %d not found:\n",
                      m_pPolicyManager->GetTestNum());

            // Check against required events first
            for (pOldLogEntry = m_RequiredEvents.begin();
                 pOldLogEntry != m_RequiredEvents.end(); ++pOldLogEntry)
            {
                ErrPrintf("    %s\n", pOldLogEntry->c_str());
            }
            foundMismatch = true;
        }
        if (foundMismatch)
        {
            return RC::DATA_MISMATCH;
        }
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Free the event log.  Called by StartTest() and the destructor.
//!
void PmGilder::FreeLog()
{
    for (list<LogEntry>::iterator pLog = m_Log.begin();
         pLog != m_Log.end(); ++pLog)
    {
        switch (pLog->m_Type)
        {
            case START_POTENTIAL:
            case END_POTENTIAL:
                delete pLog->m_pPotEvent;
                break;
            case EVENT_OCLWRRED:
                delete pLog->m_pEvent;
                break;
        }
    }
    m_Log.clear();
    m_ReqEventLogSize = 0;
}

//--------------------------------------------------------------------
//! This private method is called to determine if all required events
//! are present when gilding against a log file m_ReqEventTimeoutMs is
//! non zero
//!
//! \return true if all required events are present, false otherwise
//!
bool PmGilder::AreRequiredEventsPresent()
{
    vector<LogEntry> newLogEntries;

    if (m_InputFile.empty() || m_RequiredEvents.empty())
        return true;

    if (m_ReqEventLogSize == (UINT32)m_Log.size())
        return false;

    // Extract the log entries from m_Log that would be printed in a
    // log file, and put them in newLogEntries.
    //
    newLogEntries.reserve(m_Log.size());
    for (list<LogEntry>::iterator pLog = m_Log.begin();
         pLog != m_Log.end(); ++pLog)
    {
        if (!pLog->Matches(""))
        {
            newLogEntries.push_back(*pLog);
        }
    }

    m_ReqEventLogSize = (UINT32)m_Log.size();

    // Compare m_RequiredEvents with newLogEntries:
    // In EQUAL mode, the lists should match one-for-one.
    // In either INCLUDE or REQUIRED mode, m_RequiredEvents should be
    // a subset of newLogEntries, possibly in a different order.
    //
    // Required events:
    // In INCLUDE mode, it's possible to have required events by adding
    // a "+" in front of an event in gild file.
    // In either EQUAL or REQUIRED mode, all events in gild file are required.
    //
    if (m_Mode == EQUAL_MODE)
    {
        if (m_RequiredEvents.size() > newLogEntries.size())
        {
            DebugPrintf("%s: EQUAL_MODE Comparing: Size mismatch RequiredEvents.size(%llu) != LoggedEvents.size(%llu)\n",
                        __FUNCTION__, (UINT64)m_RequiredEvents.size(), (UINT64)newLogEntries.size());

            return false;
        }

        for (size_t ii = 0; ii < newLogEntries.size(); ++ii)
        {
            if (!newLogEntries[ii].MatchesRequiredEvent(m_RequiredEvents[ii]))
            {
                DebugPrintf("%s: EQUAL_MODE Comparing: string  %s is NOT found! Stop comparing\n",
                            __FUNCTION__, m_RequiredEvents[ii].c_str());

                // If there is a mismatch, report that all required events
                // have been received (since the test will fail gilding) so
                // that polling for required events will immediately complete
                return true;
            }
        }
    }
    else // m_Mode == INCLUDE_MODE || m_Mode == REQUIRED_MODE
    {
        vector<string>::iterator pReqEvent;

        for (pReqEvent = m_RequiredEvents.begin();
             pReqEvent != m_RequiredEvents.end(); ++pReqEvent)
        {
            bool foundMatch = false;
            for (vector<LogEntry>::iterator pNewLogEntry = newLogEntries.begin();
                 pNewLogEntry != newLogEntries.end(); ++pNewLogEntry)
            {
                if (pNewLogEntry->MatchesRequiredEvent(*pReqEvent))
                {
                    foundMatch = true;
                    newLogEntries.erase(pNewLogEntry);
                    break;
                }
            }

            DebugPrintf("%s: REQUIRED_MODE Comparing: string  %s is %s!\n",
                        __FUNCTION__, pReqEvent->c_str(), foundMatch?"found": "NOT found");

            if (!foundMatch)
                return false;
        }
    }

    return true;
}

/* static */ bool PmGilder::PollReqEvents(void * vpthis)
{
    PmGilder * pthis = (PmGilder *)vpthis;
    return pthis->AreRequiredEventsPresent();
}
