/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2012,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unittest.cpp
 * @brief  Implementation of the UnitTest class.
 */

#include "unittest.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/jswrap.h"
#include "boost/algorithm/string/join.hpp"
#include <memory>

UnitTest::TestCreatorBase* UnitTest::TestCreatorBase::s_pFirst = 0;
UnitTest::TestCreatorBase* UnitTest::TestCreatorBase::s_pLast = 0;

UnitTest::TestCreatorBase::TestCreatorBase()
: m_pNext(0)
{
    if (s_pLast)
    {
        s_pLast->m_pNext = this;
    }
    else
    {
        s_pFirst = this;
    }
    s_pLast = this;
}

UnitTest::TestCreatorBase::~TestCreatorBase()
{
    s_pFirst = 0;
    s_pLast = 0;
}

UnitTest::UnitTest(const char* name)
: m_Name(name)
, m_Mutex(0)
, m_FailCount(0)
{
}

//--------------------------------------------------------------------
//! \brief Run a suite of unit tests
//!
//! \param testList Contains the names of unit tests to run.  If
//!     empty, then run all unit tests.
//! \param skipList Contains the names of unit tests to skip.  If a
//!     test name appears in both testList and skipList, then skipList
//!     takes priority and the test is skipped.
//! \return OK if all tests pass.  Returns an error if testList or
//!     skipList contain an unknown tests name, or if there are no
//!     unit tests.  Note that it is not an error if all tests are
//!     skipped due to skipList.
//!
RC UnitTest::RunTestSuite
(
    const vector<string> &testList,
    const vector<string> &skipList
)
{
    StickyRC rc;
    const set<string> testSet(testList.begin(), testList.end());
    const set<string> skipSet(skipList.begin(), skipList.end());
    set<string> allTestNames;

    // Run the tests
    for (TestCreatorBase* pCreator = TestCreatorBase::GetFirst();
         pCreator != nullptr; pCreator = pCreator->GetNext())
    {
        // Instantiate a unit test
        unique_ptr<UnitTest> pTest(pCreator->Create());

        // Add the test name to allTestNames, and detect duplicates
        const string &testName = pTest->GetName();
        if (allTestNames.count(testName))
        {
            Printf(Tee::PriError,
                   "Duplicate test name \"%s\"\n", testName.c_str());
            rc = RC::SOFTWARE_ERROR;
        }
        allTestNames.insert(testName);

        // Run the test if it's in testSet (or if testSet is empty),
        // but not in skipSet.
        if ((testSet.empty() || testSet.count(testName)) &&
            !skipSet.count(testName))
        {
            rc = pTest->RunTest();
        }
    }

    if (allTestNames.empty())
    {
        // Report an error if there were no unit tests
        Printf(Tee::PriError,
                "No tests found!\n"
                "Try building on linux with INCLUDE_UNITTEST=true\n");
        rc = RC::UNSUPPORTED_FUNCTION;
    }
    else
    {
        // Report an error if testSet or skipSet contains bad test names
        vector<string> unknownTests;
        set_difference(testSet.begin(), testSet.end(),
                       allTestNames.begin(), allTestNames.end(),
                       inserter(unknownTests, unknownTests.end()));
        set_difference(skipSet.begin(), skipSet.end(),
                       allTestNames.begin(), allTestNames.end(),
                       inserter(unknownTests, unknownTests.end()));
        if (!unknownTests.empty())
        {
            for (const string &testName: unknownTests)
            {
                Printf(Tee::PriError, "Unknown unit test \"%s\"\n",
                       testName.c_str());
                rc = RC::BAD_PARAMETER;
            }
            Printf(Tee::PriError, "Valid unit tests are \"%s\"\n",
                   boost::algorithm::join(allTestNames, "\", \"").c_str());
        }
    }
    return rc;
}

RC UnitTest::RunAllTests()
{
    return RunTestSuite({}, {});
}

RC UnitTest::RunTest()
{
    Printf(Tee::PriNormal, "Enter: %s test\n", m_Name.c_str());

    m_Mutex = Tasker::AllocMutex("UnitTest::m_Mutex",
                                 Tasker::mtxUnchecked);
    m_FailCount = 0;

    const UINT64 startTime = Xp::GetWallTimeUS();
    Run();
    const UINT64 duration = Xp::GetWallTimeUS() - startTime;

    Tasker::FreeMutex(m_Mutex);
    m_Mutex = 0;

    if (m_FailCount)
    {
        Printf(Tee::PriError, "%s test FAILED!\n", m_Name.c_str());
    }
    else
    {
        Printf(Tee::PriLow, "%s test took %ums\n",
                m_Name.c_str(), static_cast<unsigned>(duration/1000));
    }
    Printf(Tee::PriNormal, "Exit: %s test\n", m_Name.c_str());
    return m_FailCount ? RC::SOFTWARE_ERROR : OK;
}

void UnitTest::UnitAssertCmp
(
    int line,
    bool equal,
    const char* op,
    const char* negOp,
    const char* a,
    const char* b,
    const string& valA,
    const string& valB
)
{
    if (equal)
    {
        Printf(Tee::PriLow, "line %d: \"%s\" %s \"%s\" PASSED (%s %s %s)\n",
               line, a, op, b, valA.c_str(), op, valB.c_str());
    }
    else
    {
        Printf(Tee::PriError, "line: %d: \"%s\" %s \"%s\" FAILED! (%s %s %s)\n",
               line, a, op, b, valA.c_str(), negOp, valB.c_str());
        Tasker::MutexHolder lock(m_Mutex);
        m_FailCount++;
    }
}

string UnitTest::ToString(bool value)
{
    return value ? "true" : "false";
}

string UnitTest::ToString(int value)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%d", value);
    return buf;
}

string UnitTest::ToString(unsigned value)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%u", value);
    return buf;
}

string UnitTest::ToString(UINT64 value)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%llu", value);
    return buf;
}

string UnitTest::ToString(FLOAT32 value)
{
    char buf[32];
    snprintf(buf, sizeof buf, "0x%08x", *reinterpret_cast<UINT32*>(&value));
    return buf;
}

string UnitTest::ToString(FLOAT64 value)
{
    char buf[32];
    snprintf(buf, sizeof buf, "0x%016llx", *reinterpret_cast<UINT64*>(&value));
    return buf;
}

JS_CLASS(UnitTest);
static SObject UnitTest_Object
(
   "UnitTest",
   UnitTestClass,
   0,
   0,
   "UnitTest class"
);

JS_REGISTER_METHOD(UnitTest, RunTestSuite,
    "Run a suite of unit tests.  Usage: RunTestSuite(testList, skipList)");

JS_REGISTER_METHOD(UnitTest, RunAllTests, "Run all unit tests.");
