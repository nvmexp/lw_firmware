/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016, 2019-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/clc3c0.h" // VOLTA_COMPUTE_A
#include "teegroups.h"
#include "mdiag/utils/lwgpu_classes.h"

#define MSGID() T3D_MSGID(ChipSupport)

RC TraceChannel::SetupKeplerCompute(LWGpuSubChannel* pSubch)
{
    RC rc = OK;

    DebugPrintf("Setting up initial Kepler Compute on gpu %d:0\n",
               m_Test->GetBoundGpuDevice()->GetDeviceInst());

    CHECK_RC(SetupKeplerComputePreemptive(pSubch));
    CHECK_RC(SetupKeplerComputeTrapHandle(pSubch));

    DebugPrintf("Done setting up initial Kepler Compute state\n");
    return rc;
}

RC TraceChannel::SetupKeplerComputePreemptive(LWGpuSubChannel * pSubch)
{
    RC rc;

    if (GetPreemptive())
    {
        // Program buffer address & size through firmware method
        MdiagSurf* preemptCtxBuf = GetCh()->GetPreemptCtxBuf();
        UINT64 offset = preemptCtxBuf->GetCtxDmaOffsetGpu();
        offset >>= 8;
        UINT32 data1 = LwU64_LO32(offset);
        UINT32 data2 = preemptCtxBuf->GetAllocSize();
        CHECK_RC(pSubch->MethodWriteRC(LWA0C0_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWA0C0_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWA0C0_SET_MME_SHADOW_SCRATCH(1), data1));
        CHECK_RC(pSubch->MethodWriteRC(LWA0C0_SET_MME_SHADOW_SCRATCH(2), data2));
        if (!EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_VOLTA))
        {
            CHECK_RC(pSubch->MethodWriteRC(LWA0C0_SET_FALCON06, 0));
        }
        for (UINT32 i = 0; i < 10; i++)
        {
            CHECK_RC(pSubch->MethodWriteRC(LWA0C0_NO_OPERATION, 0));
        }
    }

    return rc;
}

RC TraceChannel::SetupKeplerComputeTrapHandle(LWGpuSubChannel * pSubch)
{
    RC rc;

    GenericTraceModule* pMod = m_Trace->GetTrapHandlerModule(m_hVASpace);
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

    if(pMod)
    {
        if (!m_bSetupTrapHanderVa)
        {
            UINT32 offset = (UINT32)pMod->GetOffsetWithinDmaBuf();
            DebugPrintf(MSGID(), "Setting Kepler compute trap handler offset = 0x%x\n",offset);
            CHECK_RC(pSubch->MethodWriteRC(LWA0C0_SET_TRAP_HANDLER, offset));
        }
        else
        {
            UINT64 acturalVa = pMod->GetOffset();
            UINT32 lowerBitsVa = LwU64_LO32(acturalVa);
            UINT32 upperBitsVa = LwU64_HI32(acturalVa); // LWC397_SET_TRAP_HANDLER_A_ADDRESS_UPPER [16:0]
            DebugPrintf(MSGID(), "Setting Volta computer trap handler va = 0x%llx\n",acturalVa);
            UINT32 addressLower = DRF_NUM(C3C0, _SET_TRAP_HANDLER_B, _ADDRESS_LOWER, lowerBitsVa);
            UINT32 addressUpper = DRF_NUM(C3C0, _SET_TRAP_HANDLER_A, _ADDRESS_UPPER, upperBitsVa);
            CHECK_RC(pSubch->MethodWriteRC(LWC3C0_SET_TRAP_HANDLER_A, addressUpper));
            CHECK_RC(pSubch->MethodWriteRC(LWC3C0_SET_TRAP_HANDLER_B, addressLower));
        }
    }

    if (params->ParamPresent("-trap_handler"))
    {
        if (regHal.IsSupported(MODS_PGRAPH_PRI_GPCS_TPCS_SM_DBGR_CONTROL0))
        {
            regHal.Write32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_DBGR_CONTROL0_DEBUGGER_MODE_ON);
        }
    }

    return rc;
}
