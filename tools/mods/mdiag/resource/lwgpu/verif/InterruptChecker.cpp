/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011, 2015-2019, 2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>
#include <cstdio>
using namespace std;

#include "lwmisc.h"
#include "core/include/argread.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/CrcInfo.h"
#include "mdiag/utils/ICrcProfile.h"
#include "mdiag/utils/XMLlogging.h"
#include "core/utility/errloggr.h"
#include "fermi/gf100/dev_graphics_nobundle.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"

#include "VerifUtils.h"
#include "InterruptChecker.h"

UINT32 IntrErrorFilter2ZeroPadSize = 0xffffffff;

bool IntrErrorFilter2(UINT64, const char*, const char*);

void InterruptChecker::Setup(GpuVerif* verifier)
{
    m_Verifier = verifier;
    m_CrcMode = m_Verifier->CrcMode();
    m_Params = m_Verifier->Params();
    m_CrcProfile = m_Verifier->Profile();
}

// This function should be called when the InterruptChecker class is used
// without a GpuVerif object.
//
void InterruptChecker::StandaloneSetup
(
    const ArgReader* params, 
    ICrcProfile* crcProfile
)
{
    MASSERT(m_Verifier == nullptr);
    m_Params = params;
    m_CrcProfile = crcProfile;
}

void InterruptChecker::Cleanup()
{
    // Nothing to clean
}

TestEnums::TEST_STATUS InterruptChecker::Check(ICheckInfo* info)
{
    TestEnums::TEST_STATUS intrStatus = TestEnums::TEST_SUCCEEDED;
    ITestStateReport* report = info->Report;

    if (m_CrcMode != CrcEnums::CRC_NONE)
    {
        intrStatus = CheckIntrPassFail();
        if (intrStatus != TestEnums::TEST_SUCCEEDED)
        {
            intrStatus = TestEnums::TEST_FAILED_CRC;
        }
    }
    else
    {
        InfoPrintf("Skipping CRC check by user request!\n");
        ErrorLogger::IgnoreErrorsForThisTest();
    }

    // now see if we are in crc dump or update mode, and if so, dump the new crc file/images
    if ((m_CrcMode==CrcEnums::CRC_DUMP || m_CrcMode==CrcEnums::CRC_UPDATE))
    {
        // Interrupt files can't be dumped in standalone mode.
        MASSERT(m_Verifier != nullptr);

        InfoPrintf("-crcDump/-crlwpdate section\n");
        m_Verifier->GetDumpUtils()->DumpUpdateIntrs(report);
    }

    // Only do the status thing if we actually were told to do the crc check
    if (m_CrcMode != CrcEnums::CRC_NONE)
    {
        if (gXML != NULL)
        {
            VerifUtils::WritePostTestCheckXml(
                    string(), intrStatus, true);
        }

        if (intrStatus == TestEnums::TEST_SUCCEEDED)
        {
            report->crcCheckPassedGold();
        }
        else
        {
            report->crcCheckFailed("INTR check failed!!!");
        }
    }
    else
    {
        report->runSucceeded();
    }
    return (intrStatus != TestEnums::TEST_SUCCEEDED) ? TestEnums::TEST_FAILED_CRC : intrStatus;
}

