/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2014-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "atomfsm.h"
#include <algorithm>
#include "atomlist.h"
#include "gpu/include/gralloc.h"
#include "core/include/utility.h"
#include "class/cl0000.h"  // LW01_NULL_OBJECT

static const UINT32 MIN_ENGINE_METHOD = 0x100;

/* static */ map<UINT64, vector<AtomFsm::AtomDef> > AtomFsm::s_AtomDefs;

//--------------------------------------------------------------------
//! \brief Default constructor
//!
//! The default constructor creates an "invalid" AtomFsm, which cannot
//! be used.  To create a valid AtomFsm, use the constructor that
//! takes a Channel argument, or the copy constructor from a valid
//! AtomFsm.
//!
//! The main purpose of invalid AtomFsms is for the case when you plan
//! to assign it from a valid AtomFsm later.
//!
AtomFsm::AtomFsm() : m_pChannel(NULL)
{
    Reset();
}

//--------------------------------------------------------------------
//! \brief constructor
//!
//! \param pChannel The channel that the method stream will go into.
//!     Can be NULL, which is the same as calling the default
//!     constructor.
//!
AtomFsm::AtomFsm
(
    Channel *pChannel
) :
    m_pChannel(pChannel)
{
    if (pChannel)
    {
        InitializeAtomDefs();
    }
    Reset();
}

//--------------------------------------------------------------------
//! \brief Delete the AtomDefs parsing info.
//!
//! This method should be called at Gpu::Shutdown time.  It is
//! important not to call this method while there are any valid
//! AtomFsm objects, since the objects keep pointers into s_AtomDefs.
//!
/* static */ void AtomFsm::ShutDown()
{
    s_AtomDefs.clear();
}

//--------------------------------------------------------------------
//! \brief assignment operator
//!
AtomFsm &AtomFsm::operator=(const AtomFsm &Rhs)
{
    m_pChannel = Rhs.m_pChannel;
    memcpy(&m_hObject[0], &Rhs.m_hObject[0], sizeof(m_hObject));
    memcpy(&m_ClassID[0], &Rhs.m_ClassID[0], sizeof(m_ClassID));
    m_pAtomDef = Rhs.m_pAtomDef;
    m_Phase = Rhs.m_Phase;
    m_UnresolvedHeaderSize = Rhs.m_UnresolvedHeaderSize;

    return *this;
}

//--------------------------------------------------------------------
//! \brief Tell whether AtomFsm supports the indicated channel
//!
//! If this method returns false, you should not try to construct an
//! AtomFsm around the indicated channel.
//!
/* static */ bool AtomFsm::IsSupported(Channel *pChannel)
{
    if (pChannel->RmcGetClassEngineId(0, 0, 0, 0) == RC::UNSUPPORTED_FUNCTION)
    {
        return false;
    }
    return true;
}

//--------------------------------------------------------------------
//! Return to a quiescent state, in which there are no pending atoms,
//! but retain the SetObject() information about which class is on
//! which subchannel.
//!
void AtomFsm::CancelAtom()
{
    m_pAtomDef = NULL;
    m_Phase = 0;
    m_UnresolvedHeaderSize = 0;
}

//--------------------------------------------------------------------
//! Reset back to the initial constructor state.
//!
void AtomFsm::Reset()
{
    CancelAtom();
    for (UINT32 ii = 0; ii < Channel::NumSubchannels; ++ii)
    {
        m_hObject[ii] = 0;
        m_ClassID[ii] = LW01_NULL_OBJECT;
    }
}

//--------------------------------------------------------------------
//! \brief Set the object on a subchannel
//!
AtomFsm::Transition AtomFsm::SetObject(UINT32 Subch, LwRm::Handle Handle)
{
    MASSERT(IsValid());
    MASSERT(m_UnresolvedHeaderSize == 0);
    m_pAtomDef = NULL;

    UINT32 ClassId;
    if (m_pChannel->RmcGetClassEngineId(Handle, NULL, &ClassId, NULL) == OK)
    {
        m_hObject[Subch] = Handle;
        m_ClassID[Subch] = ClassId;
    }
    else
    {
        MASSERT(!"GET_CLASS_ENGINEID failed in AtomFsm::SetObject");
    }

    return NOT_IN_ATOM;
}

