/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_sampleone.cpp
//! \brief Example for RmTest framework.  See bug 217014
//!
//! Example of what an RmTest should look like.  Yes, each function should
//! have some documentation that doxygen can parse
//!
//! Also, try to only include headers you actually need.  And *really* be
//! careful about what headers you include from other header files.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "core/include/memcheck.h"

class SampleRmTestOne : public RmTest
{
public:
    SampleRmTestOne();
    virtual ~SampleRmTestOne();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    //@{
    //! Test specific functions
    SETGET_PROP(NumElems, UINT32);
    SETGET_PROP(SampleText, string);
    SETGET_PROP(Pass, bool);
    //@}

private:
    //@{
    //! Test specific functions
    void ResetState();
    RC DidTheUserWantTheTestToFail();
    //@}

    //@}
    //! Test specific variables
    UINT32  m_NumElems; //!< Example of a test specific variable hooked to JS
    UINT32 *m_Elems;  //!< Just so we have something to allocate in this example
    string  m_SampleText; //!< Example of a test specific variable hooked to JS
    bool    m_Pass; //!< Example of a test specific variable hooked to JS

    FancyPicker::FpContext m_FpCtx; //!< Random # and loop context for the test
    //@}
};

//! \brief Example constructor.  Don't do anything complex in constructors...
//!
//! In general constructors should be used to clear variables, setup intial
//! SENTINEL values, etc.  Don't do anything that can fail in a constructor.
//! MODS doesn't support exceptions, so there's no way to report a problem other
//! than hacking code that gets called later on to detect the error.  Much
//! cleaner to do stuff that can fail in the Setup member function.
//!
//! \sa Setup
//------------------------------------------------------------------------------
SampleRmTestOne::SampleRmTestOne()
{
    SetName("SampleRmTestOne");
    ResetState();
}

//! \brief Example destructor.  Should be redundant with Cleanup
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
SampleRmTestOne::~SampleRmTestOne()
{
    MASSERT(m_Elems == NULL);
}

