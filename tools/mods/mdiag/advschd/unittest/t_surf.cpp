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

#include "t_surf.h"
#include "pm_regis.h"
#include "pm_utils.h"
#include "pm_surf.h"
#include "pm_regex.h"

using namespace PolicyManager;
using namespace PolicyManager_UnitTest;

// /////////////////////////////////////////////////////////////////////////////
// Abstract_TestCase
// /////////////////////////////////////////////////////////////////////////////

Abstract_TestCase::Abstract_TestCase
(
    Tee::Priority pri,
    const string& name
)
: m_Pri(pri), m_Name(name), m_Description("")
{
}

Abstract_TestCase::~Abstract_TestCase()
{
}

Tee::Priority
Abstract_TestCase::GetPriority() const
{
    return m_Pri;
}

string
Abstract_TestCase::GetName() const
{
    return m_Name;
}

string
Abstract_TestCase::GetDescription() const
{
    return m_Description;
}

void
Abstract_TestCase::SetDescription(const string& des)
{
    m_Description = des;
}

// /////////////////////////////////////////////////////////////////////////////
// Abstract_TestSuite
// /////////////////////////////////////////////////////////////////////////////

Abstract_TestSuite::Abstract_TestSuite
(
    Tee::Priority testSuite_pri,
    Tee::Priority testCase_pri,
    const string& name
)
: m_TestSuite_Pri(testSuite_pri), m_TestCase_Pri(testCase_pri),
    m_Name(name), m_TestCase_Passed(0), m_TestCase_Exec(0)
{
}

Abstract_TestSuite::~Abstract_TestSuite()
{
}

Tee::Priority
Abstract_TestSuite::GetTestSuitePriority() const
{
    return m_TestSuite_Pri;
}

Tee::Priority
Abstract_TestSuite::GetTestCasePriority() const
{
    return m_TestCase_Pri;
}

string
Abstract_TestSuite::GetName() const
{
    return m_Name;
}

// -----------------------------------------------------------------------------

void
Abstract_TestSuite::GetResult
(
    UINT32 *testCase_Passed,
    UINT32 *testCase_Exec) const
{
    *testCase_Passed = m_TestCase_Passed;
    *testCase_Exec = m_TestCase_Exec;

}

// -----------------------------------------------------------------------------

void
Abstract_TestSuite::ExelwteTestCase
(
    Abstract_TestCase* pAtc
)
{
    // update the results
    m_TestCase_Exec++;

    Printf(GetTestSuitePriority(), "%s\nExelwting test case #%d (%s)\n",
           string(80, '-').c_str(),
           m_TestCase_Exec,
           pAtc->GetDescription().c_str());

    // call run on the TestCase and count the result
    bool testCase_Result = false;
    RC rc = pAtc->Run(&testCase_Result);

    //
    if (testCase_Result && rc == OK)
    {
        Printf(GetTestSuitePriority(), "Test case #%d passed\n",
               m_TestCase_Exec);
        m_TestCase_Passed++;
    }
    else
    {
        Printf(GetTestSuitePriority(), "Test case #%d *failed* (%s)\n",
               m_TestCase_Exec,
               pAtc->GetDescription().c_str());
    }
}

// /////////////////////////////////////////////////////////////////////////////
// UnitTest
// /////////////////////////////////////////////////////////////////////////////

PolicyManager_UnitTest::UnitTest *UnitTest::UnitTest_Singleton = 0;

PolicyManager_UnitTest::UnitTest*
PolicyManager_UnitTest::UnitTest::Instance()
{
    if (UnitTest_Singleton == 0)
    {
        UnitTest_Singleton = new UnitTest(Tee::PriNormal,
                                          Tee::PriNormal,
                                          Tee::PriNone);
    }
    return UnitTest_Singleton;
}

// -----------------------------------------------------------------------------

