/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mucc.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
using namespace Mucc;

// This file contains the main program for the Mucc assembler.
// See https://confluence.lwpu.com/display/GM/MuCC+Assembly+Language
//
namespace
{
    //! Set by -v flag to enable PriLow and PriDebug prints
    bool s_Verbose = false;

    //----------------------------------------------------------------
    //! \brief Used to parse command-line options from argv
    //!
    //! Check whether pArgv starts with the command-line option given
    //! in the opt arg.  If so, return true and remove the option from
    //! the start of *pArgv.  If not, return false and leave *pArgv
    //! unchanged.
    //!
    //! \param opt A command-line option to check for, such as "-p" or
    //!     "--foo".
    //! \param pOptArg If non-null, then the option takes an argument.
    //!     Store the argument in in *pOptArg on exit.
    //! \param pArgv The first time this function is called, pArgv
    //!     should point at the command-line args starting at argv[1].
    //!     One argument is stripped from the front each time this
    //!     function returns true.
    //! \param pError Used to return errors to the caller.  The EC
    //!     would normally be a return value, but for this function
    //!     it's easier to use it in an if-elseif-elseif statement and
    //!     check for errors later.
    //!
    bool GetOpt
    (
        const string&  opt,
        string*        pOptArg,
        deque<string>* pArgv,
        EC*            pError
    )
    {
        // Strip opt from the start of *pArgv, or return false
        //
        if (pArgv->front() == opt)
        {
            // Entire first arg matches opt
            //
            pArgv->pop_front();
        }
        else if (opt[1] != '-' && opt.size() == 2 &&
                 !pArgv->front().compare(0, 2, opt))
        {
            // opt is a one-letter flag like "-p", and the first
            // argument starts with it, but there are more letters.
            // If opt takes an arg, then strip the first two letters
            // so that the arg is now the first option (i.e. "-pfoo"
            // is treated like "-p foo").  If opt doesn't take an arg,
            // then remove the opt letter but leave the minus sign so
            // that several one-letter flags can be combined (i.e.
            // "-pqr" is treated like "-p -q -r").
            //
            if (pOptArg)
                pArgv->front().erase(0, 2);
            else
                pArgv->front().erase(1, 1);
        }
        else if (pOptArg && opt[1] == '-' &&
                 !pArgv->front().compare(0, opt.size() + 1, opt + "="))
        {
            // opt is a multi-letter flag like "--foo", and the
            // argument is given in the form "--foo=bar".  Treat that
            // like "--foo bar".
            //
            pArgv->front().erase(0, opt.size() + 1);
        }
        else
        {
            // The argument doesn't start with the option.
            //
            return false;
        }

        // If opt takes an argument, return it in *pOptArg
        //
        if (pOptArg)
        {
            if (pArgv->empty())
            {
                Printf(PriError,
                       "%s option requires an argument\n", opt.c_str());
                *pError = BAD_COMMAND_LINE_ARGUMENT;
                return false;
            }
            *pOptArg = move(pArgv->front());
            pArgv->pop_front();
        }
        return true;
    }
}

