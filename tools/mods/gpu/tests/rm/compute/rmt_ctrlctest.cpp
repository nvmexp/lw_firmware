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

#include "gpu/tests/lwdastst.h"
#include "core/include/jscript.h"
#include <math.h>
#include <float.h>
#include "lwda_etbl/tools_rm.h"
#include <lwca.h>
#include "lwos.h"
#include "lwRmApi.h"
#include "ctrl/ctrl2080/ctrl2080fifo.h"
#include "core/include/platform.h"
#include "gpu/tests/rmtest.h"
#include "core/include/utility.h"

static void LaunchLwdaTest(void *);

//-----------------------------------------------------------------------------
//! LWCA-based Ctrlc Test test CtrlC exit...
//!
class LwdaCtrlc : public LwdaStreamTest
{
public:
    LwdaCtrlc();
    virtual ~LwdaCtrlc();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    string IsTestSupported();
    virtual RC Run();
    virtual RC Cleanup();
    RC ControlCTest();

private:
    Lwca::Module  m_Module;
    LwRm::Handle m_hClient;
    LwRm::Handle m_hChannel;
    ModsEvent *m_pSyncWithLwKernel;
    ModsEvent *m_pSyncThreadExit;
    RC m_rcStatusCtrlcThread;
    LWtoolsContextHandlesRm m_ContextHandlesRm;
};

//-----------------------------------------------------------------------------
// JS linkage for test.
JS_CLASS_INHERIT(LwdaCtrlc, LwdaStreamTest, "test CtrlC exit");
//-----------------------------------------------------------------------------

LwdaCtrlc::LwdaCtrlc()
{
    SetName("LwdaCtrlc");
    m_hClient = 0;
    m_hChannel = 0;
    m_pSyncWithLwKernel = NULL;
    m_pSyncThreadExit = NULL;
    m_rcStatusCtrlcThread = OK;
    m_ContextHandlesRm = {};
}

LwdaCtrlc::~LwdaCtrlc()
{
}
//-----------------------------------------------------------------------------
void LwdaCtrlc::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);
}

//-----------------------------------------------------------------------------
RC LwdaCtrlc::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());
    return rc;
}

string LwdaCtrlc::IsTestSupported()
{
    if(Platform::GetSimulationMode() != Platform::Hardware)
        return "Test is not supported under simulation See Bug 770698 for more info\n";

    return RUN_RMTEST_TRUE;
}
//-----------------------------------------------------------------------------
RC LwdaCtrlc::Run()
{
    Printf(Tee::PriDebug,"Entering LwdaCtrlc::Run \n");
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_RC_INFO_PARAMS newRcInfoParams = {};

    newRcInfoParams.rcMode = LW2080_CTRL_CMD_RC_INFO_MODE_ENABLE;
    newRcInfoParams.rcBreak = LW2080_CTRL_CMD_RC_INFO_BREAK_DISABLE;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_SET_RC_INFO,
                                &newRcInfoParams, sizeof(newRcInfoParams)));


    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("ctrlc", &m_Module));

    // get rm handles for compute
    CHECK_RC(GetLwdaInstance().GetComputeEngineChannel(&m_hClient,
                                                       &m_hChannel));

    m_ContextHandlesRm. struct_size = sizeof(LWtoolsContextHandlesRm);
    CHECK_RC(GetLwdaInstance().GetContextHandles(&m_ContextHandlesRm));

    m_pSyncWithLwKernel = Tasker::AllocEvent("SyncWithLwdaKernel");
    m_pSyncThreadExit = Tasker::AllocEvent("SyncThreadExit");;
    if (!m_pSyncWithLwKernel)
    {
        Printf(Tee::PriHigh,"%s:Allocation of Mods Event failed \n",__FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Tasker::CreateThread(LaunchLwdaTest,this ,0, "SigKillThread");

    const UINT32  BlocksPerGrid = 512;
    const UINT32  ThreadsInBlock= 128;
    const UINT32  totalSize = BlocksPerGrid * ThreadsInBlock;

    Lwca::Function    funcCtrlCTest;
    funcCtrlCTest = m_Module.GetFunction("ctrlctest");
    CHECK_RC(funcCtrlCTest.InitCheck());

    Lwca::DeviceMemory devID;
    Lwca::DeviceMemory devOD;

    CHECK_RC(GetLwdaInstance().AllocDeviceMem(totalSize * sizeof(UINT32), &devID));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(totalSize * sizeof(UINT32), &devOD));

    funcCtrlCTest.SetBlockDim(ThreadsInBlock);
    funcCtrlCTest.SetGridDim(BlocksPerGrid);

    // Launch Infinite Lwca Kernel
    CHECK_RC(funcCtrlCTest.Launch(devID.GetDevicePtr(), devOD.GetDevicePtr()));

    Printf(Tee::PriDebug, "%s:infinite lwca kernel running\n",__FUNCTION__);

    // run kernel for some time
    Tasker::Sleep(2000);

    // Set the Event inorder to call Sigkill
    Tasker::SetEvent(m_pSyncWithLwKernel);

    //Wait for the Lwca Kernel to return.. Block on lwca kernel call to finish
    rc = GetLwdaInstance().Synchronize();
    if (rc != OK)
    {
        Printf(Tee::PriDebug,
            "%s: Expected error as we Preemepted the compute channel"
            "RC recovery has been happend rc = %d\n",
            __FUNCTION__, (UINT32)rc);
        // Clear the error rc
        rc.Clear();
    }

    CHECK_RC(Tasker::WaitOnEvent(m_pSyncThreadExit));

    if (m_rcStatusCtrlcThread != OK)
    {
        Printf(Tee::PriHigh,"%s: rcPreemptOnChannelFree Failed"
            "rc=%d \n",__FUNCTION__,(UINT32)rc);

        rc = m_rcStatusCtrlcThread;
    }

    // Cleanup
    devID.Free();
    devOD.Free();
    m_Module.Unload();
    Tasker::FreeEvent(m_pSyncWithLwKernel);
    Tasker::FreeEvent(m_pSyncThreadExit);

    m_pSyncWithLwKernel = NULL;

    Printf(Tee::PriDebug,"%s : Exit \n",__FUNCTION__);

    return rc;
}