//! set the object to test and reset the counters
PolicyManager_UnitTest::UnitTest::
UnitTest
(
    Tee::Priority unitTest_Pri,
    Tee::Priority testSuite_Pri,
    Tee::Priority testCase_Pri
) :
    m_UnitTest_Pri(unitTest_Pri),
    m_TestSuite_Pri(testSuite_Pri),
    m_TestCase_Pri(testCase_Pri),
    m_TestSuite_Passed(0),  m_TestSuite_Exec(0),
    m_TestCase_Passed(0), m_TestCase_Exec(0)
{
    // add the Surface_TestSuite
    AddTestSuite(new Surface_TestSuite(m_TestSuite_Pri, testCase_Pri));
}

// -----------------------------------------------------------------------------

PolicyManager_UnitTest::UnitTest::
~UnitTest()
{
    Shutdown();
}

// -----------------------------------------------------------------------------

void
PolicyManager_UnitTest::UnitTest::
Shutdown()
{
    for (TestSuites::iterator it = m_TestSuites.begin();
          it != m_TestSuites.end(); ++it)
    {
        delete (*it);
    }
    m_TestSuites.clear();
}

// -----------------------------------------------------------------------------

void
PolicyManager_UnitTest::UnitTest::
AddTestSuite(Abstract_TestSuite *pts)
{
    m_TestSuites.push_back(pts);
}

// -----------------------------------------------------------------------------

//! Execute the object test
RC
PolicyManager_UnitTest::UnitTest::
Run()
{
    UINT32 testSuiteNum =
        static_cast<UINT32>(m_TestSuites.size());
    for (UINT32 i = 0; i < testSuiteNum; ++i)
    {
        m_TestSuite_Exec++;

        // current TestSuite
        Abstract_TestSuite *pAts = m_TestSuites[i];

        Printf(m_UnitTest_Pri, "%s\n", string(80, '-').c_str() );
        Printf(m_UnitTest_Pri, "TestSuite= \"%s\" (%d / %d)\n",
               pAts->GetName().c_str(),
               m_TestSuite_Exec,
               testSuiteNum);
        Printf(m_UnitTest_Pri, "%s\n", string(80, '-').c_str() );

        // call Run on the TestSuite
        RC testSuite_rc = pAts->Run();

        // get the results of the current TestSuite
        UINT32 testCase_Exec = 0;
        UINT32 testCase_Passed = 0;
        pAts->GetResult(&testCase_Exec,
                        &testCase_Passed);
        bool testSuite_Result = (testCase_Passed == testCase_Exec);

        // print stats about this test suite
        Printf(m_UnitTest_Pri,
               "TestCases in TestSuite \"%s \" (%d / %d): "
               "passed / exelwted = %d / %d -> result= %s, "
               "rc= %s",
               pAts->GetName().c_str(),
               m_TestSuite_Exec,
               testSuiteNum,
               testCase_Passed,
               testCase_Exec,
               Bool_To_String(testSuite_Result).c_str(),
               testSuite_rc.Message()
               );

        // update the results at TestSuite level
        if (testSuite_Result && testSuite_rc == OK)
        {
            Printf(m_UnitTest_Pri, " -> Passed\n");
            m_TestSuite_Passed++;
        }
        else
        {
            // print a warning
            Printf(m_UnitTest_Pri, " -> *Failed*\n");
        }

        // update the results at TestCase level
        m_TestCase_Passed += testCase_Passed;
        m_TestCase_Exec += testCase_Exec;

        // Print status
        PrintStatus(m_UnitTest_Pri);
    }
    return OK;
}

// -----------------------------------------------------------------------------
void
PolicyManager_UnitTest::UnitTest::
PrintStatus(Tee::Priority pri) const
{
    Printf(pri, "Status: TestSuite passed= %d / %d, "
           " TestCases passed= %d / %d -> unit testing= %s\n",
           m_TestSuite_Passed,
           m_TestSuite_Exec,
           m_TestCase_Passed,
           m_TestCase_Exec,
           (IsPassed() ? "Passed" : "*Failed*") );
}

