/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Define a hierarchy of potential events, used by the gilder

#ifndef INCLUDED_PMPOTEVT_H
#define INCLUDED_PMPOTEVT_H

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

//--------------------------------------------------------------------
//! \brief Abstract class representing an advanced-scheduling potential event
//!
//! A potential event is a temporary state during which an event can
//! occur.  Every potential event has a start and an end.
//!
//! For example, a potential page-fault event on page X begin when
//! page X is unmappend.  The potential page-fault event ends when
//! page X is remapped.  While the potential page-fault is active, we
//! can get a page fault on page X.
//!
//! Some potential events require an explicit action to end them, such
//! as the remapping mentioned above.  Other potential events
//! automatically end when the event oclwrs, such as traps.
//!
//! A PROBLEM:
//!
//! Sometimes we can't tell when a potential event ends.  Consider
//! this case:
//!
//! -# Unmap page X
//! -# Set the GPU registers to request a force fault
//! -# Page-fault event oclwrs on page X
//! -# Remap page X
//!
//! The potential page-fault ended at step 4.  But did the potential
//! force-fault occur?  Potential force-faults end when the fault
//! oclwrs, not when we remap the page.  So if the fault in step 3 was
//! due to the GPU registers, then potential force-fault ended at step
//! 3; but if the fault in step 3 was a genuine page-fault, then the
//! potential force-fault hasn't ended yet.
//!
//! (It's best not to mix page faults and force faults, IMHO.)
//!
//! CONSEQUENCE OF THIS PROBLEM
//!
//! Due to this problem, the entries written to the gild file tend to
//! emphasize what actually happened, rather than PolicyManager's
//! interpretation of what happened.  Let the poor schmuk who writes
//! the perl script worry about how to interpret what happened.
//!
//! For example, even though unmapping a page gets stored in the
//! gilder as the start of a potential page fault, it gets written to
//! the gilder as "UNMAPPED_PAGE..." rather than "START_POTENTIAL
//! PAGE_FAULT...".  The unmapping is what actually happened; the idea
//! that it started a potential page fault is just our interpretation.
//!
//! For potential events that end automatically, such as traps and
//! page faults, we don't print *anything* to the gild file.  We
//! record that the event oclwrred ("what actually happened"), but we
//! don't indicate that it ended a potential event ("our
//! intepretation").
//!
class PmPotentialEvent
{
public:
    enum Type
    {
        NON_STALL_INT
    };

    PmPotentialEvent(Type type) : m_Type(type) {}
    virtual ~PmPotentialEvent();
    Type           GetType()   const { return m_Type; }

    //! Return the string that will be written to the gild file (sans '\n')
    //! when the potential event starts.  Return "" to print nothing.
    //!
    virtual string GetStartGildString()                  const = 0;

    //! Return the string that will be written to the gild file (sans '\n')
    //! when the potential event ends.  Return "" to print nothing.
    //!
    virtual string GetEndGildString()                    const = 0;

    //! Return true if the event (e.g. page fault on page X in engine
    //! Y) matches the potential event (e.g. potential page fault on
    //! page X, or potential force-fault on page Y).
    //!
    virtual bool   Matches(const PmEvent *pEvent)        const = 0;

    //! Return true if the two potential events match.  This method
    //! is used to determine whether an end-potential-event (lhs)
    //! terminates a start-potential-event (rhs).
    //!
    virtual bool   Matches(const PmPotentialEvent *pRhs) const = 0;

    //! Return true if the potential events matches a gildfile
    //! description created by GetStartGildString().  This method is
    //! used to compare this run of mods to the previous run, so this
    //! method should ignore any fields that change from run to run.
    //!
    virtual bool   MatchesStart(const string &gildString) const = 0;

    //! Return true if the potential events matches a gildfile
    //! description created by GetEndGildString().  This method is
    //! used to compare this run of mods to the previous run, so this
    //! method should ignore any fields that change from run to run.
    //!
    virtual bool   MatchesEnd(const string &gildString) const = 0;

private:
    Type m_Type;  //!< Identifies which subclass this is
};

//--------------------------------------------------------------------
//! \brief Potential NonStallInt event
//!
class PmPotential_NonStallInt : public PmPotentialEvent
{
public:
    PmPotential_NonStallInt(PmNonStallInt *pNonStallInt);
    virtual string GetStartGildString()                   const;
    virtual string GetEndGildString()                     const;
    virtual bool   Matches(const PmEvent *pEvent)         const;
    virtual bool   Matches(const PmPotentialEvent *pRhs)  const;
    virtual bool   MatchesStart(const string &gildString) const;
    virtual bool   MatchesEnd(const string &gildString)   const;

private:
    PmNonStallInt *m_pNonStallInt;
};

#endif // INCLUDED_PMPOTEVT_H

