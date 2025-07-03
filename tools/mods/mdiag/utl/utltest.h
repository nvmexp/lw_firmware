/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLTEST_H
#define INCLUDED_UTLTEST_H

#include <memory>

#include "mdiag/tests/test.h"

#include "utlpython.h"

class Test;
class UtlChannel;
class UtlTsg;
class UtlSmcEngine;
class UtlSurface;

// This class represents the C++ backing for the UTL Python Test object.
// This class is a wrapper around existing test classes like Trace3DTest.
//
class UtlTest
{
public:
    static void Register(py::module module);

    enum TEST_STATE
    {
        TEST_STATE_INITIAL = 0,
        TEST_STATE_SET_UP_START = 1,
        TEST_STATE_SET_UP_FINISHED  = 2,
        TEST_STATE_RUN     = 3,
        TEST_STATE_CLEANED = 4,
    };

    ~UtlTest();

    void AddChannel(UtlChannel* channel);
    void AddTsg(UtlTsg* utlTsg);
    void AddSurface(UtlSurface* pUtlSurface);
    void RemoveSurface(UtlSurface* pUtlSurface);
    void UpdateTrepStatus();

    TEST_STATE GetState() const { return m_State; }
    Test::TestStatus GetStatus() const { return m_Test->GetStatus(); }
    void SetStatus(const Test::TestStatus status) { m_Test->SetStatus(status); }
    UtlSurface* GetSurface(const string &name);

    static void FreeTests();
    static UtlTest* GetFromTestId(UINT32 testId);
    static UtlTest* GetFromTestPtr(Test* test);
    static UtlTest* CreateFromTestPtr(Test* test, string testConfig = "");

    // The following functions are called from the Python interpreter.
    static UtlTest* Create(py::kwargs kwargs);
    void Setup();
    void Run(py::kwargs kwargs);
    bool RunFinished() const;
    string GetName();
    UtlChannel* GetChannelByName(py::kwargs kwargs);
    py::list GetChannelsPy();
    const vector<UtlChannel*>& GetChannels() { return m_Channels; }
    py::list GetTsgsPy();
    UtlSurface* GetSurfaceByName(py::kwargs kwargs);
    py::list GetSurfacesPy();
    static py::list GetTestsPy();
    static vector<UtlTest*> GetTests();
    static UINT32 GetCfgTraceCount();
    void SetSmcEngine(py::kwargs kwargs);
    void AbortTest() { m_Test->AbortTest(); }
    UtlSmcEngine* GetSmcEngine();
    UINT32 GetTestId() const { return m_TestId; }
    vector<string> GetUserTestData();
    string GetTestArgs() const;
    string GetTracePath() const;
    Test * GetRawPtr() const { return m_Test; }
    vector<UINT08> ReadTraceFile(py::kwargs kwargs);
    bool IsAborted();
    string ToString() const;
    static UtlTest* GetScriptTest();

    static vector<string> s_CfgLines;

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlTest() = delete;
    UtlTest(UtlTest&) = delete;
    UtlTest& operator=(UtlTest&) = delete;

private:
    UtlTest(Test* test);

    void Cleanup(bool deferCleanup);
    void CleanupUtlTestResources();

    static UtlTest* CreateTest(const string& testConfig);

    UINT32 m_TestId;
    Test* m_Test;
    TEST_STATE m_State = TEST_STATE_INITIAL;
    unique_ptr<ArgDatabase> m_TestArgs;

    // If this UtlTest object was created via UTL script, then this
    // pointer will have ownership of the corresponding Test object.
    // If this UtlTest was created with an already existing Test pointer,
    // then TestDirectory has ownership of the Test object and this pointer
    // will be null.
    unique_ptr<Test> m_OwnedTest;

    // This is a list of all UtlTsg object associated with this test.
    vector<UtlTsg*> m_Tsgs;

    // This is a list of all UTL channel objects associated with this test.
    vector<UtlChannel*> m_Channels;

    // This is a list of all UTL surface objects associated with this test.
    vector<UtlSurface*> m_Surfaces; 

    // This stores the test config string for this test.
    string m_TestConfig;

    // All UtlTest objects are stored in this map.
    static map<Test*, unique_ptr<UtlTest>> s_Tests;

    static UINT32 s_RunningTestCount;
};

#endif