// -----------------------------------------------------------------------------
void
PolicyManager_UnitTest::UnitTest::
PrintFinalResult(Tee::Priority pri) const
{
    Printf(pri, "%s\n", string(80, '=').c_str() );
    Printf(pri, "Final Result:\n");
    PrintStatus(pri);
    Printf(pri, "%s\n", string(80, '=').c_str() );
}

// -----------------------------------------------------------------------------
void
PolicyManager_UnitTest::UnitTest::
GetResults(UINT32 *testSuite_Passed, UINT32 *testSuite_Exec,
           UINT32 *testCase_Passed, UINT32 *testCase_Exec) const
{
    *testSuite_Passed = m_TestSuite_Passed;
    *testSuite_Exec = m_TestSuite_Exec;
    *testCase_Passed = m_TestCase_Passed;
    *testCase_Exec = m_TestCase_Exec;
}

// -----------------------------------------------------------------------------
//! returns true if the test are passed
bool
PolicyManager_UnitTest::UnitTest::
IsPassed() const
{
    return(m_TestSuite_Passed == m_TestSuite_Exec);
}

// -----------------------------------------------------------------------------

void
PolicyManager_UnitTest::UnitTest::
SetUnitTest_Priority(Tee::Priority pri)
{
    m_UnitTest_Pri = pri;
}

void
PolicyManager_UnitTest::UnitTest::
SetTestSuite_Priority(Tee::Priority pri)
{
    m_TestSuite_Pri = pri;
}

void
PolicyManager_UnitTest::UnitTest::
SetTestCase_Priority(Tee::Priority pri)
{
    m_TestCase_Pri = pri;
}

// /////////////////////////////////////////////////////////////////////////////
// Surface_Test
// /////////////////////////////////////////////////////////////////////////////

PageRange_Debug::PageRange_Debug(UINT32 page)
:
m_RelativeOffset(page*0x1000),
m_AbsoluteOffset(page*0x1000),
m_Size(0x1000)
{
}

PageRange_Debug::PageRange_Debug(UINT64 r, UINT64 a, UINT64 s)
:
m_RelativeOffset(r),
m_AbsoluteOffset(a),
m_Size(s)
{
}

// /////////////////////////////////////////////////////////////////////////////

PolicyManager_UnitTest::
Surface_TestSuite::Surface_TestSuite
(
    Tee::Priority ts_pri,
    Tee::Priority tc_pri
)
:
Abstract_TestSuite(ts_pri, tc_pri, "Surface_TestSuite")
{
}

PolicyManager_UnitTest::Surface_TestSuite::
~Surface_TestSuite()
{
}

RC
PolicyManager_UnitTest::Surface_TestSuite::
Run()
{
    RC rc;

    // first set of TestCases
    CHECK_RC(GetPagesFromDmaMappings_Test1());

    return rc;
}

//! Set the surface used for the test a ClipId with size 0x6000
RC
PolicyManager_UnitTest::Surface_TestSuite::
Get_ClipId_0x6000_Surface(PolicyManager::Surface** theSurface) const
{
    RC rc;

    Tee::Priority pri = Abstract_TestSuite::GetTestSuitePriority();
    Printf(pri, "%s\n", __FUNCTION__);

    // performs the surface query
    Surfaces surfaces;
    RegExManager testName (".*");
    RegExManager chanName (".*");
    Surface_RegExManager surfName ("ClipID");
    RegExManager fileType (".*");
    SurfaceQuery sq = SurfaceQuery(&testName, &chanName, &surfName, &fileType);

    PolicyManager::Registry::Instance()->SurfaceQuery(sq, &surfaces);

    StringPrint(pri, "\nQuerySurface= " + sq.ToString() +
                "\n\tResult:\n" + Surfaces_To_String(surfaces) );

    // check that there is only one surface:
    MASSERT(surfaces.size() == 1);

    // returns the found surface
    *theSurface = *(surfaces.begin());

    // get the DmaMapping
    Printf(pri, "The surface: %s\n",
           (*theSurface)->ToString().c_str() );
    Surface::DmaMappings dmaMappings;
    (*theSurface)->GetDmaMappings(&dmaMappings);

    // run_tgen200.pl -target fmodel -release -level advsched/scratch -onlyTrepId f86cb3c13b831d3030802151e71d6350
    //
    // check that the surface has the "right" values for this test
    // [5647] PM: 13 / 13: Surface_DmaBuffer: (0xb3cd93c) Name= ClipID,
    // mappingNum= 0, dmaMappingNum= 1,
    // dma= VIRTUAL_MEM, hDma= 0xd1a64004[0x0], hMem= 0xd1a64003[0x0],
    // offset= 0x20103000, size= 0x6000, flags= 0x0
    MASSERT(dmaMappings.size() == 1);
    MASSERT(dmaMappings.begin()->m_size == 0x6000);

    return rc;
}

