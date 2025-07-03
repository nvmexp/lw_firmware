/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "tracechan.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "class/clc397.h" // VOLTA_A
#include "class/clc497.h" // VOLTA_B
#include "teegroups.h"

#define MSGID() T3D_MSGID(ChipSupport)

RC TraceChannel::SetupVoltaA(LWGpuSubChannel* pSubch)
{
    RC rc;

    DebugPrintf("Setting up initial VoltaA Graphics on gpu %d:0\n",
        m_Test->GetBoundGpuDevice()->GetDeviceInst());

    // tell cascade calls to pre pascal setup to ignore pagepool arg
    m_bUsePascalPagePool = true;

    CHECK_RC(SetupVoltaTrapHandle(pSubch));
    CHECK_RC(SetupMaxwellB(pSubch));

    if (params->ParamPresent("-pagepool_size"))
    {
        UINT32 poolSize = params->ParamUnsigned("-pagepool_size");
        UINT32 data, mask;
        RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

        //
        // HW now wants SCC and GCC to tak the full value sent in option as-is
        // with the Valid bit flipped ON
        //
        data = poolSize |
               regHal.SetField(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_VALID_TRUE);
        mask = 0xFFFFFFFF;

        CHECK_RC(pSubch->MethodWriteRC(LWC397_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(FalconMethodWrite(pSubch, LWC397_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL), LWC397_NO_OPERATION));

        CHECK_RC(pSubch->MethodWriteRC(LWC397_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(FalconMethodWrite(pSubch, LWC397_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_GCC_RM_PAGEPOOL), LWC397_NO_OPERATION));
    }

    DebugPrintf("Done setting up initial VoltaA Graphics state\n");
    return rc;
}

RC TraceChannel::SetupVoltaTrapHandle(LWGpuSubChannel * pSubch)
{
    // after volta ignore pre-volta setup via offset
    m_bSetupTrapHanderVa = true;

    return OK;
}
