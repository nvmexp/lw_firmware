/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"

#include "gpu/include/gpudev.h"
#include "lwmisc.h"
#include "class/cl90c0.h" // FERMI_COMPUTE_A
#include "class/clc7c0.h" // AMPERE_COMPUTE_B
#include "teegroups.h"

#define MSGID() T3D_MSGID(ChipSupport)

RC TraceChannel::SetupFermiCompute(LWGpuSubChannel* pSubch)
{
    DebugPrintf("Setting up initial Fermi Compute on gpu %d:0\n",
               m_Test->GetBoundGpuDevice()->GetDeviceInst());
    MASSERT(params);

    RC rc = OK;
    // LW_PGRAPH_PRI_GPCS_TPCS_SM_SCH_MODE is defined for gf10x
    // and later chip. Gf10x silicon will require the register
    // to be programmed differently in compute vs graphics.
    // For compute, _TEX_HASH_BLOCK_VTG_COMPUTE_DISABLE
    // For graphics, _TEX_HASH_BLOCK_VTG_COMPUTE_ENABLE
    UINT32 data, mask, i;
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

    data = regHal.SetField(MODS_PGRAPH_PRI_GPCS_TPCS_SM_SCH_MODE_TEX_HASH_BLOCK_VTG_COMPUTE_DISABLE);
    mask = regHal.LookupMask(MODS_PGRAPH_PRI_GPCS_TPCS_SM_SCH_MODE_TEX_HASH_BLOCK_VTG_COMPUTE);
    CHECK_RC(pSubch->MethodWriteRC(LW90C0_WAIT_FOR_IDLE, 0));
    CHECK_RC(pSubch->MethodWriteRC(LW90C0_SET_MME_SHADOW_SCRATCH(0), 0));
    CHECK_RC(pSubch->MethodWriteRC(LW90C0_SET_MME_SHADOW_SCRATCH(1), data));
    CHECK_RC(pSubch->MethodWriteRC(LW90C0_SET_MME_SHADOW_SCRATCH(2), mask));
    CHECK_RC(pSubch->MethodWriteRC(LW90C0_SET_FALCON04,
                                   regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_SCH_MODE)));
    for (i = 0; i < 10; i++)
    {
        CHECK_RC(pSubch->MethodWriteRC(LW90C0_NO_OPERATION, 0));
    }

    DebugPrintf("Done setting up initial Fermi Compute state\n");
    return rc;
}

UINT32 GenericTraceModule::MassageFermiComputeMethod(UINT32 subdev, UINT32 Method, UINT32 Data)
{
    Method <<= 2;
    DebugPrintf(MSGID(), "MassageFermiComputeMethod 0x%04x : 0x%08x\n", Method, Data);

    switch (Method)
    {
    default:
        // leave it alone by default
        break;
    case LW90C0_NOTIFY:
        // tests that use notify typically write to Color Target 0
        m_Test->GetSurfaceMgr()->SetSurfaceUsed(ColorSurfaceTypes[0], true, subdev);
        if (m_Trace->params->ParamPresent("-notifyAwaken"))
        {
            Data = FLD_SET_DRF(90C0, _NOTIFY, _TYPE, _WRITE_THEN_AWAKEN, Data);
            InfoPrintf("Patching NOTIFY for AWAKEN\n");
        }
        break;
    case LWC7C0_REPORT_SEMAPHORE_EXELWTE:
        if (params->ParamPresent("-semaphoreAwaken") > 0)
        {
            Data = FLD_SET_DRF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                               _AWAKEN_ENABLE, _TRUE, Data);
            InfoPrintf("Patching SEMAPHORE for AWAKEN\n");
        }
        break;
    case LW90C0_SET_REPORT_SEMAPHORE_D:
        if (params->ParamPresent("-semaphoreAwaken") > 0)
        {
            Data = FLD_SET_DRF(90C0, _SET_REPORT_SEMAPHORE_D,
                               _AWAKEN_ENABLE, _TRUE, Data);
            InfoPrintf("Patching SEMAPHORE for AWAKEN\n");
        }
        break;
    case LW90C0_SET_WORK_DISTRIBUTION:
        // We maybe need to hadle this, but it's hard to align to tesla since spec changes
        break;
    case LW90C0_LAUNCH:
        m_LaunchMethodCount++;
        break;
    case LW90C0_SET_CTA_RASTER_SIZE_A:
        if (m_LaunchMethodCount < m_CtaRasterWidth.size())
        {
            UINT32 newData = FLD_SET_DRF_NUM(90C0, _SET_CTA_RASTER_SIZE_A, _WIDTH,
                m_CtaRasterWidth[m_LaunchMethodCount], Data);
            InfoPrintf("-nset_cta_raster specified [launch#%d]. old data(0x%08x) replaced with new data(0x%08x)\n",
                m_LaunchMethodCount, Data, newData);
            Data = newData;
        }
        if (m_LaunchMethodCount < m_CtaRasterHeight.size())
        {
            UINT32 newData = FLD_SET_DRF_NUM(90C0, _SET_CTA_RASTER_SIZE_A, _HEIGHT,
                m_CtaRasterHeight[m_LaunchMethodCount], Data);
            InfoPrintf("-nset_cta_raster specified [launch#%d]. old data(0x%08x) replaced with new data(0x%08x)\n",
                m_LaunchMethodCount, Data, newData);
            Data = newData;
        }
        break;
    case LW90C0_SET_CTA_RASTER_SIZE_B:
        if (m_LaunchMethodCount < m_CtaRasterDepth.size())
        {
            UINT32 newData = FLD_SET_DRF_NUM(90C0, _SET_CTA_RASTER_SIZE_B, _DEPTH,
                m_CtaRasterDepth[m_LaunchMethodCount], Data);
            InfoPrintf("-nset_cta_raster specified [launch#%d]. old data(0x%08x) replaced with new data(0x%08x)\n",
                m_LaunchMethodCount, Data, newData);
            Data = newData;
        }
        break;
    }

    return Data;
}