TestEnums::TEST_STATUS InterruptChecker::CheckIntrPassFail()
{
    TestEnums::TEST_STATUS result = TestEnums::TEST_SUCCEEDED;

    if (m_Params->ParamPresent("-skip_intr_check") > 0)
    {
        InfoPrintf("InterruptChecker::Check:  Skipping intr check per commandline override!\n");
        ErrorLogger::IgnoreErrorsForThisTest();
        return result;
    }

    const string ifile = GetIntrFileName();
    const char *intr_file = ifile.c_str();
    FILE *wasFilePresent = NULL;

    // Yield once to give ErrLogger a chance to capture the outstanding interrupts.
    Tasker::Yield();

    UINT32 grIdx = ErrorLogger::ILWALID_GRIDX;
    ErrorLogger::CheckGrIdxMode checkGrIdx = ErrorLogger::None;
    if (m_Verifier != nullptr)
    {
        if (m_Verifier->LWGpu()->IsSMCEnabled())
        {
            if (m_Params->ParamPresent("-compare_all_intr"))
            {
                checkGrIdx = ErrorLogger::None;
            }
            else if (m_Params->ParamPresent("-compare_untagged_intr"))
            {
                checkGrIdx = ErrorLogger::IdxAndUntaggedMatch;
            }
            else
            {
                checkGrIdx = ErrorLogger::IdxMatch;
            }
        }
    
        // Send grIdx only if Smc Mode and channel is compute
        if (m_Verifier->GetSmcEngine() && 
            m_Verifier->Channel() &&
            m_Verifier->Channel()->GetComputeSubChannel())
        {
            grIdx = m_Verifier->GetSmcEngine()->GetSysPipe();
        }
    }

    // If there were interrupts detected, check to make sure that they
    // are the same interrupts that the test expects.
    if (ErrorLogger::HasErrors(checkGrIdx, grIdx))
    {
        if (m_Params->ParamPresent("-zero_pad_log_strings"))
        {
            IntrErrorFilter2ZeroPadSize = m_Params->ParamUnsigned("-zero_pad_log_strings", IntrErrorFilter2ZeroPadSize);
        }
        ErrorLogger::InstallErrorFilter(IntrErrorFilter2);
        ErrorLogger::CompareMode compMode = ErrorLogger::Exact;
        if (m_Params->ParamPresent("-interrupt_check_ignore_order") > 0)
        {
            compMode = ErrorLogger::OneToOne;
        }

        RC rc = ErrorLogger::CompareErrorsWithFile(intr_file, compMode, checkGrIdx, grIdx);

        if(rc != OK)
        {
            result = TestEnums::TEST_FAILED_CRC;
            if(Utility::OpenFile(intr_file, &wasFilePresent, "r") != OK)
            {
                if (m_Params->ParamPresent("-crcMissOK") != 0)
                {
                    result = TestEnums::TEST_SUCCEEDED;
                }
                else
                {
                    ErrPrintf("InterruptChecker::Check: interrupts detected, but %s file not found.\n",
                        intr_file);
                }
            }
            else
            {
                ErrPrintf("InterruptChecker::Check: interrupts detected, but could not compare with %s.\n",
                    intr_file);
 
 
            }
            ErrorLogger::PrintErrors(Tee::PriNormal);
            ErrorLogger::TestUnableToHandleErrors();
        }
    }

    // If there were no interrupts but -interrupt_file was specified,
    // issue an error.  Issue an additional error if the file pointed to
    // by -interrupt_file does not exist.
    else if (m_Params->ParamPresent("-interrupt_file") > 0)
    {
        ErrPrintf("-interrupt_file was specified but no interrupts were detected.\n");
        if (OK != Utility::OpenFile(intr_file, &wasFilePresent, "r"))
        {
            ErrPrintf("-interrupt_file %s does not exist.\n", intr_file);
        }
        result = TestEnums::TEST_FAILED_CRC;
    }

    // If -interrupt_file was not specified, then intr_file points to a
    // .int file in the trace directory.  If this file doesn't exist,
    // then no interrupts were expected and no error should be issued.
    // If the file does exist, then interrupts were expected and an error
    // will be issued because none were detected.
    else
    {
        // Use fopen here because Utility::OpenFile will print an error
        // message if the file doesn't exist and we actually want an
        // error message only if the file does exist.
        wasFilePresent = fopen(intr_file, "r");
        if(wasFilePresent)
        {
            ErrPrintf("InterruptChecker::Check: NO error interrupts detected, but %s file exists!\n",
                intr_file);
            result = TestEnums::TEST_FAILED_CRC;
        }
    }

    if(wasFilePresent) fclose(wasFilePresent);

    return result;
}

const string InterruptChecker::GetIntrFileName() const
{
    string IntrDir(m_CrcProfile->GetFileName()); //Get the default directory of the .int file
    string::size_type dir_sep = IntrDir.rfind( DIR_SEPARATOR_CHAR );
    string IntrFile("");
    if( dir_sep == string::npos ) //Not changing directories: the default exist in the current directory
    {
        IntrFile = IntrDir;
        IntrFile.replace( IntrFile.length()-3, 3, "int"); //default file is profile file, replacing .crc with .int
        IntrDir.erase(0, string::npos); //It's really an empty string, as there is not extra directories to go into
    }
    else
    {
        IntrFile = IntrDir.substr(dir_sep+1, string::npos);
        IntrFile.replace( IntrFile.length()-3, 3, "int");
        IntrDir.erase(dir_sep+1, string::npos); //truncate the profile file
    }

    if (m_Params->ParamPresent("-interrupt_dir")) //a manual directory is being used
    {
        string dir_name = m_Params->ParamStr("-interrupt_dir");
        if( !dir_name.empty() )
        {
            IntrDir = dir_name;
            if( DIR_SEPARATOR_CHAR != *IntrDir.rbegin() ) //if the user didn't append a / (or \) at the end
            {
                IntrDir += DIR_SEPARATOR_CHAR;
            }
        }
    }

    if (m_Params->ParamPresent("-interrupt_file")) //a manual file name is being used
    {
        IntrFile = m_Params->ParamStr("-interrupt_file");
    }

    return IntrDir + IntrFile; //directory concatenated with filename
}

