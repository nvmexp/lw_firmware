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
#include "class/cla097.h" // KEPLER_A
#include "class/cla297.h" // KEPLER_C
#include "class/clc397.h" // VOLTA_A
#include "kepler/gk104/dev_graphics_nobundle.h"
#include "teegroups.h"
#include "mdiag/utils/lwgpu_classes.h"

#define MSGID() T3D_MSGID(ChipSupport)

RC TraceChannel::SetupKepler(LWGpuSubChannel* pSubch)
{
    RC rc = OK;

    CHECK_RC(SetupFermi_c(pSubch));

    DebugPrintf("Setting up initial Kepler Graphics on gpu %d:0\n",
               m_Test->GetBoundGpuDevice()->GetDeviceInst());

    if (GetPreemptive())
    {
        // Program buffer address & size through firmware method
        MdiagSurf* preemptCtxBuf = GetCh()->GetPreemptCtxBuf();
        UINT64 offset = preemptCtxBuf->GetCtxDmaOffsetGpu();
        offset >>= 8;
        UINT32 data1 = LwU64_LO32(offset);
        UINT32 data2 = preemptCtxBuf->GetAllocSize();
        CHECK_RC(pSubch->MethodWriteRC(LWA097_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWA097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWA097_SET_MME_SHADOW_SCRATCH(1), data1));
        CHECK_RC(pSubch->MethodWriteRC(LWA097_SET_MME_SHADOW_SCRATCH(2), data2));
        if (!EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_VOLTA))
        {
            CHECK_RC(pSubch->MethodWriteRC(LWA097_SET_FALCON06, 0));
        }
        for (UINT32 i = 0; i < 10; i++)
        {
            CHECK_RC(pSubch->MethodWriteRC(LWA097_NO_OPERATION, 0));
        }
    }

    GenericTraceModule* pMod = m_Trace->GetTrapHandlerModule(m_hVASpace);
    if(pMod)
    {
        if (!m_bSetupTrapHanderVa)
        {
            UINT32 offset = (UINT32)pMod->GetOffsetWithinDmaBuf();
            DebugPrintf(MSGID(), "Setting Kepler graphics trap handler offset = 0x%x\n",offset);
            CHECK_RC(pSubch->MethodWriteRC(LWA097_SET_TRAP_HANDLER, offset));
        }
        else
        {
            UINT64 acturalVa = pMod->GetOffset();
            UINT32 lowerBitsVa = LwU64_LO32(acturalVa);
            UINT32 upperBitsVa = LwU64_HI32(acturalVa); // LWC397_SET_TRAP_HANDLER_A_ADDRESS_UPPER [16:0]
            DebugPrintf(MSGID(), "Setting Volta graphics trap handler va = 0x%llx\n",acturalVa);
            UINT32 addressLower = DRF_NUM(C397, _SET_TRAP_HANDLER_B, _ADDRESS_LOWER, lowerBitsVa);
            UINT32 addressUpper = DRF_NUM(C397, _SET_TRAP_HANDLER_A, _ADDRESS_UPPER, upperBitsVa);
            CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_TRAP_HANDLER_A, addressUpper));
            CHECK_RC(pSubch->MethodWriteRC(LWC397_SET_TRAP_HANDLER_B, addressLower));
        }
    }

    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());
    if (params->ParamPresent("-trap_handler") &&
        regHal.IsSupported(MODS_PGRAPH_PRI_GPCS_TPCS_SM_DBGR_CONTROL0_DEBUGGER_MODE_ON))
    {
        regHal.Write32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_DBGR_CONTROL0_DEBUGGER_MODE_ON);
    }

    DebugPrintf("Done setting up initial Kepler Graphics state\n");
    return rc;
}

UINT32 GenericTraceModule::GetKeplerCbsize(UINT32 gpuSubdevIdx) const
{
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, m_Test->GetLwRmPtr(), m_Test->GetSmcEngine());
    if (regHal.IsSupported(MODS_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC))
    {
        UINT32 regVal = regHal.Read32(MODS_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC);
        // Not using regHal below since it conflicts with the new register causing 
        // build failure in reghal
        return min(
            REF_VAL(LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_BETA_CBSIZE, regVal),
            REF_VAL(LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_ALPHA_CBSIZE, regVal));
    }
    return 0;
}

RC TraceChannel::SendPostTraceMethodsKepler(LWGpuSubChannel* pSubch)
{
    RC rc;

    if (params->ParamPresent("-decompress_in_place") > 0)
    {
        for (UINT32 target = 0; target < MAX_RENDER_TARGETS; target++)
        {
            MdiagSurf* surf = m_Test->surfC[target];
            if (m_Test->GetSurfaceMgr()->GetValid(surf))
            {
                for (UINT32 index = 0; index < surf->GetArraySize(); index++)
                {
                    UINT32 value = DRF_NUM(A297, _DECOMPRESS_SURFACE,
                        _MRT_SELECT, target);
                    value |= DRF_NUM(A297, _DECOMPRESS_SURFACE,
                        _RT_ARRAY_INDEX, index);

                    CHECK_RC(pSubch->MethodWriteRC(
                        LWA297_DECOMPRESS_SURFACE, value));
                }
            }
        }
    }

    return rc;
}
