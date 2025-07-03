/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/onetime.h"

#include "core/include/pool.h"
#include "core/include/debuglogsink.h"
#include "core/include/mle_protobuf.h"
#include "core/include/filesink.h"
#include "core/include/log.h"
#include "core/include/cnslmgr.h"
#include "core/include/jsonlog.h"
#include "core/include/cmdline.h"
#include "core/include/xp.h"
#include "core/include/stdoutsink.h"

long    OneTimeInit::s_OneTimeRefCount    = 0;
time_t  OneTimeInit::s_OneTimeStartTime   = 0;
time_t  OneTimeInit::s_OneTimeEndTime     = 0;
UINT64  OneTimeInit::s_OneTimeStartTimeNs = 0;
FLOAT64 OneTimeInit::s_OneTimeElapsed     = 0;

//------------------------------------------------------------------------------
// Get the date of birth of mods (its a Modular Diagnostic System!)
string OneTimeInit::GetDOB()
{
    string dateOfBirth = ctime(&s_OneTimeStartTime);
    const size_t i = dateOfBirth.find('\n');
    if (i != string::npos)
    { // replace the "\n" with " "
        dateOfBirth[i] = ' ';
    }
    return dateOfBirth.c_str();
}

//------------------------------------------------------------------------------
// Get the date of birth of mods as a UNIX timestamp
time_t OneTimeInit::GetDOBUnixTime()
{
    return s_OneTimeStartTime;
}

//------------------------------------------------------------------------------
// Get the Date of Death of mods (RIP)
string OneTimeInit::GetDOD()
{
    string dateOfDeath = ctime(&s_OneTimeEndTime);
    if (dateOfDeath.find('\n') != string::npos)
    { // replace the "\n" with " "
        dateOfDeath.replace(dateOfDeath.find('\n'), 1, 1, ' ');
    }
    return dateOfDeath;
}

