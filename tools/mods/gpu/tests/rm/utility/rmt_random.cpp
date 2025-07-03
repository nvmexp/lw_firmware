/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/types.h"
#include "core/include/tee.h"
#include "core/include/cpu.h"
#include "core/include/platform.h"
#include "rmtestutils.h"
#include "gpu/tests/rmtest.h"
#include "rmt_random.h"
#include "core/include/memcheck.h"

using namespace rmt;

//-----------------------------------------------------------------------------
Randomizable::Deleter Randomizable::deleter;

//! Randomize and create this resource,
//! with possibility of recoverable failure (negative test cases).
//!
//! Calls Randomize() then Create().
//!
//! Expected return status from Create() depends on the results of
//! CheckValidity() and CheckFeasibility().
//!
//! If invalid,             Create() *must* fail.
//! If valid and infeasible Create() *may*  fail.
//! If valid and   feasible Create() *must* pass.
//!
//! +-------+----------+-----------------+
//! | Valid | Feasible | Expected Result |
//! +-------+----------+-----------------+
//! | Yes   | Yes      |  OK             |
//! | Yes   | No       |  OK or !OK      |
//! | No    | Yes      | !OK             |
//! | No    | No       | !OK             |
//! +-------+----------+-----------------+
//!
//! @note Ideally RM would return error codes specific
//!       to the reason for ilwalidity and we could
//!       check for the expected error code instead of
//!       a blind pass/fail.
//!
//!       This would enforce better coverage and
//!       stricter maintenance of the test
//!       (e.g. to cover new negative cases).
//!
//!       This would require additional test complexity.
//!       Either the order of validity checks needs to be
//!       deterministic or the checks need to be
//!       extended to a set of potential error codes
//!       instead of a single error code.
//!
//-----------------------------------------------------------------------------
RC Randomizable::CreateRandom(const UINT32 seed)
{
    RC rc;

    // Randomize and check validity/feasibility.
    Randomize(seed);
    const char *invalid = CheckValidity();
    const char *infeasible = CheckFeasibility();

    // Print configuration details for debug.
    Printf(Tee::PriHigh, "Seed 0x%08X [%s] %s(%u):\n", seed,
           invalid ? "INVALID" : (infeasible ? "INFEASIBLE" : "VALID"),
           GetTypeName(), GetID());
    PrintParams(Tee::PriHigh);
    if (invalid)
    {
        Printf(Tee::PriHigh, "REASON INVALID: %s\n", invalid);
    }
    else if (infeasible)
    {
        Printf(Tee::PriHigh, "REASON INFEASIBLE: %s\n", infeasible);
    }

    // Wrap internal creation with conditional breakpoint disablement.
    DISABLE_BREAK_COND(nobp, invalid || infeasible);
    rc = Create();
    DISABLE_BREAK_END(nobp);

    // Check for expected results.
    if (invalid)
    {
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "Ignoring invalid test case error: %s\n",
                   rc.Message());
            rc.Clear();
        }
        else
        {
            Printf(Tee::PriHigh, "Negative test failed\n");
            rc = RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        if (rc == OK)
        {
            Printf(Tee::PriHigh, "Seed 0x%08X [RESULT] %s(%u):\n", seed,
                   GetTypeName(), GetID());
            PrintParams(Tee::PriHigh);
        }
        else if (infeasible)
        {
            Printf(Tee::PriHigh, "Ignoring infeasible test case error: %s\n",
                   rc.Message());
            rc.Clear();
        }
    }

    return rc;
}

//! Log destruction of a random resource.
//-----------------------------------------------------------------------------
void Randomizable::Deleter::operator ()(Randomizable *pObj)
{
    Printf(Tee::PriHigh, "Destroying %s(%u)\n",
           pObj->GetTypeName(), pObj->GetID());
    delete pObj;
}

