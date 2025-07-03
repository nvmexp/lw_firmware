/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "class/clc3c0.h" // VOLTA_COMPUTE_A
#include "class/clcbc0.h" // HOPPER_COMPUTE_A
#include "tracechan.h"
#include "pe_configurefile.h"
#include "tracetsg.h"
#include "teegroups.h"
#include "mdiag/utils/lwgpu_classes.h"

#define MSGID() T3D_MSGID(ChipSupport)

RC TraceChannel::SetupNonThrottledC(LWGpuSubChannel * pSubch)
{
    RC rc;

    if(params->ParamPresent("-partition_table"))
    {
        shared_ptr<SubCtx> pSubCtx = GetSubCtx();
        if(pSubCtx.get() &&
           pSubCtx->GetTraceTsg()->GetPartitionMode() == PEConfigureFile::DYNAMIC)
        {
            // Compute channel in a tsg is selected in runlist sequence,
            // we can't promise the 1st setup channel is scheduled before others.
            // So we need to send this nonthrottledC method to every channel in the subctx.
            UINT32 data = 0;
            data = DRF_NUM(C3C0, _SET_SHADER_LOCAL_MEMORY_NON_THROTTLED_C, _MAX_SM_COUNT, pSubCtx->GetMaxTpcCount());
            if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_HOPPER))
            {
                data |= DRF_NUM(CBC0, _SET_SHADER_LOCAL_MEMORY_NON_THROTTLED_C, 
                    _MAX_SINGLETON_TPC_GPC_COUNT, pSubCtx->GetMaxSingletonTpcGpcCount());
                data |= DRF_NUM(CBC0, _SET_SHADER_LOCAL_MEMORY_NON_THROTTLED_C, 
                    _MAX_PLURAL_TPC_GPC_COUNT, pSubCtx->GetMaxPluralTpcGpcCount());
            }
            pSubch->MethodWriteRC(LWC3C0_SET_SHADER_LOCAL_MEMORY_NON_THROTTLED_C, data);

            Printf(Tee::PriNormal, "SubCtx name = %s.\n", pSubCtx->GetName().c_str());
            Printf(Tee::PriNormal, "Set LMEM NonThrottledC value = 0x%04x.\n", data);
        }
    }

    return rc;
}

RC TraceChannel::SetupVoltaCompute(LWGpuSubChannel * pSubch)
{
    RC rc;

    DebugPrintf("Setting up initial Volta Compute on gpu %d:0\n",
               m_Test->GetBoundGpuDevice()->GetDeviceInst());

    CHECK_RC(SetupVoltaComputeTrapHandler(pSubch));
    CHECK_RC(SetupNonThrottledC(pSubch));
    CHECK_RC(SetupKeplerCompute(pSubch));

    DebugPrintf("Done setting up initial Volta Compute state\n");

    return rc;
}

RC TraceChannel::SetupVoltaComputeTrapHandler(LWGpuSubChannel * pSubch)
{
    // indicat to pre-volta setup functions to use offset
    // after volta setup functions to use va
    m_bSetupTrapHanderVa = true;

    return OK;
}