//------------------------------------------------------------------------------
// Get mods age at death (Live long a prosper)
string OneTimeInit::GetAge(bool addHMS)
{
    string str = Utility::StrPrintf("%.3f ", s_OneTimeElapsed);

    if (addHMS)
    {
        UINT64 timeElapsedInSec = static_cast<UINT64>(s_OneTimeElapsed);
        FLOAT64 seconds = s_OneTimeElapsed -
                          static_cast<FLOAT64>(timeElapsedInSec) +
                          timeElapsedInSec % 60;
        UINT64 minutes = (timeElapsedInSec / 60) % 60;
        UINT64 hours = (timeElapsedInSec / 3600);
        str += Utility::StrPrintf("seconds (%02llu:%02llu:%06.3f h:m:s)",
                                  hours, minutes, seconds);
    }
    return str;
}
//------------------------------------------------------------------------------
// Take the current timestamp to determine Age() and DOD()
void OneTimeInit::Timestamp(Mods::Stage stage)
{
    switch (stage)
    {
        case Mods::Stage::Start:
            // s_OneTimeStartTime is time in seconds since Epoch and it is used to indicate start
            // of MODS in the binary log (MLE).  Then s_OneTimeStartTimeNs is used to callwlate
            // timestamp since the beginning of MODS.  Every entry in binary log (MLE) has
            // a timestamp in ns since the beginning of MODS.  In order to be able to synchronize
            // mutiple logs, e.g. from parallel MODS runs or from other tools, the time when
            // MODS started must be added to the time stamp to achieve time with good enough
            // precision.
#ifdef __linux__
            // On Linux GetWallTimeNS() returns time since Epoch in ns.
            // MODS start time in the log is printed with 1s precision, so round it down to
            // a full second so that the timestamps can still be used to callwlate times of
            // each log entry with a higher precision than 1s.
            s_OneTimeStartTime   = static_cast<time_t>(Xp::GetWallTimeNS() / 1'000'000'000ULL);
            s_OneTimeStartTimeNs = s_OneTimeStartTime * 1'000'000'000ULL;
#else
            // On non-Linux OS-es we can't obtain time since Epoch with a sub-1s precision,
            // so the log entries will always have 1s precision.
            s_OneTimeStartTime   = time(0);
            s_OneTimeStartTimeNs = Xp::GetWallTimeNS();
#endif
            break;

        case Mods::Stage::End:
            s_OneTimeEndTime = time(0);
            s_OneTimeElapsed =
                static_cast<FLOAT64>(Xp::GetWallTimeNS() - s_OneTimeStartTimeNs)
                / 1000'000'000.0;

            // Sanity check: time elapsed < 1 year
            MASSERT(s_OneTimeElapsed < 60 * 60 * 24 * 365);
            break;
        default:
            break;
    }
}

void OneTimeInit::PrintStageTimestamp(Mods::Stage modsStage, bool skipMleHeader)
{
    const Tee::Priority pri = Tee::PriNormal;
    if ((modsStage & Mods::Stage::Start) != 0)
    {
        if (!skipMleHeader)
        {
            if (CommandLine::MleStdOut())
            {
                bool printedMleHeader = false;

                // Try printing to the stdout sink
                if (Tee::GetStdoutSink())
                {
                    printedMleHeader = Tee::GetStdoutSink()->PrintMleHeader();
                }

                // If stdout didn't work, try screen which also goes to stdout
                if (!printedMleHeader && Tee::GetScreenSink())
                {
                    printedMleHeader = Tee::GetScreenSink()->PrintMleHeader();
                }
                MASSERT(printedMleHeader);
            }

            FileSink *pMleSink = Tee::GetMleSink();
            if (pMleSink && pMleSink->IsFileOpen())
            {
                bool printed = pMleSink->PrintMleHeader();
                MASSERT(printed);
                (void)printed;
            }
        }

        // Print the DOB timestamp
        static constexpr char MLE_TIME_HEADER[] = "MLE Start Time:";
        Printf(Tee::MleOnly, "%s %lld%s\n", MLE_TIME_HEADER,
               static_cast<INT64>(OneTimeInit::GetDOBUnixTime()),
               OneTimeInit::UtcOffsetStr().c_str());

        Printf(pri, "MODS start: %s\n", OneTimeInit::GetDOB().c_str());
    }

    if ((modsStage & Mods::Stage::End) != 0)
    {
        Printf(pri, "MODS end  : %s [%s]\n",
               OneTimeInit::GetDOD().c_str(),
               OneTimeInit::GetAge(true).c_str());
    }
}

//------------------------------------------------------------------------------
// Determine the UTC offset to local time and return as a string
string OneTimeInit::UtcOffsetStr()
{
    time_t t = time(nullptr);
    tm local = *localtime(&t);
    tm utc = *gmtime(&t);

    bool positiveOffset;
    if (utc.tm_year != local.tm_year)
    {
        positiveOffset = utc.tm_year < local.tm_year;
    }
    else if (utc.tm_yday != local.tm_yday)
    {
        positiveOffset = utc.tm_yday < local.tm_yday;
    }
    else if (utc.tm_hour != local.tm_hour)
    {
        positiveOffset = utc.tm_hour < local.tm_hour;
    }
    else if (utc.tm_min != local.tm_min)
    {
        positiveOffset = utc.tm_min < local.tm_min;
    }
    else
    {
        // If the timezones are equal (i.e. GMT) the offset's
        // still written as a positive offset (+00:00)
        positiveOffset = utc.tm_sec <= local.tm_sec;
    }

    tm* ahead;
    tm* behind;

    if (positiveOffset)
    {
        ahead = &local;
        behind = &utc;
    }
    else
    {
        ahead = &utc;
        behind = &local;
    }

    // if the days are different, ahead crossed to a new day, so
    // we need to add that day to ahead's hours before callwlating
    if (ahead->tm_yday != behind->tm_yday)
    {
        ahead->tm_hour += 24;
    }

    int aheadInMin = ahead->tm_hour * 60 + ahead->tm_min;
    int behindInMin = behind->tm_hour * 60 + behind->tm_min;
    int offsetInMin = aheadInMin - behindInMin;

    int hours = offsetInMin / 60;
    int minutes = offsetInMin % 60;
    MASSERT(hours <= 14);
    MASSERT(minutes % 15 == 0);
    char sign = positiveOffset ? '+' : '-';

    return Utility::StrPrintf("%c%02d:%02d", sign, hours, minutes);
}

OneTimeInit::OneTimeInit()
{
    if (0 == s_OneTimeRefCount++)
    {
        // birth certificate
        Timestamp(Mods::Stage::Start);

        // Setup initial timestamp for logging
        Tee::InitTimestamp(s_OneTimeStartTimeNs);

        CommandLine::Initialize();

        // Tee should be available right off the bat, but as it is used by the
        // leak checking mechanism (and must persist through its shutdown), its
        // allocations cannot be monitored by the leak checker
        Tee::TeeInit();

        // Initialize the LwDiagUtils library so that it calls MODS specific
        // functions for asserts and printf
        // It should be done after LeakCheckInit as the library shutdown
        // happens before LeakCheckShutdown
        LwDiagUtils::Initialize(ModsAssertFail, ModsExterlwAPrintf);
    }
}

OneTimeInit::~OneTimeInit()
{
    if (0 == --s_OneTimeRefCount)
    {
        // death certificate (RIP)
        Timestamp(Mods::Stage::End);
        PrintStageTimestamp(Mods::Stage::End);
        // Make sure JsonItem resources are freed before the leak-check.
        JsonItem::FreeSharedResources();

        // unhook the screen sink, since ConsoleManager will be shut down
        DataSink *screen = Tee::GetScreenSink();
        Tee::SetScreenSink(NULL);

        // since the screen sink is gone, use the StdOut sink for display
        Tee::GetStdoutSink()->Enable();

        ConsoleManager::Shutdown();

        // Shutdown the LwDiagUtils library to free any allocations
        LwDiagUtils::Shutdown();

        // Free the TeeModule maps.
        Tee::FreeTeeModules();

        // Release debug log sink before checking for leaks
        Tee::GetDebugLogSink()->Uninitialize();

        CommandLine::Reset();

        // perform one-time shutdown
        bool success = Pool::LeakCheckShutdown();

        // Print message to MLE that MODS has finished
        Mle::Print(Mle::Entry::mods_end).rc(Log::FirstError());

        // destroy Tee now that the leak checker has done its thing
        Tee::SetScreenSink(screen);
        Tee::TeeDestroy();

        CommandLine::Shutdown();

        if (!success)
        {
            // return a failure exit code if we exceeded the leak threshold
            Xp::ExitFromGlobalObjectDestructor(1);
        }
    }
}