// -----------------------------------------------------------------------------
//! tests the method Surface::GetPagesFromDmaMappings
RC
PolicyManager_UnitTest::Surface_TestSuite::
GetPagesFromDmaMappings_Test1()
{
    Tee::Priority pri = Abstract_TestSuite::GetTestSuitePriority();
    RC rc;

    // surface used to test
    Surface* theSurface = 0;

    // get the right surface
    Printf(pri, "%s\n", string(80, '.').c_str() );
    Printf(pri, "TestCases from %s\n", __FUNCTION__);
    Printf(pri, "%s\n", string(80, '.').c_str() );
    CHECK_RC(Get_ClipId_0x6000_Surface(&theSurface));

    Printf(pri, "Computing the pages for the surface (%p): %s\n",
           theSurface,
           theSurface->ToString().c_str() );

    // visitor for GetVirtualPage
    Surface_GetPagesFromAddressRange_Test st(pri, theSurface);

    // visitor for IsIncludedInAddressRange
    Surface_IsIncludedInAddressRange_Test si(pri, theSurface);

    // assuming bytesPerPage = 4096 = 0x1000

#define AT_LINE Format_To_String("%s at line %d", __FILE__, __LINE__)

    // first page
    {
        PageRange_Debug pVp[] = { PageRange_Debug(0)};
        st.Setup(0x0, 0x1000, "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x0, 0x0, 0x1000-1)};
        st.Setup(0x0, (0x1000-1), "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x0, 0x0, 0x1)};
        st.Setup(0x0, 0x1, "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    // first page (with an offset = 1)
    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x1, 0x1, 0xffe)};
        st.Setup(0x1, (0x1000-2), "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x1, 0x1, 0xfff)};
        st.Setup(0x1, 0x1000-1,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x1, 0x1, 0x1)};
        st.Setup(0x1, 0x1,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    // last page
    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x5000, 0x5000, 0xffe)};
        st.Setup(0x5000, (0x1000-2),
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(5)};
        st.Setup(0x5000, 0x1000,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x5001, 0x5001, 0xfff)};
        st.Setup(0x5001, 0x1000-1,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x5010, 0x5010, 0x1)};
        st.Setup(0x5010, 0x1,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    // 1st and 2nd page
    {
        PageRange_Debug pVp[] = {
            PageRange_Debug(0x0, 0x0, 0x1001)};
        st.Setup(0, (0x1001),
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x0, 0x0, 0x2000)};
        st.Setup(0, (0x2000),
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }
    {
        PageRange_Debug pVp[] = {
            PageRange_Debug(0x0, 0x0, 0x1fff)};
        st.Setup(0, (0x1FFF),
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    // all pages
    {
        PageRange_Debug pVp[] = {
            PageRange_Debug(0x0, 0x0, 0x6000)
        };

        st.Setup(0x0000, 0x6000,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);

        st.Setup(0x0000, 0x0000,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = {
            PageRange_Debug(0x500, 0x500, 0x5500)
        };
        st.Setup(0x0500, (0x5500),
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = {
            PageRange_Debug(0x1, 0x1, 0x5FFF)
        };
        st.Setup(0x0001, 0x5FFF,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = {
            PageRange_Debug(0xFFF, 0xFFF, 0x5001),
        };

        st.Setup(0x0FFF, 0x5001,
                 "", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    // no match
    {
        // Windows builds whine that an array with 0 elements cannot be
        // initialized: PageRange_Debug pVp[] = {};
        // So I initialize it, and then pass a size = 0
        PageRange_Debug pVp[] = { PageRange_Debug(0)};
        st.Setup(0x6000, (0x1),
                 "Nomatch", pVp, 0, AT_LINE);
        ExelwteTestCase(&st);

        st.Setup(0x7000, (0x1000),
                 "Nomatch", pVp, 0, AT_LINE);
        ExelwteTestCase(&st);
    }

    // overflow
    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x0, 0x0, 0x6000)};
        st.Setup(0x0000, (0x6001),
                 "Overflow", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x5500, 0x5500, 0xb00)};
        st.Setup(0x5500, (0x1000),
                 "Overflow", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    {
        PageRange_Debug pVp[] = { PageRange_Debug(0x5000, 0x5000, 0x1000)};
        st.Setup(0x5000, (0x1500),
                 "Overflow", pVp, sizeof(pVp)/sizeof(pVp[0]), AT_LINE);
        ExelwteTestCase(&st);
    }

    // check IsIncluded
    // ----------------
    Surface::DmaMappings dmaMappings;
    theSurface->GetDmaMappings(&dmaMappings);
    UINT64 base = dmaMappings[0].m_offset;

    si.Setup( base + 0x0, 0x0, 0x1000, true, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x1, 0x0, 0x1000, true, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x1000, 0x0, 0x1000, false, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0xfff, 0x0, 0x1000, true, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0xfff, 0x0, 0xfff, false, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0xfff, 0x0, 0x1000, true, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x1000, 0x0, 0x6000, true, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0xfff, 0x0, 0x6000, true, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x6000, 0x0, 0x6000, false, "", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x0, 0x1, 0x6000, false, "Overflow", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x0, 0x1, 0x7000, false, "Overflow", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x0, 0x6000, 0x7000, false, "NoMatch", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x6500, 0x6000, 0x7000, false, "NoMatch", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x5000, 0x5000, 0x7000, true, "Overflow", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x6000, 0x5000, 0x7000, false, "Overflow", AT_LINE);
    ExelwteTestCase(&si);

    //
    si.Setup( base +0x5fff, 0x5000, 0x7000, true, "Overflow", AT_LINE);
    ExelwteTestCase(&si);

    return rc;
}

#undef AT_LINE

// /////////////////////////////////////////////////////////////////////////////
// Object: Surface
// Method: GetPagesFromAddressRange
// /////////////////////////////////////////////////////////////////////////////

// It is loaded with a surface to test the GetPagesFromAddressRange
// method.
// This test accepts a set of inputs/expected outputs because otherwise I
// should create and pass a set of test classes
PolicyManager_UnitTest::Surface_GetPagesFromAddressRange_Test::
Surface_GetPagesFromAddressRange_Test
(Tee::Priority pri, PolicyManager::Surface *ps)
:
    Abstract_TestCase(pri),
    m_PSurface(ps), m_IsSetup(false)
{
    // get the DmaMapping
    Printf(GetPriority(), "Computing the pages for the surface (%p): %s\n",
           m_PSurface,
           m_PSurface->ToString().c_str() );

    Surface::DmaMappings dmaMappings;
    m_PSurface->GetDmaMappings(&dmaMappings);

    // check that the surface has the "right" values for this test
    // [5647] PM: 13 / 13: Surface_DmaBuffer: (0xb3cd93c) Name= ClipID,
    // mappingNum= 0, dmaMappingNum= 1,
    // dma= VIRTUAL_MEM, hDma= 0xd1a64004[0x0], hMem= 0xd1a64003[0x0],
    // offset= 0x20103000, size= 0x6000, flags= 0x0
    MASSERT(dmaMappings.size() == 1);
    MASSERT(dmaMappings.begin()->m_size == 0x6000);
}

// -----------------------------------------------------------------------------

//! Store the params
// A little lwmbersome copying the params.
// - A better solution is to pass also the TestRunner that is called by the
// TestObject (but this means trusting the testing object)
// - use a struct to bundle the params so passing/copying them is easier
RC
PolicyManager_UnitTest::Surface_GetPagesFromAddressRange_Test::
Setup
(
    UINT64 offset, UINT64 size,
    const string& expectedErrorMsg,
    PageRange_Debug *pExpectedVP, UINT32 expectedNum,
    const string& description /* = "" */)
{
    if (m_IsSetup)
    {
        MASSERT(!"Trying to setup the params even if they are already initialized");
    }
    m_IsSetup = true;
    //
    m_Offset = offset;
    m_Size = size;
    m_ExpectedErrorMsg = expectedErrorMsg;
    m_pExpectedVP = pExpectedVP;
    m_ExpectedNum = expectedNum;

    SetDescription(description);

    // in future check the consistency of the params
    return OK;
}

// -----------------------------------------------------------------------------

//! Execute
RC
PolicyManager_UnitTest::Surface_GetPagesFromAddressRange_Test::
Run(bool *result)
{
    RC rc;

    Tee::Priority pri = GetPriority();

    if (!m_IsSetup)
    {
        MASSERT(!"Trying to exelwting a test without setting up the params");
    }
    m_IsSetup = false;

    // name of the routine
    string routineTestName = "GetPagesFromAddressRange";

    // by default
    *result = true;
    bool error = true;

    // return params
    PageRanges pageRanges;
    string errorMsg;
    Printf(pri, "==> Calling: %s(offset= %s , size= %s)\n",
           routineTestName.c_str(),
           UINT64_To_String(m_Offset).c_str(),
           UINT64_To_String(m_Size).c_str() );

    // call the functions
    CHECK_RC(m_PSurface->GetPagesFromAddressRange(
        m_Offset, m_Size, &pageRanges, &errorMsg
        ));

    Printf(pri, "--> Results:\n");

    // ErrorMsg
    error = ((errorMsg != "") != (m_ExpectedErrorMsg != ""));
    *result &= !error;
    Printf(pri, "\t[Expected/Returned] errorMsg= %s / %s%s\n",
           m_ExpectedErrorMsg.c_str(),
           errorMsg.c_str(),
           (error ? " -> *error*\n" : "")
          );

    // check size
    error = (pageRanges.size() != m_ExpectedNum);
    *result &= !error;
    Printf(pri, "\t[Expected/Returned] size= %d / %d%s",
           m_ExpectedNum,
           static_cast<UINT32>(pageRanges.size()),
           (error ? " -> *error*\n" : "")
           );

    // check the vector
    for (UINT32 i = 0; i < m_ExpectedNum; i++)
    {
        error =
            !(m_pExpectedVP[i].m_RelativeOffset == pageRanges[i].m_RelativeOffset &&
              m_pExpectedVP[i].m_AbsoluteOffset == pageRanges[i].m_AbsoluteOffset &&
              m_pExpectedVP[i].m_Size == pageRanges[i].m_Size);
        *result &= !error;
        Printf(pri, "\t[Expected/Returned] num= %d, relOffset= %s / %s, "
               "absOffset= %s / %s, size= %s / %s%s",
               i,
               UINT64_To_String(m_pExpectedVP[i].m_RelativeOffset).c_str(),
               UINT64_To_String(pageRanges[i].m_RelativeOffset).c_str(),
               UINT64_To_String(m_pExpectedVP[i].m_AbsoluteOffset).c_str(),
               UINT64_To_String(pageRanges[i].m_AbsoluteOffset).c_str(),
               UINT64_To_String(m_pExpectedVP[i].m_Size).c_str(),
               UINT64_To_String(pageRanges[i].m_Size).c_str(),
               (error ? " -> *error*\n" : "")
               );
    }

    return rc;
}

// /////////////////////////////////////////////////////////////////////////////
// Object: Surface
// Method: IsIncludedInAddressRange
// /////////////////////////////////////////////////////////////////////////////

Surface_IsIncludedInAddressRange_Test::
Surface_IsIncludedInAddressRange_Test
(Tee::Priority pri, PolicyManager::Surface *ps)
:
    Abstract_TestCase(pri),
    m_PSurface(ps), m_IsSetup(false)
{
    // get the DmaMapping
    Printf(GetPriority(), "Computing the pages for the surface (%p): %s\n",
           m_PSurface,
           m_PSurface->ToString().c_str() );

    Surface::DmaMappings dmaMappings;
    m_PSurface->GetDmaMappings(&dmaMappings);

    // check that the surface has the "right" values for this test
    // [5647] PM: 13 / 13: Surface_DmaBuffer: (0xb3cd93c) Name= ClipID,
    // mappingNum= 0, dmaMappingNum= 1,
    // dma= VIRTUAL_MEM, hDma= 0xd1a64004[0x0], hMem= 0xd1a64003[0x0],
    // offset= 0x20103000, size= 0x6000, flags= 0x0
    MASSERT(dmaMappings.size() == 1);
    MASSERT(dmaMappings.begin()->m_size == 0x6000);
}

// -----------------------------------------------------------------------------

//! Store the params
RC
Surface_IsIncludedInAddressRange_Test::
Setup
(
    UINT64 gpuAddr,
    UINT64 offset, UINT64 size,
    bool isIncluded,
    const string& expectedErrorMsg,
    const string& description /* = "" */)
{
    if (m_IsSetup)
    {
        MASSERT(!"Trying to setup the params even if they are already initialized");
    }
    m_IsSetup = true;
    // input
    m_GpuAddr = gpuAddr;
    m_Offset = offset;
    m_Size = size;
    // output
    m_IsIncluded = isIncluded;
    m_ExpectedErrorMsg = expectedErrorMsg;

    SetDescription(description);

    // in future check the consistency of the params
    return OK;
}

// -----------------------------------------------------------------------------

//! Execute
RC
Surface_IsIncludedInAddressRange_Test::
Run(bool *result)
{
    RC rc;

    Tee::Priority pri = GetPriority();

    if (!m_IsSetup)
    {
        MASSERT(!"Trying to exelwting a test without setting up the params");
    }
    m_IsSetup = false;

    // name of the routine
    // TODO: move in the base class
    string routineTestName = "IsIncludedInAddressRange";

    // by default
    *result = true;

    // return params
    bool isIncluded = false;
    string errorMsg = "";
    Printf(pri, "==> Calling: %s (gpuAddr= %s, offset= %s , size= %s)\n",
           routineTestName.c_str(),
           UINT64_To_String(m_GpuAddr).c_str(),
           UINT64_To_String(m_Offset).c_str(),
           UINT64_To_String(m_Size).c_str() );

    // call the functions
    CHECK_RC(m_PSurface->IsIncludedInAddressRange(
        m_GpuAddr, m_Offset, m_Size, &isIncluded, &errorMsg
        ));

    Printf(pri, "--> Results:\n");

    // isIncluded
    bool error = (isIncluded != m_IsIncluded);
    *result &= !error;
    Printf(pri, "\t[Expected/Returned] isIncluded= %s / %s %s\n",
           Bool_To_String(m_IsIncluded).c_str(),
           Bool_To_String(isIncluded).c_str(),
           (error ? "-> *error*" : "")
           );

    // ErrorMsg
    error = ((errorMsg != "") != (m_ExpectedErrorMsg != ""));
    *result &= !error;
    Printf(pri, "\t[Expected/Returned] errorMsg= \"%s\" / \"%s\" %s\n",
           m_ExpectedErrorMsg.c_str(),
           errorMsg.c_str(),
           (error ? "-> *error*" : "")
          );

    return rc;
}