//--------------------------------------------------------------------
//! \brief Unset the object on a subchannel
//!
AtomFsm::Transition AtomFsm::UnsetObject(LwRm::Handle Handle)
{
    MASSERT(IsValid());
    for (UINT32 Subch = 0; Subch != Channel::NumSubchannels; ++Subch)
    {
        if (m_hObject[Subch] == Handle)
        {
            if (m_pAtomDef != NULL && m_pAtomDef->ClassID == m_ClassID[Subch])
            {
                m_pAtomDef = NULL;
            }
            m_hObject[Subch] = 0;
            m_ClassID[Subch] = LW01_NULL_OBJECT;
        }
    }

    return (m_pAtomDef || m_UnresolvedHeaderSize) ? IN_ATOM : NOT_IN_ATOM;
}

//--------------------------------------------------------------------
//! \brief Update the state machine for a channel method
//!
//! This function updates the state machine for a Channel::Write*(),
//! and returns a Transition telling how the atom state was changed.
//! If a Channel::Write*() is called with N methods, this function
//! must be called N times to update the state.
//!
AtomFsm::Transition AtomFsm::EatMethod
(
    UINT32 Subch,
    UINT32 Method,
    UINT32 Data
)
{
    Transition Returlwalue = EatOrPeekMethod(Subch, Method, Data,
                                             &m_pAtomDef, &m_Phase,
                                             &m_UnresolvedHeaderSize);

    // Sanity check: Make sure that the return value and InAtom()
    // agree with each other.
    //
    switch (Returlwalue)
    {
        case IN_ATOM:
        case IN_NEW_ATOM:
            MASSERT(InAtom());
            break;
        case NOT_IN_ATOM:
        case ATOM_ENDS_AFTER:
            MASSERT(!InAtom());
            break;
    }
    return Returlwalue;
}

//--------------------------------------------------------------------
//! \brief Update the state machine for a channel header
//!
//! This function is similar to EatMethod(), but it updates the state
//! machine to reflect one of the Channel::WriteHeader* functions.
//! The NonInc argument tells whether this is an incrementing or
//! non-incrementing header.
//!
AtomFsm::Transition AtomFsm::EatHeader
(
    UINT32 Subch,
    UINT32 Method,
    UINT32 Count,
    bool NonInc
)
{
    MASSERT(Count > 0);
    MASSERT(m_UnresolvedHeaderSize == 0);

    Transition FirstTransition = EatMethod(Subch, Method, 0);
    for (UINT32 ii = 1; ii < Count; ++ii)
        EatMethod(Subch, NonInc ? Method : (Method + 4*ii), 0);
    m_UnresolvedHeaderSize = 4 * Count;

    if (FirstTransition == IN_ATOM || FirstTransition == ATOM_ENDS_AFTER)
        return IN_ATOM;
    else
        return IN_NEW_ATOM;
}

//--------------------------------------------------------------------
//! \brief Update the state machine for a CallSubroutine
//!
//! This function is similar to EatMethod(), but it updates the state
//! machine to reflect a Channel::CallSubroutine() function.
//!
//! Note that the methods passed into CallSubroutine() are a bit
//! opaque, so this class tends to assume that nothing interesting
//! happens in a CallSubroutine(), aside from anything we can deduce
//! from a WriteHeader() just before CallSubroutine().  So mods
//! implicitly requires that tests can't "hide" methods such as
//! LW9097_BEGIN, LW9097_END, CALL_MME_MACRO, and unresolved headers
//! inside of CallSubroutine().  Any such "hidden" methods won't be
//! recognized as atoms.
//!
AtomFsm::Transition AtomFsm::EatSubroutine(UINT32 Count)
{
    UINT32 OldUnresolvedHeaderSize = m_UnresolvedHeaderSize;

    m_UnresolvedHeaderSize -= min(m_UnresolvedHeaderSize, Count);
    if (m_pAtomDef || m_UnresolvedHeaderSize)
        return IN_ATOM;
    else if (OldUnresolvedHeaderSize > 0)
        return ATOM_ENDS_AFTER;
    else
        return NOT_IN_ATOM;
}

