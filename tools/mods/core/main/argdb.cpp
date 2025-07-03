/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2013,2016,2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/cmdline.h"
#include "core/include/argpproc.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/fileholder.h"

#include <vector>
#include <string>
#include <string.h>
#include <ctype.h>

// $Id: $

//--------------------------------------------------------------------
//!
//! \file argdb.cpp
//! \brief Create an argument database for processing command line arguments
//!
//! Each command line argument is added to the database as a seperate entry
//! with a particular scope.  The database tracks the usage of each argument
//! (i.e. how many times a parser has parsed an argument)
//!
//--------------------------------------------------------------------

//--------------------------------------------------------------------
//! \brief Constructor - creates an initially empty database
//!
ArgDatabase::ArgDatabase() :
    m_pHead(NULL),
    m_ppTail(&m_pHead),
    m_pParentDb(NULL),
    m_RefCount(0),
    m_pJsArgDatabaseObj(NULL)
{
}

//--------------------------------------------------------------------
//! \brief Constructor - Create a database that references another
//!
//! \param pParentDb :  Pointer to the parent database to allow chaining
//!                     databases for easy removal of a block of arguments
//!
ArgDatabase::ArgDatabase(ArgDatabase *pParentDb) :
    m_pHead(NULL),
    m_ppTail(&m_pHead),
    m_pParentDb(pParentDb),
    m_RefCount(0),
    m_pJsArgDatabaseObj(NULL)
{
    m_pParentDb->m_RefCount++;
}

//--------------------------------------------------------------------
//! \brief Destructor - Delete the database and any allocated data
//!
ArgDatabase::~ArgDatabase()
{
    if (m_RefCount > 0)
    {
        Printf(Tee::PriHigh,
               "ArgDatabase (%p) being destroyed while reference count (%d) is nonzero!\n",
               this, m_RefCount);
    }

    ClearArgs();

    if (m_pParentDb) m_pParentDb->m_RefCount--;
}

//-----------------------------------------------------------------------------
//! \brief Clear all arguments from the database
//!
void ArgDatabase::ClearArgs()
{
    struct ArgDatabaseEntry *pOldHead;

    while (m_pHead)
    {
        pOldHead = m_pHead;
        m_pHead = m_pHead->pNext;

        DeleteEntry(pOldHead);
    }

    m_ppTail = &m_pHead;
}

