/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef CONLWRRENT_TEST_H
#define CONLWRRENT_TEST_H

#include "test.h"
#include "mdiag/thread.h"

// A conlwrrent test is one which can run with very little CPU overhead.
// Ideally, this CPU overhead would be done in an interrupt handler so that
// the device could be kept going even when the CPU is busy doing something
// else (like CRC check of another test).

// common args for all conlwrrent tests
//     - block
//     - forever
//     - time
extern const ParamDecl conlwrrentParams[];

class ConlwrrentTest : public Test
{
public:
    // The conlwrrent test should define enums which define the set of states for that device.
    // It must minimally include BUSY and FINISHED.
    enum { BUSY = 0,
           FINISHED = 1,
           FAILED = 2,
           START_OF_OTHER_ENUMS = 3
    };

protected:
    // Setup() and CleanUp are inherited from class Test.
    // Setup should allocate resources and do one-time initialization.
    // Cleanup should deallocate the resources.
    // See class Test for more details.

    // Run() can not be overridden (it is made private)
    // You can specify any of the virtual functions which Run() calls.
    // The pseudo-code for Run() is included because the implementation
    // details are important.

protected:
    // Test3D calls it
    virtual void Run();
    /*
        forever_loop {
            int deviceStatus = init(); // to allow different initialization after each CRC check (e.g. randomization)
            if (block) Block(); // we can optionally synchronize with other conlwrrent tests

            do {
                deviceStatus = stimulateDevice(deviceStatus); // send commands to device
                while (deviceStatus == BUSY) {
                    Yield();
                    deviceStatus = getDeviceStatus(); // wait until device can accept more commands
                }
            } while (deviceStatus != FINISHED); // no more commands to send
            
            if (block) Block();

            if (deviceStatus != INTENTIONAL_HANG)
                checkPassFail(); // do a CRC check or something similar

            shutdown(); // some devices need to shutdown before they can restart
        }
    */

public:
    /////////////////////////////
    // This pure virtual method must contain the test initialization that
    // needs to be done at the beginning of each iteration. Typically,
    // memory allocation is done once, and thus does not go in this
    // virtual method. But memory initialization with for example
    // random data does go into this method.
    virtual int BeginIteration() = 0;

    /////////////////////////////
    // This pure virtual method contains the command stream generation
    // for the device, which can be dependent on the state of
    // the device. This method does not read the device status, that
    // is an input to the method, but it can change the status of the
    // device. An example is the audio device. This method would
    // contain the delivery of the audio commands to the device, and
    // a final go, after which the device will indicate that it is
    // busy.
    virtual int StimulateDevice(int deviceStatus) = 0;

    //////////////////////////////
    // This pure virtual method can only obtain the status of the device.
    // It is not allowed to modify the device status in any way.
    virtual int GetDeviceStatus() = 0;

    //////////////////////////////
    // This virtual method is a generic hook to properly shutdown
    // a device prior to starting a test. or a test iteration.
    // Default is an empty implementation.
    virtual void EndIteration() {};

    //////////////////////////////
    // This pure virtual method is intended to be used as the hook
    // to allow the test to check itself. CRC checks would go here.
    virtual TestStatus CheckPassFail() = 0;

    //////////////////////////////
    // This method is for informational messages only.
    // It is used to identify which test does the printing.
    // The string returned should not have a newline.
    virtual const char* GetTestName();

    //////////////////////////////
    // This virtual method is for debugging a hang.
    // By default, it does nothing. The intent is for the test
    // to dump information helpful in debugging the hang.
    // It might prefer dumping this info to a file to avoid
    // file flushing problems in DOS when redirecting stdout.
    virtual void DumpStateAfterHang() {}

    //////////////////////////////
    // This virtual method is for reporting performance
    // statistics for each test which might be specific
    // the device doing transfer. This is besides the
    // conlwrrency statistics printed in
    // conlwrrentTest::Run
    virtual void ReportPerfStatistics() {}

    bool IsBlockEnabled() const { return _block; }

protected:
    ConlwrrentTest();
    virtual ~ConlwrrentTest();
    void ParseConlwrrentArgs(class ArgReader *args);
    FLOAT64 GetTimeoutMs() { return m_TimeoutMs; }
    bool _Trigger_LA;
    UINT32 _Trigger_Addr;

private:
    enum SyncPoints 
    {
        TEST_INIT = 0,
        TEST_RUN = 1,
        TEST_END = 2
    };
    virtual void EndTest() { _endTest = true; } // private because it should not be overridden

    int _reportProgress;
    bool _block;
    bool _endTest;
    bool _timemode;
    bool _doCheck;
    FLOAT64 m_TimeoutMs;
    static vector<unique_ptr<Thread::BlockSyncPoint>> s_SyncPointVec;
};

extern const ParamDecl conlwrrentParams[];

#endif /* CONLWRRENT_TEST_H */
