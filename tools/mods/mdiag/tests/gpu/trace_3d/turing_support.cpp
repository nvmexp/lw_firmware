/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
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
#include "class/clc597.h" // TURING_A

RC TraceChannel::SetupTuringA(LWGpuSubChannel* pSubch)
{
    RC rc;

    DebugPrintf("Setting up initial TuringA Graphics on gpu %d:0\n",
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

        CHECK_RC(pSubch->MethodWriteRC(LWC597_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC597_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC597_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWC597_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(FalconMethodWrite(pSubch, LWC597_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL), LWC597_NO_OPERATION)); 
        
        CHECK_RC(pSubch->MethodWriteRC(LWC597_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC597_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC597_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWC597_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(FalconMethodWrite(pSubch, LWC597_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_GCC_RM_PAGEPOOL), LWC597_NO_OPERATION));
    }

    if (params->ParamPresent("-coarse_shading"))
    {
        const UINT32 setShadingRateIndexSurfaceSizeA =
            DRF_NUM(C597, _SET_SHADING_RATE_INDEX_SURFACE_SIZE_A, _WIDTH, 0) |
            DRF_NUM(C597, _SET_SHADING_RATE_INDEX_SURFACE_SIZE_A, _HEIGHT, 0);

        const UINT32 setVariablePixelShadingControl =
            DRF_DEF(C597, _SET_VARIABLE_PIXEL_RATE_SHADING_CONTROL,
                _ENABLE, _TRUE);

        UINT32 setVariablePixelRateShadingIndexToRateA = 0;

        const auto coarseShading = static_cast<CoarseShading>(
            params->ParamUnsigned("-coarse_shading"));

        switch (coarseShading)
        {
            case COARSE_SHADING_1X2:
            {
                setVariablePixelRateShadingIndexToRateA |=
                    DRF_DEF(C597,
                        _SET_VARIABLE_PIXEL_RATE_SHADING_INDEX_TO_RATE_A,
                        _RATE_INDEX0 , _PS_X1_PER_1X2_RASTER_PIXELS);
                break;
            }
            case COARSE_SHADING_2X1:
            {
                setVariablePixelRateShadingIndexToRateA |=
                    DRF_DEF(C597,
                        _SET_VARIABLE_PIXEL_RATE_SHADING_INDEX_TO_RATE_A,
                        _RATE_INDEX0 , _PS_X1_PER_2X1_RASTER_PIXELS);
                break;
            }
            case COARSE_SHADING_2X2:
            {
                setVariablePixelRateShadingIndexToRateA |=
                    DRF_DEF(C597,
                        _SET_VARIABLE_PIXEL_RATE_SHADING_INDEX_TO_RATE_A,
                        _RATE_INDEX0 , _PS_X1_PER_2X2_RASTER_PIXELS);
                break;
            }
            case COARSE_SHADING_2X4:
            {
                setVariablePixelRateShadingIndexToRateA |=
                    DRF_DEF(C597,
                        _SET_VARIABLE_PIXEL_RATE_SHADING_INDEX_TO_RATE_A,
                        _RATE_INDEX0 , _PS_X1_PER_2X4_RASTER_PIXELS);
                break;
            }
            case COARSE_SHADING_4X2:
            {
                setVariablePixelRateShadingIndexToRateA |=
                    DRF_DEF(C597,
                        _SET_VARIABLE_PIXEL_RATE_SHADING_INDEX_TO_RATE_A,
                        _RATE_INDEX0 , _PS_X1_PER_4X2_RASTER_PIXELS);
                break;
            }
            case COARSE_SHADING_4X4:
            {
                setVariablePixelRateShadingIndexToRateA |=
                    DRF_DEF(C597,
                        _SET_VARIABLE_PIXEL_RATE_SHADING_INDEX_TO_RATE_A,
                        _RATE_INDEX0 , _PS_X1_PER_4X4_RASTER_PIXELS);
                break;
            }
            default:
            {
                MASSERT(!"Invalid coarse shading parameter");
            }
        }

        CHECK_RC(pSubch->MethodWriteRC(
                LWC597_SET_SHADING_RATE_INDEX_SURFACE_SIZE_A(0),
                setShadingRateIndexSurfaceSizeA));

        const auto numberOfViewports =
            sizeof (turing_a_struct().SetVariablePixelRateShading) /
            sizeof (turing_a_struct().SetVariablePixelRateShading[0]);

        for (auto i = decltype(numberOfViewports){0};
             i < numberOfViewports; ++i)
        {
            CHECK_RC(pSubch->MethodWriteRC(
                    LWC597_SET_VARIABLE_PIXEL_RATE_SHADING_CONTROL(i),
                    setVariablePixelShadingControl));
            CHECK_RC(pSubch->MethodWriteRC(
                    LWC597_SET_VARIABLE_PIXEL_RATE_SHADING_INDEX_TO_RATE_A(i),
                    setVariablePixelRateShadingIndexToRateA));
        }
    }

    DebugPrintf("Done setting up initial TuringA Graphics state\n");
    return rc;
}
