/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2010,2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "trep.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string.h>

//------------------------------------------------------------------------------
Trep::Trep() :
    m_FileName(NULL),
    m_pFile(NULL),
    m_FileOpenRC(OK)
{

}

//------------------------------------------------------------------------------
Trep::~Trep()
{
    if(m_pFile)
    {
        fclose(m_pFile);
        m_pFile = NULL;
    }
}

//! \brief Set the trep file name
//!
//! Note that I'm maintaining compatibility with the old mdiag implementation
//! so it's pretty forgiving.
//------------------------------------------------------------------------------
RC Trep::SetTrepFileName( const char *newName )
{
    MASSERT(newName != NULL);

    if(m_pFile != NULL)
    {
        if( (m_FileName != NULL) && (strcmp(m_FileName, newName) == 0) )
        {
            Printf(Tee::PriLow,
                " Trep::SetTrepFileName: called when already open with identical"
                " filename (%s), doing nothing\n", newName );
            return OK;
        }
        else
        {
            Printf(Tee::PriLow,
                "Trep::SetTrepFileName: changing trep file from %s to %s\n",
                m_FileName, newName );
            fclose(m_pFile);
            m_pFile = NULL;
        }
    }

    m_FileName = newName;
    m_FileOpenRC = Utility::OpenFile(m_FileName, &m_pFile, "a");

    if(m_FileOpenRC != OK)
    {
        Printf(Tee::PriAlways,
            "Trep::SetTrepFileName: couldn't open trep file %s\n", newName );
    }
    else
    {
        Printf(Tee::PriDebug, "Trep::SetTrepFileName: trep file set to %s\n",
            newName );
    }

    return m_FileOpenRC;
}

//! \brief Append a string to the trep file
//!
//! Note that I'm maintaining compatibility with the old mdiag implementation
//! so it's pretty forgiving (and inefficient...why didn't they report an error
//! if the file wasn't open?)
//------------------------------------------------------------------------------
RC Trep::AppendTrepString( const char *str )
{
    if ( (m_FileName != NULL ) && (m_pFile == NULL) )
    {
        m_FileOpenRC = Utility::OpenFile(m_FileName, &m_pFile, "a");

        if (m_FileOpenRC != OK)
        {
            Printf(Tee::PriAlways,
                "TestDirectory::AppendTrepString: open of trep file %s failed\n",
                 m_FileName );
        }
    }
    else
    {
        if(m_pFile)
        {
            fprintf(m_pFile, "%s", str);
            fflush(m_pFile);
        }
        else
        {
            Printf(Tee::PriLow, "Couldn't write \"%s\" to trep file because the "
                "file couldn't be opened\n", str);
        }
    }

    return m_FileOpenRC;
}

//! \brief Append a test result string to trepfile based on RC
//!
//! I don't know why the original design didn't do it this way, perhaps
//! things were different then.
//!
//! In the current state of things, testgen looks for a specific line:
//!
//! test #<num>: <status>
//!
//! to indicate pass/fail.  Centralizing this logic is a good thing (tm)
//!
//! Oh, and while I'm at it:  this is a new function so it doesn't have to
//! be compatible with the old-style trep behavior.  If the file isn't open,
//! this function fails.
//------------------------------------------------------------------------------
RC Trep::AppendTrepResult
(
    UINT32 testNum, //!< Test number for this result (can run multiple in parallel)
    INT32 testRC    //!< We only need the INT32 value from RC, and that's all JS gives us anyway
)
{
    if(m_FileOpenRC != OK)
    {
        Printf(Tee::PriLow, "Couldn't AppendTrepResult because the trep file "
            "%s isn't open!\n", (m_FileName != NULL) ? m_FileName : "");
        return m_FileOpenRC;
    }

    const char * result;

    // This is coarse right now (just a few "popular" RCs).  Can improve later.
    //
    // mdiag deals with this in mdiag/tests/testdir.cpp
    switch(testRC)
    {
        case OK:
            // Not bothering with (gold) since MODS doesn't have a distinction
            // between "gold" and "lead" right now
            result = "TEST_SUCCEEDED";
            break;

        case RC::GOLDEN_VALUE_NOT_FOUND:
            result = "TEST_CRC_NON_EXISTANT";
            break;

        case RC::GOLDEN_VALUE_MISCOMPARE:
            result = "TEST_FAILED_CRC";
            break;

        default:
            result = "TEST_FAILED";
            break;
    }

    fprintf(m_pFile, "test #%d: %s\n", testNum, result);

    return m_FileOpenRC;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------
JS_CLASS_NO_ARGS(Trep, "Trep (Test Report) object", "Usage: new Trep()");
JS_STEST_BIND_ONE_ARG(Trep, SetTrepFileName, name, string, "Set the trep file");
JS_STEST_BIND_TWO_ARGS(Trep, AppendTrepResult, testNum, UINT32, testRC, INT32, "Write a test result to the trep file");