//--------------------------------------------------------------------
//! \brief Return Transition information for the next EatMethod
//!
//! This function allows you to get transition information for the
//! next EatMethod() function, without actually "eating" the method.
//! It returns the same same Transition value as EatMethod() would've
//! returned, but leaves the state machine in (roughly) the same state
//! so that you still need to call EatMethod to update the state.
//!
//! If you call PeekMethod, then the next Eat* function you call
//! should be an EatMethod with the same arguments.
//!
//! Ideally, this should be a const method that doesn't change the
//! state.  But with some atomic operations, you can't tell that the
//! atom has ended until you see the next method.  So, to simplify the
//! callers, we cheat a bit: if a PeekMethod shows that the atom
//! *should* have ended just before the method we're about to
//! Peek/Eat, we update the state to reflect that.
//!
AtomFsm::Transition AtomFsm::PeekMethod
(
    UINT32 Subch,
    UINT32 Method,
    UINT32 Data
)
{
    AtomDef *dummy1 = m_pAtomDef;
    UINT32   dummy2 = m_Phase;
    UINT32   dummy3 = m_UnresolvedHeaderSize;

    Transition Returlwalue = EatOrPeekMethod(Subch, Method, Data,
                                             &dummy1, &dummy2, &dummy3);
    if (Returlwalue == NOT_IN_ATOM || Returlwalue == IN_NEW_ATOM)
    {
        m_pAtomDef = NULL;
    }
    return Returlwalue;
}

//--------------------------------------------------------------------
//! \brief Return Transition information for the next EatHeader
//!
//! This function allows you to get transition information for the
//! next EatHeader, just like PeekMethod returns transition info for
//! the next EatMethod.
//!
AtomFsm::Transition AtomFsm::PeekHeader
(
    UINT32 Subch,
    UINT32 Method,
    UINT32 Count,
    bool NonInc
)
{
    MASSERT(Count > 0);
    MASSERT(m_UnresolvedHeaderSize == 0);

    Transition FirstTransition = PeekMethod(Subch, Method, 0);

    if (FirstTransition == IN_ATOM || FirstTransition == ATOM_ENDS_AFTER)
        return IN_ATOM;
    else
        return IN_NEW_ATOM;
}

//--------------------------------------------------------------------
//! \brief Used to parse atomlist.h
//!
//! This function appends an empty AtomPhase to an AtomDef.  The
//! AtomPhase should get filled with information by subsequent
//! operator,() functions.
//!
AtomFsm::AtomDef &AtomFsm::AtomDef::AddPhase
(
    PhaseType phaseType,
    ParseType parseType
)
{
    AtomPhase NewPhase;
    NewPhase.Type = phaseType;
    NewPhase.Mask = 0;
    NewPhase.Data = 0;
    Phases.push_back(NewPhase);
    LwrrentParseType = parseType;
    return *this;
}