//-----------------------------------------------------------------------------
//! \brief Add a single arg, with an optional scope qualifier to the database
//!
//! \param pArg   : The arg to add to the database.  Memory is allocated for the
//!                 argument and the data is copied into the database
//! \param pScope : Optional scope for the argument.  If a scope is specified
//!                 then the argument will only be visible when that scope is
//!                 parsed
//! \param flags  : Flags for the argumnent (MUST_USE, OPTIONAL)
//!
//! \return OK if the adding the argument succeeds, not OK otherwise
//!
RC ArgDatabase::AddSingleArg(const char *pArg,
                             const char *pScope /*= NULL*/,
                             UINT32      flags /*= 0*/)
{
    *m_ppTail = new ArgDatabaseEntry;
    if (*m_ppTail == NULL)
        return RC::CANNOT_ALLOCATE_MEMORY;

    (*m_ppTail)->pArg        = Utility::StrDup(pArg);
    (*m_ppTail)->pScope      = Utility::StrDup(pScope);
    (*m_ppTail)->flags      = flags;
    (*m_ppTail)->usageCount = 0;

    (*m_ppTail)->pNext = 0;
    m_ppTail = &((*m_ppTail)->pNext);

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Add an entire arg line, with an optional scope qualifier at the
//!        start of the line
//!
//! \param pLine  : The line of arguments to add to the database.
//! \param flags  : Flags for all argumnents on the line (MUST_USE, OPTIONAL)
//!
//! \return OK if the adding the line succeeds, not OK otherwise
//!
RC ArgDatabase::AddArgLine(const char *pLine, UINT32 flags /*= 0*/)
{
    const char          *pDoubleColon;
    const char          *pScopeStart = NULL;
    UINT32               scopeLen = 0;
    vector<const char *> lineTokens;
    UINT32               lwrToken;
    string               arg;
    bool                 bFirstToken;
    RC                   rc;

    // first, see if there's a double colon - that marks the scope specifier
    pDoubleColon = strstr(pLine, "::");
    if (pDoubleColon)
    {
        // The start of the scope is the first non white-space character on the
        // line
        pScopeStart = pLine;
        while (isspace(*pScopeStart)) pScopeStart++;

        // If the line did not start with just "::" and nothing after it
        if(pScopeStart < pDoubleColon)
        {
            // Setup the scope removing any whitespace that oclwrs between
            // <scope> and "::"
            scopeLen = (UINT32)(pDoubleColon - pScopeStart);
            while((scopeLen > 0) && isspace(pScopeStart[scopeLen-1])) scopeLen--;
        }

        if (scopeLen == 0)
            pScopeStart = NULL;

        // The actual arguments occur immediately after the scope
        pLine = pDoubleColon + 2;
    }

    // Create a copy of the line and then tokenize it in place
    vector<char> pLineCopy(strlen(pLine) + 1);

    strcpy(&pLineCopy[0], pLine);

    Utility::TokenizeBuffer(&pLineCopy[0], (UINT32)strlen(pLine),
                            &lineTokens, false);

    for (lwrToken = 0; lwrToken < lineTokens.size(); lwrToken++)
    {
        arg = "";

        // Handle a +begin/+end block within the line which has the effect of
        // adding everything between +begin/+end as a single argument
        if (!strcmp(lineTokens[lwrToken], "+begin"))
        {
            bool bEnd = false;

            bFirstToken = true;
            lwrToken++;
            while ((lwrToken < lineTokens.size()) && !bEnd)
            {
                if (!strcmp(lineTokens[lwrToken], "+end"))
                {
                    bEnd = true;
                }
                else if (!strcmp(lineTokens[lwrToken], "+begin"))
                {
                    Printf(Tee::PriHigh,
                           "%s : cannot nest +begin/+end blocks\n",
                           __FUNCTION__);
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
                else
                {
                    // Add a space between tokens
                    if (!bFirstToken)
                        arg += " ";
                    else
                        bFirstToken = false;
                    arg += lineTokens[lwrToken];
                    lwrToken++;
                }
            }

            if (!bEnd)
            {
                Printf(Tee::PriHigh, "%s : +begin/+end block not terminated\n",
                       __FUNCTION__);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
        else if (!strcmp(lineTokens[lwrToken], "+end"))
        {
            Printf(Tee::PriHigh, "%s : +end without corresponding +begin\n",
                   __FUNCTION__);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if((lineTokens[lwrToken][0] == '\"') || (lineTokens[lwrToken][0] == '\''))
        {
            // Keep support for quoted arguments.  Tokenizing will bread a quoted
            // argument on whitespace boundaries, recreate the original quoting
            // before adding the argument as a whole to the database
            char endQuotes = lineTokens[lwrToken][0];
            bFirstToken = true;
            if (strlen(lineTokens[lwrToken]) == 1)
            {
                // In case start quote is a single token, the condition
                // (lineTokens[lwrToken][strlen(lineTokens[lwrToken]) - 1] != endQuotes))
                // can't work to judge the end quote. So consume the start quote token
                // before the loop.
                arg += lineTokens[lwrToken];
                lwrToken++;
            }

            while ((lwrToken < lineTokens.size()) &&
                   (lineTokens[lwrToken][strlen(lineTokens[lwrToken]) - 1] != endQuotes))
            {
                // Add a space between tokens
                if (!bFirstToken)
                    arg += " ";
                else
                    bFirstToken = false;
                arg += lineTokens[lwrToken];
                lwrToken++;
            }

            if (lwrToken == lineTokens.size())
            {
                Printf(Tee::PriHigh, "%s : unterminated quoted block\n",
                       __FUNCTION__);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            // Add a space between tokens
            if (!bFirstToken)
                arg += " ";
            arg += lineTokens[lwrToken];
            arg = arg.substr(1, arg.size() - 2);
        }
        else
        {
            arg = lineTokens[lwrToken];
        }

        CHECK_RC(AddSingleArg(arg.c_str(), pScopeStart, flags));
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Add an entire file of arguments, each line is added separately and
//!        may have a scope specifier
//!
//! \param pFilename : The file of arguments to add to the database.
//! \param flags     : Flags for all argumnents in the file (MUST_USE, OPTIONAL)
//!
//! \return OK if the adding the file succeeds, not OK otherwise
//!
RC ArgDatabase::AddArgFile(const char *pFilename, UINT32 flags /*= 0*/)
{
    FileHolder file;
    long   fileSize = 0;
    string blockArg;
    RC     rc;
    vector<const char *> fileTokens;
    UINT32               lwrToken;

    CHECK_RC(file.Open(pFilename, "rb"));

    if (OK != Utility::FileSize(file.GetFile(), &fileSize))
    {
        Printf(Tee::PriHigh, "Unable to determine size of file %s\n",
               pFilename);
        CHECK_RC(RC::CANNOT_OPEN_FILE);
    }

    if (fileSize == 0)
    {
        Printf(Tee::PriHigh, "Test file %s is empty\n", pFilename);
        return OK;
    }

    vector<char> fileData(fileSize + 1, 0);

    if (1 != fread(&fileData[0], fileSize, 1, file.GetFile()))
    {
        Printf(Tee::PriHigh, "cannot read file %s\n", pFilename);
        CHECK_RC(RC::FILE_READ_ERROR);
    }

    // Tokenize the entire file.  File comments are not part of the
    // tokens
    Utility::TokenizeBuffer(&fileData[0], fileSize, &fileTokens, true);

    for (lwrToken = 0; lwrToken < fileTokens.size(); lwrToken++)
    {
        // Reconstruct each line from the tokens (without comments)
        blockArg = "";
        while (!Utility::IsNewLineToken(fileTokens[lwrToken]))
        {
            blockArg += fileTokens[lwrToken++];
        }

        // Add each line seperately
        if (OK != (rc = AddArgLine(blockArg.c_str(), flags)))
        {
            Printf(Tee::PriHigh, "Can't add argument line from file %s!\n", pFilename);
            Printf(Tee::PriHigh, "   Options=%s\n", blockArg.c_str());
            CHECK_RC(rc);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Add an entire file of tests, each line is added separately and
//!        interpreted as a seperate test.  The first word on each test line
//!        MUST be the name of the test
//!
//! \param pFilename : The file of tests to add to the database.
//! \param flags     : Flags for all tests in the file (MUST_USE, OPTIONAL)
//!
//! \return OK if the adding the file succeeds, not OK otherwise
//!
RC ArgDatabase::AddTestFile(const char *pFilename, UINT32 flags /*= 0*/)
{
    FileHolder testFile;
    char   line[1024];
    RC     rc;

    CHECK_RC(testFile.Open(pFilename, "r"));

    while(fgets(line, sizeof(line)-1, testFile.GetFile()))
    {
        string buffer;

        // Grab an entire line and append it to the buffer, not that the line
        // can end with either a '\n' or EOF
        while (line[strlen(line) - 1] != '\n')
        {
            buffer += line;
            if(fgets(line, sizeof(line)-1, testFile.GetFile()) == 0)
                break;
        }

        // Remove the trailing '\n'
        if (line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';
        buffer += line;

        // As long as the line is not a comment or an empty line, add the
        // entire line as a test (test argument being '-e')
        if ((buffer[0] != '#') && (buffer[0] != '\n'))
        {
            CHECK_RC(AddSingleArg("-e"));
            CHECK_RC(AddSingleArg(buffer.c_str()));
        }
    }

    return OK;
}

bool ArgDatabase::AreTestEscapeArgsPresent() const
{
    const struct ArgDatabaseEntry *pLwrrentEntry;
    pLwrrentEntry = m_pHead;
    while (pLwrrentEntry)
    {
        if ((pLwrrentEntry->usageCount != 0) &&
            (pLwrrentEntry->flags & ArgDatabase::ARG_TEST_ESCAPE))
        {
            return true;
        }
        pLwrrentEntry = pLwrrentEntry->pNext;
    }
    return false;
}

//-----------------------------------------------------------------------------
void ArgDatabase::PrintTestEscapeArgs() const
{
    const struct ArgDatabaseEntry *pLwrrentEntry;
    bool bTestEscapeArgFound = false;

    pLwrrentEntry = m_pHead;

    static const UINT32 widthRemaining = 61;
    string testEscapeStr = "*******************************************************************\n"
                           "*  THE FOLLOWING ARGUMENTS MAY CAUSE TEST ESCAPES                 *\n";
    while (pLwrrentEntry)
    {
        if ((pLwrrentEntry->usageCount != 0) &&
            (pLwrrentEntry->flags & ArgDatabase::ARG_TEST_ESCAPE))
        {
            bTestEscapeArgFound = true;
            testEscapeStr += Utility::StrPrintf("*    %s", pLwrrentEntry->pArg);
            testEscapeStr += string(widthRemaining - strlen(pLwrrentEntry->pArg), ' ');
            testEscapeStr += "*\n";
        }
        pLwrrentEntry = pLwrrentEntry->pNext;
    }
    if (bTestEscapeArgFound)
    {
        Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_LOW,
               "%s"
               "*******************************************************************\n",
               testEscapeStr.c_str());
    }
}

//-----------------------------------------------------------------------------
//! \brief Check the database for unused arguments (i.e. arguments that are
//!        not optional and have never been parsed by an ArgReader)
//!
//! \param bPrintUnusedArgs : true if a message should be printed for each
//!                           unused argument, false if it should just return
//!                           the count.
//! \return The number of unused arguments found
//!
UINT32 ArgDatabase::CheckForUnusedArgs(bool bPrintUnusedArgs) const
{
    const struct ArgDatabaseEntry *pLwrrentEntry;
    UINT32       warnCount;

    pLwrrentEntry = m_pHead;
    warnCount = 0;
    while(pLwrrentEntry)
    {
        if((pLwrrentEntry->usageCount == 0) &&
           !(pLwrrentEntry->flags & ArgDatabase::ARG_OPTIONAL))
        {
            if (bPrintUnusedArgs)
            {
                Printf(Tee::PriHigh, "argument '%s' not used!\n",
                       pLwrrentEntry->pArg);
            }
            warnCount++;
        }

        pLwrrentEntry = pLwrrentEntry->pNext;
    }

    return (warnCount);
}

//-----------------------------------------------------------------------------
//! \brief Copy all arguments into the destination buffer.  Doesn't handle all
//!        cases (like "", for example). If size == 0, return the cumulative 
//!        max size of the arguments. 
//!
//! \param pArgBuffer : Destination buffer for the arguments
//! \param size       : Size in bytes of the argument buffer
//! \return size of the argument list (possibly more if size == 0)
//!
UINT32 ArgDatabase::ListAllArgs(char *pArgBuffer, UINT32 size)
{
    UINT32 offset = 0;

    memset(pArgBuffer, 0, size);

    for (ArgDatabaseEntry *pLwrrentEntry = m_pHead; pLwrrentEntry != nullptr; 
            pLwrrentEntry = pLwrrentEntry->pNext)
    {
        UINT32 argLen = (UINT32)strlen(pLwrrentEntry->pArg);
        UINT32 scopeLen = 0;
        UINT32 maxArgLen = argLen + 2;  // Two " may be added if the arg
                                        // contains spaces

        // If the scope is present, the argument will be copied in with:
        // <scope>::<arg> into the buffer
        if (pLwrrentEntry->pScope)
        {
            // Get the scope length and update the maximum argument length
            // including the "::" that will be added
            scopeLen = (UINT32)strlen(pLwrrentEntry->pScope);
            maxArgLen += scopeLen + 2;
        }

        // Caller is requesting the total size of the argument list
        // Just add the maxArgLen to offset 
        if (size == 0)
        {
            offset += maxArgLen;
            continue;
        }

        // protect against overflow (include the space between arguments)
        if ((offset + maxArgLen + 1) >= size)
            return (offset + 1);

        // one space between each argument
        if (offset)
            pArgBuffer[offset++] = ' ';

        // Copy the scope into the buffer
        if (pLwrrentEntry->pScope)
        {
            strcpy(pArgBuffer + offset, pLwrrentEntry->pScope);
            offset += scopeLen;
            pArgBuffer[offset++] = ':';
            pArgBuffer[offset++] = ':';
        }

        // If the argument contains spaces, copy the argument into the buffer
        // with " to delineate it, otherwise put the argument in without "
        if (strchr(pLwrrentEntry->pArg, ' '))
        {
            pArgBuffer[offset++] = '\"';
            strcpy(pArgBuffer + offset, pLwrrentEntry->pArg);
            offset += argLen;
            pArgBuffer[offset++] = '\"';
        }
        else
        {
            strcpy(pArgBuffer + offset, pLwrrentEntry->pArg);
            offset += argLen;
        }
    }

    return (offset + 1);
}

//-----------------------------------------------------------------------------
//! \brief Remove a particular database entry
//!
//! \param pEntry : Pointer to entry to remove
//!
//! \return OK if successful, not OK otherwise
RC ArgDatabase::RemoveEntry(ArgDatabaseEntry *pEntry)
{
    // Handle deleting the head of the linked list as a special case
    if (m_pHead == pEntry)
    {
        m_pHead = m_pHead->pNext;
        if (m_pHead == nullptr)
            m_ppTail = &m_pHead;
        DeleteEntry(pEntry);
        return OK;
    }

    // Search for the entry immediately before the one we want to delete
    // since we are deleting from a singly linked list
    ArgDatabaseEntry *pLwrEntry = m_pHead;
    while (pLwrEntry && (pLwrEntry->pNext != pEntry))
    {
        pLwrEntry = pLwrEntry->pNext;
    }

    // Entry not found, return error
    if (pLwrEntry == nullptr)
        return RC::SOFTWARE_ERROR;

    // Link over the entry we are about to delete and point the tail
    // appropriately if the current entry then becomes the last entry in the
    // list
    pLwrEntry->pNext = pLwrEntry->pNext->pNext;
    if (pLwrEntry->pNext == nullptr)
        m_ppTail = &pLwrEntry->pNext;

    // Finally delete the entry
    DeleteEntry(pEntry);

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Delete a database entry
//!
//! \param pEntry : Entry to delete
//!
void ArgDatabase::DeleteEntry(ArgDatabaseEntry *pEntry)
{
    // Both the argument (and the scope if present) were set with
    // Utility::StrDup so it is necessary to free the data that was
    // allocated
    delete[] pEntry->pArg;
    if(pEntry->pScope) delete[] pEntry->pScope;
    pEntry->pArg = nullptr;
    pEntry->pScope = nullptr;
    delete pEntry;
}

//-----------------------------------------------------------------------------
//! \brief Javascript class for ArgDatabase
//!
static JSClass ArgDatabase_class =
{
    "ArgDatabase",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

//-----------------------------------------------------------------------------
//! \brief Javascript argument for ArgDatabase
//!
static SObject ArgDatabase_Object
(
    "ArgDatabase",
    ArgDatabase_class,
    0,
    0,
    "Argument database"
);

//-----------------------------------------------------------------------------
//! \brief Create a JSObject for the ArgDatabase
//!
//! \param cx    : Pointer to the javascript context.
//! \param obj   : Pointer to the javascript object.
//!
//! \return OK if the object was successfuly created, not OK otherwise
//!
RC ArgDatabase::CreateJSObject(JSContext *pContext, JSObject *pObject)
{
    //! Only create one JSObject per ArgDatabase
    if (m_pJsArgDatabaseObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this ArgDatabase.\n");
        return OK;
    }

    m_pJsArgDatabaseObj = JS_DefineObject(pContext,
                                          pObject,     // Global object
                                          "ArgDb",     // Property name
                                          &ArgDatabase_class,
                                          ArgDatabase_Object.GetJSObject(),
                                          JSPROP_READONLY);

    if (!m_pJsArgDatabaseObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    // Store the current ArgDatabase instance into the private area
    // of the new JSOBject.  This will tie the two together.
    if (JS_SetPrivate(pContext, m_pJsArgDatabaseObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of ArgDatabase.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Add the arguments represented in the provided JsArray to the
//!        argument database (required because declaring a vector<string>
//!        inside JS_STEST_LWSTOM results in a compiler warning)
//!
//! \param pContext  : Javascript context for the operation
//! \param pArgArray : Pointer to a JsArray representing the arguments to add
//! \param pArgDb    : Pointer to the argument database to add arguments to
//!
//! \return OK if successful, not OK otherwise
static RC AddArgsToDatabase(JSContext   *pContext,
                            JsArray     *pArgArray,
                            ArgDatabase *pArgDb)
{
    string             Arg;
    vector<string>     StrArgvArray;
    JavaScriptPtr      pJavaScript;

    // Colwert the provided JsArray to a vector of strings
    for (UINT32 i = 0; i < pArgArray->size(); i++)
    {
        if (OK != pJavaScript->FromJsval((*pArgArray)[i], &Arg))
        {
            JS_ReportWarning(pContext, "Unable to colwert argument to a string");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        StrArgvArray.push_back(Arg);
    }

    ArgPreProcessor    argPreProc;
    // Preprocess the arguments and add them to the database
    RC rc = argPreProc.PreprocessArgs(&StrArgvArray);
    if (rc != OK)
    {
        JS_ReportWarning(pContext, "Unable to preprocess arguments");
        return rc;
    }

    return argPreProc.AddArgsToDatabase(pArgDb);
}

//-----------------------------------------------------------------------------
//! \brief Add the arguments represented in the provided JsArray to the
//!        argument database
//!
//! \param ArgvArray : JsArray representing the arguments to add
//!
//! \return OK if successful, not OK otherwise
JS_STEST_LWSTOM(ArgDatabase, AddArgs, 1, "Add Arguments to the database")
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    const char *  pUsage = "Usage: ArgDatabase.AddArgs(ArgumentArray)\n";
    JsArray       ArgvArray;
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ArgvArray)))
    {
        JS_ReportWarning(pContext, pUsage);
        RETURN_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
    }

    ArgDatabase *pArgDatabase = JS_GET_PRIVATE(ArgDatabase, pContext, pObject, "ArgDatabase");
    if (pArgDatabase == NULL)
    {
        JS_ReportWarning(pContext, pUsage);
        RETURN_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
    }

    RETURN_RC(AddArgsToDatabase(pContext, &ArgvArray, pArgDatabase));
}

//-----------------------------------------------------------------------------
//! \brief Clear all arguments from the database
//!
JS_STEST_LWSTOM(ArgDatabase, ClearArgs, 0,
                "Clear all arguments from the database")
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportWarning(pContext, "Usage: ArgDatabase.ClearArgs()");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    ArgDatabase *pArgDatabase = JS_GET_PRIVATE(ArgDatabase, pContext, pObject, "ArgDatabase");
    if (pArgDatabase == NULL)
    {
        JS_ReportWarning(pContext, "Cannot get argument database");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    pArgDatabase->ClearArgs();

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
//! \brief Display all the arguments in the database
//!
//! \return OK if successful, not OK otherwise
JS_STEST_LWSTOM(ArgDatabase, ListAllArgs, 0, "List all arguments")
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportWarning(pContext, "Usage: ArgDatabase.ListAllArgs()");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    ArgDatabase *pArgDatabase = JS_GET_PRIVATE(ArgDatabase, pContext, pObject, "ArgDatabase");
    if (pArgDatabase == NULL)
    {
        JS_ReportWarning(pContext, "Cannot get argument database");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    char *pArgs = new char[4096];
    if (pArgs == NULL)
        RETURN_RC(RC::CANNOT_ALLOCATE_MEMORY);

    pArgDatabase->ListAllArgs(pArgs, 4096);

    Printf(Tee::PriHigh, "Arguments : %s\n", pArgs);

    delete[] pArgs;

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
//! \brief Check the database for unused arguments (i.e. arguments that are
//!        not optional and have never been parsed by an ArgReader)
//!
//! \param bPrintUnusedArgs : true if a message should be printed for each
//!                           unused argument, false if it should just return
//!                           the count.
//!
//! \return The number of unused arguments found
//!
JS_SMETHOD_BIND_ONE_ARG(ArgDatabase, CheckForUnusedArgs, bPrintArgs, bool,
                        "Check for unused arguments in the ArgDatabase");

JS_SMETHOD_BIND_NO_ARGS(ArgDatabase, AreTestEscapeArgsPresent,
                        "Check whether test escape arguments are present in the ArgDatabase");
JS_SMETHOD_VOID_BIND_NO_ARGS(ArgDatabase, PrintTestEscapeArgs,
                        "Print test escape arguments if any are present in the database");
