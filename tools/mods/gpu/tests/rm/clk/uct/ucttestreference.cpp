/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ucttestreference.h"
#include "uctexception.h"
#include "gpu/include/gpusbdev.h"

// Implementation of classes inheriting from TestReference are found
// in their own source files

uct::ExceptionList uct::TestReference::resolve(
    const rmt::FileLocation             &location,
    rmt::String                          ref,
    const VbiosPStateArray              &vbiosPStateMap,
    const TestSpec::NameMap             &testSpecMap)
{
    rmt::String shard;
    bool bInitPState;
    ExceptionList status;

    // Remember the file location of the field in the trial spec.
    file = location.file;
    line = location.line;

    // Clear any existing state.
    base.ilwalidate();
    extension.clear();

    // The first element should be the VBIOS P-State
    shard = ref.extractShard(':');

    // Is this a init PState?
    rmt::String c = VbiosPState::initPStateName;
    c.lcase();
    bInitPState = (shard.compare(c) == 0);

    //
    // VBIOS P-States have the form "vbiosp" <number>.  Complain if the reference
    // is neither a VBIOS P-State nor a init pstate
    //
    if (!shard.startsWith("vbiosp") && !bInitPState)
        status.push(SolverException(shard.defaultTo("[empty]") +
            ": Base of a p-state reference must be either a VBIOS PState or " +
            VbiosPState::initPStateName, location, __PRETTY_FUNCTION__));

    // Determine the number used
    else
    {
        if (!bInitPState)
        {
            // Discard the text "vbios" and extract a (presumably decimal) number.
            rmt::String s = shard;
            s.erase(0, 6);
            long n = s.extractLong();

            // P-State nunber must be and integer and must not have extraneous text
            if (errno || !s.empty())
                status.push(SolverException(shard.defaultTo("[empty]") +
                    ": Invalid VBIOS p-state index", location, __PRETTY_FUNCTION__));

            // This VBIOS does not have the requested VBIOS defined.
            else if(!vbiosPStateMap.isValid((VbiosPStateArray::Index) n))
            {
                Printf(Tee::PriHigh, shard +": Undefined VBIOS p-state - vbiosp%ld, therefore skipping\n",
                      n, __PRETTY_FUNCTION__);
                return Exception("Undefined VBIOS P-state, therefore skipping", __PRETTY_FUNCTION__, Exception::Unsupported);
            }

            // Remember the base VBIOS p-state
            else
                base = vbiosPStateMap[n];
        }
        else
            base = vbiosPStateMap.initPState;
    }

    // Now for the extensions
    while (!ref.empty())
    {
        // Look for the colon-delimited partial p-state name
        shard = ref.extractShard(':');

        TestSpec::NameMap::const_iterator pTestSpec = testSpecMap.find(shard);

        // None found, so create an exception.
        if (pTestSpec == testSpecMap.end())
            status.push(SolverException(shard.defaultTo("[empty]") +
                ": Undefined Test Spec", location, __PRETTY_FUNCTION__));

        // Add to the extension list.
        else
        {
            // Check that the extension is a sine test, otherwise we should
            // disable synchronized iteration
            if(pTestSpec->second->type != TestSpec::SineSpec)
                isSynchronizedPossible = false;

            extension.push_back(pTestSpec->second);
        }
    }

    return status;
}

//! \brief      Computes the cardinality of all test references
unsigned long long uct::TestReference::Vector::cardinality() const
{
    unsigned long long c = 0;
    const_iterator ppReference;

    // Simply go through all the references in the vector and
    // ask how many iterations each of them has
    for (ppReference = begin();
         ppReference!= end(); ++ppReference)
    {
        c += (*ppReference)->cardinality();
    }

    return c;
}

//! \brief      Generate the i-th full pstate from the entire test
//!             reference pool
uct::ExceptionList uct::TestReference::Vector::fullPState(
    FullPState &fullPState,
    unsigned long long i,
    FreqDomainBitmap freqMask) const
{
    const_iterator ppReference;
    unsigned long long c;

    // Iterate through the references starting with the first.
    for (ppReference = begin();
         ppReference!= end(); ++ppReference)
    {
        MASSERT(*ppReference);
        TestReference &ref = **ppReference;

        // Return if this reference contains the current (i-th) iteration
        if (i < (c = ref.cardinality()))
            return ref.fullPState(fullPState, i, freqMask);
        else
            i -= c;
    }

    // If we exited the loop without hitting any full state then it means
    // that the iteration setting is set too large
    return Exception("'i' parameter is too large, check the end setting for this trial spec",
        fullPState.start, __PRETTY_FUNCTION__, Exception::Fatal);
}

uct::ExceptionList uct::TestReference::fullPState(
    uct::FullPState &fullPState,
    unsigned long long i,
    FreqDomainBitmap freqMask) const
{
    TestSpec::Vector::const_iterator pTestSpec;
    unsigned long long c = 0;
    ExceptionList status;

    Printf(Tee::PriHigh, "TestReference[synchronized=%u]::fullPState iteration %llu\n", isSynchronizedPossible, i);

    // Start with the base VBIOS p-state
    fullPState = base;

    // Assign the file and line of this sine reference so that errors
    // can be traced back to ctp file and line
    fullPState.start.file = file;
    fullPState.start.line = line;

    // For the current base pstate, apply each extension in turn
    if(isSynchronizedPossible)
    {
        // If we can apply the same i to all extensions (i.e. synchronized) then we will
        for(pTestSpec = extension.begin(); pTestSpec != extension.end(); ++pTestSpec)
            status.push((*pTestSpec)->fullPState(fullPState, i, freqMask));
    }
    else
    {
        // Else we will use permutation
        for(pTestSpec = extension.begin(); pTestSpec != extension.end(); ++pTestSpec)
        {
            c = (*pTestSpec)->cardinality();
            status.push((*pTestSpec)->fullPState(fullPState, i % c, freqMask));
            i /= c;
        }

        //
        // We would have consumed the index if it were in range.
        // If this assertion fires, it probably means that TrialSpec::Iterator::fullPState
        // was called after the iterator found the end of the trial spec.
        //
        MASSERT(!i);
    }

    if(!status.isOkay())
        fullPState.ilwalidate();

    return status;
}

unsigned long long uct::TestReference::cardinality() const
{
    unsigned long long c = 1;
    TestSpec::Vector::const_iterator pTestSpec;

    if(isSynchronizedPossible)
    {
        // If extensions can be synchronized, then we will return the largest
        // count among the extensions
        for(pTestSpec = extension.begin(); pTestSpec != extension.end(); ++pTestSpec)
        {
            if ((*pTestSpec)->cardinality() > c)
                c = (*pTestSpec)->cardinality();
        }
    }
    else
    {
        // Otherwise we will compute the # of permutations available
        for(pTestSpec = extension.begin(); pTestSpec != extension.end(); ++pTestSpec)
            c *= (*pTestSpec)->cardinality();
    }

    return c;
}

