/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ecov_verifier.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "core/include/zlib.h"
#include "core/include/utility.h"
#include <ctype.h>


EcovVerifier::EcovVerifier(const ArgReader* tparams) :
    m_TestParams(tparams),
    m_ResParams(nullptr),
    m_JsParams(nullptr), 
    m_DirSep(1, DIR_SEPARATOR_CHAR),
    m_DirSep2(1, DIR_SEPARATOR_CHAR2),
    m_SpaceDelim(SPACE_DELIMITER),
    m_TestName("test")
{
    if (m_TestParams->ParamPresent("-ignore_ecov_tags"))
    {
        string classNameIgnoreStr = m_TestParams->ParamStr("-ignore_ecov_tags");
        RC rc = Utility::Tokenizer(classNameIgnoreStr, ":", &m_ClassNameIgnoreList);
        if (OK != rc)
        {
            Printf(Tee::PriError, "Failed to parse the value %s for argument -ignore_ecov_tags", classNameIgnoreStr.c_str());
            MASSERT(0);
        }
    }
}

EcovVerifier::~EcovVerifier()
{
    ClearEcovList();

    for (auto simEvent : m_SimEvents)
    {
        delete simEvent.second;
    }
    m_SimEvents.clear();
}

void EcovVerifier::ClearEcovList()
{
    for (auto ecovItem : m_ECovList)
    {
        delete ecovItem;
    }

    m_ECovList.clear();
}

EcovVerifier::TEST_STATUS EcovVerifier::VerifyECov()
{
    // Clear out a list with empty strings only
    if (static_cast<UINT32>(std::count(
        m_CrcChain.begin(), m_CrcChain.end(), "")) == m_CrcChain.size())
    {
        m_CrcChain.clear();
    }

    // Lwrrently Ecov only supports FMODEL
    MASSERT(Platform::GetSimulationMode() == Platform::Fmodel);

    Printf(Tee::PriAlways, "Checking Ecover for this test...\n");

    // Parse ecov file
    const EventCover::Status parseStatus = ParseEcovFiles();
    if (parseStatus == EventCover::Status::False)
    {
        return EcovVerifier::TEST_SUCCEEDED;
    }
    else if (parseStatus == EventCover::Status::Error)
    {
        return EcovVerifier::TEST_FAILED_ECOV;
    }

    // Setup simulation events
    string test_name = "ecov.dat";
    if (!SetupSimEvent(test_name))
    {
        Printf(Tee::PriError, "failed -- error parsing Ecov data file %s\n", test_name.c_str());
        return EcovVerifier::TEST_FAILED_ECOV;
    }
    Printf(Tee::PriAlways, "\n");

    // Check each ECover assertion
    EcovVerifier::TEST_STATUS rc_code = EcovVerifier::TEST_SUCCEEDED;
    for (auto ecovItem : m_ECovList)
    {
        const EventCover::Status matchCondition =
            ecovItem->MatchCondition(m_TestParams, m_ResParams, m_JsParams, m_CrcChain, m_ClassNameIgnoreList);
        if (matchCondition == EventCover::Status::False)
        {
            Printf(Tee::PriDebug, "Line %d in %s skipped: condition not match\n",
                ecovItem->GetLineNumber(),
                ecovItem->GetFileName().c_str());
        }
        else if ((matchCondition == EventCover::Status::Error) &&
            (m_TestParams->ParamPresent("-ignore_ecov_file_errors") > 0))
        {
            Printf(Tee::PriDebug, "Line %d in %s skipped: line has errors, but ignored due to -ignore_ecov_file_errors\n",
                ecovItem->GetLineNumber(),
                ecovItem->GetFileName().c_str());
        }
        else if (matchCondition == EventCover::Status::Error)
        {
            Printf(Tee::PriDebug, "Line %d in %s has errors\n",
                ecovItem->GetLineNumber(),
                ecovItem->GetFileName().c_str());
            rc_code = EcovVerifier::TEST_FAILED_ECOV;
        }
        else if (!ecovItem->MatchEvent(m_SimEvents))
        {
            Printf(Tee::PriError, "ECov failed matching (line %d in %s): '%s'\n",
                ecovItem->GetLineNumber(),
                ecovItem->GetFileName().c_str(),
                ecovItem->GetEvent().c_str());
            rc_code = EcovVerifier::TEST_FAILED_ECOV;
        }
    }

    Printf(Tee::PriAlways, "Ecover checking %s!\n", (rc_code == EcovVerifier::TEST_SUCCEEDED) ? "Passed" : "Failed");
    return rc_code;
}

