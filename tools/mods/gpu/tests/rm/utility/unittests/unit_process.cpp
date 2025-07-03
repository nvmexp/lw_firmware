/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/unittests/unittest.h"
#include "core/include/tasker.h"
#include "random.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "gpu/utility/fakeproc.h"
#include "core/include/modsdrv.h"

//****************************************************************************
// Fake PID test
//****************************************************************************
namespace
{
    class FakePIDTest : public UnitTest
    {
    public:
        FakePIDTest();
        virtual ~FakePIDTest();
        virtual void Run();

    private:
        virtual void Thread();
        UINT32 m_pid;
        static void RunThread(void*);
        void RAIITest(UINT32);
    };

    ADD_UNIT_TEST(FakePIDTest);

    FakePIDTest::FakePIDTest()
        : UnitTest("Fake PID Test")
    {
    }

    FakePIDTest::~FakePIDTest()
    {
    }

    void FakePIDTest::RunThread(void* params)
    {
        FakePIDTest* pTest = (FakePIDTest*) params;
        pTest->Thread();
    }

    void FakePIDTest::RAIITest(UINT32 pid)
    {
        UINT32 lwrrPid;
        RMFakeProcess::AsFakeProcess runas(pid);
        lwrrPid = RMFakeProcess::GetFakeProcessID();
        UNIT_ASSERT_EQ(lwrrPid, pid);
    }

    void FakePIDTest::Thread()
    {
        UINT32 pid;
        pid = RMFakeProcess::GetFakeProcessID();
        UNIT_ASSERT_EQ(pid, m_pid);

        RAIITest((UINT32)Tasker::LwrrentThread());

        pid = RMFakeProcess::GetFakeProcessID();
        UNIT_ASSERT_EQ(pid, m_pid);

        return;
    }

    void FakePIDTest::Run()
    {
        UINT32 pid;
        RMFakeProcess::GetFakeProcessID();

        m_pid = 42;
        RMFakeProcess::SetFakeProcessID(m_pid);
        pid = RMFakeProcess::GetFakeProcessID();

        UNIT_ASSERT_EQ(pid, ModsDrvGetFakeProcessID());

        UNIT_ASSERT_EQ(pid, m_pid);

        vector<Tasker::ThreadID> threads = Tasker::CreateThreadGroup(FakePIDTest::RunThread, this, 10, "Fake PID Test", true, nullptr);
        Tasker::Join(threads);

        // Make sure none of the children threads mangled my PID
        pid = RMFakeProcess::GetFakeProcessID();
        UNIT_ASSERT_EQ(pid, m_pid);
    }

}
