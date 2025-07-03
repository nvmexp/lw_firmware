/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2013,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTTESTREFERENCE_H_
#define UCTTESTREFERENCE_H_

#include <memory>
#include <vector>

#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctexception.h"
#include "uctdomain.h"
#include "uctpstate.h"
#include "ucttestspec.h"

namespace uct{

    //!
    //! \brief      Test Reference
    //!
    //! \details    Each test reference uses a fully defined vbios pstate, followed
    //!             by zero or more partial pstate (see pstate field of trial specification)
    //!             and zero or more sine tests (see sine field in trial specification)
    //!
    struct TestReference: public rmt::FileLocation
    {
        //! \brief      Forces assignment of type on construction
        inline TestReference()
        {
            // Assume we can synchronize unless resolve detects otherwise
            isSynchronizedPossible = true;
        }

        //! \brief      Default destructor
        virtual ~TestReference() {  };

        //! \brief      Vector of test references
        struct Vector: public std::vector<shared_ptr<TestReference>>
        {
            //!
            //! \brief      Total cardinality of all references
            //!
            //! \details    This function returns the sum of the cardinalities
            //!             of each p-state reference in this vector.  In effect,
            //!             this is the number of fully-defined p-states which
            //!             can be generated.
            //!
            //!             Since each p-state reference represents at least one
            //!             fully-defined p-state (its vBIOS base), zero is
            //!             returned only if the vector is empty.
            //!
            unsigned long long cardinality() const;

            //!
            //! \brief      'i'th fully-defined p-state
            //!
            //! \retval     Invalid 'FullPState' if 'i' is out of range
            //!
            ExceptionList fullPState(
                FullPState &fullPState,
                unsigned long long i,
                FreqDomainBitmap freqMask) const;
        };

        //! \brief      Base VBIOS P-States
        FullPState base;

        //!
        //! \brief      A test reference can compute iterations/cadinality in two
        //!             different modes, this determines which mode can be used
        //!
        //! \details    When a test reference contain only sine tests, at each
        //!             iteration each extension is given the same 'i', essentially
        //!             synchronizing the 't' in sine test so that sine spec sees the
        //!             same t.
        //!
        //!             Whenever a test reference contains a partial pstate reference
        //!             (whether its all partial pstate or not), then permutation
        //!             mode will be used (the original mode of UCT)
        //!
        bool isSynchronizedPossible;

        //!
        //! \brief      Holds the extenstions (separated by :) this test reference has
        //!
        //! \details    During parse time, TestFields parse its test list into
        //!             referenceTextList, these items are then solved for in
        //!             TestReference::resolve, which creates a TestReference for
        //!             each text, breaks up the extensions by ':' and inserts the
        //!             pointer of the TestSpec referred to by the text into extension
        //!
        TestSpec::Vector extension;

        //! \brief      Given a test reference text, resolve to a list of
        //!             extensions containing TestSpec objects
        ExceptionList resolve(
            const rmt::FileLocation             &location,
            rmt::String                          ref,
            const VbiosPStateArray              &vbiosPStateMap,
            const TestSpec::NameMap             &testSpecMap);

        //! \brief      Method a child class must implement, method returns
        //!             number of iterations the reference contains
        unsigned long long cardinality() const;

        //! \brief      Generate a full pstate from the reference
        ExceptionList fullPState(
            FullPState &pstate,
            unsigned long long i,
            FreqDomainBitmap freqMask) const;
    };
}
#endif

