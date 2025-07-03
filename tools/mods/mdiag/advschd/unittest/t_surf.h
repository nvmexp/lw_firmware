/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2007, 2016 by LWPU Corporation.  All rights reserved.
* All information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file  t_surf.cpp / .h
//! \brief Contains objects and code to test the Surface object

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#include "core/include/utility.h"

#ifndef INCLUDED_PM_UNITTEST_T_SURF_H
#define INCLUDED_PM_UNITTEST_T_SURF_H

// Responsabilites and interactions btw objects
//
// * UnitTest:
// - it runs a set of test suites
// - it is a singleton
// - tests are added pushing ptr of Abstract_TestSuite
// - lwrrently, the ctor contains all the Test Suite to run
// - it keep the count of TestSuites and TestCases passed and exelwted
// - it is query after Run() to get/print the results of the test
//
// * TestSuite:
// - it runs a set of tests related to one or more methods of an object
// - a concrete class runs a set of TestCases
// - it keeps the count of TestCases passed and exelwted in the current TestSuite
//
// * TestCase:
// - it runs a single exelwtion of a method of an object
// - Setup() is used to set the input params and the expected results
// - Run() runs a particular test case

// Checklist about how to add a test suite (TO BE COMPLETED)
// ---------------------------------------
// - write a set of test cases (usually in a separate single h/cpp file) in the
//   form of UnitTest
// - for each test case writes a test result (TestResult)
// - update the main object in maintest.h/cpp which runs the test suites

// fwd
namespace PolicyManager
{
    class Surface;
};

//! Namespace containing objects used in generic unit testing (not necessarily
// of PolicyManager objects)
namespace PolicyManager_UnitTest
{
    // ////////////////////////////////////////////////////////////////////////
    // Abstract_TestCase
    // ////////////////////////////////////////////////////////////////////////

    //! Abstract class containing a test case for testing one methods of an object
    //!
    //! An object of this class is created and run by a TestSuite
    class Abstract_TestCase
    {
    public:
        //! Ctor
        Abstract_TestCase(Tee::Priority pri,
                          const string& name = "");
        //! Dector
        virtual ~Abstract_TestCase();
        //! Get the name
        string GetName() const;
        //! Run the test case and collects the results
        virtual RC Run(bool *result) = 0;
        //! Get the priority for this test
        Tee::Priority GetPriority() const;
        //!
        string GetDescription() const;
        //!
        void SetDescription(const string& des);
    private:
        //! Priority printing
        Tee::Priority m_Pri;
        //! TestCase's name
        string m_Name;
        //
        string m_Description;
    };

    // ////////////////////////////////////////////////////////////////////////
    // Abstract_TestSuite
    // ////////////////////////////////////////////////////////////////////////

    //! Abstract class containing a test suite for testing one or more methods
    //! of an object
    //!
    //! A TestSuite exelwtes test cases (derived from Abstract_TestCase)
    class Abstract_TestSuite
    {
    public:
        //! Ctor
        Abstract_TestSuite(Tee::Priority testSuite_pri,
                           Tee::Priority testCase_pri,
                           const string& name);
        //! Dector
        virtual ~Abstract_TestSuite();
        //! Get the name
        string GetName() const;
        //! Get the results for the current TestCases
        void GetResult(UINT32 *testCase_Passed, UINT32 *testCase_Exec) const;
        //! Execute the test and return the result
        virtual RC Run() = 0;
    protected:
        //! Get the priority for this test suite
        Tee::Priority GetTestSuitePriority() const;
        //! Get the priority for the test cases
        Tee::Priority GetTestCasePriority() const;
        //! Execute a test case, updating the statistics
        void ExelwteTestCase(Abstract_TestCase* pAtc);
    private:
        //! Priority to print the info of the TestSuite
        Tee::Priority m_TestSuite_Pri;
        //! Priority to print the info of the TestCase
        Tee::Priority m_TestCase_Pri;
        //! TestSuite's name
        string m_Name;
        //! TestCases passed in this TestSuite
        UINT32 m_TestCase_Passed;
        //! TestCases passed in this TestSuite
        UINT32 m_TestCase_Exec;
    };

    // ////////////////////////////////////////////////////////////////////////
    // UnitTest
    // ////////////////////////////////////////////////////////////////////////

    //! Runs all the test suites, aclwmulating the results and returing pass/fail
    class UnitTest
    {
    public:
        //! Returns the singleton
        static UnitTest* Instance();
        //! To free all the resources of the UnitTest singleton
        void Shutdown();

        //! Set the printing priority for the Unit Test
        void SetUnitTest_Priority(Tee::Priority pri);
        //! Set the printing priority for the Test Suites
        void SetTestSuite_Priority(Tee::Priority pri);
        //! Set the printing priority for the Test Cases
        void SetTestCase_Priority(Tee::Priority pri);

        //! Run all the test suites
        RC Run();

