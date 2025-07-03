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
//! \brief Implement a hierarchy of potential events, used by the gilder

#include "pmpotevt.h"
#include "gpu/include/gpudev.h"
#include "pmchan.h"
#include "pmevent.h"
#include "pmutils.h"
#include "core/include/utility.h"

//--------------------------------------------------------------------
//! \brief destructor
//!
PmPotentialEvent::~PmPotentialEvent()
{
}

//////////////////////////////////////////////////////////////////////
// PmPotential_NonStallInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmPotential_NonStallInt::PmPotential_NonStallInt
(
    PmNonStallInt *pNonStallInt
) :
    PmPotentialEvent(NON_STALL_INT),
    m_pNonStallInt(pNonStallInt)
{
    MASSERT(pNonStallInt != NULL);
}

//--------------------------------------------------------------------
//! \brief Implements base-class method
//!
/* virtual */ string PmPotential_NonStallInt::GetStartGildString() const
{
    return "REQUEST_NON_STALL_INT " + m_pNonStallInt->ToString();
}

//--------------------------------------------------------------------
//! \brief Implements base-class method.
//!
//! Prints "" because the potential interrupt was ended when the
//! interrupt oclwrred, which was already recorded in the gild file.
//!
/* virtual */ string PmPotential_NonStallInt::GetEndGildString() const
{
    return "";
}

//--------------------------------------------------------------------
//! \brief Implements base-class method.
//!
/* virtual */ bool PmPotential_NonStallInt::Matches
(
    const PmEvent *pEvent
) const
{
    MASSERT(pEvent != NULL);

    if (pEvent->GetType() != PmEvent::NON_STALL_INT)
        return false;
    const PmEvent_NonStallInt *pNonStallIntEvent =
        static_cast<const PmEvent_NonStallInt*>(pEvent);

    return (m_pNonStallInt == pNonStallIntEvent->GetNonStallInt());
}

//--------------------------------------------------------------------
//! \brief Implements base-class method.
//!
/* virtual */ bool PmPotential_NonStallInt::Matches
(
    const PmPotentialEvent *pRhs
) const
{
    MASSERT(pRhs != NULL);

    if (pRhs->GetType() != NON_STALL_INT)
        return false;
    const PmPotential_NonStallInt &rhs =
        *static_cast<const PmPotential_NonStallInt*>(pRhs);

    return (m_pNonStallInt == rhs.m_pNonStallInt);
}

//--------------------------------------------------------------------
//! \brief Implements base-class method.
//!
/* virtual */ bool PmPotential_NonStallInt::MatchesStart
(
    const string &gildString
) const
{
    return gildString == GetStartGildString();
}

//--------------------------------------------------------------------
//! \brief Implements base-class method.
//!
/* virtual */ bool PmPotential_NonStallInt::MatchesEnd
(
    const string &gildString
) const
{
    return gildString == "";
}
