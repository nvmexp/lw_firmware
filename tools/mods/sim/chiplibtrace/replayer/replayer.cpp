/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014,2016-2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To do: Instruction of the tool
//
#ifdef INCLUDE_MDIAG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include "core/include/types.h"
#include "assert.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/argdb.h"
#include "chiplibtrace.h"
#include "core/include/argread.h"
#include "core/include/platform.h"
#include "core/include/gpu.h"
#include "core/include/chiplibtracecapture.h"

#include "mdiag/tests/testdir.h"
#include "core/utility/trep.h"
#include "mdiag/sysspec.h"
#include "core/include/xp.h"
#include <time.h>

namespace Replayer{

class ChiplibTrace;
class TraceParser
{
public:
    TraceParser(){};
    virtual RC Parse(const std::string& file, ChiplibTrace& trace);
    virtual ~TraceParser(){};
};

static bool IsReplaying = false;
bool IsReplayingTrace()
{
    return IsReplaying;
}
extern const ParamDecl replayer_params[] =
{

    STRING_PARAM("-i", "specifies path to input trace file"),
    SIMPLE_PARAM("-disable_escape", "disable escape read/write in replayer"),
    STRING_PARAM("-ref_file", "specifiy ref file to be used for register decoding"),
    STRING_PARAM("-trepfile", "Set the name of the trep (test report) file"),
    UNSIGNED_PARAM("-sleep_ms_hw", "max sleep time in ms on hw"),
    UNSIGNED_PARAM("-poll_cnt_hw", "max poll count for read"),
    SIMPLE_PARAM("-ignore_optional", "ignore optional trace ops"),
    SIMPLE_PARAM("-full_trace_read", "read trace in full before replaying"),
    SIMPLE_PARAM("-bar1_must_match", "fail test if bar1 read mismatch after max retry"),
    SIMPLE_PARAM("-disable_bar1_must_match", "disable -bar1_must_match which is enabled by default"),
    SIMPLE_PARAM("-bar0_must_match", "fail test if some special bar0 read mismatch after max retry"),
    LAST_PARAM
};

static const ParamConstraints args_constraints[] =
{
    MUTUAL_EXCLUSIVE_PARAM("-bar1_must_match", "-disable_bar1_must_match"),
};

RC Run(ArgDatabase *pArgs)
{
    IsReplaying = true;
    time_t start_time = time(NULL);
    Printf(Tee::PriDebug, "Starting Replayer: Time  %ld\n", start_time);
    unique_ptr<ArgReader> reader(new ArgReader(replayer_params));
    std::string trace;
    if (!reader->ParseArgs(pArgs))
    {
        Printf(Tee::PriNormal, "trace_3d : Factory() error reading device parameters!\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (!reader->ValidateConstraints(args_constraints))
    {
        Printf(Tee::PriError, "Argument validation failed!\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (reader->ParamPresent("-i")) {
        trace = string(reader->ParamStr("-i"));
    }
    ReplayOptions option;
    if (reader->ParamPresent("-sleep_ms_hw")) {
        option.maxSleepTimeInMs = reader->ParamUnsigned("-sleep_ms_hw", 100);
    }
    if (reader->ParamPresent("-poll_cnt_hw")) {
        option.maxPollCnt = reader->ParamUnsigned("-poll_cnt_hw", 5);
    }
    if (reader->ParamPresent("-disable_escape")) {
        option.enableEscape = false;
    }
    std::string refFile;
    if (reader->ParamPresent("-ref_file")) {
        refFile = string(reader->ParamStr("-ref_file"));
    }
    if (reader->ParamPresent("-ignore_optional")) {
        option.replayOptional = false;
    }
    if (reader->ParamPresent("-full_trace_read")) {
        option.inplaceReplay = false;
    }
    if (reader->ParamPresent("-bar1_must_match")) {
        option.bar1MustMatch = true;
    }
    if (reader->ParamPresent("-disable_bar1_must_match")) {
        option.bar1MustMatch = false;
    }

    using namespace Replayer;
    TraceParser parser;
    ChiplibTrace t(option, refFile);
    ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
    Platform::Initialize();

    RC rc = parser.Parse(trace, t);
    if(rc != OK)
    {
        Printf(Tee::PriNormal, "Parse trace failed: %s\n", rc.Message());
        std::string message;
        t.Report(message);
        return rc;
    }

    if (!option.inplaceReplay)
    {
        RC rc = t.Replay();
        if (rc != OK)
        {
            Printf(Tee::PriNormal, "Replay trace failed: %s\n", rc.Message());
            std::string message;
            t.Report(message);
            return rc;
        }
    }

    std::string message;
    RC replayPassed = t.Report(message);
    Trep trep;
    TestDirectory testdir(pArgs, &trep);
    if(reader->ParamPresent("-trepfile"))
        trep.SetTrepFileName(reader->ParamStr("-trepfile"));
    else
        trep.SetTrepFileName("trep.txt");
    trep.AppendTrepResult(1, replayPassed);
    // Platform::Cleanup() is called by mods from Xp::OnExit();
    time_t end_time = time(NULL);
    Printf(Tee::PriDebug, "Ending Replayer: Time  %ld\n", end_time);
    Printf(Tee::PriNormal, "Total time to replay: %ld seconds\n",
            (long)(end_time - start_time));
    IsReplaying = false;
    return replayPassed;
}

static int Tokenize(char* buf, std::vector<const char*>& tokens)
{
    if (!buf)
        return -1;
    char* p = buf;
    bool in_token = true;
    bool in_comment = false;
    while (*p)
    {
        if (isspace(*p))
        {
            if (*p == '\n')
            {
                *p = '\0';
                break;
            }
            if (!in_token && !in_comment)
            {
                *p = '\0';
                in_token = true;
            }
        }
        else if (*p == '#')
        {
            in_comment = true;
            in_token = false;
            tokens.push_back(p);
        }
        else
        {
            if (in_token)
            {
                in_token = false;
                tokens.push_back(p);
            }
        }
        p++;
    }
    return 0;
}

RC TraceParser::Parse(const string& file, ChiplibTrace& trace)
{
    string fileName = file;
    string zippedFileName = fileName + ".gz";
    if (!Xp::DoesFileExist(file))
    {
        Printf(Tee::PriHigh, "Warning: Could not open trace file \"%s\", trying \"%s\"...\n",
            fileName.c_str(), zippedFileName.c_str());

        if (!Xp::DoesFileExist(zippedFileName))
        {
            Printf(Tee::PriHigh, "Error: Could not open neither \"%s\" nor \"%s\", bailing out.\n",
                fileName.c_str(), zippedFileName.c_str());
            return RC::CANNOT_OPEN_FILE;
        }

        fileName = zippedFileName;
    }

    FileIO *fh = Sys::GetFileIO(fileName.c_str(), "r");
    if (fh == NULL)
    {
        Printf(Tee::PriHigh, "Error: Could not open neither \"%s\" nor \"%s\", bailing out.\n",
            fileName.c_str(), zippedFileName.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    TraceOp* op = NULL;
    char* buf = NULL;
    bool isOptionalSection = false;
    int lineNum = 0;
    string line;
    while (fh->FileGetLine(line))
    {
        lineNum++;

        buf = new char[line.size() + 1];
        strcpy(buf, line.c_str());
        std::vector<const char*> tokens;
        Tokenize(buf, tokens);
        size_t sz = tokens.size();
        if (!sz)
            continue;
        std::string comment;
        if (tokens[sz-1] && tokens[sz-1][0] == '#')
        {
            comment = tokens[sz-1];
            tokens.pop_back();
        }
        if     (strcmp(tokens[0], "rd"        ) == 0)
            op = new BusMemOp(&trace, tokens, BusMemOp::READ, comment, lineNum);
        else if(strcmp(tokens[0], "wr"        ) == 0)
            op = new BusMemOp(&trace, tokens, BusMemOp::WRITE, comment, lineNum);
        else if(strcmp(tokens[0], "rd_mask"   ) == 0)
            op = new Mask32ReadOp(&trace, tokens, comment, lineNum);
        else if(strcmp(tokens[0], "esc_rd"    ) == 0)
            op = new EscapeOp(&trace, tokens, EscapeOp::READ, comment, lineNum);
        else if(strcmp(tokens[0], "esc_wr"    ) == 0)
            op = new EscapeOp(&trace, tokens, EscapeOp::WRITE, comment, lineNum);
        else if(strcmp(tokens[0], "@tag_begin"  ) == 0 &&
                strcmp(tokens[1],"optional") == 0)
        {
            isOptionalSection = true;
            op = new TagInfoOp(&trace, tokens, comment, lineNum);
        }
        else if (strcmp(tokens[0], "@tag_end") == 0 &&
                strcmp(tokens[1], "optional") == 0)
        {
            op = new TagInfoOp(&trace, tokens, comment, lineNum);
            isOptionalSection = false;
        }
        else if(strcmp(tokens[0], "@annot"    ) == 0)
            op = new AnnotationOp(&trace, tokens, comment, lineNum);
        else if(strcmp(tokens[0], "dump_raw_mem") == 0)
            op = new DumpRawMemOp(&trace, tokens, comment, lineNum);
        else if(strcmp(tokens[0], "load_raw_mem") == 0)
            op = new LoadRawMemOp(&trace, tokens, comment, lineNum);
        else if(strcmp(tokens[0], "dump_phys_mem") == 0)
            op = 0;  // This op is only for the verilog replayer flow.
        else if(strcmp(tokens[0], "load_phys_mem") == 0)
            op = 0;  // This op is only for the verilog replayer flow.
        else if(tokens[0][0]  ==  '@'               )
            op = new TagInfoOp(&trace, tokens, comment, lineNum);
        else if(strcmp(tokens[0], "0") == 0)
            op = new BusMemOp(&trace, tokens, BusMemOp::WRITE, comment, lineNum);
        else if(strcmp(tokens[0], "1") == 0)
            op = new Mask32ReadOp(&trace, tokens, comment, lineNum);
        else
            op = new TraceOp(&trace, tokens, comment, lineNum);
        if (op)
        {
            RC rc = trace.AddTraceOp(op, isOptionalSection);
            if (rc != OK)
            {
                delete[] buf;
                return rc;
            }
        }
        delete[] buf;
    }

    fh->FileClose();
    Sys::ReleaseFileIO(fh);
    fh = NULL;

    return OK;
}
}

JS_CLASS(Replayer);

SObject Replayer_Object
(
    "Replayer",
    ReplayerClass,
    0,
    0,
    "Replay Tests."
);
C_(Replayer_Run);
static STest Replayer_Run
(
    Replayer_Object,
    "Run",
    C_Replayer_Run,
    0,
    "Run replayer test."
);
C_(Replayer_Run)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char   *usage = "Usage: Mdiag.Run(ArgDatabase)\n";
    JavaScriptPtr pJavaScript;
    JSObject     *jsArgDb;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsArgDb)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    ArgDatabase *pArgDb = JS_GET_PRIVATE(ArgDatabase, pContext,
            jsArgDb, "ArgDatabase");
    if (pArgDb == NULL)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RETURN_RC(Replayer::Run(pArgDb));
}
#else
namespace Replayer
{
    // sim platform depends on this function;
    // Replayer implementation depends on mdiag;
    // This dummy implementation is for the case mdiag is missing.
    bool IsReplayingTrace()
    {
        return false;
    }
}
#endif // INCLUDE_MDIAG

