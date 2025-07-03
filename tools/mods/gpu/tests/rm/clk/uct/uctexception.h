/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTEXCEPTION_H_
#define UCTEXCEPTION_H_

#include <list>

#include "core/include/massert.h"
#include "core/include/rc.h"

#include "gpu/tests/rm/utility/rmtmemory.h"
#include "gpu/tests/rm/utility/rmtstream.h"
#include "gpu/tests/rm/utility/rmtstring.h"
#include "uctstatement.h"

// Allow __PRETTY_FUNCTION__ to work everywhere that __FUNCTION__ works.
#if (!defined(__GNUC__) || __GNUC__ < 2) && !defined(__PRETTY_FUNCTION__)
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

namespace uct
{
    class FieldVector;
    class FullPState;

    //!
    //! \brief      Base exception
    //!
    //! \details    The default constructor creates a 'Null' exception, which
    //!             means that there has been no error or failure.
    //!
    //!             In general, the inherited 'file' and 'line' members indicate
    //!             the location within the CTP (Clock Test Profile) file that
    //!             is associated with the exception (not __FILE__ and __LINE__).
    //!             However, no logic within this class depends on such.
    //!
    struct Exception: public rmt::FileLocation
    {
        //! \brief      Type and/or severity of the error
        enum Level
        {
            Null        =   0,  //!< No exception
            Skipped     =  30,  //!< Skipped test: Continue running tests
            Failure     =  60,  //!< Test failure: Continue running tests
            DryRun      =  90,  //!< Dry run mode: Run no tests but print as though we do.
            Unsupported = 120,  //!< Feature required (e.g. RAM type): Run no tests but continue parsing and checking features.
            Syntax      = 150,  //!< Syntax error: Run no tests but continue parsing.
            Error       = 180,  //!< General error: Run no tests but continue parsing.
            Fatal       = 210,  //!< Fatal error: Exit immediately
        } level;

        //!
        //! \brief      MODS Result Code
        //!
        //! \details    Zero (OK) means "not-applicable" or "unknown" since a
        //!             nonexception (i.e. "okay") is indicated by a 'Null' level.
        //!
        RC rc;

        //! \brief      Error message
        rmt::String message;

        //!
        //! \brief      Exception 'thrower'
        //!
        //! \details    This is __FUNCTION__ or __PRETTY_FUNCTION__ where the
        //!             exception is first detected.  If this field is initialized
        //!             with __PRETTY_FUNCTION__ passed through a constructor,
        //!             the return value and parameter list is discarded to
        //!             improver readability.
        //!
        rmt::String function;

        //!
        //! \brief      Default MODS Result Code
        //! \see        diag/mods/core/error/errors.h
        //!
        static inline RC defaultRC(Level level)
        {
            switch(level)
            {
            case Null:        return RC();                          // No exception
            case Skipped:     return RC::SPECIFIC_TEST_NOT_RUN;     // Skipped tests
            case Failure:     return RC::LWRM_ERROR;                // Test failure
            case DryRun:      return RC();                          // Dry run mode is not a MODS error
            case Unsupported: return RC::UNSUPPORTED_SYSTEM_CONFIG; // Feature required (e.g. RAM type)
            case Syntax:      return RC::ILWALID_FILE_FORMAT;       // Syntax error in CTP file
            case Error:       return RC::SOFTWARE_ERROR;            // General error
            case Fatal:       return RC::SOFTWARE_ERROR;            // Fatal error: Exit immediately
            }

            return RC::SOFTWARE_ERROR;          // Whatever
        }

        //! \brief      Construct null exception
        inline Exception()
        {
            this->level     = Null;
            this->function  = NULL;
        }

        //! \brief      Construct a general exception unassociated with a file location
        inline Exception(const rmt::String &message, const char *function, Level level)
        {
            this->level     = level;
            this->function  = rmt::String::function(function);
            this->message   = message;
            this->rc        = OK;
        }

        //! \brief      Construct a general exception associated with a file location
        inline Exception(const rmt::String &message, const rmt::FileLocation &location, const char *function, Level level)
        {
            this->file      = location.file;
            this->line      = location.line;
            this->function  = rmt::String::function(function);
            this->level     = level;
            this->message   = message;
            this->rc        = OK;
        }

