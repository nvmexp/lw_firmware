/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2012,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ATOMFSM_H
#define INCLUDED_ATOMFSM_H

#ifndef INCLUDED_CHANNEL_H
#include "core/include/channel.h"
#endif

//--------------------------------------------------------------------
//! \brief Finite state machine to detect atomic operations
//!
//! This state machine takes channel methods as inputs, and detects
//! where atomic operations begin and end.  An "atomic operation" is a
//! series of methods, method headers, etc that are required to stay
//! together.  It is forbidden to insert methods into the middle of an
//! atomic method.
//!
//! By itself, this class does not protect atomic operations -- it
//! merely detects them.  It is up to other classes (especially
//! AtomChannelWrapper) to create an AtomFsm and use it to protect
//! atoms.
//!
//! The "main" method of AtomFsm is EatMethod(), which takes a Channel
//! method, updates the state, and returns a Transition enum that
//! tells whether the method is the start of an atom, end of an atom,
//! middle of an atom, not in an atom, etc.
//!
//! There are two types of atoms: a WriteHeader that hasn't had it's
//! data written yet, and a series of specific methods.  The former is
//! tracked by m_UnresolvedHeaderSize.  The latter is defined by a
//! pseudo-grammar in atomlist.h, which can specify atoms of the form
//! "one of method A, followed by any number of methods B, C, and/or
//! D, followed by one of method E or F, ...".  The atomlist.h atoms
//! are tracked by m_AtomDefs and m_pLwrrentAtom.  It's possible for
//! the two types to overlap: a header/data sequence can occur in the
//! middle of an atomic series of methods.
//!
//! An AtomFsm can be copied by copy-constructor or assignment, which
//! can be useful if one class wants to "probe ahead" a bit, without
//! losing the initial state.  To keep AtomFsm's reasonably
//! lightweight, the data from atomlist.h is stored in a shared static
//! structure, which is deleted when the last valid AtomFsm is
//! deleted.
//!
//! (BTW, I hate classes that parse the methods, too.  Maybe we'll
//! figure out a better way to solve the problem someday.)
//!
class AtomFsm
{
public:
    //! Return values from methods that "eat" or "peek" channel methods
    //!
    enum Transition
    {
        NOT_IN_ATOM,    //!< The method is not part of an atom.
        IN_ATOM,        //!< The method is a continuation of the current atom
        IN_NEW_ATOM,    //!< The method is the start of a new atom
        ATOM_ENDS_AFTER //!< The current atom ends after this method
    };

    AtomFsm();
    AtomFsm(Channel *pChannel);
    AtomFsm(const AtomFsm &Rhs) : m_pChannel(NULL) { *this = Rhs; }
    static void ShutDown();

    AtomFsm &operator=(const AtomFsm &Rhs);
    static bool IsSupported(Channel *pChannel);
    bool IsValid() const { return m_pChannel != NULL; }

    void CancelAtom();
    void Reset();

    Transition SetObject(UINT32 Subch, LwRm::Handle Handle);
    Transition UnsetObject(LwRm::Handle Subch);

    Transition EatMethod(UINT32 Subch, UINT32 Method, UINT32 Data);
    Transition EatHeader(UINT32 Subch, UINT32 Method, UINT32 Count,
                         bool NonInc);
    Transition EatSubroutine(UINT32 Size);

    Transition PeekMethod(UINT32 Subch, UINT32 Method, UINT32 Data);
    Transition PeekHeader(UINT32 Subch, UINT32 Method, UINT32 Count,
                          bool NonInc);

    //! Colwenience method to tell whether inserting something after
    //! the last Eat* method would break an atom.  It's possible to
    //! deduce that info by keeping track of the Transition return
    //! values.
    //!
    bool InAtom() const { return m_pAtomDef || m_UnresolvedHeaderSize; }

private:
    //! Used to parse atomlist.h.  Tells how to process the
    //! (remaining) arguments in the current line.
    //!
    enum ParseType
    {
        PARSE_WITHOUT_DATA,    //!< Command does not have Mask+Data info
        PARSE_WITH_DATA,       //!< Command ends with Mask+Data info
        PARSE_WITH_DATA_1ARG,  //!< Cmd ends w/Mask+Data, & we've parsed 1 arg
        PARSE_WITH_DATA_NARGS  //!< Cmd ends w/Mask+Data; & we've already
                               //!< parsed 2 or more args
    };

    //! Used by AtomPhase.  See AtomDef for more details.
    //!
    enum PhaseType
    {
        TYPE_ONE_OF,
        TYPE_MANY_OF,
        TYPE_ONE_NOT_OF,
        TYPE_MANY_NOT_OF
    };

    //! One phase of an AtomDef.  See AtomDef for more details.
    //!
    struct AtomPhase
    {
        PhaseType   Type;    //!< The type of phase
        set<UINT32> Methods; //!< The methods expected in this phase
        UINT32      Mask;    //!< If nonzero, a method only matches this phase
                             //!< if (method_data & Mask) == Data
        UINT32      Data;    //!< See Mask
    };

    //! This class is used to store an atom definition parsed from
    //! atomlist.h.
    //!
    //! Each AtomDef consists of a series of "phases" that the atom
    //! passes through.  A phase contains information such as "one of
    //! methods A or B", "zero or more of methods A, B, or C", etc.
    //! See atomlist.h for a more complete list.
    //!
    //! The AtomDefs are stored statically, in s_AtomDefs, and freed
    //! when the last valid AtomFsm is deleted.
    //!
    struct AtomDef
    {
        UINT32 ClassID;
        vector<AtomPhase> Phases;

        AtomDef &AddPhase(PhaseType phaseType, ParseType parseType);
        AtomDef &operator,(UINT32 Arg);
        void FinishParsing();
        ParseType LwrrentParseType;
    };

    //! Subclass of vector<UINT32> that uses the comma operator to
    //! append to the vector.  Used to parse some comma-separated
    //! lists from atomlist.h.
    //!
    class CommaSepList : public vector<UINT32>
    {
    public:
        CommaSepList &operator,(UINT32 Val) { push_back(Val); return *this; }
        CommaSepList &operator,(const vector<UINT32> &Rhs)
            { insert(end(), Rhs.begin(), Rhs.end()); return *this; }
    };

    static void InitializeAtomDefs();
    static vector<UINT32> GetClassList(UINT32 OldestClass, UINT32 NewestClass);
    static UINT64 GetKey(UINT32 ClassID, UINT32 Method);
    Transition EatOrPeekMethod(UINT32 Subch, UINT32 Method, UINT32 Data,
                               AtomDef **ppAtomDef, UINT32 *pPhase,
                               UINT32 *pUnresolvedHeaderSize) const;

    static map<UINT64, vector<AtomDef> > s_AtomDefs;

    Channel *m_pChannel;   //!< The channel that we're processing methods for
    LwRm::Handle m_hObject[Channel::NumSubchannels];
                                                //!< The object on each subch.
                                                //!< Set by SetObject().
    UINT32 m_ClassID[Channel::NumSubchannels];  //!< The class on each subchan.
                                                //!< Set by SetObject().
    AtomDef *m_pAtomDef;        //!< Points at the AtomDef of the current atom
    UINT32 m_Phase;             //!< Current index into m_pAtomDef->Phases[]
    UINT32 m_UnresolvedHeaderSize; //!< Used for "unresolved header" atoms
};

#endif // INCLUDED_ATOMFSM_H
