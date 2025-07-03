/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file
//!
//! Implements various exceptions classes
//!

#include <sstream>

#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctexception.h"
#include "uctfield.h"
#include "uctpstate.h"

/*******************************************************************************
    Exception
*******************************************************************************/

//! \brief Exception info as a string
rmt::String uct::Exception::string() const
{
    rmt::String s = locationString();

    switch (level)
    {
        // Empty string for nonexceptions
        case Null:          return NULL;

        // Indicate the level before the msssage
        case Skipped:       s += "Skipped";     break;
        case Failure:       s += "Failure";     break;
        case Syntax:        s += "Syntax";      break;
        case DryRun:        s += "DryRun";      break;
        case Unsupported:   s += "Unsupported"; break;
        case Error:         s += "Error";       break;
        case Fatal:         s += "Fatal";       break;
    }

    // Append the message itself (if any)
    if (!message.empty())
        s += ": " + message;

    // Append the function in brackets (if any)
    if (function && *function)
        s += " [" + function + ']';

    // Done
    return s;
}

// Make a copy of this
uct::Exception *uct::Exception::clone() const
{
    return new Exception(*this);
}

/*******************************************************************************
    ReaderException
*******************************************************************************/

// Construct a syntax or similar exception
uct::ReaderException::ReaderException(const rmt::String &message, const FieldVector &block, const char *function, Level level):
    Exception(message, function, level)
{
    const Field *pStart = block.start();
    if (pStart)
    {
        this->statement = *pStart;
        this->file      = pStart->file;
        this->line      = pStart->line;
    }
}

// Exception info as a string
rmt::String uct::ReaderException::string() const
{
    // Start with the normal exception data
    rmt::String s = Exception::string();

    // Add statement details as applicable
    if (!statement.isEmpty())
        s += '\n' + statement.debugString();

    // Done
    return s;
}

// Make a copy of this
uct::Exception *uct::ReaderException::clone() const
{
    return new ReaderException(*this);
}

/*******************************************************************************
    PStateException
*******************************************************************************/

// Destruction
uct::PStateException::~PStateException()
{
    if (pPState)
        delete pPState;
}

// Allocate and copy the p-state
void uct::PStateException::setPState(const FullPState &pstate)
{
    pPState = new FullPState(pstate);
    file    = pstate.start.file;
    line    = pstate.start.line;
}

//!
//! \brief      String in human-readable form.
//!
//! \details    No newlines are added to the beginning or end of the text.
//!             A newline is inserted between lines.
//!
//!             The format is:
//!             file(line): level: message [function]
//!             pstate: details
//!
rmt::String uct::PStateException::string() const
{
    // Start with the base class and concatenate the p-state info
    rmt::String s = Exception::string();

    // Add p-state info if it exists.
    if (pPState)
        s += rmt::String("\n") + pPState->string();

    // That's all there is to it.
    return s;
}

//! \brief      Copy the exception object
uct::Exception *uct::PStateException::clone() const
{
    return new PStateException(*this);
}

/*******************************************************************************
    ExceptionList
*******************************************************************************/

// Destruction
uct::ExceptionList::~ExceptionList()
{
    const_iterator ppException;
    for (ppException = begin(); ppException != end(); ++ppException)
        if (*ppException)
            delete *ppException;
}

// Push while updating 'level' and 'rc'
void uct::ExceptionList::push_back(const Exception * const &pException)
{
    // Record the MODS result code if this is the most serious error
    // or if no result code has yet been recorded.
    if (pException && (pException->level > level || rc == OK))
        rc = pException->rc;

    // Record the most serious level.
    if (pException && pException->level > level)
        level = pException->level;

    // Add the exception to the list.
    if (pException)
        std::list<const Exception *>::push_back(pException);
}

// Copy all elements from 'that' to 'this'
void uct::ExceptionList::push(const uct::ExceptionList &that)
{
    const_iterator ppException;
    for (ppException = that.begin(); ppException != that.end(); ppException++)
        push_back((*ppException)->clone());
}

// Human-readable text
rmt::String uct::ExceptionList::string() const
{
    switch (size())
    {
        // An empty list means that there are no exceptions.
        case 0:     return "There are no exceptions.";

        // A singleton list is the same as an exception object.
        case 1:     return front()->string();
    }

    // Multiple exceptions are more complicated.
    std::stringstream oss;
    const_iterator ppException;

    // Summarize the exception list.
    oss << size() << " exceptions result in MODS RC=" << (unsigned) modsRC();

    // Add on the details from each exception.
    for (ppException = begin(); ppException != end(); ++ppException)
        oss << '\n' << (*ppException)->string();
    return oss;
}

