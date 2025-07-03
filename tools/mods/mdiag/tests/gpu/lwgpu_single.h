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
#pragma once

// parent class for any LWGpu test that intends to use just a single channel
//  takes care of finding an lwgpu based on desired classes, getting a channel,
//  setting up a push buffer if desired

#include "mdiag/tests/loopingtest.h"
#include "lwgpu_ch_helper.h"
#include "lwgpu_subchannel.h"

#define lwgpu_single_params GpuChannelHelper::Params

class ArgReader;
class LWGpuResource;
class LWGpuChannel;
class LWGpuSubChannel;

// subclasses from LoopingTest so '-forever' support is there (i.e. you can
//   call KeepLooping()), but you still have to add FOREVER_PARAM to your
//   ParamDecl list in your test

class LWGpuSingleChannelTest : public LoopingTest {
public:
    LWGpuSingleChannelTest(ArgReader *params);

    virtual ~LWGpuSingleChannelTest(void);

    bool SetupLWGpuResource
    (
        UINT32 numClasses,
        const UINT32 classIds[],
        const string &smcEngineLabel = UNSPECIFIED_SMC_ENGINE
    );

    virtual int SetupSingleChanTest
    (
        UINT32 engineId
    );

    // Methods from class Test
    virtual void PreRun(void);
    virtual void PostRun(void);
    virtual void CleanUp(void);
    virtual LwRm* GetLwRmPtr() { return m_pLwRm; }
    virtual SmcEngine* GetSmcEngine() { return m_pSmcEngine; }
protected:
    ArgReader *m_pParams;
    LWGpuResource *lwgpu = nullptr;
    LWGpuChannel *ch = nullptr;
    GpuChannelHelper *chHelper = nullptr;
    SmcEngine  *m_pSmcEngine = nullptr;
    LwRm *m_pLwRm = nullptr;
};