//! \brief Whether or not the test is supported in the current environment
//!
//! IsTestSupported advertises whether or not the test should be ilwoked in the
//! current environment.  For example, if I'm writing a test that uses a
//! particular set of HW classes, the test IsTestSupported in the case where
//! RM advertises that the HW supports the classes that I need.
//!
//! This particular sample test is supported in all elwironments, so it always
//! returns RUN_RMTEST_TRUE.
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string SampleRmTestOne::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test
//!
//! Remember how we couldn't do anything fancy in the constructor?  Well now
//! is your chance.  Setup should prepare all state that the Run function needs
//! to execute properly.  Care should be taken so that Cleanup works properly
//! in the case where Setup bails out early (for example due to a resource
//! allocation failure...init pointers to NULL in the ctor so you're not freeing
//! a wild pointer in Cleanup).
//!
//! Because Setup reserves all resources that a test needs "up front", multiple
//! tests can be Setup serially then kicked off (Run) in parallel
//!
//! \return RC -> OK if the test can be Run, test-specific RC if something
//!         failed while selwring resources
//------------------------------------------------------------------------------
RC SampleRmTestOne::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    if(m_NumElems == 0)
    {
        Printf(Tee::PriHigh, "ERROR: SampleRmTestOne expected NumElems > 0!\n");
        return RC::ILWALID_INPUT;
    }

    m_Elems = new UINT32[m_NumElems];

    if(m_Elems == NULL)
    {
        Printf(Tee::PriHigh, "ERROR: Couldn't allocate memory for example array!\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    return rc;
}

//! \brief Run the test!
//!
//! This test doesn't really test anything, so we'll just look at the m_Pass
//! variable we've hooked up to JS so that PASS/FAIL can be controlled from
//! script.  Not that it needs mentioning, but please use good programming style
//! and decomp your functions into subroutines as appropriate
//!
//! \return OK if the test passed, test-specific RC if it failed
//------------------------------------------------------------------------------
RC SampleRmTestOne::Run()
{
    RC rc;
    GpuTestConfiguration *pTstCfg = GetTestConfiguration();
    MASSERT(pTstCfg);

    UINT32 startLoop = pTstCfg->StartLoop();
    UINT32 endLoop   = startLoop + pTstCfg->Loops();

    Printf(Tee::PriNormal, "SampleRmTestOne run with %d elems and '%s' text\n",
        m_NumElems, m_SampleText.c_str());

    m_FpCtx.pJSObj = GetJSObject(); // Not really needed for this test
    m_FpCtx.Rand.SeedRandom(pTstCfg->Seed());

    for(m_FpCtx.LoopNum = startLoop; m_FpCtx.LoopNum < endLoop; ++m_FpCtx.LoopNum)
    {
        // Print some random numbers just for fun...this would be test code
        // in a real test
        UINT32 lwrLoop = m_FpCtx.LoopNum;
        UINT32 resultForThisLoop = m_FpCtx.Rand.GetRandom(startLoop, lwrLoop);

        Printf(Tee::PriLow, "Result for loop %d: %d\n", lwrLoop,
            resultForThisLoop);
    }

    // Check for errors.  This is a sample test so I gave this function
    // a silly name...
    CHECK_RC(DidTheUserWantTheTestToFail());

    return rc;
}

//! \brief Free any resources that the test selwred
//!
//! Cleanup all resources that the test selwred no matter what happened
//! previously.  As mentioned in Setup, Cleanup can be called in a variety of
//! different scenarios...anything that can fail in Setup or Run could be a
//! valid "current state" when Cleanup is called.
//!
//! Note that Cleanup doesn't return anything.  Cleanup takes its lwe from
//! "free" (C standard library function).  Releasing resources should always
//! succeed in a sane system.  Unlike allocation, failure on free isn't
//! something that a client can reasonably handle.  Take the following C
//! example where we pretend "free" returns 0 on success and non-zero on failure
//!
//! \verbatim
//! char * ptr = malloc(something);
//! if(ptr == NULL)
//! {
//!     ptr = TryFallback();
//!     if(ptr == NULL)
//!         return CANNOT_ALLOCATE_MEMORY;
//! }
//!
//! ...
//!
//! int retVal = free(ptr);
//! if(retVal)
//! {
//!    // Umm...now what.  The details of free are hidden from us.
//! }
//! \endverbatim

//------------------------------------------------------------------------------
RC SampleRmTestOne::Cleanup()
{
    delete [] m_Elems;
    ResetState();

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief Reset member variables to default values
//------------------------------------------------------------------------------
void SampleRmTestOne::ResetState()
{
    m_NumElems = 0;
    m_Elems = NULL;
    m_SampleText = "Sample";
    m_Pass = false;
}

//! \brief The "check the results" part of this fake test
//!
//! Normally you'd be testing something so you wouldn't have to contrive
//! a way for your test to PASS/FAIL.  In this case I just check a JS property
//! (which is set before the test runs) to determine whether the test
//! passed.  I return GOLDEN_VALUE_MISCOMPARE because we're "comparing" against
//! "true", in a real test you'd want to use a meaningful error code from
//! errors.h.
//!
//! BTW, if you're drawing something you should probably check out the
//! Goldelwalues object.
//!
//! \sa Goldelwalues (if you're drawing something)
//!
//! \return OK if the test passed, GOLDEN_VALUE_MISCOMPARE
//------------------------------------------------------------------------------
RC SampleRmTestOne::DidTheUserWantTheTestToFail()
{
    if (m_Pass) return OK;

    return RC::GOLDEN_VALUE_MISCOMPARE;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SampleRmTestOne, RmTest, "Sample RM test number 1");
CLASS_PROP_READWRITE(SampleRmTestOne, NumElems, UINT32,
    "Number of 'elems' to allocate in this sample test");
CLASS_PROP_READWRITE(SampleRmTestOne, SampleText, string,
    "Text string to print in this test as an example of passing text from JS");
CLASS_PROP_READWRITE(SampleRmTestOne, Pass, bool,
    "Control test pass/fail from JS for this sample test");
