/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011, 2014, 2016-2018, 2020 by LWPU Corporation.  All 
 * rights reserved.  All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// Most logic for gf11x is in fermi_support.cpp

#include "mdiag/tests/stdtest.h"
#include "trace_3d.h"
#include "mdiag/utils/utils.h"
#include "lwos.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpusbdev.h"
#include "class/cl9297.h" // FERMI_C
#include "teegroups.h"

#define MSGID() T3D_MSGID(ChipSupport)

RC TraceChannel::SetupFermi_c(LWGpuSubChannel* pSubch)
{
    RC rc = OK;
    GpuDevice* pGpuDev = m_Test->GetBoundGpuDevice();
    UINT32 subdevNum = pGpuDev->GetNumSubdevices();
    LwRm::Handle hCtx = pSubch->channel()->ChannelHandle();
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

    if (!m_bUsePascalPagePool && params->ParamPresent("-pagepool_size"))
    {
        // PD_RM_PAGEPOOL should be programmed whenever SCC_RM_PAGEPOOL is programmed
        UINT32 poolSize = params->ParamUnsigned("-pagepool_size");
        UINT32 poolTotal, poolMax;
        UINT32 i, data, mask;

        poolTotal = regHal.GetField(poolSize, MODS_PGRAPH_PRI_PD_RM_PAGEPOOL_TOTAL_PAGES);
        poolMax = regHal.GetField(poolSize, MODS_PGRAPH_PRI_PD_RM_PAGEPOOL_MAX_VALID_PAGES);

        // PD takes TOTAL_PAGES and MAX_VALID_PAGES
        data = regHal.SetField(MODS_PGRAPH_PRI_PD_RM_PAGEPOOL_TOTAL_PAGES, poolTotal) |
               regHal.SetField(MODS_PGRAPH_PRI_PD_RM_PAGEPOOL_MAX_VALID_PAGES, poolMax);
        mask = regHal.LookupMask(MODS_PGRAPH_PRI_PD_RM_PAGEPOOL_TOTAL_PAGES) |
               regHal.LookupMask(MODS_PGRAPH_PRI_PD_RM_PAGEPOOL_MAX_VALID_PAGES);
        CHECK_RC(pSubch->MethodWriteRC(LW9297_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_PD_RM_PAGEPOOL)));
        for(i=0; i<10; i++) CHECK_RC(pSubch->MethodWriteRC(LW9297_NO_OPERATION, 0));
    }

    if (params->ParamPresent("-visible_early_z") > 0)
    {
        for (UINT32 subdev = 0; subdev < subdevNum; ++subdev)
        {
            UINT32 regVal = 0;
            GpuSubdevice *pGpuSubdev = pGpuDev->GetSubdevice(subdev);
            CHECK_RC(pGpuSubdev->CtxRegRd32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PPCS_WWDX_CONFIG), &regVal, GetLwRmPtr()));
            regHal.SetField(&regVal, MODS_PGRAPH_PRI_GPCS_PPCS_WWDX_CONFIG_FORCE_PRIMS_TO_ALL_GPCS_TRUE);
            CHECK_RC(pGpuSubdev->CtxRegWr32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PPCS_WWDX_CONFIG), regVal, GetLwRmPtr()));
            DebugPrintf(MSGID(), "LW_PGRAPH_PRI_GPCS_PPCS_WWDX_CONFIG(%x)=%x\n", regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PPCS_WWDX_CONFIG), regVal);
        }
    }

    return SetupFermi(pSubch);
}

// Method massager for fermi_c class only
UINT32 GenericTraceModule::MassageFermi_cMethod(UINT32 subdev, UINT32 Method, UINT32 Data)
{
    UINT32 lwr_method = Method<<2;

    // Watch out for option -no_pb_massage, we're not checking that option here
    // since we're not change method yet.
    switch (lwr_method)
    {
        default:
            // leave it alone by default
            break;
        case LW9297_SET_TEX_HEADER_EXTENDED_DIMENSIONS:
            if (FLD_TEST_DRF(9297, _SET_TEX_HEADER_EXTENDED_DIMENSIONS, _ENABLE, _FALSE, Data))
            {
                if (params->ParamPresent("-allow_ext_texheader_disabled") !=0)
                {
                    WarnPrintf("Extended dimension is disabled in the texture header!\n");
                }
                else
                {
                    // Need a better way to exit
                    MASSERT(!"MODS requires LW9297_SET_TEX_HEADER_EXTENDED_DIMENSIONS_ENABLE to be TRUE!\n");
                }
            }
            break;
    }

    return MassageFermiMethod(subdev, Method, Data);
}

