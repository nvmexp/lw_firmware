/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009, 2019-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "stdtest.h"

#include "conlwrrentTest.h"
#include "testdir.h"
#include "test_state_report.h"
#include "core/include/chiplibtracecapture.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include <memory>
#include "mdiag/utl/utl.h"
#include "mdiag/smc/smcengine.h"

extern const ParamDecl conlwrrentParams[] = {
    { "-timeout_ms", "f", (ParamDecl::ALIAS_START | ParamDecl::ALIAS_OVERRIDE_OK), 0, 0, "Amount of time in milliseconds to wait before declaring the test hung" },
    { "-hwWaitFail", "f", ParamDecl::ALIAS_MEMBER, 0, 0, "Amount of time in milliseconds to wait before declaring the test hung (Alias of -timeout_ms)" },

    UNSIGNED_PARAM("-progress", "Report progress to stdout every N iterations (default=disable)."),
    SIMPLE_PARAM("-block", "Enable synchronization with other tests by blocking after init and before data compare (default=disable)."),
    SIMPLE_PARAM("-forever", "Repeat the init-run-check outer sequence until error or timeout (default=disable)."),
    SIMPLE_PARAM("-time", "Enable timing of the each init, run, and check part (default=disable)."),
    SIMPLE_PARAM("-nocheck", "Disable checking for failures to increase conlwrrency (default=check)."),
    UNSIGNED_PARAM("-trig_LA", "triggers Logic Analiser by io write to address parameter in the start of Run()"),
    SIMPLE_PARAM("-serial", "forces all tests to run serially"),
    LAST_PARAM
};

vector<unique_ptr<Thread::BlockSyncPoint>> ConlwrrentTest::s_SyncPointVec;

void
ConlwrrentTest::ParseConlwrrentArgs(ArgReader *args)
{
    if (args->ParamPresent("-progress")) {
        _reportProgress = args->ParamUnsigned("-progress");
        InfoPrintf("%s: Progress reported every %d lines.\n", GetTestName(), _reportProgress);
    }

    if (args->ParamPresent("-block"))
    {
        if (args->ParamPresent("-serial"))
        {
            WarnPrintf("-block argument is ignored when -serial is specified.\n");
        }
        else
        {
            _block = true;
            InfoPrintf("%s: Blocking enabled.\n", GetTestName());
        }
    }

    if (args->ParamPresent("-forever")) {
        _endTest = false;
        InfoPrintf("%s: Forever option specified.\n", GetTestName());
    }

    if (args->ParamPresent("-time")) {
        _timemode = true;
        InfoPrintf("%s: Timing statistics enabled.\n", GetTestName());
    }

    if (args->ParamPresent("-nocheck")) {
        _doCheck = false;
        InfoPrintf("%s: Pass/fail checking disabled.\n", GetTestName());
    }

    if (args->ParamPresent("-trig_LA")) {
        _Trigger_LA = true;
        _Trigger_Addr = args->ParamUnsigned("-trig_LA");
        InfoPrintf("Will trigger LA at 0x%x on Run()\n",_Trigger_Addr);
    }

    m_TimeoutMs = args->ParamFloat("-timeout_ms", m_TimeoutMs);
}

ConlwrrentTest::ConlwrrentTest() : Test()
{
    DebugPrintf ("Entering ConlwrrentTest Constructor.\n");

    _reportProgress = 0;
    _block = false;
    _endTest = true;
    _timemode = false;
    _doCheck = true;
    _Trigger_LA = false;
    m_TimeoutMs = 10000;

    DebugPrintf ("Leaving ConlwrrentTest Constructor.\n");
}

// We need atleast one non-inlined virtual function to have a single
// copy of the virtual function table. Otherwise it exists statically
// in every file which includes ConlwrrenTest.h. By convention, this
// is the destructor.
ConlwrrentTest::~ConlwrrentTest()
{
}

// we use doubles to maintain microsecond accuracy
static inline double timediff(UINT64 t1usec, UINT64 t2usec) {
  return (double(t2usec)-double(t1usec))/1.0e6;
}