//--------------------------------------------------------------------
//! \brief Used to parse atomlist.h
//!
//! This function overloads the "comma" operator, which allows
//! atomlist.h to specify phase information as a human-readable
//! comma-separated list.  This method inserts one entry from the
//! comma-separated list into the last phase of the AtomDef.
//!
AtomFsm::AtomDef &AtomFsm::AtomDef::operator,(UINT32 Arg)
{
    MASSERT(Phases.size() > 0);
    switch (LwrrentParseType)
    {
        case PARSE_WITHOUT_DATA:
            Phases.back().Methods.insert(Arg);
            break;
        case PARSE_WITH_DATA:
            Phases.back().Data = Arg;
            LwrrentParseType = PARSE_WITH_DATA_1ARG;
            break;
        case PARSE_WITH_DATA_1ARG:
            Phases.back().Mask = Phases.back().Data;
            Phases.back().Data = Arg;
            LwrrentParseType = PARSE_WITH_DATA_NARGS;
            break;
        case PARSE_WITH_DATA_NARGS:
            Phases.back().Methods.insert(Phases.back().Mask);
            Phases.back().Mask = Phases.back().Data;
            Phases.back().Data = Arg;
            break;
        default:
            MASSERT(!"Illegal case in AtomDef::operator,");
    }
    return *this;
}

//--------------------------------------------------------------------
//! Check for errors after an AtomDef is parsed from atomlist.h, and
//! MASSERT if errors are found.
//!
void AtomFsm::AtomDef::FinishParsing()
{
    MASSERT(Phases.size() > 0);
    switch (Phases[0].Type)
    {
        case TYPE_ONE_OF:
            MASSERT(Phases.size() > 1);
            break;
        case TYPE_MANY_OF:
            break;
        default:
            MASSERT(!"Atom must start with ONE_OF or MANY_OF");
    }
    for (vector<AtomPhase>::iterator pPhase = Phases.begin();
         pPhase != Phases.end(); ++pPhase)
    {
        MASSERT(pPhase->Methods.size() > 0);
    }
}

//--------------------------------------------------------------------
//! \brief Initialize the s_AtomDefs table
//!
//! This function is called by the constructor and assignment
//! operator, to initialize s_AtomDefs if there are any AtomFsm
//! objects that need it.  s_AtomDefs is destroyed by ShutDown().
//!
//! The parsing process normally consists of:
//! - 1. Create an AtomDef
//! - 2. Call AtomDef::AddPhase to append a phase
//! - 3. Call the AtomDef comma operator N times to fill the phase
//! - 4. Repeat 2-3 until the AtomDef is parsed.
//! - 5, Call AtomDef::FinishParsing() to check for errors.
//! - 6, Add the AtomDef to AtomFsm::s_AtomDefs
//! - 7. Repeat 1-7 until we reach the end of atomlist.h
//!
void AtomFsm::InitializeAtomDefs()
{
    if (!s_AtomDefs.empty())
    {
        return;
    }

    // If we get this far, then we need to parse atomlist.h.  We do
    // this by creating some macros, and #include'ing atomlist.h.
    //

#define DEFINE_ATOM                             \
    {                                           \
        AtomDef NewAtomDef;                     \
        CommaSepList ClassIDs;                  \
        ClassIDs ,

#define PARSE_COMMAND(PHASE_TYPE, PARSE_TYPE)          \
        ;                                              \
        NewAtomDef.AddPhase(PHASE_TYPE, PARSE_TYPE) ,

#define END_ATOM                                                        \
        ;                                                               \
        NewAtomDef.FinishParsing();                                     \
        for (CommaSepList::iterator pClassID = ClassIDs.begin();        \
             pClassID != ClassIDs.end(); ++pClassID)                    \
        {                                                               \
            NewAtomDef.ClassID = *pClassID;                             \
            set<UINT32> &StartMethods = NewAtomDef.Phases[0].Methods;   \
            for (set<UINT32>::iterator pMethod = StartMethods.begin();  \
                 pMethod != StartMethods.end(); ++pMethod)              \
            {                                                           \
                UINT64 Key = GetKey(NewAtomDef.ClassID, *pMethod);      \
                s_AtomDefs[Key].push_back(NewAtomDef);                  \
            }                                                           \
        }                                                               \
    }

#define DEFINE_ATOMS(NUM_ATOMS)                  \
    for (UINT32 idx = 0; idx < NUM_ATOMS; ++idx) \
    DEFINE_ATOM

#define ONE_OF           PARSE_COMMAND(TYPE_ONE_OF, PARSE_WITHOUT_DATA)
#define MANY_OF          PARSE_COMMAND(TYPE_MANY_OF, PARSE_WITHOUT_DATA)
#define ONE_NOT_OF       PARSE_COMMAND(TYPE_ONE_NOT_OF, PARSE_WITHOUT_DATA)
#define MANY_NOT_OF      PARSE_COMMAND(TYPE_MANY_NOT_OF, PARSE_WITHOUT_DATA)
#define METHOD_WITH_DATA PARSE_COMMAND(TYPE_ONE_OF, PARSE_WITH_DATA)

#define CLASSES_BETWEEN(OLDEST, NEWEST) GetClassList(OLDEST, NEWEST)
#define CLASSES_SINCE(OLDEST) GetClassList(OLDEST, LW01_NULL_OBJECT)

#include "atomlist.h"

#undef DEFINE_ATOM
#undef PARSE_COMMAND
#undef END_ATOM
#undef DEFINE_ATOMS
#undef ONE_OF
#undef MANY_OF
#undef ONE_NOT_OF
#undef MANY_NOT_OF
#undef METHOD_WITH_DATA
}