        //!
        //! \brief      Construct a general MODS exception
        //!
        //! \details    If 'rc' is OK, then the Null exception is constructed.
        //!
        inline Exception(RC rc, const rmt::String &message, const rmt::FileLocation &location, const char *function, Level level = Failure)
        {
            // Null exception if all is okay
            if (rc == OK)
            {
                this->level     = Null;
                this->function  = NULL;
            }

            // Normal exception otherwise
            else
            {
                this->file      = location.file;
                this->line      = location.line;
                this->function  = rmt::String::function(function);
                this->message   = message;
                this->rc        = rc;
                this->level     = level;
            }
        }

        //!
        //! \brief      Construct an RMAPI exception
        //!
        //! \details    'level' is set to 'Null' if 'rc' is OK.
        //!
        inline Exception(RC rc, const char *command, const char *function, Level level = Failure)
        {
            // Null exception if all is okay
            if (rc == OK)
            {
                this->level     = Null;
                this->function  = NULL;
            }

            // Normal exception otherwise
            else
            {
                this->function  = rmt::String::function(function);
                this->rc        = rc;
                this->level     = level;

                // Infer the message from the command and MODS result code
                if (command)
                    message = rmt::String(command) + " failed";
                if (command && rc != OK)
                    message += " with \"";
                if (rc != OK)
                    message += rc.Message();
                if (command && rc != OK)
                    message += "\"";
            }
        }

        //!
        //! \brief      Construct an RMAPI exception
        //!
        //! \details    'level' is set to 'Null' if 'rc' is OK.
        //!
        inline Exception(RC rc, const char *command, const rmt::FileLocation &location, const char *function, Level level = Failure)
        {
            // Null exception if all is okay
            if (rc == OK)
            {
                this->level     = Null;
                this->function  = NULL;
            }

            // Normal exception otherwise
            else
            {
                this->file      = location.file;
                this->line      = location.line;
                this->function  = rmt::String::function(function);
                this->rc        = rc;
                this->level     = level;

                // Infer the message from the command and MODS result code
                if (command)
                    message = rmt::String(command) + " failed";
                if (command && rc != OK)
                    message += " with \"";
                if (rc != OK)
                    message += rc.Message();
                if (command && rc != OK)
                    message += "\"";
            }
        }

        //! \brief      Destruct
        virtual ~Exception()
        {   }

        //! \brief      Is this null?
        inline bool operator!() const
        {
            return level == Null;
        }

        //! \brief      Is this not null?
        inline operator bool() const
        {
            return level != Null;
        }

        //!
        //! \brief      String in human-readable form.
        //!
        //! \details    No newlines are inserted into the text.
        //!
        //!             The format is:
        //!             file(line): level: message [function]
        //!
        virtual rmt::String string() const;

        //!
        //! \brief      Copy the exception object
        //!
        //! \details    The caller owns the newly created object and is expected
        //!             to delete it when it is no longer needed.
        //!
        virtual Exception *clone() const;
    };

    //!
    //! \brief      UCTReader Exception
    //!
    //! \details    A class for reporting status to upper levels by UCTReader.
    //!             The typical 'level' is one of 'Null', 'Eof', or 'Syntax'.
    //!
    struct ReaderException: Exception
    {
        //! \brief      Statement with syntax (or similar) error
        CtpStatement    statement;

        //! \brief      Construct null exception
        inline ReaderException():
            Exception()  { }

        // TODO: Retire this constructor in favor of one that includes the statement
        inline ReaderException(const rmt::String &message, const rmt::FileLocation &location, const char *function, Level level = Syntax):
            Exception(message, location, function, level)  { }

        //! \brief      Construct a syntax or similar exception
        inline ReaderException(const rmt::String &message, const CtpStatement &statement, const char *function, Level level = Syntax):
            Exception(message, statement, function, level)
        {
            this->statement = statement;
        }

        //! \brief      Construct a syntax or similar exception
        ReaderException(const rmt::String &message, const FieldVector &block, const char *function, Level level = Syntax);