// Helper for ErrorLogger so that it can support legacy HW engine error
// handling in the same way as before
//------------------------------------------------------------------------------
struct IntrEntry
{
    UINT32 gr_intr;
    UINT32 class_error;
    UINT32 addr;
    UINT32 data_lo;
};

void ZeroPad(const char* str, std::string* strPadded, UINT32 size)
{
    const char* cursor = str;
    while (*(cursor+1) != '\0')
    {
        if (*cursor == '0' && (*(cursor+1) == 'x' || *(cursor+1) == 'X'))
        {
            cursor += 2;
            int numLength = 0;
            const char* endNum = cursor;
            while (*endNum != '\0' && *endNum != ' ' && *endNum != ',' &&
                   *endNum != '(' && *endNum != ')')
            {
                endNum++;
                numLength++;
            }
            *strPadded += '0';
            *strPadded += 'x';
            for (int index = size; index > numLength; index--)
                *strPadded += '0';
        }
        else
        {
            *strPadded += *cursor;
            cursor++;
        }
    }
    *strPadded += *cursor;
}

bool IntrErrorFilter2(UINT64 intr, const char *lwrErr, const char *expectedErr)
{
    IntrEntry lwr, expected;

    // If either the current or expected error doesnt match the default interrupt format
    // assume it is a real error
    if ((sscanf(lwrErr, "gr_intr=0x%08x class_error=0x%08x trapped addr=0x%08x data_lo=0x%08x\n",
                &lwr.gr_intr, &lwr.class_error, &lwr.addr, &lwr.data_lo) != 4) ||
        (sscanf(expectedErr, "gr_intr=0x%08x class_error=0x%08x trapped addr=0x%08x data_lo=0x%08x\n",
                &expected.gr_intr, &expected.class_error, &expected.addr, &expected.data_lo) != 4))
    {
        if( IntrErrorFilter2ZeroPadSize != 0xffffffff )
        {
            std::string lwrErrPad;
            ZeroPad(lwrErr, &lwrErrPad, IntrErrorFilter2ZeroPadSize);
            if(Utility::MatchWildCard(lwrErrPad.c_str(), expectedErr) == true)
            {
                DebugPrintf("TraceFile::IntrErrorFilter: Previous miscompare deemed acceptable\n");
                return false;
            }
        }
        return true;
    }

    // would be nice to pull these in from a ref header!
    #define TRAPPED_ADDR_MTHD_MSK 0xffff

    // We always expect to match in the STATUS valid bit
    bool failStatus =
        DRF_VAL(_PGRAPH, _TRAPPED_ADDR, _STATUS, lwr.addr) !=
        DRF_VAL(_PGRAPH, _TRAPPED_ADDR, _STATUS, expected.addr);

    bool failMethod = false;
    bool failDataLo = false;
    if (DRF_VAL(_PGRAPH, _TRAPPED_ADDR, _STATUS, lwr.addr) ==
        LW_PGRAPH_TRAPPED_ADDR_STATUS_VALID)
    {
        // TRAPPED_ADDR is valid -- compare method and data_lo
        failMethod = (lwr.addr & TRAPPED_ADDR_MTHD_MSK) != (expected.addr & TRAPPED_ADDR_MTHD_MSK);
        failDataLo = lwr.data_lo != expected.data_lo;
    }

    // Note that we never check DATA_HI.  It's not deterministic, and it's never
    // relevant to the error anyway.  If the method represented by DATA_HI were
    // to cause an error, then it'd get moved down into DATA_LO first before the
    // error was reported.

    // if this is a SetContextDma method with a handle, data will not match
    if ((lwr.addr & TRAPPED_ADDR_MTHD_MSK) < 0x0200) // method where host does hash lookup
        failDataLo = false;

    if ((lwr.gr_intr != expected.gr_intr) ||
        (lwr.class_error != expected.class_error) ||
        failStatus || failMethod || failDataLo)
    {
        InfoPrintf("TraceFile::IntrErrorFilter:\n"
            "expected:\n%s"
            "actual:\n"
            "gr_intr=0x%08x class_error=0x%08x trapped addr=0x%08x data_lo=0x%08x\n"
            "Mismatch on interrupt %lld\n",
            expectedErr,
            lwr.gr_intr, lwr.class_error, lwr.addr, lwr.data_lo, intr);
        return true; // there was a real error
    }

    DebugPrintf("TraceFile::IntrErrorFilter: Previous miscompare deemed acceptable\n");
    return false;
}