//--------------------------------------------------------------------
//! \brief Main program for mucc assembler
//!
//! This function is the bulk of the main program, except for some
//! LwDiagUtil-related setup and cleanup done by main()
//!
EC MuccMain(int argc, const char* argv[])
{
    const string myName = StripDirectory(argv[0]);
    const string usage =
        "usage: " + myName + " -l litter [options] assembly_file\n"
        "\n"
        "Options:\n"
        "    -D macro=[defn] ... - Define a macro\n"
        "    -I dir ...          - Add a directory to the include path\n"
        "    -l litter           - The litter to compile for (required); see below\n"
        "    --json filename     - Write the output as JSON; filename \"-\" is stdout\n"
        "    --simulate          - Simulate the mucc program\n"
        "    -v, --verbose       - Enable verbose prints\n"
        "    -W<warning>         - Enable the indicated warning; see below\n"
        "    -Wno<warning>       - Disable the indicated warning; see below\n"
        "    -Wall               - Enable all warnings except -Werror (default)\n"
        "    -Wnone              - Disable all warnings except -Werror\n"
        "    -h, --help          - Print a help message and exit\n"
        "\n"
        "Litters (case insensitive, used for -l flag):\n" +
        "    " + boost::algorithm::join(GetLitterNames(), "\n    ") + "\n"
        "\n"
        "Warnings (used in -W<warning> and -Wno<warning> flags):\n"
        "    midlabel - Warn if a label is in the middle of an instruction\n"
        "    nop      - Warn if the \"NOP\" opcode seems to be misused\n"
        "    octal    - Warn if an octal constant seems to be malformed\n"
        "    error    - Treat warnings as errors\n"
        "\n"
        "See here for the syntax of the assembly file:\n"
        "    https://confluence.lwpu.com/display/GM/MuCC+Assembly+Language";
    Preprocessor preprocessor;
    Launcher     launcher;
    string       jsonFile;
    bool         runSimulation = false;
    bool         parsedLitterOption = false;
    EC ec = OK;

    // Parse the command-line options
    //
    deque<string> args;
    for (int ii = 1; ii < argc; ++ii)
    {
        args.push_back(argv[ii]);
    }

    while (args.size() > 0 && args[0].size() >= 2 && args[0][0] == '-')
    {
        string optArg;
        if (GetOpt("--", nullptr, &args, &ec))
        {
            break;
        }
        else if (GetOpt("-D", &optArg, &args, &ec))
        {
            auto equalPos = optArg.find('=');
            if (equalPos == string::npos)
            {
                preprocessor.AddMacro(optArg.c_str(), "");
            }
            else
            {
                preprocessor.AddMacro(optArg.substr(0, equalPos).c_str(),
                                      optArg.substr(equalPos + 1).c_str());
            }
        }
        else if (GetOpt("-I", &optArg, &args, &ec))
        {
            preprocessor.AddSearchPath(optArg);
        }
        else if (GetOpt("-l", &optArg, &args, &ec))
        {
            AmapLitter litter = NameToLitter(optArg);
            if (litter == LITTER_ILWALID)
            {
                Printf(PriError, "Unknown litter %s; options are:\n",
                       optArg.c_str());
                for (const string& name: GetLitterNames())
                {
                    Printf(PriError, "    %s\n", name.c_str());
                }
                FIRST_EC(BAD_COMMAND_LINE_ARGUMENT);
            }
            if (parsedLitterOption)
            {
                Printf(PriError,
                       "Passing multiple -l options on command line\n");
                FIRST_EC(BAD_COMMAND_LINE_ARGUMENT);
            }
            parsedLitterOption = true;
            launcher.SetLitter(litter);
        }
        else if (GetOpt("--json", &optArg, &args, &ec))
        {
            if (jsonFile != "")
            {
                Printf(PriError,
                       "Passing multiple --json arguments on command line\n");
                FIRST_EC(BAD_COMMAND_LINE_ARGUMENT);
            }
            jsonFile = optArg;
        }
        else if (GetOpt("--simulate", nullptr, &args, &ec))
        {
            runSimulation = true;
        }
        else if (GetOpt("-v", nullptr, &args, &ec) ||
                 GetOpt("--verbose", nullptr, &args, &ec))
        {
            s_Verbose = true;
        }
        else if (GetOpt("-Wmidlabel", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_MIDLABEL, true);
        }
        else if (GetOpt("-Wnomidlabel", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_MIDLABEL, false);
        }
        else if (GetOpt("-Wnop", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_NOP, true);
        }
        else if (GetOpt("-Wnonop", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_NOP, false);
        }
        else if (GetOpt("-Woctal", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_OCTAL, true);
        }
        else if (GetOpt("-Wnooctal", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_OCTAL, false);
        }
        else if (GetOpt("-Werror", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::TREAT_WARNINGS_AS_ERRORS, true);
        }
        else if (GetOpt("-Wnoerror", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::TREAT_WARNINGS_AS_ERRORS, false);
        }
        else if (GetOpt("-Wall", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_MIDLABEL, true);
            launcher.SetFlag(Flag::WARN_NOP,      true);
            launcher.SetFlag(Flag::WARN_OCTAL,    true);
        }
        else if (GetOpt("-Wnone", nullptr, &args, &ec))
        {
            launcher.SetFlag(Flag::WARN_MIDLABEL, false);
            launcher.SetFlag(Flag::WARN_NOP,      false);
            launcher.SetFlag(Flag::WARN_OCTAL,    false);
        }
        else if (GetOpt("--help", nullptr, &args, &ec) ||
                 GetOpt("-h", nullptr, &args, &ec))
        {
            Printf(PriNormal, "%s\n", usage.c_str());
            return OK;
        }
        else
        {
            Printf(PriError,
                   "Unknown command-line option %s\n", args[0].c_str());
            args.clear();
            ec = BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    // Make sure the user passed a -l flag
    //
    if (!parsedLitterOption)
    {
        Printf(PriError, "Must use \"-l litter\" arg; options are:\n");
        for (const string& name: GetLitterNames())
        {
            Printf(PriError, "    %s\n", name.c_str());
        }
        FIRST_EC(BAD_COMMAND_LINE_ARGUMENT);
    }

    // Make sure the user passed a source file
    //
    string inputFile;
    switch (args.size())
    {
        case 0:
            Printf(PriError, "Missing input file\n");
            FIRST_EC(BAD_COMMAND_LINE_ARGUMENT);
            break;
        case 1:
            inputFile = args[0];
            break;
        default:
            Printf(PriError, "Too many command-line arguments\n");
            FIRST_EC(BAD_COMMAND_LINE_ARGUMENT);
            break;
    }

    // If everything's OK so far, pass the source file through the
    // preprocessor
    //
    if (ec == OK)
    {
        const string inputFileDir = StripFilename(inputFile.c_str());
        preprocessor.AddSearchPath(inputFileDir.empty() ? "." : inputFileDir);
        FIRST_EC(preprocessor.LoadFile(inputFile));
    }

    // Until now, we've been using FIRST_EC while reading the command
    // line, so that all errors can be printed.  But we're done now,
    // so exit if there were any errors.
    //
    if (ec != OK)
    {
        Printf(PriError, "Run '%s --help' for help.\n", myName.c_str());
        return ec;
    }

    // Pass the file to the assembler
    //
    unique_ptr<Program> pProgram;
    if (boost::algorithm::ends_with(inputFile, ".mucc"))
    {
        pProgram = launcher.AllocProgram();
        CHECK_EC(pProgram->Assemble(&preprocessor));
    }
    else
    {
        Printf(PriError, "File %s has unknown suffix; expected .mucc\n",
               inputFile.c_str());
        return BAD_COMMAND_LINE_ARGUMENT;
    }

    // Write assembler output to JSON if requested
    //
    if (!jsonFile.empty())
    {
        if (jsonFile == "-")
        {
            pProgram->PrintJson(stdout);
        }
        else
        {
            FileHolder fileHolder;
            CHECK_EC(fileHolder.Open(jsonFile, "w"));
            pProgram->PrintJson(fileHolder.GetFile());
        }
    }

    // Run simulation if requested
    //
    if (runSimulation)
    {
        CHECK_EC(pProgram->Simulate());
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Assert function passed to LwDiagUtils::Initialize()
//!
void MuccAssert
(
    const char* file,
    int         line,
    const char* function,
    const char* test
)
{
    Printf(PriError, "%s[%d]: %s: assert(%s) failed\n",
           file, line, function, test);
    fflush(nullptr);
    abort();
}

//--------------------------------------------------------------------
//! \brief Print function passed to LwDiagUtils::Initialize()
//!
//! This method prints most messages to stdout, but it prints PriWarn
//! and PriError to stderr and prepends "WARNING:" or "ERROR:" to them
//! respectively.  The prepend is removed if the format is just "\n",
//! so that we can print a blank line to stderr after a warning or
//! error.
//!
INT32 MuccVAPrintf
(
    INT32       priority,
    UINT32      moduleCode,
    UINT32      sps,
    const char* format,
    va_list     args
)
{
    INT32 printSize = 0;
    switch (priority)
    {
        case PriNormal:
        case PriHigh:
        case PriAlways:
        case ScreenOnly:
            printSize += vprintf(format, args);
            break;
        case PriWarn:
            if (strcmp(format, "\n") != 0)
            {
                printSize += fprintf(stderr, "WARNING: ");
            }
            printSize += vfprintf(stderr, format, args);
            break;
        case PriError:
            if (strcmp(format, "\n") != 0)
            {
                printSize += fprintf(stderr, "ERROR: ");
            }
            printSize += vfprintf(stderr, format, args);
            break;
        case PriDebug:
        case PriLow:
            printSize += (s_Verbose ? vprintf(format, args) : 0);
            break;
        default:
            break;
    }
    return printSize;
}

//--------------------------------------------------------------------
//! \brief Main program: Initialize LwDiagUtils and call MuccMain()
//!
int main(int argc, const char *argv[])
{
    LwDiagUtils::Initialize(MuccAssert, MuccVAPrintf);
    const EC ec = MuccMain(argc, argv);
    LwDiagUtils::Shutdown();
    return ec;
}
