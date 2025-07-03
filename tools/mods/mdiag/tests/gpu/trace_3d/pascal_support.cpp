/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
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
#include "class/clc097.h" // PASCAL_A

RC TraceChannel::SetupPascalA(LWGpuSubChannel* pSubch)
{
    RC rc;

    DebugPrintf("Setting up initial PascalA Graphics on gpu %d:0\n",
        m_Test->GetBoundGpuDevice()->GetDeviceInst());

    // tell cascade calls to pre pascal setup to ignore pagepool arg
    m_bUsePascalPagePool = true;

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

        CHECK_RC(pSubch->MethodWriteRC(LWC097_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(FalconMethodWrite(pSubch, LWC097_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL), LWC097_NO_OPERATION)); 

        CHECK_RC(pSubch->MethodWriteRC(LWC097_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(FalconMethodWrite(pSubch, LWC097_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_GCC_RM_PAGEPOOL), LWC097_NO_OPERATION));
    }

    DebugPrintf("Done setting up initial PascalA Graphics state\n");
    return rc;
}

RC TraceChannel::SetZbcXChannelRegPascal(LWGpuSubChannel *subchannel)
{
    RC rc = OK;
    GpuDevice *gpuDev = m_Test->GetBoundGpuDevice();
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

    if (gpuDev->GetSubdevice(0)->Regs().IsSupported(MODS_PGRAPH_PRI_GPCS_SWDX_DSS_ROP_DEBUG))
    {
        // Use an inline falcon firmware to set the
        // LW_PGRAPH_PRI_GPCS_SWDX_DSS_ROP_DEBUG register properly on a per-context
        // basis.  See bug 561737.
        UINT32 data = regHal.SetField(MODS_PGRAPH_PRI_GPCS_SWDX_DSS_ROP_DEBUG_X_CHANNEL_ONLY_MATCH_0_ENABLE);
        UINT32 mask = regHal.LookupMask(MODS_PGRAPH_PRI_GPCS_SWDX_DSS_ROP_DEBUG_X_CHANNEL_ONLY_MATCH_0);
        CHECK_RC(subchannel->MethodWriteRC(LWC097_WAIT_FOR_IDLE, 0));
        CHECK_RC(subchannel->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(subchannel->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(subchannel->MethodWriteRC(LWC097_SET_MME_SHADOW_SCRATCH(2), mask));

        CHECK_RC(FalconMethodWrite(subchannel, LWC097_SET_FALCON04, 
            regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_SWDX_DSS_ROP_DEBUG), 
            LWC097_NO_OPERATION));
    }

    return rc;
}

UINT32 GenericTraceModule::GetPascalCbsize(UINT32 gpuSubdevIdx) const
{
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, m_Test->GetLwRmPtr(), m_Test->GetSmcEngine());
    return min(
            regHal.Read32(MODS_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_BETA_CBSIZE),
            regHal.Read32(MODS_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_ALPHA_CBSIZE)
            );
}

