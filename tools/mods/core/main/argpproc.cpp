
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/argpproc.h"
#include "core/include/argdb.h"
#include "core/include/gpu.h"
#include "core/include/utility.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/fileholder.h"

#include <errno.h>

// $Id: $

//--------------------------------------------------------------------
//!
//! \file argpproc.cpp
//! \brief Create an argument database preprocessor
//!
//! There are several shorthand notations that may be used for colwenience
//! on the command line.  The preprocessor expands the shorthand arguments
//! See the comment above the class for details.
//!
//--------------------------------------------------------------------

bool ArgPreProcessor::s_bDevScopeShortlwt = false;
string ArgPreProcessor::s_TestArg = "";
string ArgPreProcessor::s_TestFileArg = "";
set<string> ArgPreProcessor::s_OptionalArgs;

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
ArgPreProcessor::ArgPreProcessor() :
    m_LwrrentScope(SCOPE_NONE),
    m_LwrrentScopeName(""),
    m_bInQuotedBlock(false),
    m_bInTestQuotedBlock(false),
    m_ErrorString(""),
    m_TestErrTok(-1),
    m_TestTokenCount(0),
    m_LastDevScope(Gpu::MaxNumDevices)
{
}

//-----------------------------------------------------------------------------
//! \brief Preprocess the arguments into a valid format
//!
//! \param pArgArray : Pointer to a vector of strings that contains the
//!                    arguments to be processed
//!
//! \return OK if preprocessing succeeded and the Args are valid
//!
RC ArgPreProcessor::PreprocessArgs(vector<string> *pArgArray)
{
    RC rc;
    UINT32 i;
    string argLine = "";
    string arg, scopeName;
    UINT32 testQuotedIndex = 0;

    // Process each argument individually
    for (i = 0; i < pArgArray->size(); i++)
    {
        arg = (*pArgArray)[i];

        // If the argument is scoping or a test argument
        // then extract the scope (unless within a quoted block in which case
        // the quoting takes priority over all other operations)
        if (!m_bInQuotedBlock && IsScopeOrTestArg(arg))
        {
            i++;
            if (i >= pArgArray->size())
            {
                m_ErrorString = "Scope name not found";
                rc = RC::BAD_COMMAND_LINE_ARGUMENT;
                break;
            }

            scopeName = (*pArgArray)[i];
        }

        rc = ValidateArg(arg);
        if (rc != OK)
            break;

        if (arg == "+begin")
        {
            m_bInQuotedBlock = true;
            argLine = "";
        }
        else if (arg == "+end")
        {
            // If the +begin/+end was empty it is also an error
            if (argLine == "")
            {
                m_ErrorString = "Empty +begin/+end section found";
                rc = RC::BAD_COMMAND_LINE_ARGUMENT;
                break;
            }

            m_bInQuotedBlock = false;
            if (m_bInTestQuotedBlock)
            {
                rc = ParseTest(argLine);
                if (rc != OK)
                {
                    // Point i at the correct index where the error oclwrred
                    // so that it will get flagged correctly
                    i = testQuotedIndex + m_TestErrTok;
                    break;
                }
                m_bInTestQuotedBlock = false;
            }
            else
            {
                m_PreProcessedArgs.push_back(argLine);
            }
        }
        else if (m_bInQuotedBlock)
        {
            argLine += arg;
            argLine += " ";
        }
        else if ((s_bDevScopeShortlwt && (arg == "-dev")) ||
                 (arg == "+BEGINDEV"))
        {
            rc = StartScope(scopeName, SCOPE_DEVICE);
            if (rc != OK)
                break;

            if (scopeName != "all")
            {
                // No need to verify that strtoul() returns a valid value since
                // this was done in StartScope()
                UINT32 devScope = strtoul(scopeName.c_str(), 0, 0);

                // Remember the last oclwrence of the "-dev" argument so that
                // the correct device state can be set when the command line
                // is processed.
                if (arg == "-dev")
                    m_LastDevScope = devScope;

                m_DevScopes.insert(devScope);
            }
        }
        else if ((s_TestFileArg != "") && (arg == s_TestFileArg))
        {
            if (s_TestArg != "")
            {
                rc = ParseTestFile(scopeName);
                if(rc != OK)
                    break;
            }
            else
            {
                m_ErrorString = "s_TestFileArg found but s_TestArg is not set";
                rc = RC::BAD_COMMAND_LINE_ARGUMENT;
                break;
            }
        }
        else if ((s_TestArg != "") && (arg == s_TestArg))
        {
            // If the test string starts a quoted block, then the test needs
            // to be handled differently (i.e. all the arguments between +begin
            // and +end grouped together and submitted as the test args)
            if (scopeName == "+begin")
            {
                m_bInTestQuotedBlock = true;
                m_bInQuotedBlock = true;
                testQuotedIndex = i;
                argLine = "";
            }
            else
            {
                rc = ParseTest(scopeName);
                if (rc != OK)
                    break;
            }
        }
        else if (arg == "+BEGINTEST")
        {
            if (s_TestArg != "")
            {
                rc = StartScope(scopeName, SCOPE_TEST);
                if (rc != OK)
                    break;
            }
            else
            {
                m_ErrorString = "+BEGINTEST found but s_TestArg is not set";
                rc = RC::BAD_COMMAND_LINE_ARGUMENT;
                break;
            }
        }
        else if ((arg == "+ENDTEST") || (arg == "+ENDDEV"))
        {
            rc = TerminateScope(scopeName);
            if (rc != OK)
                break;
        }
        else
        {
            m_PreProcessedArgs.push_back(arg);
        }
    }

    // Catch breaks from loop
    if (rc != OK)
    {
        ReportError(pArgArray, i);
        return rc;
    }

    if (m_bInQuotedBlock)
    {
         m_ErrorString = "+begin found without corresponding +end";
         ReportError(pArgArray, i);
         return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_LwrrentScope != SCOPE_NONE)
    {
        rc = TerminateScope(m_LwrrentScopeName);
        if (rc != OK)
        {
            ReportError(pArgArray, i);
            return rc;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Add the preprocessed arguments to the argument database
//!
//! \param pArgDatabase : Pointer to the argument database to add arguments
//!
//! \return OK if adding arguments succeeded
//!
RC ArgPreProcessor::AddArgsToDatabase(ArgDatabase *pArgDatabase)
{
    vector<string>::iterator strIter = m_PreProcessedArgs.begin();
    string argLine;
    RC     rc;

    m_LwrrentScopeName = "";
    m_LwrrentScope = SCOPE_NONE;

    while(strIter != m_PreProcessedArgs.end())
    {
        if (*strIter == "+BEGINDEV")
        {
            // Create the string for the device scope
            strIter++;
            m_LwrrentScopeName = "dev" + *strIter;
        }
        else if (*strIter == "+ENDDEV")
        {
            // Skip past the device scope name at the end of the device scope
            strIter++;
            m_LwrrentScopeName = "";
        }
        else if (*strIter == "+BEGINTEST")
        {
            // Tests are handled slightly differently than devices.  The format
            // for a test line in the argument database is s_TestArg followed by
            // "testname [testargs]" as a single quoted argument
            strIter++;
            m_LwrrentScope = SCOPE_TEST;
            CHECK_RC(pArgDatabase->AddSingleArg(s_TestArg.c_str()));
            argLine = *strIter;
        }
        else if (*strIter == "+ENDTEST")
        {
            // Skip past the device scope name at the end of the test and add
            // the quoted test string to the database
            strIter++;
            CHECK_RC(pArgDatabase->AddSingleArg(argLine.c_str()));
            m_LwrrentScope = SCOPE_NONE;
        }
        else if (m_LwrrentScope == SCOPE_TEST)
        {
            // When in a test scope since the entire test is added as a single
            // argument, it is necessary to restore +begin/end sections
            argLine += " ";
            if (strIter->find_first_of(' ') != string::npos)
            {
                argLine += "+begin ";
                argLine += *strIter;
                argLine += " +end";
            }
            else
            {
                argLine += *strIter;
            }
        }
        else
        {
            // Otherwise add the argument to the current sceop
            UINT32 flags = 0; // 0 is the default value
            if (s_OptionalArgs.count(*strIter) > 0)
            {
                flags = ArgDatabase::ARG_OPTIONAL;
            }
            CHECK_RC(pArgDatabase->AddSingleArg(strIter->c_str(),
                                       (m_LwrrentScopeName == "") ?
                                           NULL : m_LwrrentScopeName.c_str(),
                                       flags));
        }

        strIter++;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the list of all device scopes specified on the command line
//!
//! \return The first device scope string, or Gpu::MaxNumDevices if there are
//!         no device scopes
//!
UINT32 ArgPreProcessor::GetFirstDevScope()
{
    if (!m_DevScopes.empty())
        return *(m_DevScopes.begin());

    return Gpu::MaxNumDevices;
}

//-----------------------------------------------------------------------------
//! \brief Get the list of all device scopes specified on the command line
//!
//! \param prev : The previous device scope
//!
//! \return The next device scope string, or Gpu::MaxNumDevices if there are no
//!         more device scopes
//!
UINT32 ArgPreProcessor::GetNextDevScope(UINT32 prev)
{
    set<UINT32>::iterator devIter = m_DevScopes.find(prev);

    if (devIter != m_DevScopes.end())
        devIter++;

    if (devIter != m_DevScopes.end())
        return *devIter;

    return Gpu::MaxNumDevices;
}

//-----------------------------------------------------------------------------
//! \brief Print out the preprocessed arguments
//!
void ArgPreProcessor::PrintArgs()
{
    vector<string>::iterator strIter = m_PreProcessedArgs.begin();

    Printf(Tee::PriDebug, "Preprocessed Arguments : ");
    while(strIter != m_PreProcessedArgs.end())
    {
        if (strIter->find(' ', 0) != string::npos)
            Printf(Tee::PriDebug, "\"%s\" ", strIter->c_str());
        else
            Printf(Tee::PriDebug, "%s ",strIter->c_str());
        strIter++;
    }
    Printf(Tee::PriHigh, "\n");
}

//-----------------------------------------------------------------------------
//! \brief Enable/disable the use of "-dev" as a shortlwt
//!
//! \param bEnable : true to enable shortlwt, false to disable
//!
//! \return OK
//!
/* static */ RC ArgPreProcessor::SetDevScopeShortlwt(bool bEnable)
{
    s_bDevScopeShortlwt = bEnable;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Set the string to be used for a test argument
//!
//! \param testArg : string to be used as a test argument
//!
//! \return OK
//!
/* static */ RC ArgPreProcessor::SetTestArg(const string& testArg)
{
    s_TestArg = testArg;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Set the string to be used for a test file argument
//!
//! \param testArg : string to be used as a test file argument
//!
//! \return OK
//!
/* static */ RC ArgPreProcessor::SetTestFileArg(const string& testFileArg)
{
    s_TestFileArg = testFileArg;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Add the string name of optional argument
//!
//! \param optionalArg : string name of optional argument
//!
//! \return OK
//!
/* static */ RC ArgPreProcessor::AddOptionalArg(const string& optArg)
{
    s_OptionalArgs.insert(optArg);
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Check whether an argument is a scoping argument
//!
//! \param arg : A string containing argument to check
//!
//! \return true if the argument either starts or ends a scope
//!
bool ArgPreProcessor::IsScopeOrTestArg(const string& arg)
{
    return ((s_bDevScopeShortlwt && (arg == "-dev")) ||
            ((s_TestArg != "") && (arg == s_TestArg)) ||
            ((s_TestFileArg != "") && (arg == s_TestFileArg)) ||
            (arg == "+BEGINDEV")  || (arg == "+ENDDEV") ||
            (arg == "+BEGINTEST")  || (arg == "+ENDTEST"));
}

//-----------------------------------------------------------------------------
//! \brief Validate whether the provided argument is a legal argument
//!        (i.e. enforce all scoping rules)
//!
//! \param arg : A string containing argument to validate
//!
//! \return OK if the argument is valid, RC::BAD_COMMAND_LINE_ARGUMENT
//!         otherwise
//!
RC ArgPreProcessor::ValidateArg(const string& arg)
{
    // Enforce rule 2 : +begin/+end sections may not be nested within another
    //                  +begin/+end section
    if (m_bInQuotedBlock)
    {
        if (arg == "+begin")
        {
            m_ErrorString = "+begin found within +begin/+end section";
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else
        {
            // Skip all other checks within a quoted block (giving quoting
            // priority over all other operations)
            return OK;
        }
    }

    // Enforce rule 1 : No nesting of test or device scopes is permitted
    if (((arg == "+BEGINDEV") || (arg == "+BEGINTEST")) &&
        (m_LwrrentScope != SCOPE_NONE))
    {
        m_ErrorString = arg + " section found within ";
        m_ErrorString += (m_LwrrentScope == SCOPE_TEST) ? "test" : "device";
        m_ErrorString += " scope (" + m_LwrrentScopeName + ")";
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Ensure that if ending a scope, that the preprocessor is actually within
    // a scope
    if ((arg == "+end") ||
        ((arg == "+ENDDEV") && (m_LwrrentScope != SCOPE_DEVICE)) ||
        ((arg == "+ENDTEST") && (m_LwrrentScope != SCOPE_TEST)))
    {
        m_ErrorString = arg + " found without corresponding begin";
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Start an new scope (terminates any existing scope as well)
//!
//! \param scopeName : The name of the new scope
//! \param newScope  : The type of the new scope
//!
//! \return OK if starting the new scope succeeded,
//!         RC::BAD_COMMAND_LINE_ARGUMENT otherwise
//!
RC ArgPreProcessor::StartScope(const string& scopeName, ArgScope newScope)
{
    RC rc;

    // Scope names must not also be a scope argument
    if (IsScopeOrTestArg(scopeName) || (scopeName == "+begin") ||
        (scopeName == "+end"))
    {
        m_ErrorString = "Invalid ";
        m_ErrorString += (newScope == SCOPE_TEST) ? "test" : "device";
        m_ErrorString += " scope " + scopeName;
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // If starting a device scope, validate that the scope name is either
    // 'all' or a number between 0 and Gpu::MaxNumDevices indicating the
    // device instance
    if ((newScope == SCOPE_DEVICE) && (scopeName != "all"))
    {
        errno = 0;

        UINT32 devInst = strtoul(scopeName.c_str(), 0, 0);

        if ((errno != 0) || (devInst >= Gpu::MaxNumDevices))
        {
            m_ErrorString = "Invalid device " + scopeName +
                            " for device scope";
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    // For device scopes it is only necessary to terminate the current scope
    // if changing to a test scope or if the device scope name has changed.
    // For test scopes it is always necessary to terminate the current scope
    // since multiple tests with the same scopeName (i.e. test name) may be
    // specified on a single command line.
    if (((m_LwrrentScope == SCOPE_DEVICE) &&
         ((newScope == SCOPE_TEST) || (m_LwrrentScopeName != scopeName))) ||
        (m_LwrrentScope == SCOPE_TEST))
    {
        CHECK_RC(TerminateScope(m_LwrrentScopeName));
    }

    // Only start the new scope when moving to a new scope
    if ((newScope != SCOPE_DEVICE) ||
        ((scopeName != "all") && (scopeName != m_LwrrentScopeName)))
    {
        m_LwrrentScopeName = scopeName;
        m_PreProcessedArgs.push_back((newScope == SCOPE_DEVICE) ?
                                        string("+BEGINDEV") :
                                        string("+BEGINTEST"));
        m_PreProcessedArgs.push_back(scopeName);
        m_LwrrentScope = newScope;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Terminate an existing scope
//!
//! \param scopeName : The name of the scope to terminate
//!
//! \return OK if terminating the scope succeeded,
//!         RC::BAD_COMMAND_LINE_ARGUMENT otherwise
//!
RC ArgPreProcessor::TerminateScope(const string& scopeName)
{
    // Must only terminate a scope if lwrrently processing a scope
    if (m_LwrrentScope == SCOPE_NONE)
    {
        m_ErrorString = "Scope terminated when not processing a scope";
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    // Must not terminate a scope within a quoted block
    if (m_bInQuotedBlock)
    {
        m_ErrorString = (m_LwrrentScope == SCOPE_TEST) ? "Test" : "Device";
        m_ErrorString += " scope (" + m_LwrrentScopeName + ") ";
        m_ErrorString += "terminated within +begin/+end section";
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Must only terminate the current scope
    if (scopeName != m_LwrrentScopeName)
    {
        m_ErrorString = (m_LwrrentScope == SCOPE_TEST) ? "Test" : "Device";
        m_ErrorString += " scope (" + m_LwrrentScopeName + ") ";
        m_ErrorString += "terminated with incorrect scope name " + scopeName;
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Push in the appropriate scope terminator
    if (m_LwrrentScope == SCOPE_DEVICE)
    {
        m_PreProcessedArgs.push_back(string("+ENDDEV"));
    }
    else if (m_LwrrentScope == SCOPE_TEST)
    {
        m_PreProcessedArgs.push_back(string("+ENDTEST"));
    }

    // Push the terminated scope and update the scope variables
    m_PreProcessedArgs.push_back(m_LwrrentScopeName);
    m_LwrrentScope = SCOPE_NONE;
    m_LwrrentScopeName = "";

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Parse a test file into the command line
//!
//! \param filename : The filename to read
//!
//! \return OK if processing the test file succeeded
//!
RC ArgPreProcessor::ParseTestFile(const string& filename)
{
    FileHolder file;
    long    fileSize = 0;
    RC      rc;
    UINT32  lwrTokenNum = 0;
    vector<const char *> fileTokens;
    INT32   errLineNum = -1;
    UINT32  tokensProcessed = 0;

    // Clear the error position string.  If this string is not empty then
    // when the error is reported, it will be reported as a test file error
    m_TestFileErrPos = "";

    if (OK != (rc = file.Open(filename, "rb")))
    {
        m_ErrorString = "Unable to open file " + filename;
        return rc;
    }

    if (OK != Utility::FileSize(file.GetFile(), &fileSize))
    {
        m_ErrorString = "Unable to determine size of file " + filename;
        CHECK_RC(RC::CANNOT_OPEN_FILE);
    }

    if (fileSize == 0)
    {
        Printf(Tee::PriHigh, "Test file %s is empty\n", filename.c_str());
        return OK;
    }

    vector<char> fileData(fileSize + 1, 0);

    if (1 != fread(&fileData[0], fileSize, 1, file.GetFile()))
    {
        m_ErrorString = "Cannot read test file " + filename;
        CHECK_RC(RC::FILE_READ_ERROR);
    }

    Utility::TokenizeBuffer(&fileData[0], fileSize, &fileTokens, true);

    // Keep track of the line number for reporting errors (start counting
    // at 1)
    errLineNum = 1;
    for (lwrTokenNum = 0; lwrTokenNum < fileTokens.size();
         errLineNum++, lwrTokenNum += tokensProcessed)
    {
        // If there was an error and it oclwrred when processing the test file
        rc = AddTest(fileTokens, lwrTokenNum, &tokensProcessed);
        if (rc != OK)
        {
            // Construct the error string that shows where the error oclwrred in
            // the test file
            m_TestFileErrPos = "               when processing " + filename + " ";
            if (m_TestErrTok != -1)
            {
                m_TestFileErrPos +=
                    Utility::StrPrintf("at line %d, argument %d of %d",
                                       errLineNum, m_TestErrTok, m_TestTokenCount);
            }
            else
            {
                m_TestFileErrPos += Utility::StrPrintf("at end of line %d",
                                                       errLineNum);
            }
            return rc;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Parse a test into the command line
//!
//! \param teststring : The teststring to read
//!
//! \return OK if processing the test succeeded
//!
RC ArgPreProcessor::ParseTest(const string& testString)
{
    UINT32 tokensProcessed;

    vector<const char *> testTokens;
    vector<char> tokenBuffer(testString.size() + 1, 0);
    testString.copy(&tokenBuffer[0], testString.size());

    Utility::TokenizeBuffer(&tokenBuffer[0], (UINT32)testString.size(), &testTokens, false);

    return AddTest(testTokens, 0, &tokensProcessed);
}

//-----------------------------------------------------------------------------
//! \brief Add a test to the command line
//!
//! \param tokens           : The tokenized test line
//! \param startToken       : The starting token within the line for the test
//!                           arguments
//! \param pTokensProcessed : The number of tokens (beginning at startToken)
//!                           that were processed before the error oclwrred
//!
//! \return OK if successful, not OK otherwise
//!
RC ArgPreProcessor::AddTest
(
    vector<const char *> &tokens,
    UINT32                startToken,
    UINT32               *pTokensProcessed
)
{
    string arg;
    string argLine;
    UINT32 lwrToken = startToken;
    RC     rc;

    // Initialize the error token number and error line string
    m_TestErrLine = "";
    m_TestErrTok = -1;

    *pTokensProcessed = 0;

    if (tokens.empty())
    {
        m_ErrorString = "No test name found";
        CreateTestErrorLine(tokens, startToken, *pTokensProcessed);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // The test name may not be a scope or quoting argument
    if (IsScopeOrTestArg(tokens[lwrToken]) || !strcmp(tokens[lwrToken], "+begin") ||
        !strcmp(tokens[lwrToken], "+end"))
    {
        m_ErrorString = "Invalid test name " + string(tokens[lwrToken]);
        CreateTestErrorLine(tokens, startToken, *pTokensProcessed);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    m_bInQuotedBlock = false;

    // Start the test scope
    rc = StartScope(tokens[lwrToken], SCOPE_TEST);
    if (rc != OK)
    {
        CreateTestErrorLine(tokens, startToken, *pTokensProcessed);
        return rc;
    }

    (*pTokensProcessed)++;
    lwrToken++;

    while((lwrToken < tokens.size()) &&
          !Utility::IsNewLineToken(tokens[lwrToken]))
    {
        arg = tokens[lwrToken];

        // Keep support for +begin/+end within test files
        if (arg == "+begin")
        {
            if (m_bInQuotedBlock)
            {
                m_ErrorString = "Nested +begin/+end section found";
                CreateTestErrorLine(tokens, startToken, *pTokensProcessed);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            m_bInQuotedBlock = true;
            argLine = "";
        }
        else if (arg == "+end")
        {
            if (!m_bInQuotedBlock)
            {
                m_ErrorString = "+end found without +begin";
                CreateTestErrorLine(tokens, startToken, *pTokensProcessed);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            m_bInQuotedBlock = false;
            m_PreProcessedArgs.push_back(argLine);
        }
        else if (m_bInQuotedBlock)
        {
            argLine += arg;
            argLine += " ";
        }
        // When processing a file, the only scoping arguments
        // that are supported are +begin/+end, anything else
        // would violate scope nesting rules
        else if (IsScopeOrTestArg(arg))
        {
            m_ErrorString = "Invalid scoping argument " + arg;
            CreateTestErrorLine(tokens, startToken, *pTokensProcessed);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else
        {
            m_PreProcessedArgs.push_back(arg);
        }

        lwrToken++;
        (*pTokensProcessed)++;
    }

    // Terminate the test scope
    rc = TerminateScope(m_LwrrentScopeName);
    if (rc != OK)
    {
        CreateTestErrorLine(tokens, startToken, *pTokensProcessed);
        return rc;
    }

    // Flag the newline token as processed
    (*pTokensProcessed)++;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Create the error line for a test (i.e. mark off the argument within
//!        the test line that causes an error
//!
//! \param tokens          : The tokenized test line
//! \param startToken      : The starting token within the line for the test
//!                          arguments
//! \param tokensProcessed : The number of tokens (beginning at startToken)
//!                          that were processed before the error oclwrred
//!
void ArgPreProcessor::CreateTestErrorLine
(
    vector<const char *> &tokens,
    UINT32                startToken,
    UINT32                tokensProcessed
)
{
    UINT32 lwrToken;

    // Initialize the error token number and token count
    m_TestErrTok     = tokensProcessed + 1;
    m_TestTokenCount = m_TestErrTok;

    // Add the test line up to where the error oclwrred to the error string
    for (lwrToken = startToken;
          lwrToken < (startToken + tokensProcessed); lwrToken++)
    {
        m_TestErrLine += string(tokens[lwrToken]) ;
        if (lwrToken != (startToken + tokensProcessed - 1))
            m_TestErrLine += " ";
    }

    // Mark off the offending token with ">>>  <<<" if the error did not
    // occur at the end of a line
    if (!Utility::IsNewLineToken(tokens[lwrToken]))
    {
        if (lwrToken != startToken)
            m_TestErrLine += " ";
        m_TestErrLine += ">>>";
        m_TestErrLine += string(tokens[lwrToken++]) + "<<<";
        while (!Utility::IsNewLineToken(tokens[lwrToken]))
        {
            m_TestErrLine += " " + string(tokens[lwrToken++]);
            m_TestTokenCount++;
        }
    }
    else
    {
        m_TestErrTok = -1;
    }
}

//-----------------------------------------------------------------------------
//! \brief Report an error that oclwrred when processing the command line
//!
//! \param pArgArray : The command line
//! \param index     : Index where the error oclwrred on the command line
//!
void ArgPreProcessor::ReportError(vector<string> *pArgArray, UINT32 index)
{
    string arg;
    UINT32 startMarker = index;

    if (m_TestFileErrPos != "")
    {
        Printf(Tee::PriHigh, "Syntax error : %s\n", m_ErrorString.c_str());
        Printf(Tee::PriHigh, "%s\n", m_TestFileErrPos.c_str());
        Printf(Tee::PriHigh, "    %s\n", m_TestErrLine.c_str());
        return;
    }

    if (index < pArgArray->size())
    {
        if ((index != 0) && IsScopeOrTestArg((*pArgArray)[index - 1]) &&
            (((*pArgArray)[index - 1] != s_TestArg) || (m_TestErrLine == "")))
            startMarker = index - 1;

        Printf(Tee::PriHigh, "Syntax error : %s %s argument %d of %d\n",
                  m_ErrorString.c_str(), (m_TestErrLine == "") ? "at" : "within",
                  index, (UINT32)pArgArray->size());
    }
    else
    {
        Printf(Tee::PriHigh, "Syntax error : %s at end of input\n", m_ErrorString.c_str());
    }

    Printf(Tee::PriHigh, "    ");
    for (UINT32 i = 0; i < pArgArray->size(); i++)
    {
        if ((i == startMarker) &&
            (m_TestErrLine != "") && !m_bInQuotedBlock)
        {
            Printf(Tee::PriHigh, "\"%s\" ", m_TestErrLine.c_str());
        }
        else
        {
            arg = (*pArgArray)[i];
            if (i == startMarker)
                Printf(Tee::PriHigh, ">>>");
            if (arg.find(' ', 0) != string::npos)
                Printf(Tee::PriHigh, "\"%s\"", arg.c_str());
            else
                Printf(Tee::PriHigh, "%s", arg.c_str());
            if (i == index)
                Printf(Tee::PriHigh, "<<< ");
            else
                Printf(Tee::PriHigh, " ");
        }
    }
    Printf(Tee::PriHigh, "\n");
}

//-----------------------------------------------------------------------------
//! \brief Javascript class for ArgPreProcessor
//!
JS_CLASS(ArgPreProcessor);

//-----------------------------------------------------------------------------
//! \brief Javascript object for ArgPreProcessor
//!
SObject ArgPreProcessor_Object
(
   "ArgPreProc",
   ArgPreProcessorClass,
   0,
   0,
   "Argument pre-processor control"
);

//-----------------------------------------------------------------------------
//! \brief Enable/disable special handling of "-dev" command line argument to
//!        specify a scope, when false "-dev" will be handled just like any
//!        other argument
PROP_READWRITE_NAMESPACE(ArgPreProcessor, DevScopeShortlwt, bool,
                         "Enable '-dev' shortlwt for specifying a device scope");

//-----------------------------------------------------------------------------
//! \brief Set the argument to use as the test argument, if not set, then no
//!        test scopes will be created and BEGINTEST / ENDTEST will be ignored
//!        since it is unclear how the tests should be added.
PROP_READWRITE_NAMESPACE(ArgPreProcessor, TestArg, string,
                         "Set the argument to process as a test argument");

//-----------------------------------------------------------------------------
//! \brief Set the argument to use as the test file argument.  Only valid if
//!        the TestArg has been set.  If not set, then no test files will be
//!        processed since the TestArg is used when processing test files
PROP_READWRITE_NAMESPACE(ArgPreProcessor, TestFileArg, string,
                         "Set the argument to process as a test file argument");

//-----------------------------------------------------------------------------
//! \brief Export function AddOptionalArg to let JS add optional args. MODS
//!        will not complain if optional args are unused in test.
JS_STEST_BIND_ONE_ARG_NAMESPACE(ArgPreProcessor, AddOptionalArg, string, optArg,
                                "Add an optional argument");
