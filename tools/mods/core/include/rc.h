/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012, 2014-2020 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// RC - Return Code

#pragma once

#ifndef INCLUDED_RC_H
#define INCLUDED_RC_H

#include "types.h"
#include "tee.h"

#ifdef SEQ_FAIL
#include "seqfail.h"
#endif

constexpr INT32 OK = 0;

//! \brief Return code, wrapper around an enum value from errors.h.
//!
//! It is an error to assign OK to a !OK RC object, it will assert that
//! the test is losing error status and possibly has a false-pass bug.
//!
//! To restore a !OK RC object to OK, you must use Clear.
//!
class RC
{
private:
    INT32 m_rc = OK;

public:
    enum RCCodes
    {
        #undef DEFINE_RC
        #define DEFINE_RC(errno, code, message) code = errno,
        #define DEFINE_RETURN_CODES
        #include "core/error/errors.h"
    };

    enum
    {
        TEST_BASE  = 1000,     // Encode test number in rc by multiplying by 1000.
        PHASE_BASE = 1000000   // Encode test phase in rc by multiplying by 1000000.
    };

    RC()                          = default;
    RC(INT32 rc)                  : m_rc(rc) { }
    RC(const RC& rc)              = default;
    ~RC()                         = default;

    void Clear()                  { m_rc = OK; }

    //! \brief Note: asserts if !OK is replaced with OK
    const RC& operator=(INT32 rc) { Set(rc); return *this;}
    RC& operator=(const RC& rc)   { Set(rc.Get()); return *this;}

    INT32        Get() const      { return m_rc; }
    operator     INT32() const    { return m_rc; }
    const char * Message() const;

protected:
    void Warn() const;
    void Set(INT32 rc)
    {
#ifdef SEQ_FAIL
        if (rc == OK && m_rc == OK && Utility::SeqFail())
        {
            rc = SOFTWARE_ERROR;
        }
#endif
        // Use MASSERT here once all issues are fixed.
#if defined(DEBUG) && !defined(MLE_TOOL)
        // Replacing error with OK is not allowed, must use Clear.
        if ((OK != m_rc) && (OK == rc))
        {
            Warn();
        }
#endif
        m_rc = rc;
    }
};

#ifdef __RESHARPER__
#define RESHARPER_HAS_SIDE_EFFECTS [[rscpp::has_side_effects]]
#else
#define RESHARPER_HAS_SIDE_EFFECTS
#endif

//! \brief Variant of RC that preserves first !OK value.
class StickyRC : public RC
{
public:
    using RC::RC;

    StickyRC()             = default;
    RESHARPER_HAS_SIDE_EFFECTS
    StickyRC(const RC& rc) : RC(rc) { }

    //! \brief Note: can't assert, but preserves first !OK assignment.
    RESHARPER_HAS_SIDE_EFFECTS
    StickyRC& operator=(INT32 rc)            { SSet(rc); return *this; }
    RESHARPER_HAS_SIDE_EFFECTS
    StickyRC& operator=(const RC& rc)        { SSet(rc.Get()); return *this; }
    RESHARPER_HAS_SIDE_EFFECTS
    StickyRC& operator=(const StickyRC& rc)  { SSet(rc.Get()); return *this; }

protected:
    void SSet(INT32 rc)
    {
        if (OK == Get())
            RC::Set(rc);
    }
};

#if defined(DEBUG) && !defined(MLE_TOOL)
#define DEBUG_CHECK_RC 1
#endif

#if !defined (DEBUG_CHECK_RC)
    #define CHECK_RC(f)             \
        do {                        \
            if (OK != (rc = (f)))   \
                return rc;          \
        } while (0)

    #define CHECK_RC_CLEANUP(f)     \
        do {                        \
            if (OK != (rc = (f)))   \
                goto Cleanup;       \
        } while (0)

    #define CHECK_RC_LABEL(l, f)    \
        do {                        \
            if (OK != (rc = (f)))   \
                goto l;             \
        } while (0)

    #define RETURN_RC_MSG(rc, ...)                                      \
        do {                                                            \
            CheckRcPrintfWrapper(Tee::PriNormal, __VA_ARGS__);          \
            return rc;                                                  \
        } while(0)

    #define CHECK_RC_MSG(f, ...)                                        \
        do {                                                            \
            if (OK != (rc = (f)))                                       \
            {                                                           \
                CheckRcPrintfWrapper(Tee::PriError, __VA_ARGS__);       \
                return rc;                                              \
            }                                                           \
        } while(0)

