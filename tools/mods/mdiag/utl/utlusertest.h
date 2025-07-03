/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLUSERTEST_H
#define INCLUDED_UTLUSERTEST_H

#ifndef _WIN32

#include "mdiag/lwgpu.h"
#include "mdiag/tests/test.h"
#include "mdiag/utils/mdiagsurf.h"

#include "utlpython.h"

class MdiagSurf;
class UtlChannel;
class UtlSurface;
class UtlTsg;

// This file contains classes used to implement UTL mdiag tests.  A
// user-created UTL test is similar to misc tests like frontdoor and vpr,
// but instead of being implemented in C++, the test-specific code is
// implemented in Python.  This frees a test creator from having to edit and
// submit MODS source code in order to make a new test.  These UTL tests
// can be specified in a test's .cfg file just like other mdiag tests.
//
// To create a UTL-based test, the user must implement a test class in a
// Python file.  This test class must inherit from the Python base class
// utl.UserTest, which is exported in ultmodules.h and is backed by the
// C++ class UtlUserTestPy.  The test class must also implement the functions
// Setup and Run.  These Python functions are called by the UtlUserTest class,
// which acts as the interface between the mdiag C++ layer and the Python
// test class.  Lastly, the test class must set the inherited status member
// variable to the correct status to indicate whether or not the test
// passed or failed.


// This class represents the interface between mdiag and the Python test class.
// It owns a pointer to an instantiation of the Python test class, as well
// lists of resources allocated by that test instance.
//
class UtlUserTest : public Test
{
public:
    ~UtlUserTest();
    UtlUserTest(UtlUserTest&) = delete;
    UtlUserTest& operator=(UtlUserTest&) = delete;

    static Test* Factory(ArgDatabase *args);

    int Setup() override;
    void Run() override;
    void CleanUp() override;
    bool IsEcoverSupported() override { return true; }
    void EnableEcoverDumpInSim(bool enabled) override {} // bypass the default impl in Test as trace_3d/v2d.

    void AddSurface(const string& name, UtlSurface* surface);
    UtlSurface* GetSurface(const string& name);
    void AddTsg(const string& name, UtlTsg* tsg);
    UtlTsg* GetTsg(const string& name);
    void AddChannel(const string& name, UtlChannel* channel);
    UtlChannel* GetChannel(const string& name);

    const ArgReader* GetParam() const override { return m_Reader.get(); }
    void AbortTest() override { m_IsAborted = true; }
    bool AbortedTest() const override { return m_IsAborted; }
    LwRm* GetLwRmPtr() override { return m_LwRm; }
    const char* GetTestName() override { return m_TestName.c_str(); }
    GpuDevice* GetGpuDevice() { return m_GpuResource->GetGpuDevice(); }
    SmcEngine* GetSmcEngine() override { return m_SmcEngine; }
    LWGpuResource* GetGpuResource() const override { return m_GpuResource; }

    static UtlUserTest* GetTestFromPythonObject(py::object userTestObj);
    static void FreePythonObjects();

private:
    UtlUserTest();

    RC ImportTestClass(const string& testPath, const string& moduleName);
    RC StartTestInPolicyManager();
    RC AddChannelToPolicyManager(LWGpuChannel* lwgpuChannel);
    void CheckForInterrupts();

    unique_ptr<EcovVerifier> m_EcovVerifier;

    // The name of the Python class that implements the user test.
    string m_ClassName;

    // A reference to the instantiation of the Python class
    // specified by m_ClassName.
    py::object m_PythonTest;

    // A map of all surfaces associated with the test.
    map<string, UtlSurface*> m_Surfaces;

    // A map of all TSGs associated with the test.
    map<string, UtlTsg*> m_Tsgs;

    // A map of all channels associated with the test.
    map<string, UtlChannel*> m_Channels;

    // The arguments pass to the test.
    unique_ptr<ArgReader> m_Reader;

    // A map of all user test objects, referenced by the Python object pointer.
    static map<PyObject*, UtlUserTest*> s_UserTestsMap;

    bool m_CheckForInterrupts = false;
    bool m_IsAborted = false;
    LwRm* m_LwRm = nullptr;
    string m_TestName;
    LWGpuResource* m_GpuResource = nullptr;
    SmcEngine* m_SmcEngine = nullptr;
};

// This class is the C++ backing for the Python class utl.UserTest.
// When a user implements a UTL test class, their class must derive from
// the Python class utl.UserTest.
//
class UtlUserTestPy
{
public:
    static void Register(py::module module);

    UtlUserTestPy();
    UtlUserTestPy(UtlUserTestPy&) = delete;
    UtlUserTestPy &operator=(UtlUserTestPy&) = delete;
    ~UtlUserTestPy() = default;

    vector<string> GetUserTestData();

    // This stores the current status of the user test.  The user test
    // class will set this value from Python to indicate if the test
    // has finished and if the test has passed or failed.
    Test::TestStatus m_Status = Test::TestStatus::TEST_NOT_STARTED;

    UtlUserTest* m_pUtlUserTest = nullptr;
};

#ifdef MAKE_TEST_LIST
#ifdef INCLUDE_MDIAGUTL
CREATE_TEST_LIST_ENTRY(utl, UtlUserTest, "user-defined UTL test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &utl_testentry
#endif
#endif

#endif
#endif
