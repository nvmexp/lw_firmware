/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2013, 2019-2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _LWGPU_BASETEST_H_
#define _LWGPU_BASETEST_H_

// parent class for any LWGpu test that intends to use just a single channel
//  takes care of finding an lwgpu based on desired classes, getting a channel,
//  setting up a push buffer if desired

#include "mdiag/tests/test.h"

#include "lwgpu_ch_helper.h"

extern const ParamDecl lwgpu_basetest_ctx_params[];
extern const ParamConstraints lwgpu_basetest_ctx_constraints[];

class ArgReader;
class LWGpuResource;
class LWGpuChannel;

// subclasses from LoopingTest so '-forever' support is there (i.e. you can
//   call KeepLooping()), but you still have to add FOREVER_PARAM to your
//   ParamDecl list in your test

class LWGpuBaseTest : public Test
{
public:
    LWGpuBaseTest(ArgReader *params);

    virtual ~LWGpuBaseTest(void);

    virtual LWGpuResource* FindGpuForClass
    (
        UINT32 numClasses,
        const UINT32 classIds[]
    );

protected:
    ArgReader *m_pParams;

private:
    GpuChannelHelper *m_chHelper;
};

#endif