void
ConlwrrentTest::Run()
{
    // these are used for timing
    int testloops = 0;
    UINT64 StartuSec = 0;
    UINT64 EnduSec;

    double TotalInitTime = 0;
    double TotalLoopTime = 0;
    double TotalCheckPFTime = 0;
    double TotalShutdownTime = 0;

    // for the loop time (device busy time), we also want best case and worst case times
    double BCLoopTime = 1.0e11; // this is larger than the largest timer value: 2^32 sec < 2^33 = 8^11 < 10^11
    double WCLoopTime = 0;

    // Used for hang detection.
    bool hangDetected = false;

    UINT32 NumTimesPolled, NumTimesBusy;

    if (_block) 
    {
        if (s_SyncPointVec.size() == 0)
        {
            for (UINT32 syncPoint = TEST_INIT; syncPoint <= TEST_END; ++syncPoint)
            {
                s_SyncPointVec.push_back(make_unique<Thread::BlockSyncPoint>(string("TEST_STAGE") + to_string(syncPoint)));
            }
        }
        LWGpuResource::GetTestDirectory()->RegisterForBlock(s_SyncPointVec[TEST_INIT].get());
        Yield(); // everyone must register before calling block
    }

    if (TEST_NOT_STARTED == GetStatus())
    {
        SetStatus(TEST_INCOMPLETE);
    }

    if(_Trigger_LA)
    {
        if(_Trigger_Addr <= 0xffff)
        {
            UINT32 data = 0;
            InfoPrintf("Attention! Run-Begin Triggering the logic analiser at IO READ(0x%x)\n",_Trigger_Addr);
            Platform::PioRead32(_Trigger_Addr, &data);
        }
        else
        {
            InfoPrintf("Attention! Run-Begin Triggering the logic analiser at MEM READ(0x%x)\n",_Trigger_Addr);
            MEM_RD32(_Trigger_Addr);
        }
    }

    do {
        // this is the start of the -forever loop
        if (!_endTest)
        {
            InfoPrintf("%s: Starting forever test loop #%d\n", GetTestName(), testloops);
        }
        else
        {
            InfoPrintf("%s: Starting test\n", GetTestName());
        }
        testloops++;

        if (GetSmcEngine())
        {
            InfoPrintf("%s: Running on SmcEngine %s\n", GetTestName(), GetSmcEngine()->GetName().c_str());
        }

        // call init to setup for a low overhead run
        if (_timemode)
        {
            DebugPrintf("%s: GetTime()...\n", GetTestName());
            StartuSec = Platform::GetTimeUS();
            DebugPrintf("%s: GetTime()done\n", GetTestName());
        }

        DebugPrintf("%s: BeginIteration()...\n", GetTestName());
        int deviceStatus = BeginIteration(); // to allow different initialization after each CRC check (e.g. randomization)
        DebugPrintf("%s: BeginIteration() done\n", GetTestName());

        // if the test failed at this point, we need to set status and get out now, as trying to go on from
        // here creates all kinds of problems since the test cleaned itself up and no longer really exists
        // in any state worth messing with
        if(deviceStatus == FAILED)
        {
            if (_block)
                LWGpuResource::GetTestDirectory()->Block(s_SyncPointVec[TEST_INIT].get(), s_SyncPointVec[TEST_INIT+1].get());
            SetStatus(TEST_FAILED);
            return;
        }

        NumTimesPolled = NumTimesBusy = 0;

        if (_timemode) {
            EnduSec = Platform::GetTimeUS();
            TotalInitTime += timediff(StartuSec, EnduSec);
        }

        // optionally let everyone synchronize after init, so that every device starts running at the same time
        if (_block) {
            InfoPrintf("Before Block()\n");
            LWGpuResource::GetTestDirectory()->Block(s_SyncPointVec[TEST_INIT].get(), s_SyncPointVec[TEST_INIT+1].get());
            InfoPrintf("After Block()\n");
        }

        StartuSec = Platform::GetTimeUS();

        auto testRunScope = make_unique<ChiplibOpScope>(string(GetTestName()) + " Running", NON_IRQ,
                                                        ChiplibOpScope::SCOPE_COMMON, nullptr);

        do {
            // this is where the device goes busy with low cpu overhead
            // we can loop multiple times to extend the running time before we check for pass/fail and reinit
            DebugPrintf("%s: StimulateDevice()...\n", GetTestName());
            deviceStatus = StimulateDevice(deviceStatus);
            DebugPrintf("%s: StimulateDevice() done.\n", GetTestName());

            NumTimesPolled++;
            int busycount = 0;
            while (deviceStatus == BUSY) {
                Yield();

                deviceStatus = GetDeviceStatus();

                if(deviceStatus == FAILED) break;
                busycount++;
                NumTimesBusy += (deviceStatus == BUSY);
                NumTimesPolled++;

                UINT64 LwruSec = Platform::GetTimeUS();
                if (LwruSec > StartuSec
                    && (LwruSec - StartuSec > m_TimeoutMs*1000) 
                    && deviceStatus != FINISHED)
                {
                    ErrPrintf("%s: hang detected busy waiting. waitcount = %d, "
                        "(%lld -- %lld)",
                        GetTestName(), busycount, StartuSec, LwruSec);
                    RawPrintf("\n"); // do a RawPrintf to flush
                    _endTest = true;
                    hangDetected = true;
                    deviceStatus = FINISHED; // force us out of the loop
                }

                if (!(busycount % 10))
                {
                    DebugPrintf("%s: busycount=%d\n", GetTestName());
                }
            }
        } while ((deviceStatus != FINISHED) && (deviceStatus != FAILED));

        if (Utl::HasInstance())
        {
            Utl::Instance()->TriggerTestMethodsDoneEvent(this);
        }

        testRunScope.reset();

        if (_timemode) {
            EnduSec = Platform::GetTimeUS();

            // Time callwlations for each loop
            double timed = timediff(StartuSec, EnduSec);
            if (timed < BCLoopTime)
                BCLoopTime = timed;
            if (timed > WCLoopTime)
                WCLoopTime = timed;
            TotalLoopTime += timed;
        }

        if (hangDetected) {
            DumpStateAfterHang();
            break; // terminate the loop
        }

         // we block again to wait for everyone to finish before doing cpu-intensive work
        if (_block)
           LWGpuResource::GetTestDirectory()->Block(s_SyncPointVec[TEST_RUN].get(), s_SyncPointVec[TEST_RUN+1].get());

        // check if the results are correct
        if (_timemode)
            StartuSec = Platform::GetTimeUS();

        TestStatus status = TEST_SUCCEEDED;

        if(deviceStatus == FAILED)
            status = TEST_FAILED;
        else
        {
            if (_doCheck)
                status = CheckPassFail(); // do a CRC check or something similar
            if (hangDetected)
                status = TEST_FAILED_TIMEOUT;
        }

         // When the test is failed manually, don't set back to SUCCEEDED.
         if (GetStatus() != TEST_FAILED)
            SetStatus(status);
         if (GetStatus() != TEST_SUCCEEDED) {
            _endTest = true;
            ErrPrintf("%s: test failed! Ending test.\n", GetTestName());
            // to include crc checking

            if (Platform::IsPhysFunMode() || Platform::IsVirtFunMode())
            {
                // In such case, need disable SRIOV sync-up for tests to unblock others.
                InfoPrintf(SRIOVTestSyncTag "ConlwrrentTest::%s: Test failed, disable test sync-up.\n",
                    __FUNCTION__);
            }
            VmiopMdiagElwManager::Instance()->StopTestSync();

            break;
        }

        if (_timemode) {
            EnduSec = Platform::GetTimeUS();
            TotalCheckPFTime += timediff(StartuSec, EnduSec);
        }

        // go through a shutdown sequence before starting over
        if (_timemode)
            StartuSec = Platform::GetTimeUS();

        EndIteration(); // some devices need to shutdown before they can restart

        if (_timemode) {
            EnduSec = Platform::GetTimeUS();
            TotalShutdownTime += timediff(StartuSec, EnduSec);
        }

        if (_reportProgress!=0 && testloops%_reportProgress==0) {
            InfoPrintf("%s progress report: \titeration %d\n", GetTestName(), testloops);
        }
    } while (!_endTest);

    if (_timemode) {
        float AvgInitTime = (float) (TotalInitTime / testloops);
        float AvgLoopTime = (float) (TotalLoopTime / testloops);
        float AvgCheckPFTime = (float) (TotalCheckPFTime / testloops);
        float AvgShutdownTime = (float) (TotalShutdownTime / testloops);

        const char* testname = GetTestName();
        InfoPrintf("%s: Test timing statistics:\n", testname);
        InfoPrintf("%s: Number of test iterations     = %d\n", testname, testloops);
        InfoPrintf("%s: Total exelwtion loop time     = %f sec\n", testname, TotalLoopTime);
        InfoPrintf("%s: Average exelwtion loop time   = %f sec\n", testname, AvgLoopTime);
        InfoPrintf("%s: Worst Case loop time          = %f sec\n", testname, WCLoopTime);
        InfoPrintf("%s: Best Case loop time           = %f sec\n", testname, BCLoopTime);
        InfoPrintf("%s: Average Init time             = %f sec\n", testname, AvgInitTime);
        InfoPrintf("%s: Average Data Checking time    = %f sec\n", testname, AvgCheckPFTime);
        InfoPrintf("%s: Average Shutdown time         = %f sec\n", testname, AvgShutdownTime);
        InfoPrintf("%s: Number of times polled        = %d\n", testname, NumTimesPolled);
        InfoPrintf("%s: Number of times busy          = %d\n", testname, NumTimesBusy);
        InfoPrintf("%s: Busy ratio                    = %f\n", testname, ((float)NumTimesBusy)/NumTimesPolled);

        ReportPerfStatistics();
    }

    if(_Trigger_LA)
    {
        if(_Trigger_Addr <= 0xffff)
        {
            UINT32 data = 0;
            InfoPrintf("Attention! Run-Finish Triggering the logic analiser at IO READ(0x%x)\n",_Trigger_Addr);
            Platform::PioRead32(_Trigger_Addr, &data);
        }
        else
        {
            InfoPrintf("Attention! Run-Finish Triggering the logic analiser at MEM READ(0x%x)\n",_Trigger_Addr);
            MEM_RD32(_Trigger_Addr);
        }
    }
}

/* virtual */const char* ConlwrrentTest::GetTestName()
{
    return "Conlwrrent Test";
}

