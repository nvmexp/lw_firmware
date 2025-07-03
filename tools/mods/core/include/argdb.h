/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008, 2019 by LWPU Corporation.  All rights reserved.  All
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
//! \brief Create an argument database for processing command line arguments
//!
//! There are several shorthand notations that may be used for colwenience
//! on the command line.  The preprocessor expands the shorthand arguments
//! See the comment above the class for details.
//!
//--------------------------------------------------------------------

#ifndef INCLUDED_ARGDB_H
#define INCLUDED_ARGDB_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

// Forward declarations
struct JSObject;
struct JSContext;

//--------------------------------------------------------------------
//! This structure describes an individual entry in the argument database
struct ArgDatabaseEntry
{
    char   *pArg;           //!< The argument from the command line
    char   *pScope;         //!< The scope where the argument should be applied
    UINT32  flags;          //!< Flags for the argument (MUST_USE, OPTIONAL)
    UINT32  usageCount;     //!< Number of times the argument was used (i.e.
                            //!< the number of times that an ArgReader parsed
                            //!< the particular argument

    struct ArgDatabaseEntry *pNext; //!< Pointer to the next entry in the database
};

//--------------------------------------------------------------------
//! \brief Command line argument database class
//!
class ArgDatabase
{
public:
    //! This enumeration describes the flags associated with each argumnet
    enum
    {
        ARG_MUST_USE    = (1 << 0),   //!< Argument must be used on any parsing pass
        ARG_OPTIONAL    = (1 << 1),   //!< Argument doesn't ever HAVE to be used
        ARG_TEST_ESCAPE = (1 << 2),   //!< Argument causes a test escape
    };

    ArgDatabase();
    ArgDatabase(ArgDatabase *pParentDb);
    ~ArgDatabase();

    void ClearArgs();
    bool IsEmpty() { return (m_pHead == NULL); }
    RC AddSingleArg(const char *pArg, const char *pScope = NULL, UINT32 flags = 0);
    RC AddArgLine(const char *pLine, UINT32 flags = 0);
    RC AddArgFile(const char *pFilename, UINT32 flags = 0);
    RC AddTestFile(const char *pFilename, UINT32 flags = 0);

    bool   AreTestEscapeArgsPresent() const;
    void   PrintTestEscapeArgs() const;
    UINT32 CheckForUnusedArgs(bool bPrintUnusedArgs) const;
    UINT32 ListAllArgs(char *pArgBuffer, UINT32 size);
    RC     CreateJSObject(JSContext *pContext, JSObject *pObject);
    JSObject * GetJSObject() {return m_pJsArgDatabaseObj;}

    friend class ArgReader;

private:
    RC RemoveEntry(ArgDatabaseEntry *pEntry);
    void DeleteEntry(ArgDatabaseEntry *pEntry);

    struct ArgDatabaseEntry  *m_pHead;      //!< Pointer to the head of the
                                            //!< argument database
    struct ArgDatabaseEntry **m_ppTail;     //!< Pointer to the tail of the
                                            //!< argument database
    ArgDatabase              *m_pParentDb;  //!< Pointer to the parent
                                            //!< database

    //! Number of objects that reference this database, all direct children
    //! add to this count and any ArgReaders that have parsed from this
    //! database also add to this count.  If the database is destroyed and
    //! the reference count is non-zero the destructor will complain
    UINT32                    m_RefCount;

    //! Javascript representation for the database
    JSObject                 *m_pJsArgDatabaseObj;
};

#endif