//--------------------------------------------------------------------
//! \brief Return a list of all classes from OldestClass to NewestClass.
//!
//! Used to parse the CLASSES_SINCE and CLASSES_BETWEEN directives in
//! atomlist.h.  All returned classes will be of the same type (3D,
//! compute, etc), as defined by the lists in gralloc.h.
//!
//! If NewestClass is LW01_NULL_OBJECT, it means to use the latest
//! class in gralloc.h.
//!
/* static */ vector<UINT32> AtomFsm::GetClassList
(
    UINT32 OldestClass,
    UINT32 NewestClass
)
{
    // Lwrrently only supports the following class lists.  If they add
    // other types of classes to atomlist.h, then add the appropriate
    // functions below.
    //
    ThreeDAlloc threeDAlloc;
    ComputeAlloc computeAlloc;
    Inline2MemoryAlloc inline2MemoryAlloc;

    GrClassAllocator *grallocs[] =
    {
        &threeDAlloc,
        &computeAlloc,
        &inline2MemoryAlloc
    };

    for (UINT32 ii = 0; ii < NUMELEMS(grallocs); ++ii)
    {
        if (grallocs[ii]->HasClass(OldestClass))
        {
            grallocs[ii]->SetOldestSupportedClass(OldestClass);
            if (NewestClass != LW01_NULL_OBJECT)
            {
                grallocs[ii]->SetNewestSupportedClass(NewestClass);
            }
            const UINT32 *pClassList;
            UINT32 numClasses;
            grallocs[ii]->GetClassList(&pClassList, &numClasses);
            return vector<UINT32>(pClassList, pClassList + numClasses);
        }
    }
    MASSERT(!"Illegal class in AtomFsm::GetClassList");
    return vector<UINT32>();
}

//--------------------------------------------------------------------
//! \brief Generate a 64-bit key from a class/method pair.
//!
//! s_AtomDefs stores the AtomDefs by key.  This is done to speed up
//! the task of finding out whether a given class/method is the start
//! of a new atom.  An STL map<> lookup takes O(log n) time, so by
//! storing s_AtomDefs as a map which is keyed off the first method in
//! the atom, we can quickly look up the AtomDef(s) that could start
//! with a given method.
//!
/* static */ UINT64 AtomFsm::GetKey
(
    UINT32 ClassID,
    UINT32 Method
)
{
    return (((UINT64)ClassID) << 32) | Method;
}

