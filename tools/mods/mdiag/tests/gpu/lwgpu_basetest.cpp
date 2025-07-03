/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "lwgpu_basetest.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"

extern const ParamDecl lwgpu_basetest_ctx_params[] =
{
    PARAM_SUBDECL(GpuChannelHelper::Params),

    LAST_PARAM
};

extern const ParamConstraints lwgpu_basetest_ctx_constraints[] =
{
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSeed0"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSeed1"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSeed2"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswPoint"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswGran"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswNum"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSelf"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswReset"),

    LAST_CONSTRAINT
};

LWGpuBaseTest::LWGpuBaseTest(ArgReader *params) :
    Test()
{
    m_pParams = params;

    m_chHelper = mk_GpuChannelHelper("<unknown>", NULL, this, LwRmPtr().Get());
    if ( ! m_chHelper )
    {
        ErrPrintf("create GpuChannelHelper failed\n");
        return;
    }

    if ( ! m_chHelper->ParseChannelArgs(params) )
    {
        ErrPrintf("ParseChannelArgs failed\n");
        delete m_chHelper;
        m_chHelper = NULL;
        return;
    }

    //m_useCtxSwitch = useCtxSwitch;

}

LWGpuBaseTest::~LWGpuBaseTest(void)
{
    delete m_pParams;

    if (m_chHelper)
        delete m_chHelper;
}

LWGpuResource* LWGpuBaseTest::FindGpuForClass
(
    UINT32 numClasses,
    const UINT32 classIds[]
)
{
    return LWGpuResource::GetGpuByClassSupported(numClasses, classIds);
}