        virtual ~ReaderException()
        {   }

        //!
        //! \brief      String in human-readable form.
        //!
        //! \details    No newlines are added to the beginning or end of the text.
        //!             A newline is inserted between lines.
        //!
        //!             The format is:
        //!             file(line): level: message [function]
        //!             file(line): statement
        //!
        //!             Note that it is possible (albeit unlikely) that 'file'
        //!             and 'line' differ between the two lines.  The first
        //!             line shows the location of 'this' exception object and
        //!             the second is from 'this->statement'.  They would differ
        //!             only if this exception was manipulated after construction
        //!             of if 'statement' is empty.
        //!
        virtual rmt::String string() const;

        //! \brief      Copy the exception object
        virtual Exception *clone() const;
    };

    //!
    //! \brief      Solver Exception
    //!
    //! \todo   Consider eliminating this class altogether.
    //!
    struct SolverException: Exception
    {
        //! \brief      Construct null exception
        inline SolverException(): Exception()
        {   }

        // TODO: Retire this in favor of constructors that require a message
        inline SolverException(Level level):
          Exception(NULL, NULL, level)
        {   }

        //! \brief      Construct a syntax or similar exception
        inline SolverException(const rmt::String &message, const rmt::FileLocation &location, const char *function, Level level = Syntax):
          Exception(message, location, function, level)
        {   }

        //! \brief      Construct an exception without a CTP file location
        inline SolverException(const rmt::String &message, const char *function, Level level = Error):
          Exception(message, function, level)
        {   }

        virtual ~SolverException()
        {   }
    };

    //!
    //! \brief      Exception with Fully-Defined P-State
    //!
    class PStateException: public Exception
    {
    protected:
        //!
        //! \brief      P-State ilwolved in the exception
        //!
        //! \details    A pointer is used so that the FullPState struct does not
        //!             have to be defined in this header.  This permits the
        //!             header that defines FullPState (uctpstate.h) to include
        //!             this header without cirlwlar dependencies.
        //!
        //! \todo       Consider moving this class to uctpstate.h so that we
        //!             can use an actual object rather than a pointer and
        //!             get rid of the memory management logic.
        //!
        const FullPState  *pPState;

        //!
        //! \brief      Allocate/copy the p-state
        //!
        //! \details    This funcion makes a copy of the p-state object and
        //!             assigned the internal pointer to the new copy.
        //!             We need a copy because the destructor deletes it.
        //!
        //! \pre        No p-state object has previously been allocated.
        //!
        void setPState(const FullPState &pstate);

    public:
        //! \brief      Construct null exception
        inline PStateException(): Exception()
        {
            pPState = NULL;
        }

        //! \brief      Copy Construction
        inline PStateException(const PStateException &that)
        {
            if (that.pPState)
                setPState(*that.pPState);
            else
                pPState = NULL;
        }

        //!
        //! \brief      Construct a MODS exception
        //!
        //! \details    If 'rc' is OK, then the Null exception is constructed.
        //!
        inline PStateException(RC rc, const rmt::String &message, const FullPState &pstate, const char *function, Level level = Failure):
            Exception(rc, message, FileLocation(), function, level)
        {
            if (rc != OK)
                setPState(pstate);
        }

        //! \brief      Destruction
        virtual ~PStateException();

        //!
        //! \brief      String in human-readable form.
        //!
        //! \details    No newlines are added to the beginning or end of the text.
        //!             A newline is inserted between lines.
        //!
        //!             The format is:
        //!             file(line): level: message [function]
        //!             file(line): pstate-details
        //!
        //!             Note that 'file' and 'line' may differ between the two
        //!             lines, especially if this excpetion object was constructed
        //!             from another exception object.  The first line shows the
        //!             location the original exception (typically the location
        //!             of the trial specification).  The second line is from
        //!             the p-state (typically the reference within the trial
        //!             specification).
        //!
        virtual rmt::String string() const;

        //! \brief      Copy the exception object
        virtual Exception *clone() const;
    };