//--------------------------------------------------------------------
//! \brief Used to implement EatMethod and PeekMethod
//!
//! This function is the heart of EatMethod and PeekMethod.  It takes
//! the same args as EatMethod & PeekMethod, plus three args that
//! correspond to the state of the state machine: ppAtomDef, pPhase,
//! and pUnresolvedHeader.
//!
//! When called by EatMethod, the three "state" args point at the
//! corresponding data in the AtomFsm object, so that this method
//! updates the actual state.  When called by PeekMethod, those three
//! args point at local variables, so that this method does not update
//! the actual state.
//!
AtomFsm::Transition AtomFsm::EatOrPeekMethod
(
    UINT32 Subch,
    UINT32 Method,
    UINT32 Data,
    AtomDef **ppAtomDef,
    UINT32 *pPhase,
    UINT32 *pUnresolvedHeaderSize
) const
{
    MASSERT(IsValid());
    MASSERT(Subch < Channel::NumSubchannels);
    MASSERT(ppAtomDef != NULL);
    MASSERT(pPhase != NULL);
    MASSERT(pUnresolvedHeaderSize != NULL);
    MASSERT(*pUnresolvedHeaderSize == 0);

    // Handle non-engine methods
    //
    if (Method < MIN_ENGINE_METHOD)
    {
        return *ppAtomDef ? IN_ATOM : NOT_IN_ATOM;
    }

    // End the current atom if we start writing to another class
    //
    if (*ppAtomDef && (*ppAtomDef)->ClassID != m_ClassID[Subch])
    {
        *ppAtomDef = NULL;
    }

    // Advance the phase of the current atom.  Return with IN_ATOM or
    // ATOM_ENDS_AFTER if this method is still part of the previous
    // atom.
    //
    while (*ppAtomDef)
    {
        const AtomPhase &Phase = (*ppAtomDef)->Phases[*pPhase];
        bool Matches = (Phase.Methods.count(Method) > 0 &&
                        ((Data & Phase.Mask) == Phase.Data));

        switch (Phase.Type)
        {
            case TYPE_ONE_NOT_OF:
                Matches = !Matches;
                // FALLTHRU
            case TYPE_ONE_OF:
                if (Matches)
                {
                    ++(*pPhase);
                    if (*pPhase < (*ppAtomDef)->Phases.size())
                    {
                        return IN_ATOM;
                    }
                    else
                    {
                        *ppAtomDef = NULL;
                        return ATOM_ENDS_AFTER;
                    }
                }
                else
                {
                    *ppAtomDef = NULL;
                    // Continue below to check for a new atom
                }
                break;
            case TYPE_MANY_NOT_OF:
                Matches = !Matches;
                // FALLTHRU
            case TYPE_MANY_OF:
                if (Matches)
                {
                    return IN_ATOM;
                }
                else
                {
                    ++(*pPhase);
                    if (*pPhase >= (*ppAtomDef)->Phases.size())
                    {
                        *ppAtomDef = NULL;
                    }
                    // Continue to the next phase or to check for new atom
                }
                break;
            default:
                MASSERT(!"Illegal case in AtomFsm::EatMethod");
        }
    }

    // If we get this far, we're no longer in the previous atom (if any).
    // Check for the start of a new atom.
    //
    UINT64 Key = GetKey(m_ClassID[Subch], Method);
    if (s_AtomDefs.count(Key) > 0)
    {
        vector<AtomDef> &AtomDefs = s_AtomDefs[Key];
        for (vector<AtomDef>::iterator pAtomDef = AtomDefs.begin();
             pAtomDef != AtomDefs.end(); ++pAtomDef)
        {
            if (pAtomDef->ClassID == m_ClassID[Subch] &&
                pAtomDef->Phases[0].Methods.count(Method) > 0 &&
                (Data & pAtomDef->Phases[0].Mask) == pAtomDef->Phases[0].Data)
            {
                *ppAtomDef = &(*pAtomDef);
                *pPhase = (pAtomDef->Phases[0].Type == TYPE_MANY_OF) ? 0 : 1;
                return IN_NEW_ATOM;
            }
        }
    }

    return NOT_IN_ATOM;
}
