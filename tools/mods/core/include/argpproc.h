
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// $Id: $

//--------------------------------------------------------------------
//!
//! \file argdb.h
//! \brief Create an argument database preprocessor
//!
//! There are several shorthand notations that may be used for colwenience
//! on the command line.  The preprocessor expands the shorthand arguments
//! See the comment above the class for details.
//!
//--------------------------------------------------------------------

#ifndef INCLUDED_ARGPPROC_H
#define INCLUDED_ARGPPROC_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif
#ifndef INCLUDED_STL_STRING
#define INCLUDED_STL_STRING
#include <string>
#endif
#ifndef INCLUDED_STL_SET
#define INCLUDED_STL_SET
#include <set>
#endif

class ArgDatabase;

//--------------------------------------------------------------------
//! \brief The ArgPreProcessor preprocesses the command line.
//!
//! The ArgPreProcessor preprocesses all shorthand notations from the command
//! line.  The ArgPreProcessor also validates the integrity of the command line.
//! Shorthand entries and what they expand to are as follows:
//!
//!      -dev #    :  [+ENDDEV lwr_# | +ENDTEST lwr_test] +BEGINDEV #
//!          Shorthand to indicate that all subsequent arguments apply to the
//!          specificed device instance.  The '-dev' argument will
//!          terminate any existing device or test scope.
//!
//!      -dev all  :  [+ENDDEV lwr_# | +ENDTEST lwr_test]
//!          Shorthand to indicate that any subsequent arguments apply to all
//!          devices.  This effectively terminates any existing device or test
//!          scope
//!
//!      -<testarg> testname  :  [+ENDDEV lwr_# | +ENDTEST lwr_test] +BEGINTEST testname +ENDTEST testname
//!      -<testarg> "testname testargs"  :
//!      -<testarg> +begin testname testargs +end  :
//!              [+ENDDEV lwr_# | +ENDTEST lwr_test] +BEGINTEST testname testargs +ENDTEST testname
//!          Shorthand to add a test to the test list.  This effectively
//!          terminates any existing device or test scope.  The default
//!          testarg is not set effectively elimitating this command line
//!          shortlwt.  To enable the shortlwt, the testarg must be set via
//!          SetTestArg()
//!
//!      -<testfilearg> filename : [+ENDDEV lwr_# | +ENDTEST lwr_test] +BEGINTEST testname [args] +ENDTEST testname ...
//!          Shorthand to add a testfile to the test list.  Each line in the
//!          file is interpreted as a seperate test argument and generates
//!          an appropriate +BEGINTEST ... +ENDTEST section.  This effectively
//!          terminates any existing device or test scope.  The default
//!          testfilearg is not set effectively elimitating this command line
//!          shortlwt.  To enable the shortlwt, the testarg must be set via
//!          SetTestFileArg().  This shortlwt will also not be available
//!          unless the testarg is also set via SetTestArg().
//!
//!      +begin arg_with_spaces +end : "arg_with_spaces"
//!          Used to effectively 'quote' an argument that contains characters
//!          (i.e. spaces) that would normally be interpreted as argument breaks
//!          All characters between +begin/+end are added to the command line
//!          as a single argument
//!
//! Except where indicated, the ArgPreProcessor follows the following rules:
//!
//!      1.  No nesting of test or device scopes is permitted
//!      2.  +begin/+end sections may be nested within a test or device scope
//!          but not within another +begin/+end section
//!      3.  All scope specifiers are ignored within +begin/+end sections
//!          (quoting takes priority over all other arguments)
//!      4.  The command line must never end with an open +begin/+end section
//!      5.  All open scopes are terminated after preprocessing
//!      6.  Both shorthand and longhand notations may be used on the command
//!          line, however the resulting preprocessed line must still be valid
//!
class ArgPreProcessor
{
public:
    ArgPreProcessor();
    RC PreprocessArgs(vector<string> *pArgvArray);
    RC AddArgsToDatabase(ArgDatabase *pArgDatabase);
    UINT32 GetFirstDevScope();
    UINT32 GetNextDevScope(UINT32 prev);
    UINT32 LastDevScope(void) { return m_LastDevScope; }
    void PrintArgs();

    static bool GetDevScopeShortlwt()  { return s_bDevScopeShortlwt; }
    static RC SetDevScopeShortlwt(bool bEnable);
    static string GetTestArg() { return s_TestArg; }
    static RC SetTestArg(const string& testArg);
    static string GetTestFileArg() { return s_TestFileArg; }
    static RC SetTestFileArg(const string& testFileArg);
    static RC AddOptionalArg(const string& optArg);

private:
    //! This enumeration describes the types of scopes that may be processed
    enum ArgScope
    {
        SCOPE_NONE = 0,     //!< No scope - global arguments
        SCOPE_DEVICE = 1,   //!< Device scope
        SCOPE_TEST = 2      //!< Test scope
    };

    ArgScope       m_LwrrentScope;      //!< Current scope for all arguments
    string         m_LwrrentScopeName;  //!< Name of the current scope
    bool           m_bInQuotedBlock;    //!< true if lwrrently processing a
                                        //!< +begin/+end section
    bool           m_bInTestQuotedBlock;//!< true if inside a legacy style
                                        //!< -e +begin <testname> <args> +end
    string         m_ErrorString;       //!< String describing the last error
    string         m_TestFileErrPos;    //!< String descibing where the error
                                        //!< oclwrred in the test file
    string         m_TestErrLine;       //!< Test line where error oclwrred
    INT32          m_TestErrTok;        //!< The token number on the line where
                                        //!< an error oclwrred
    UINT32         m_TestTokenCount;    //!< Number of tokens within the test
                                        //!< argumnent
    vector<string> m_PreProcessedArgs;  //!< Holds the preprocessed args
    set<UINT32>    m_DevScopes;         //!< Set of all dev scopes encountered
                                        //!< on the command line
    UINT32         m_LastDevScope;      //!< Last device scope encountered on
                                        //!< the command line;

    static bool    s_bDevScopeShortlwt; //!< Enable shortlwt of "-dev" into a
                                        //!< device scope
    static string  s_TestArg;           //!< Set the parameter to interpret as a
                                        //!< test specification
    static string  s_TestFileArg;       //!< Set the parameter to interpret as a
                                        //!< test file specification
    static set<string> s_OptionalArgs;  //!< Holds the optional args
                                        //!< optional args may be unused in test

    bool IsScopeOrTestArg(const string& arg);
    RC   ValidateArg(const string& arg);
    RC   StartScope(const string& scopeName, ArgScope NewScope);
    RC   TerminateScope(const string& scopeName);
    RC   ParseTestFile(const string& filename);
    RC   ParseTest(const string& testString);
    RC   AddTest(vector<const char *> &tokens,
                 UINT32                startToken,
                 UINT32               *tokensProcessed);
    void CreateTestErrorLine(vector<const char *> &tokens, UINT32 startToken,
                             UINT32 tokensProcessed);
    void ReportError(vector<string> *pArgArray, UINT32 index);
};

#endif