        //! Print testing results
        void PrintStatus(Tee::Priority pri) const;
        //! Print final results of test
        void PrintFinalResult(Tee::Priority pri) const;
        //! Get the results
        void GetResults(UINT32 *testSuite_Passed, UINT32 *testSuite_Exec,
                        UINT32 *testCase_Passed, UINT32 *testCase_Exec
                        ) const;
        //! returns true if the test are passed
        bool IsPassed() const;

        //! Add a TestSuite to run
        void AddTestSuite(Abstract_TestSuite *pts);
    private:
        //! Singleton instance
        static UnitTest *UnitTest_Singleton;
        //! Ctor
        UnitTest
        (
            Tee::Priority unitTest_Pri,
            Tee::Priority testSuite_Pri,
            Tee::Priority testCase_Pri
        );
        //! Dector
        ~UnitTest();

        //! Priority printing of the UnitTest
        Tee::Priority m_UnitTest_Pri;
        //! Priority printing for each TestSuite
        Tee::Priority m_TestSuite_Pri;
        //! Priority printing for each TestCase
        Tee::Priority m_TestCase_Pri;
        //! Vector with all the TestSuites (owner)
        typedef vector<Abstract_TestSuite*> TestSuites;
        TestSuites m_TestSuites;
        // Outcome for all the test suites (in terms of TestSuite)
        UINT32 m_TestSuite_Passed;
        UINT32 m_TestSuite_Exec;
        //! Total number of testcases exelwted/passed
        UINT32 m_TestCase_Passed;
        UINT32 m_TestCase_Exec;
    };

    // ////////////////////////////////////////////////////////////////////////
    // Surface_TestSuite
    // ////////////////////////////////////////////////////////////////////////

    //! \brief Used to simplify passing the expected result
    //
    // I'm trying not to change the interface of Surface only to simplify the test
    // gcc doesn't work if I forward-declare this data-struct
    struct PageRange_Debug
    {
        UINT64 m_RelativeOffset;
        UINT64 m_AbsoluteOffset;
        UINT64 m_Size;
        //! To build a 4K aligned page
        PageRange_Debug(UINT32 page);
        //! Normal ctor
        PageRange_Debug(UINT64 r, UINT64 a, UINT64 s);
    };

    // -------------------------------------------------------------------------
    //! Tests a Surface object
    class Surface_TestSuite : public Abstract_TestSuite
    {
    public:
        //! ctor
        Surface_TestSuite(Tee::Priority ts_pri, Tee::Priority tc_pri);
        //! dector
        ~Surface_TestSuite();
        //! Execute the test suite and return the result
        RC Run();
    protected:
        //! Get a ClipId surface with size=0x6000. It can be generated by:
        //! run_tgen200.pl -target fmodel -release -level advsched/sanity -verbose -nousetracecache -runInPlace -only wid
        RC Get_ClipId_0x6000_Surface(PolicyManager::Surface** theSurface) const;
        //! Tests the method GetPagesFromDmaMappings
        RC GetPagesFromDmaMappings_Test1();
    };

    // -------------------------------------------------------------------------
    //! Test the method "GetPagesFromAddresRange" of Surface
    class Surface_GetPagesFromAddressRange_Test
        : public Abstract_TestCase
    {
    public:
        //! ctor
        Surface_GetPagesFromAddressRange_Test
            (Tee::Priority pri, PolicyManager::Surface *ps);

        //! Store the params for a call to method under test
        RC Setup(UINT64 offset, UINT64 size,
                 const string& expectedErrorMsg,
                 PageRange_Debug *pExpectedVP, UINT32 expectedNum,
                 const string& description = "");

        //! Execute the test and return the result
        RC Run(bool *result);
    private:
        PolicyManager::Surface *m_PSurface;
        //! set by Setup, reset by Run
        // TODO: move this member and the code in the base class
        bool m_IsSetup;
        //! input params
        UINT64 m_Offset;
        UINT64 m_Size;
        //! output params
        string m_ExpectedErrorMsg;
        PageRange_Debug* m_pExpectedVP;
        UINT32 m_ExpectedNum;
    };

    // -------------------------------------------------------------------------
    //! Test the method "IsIncludedInAddressRange" of Surface
    class Surface_IsIncludedInAddressRange_Test
        : public Abstract_TestCase
    {
    public:
        //! ctor
        Surface_IsIncludedInAddressRange_Test
            (Tee::Priority pri, PolicyManager::Surface *ps);

        //! Store the params for a call to method under test
        RC Setup(UINT64 gpuAddr,
                 UINT64 offset, UINT64 size,
                 bool isIncluded,
                 const string& expectedErrorMsg,
                 const string& description = "");

        //! Execute the test and return the result
        RC Run(bool *result);
    private:
        PolicyManager::Surface *m_PSurface;
        //! set by Setup, reset by Run
        bool m_IsSetup;
        //! input params
        UINT64 m_GpuAddr;
        UINT64 m_Offset;
        UINT64 m_Size;
        //! output params
        bool m_IsIncluded;
        string m_ExpectedErrorMsg;
    };

} // PolicyManager_UnitTest

#endif