// Parsing test.ecov file
EventCover::Status EcovVerifier::ParseEcovFiles()
{
    ClearEcovList();

    if (m_EcovFileNames.empty())
    {
        string filename(m_TestName);
        string::size_type fpos = filename.rfind(m_DirSep);
        if (fpos != string::npos)
        {
            filename.replace(fpos+1, string::npos, "test.ecov");
        }
        else
        {
            filename = "test.ecov";
        }

        return ParseEcovFile(filename);
    }
    else
    {
        auto it = m_EcovFileNames.begin();
        EventCover::Status statusS = ParseEcovFile(*it);
        ++it;
        for (; it < m_EcovFileNames.end(); ++it)
        {
            const EventCover::Status status = ParseEcovFile(*it);
            if (statusS == EventCover::Status::True)
            {
                if (status == EventCover::Status::Error)
                {
                    statusS = status;
                }
            }
            else if (statusS == EventCover::Status::False)
            {
                statusS = status;
            }
        }

        return statusS;
    }
}

EventCover::Status EcovVerifier::ParseEcovFile(const string& ecovFileName)
{
    FileHolder file;
    gzFile lgzFile = 0; // pointer to gzipped file;
    bool zipFile = false; // indicates if we are parsing a zip file

    DEFER
    {
        if (lgzFile)
        {
            gzclose(lgzFile);
        }
    };

    if (OK != file.Open(ecovFileName, "r"))
    {
        // check compressed copy
        string zFileName = ecovFileName + ".gz";
        lgzFile = gzopen(zFileName.c_str(), "rb");
        if (!lgzFile)
        {
            // bug2608708
            // As requested, if user does not specify an ecov file
            // and the default ecov file does not exist, 
            // mods would skip ecov check.
            if ((m_TestParams->ParamPresent("-ecov_file") == 0) &&
                (RC::FILE_DOES_NOT_EXIST == file.Open(ecovFileName, "r")) &&
                (RC::FILE_DOES_NOT_EXIST == file.Open(zFileName, "r")))
            {
                Printf(Tee::PriAlways, "Skipped: Could not open ECov File %s for reading\n",
                    ecovFileName.c_str());
                return EventCover::Status::False;
            }

            if (m_TestParams->ParamPresent("-ignore_ecov_file_errors") > 0)
            {
                Printf(Tee::PriAlways, "Skipped: Could not open ECov File %s for reading, but ignored due to -ignore_ecov_file_errors\n",
                    ecovFileName.c_str());
                return EventCover::Status::False;
            }
            else
            {
                Printf(Tee::PriError, "Could not open ECov File %s for reading\n", ecovFileName.c_str());
                return EventCover::Status::Error;
            }
        }
        zipFile = true;
    }

    unique_ptr<EventCover> lwr_ecov = make_unique<EventCover>(m_TestChipName, ecovFileName);
    int         line_num = 0;
    char        buf[512];
    string      lwr_line;
    EventCover::Status status = EventCover::Status::True;
    while ((!zipFile && !feof(file.GetFile())) || 
        (zipFile && !gzeof(lgzFile)))
    {
        if (zipFile)
        {
            if (Z_NULL == gzgets(lgzFile, buf, sizeof(buf)))
            {
                // If a line could not be read from the file, check to see if
                // end-of-file was reached.  If it has, simply exit the loop.
                // Otherwise, there was a real error during file read.
                if (gzeof(lgzFile))
                {
                    break;
                }
                else
                {
                    if (m_TestParams->ParamPresent("-ignore_ecov_file_errors") > 0)
                    {
                        Printf(Tee::PriAlways, "Skipped: Could not open ECov File %s for reading, but ignored due to -ignore_ecov_file_errors\n",
                            ecovFileName.c_str());
                        return EventCover::Status::False;
                    }
                    else
                    {
                        Printf(Tee::PriError, "Could not open ECov File %s for reading\n", ecovFileName.c_str());
                        return EventCover::Status::Error;
                    }
                }
            }
        }
        else
        {
            if (0 == fgets(buf, sizeof(buf), file.GetFile()))
            {
                // If a line could not be read from the file, check to see if
                // end-of-file was reached.  If it has, simply exit the loop.
                // Otherwise, there was a real error during file read.
                if (feof(file.GetFile()))
                {
                    break;
                }
                else
                {
                    if (m_TestParams->ParamPresent("-ignore_ecov_file_errors") > 0)
                    {
                        Printf(Tee::PriAlways, "Skipped: Could not open ECov File %s for reading, but ignored due to -ignore_ecov_file_errors\n",
                            ecovFileName.c_str());
                        return EventCover::Status::False;
                    }
                    else
                    {
                        Printf(Tee::PriError, "Could not open ECov File %s for reading\n", ecovFileName.c_str());
                        return EventCover::Status::Error;
                    }
                }
            }
        }

        lwr_line = buf;
        const EventCover::Status lineStatus = lwr_ecov->ParseOneLine(lwr_line, ++line_num);
        if (lineStatus == EventCover::Status::True)
        {
            m_ECovList.push_back(lwr_ecov.release());
            lwr_ecov = make_unique<EventCover>(m_TestChipName, ecovFileName);
        }
        else if (lineStatus == EventCover::Status::Error)
        {
            Printf(Tee::PriError, "ECov File %s line %d has errors\n",
                ecovFileName.c_str(), line_num);
            status = EventCover::Status::Error;
        }
    }

    if (status == EventCover::Status::True)
    {
        Printf(Tee::PriAlways, "Info: Parse ECov File %s successfully \n", ecovFileName.c_str());
    }
    else if (status == EventCover::Status::Error)
    {
        if (m_TestParams->ParamPresent("-ignore_ecov_file_errors") > 0)
        {
            Printf(Tee::PriAlways, "Error ignored: ECov File %s has errors, which are ignored due to -ignore_ecov_file_errors\n",
                ecovFileName.c_str());
            status = EventCover::Status::True;
        }
        else
        {
            Printf(Tee::PriError, "ECov File %s has errors\n", ecovFileName.c_str());
        }
    }

    return status;
}