    //!
    //! \brief      List of Exception.
    //!
    //! \ilwariant  None of the elements are 'Null'
    //!
    class ExceptionList: protected std::list<const Exception *>
    {
    protected:
        //!
        //! \brief      Most serious level in the list
        //!
        //! \ilwariant  'level' is 'Null' if and only if the list is empty.
        //!
        Exception::Level level;

        //!
        //! \brief      MODS result code associated with the most serious level
        //! \see        modsRC
        //! \see        Exception::defaultRC
        //!
        //! \details    Zero (OK) means "not-applicable" or "unknown" since an
        //!             empty list (i.e. "okay") is indicated by a 'Null' level.
        //!             Note that 'modsRC' applies 'defailtRC' when 'rc' is zero.
        //!
        RC rc;

        //!
        //! \brief      Push while updating 'level' and 'rc'
        //!
        //! \details    If 'pException' is NULL, no action is taken.  However,
        //!             this function does not filter out 'Null' exceptions.
        //!
        //!             This list object takes ownership of the exception object
        //!             point to by 'pException' so that it will be deleted when
        //!             the list is destructed.
        //!
        void push_back(const Exception * const &pException);

    public:

        //! \brief      Construct an empty exception list
        inline ExceptionList(): std::list<const Exception *>()
        {
            level = Exception::Null;
        }

        //! \brief      Copy each exception into newly constructed list
        inline ExceptionList(const ExceptionList &that)
        {
            level = that.level;
            rc    = that.rc;
            push(that);
        }

        //! \brief      Construct a singleton list
        inline ExceptionList(const Exception &that)
        {
            level = Exception::Null;
            push(that);
        }

        //! \brief      Destruction
        virtual ~ExceptionList();

        //! \brief      Copy each exception into newly constructed list
        inline ExceptionList &operator=(const ExceptionList &that)
        {
            level = that.level;
            rc    = that.rc;
            push(that);
            return *this;
        }

        //!
        //! \brief      Push unless the exception is okay (null).
        //!
        //! \details    A clone of the parameter object is pushed onto this list
        //!             unless it is a 'Null' exception.
        //!
        inline const Exception &push(const Exception &exception)
        {
            if (exception)
                push_back(exception.clone());
            return exception;
        }

        //!
        //! \brief      Copy all elements from 'that' to 'this'
        //!
        //! \details    A clone of each object is pushed onto this list.
        //!
        void push(const ExceptionList &that);

        //! @brief      Next element in the queue
        inline const Exception *front() const
        {
            return std::list<const Exception *>::front();
        }

        //! \brief      Remove all exceptions
        inline void clear()
        {
            level = Exception::Null;
            rc    = OK;
            return std::list<const Exception *>::clear();
        }

        //! \brief      Number of exceptions in the list
        inline size_type size() const
        {
            return std::list<const Exception *>::size();
        }

        //
        //! \brief      MODS result code associated with the most serios level
        //! \see        rc
        //! \see        Exception::defaultRC
        //!
        //! \ilwariant  'modsRC' is OK if the list is empty.
        //!
        inline RC modsRC() const
        {
            if (rc != OK)
                return rc;
            return Exception::defaultRC(level);
        }

        //! \brief      Highest level
        inline const Exception mostRecent() const
        {
            // Null expcetion if the list is empty
            if (empty())
                return Exception();

            // Last exception pushed onto the list.
            return *std::list<const Exception *>::back();
        }

        //! \brief      Highest level
        inline Exception::Level mostSerious() const
        {
            return level;
        }

        //! \details    All is okay if the queue is empty
        inline bool isOkay() const
        {
            return empty();
        }

        //! \brief      Is this at least as serious than a syntax (or other) error?
        inline bool isSerious(Exception::Level level = Exception::Syntax) const
        {
            return this->level >= level;
        }

        //! \brief      Is the level fatal or worse?
        inline bool isFatal() const
        {
            return level >= Exception::Fatal;
        }

        //! \brief      Is the level Unsupported?
        inline bool isUnsupported() const
        {
            return level == Exception::Unsupported;
        }

        //! \brief      Text for all elements
        rmt::String string() const;
    };
};

#endif