// NOTE: FIRST_RC is deprecated, please use StickyRC instead.

    #define FIRST_RC(f)             \
        do {                        \
            rc = (f);               \
            if (OK == firstRc)      \
                firstRc = rc;       \
        } while (0)

#else
    #define CHECK_RC(f)                                                 \
        do {                                                            \
            if (OK != (rc = (f)))                                       \
            {                                                           \
                ::Printf(Tee::PriLow,                                   \
                         "CHECK_RC returned %d at %s %d\n",             \
                         static_cast<UINT32>(rc), __FILE__, __LINE__);  \
                return rc;                                              \
            }                                                           \
        } while (0)

    #define CHECK_RC_CLEANUP(f)                                         \
        do {                                                            \
            if (OK != (rc = (f)))                                       \
            {                                                           \
                ::Printf(Tee::PriLow,                                   \
                         "CHECK_RC_CLEANUP returned %d at %s %d\n",     \
                         static_cast<UINT32>(rc), __FILE__, __LINE__);  \
                goto Cleanup;                                           \
            }                                                           \
        } while (0)

    #define CHECK_RC_LABEL(l, f)                                        \
        do {                                                            \
            if (OK != (rc = (f)))                                       \
            {                                                           \
                ::Printf(Tee::PriLow,                                   \
                         "CHECK_RC_LABEL: \"" #f "\" returned %x at %s %d\n", \
                         static_cast<UINT32>(rc), __FILE__, __LINE__);  \
                goto l;                                                 \
            }                                                           \
        } while (0)

    #define RETURN_RC_MSG(rc, ...)                                      \
        do {                                                            \
            ::Printf(Tee::PriLow,                                       \
                     "RETURN_RC_MSG returned %d at %s %d\n",            \
                     static_cast<UINT32>(rc), __FILE__, __LINE__);      \
            CheckRcPrintfWrapper(Tee::PriNormal, __VA_ARGS__);          \
            return rc;                                                  \
        } while(0)

    // Chatty version of CHECK_RC, especially useful for functions
    // that are called from constructors

    #define CHECK_RC_MSG(f, ...)                                        \
        do {                                                            \
            if (OK != (rc = (f)))                                       \
            {                                                           \
                ::Printf(Tee::PriLow,                                   \
                         "CHECK_RC_MSG returned %d at %s %d\n",         \
                         static_cast<UINT32>(rc), __FILE__, __LINE__);  \
                CheckRcPrintfWrapper(Tee::PriError, __VA_ARGS__);       \
                return rc;                                              \
            }                                                           \
        } while(0)

// NOTE: FIRST_RC is deprecated, please use StickyRC instead.

    #define FIRST_RC(f)                                                 \
        do {                                                            \
            if (OK != (rc = (f)))                                       \
            {                                                           \
                ::Printf(Tee::PriLow,                                   \
                         "FIRST_RC returned %d at %s %d\n",             \
                         static_cast<UINT32>(rc), __FILE__, __LINE__);  \
            }                                                           \
            if (OK == firstRc)                                          \
                firstRc = rc;                                           \
        } while (0)

#endif // DEBUG_CHECK_RC

// This function is used by CHECK_RC_MSG so the message can accept
// printf args while preserving prior behavior.  If the message is a
// simple string, it is printed as printf("%s\n", msg).  If it is
// followed by printf args, it is printed as printf(msg + "\n", args).
// Either way, the message does not have a terminating "\n".
template<typename... Types> inline void CheckRcPrintfWrapper
(
    Tee::Priority pri,
    const char*   format,
    Types...      args
)
{
    Printf(pri, (std::string(format) + "\n").c_str(), args...);
}
template<> inline void CheckRcPrintfWrapper(Tee::Priority pri, const char* str)
{
    Printf(pri, "%s\n", str);
}

#endif // ! INCLUDED_RC_H
