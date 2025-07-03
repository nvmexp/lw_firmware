/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "lwgpu_single.h"
#include "lwgpu_ch_helper.h"
#include "lwgpu_subchannel.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/utl/utl.h"
#include "mdiag/smc/smcengine.h"

#define TSTR "LWGpuSingleChannelTest:"

LWGpuSingleChannelTest::LWGpuSingleChannelTest(ArgReader *params) :
    LoopingTest(params), 
    m_pParams(params) 
{
    m_pLwRm = LwRmPtr().Get();

    chHelper = mk_GpuChannelHelper(TSTR, NULL, this, m_pLwRm, m_pSmcEngine);

    if (!chHelper)
    {
        ErrPrintf(TSTR "create GpuChannelHelper failed\n");
    }
    else if ( ! chHelper->ParseChannelArgs(params) )
    {
        ErrPrintf(TSTR ": Channel Helper ParseChannelArgs failed\n");
        delete chHelper;
        chHelper = nullptr;
    }
}

LWGpuSingleChannelTest::~LWGpuSingleChannelTest(void)
{
    if (chHelper)
        delete chHelper;
    if (m_pParams)
        delete m_pParams;
}

bool LWGpuSingleChannelTest::SetupLWGpuResource
(
    UINT32 numClasses,
    const UINT32 classIds[],
    const string &smcEngineLabel
)
{
    // the goal is to find an LWGpu we can share that has a channel available
    // if we're context switching, allocate the LWGpu as a shared resource
    chHelper->set_class_ids(numClasses, classIds);

    if (!chHelper->acquire_gpu_resource()) 
    {
        ErrPrintf("Can't aquire GPU\n");
        return false;
    }

    lwgpu = chHelper->gpu_resource();

    m_pSmcEngine = lwgpu->GetSmcEngine(smcEngineLabel);

    if (m_pSmcEngine)
    {
        m_pLwRm = lwgpu->GetLwRmPtr(m_pSmcEngine);
    }

    chHelper->SetSmcEngine(GetSmcEngine());
    chHelper->SetLwRmPtr(GetLwRmPtr());

    if (!chHelper->acquire_channel()) 
    {
        ErrPrintf("Channel Create failed\n");
        chHelper->release_gpu_resource();
        return false;
    }

    return true;
}

int LWGpuSingleChannelTest::SetupSingleChanTest
(
    UINT32 engineId
)
{
    MASSERT(lwgpu);
    
    ch = chHelper->channel();

    ch->ParseChannelArgs(m_pParams);

    // Create a default VA space for the channel.
    LWGpuResource::VaSpaceManager *vaSpaceMgr = lwgpu->GetVaSpaceManager(GetLwRmPtr());

    // Sequence ID is one-based, but the test ID used for accessing
    // VA spaces is zero-based.
    UINT32 testId = static_cast<UINT32>(GetSeqId()) - 1;
    shared_ptr<VaSpace> vaSpace = vaSpaceMgr->CreateResourceObject(
        testId, "default");
    ch->SetVASpace(vaSpace);

    if (!chHelper->alloc_channel(engineId))
    {
        ErrPrintf("Channel Alloc failed\n");
        chHelper->release_channel();
        chHelper->release_gpu_resource();
        return 0;
    }

    if (Utl::HasInstance())
    {
        ch->SetName("single");
        Utl::Instance()->AddTestChannel(this, ch, ch->GetName(), nullptr);
    }

    return 1;
}

void LWGpuSingleChannelTest::CleanUp(void)
{
    // clean up is easy - nuke the channel and release the LWGpu
    if(ch) {
        DebugPrintf("LWGpuSingleChannelTest::CleanUp(): Deleting channel.\n");
        ch->WaitForIdle();
        chHelper->release_channel();
    }

    if(lwgpu) {
        DebugPrintf("LWGpuSingleChannelTest::CleanUp(): Deleting vaspace.\n");
        LWGpuResource::VaSpaceManager *vaSpaceMgr = lwgpu->GetVaSpaceManager(GetLwRmPtr());
        UINT32 testId = static_cast<UINT32>(GetSeqId()) - 1;
        vaSpaceMgr->FreeObjectInTest(testId);

        DebugPrintf("LWGpuSingleChannelTest::CleanUp(): Releasing GPU.\n");
        chHelper->release_gpu_resource();
    }

    lwgpu = nullptr;
    ch = nullptr;
}

void LWGpuSingleChannelTest::PreRun(void)
{
    MASSERT(lwgpu);
    LWGpuContexScheduler::Pointer scheduler = lwgpu->GetContextScheduler();
    if (scheduler)
    {
        RC rc;
        rc = scheduler->BindThreadToTest(this);
        if (OK != rc)
        {
            ErrPrintf("BindThreadToTest failed: %s\n", rc.Message());
            MASSERT(0);
        }
        rc = scheduler->AcquireExelwtion();
        if (OK != rc)
        {
            ErrPrintf("AcquireExelwtion failed: %s\n", rc.Message());
            MASSERT(0);
        }
    }

    chHelper->BeginChannelSwitch();
}

void LWGpuSingleChannelTest::PostRun(void)
{
    MASSERT(lwgpu);

    chHelper->EndChannelSwitch();

    LWGpuContexScheduler::Pointer scheduler = lwgpu->GetContextScheduler();
    if (scheduler)
    {
        RC rc;
        rc = scheduler->ReleaseExelwtion();
        if (OK != rc)
        {
            ErrPrintf("ReleaseExelwtion failed: %s\n", rc.Message());
            MASSERT(0);
        }
        rc = scheduler->UnRegisterTestFromContext();
        if (OK != rc)
        {
            ErrPrintf("UnRegisterTestFromContext failed: %s\n", rc.Message());
            MASSERT(0);
        }
    }

    // Enable Ecover dumping
    Test::PostRun();
}