RC LwdaCtrlc::ControlCTest()
{
    RC rc;
    UINT32  rmStatus;

    // Wait On Event for Lwca infinite kernel to Launch
    CHECK_RC(Tasker::WaitOnEvent(m_pSyncWithLwKernel));

    Printf(Tee::PriHigh,"%s: Preempt the Compute Channel and do \n"
        "rcErrorRecoveryLite for Client:0x%x Channel: 0x%x \n",
        __FUNCTION__, (UINT32) m_hClient, (UINT32)m_hChannel);

    // call Preempt RmControl call here
    LW2080_CTRL_FIFO_CHANNEL_PREEMPTIVE_REMOVAL_PARAMS fifoPreemptParams;
    memset(&fifoPreemptParams, 0, sizeof(fifoPreemptParams));
    fifoPreemptParams.hChannel = m_hChannel;

    rmStatus = LwRmControl(m_ContextHandlesRm.rmClient,/*Lwca client handle*/
                           m_ContextHandlesRm.rmSubDevice,
                           LW2080_CTRL_CMD_FIFO_CHANNEL_PREEMPTIVE_REMOVAL,
                           &fifoPreemptParams,
                           sizeof(fifoPreemptParams));

    rc = RmApiStatusToModsRC(rmStatus);
    if ( rc != OK)
    {
        Printf(Tee::PriHigh,"%s: rcChannelPreemptiveRemoval"
            "failed need ilwestigation status rc= %d \n",__FUNCTION__,(UINT32)rc);
        // Free the Client ..
        LwRmFree(m_hClient, m_hClient, m_hClient);
    }

    Printf(Tee::PriDebug," Exit: %s \n",__FUNCTION__);

    m_rcStatusCtrlcThread = rc;

    // Set the Event Done with the RC recovery path .. Now Signal Main thread.
    Tasker::SetEvent(m_pSyncThreadExit);
    return rc;
}
void LaunchLwdaTest(void *p)
{
    LwdaCtrlc *pLwdaCtrlc = (LwdaCtrlc *)p;
    pLwdaCtrlc->ControlCTest();
    return;
}

//-----------------------------------------------------------------------------
RC LwdaCtrlc::Cleanup()
{
    RC rc;
    rc= LwdaStreamTest::Cleanup();
    rc.Clear();
    return rc;
}