// Parse ecov.dat file generated by FMODEL
bool EcovVerifier::SetupSimEvent(const string& file_name)
{
    SimEvent* lwr_event = 0;
    string compressed_file = file_name + ".gz";
    gzFile lgzFile = gzopen(compressed_file.c_str(), "rb");

    FileHolder file;
    bool zipFile = true;
    bool status = true;

    if (!lgzFile)                 // Also try uncompressed version
    {
        if (OK != file.Open(file_name, "r"))
        {
            Printf(Tee::PriError, "Could not open ECov file %s or %s generated by FMODEL\n",
                file_name.c_str(), compressed_file.c_str());
            return false;
        }
        zipFile = false;
    }

    string lwr_line;
    char buf[512];
    while ((!zipFile && !feof(file.GetFile())) ||
        (zipFile && !gzeof(lgzFile)))
    {
        if (zipFile)
        {
            gzgets(lgzFile, buf, sizeof(buf));
        }
        else
        {
            if (0 == fgets(buf, sizeof(buf), file.GetFile()))
            {
                Printf(Tee::PriError, "Unable to read ECov file %s", file_name.c_str());
                return false;
            }
        }

        lwr_line = buf;

        Tokenizer ec_tokenizer(lwr_line, SPACE_DELIMITER, "\"");
        string    lwr_tok;
        EventCover::Status tokenStatus;

        if (((tokenStatus = ec_tokenizer.GetNextToken(lwr_tok)) != EventCover::Status::True) ||
            (lwr_tok[0] == '#'))
        {
            if (tokenStatus == EventCover::Status::Error)
            {
                status = false;
            }

            // Empty or comment line
            continue;
        }

        if (!isspace(lwr_line[0])) // New Event always starts without space
        {
            Tokenizer::StripQuotes(lwr_tok); // Name

            SimEventListType::iterator iter = m_SimEvents.find(lwr_tok);

            if (iter == m_SimEvents.end())
            {
                lwr_event = new SimEvent(lwr_tok, 0);
                m_SimEvents[lwr_tok] = lwr_event;
            }
            else
            {
                Printf(Tee::PriAlways, "Event \"%s\" defined twice in %s\n", lwr_tok.c_str(),
                    file_name.c_str());
                lwr_event = iter->second;
            }
            MASSERT(lwr_event);

            // Get the subdimension number
            ec_tokenizer.ResetDelimiter(m_SpaceDelim+"()");
            while ((tokenStatus = ec_tokenizer.GetNextToken(lwr_tok)) == EventCover::Status::True)
            {
                int dim = strtol(lwr_tok.c_str(), 0, 0);
                if (dim >= 0)
                {
                    lwr_event->AddSubEvtDim(dim);
                }
                else
                {
                    MASSERT(!"Unsupported dimension size!");
                }
            }

            if (tokenStatus == EventCover::Status::Error)
            {
                status = false;
            }

            if (lwr_event->GetSubEventVecDim())
            {
                // Prepare to store subevent counts
                lwr_event->ResizeSubEvtCountVec();
            }
            continue;
        }

        if (!lwr_event)
        {
            Printf(Tee::PriError, "%s: Subevent/event count defined before event itself\n", lwr_line.c_str());

            if (zipFile)
                gzclose(lgzFile);

            return false;
        }

        // If no subevent, this is event count
        size_t subevt_dim = lwr_event->GetSubEventVecDim();
        if (!subevt_dim)
        {
            lwr_event->SetEventCount(strtoul(lwr_tok.c_str(), 0, 0));
            lwr_event = 0;
            continue;
        }

        // Set sub-event index name and value
        SubEvtAddrType  subevt_v;
        do
        {
            size_t lwr_idx = subevt_v.size();
            char*  next_val = 0;

            if (lwr_tok[0] == '-')   // Invalid subevent index
            {
                // Invalid count always store as the last element
                MASSERT(subevt_dim > lwr_idx);
                subevt_v.push_back(lwr_event->GetSubEventDimSize(lwr_idx));
            }
            else                // regular index
            {
                UINT32 val = strtoul(lwr_tok.c_str(), &next_val, 0);

                if (subevt_dim == lwr_idx) // subevent count
                {
                    lwr_event->SetSubEventCount(val, subevt_v);
                    MASSERT((*next_val) != '=');
                    break;      // Ignore the rest of the line
                }
                else            // subevent index
                {
                    subevt_v.push_back(val);

                    if (*next_val == '=') // Subevent idx name
                    {
                        string vec_name(next_val+1);
                        lwr_event->SetSubEvtIdxName(lwr_idx, val, Tokenizer::StripQuotes(vec_name));
                    }
                }
            }
        }
        while (ec_tokenizer.GetNextToken(lwr_tok) == EventCover::Status::True);
    }

    if (zipFile)
        gzclose(lgzFile);

    return status;
}

void EcovVerifier::AddCrcChain(const CrcChain* crcc)
{
    if (!crcc)
    {
        return;
    }

    m_CrcChain.insert(m_CrcChain.end(), crcc->begin(), crcc->end());
}

void EcovVerifier::SetGpuResourceParams(const ArgReader* pResParams)
{
    m_ResParams = pResParams;
}

void EcovVerifier::SetJsParams(const ArgReader* pJsParams)
{
    m_JsParams = pJsParams;
}

void EcovVerifier::SetTestName(const string& testName)
{
    m_TestName = Utility::FixPathSeparators(testName);
}

void EcovVerifier::SetTestChipName(const string& TestChipName)
{
    m_TestChipName = TestChipName;
}

void EcovVerifier::SetupEcovFile(const string& ecovFileName)
{
    m_EcovFileNames.push_back(Utility::FixPathSeparators(ecovFileName));
}

void EcovVerifier::DumpEcovData(bool isCtxSwitching)
{
    static bool ecovDataDumped = false;

    // If all tests run in parallel, current FModel only have one ecover event database.
    // And each time MODS send down "LwrrentTestEnded", FModel will dump database and clear it.
    // So MODS has to dump only once for context switching mode
    if (ecovDataDumped && isCtxSwitching)
        return;

    Platform::EscapeWrite("LwrrentTestEnded", 0, 4, 0);
    ecovDataDumped = true;
}
