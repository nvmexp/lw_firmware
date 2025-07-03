/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "tracerel.h"

#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "mdiag/utils/utils.h"
#include "lwmisc.h"
#include "class/cl9097.h" // FERMI_A
#include "class/cl9297.h" // FERMI_C
#include "class/cl90c0.h" // FERMI_COMPUTE_A
#include "class/cl91c0.h" // FERMI_COMPUTE_B
#include "class/cla097.h" // KEPLER_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc797.h" // AMPERE_B
#include "mdiag/utils/lwgpu_classes.h"
#include "ctrl/ctrl2080.h"

#include "lwos.h"
#include "core/include/channel.h"
#include "mdiag/utils/XMLlogging.h"

#include "fermi/gf100/dev_ram.h"
#include "bufferdumper.h"
#include "mdiag/advschd/policymn.h"
#include "slisurf.h"

#include "mdiag/gf100/sph_support.h"
#include "teegroups.h"
#include "mdiag/zbctable.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/utl/utl.h"

#define SUBPIXEL_PRECISION 0.5/4096.0

#define MSGID() T3D_MSGID(ChipSupport)

// Copy one bit from src to dest
static void UtilBitCopy( UINT32 src, UINT32* dest, UINT32 src_bit, UINT32 dest_bit )
{
    UINT32  mask_bit = (1 << (dest_bit%32));
    if( src & (1 << (src_bit%32)) )
    {
        *dest |= mask_bit;
    }
    else
    {
        *dest &= ~mask_bit;
    }
}

static float getZlwllRamFormatAliquotScaleFactor(UINT32 zlwll_ram_format)
{
    switch (zlwll_ram_format)
    {
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_16X16X2_4X4:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_16X16_4X8:
            return 0.5;
            break;
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_ZS_16X16_4X4:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_16X16_4X2:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_16X16_2X4:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_16X8_4X4:
            return 1;
            break;
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_8X8_4X2:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_8X8_2X4:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_ZS_16X8_4X2:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_ZS_16X8_2X4:
            return 2;
            break;

        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_4X8_2X2:
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_ZS_8X8_2X2:
            return 4;
            break;
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_Z_4X8_1X1:
            return 8;
            break;
        case LW9297_SET_ZLWLL_SUBREGION_ALLOCATION_FORMAT_NONE:
            return 0;
            break;
        default:
            ErrPrintf("Not a supported Zlwll format: %d\n",zlwll_ram_format);
            return -1;
            break;
    }
    return -1;
}

namespace
{
    bool IsPmTriggerMethod(UINT32 address)
    {
        return (address == LW9097_PM_TRIGGER) ||
               (address == LWA097_PM_TRIGGER_WFI) ||
               (address == LW9097_PM_TRIGGER_END);
    }
}

RC TraceChannel::SetupFermi(LWGpuSubChannel* pSubch)
{
    RC rc = OK;
    DebugPrintf("Setting up initial Fermi state on gpu %d:0\n",
               m_Test->GetBoundGpuDevice()->GetDeviceInst());
    MASSERT(params);
    LwRm* pLwRm = GetLwRmPtr();
    LwRm::Handle hCtx = pSubch->channel()->ChannelHandle();
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

    if ((params->ParamPresent("-blend_zero_times_x_is_zero") && !params->ParamPresent("-blend_allow_float_pixel_kills")) ||
        (!params->ParamPresent("-blend_zero_times_x_is_zero") && params->ParamPresent("-blend_allow_float_pixel_kills")))
    {
        ErrPrintf("Option '-blend_zero_times_x_is_zero' and '-blend_allow_float_pixel_kills' "
            " need to present at the same time.\n");
        MASSERT(!"See bug 372149 for details");
    }

    MASSERT(GetGrSubChannel() != 0);

    // -vab_none is -vab_size param group member #4
    //
    bool skipVabSetup =( ( params->ParamPresent("-vab_size") &&
                           params->ParamUnsigned("-vab_size") == 4
                          ) || m_IgnoreVAB);
    if (!skipVabSetup)
        CHECK_RC(SetupFermiVabBuffer(pSubch));

    if (params->ParamPresent("-pm_ilwalidate_cache") > 0)
    {
        UINT32 value = DRF_DEF(9097, _ILWALIDATE_SHADER_CACHES, _DATA, _TRUE);
        value |= DRF_DEF(9097, _ILWALIDATE_SHADER_CACHES, _LOCKS, _TRUE);
        value |= DRF_DEF(9097, _ILWALIDATE_SHADER_CACHES, _FLUSH_DATA, _FALSE);

        CHECK_RC(pSubch->MethodWriteRC(LW9097_ILWALIDATE_SHADER_CACHES, value));
    }

    // GS spill region
    if (params->ParamPresent("-spill_region") > 0)
    {
        m_Test->m_GSSpillRegion.SetProtect(Memory::ReadWrite);
        m_Test->m_GSSpillRegion.SetAlignment(4096);
        m_Test->m_GSSpillRegion.SetArrayPitch(params->ParamUnsigned("-spill_region"));
        m_Test->m_GSSpillRegion.SetColorFormat(ColorUtils::Y8);
        m_Test->m_GSSpillRegion.SetForceSizeAlloc(true);
        SetPteKind(m_Test->m_GSSpillRegion, m_Test->m_GSSpillRegionPteKindName, m_Test->GetBoundGpuDevice());
        CHECK_RC(m_Test->m_GSSpillRegion.Alloc(m_Test->GetBoundGpuDevice(), pLwRm));
        PrintDmaBufferParams(m_Test->m_GSSpillRegion);
        m_Test->m_BuffInfo.SetDmaBuff("GS Spill Region", m_Test->m_GSSpillRegion, 0,
                                      0);

        const UINT64 GSSROffset = m_Test->m_GSSpillRegion.GetCtxDmaOffsetGpu()
            + m_Test->m_GSSpillRegion.GetExtraAllocSize();
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_GS_SPILL_REGION_A,
                    DRF_NUM(9097, _SET_GS_SPILL_REGION_A, _ADDRESS_UPPER,
                        LwU64_HI32(GSSROffset))));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_GS_SPILL_REGION_B,
                    DRF_NUM(9097, _SET_GS_SPILL_REGION_B, _ADDRESS_LOWER,
                        LwU64_LO32(GSSROffset))));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_GS_SPILL_REGION_C,
                    DRF_NUM(9097, _SET_GS_SPILL_REGION_C, _SIZE, m_Test->m_GSSpillRegion.GetSize())));
    }

    GpuDevice* pGpuDev = m_Test->GetBoundGpuDevice();

    if (m_Test->GetSurfaceMgr()->GetNeedClipID())
    {
        MdiagSurf *ClipID = m_Test->GetSurfaceMgr()->GetClipIDSurface();
        for (UINT32 s = 0; s < pGpuDev->GetNumSubdevices(); ++s)
        {
            Channel *modschannel = pSubch->channel()->GetModsChannel();

            // ClipId also could be put in peer
            if (pGpuDev->GetNumSubdevices() > 1)
            {
                CHECK_RC(modschannel->WriteSetSubdevice(1 << s));
            }
            UINT64 peerOffset = m_Test->GetSSM()->GetCtxDmaOffset(ClipID, pGpuDev, s);
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_ID_MEMORY_A, LwU64_HI32(peerOffset)));
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_ID_MEMORY_B, LwU64_LO32(peerOffset)));

            if (pGpuDev->GetNumSubdevices() > 1)
            {
                CHECK_RC(modschannel->WriteSetSubdevice(Channel::AllSubdevicesMask));
            }
        }
    }

    if (!m_Test->GetAllocSurfaceCommandPresent() ||
        params->ParamPresent("-send_rt_methods"))
    {
        CHECK_RC(SendRenderTargetMethods(pSubch));
    }

    if (m_Test->GetSurfaceMgr()->GetMultiSampleOverride())
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ANTI_ALIAS_ENABLE,
            DRF_NUM(9097, _SET_ANTI_ALIAS_ENABLE, _V, m_Test->GetSurfaceMgr()->GetMultiSampleOverrideValue())));
    }
    if (m_Test->GetSurfaceMgr()->GetNonMultisampledZOverride())
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_NONMULTISAMPLED_Z,
            DRF_DEF(9097, _SET_NONMULTISAMPLED_Z, _V, _AT_PIXEL_CENTER)));
    }

    UINT32 width  = m_Test->GetSurfaceMgr()->GetWidth();
    UINT32 height = m_Test->GetSurfaceMgr()->GetHeight();
    UINT32 woffset_x = m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetX();
    UINT32 woffset_y = m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetY();
    UINT32 voffset_x = 0;
    UINT32 voffset_y = 0;
    if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
    {
        UINT32 fullWidth = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
        UINT32 fullHeight = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
        UINT32 offsetX = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
        UINT32 offsetY = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
        voffset_x = fullWidth - width - offsetX;
        voffset_y = fullHeight - height - offsetY;
    }

    // Adjusted for window offset
    UINT32 clip_h =
        DRF_NUM(9097, _SET_SURFACE_CLIP_HORIZONTAL, _X, woffset_x+voffset_x) |
        DRF_NUM(9097, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, width);

    // Adjusted for window offset
    UINT32 clip_v =
        DRF_NUM(9097, _SET_SURFACE_CLIP_VERTICAL, _Y, woffset_y+voffset_y) |
        DRF_NUM(9097, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, height);

    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_HORIZONTAL, clip_h));
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_VERTICAL, clip_v));

    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_WINDOW_OFFSET_X, woffset_x));
    if (params->ParamPresent("-yilw"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_WINDOW_OFFSET_Y, height - woffset_y));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_WINDOW_ORIGIN,
            DRF_DEF(9097, _SET_WINDOW_ORIGIN, _MODE, _LOWER_LEFT) |
            DRF_DEF(9097, _SET_WINDOW_ORIGIN, _FLIP_Y, _FALSE)));
    }
    else
    {
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_WINDOW_OFFSET_Y, woffset_y));
    }

    // block size of the clipid
    UINT32 clip_bl_size = DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _WIDTH, _ONE_GOB) |
                          DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _DEPTH, _ONE_GOB);
    switch (m_Test->GetSurfaceMgr()->GetClipIDBlockHeight())
    {
        case 1:
            clip_bl_size |= DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _HEIGHT, _ONE_GOB);
            break;
        case 2:
            clip_bl_size |= DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _HEIGHT, _TWO_GOBS);
            break;
        case 4:
            clip_bl_size |= DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _HEIGHT, _FOUR_GOBS);
            break;
        case 8:
            clip_bl_size |= DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _HEIGHT, _EIGHT_GOBS);
            break;
        case 16:
            clip_bl_size |= DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _HEIGHT, _SIXTEEN_GOBS);
            break;
        case 32:
            clip_bl_size |= DRF_DEF(9097, _SET_SURFACE_CLIP_ID_BLOCK_SIZE, _HEIGHT, _THIRTYTWO_GOBS);
            break;
        default:
            ErrPrintf("Wrong ClipID block height\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_ID_BLOCK_SIZE, clip_bl_size));

    if (m_Test->scissorEnable)
    {
        InfoPrintf("force scissor enabled: %dx%d+%d+%d\n",
            m_Test->scissorWidth, m_Test->scissorHeight, m_Test->scissorXmin, m_Test->scissorYmin);

        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SCISSOR_HORIZONTAL(0),
            DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMIN, m_Test->scissorXmin) |
            DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMAX, m_Test->scissorXmin+m_Test->scissorWidth)));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SCISSOR_VERTICAL(0),
            DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMIN, m_Test->scissorYmin) |
            DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMAX, m_Test->scissorYmin+m_Test->scissorHeight)));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SCISSOR_ENABLE(0),
            LW9097_SET_SCISSOR_ENABLE_V_TRUE));
    }

    UINT32 viewport_h, viewport_v;
    if (params->ParamPresent("-viewport_clip") > 0)
    {
        viewport_h =
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_HORIZONTAL, _X0,
                    params->ParamNUnsigned("-viewport_clip", 0)) |
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_HORIZONTAL, _WIDTH,
                    params->ParamNUnsigned("-viewport_clip", 1));

        viewport_v =
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_VERTICAL, _Y0,
                    params->ParamNUnsigned("-viewport_clip", 2)) |
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_VERTICAL, _HEIGHT,
                    params->ParamNUnsigned("-viewport_clip", 3));
    }
    else
    {
        viewport_h =
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_HORIZONTAL, _X0, 0) |
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_HORIZONTAL, _WIDTH, width);

        viewport_v =
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_VERTICAL, _Y0, 0) |
            DRF_NUM(9097, _SET_VIEWPORT_CLIP_VERTICAL, _HEIGHT, height);
    }
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_CLIP_HORIZONTAL(0), viewport_h));
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_CLIP_VERTICAL(0), viewport_v));

    if (params->ParamPresent("-viewport_scale") > 0)
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_SCALE_X(0),
                    DRF_NUM(9097, _SET_VIEWPORT_SCALE_X, _V, params->ParamNUnsigned("-viewport_scale", 0))));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_SCALE_Y(0),
                    DRF_NUM(9097, _SET_VIEWPORT_SCALE_Y, _V, params->ParamNUnsigned("-viewport_scale", 1))));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_SCALE_Z(0),
                    DRF_NUM(9097, _SET_VIEWPORT_SCALE_Z, _V, params->ParamNUnsigned("-viewport_scale", 2))));
    }

    if (params->ParamPresent("-viewport_offset") > 0)
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_OFFSET_X(0),
                    DRF_NUM(9097, _SET_VIEWPORT_OFFSET_X, _V, params->ParamNUnsigned("-viewport_offset", 0))));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_OFFSET_Y(0),
                    DRF_NUM(9097, _SET_VIEWPORT_OFFSET_Y, _V, params->ParamNUnsigned("-viewport_offset", 1))));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_OFFSET_Z(0),
                    DRF_NUM(9097, _SET_VIEWPORT_OFFSET_Z, _V, params->ParamNUnsigned("-viewport_offset", 2))));
    }

    if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
    {
        UINT32 fullWidth = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
        UINT32 fullHeight = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
        UINT32 offsetX = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
        UINT32 offsetY = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
        UINT32 SurfaceWidth = m_Test->GetSurfaceMgr()->GetWidth();
        UINT32 SurfaceHeight = m_Test->GetSurfaceMgr()->GetHeight();

        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_WINDOW_CLIP_HORIZONTAL(0),
                    DRF_NUM(9097, _SET_WINDOW_CLIP_HORIZONTAL, _XMIN, fullWidth - offsetX -SurfaceWidth) |
                    DRF_NUM(9097, _SET_WINDOW_CLIP_HORIZONTAL, _XMAX, fullWidth - offsetX)));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_WINDOW_CLIP_VERTICAL(0),
                    DRF_NUM(9097, _SET_WINDOW_CLIP_VERTICAL, _YMIN, fullHeight - offsetY -SurfaceHeight) |
                    DRF_NUM(9097, _SET_WINDOW_CLIP_VERTICAL, _YMAX, fullHeight - offsetY)));

        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SCISSOR_HORIZONTAL(0),
                    DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMIN, fullWidth - offsetX -SurfaceWidth) |
                    DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMAX, fullWidth - offsetX)));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SCISSOR_VERTICAL(0),
                    DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMIN, fullHeight - offsetY -SurfaceHeight) |
                    DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMAX, fullHeight - offsetY)));

        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_CLEAR_RECT_HORIZONTAL,
                    DRF_NUM(9097, _SET_CLEAR_RECT_HORIZONTAL, _XMIN, fullWidth - offsetX -SurfaceWidth) |
                    DRF_NUM(9097, _SET_CLEAR_RECT_HORIZONTAL, _XMAX, fullWidth - offsetX)));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_CLEAR_RECT_VERTICAL,
                    DRF_NUM(9097, _SET_CLEAR_RECT_VERTICAL, _YMIN, fullHeight - offsetY -SurfaceHeight) |
                    DRF_NUM(9097, _SET_CLEAR_RECT_VERTICAL, _YMAX, fullHeight - offsetY)));

        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_HORIZONTAL,
                    DRF_NUM(9097, _SET_SURFACE_CLIP_HORIZONTAL, _X, fullWidth - offsetX -SurfaceWidth) |
                    DRF_NUM(9097, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, SurfaceWidth)));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_VERTICAL,
                    DRF_NUM(9097, _SET_SURFACE_CLIP_VERTICAL, _Y, fullHeight - offsetY -SurfaceHeight) |
                    DRF_NUM(9097, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, SurfaceHeight)));
    }

    // bug 357946, options for SET_VIEWPORT_CLIP_CONTROL
    if ((params->ParamPresent("-geom_clip") > 0) ||
        (params->ParamPresent("-geometry_clip") > 0) ||
        (params->ParamPresent("-pixel_min_z") > 0) ||
        (params->ParamPresent("-pixel_max_z") > 0))
    {
        UINT32 value = 0x0;

        // set pixel_min_z field, default value CLAMP = 1
        value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _PIXEL_MIN_Z,
            _CLAMP, value);
        if ((params->ParamPresent("-pixel_min_z") > 0) &&
            (params->ParamUnsigned("-pixel_min_z") == 0))
            value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _PIXEL_MIN_Z,
                _CLIP, value);

        // set pixel_max_z field, default value CLAMP = 1
        value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _PIXEL_MAX_Z,
            _CLAMP, value);
        if ((params->ParamPresent("-pixel_max_z") > 0) &&
            (params->ParamUnsigned("-pixel_max_z") == 0))
            value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _PIXEL_MAX_Z,
                _CLIP, value);

        // set line_pont_lwll_guardband field default value = 1
        value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _LINE_POINT_LWLL_GUARDBAND,
            _SCALE_1, value);

        // set geometry_clip field, default value FRUSTUM_Z_CLIP = 5
        value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
            _FRUSTUM_Z_CLIP, value);
        if (params->ParamPresent("-geom_clip") > 0)
        {
            switch(params->ParamUnsigned("-geom_clip"))
            {
            case 0:
                value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                    _WZERO_CLIP, value);
                break;
            case 1:
                value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                    _PASSTHRU, value);
                break;
            case 2:
                value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                    _FRUSTUM_XY_CLIP, value);
                break;
            case 3:
                value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                    _FRUSTUM_XYZ_CLIP, value);
                break;
            case 4:
                value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                    _WZERO_CLIP_NO_Z_LWLL, value);
                break;
            case 5:
                value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                    _FRUSTUM_Z_CLIP, value);
                break;
            }
        }

        // another way to set geometry clip field, for backwards compatibility
        if (params->ParamPresent("-geometry_clip"))
        {
            value |= DRF_NUM(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                params->ParamUnsigned("-geometry_clip"));
        }

        // set geometry_guardband_z field, default value SCALE_1 = 2
        value = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_GUARDBAND_Z,
            _SCALE_1, value);

        DebugPrintf(MSGID(), "Injecting LW9097_SET_VIEWPORT_CLIP_CONTROL w/ value 0x%08x\n", value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VIEWPORT_CLIP_CONTROL, value));
    }

    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_CLIP_ID_CLEAR_RECT_HORIZONTAL,
        DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_HORIZONTAL, _XMIN, woffset_x) |
        DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_HORIZONTAL, _XMAX, width + woffset_x)));
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_CLIP_ID_CLEAR_RECT_VERTICAL,
        DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_VERTICAL, _YMIN, woffset_y) |
        DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_VERTICAL, _YMAX, height + woffset_y)));

    UINT32 ClipIdWidth = (width + woffset_x + 15) & ~15;
    UINT32 ClipIdHeight = (height + woffset_y + 3) & ~3;
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_ID_WIDTH, ClipIdWidth));
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SURFACE_CLIP_ID_HEIGHT, ClipIdHeight));

    // Enable/disable Z and stencil lwll
    UINT32 Zlwll = 0;
    if (m_Test->GetSurfaceMgr()->IsZlwllEnabled()) {
        InfoPrintf("Z Lwll enabled\n");
        Zlwll |= DRF_DEF(9097, _SET_ZLWLL, _Z_ENABLE, _TRUE);
    }
    if (m_Test->GetSurfaceMgr()->IsSlwllEnabled()) {
        InfoPrintf("Stencil Lwll enabled\n");
        Zlwll |= DRF_DEF(9097, _SET_ZLWLL, _STENCIL_ENABLE, _TRUE);
        InfoPrintf("Injecting SetZlwll(), data=0x%08x\n", Zlwll);
    }

    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZLWLL, Zlwll));
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZLWLL_BOUNDS, 0));

    UINT32 ZlwllDirFormat = 0;
    if( params->ParamPresent("-set_zlwll_format")>0 )
    {
        ZlwllDirFormat = DRF_NUM(9097, _SET_ZLWLL_DIR_FORMAT, _ZFORMAT,
            params->ParamUnsigned("-set_zlwll_format"));
    }
    else if (ColorUtils::IsFloat(m_Test->surfZ->GetColorFormat())) {
        // For a floating point Z buffer, the ZF32_1 Zlwll format is usually the suitable one
        ZlwllDirFormat |= DRF_DEF(9097, _SET_ZLWLL_DIR_FORMAT, _ZFORMAT, _ZF32_1);
    } else {
        // For a fixed point Z buffer, the FP Zlwll format is usually the most colwenient one
        ZlwllDirFormat |= DRF_DEF(9097, _SET_ZLWLL_DIR_FORMAT, _ZFORMAT, _FP);
    }

    if( params->ParamPresent("-set_zlwll_dir")>0 )
    {
        ZlwllDirFormat |= DRF_NUM(9097, _SET_ZLWLL_DIR_FORMAT, _ZDIR,
            params->ParamUnsigned("-set_zlwll_dir"));
    }
    else
    {
        ZlwllDirFormat |= DRF_DEF(9097, _SET_ZLWLL_DIR_FORMAT, _ZDIR, _LESS);
    }

    // Set the zlwll region if the Z buffer has one
    if (m_Test->surfZ && (m_Test->surfZ->GetZlwllRegion() >= 0))
    {
        AddZLwllId(m_Test->surfZ->GetZlwllRegion());
    }
    typedef set<UINT32>::const_iterator zlwll_ids;
    zlwll_ids ids_end = m_Trace->GetZLwllIds().end();
    for (zlwll_ids iter=m_Trace->GetZLwllIds().begin(); iter!=ids_end; ++iter)
    {
        // Initialize SET_ZLWLL_DIR_FORMAT with the best default values; the trace can override
        InfoPrintf("Injecting SetZlwllDirFormat() for region %d, data=0x%08x\n",
            ZlwllDirFormat, *iter);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ACTIVE_ZLWLL_REGION,
                 REF_NUM(LW9097_SET_ACTIVE_ZLWLL_REGION_ID, *iter)));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZLWLL_DIR_FORMAT, ZlwllDirFormat));
    }
    // Restore the current active zlwll region
    if (m_Trace->GetZLwllIds().size() > 0)
    {
        InfoPrintf("Injecting SetActiveZlwllRegion(), data=0x%x\n", m_Test->surfZ->GetZlwllRegion());
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ACTIVE_ZLWLL_REGION, m_Test->surfZ->GetZlwllRegion()));
    }

    // Set clear color and Z to zero to match back door behaviour (see bug 184715 for details)
    const RGBAFloat clearValue = m_Trace->GetColorClear();
    if (clearValue.IsSet())
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_CLEAR_VALUE(0), clearValue.GetRFloat32()));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_CLEAR_VALUE(1), clearValue.GetGFloat32()));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_CLEAR_VALUE(2), clearValue.GetBFloat32()));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_CLEAR_VALUE(3), clearValue.GetAFloat32()));
    }
    if (m_Trace->GetZetaClear().IsSet())
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_Z_CLEAR_VALUE, m_Trace->GetZetaClear().GetFloat()));
    }
    if (m_Trace->GetStencilClear().IsSet())
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_STENCIL_CLEAR_VALUE, m_Trace->GetStencilClear().Get()));
    }

    bool setZbcReg = false;

    // If any of the global ZBC arguments are specified, call GetZbcTable
    // without using the return value.  This will ensure that the arguments
    // get processed (if they haven't already been in a prior test).
    if (m_Test->GetGpuResource()->HasGlobalZbcArguments())
    {
        m_Test->GetGpuResource()->GetZbcTable(GetLwRmPtr());
        setZbcReg = true;
    }

    // If there are no global ZBC arguments but there are ZBC rendertarget
    // arguments, add a ZBC entry based on the clear values from the trace.
    else
    {
        bool addTraceZbc = false;

        if (params->ParamPresent("-zbc_mode") && (params->ParamUnsigned("-zbc_mode") > 0))
        {
            addTraceZbc = true;
        }
        else if (params->ParamPresent("-zbc_modeC") && (params->ParamUnsigned("-zbc_modeC") > 0))
        {
            addTraceZbc = true;
        }
        else if (params->ParamPresent("-zbc_modeZ") && (params->ParamUnsigned("-zbc_modeZ") > 0))
        {
            addTraceZbc = true;
        }

        if (addTraceZbc)
        {
            ZbcTable *zbcTable = m_Test->GetGpuResource()->GetZbcTable(GetLwRmPtr());
            GpuSubdevice *gpuSubdevice = m_Test->GetBoundGpuDevice()->GetSubdevice(0);
            bool skipL2Table = params->ParamPresent("-zbc_skip_ltc_setup");

            CHECK_RC(zbcTable->AddColorToZbcTable(
                gpuSubdevice,
                m_Test->surfC[0]->GetColorFormat(),
                m_Trace->GetColorClear(),
                GetLwRmPtr(),
                skipL2Table));

            CHECK_RC(zbcTable->AddDepthToZbcTable(
                gpuSubdevice,
                m_Test->surfZ->GetColorFormat(),
                m_Trace->GetZetaClear(),
                GetLwRmPtr(),
                skipL2Table));

            CHECK_RC(zbcTable->AddStencilToZbcTable(
                gpuSubdevice,
                m_Test->surfZ->GetColorFormat(),
                m_Trace->GetStencilClear(),
                GetLwRmPtr(),
                skipL2Table));

            setZbcReg = true;
        }

        // For context-switching tests, the X channel debug register bit
        // must be set on all tests if any of the tests use ZBC.  However,
        // there is no way to know if other tests are using ZBC, so this
        // bit will always be set in context switching mode.  This is
        // only legal because tests which set the ZBC registers directly
        // are not used in context-switching tests.  (See bug 561737)
        else if (m_Test->IsTestCtxSwitching())
        {
            setZbcReg = true;
        }
    }

    if (setZbcReg)
    {
        CHECK_RC(SetZbcXChannelReg(pSubch));
    }

    if (params->ParamPresent("-zlwll_criterion"))
    {
        UINT32 ZlwllCriterion = params->ParamUnsigned("-zlwll_criterion"); // 0xFRRMM
        UINT32 zlwllcriterion =
            DRF_NUM(9097, _SET_ZLWLL_CRITERION, _SFUNC, (ZlwllCriterion >> 16) & 0x7) |
            DRF_NUM(9097, _SET_ZLWLL_CRITERION, _SREF,  (ZlwllCriterion >> 8) & 0xFF) |
            DRF_NUM(9097, _SET_ZLWLL_CRITERION, _SMASK, (ZlwllCriterion & 0xFF));
        InfoPrintf("Injecting SetZlwllCriterion(), data=0x%08x\n", zlwllcriterion);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZLWLL_CRITERION, zlwllcriterion));
    }

    if ( params->ParamPresent("-zlwll_subregions") ||
         params->ParamPresent("-zlwll_allocation")
    )
    {
        if(m_Trace->GetZLwllIds().size() == 0)
        {
             InfoPrintf("Setting Zlwll region to 0 since RM did not allocate a region\n");
             CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ACTIVE_ZLWLL_REGION,
                   REF_NUM(LW9097_SET_ACTIVE_ZLWLL_REGION_ID, 0x0)));
        }
        LW2080_CTRL_GR_GET_ZLWLL_INFO_PARAMS zlwllInfoParams = {0};
        LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(
            m_Test->GetBoundGpuDevice()->GetSubdevice(0));

        RC rc = pLwRm->Control(subdevHandle,
                LW2080_CTRL_CMD_GR_GET_ZLWLL_INFO,
                &zlwllInfoParams, sizeof(zlwllInfoParams));
        if (OK != rc)
        {
            ErrPrintf("Can not get ZlwllIdParams: %s\n",
                    rc.Message());
            return RC::SOFTWARE_ERROR;
        }

        // figure out the number of aliquots it takes to cover the surface
        UINT32 aliquotsAvailable = zlwllInfoParams.aliquotTotal;
        // round Zlwll surface sized to up to something divisble by aliquot width;
        int regionWidth = ((m_Test->GetSurfaceMgr()->GetWidth()+zlwllInfoParams.widthAlignPixels-1)/zlwllInfoParams.widthAlignPixels)*zlwllInfoParams.widthAlignPixels;
        // round Zlwll surface sized to up to something divisble by 64;
        int regionHeight = ((m_Test->GetSurfaceMgr()->GetHeight()+ 63)/64)*64 ;
        // aliquots = #pixels / (pixels per aliquot)
        UINT32 aliquots = (regionWidth * regionHeight) / zlwllInfoParams.pixelSquaresByAliquots;
        // round up 16;
        aliquots = (( aliquots + 15 ) / 16) * 16;

        InfoPrintf("Zlwll region normalized aliquots = %d\n", aliquots);
        UINT32 bundleData;

        UINT32 alignedWidth = ALIGN_UP(m_Test->GetSurfaceMgr()->GetWidth(), 16U);
        bundleData = DRF_NUM(9297, _SET_ZLWLL_REGION_SIZE_A, _WIDTH, alignedWidth);
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_SIZE_A, bundleData));

        UINT32 alignedHeight = ALIGN_UP(m_Test->GetSurfaceMgr()->GetHeight(), 64U);
        bundleData = DRF_NUM(9297, _SET_ZLWLL_REGION_SIZE_B, _HEIGHT, alignedHeight);
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_SIZE_B, bundleData));

        // only cover one layer of a render target array
        bundleData = DRF_NUM(9297, _SET_ZLWLL_REGION_SIZE_C, _DEPTH, 1);
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_SIZE_C, bundleData));

        // always cover starting at the upper left
        bundleData = DRF_NUM(9297, _SET_ZLWLL_REGION_PIXEL_OFFSET_A, _WIDTH, 0);
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_PIXEL_OFFSET_A, bundleData));
        bundleData = DRF_NUM(9297, _SET_ZLWLL_REGION_PIXEL_OFFSET_B, _HEIGHT, 0);
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_PIXEL_OFFSET_B, bundleData));
        bundleData = DRF_NUM(9297, _SET_ZLWLL_REGION_PIXEL_OFFSET_C, _DEPTH, 0);
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_PIXEL_OFFSET_C, bundleData));

        UINT32 totalAliquots = 0;
        if (params->ParamPresent("-zlwll_subregions"))
        {
            // enable Zlwll subregions, and set the number of normalized aliquots
            bundleData =
                    DRF_DEF(9297, _SET_ZLWLL_SUBREGION, _ENABLE, _TRUE) |
                    DRF_NUM(9297, _SET_ZLWLL_SUBREGION, _NORMALIZED_ALIQUOTS, aliquots);
            CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_SUBREGION, bundleData));

            // For each subregion set the allocation as describe in the command line
            string bits_str = params->ParamStr("-zlwll_subregions");
            UINT32 subRegionId = 0;
            while (bits_str != "")
            {
                UINT32 ramFormat;
                string one_pair;
                size_t pos = bits_str.find_first_of(',');
                if (pos == string::npos)
                {
                    // Last pair
                    one_pair = bits_str;
                    bits_str = "";
                }
                else
                {
                    one_pair = bits_str.substr(0, pos);
                    bits_str = bits_str.substr(pos + 1, bits_str.size());
                }

                sscanf(one_pair.c_str(), "%d", &ramFormat);
                // scale the number of aliquots in the region based on the RAM format (round up)
                UINT32 subregionAliquots = (UINT32) ((aliquots / 16)*getZlwllRamFormatAliquotScaleFactor(ramFormat)+0.5);
                InfoPrintf("Allocating Zlwll subregion %d with format %d with %d aliquots: multiply factor %f\n", subRegionId,ramFormat,subregionAliquots,getZlwllRamFormatAliquotScaleFactor(ramFormat));
                bundleData =
                        DRF_NUM(9297, _SET_ZLWLL_SUBREGION_ALLOCATION, _SUBREGION_ID, subRegionId) |
                        DRF_NUM(9297, _SET_ZLWLL_SUBREGION_ALLOCATION, _ALIQUOTS, subregionAliquots) |
                        DRF_NUM(9297, _SET_ZLWLL_SUBREGION_ALLOCATION, _FORMAT, ramFormat);
                CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_SUBREGION_ALLOCATION, bundleData));
                subRegionId++;
                // keep track of the total aliquots from all subregions
                totalAliquots += subregionAliquots;
            }
            // apply the zlwll subregion allocation
            bundleData = DRF_DEF(9297, _ASSIGN_ZLWLL_SUBREGIONS, _ALGORITHM, _Static);
            CHECK_RC(pSubch->MethodWriteRC(LW9297_ASSIGN_ZLWLL_SUBREGIONS, bundleData));
        }
        // Allocation Zlwll without subregions
        else if ( params->ParamPresent("-zlwll_allocation"))
        {
            // ram format is from the command line
            UINT32 ramFormat = params->ParamUnsigned("-zlwll_allocation");
            // scale the number of aliquots based on the RAM format (round up)
            totalAliquots = (UINT32)(aliquots *getZlwllRamFormatAliquotScaleFactor(ramFormat)+0.5f);
            InfoPrintf("Setting Zlwll ram format to %d\n", ramFormat);
            bundleData =
                    DRF_NUM(9297, _SET_ZLWLL_REGION_FORMAT, _TYPE, ramFormat);
            CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_FORMAT, bundleData));

        }
        if(totalAliquots > aliquotsAvailable)
        {
            InfoPrintf("clamping aliquots in region from %d to %d",totalAliquots, aliquotsAvailable);
            totalAliquots = aliquotsAvailable;
        }
        InfoPrintf("Allocating Zlwll region with %d aliquots\n", totalAliquots );
        bundleData =
            DRF_NUM(9297, _SET_ZLWLL_REGION_ALIQUOTS, _PER_LAYER, totalAliquots );
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_ALIQUOTS, bundleData));
        bundleData =
            DRF_NUM(9297, _SET_ZLWLL_REGION_LOCATION, _START_ALIQUOT, 0) |
            DRF_NUM(9297, _SET_ZLWLL_REGION_LOCATION, _ALIQUOT_COUNT, totalAliquots );
        CHECK_RC(pSubch->MethodWriteRC(LW9297_SET_ZLWLL_REGION_LOCATION, bundleData));

    }

    //
    // ForceVisible will now use an inline falcon firmware method to set
    // the registers properly on a per-context basis, since they happen in
    // the normal method stream and will alter only the active chanel!
    //
    if (params->ParamPresent("-ForceVisible"))
    {
        UINT32 i{0}, data{0}, mask{0}, regAddr{0};
        // ROP PRI register definitions are changed since Ampere, read from manual file first
        auto lwgpu = GetGpuResource();

        // LW_PGRAPH_PRI_BES_ZROP_DEBUG1 Register exists
        // This register does exist for chips > Pascal
        if (0 == lwgpu->GetRegNum("LW_PGRAPH_PRI_BES_ZROP_DEBUG1", &regAddr))
        {
            auto reg = lwgpu->GetRegister("LW_PGRAPH_PRI_BES_ZROP_DEBUG1");
            unique_ptr<IRegisterField> field;
            if ((field = reg->FindField("LW_PGRAPH_PRI_BES_ZROP_DEBUG1_ZRD_ZREAD")))
            {
                lwgpu->GetRegFldDef("LW_PGRAPH_PRI_BES_ZROP_DEBUG1", "_ZRD_ZREAD", "_FORCE_ZREAD", &data);
                data <<= field->GetStartBit();
                mask = field->GetMask();

                CHECK_RC(pSubch->MethodWriteRC(LW9097_WAIT_FOR_IDLE, 0));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(0), 0));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(1), data));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(2), mask));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_FALCON04, regAddr));
                for(i=0; i<10; i++) CHECK_RC(pSubch->MethodWriteRC(LW9097_NO_OPERATION, 0));
            }
        }

        data = mask = regAddr = 0;
        if (0 == lwgpu->GetRegNum("LW_PGRAPH_PRI_GPCS_ROPS_ZROP_DEBUG2", &regAddr))
        {
            auto reg = lwgpu->GetRegister("LW_PGRAPH_PRI_GPCS_ROPS_ZROP_DEBUG2");
            auto field = reg->FindField("LW_PGRAPH_PRI_GPCS_ROPS_ZROP_DEBUG2_ZWR_FORCE_VISIBLE");
            lwgpu->GetRegFldDef("LW_PGRAPH_PRI_GPCS_ROPS_ZROP_DEBUG2", "_ZWR_FORCE_VISIBLE", "_ENABLE", &data);
            data <<= field->GetStartBit();
            mask = field->GetMask();
        }
        else if (regHal.IsSupported(MODS_PGRAPH_PRI_BES_ZROP_DEBUG2))
        {
            data = regHal.SetField(MODS_PGRAPH_PRI_BES_ZROP_DEBUG2_ZWR_FORCE_VISIBLE_ENABLE);
            mask = regHal.LookupMask(MODS_PGRAPH_PRI_BES_ZROP_DEBUG2_ZWR_FORCE_VISIBLE);
            regAddr = regHal.LookupAddress(MODS_PGRAPH_PRI_BES_ZROP_DEBUG2);
        }

        if (regAddr != 0)
        {
            CHECK_RC(pSubch->MethodWriteRC(LW9097_WAIT_FOR_IDLE, 0));
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(0), 0));
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(1), data));
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(2), mask));
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_FALCON04, regAddr));
            for (i = 0; i < 10; i++) 
            {
                CHECK_RC(pSubch->MethodWriteRC(LW9097_NO_OPERATION, 0));
            }
        }

        // make this call to handle any book keeping at other levels
        CHECK_RC(m_Test->GetSurfaceMgr()->SetupForceVisible());
    }

    if (!m_bUsePascalPagePool && params->ParamPresent("-pagepool_size"))
    {
        UINT32 poolSize = params->ParamUnsigned("-pagepool_size");
        UINT32 poolTotal, poolMax;
        UINT32 i, data, mask;

        poolTotal = regHal.GetField(poolSize, MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_TOTAL_PAGES);
        poolMax = regHal.GetField(poolSize, MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_MAX_VALID_PAGES);

        // SCC takes TOTAL_PAGES and MAX_VALID_PAGES
        data = regHal.SetField(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_TOTAL_PAGES, poolTotal) |
               regHal.SetField(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_MAX_VALID_PAGES, poolMax);
        mask = regHal.LookupMask(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_TOTAL_PAGES) |
               regHal.LookupMask(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_MAX_VALID_PAGES);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL)));
        for(i=0; i<10; i++) CHECK_RC(pSubch->MethodWriteRC(LW9097_NO_OPERATION, 0));

        // GCC only take TOTAL_PAGES
        data = regHal.SetField(MODS_PGRAPH_PRI_SCC_RM_PAGEPOOL_TOTAL_PAGES, poolTotal);
        mask = regHal.LookupMask(MODS_PGRAPH_PRI_GPCS_GCC_RM_PAGEPOOL_TOTAL_PAGES);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_GCC_RM_PAGEPOOL)));
        for(i=0; i<10; i++) CHECK_RC(pSubch->MethodWriteRC(LW9097_NO_OPERATION, 0));
    }

    // bug# 357796, reduction threshold options
    if (params->ParamPresent("-reduce_thresh_u8") > 0)
    {
        UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_u8", 0);
        UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_u8", 1);
        UINT32 value = 0;

        value = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM8, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
        value |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM8, _ALL_COVERED, consVal);

        DebugPrintf(MSGID(), "Sending method LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM8 with value: %d\n", value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM8, value));
    }

    if (params->ParamPresent("-reduce_thresh_fp16") > 0)
    {
        UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_fp16", 0);
        UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_fp16", 1);
        UINT32 value = 0;

        value = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP16, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
        value |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP16, _ALL_COVERED, consVal);

        DebugPrintf(MSGID(), "Sending method LW9097_SET_REDUCE_COLOR_THRESHOLDS_FP16 with value: %d\n", value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_REDUCE_COLOR_THRESHOLDS_FP16, value));
    }

    if (params->ParamPresent("-reduce_thresh_u10") > 0)
    {
        UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_u10", 0);
        UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_u10", 1);
        UINT32 value = 0;

        value = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM10, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
        value |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM10, _ALL_COVERED, consVal);

        DebugPrintf(MSGID(), "Sending method LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM10 with value: %d\n", value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM10, value));
    }

    if (params->ParamPresent("-reduce_thresh_u16") > 0)
    {
        UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_u16", 0);
        UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_u16", 1);
        UINT32 value = 0;

        value = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM16, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
        value |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM16, _ALL_COVERED, consVal);

        DebugPrintf(MSGID(), "Sending method LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM16 with value: %d\n", value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM16, value));
    }

    if (params->ParamPresent("-reduce_thresh_fp11") > 0)
    {
        UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_fp11", 0);
        UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_fp11", 1);
        UINT32 value = 0;

        value = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP11, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
        value |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP11, _ALL_COVERED, consVal);

        DebugPrintf(MSGID(), "Sending method LW9097_SET_REDUCE_COLOR_THRESHOLDS_FP11 with value: %d\n", value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_REDUCE_COLOR_THRESHOLDS_FP11, value));
    }

    if (params->ParamPresent("-reduce_thresh_srgb8") > 0)
    {
        UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_srgb8", 0);
        UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_srgb8", 1);
        UINT32 value = 0;

        value = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_SRGB8, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
        value |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_SRGB8, _ALL_COVERED, consVal);

        DebugPrintf(MSGID(), "Sending method LW9097_SET_REDUCE_COLOR_THRESHOLDS_SRGB8 with value: %d\n", value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_REDUCE_COLOR_THRESHOLDS_SRGB8, value));
    }

    // bug# 383085, alpha_covg options for fermi
    if (params->ParamPresent("-alpha_coverage_aa") > 0 ||
        params->ParamPresent("-alpha_coverage_psmask") > 0)
    {
        UINT32 value = 0x0;
        if (params->ParamPresent("-alpha_coverage_aa") > 0 &&
            params->ParamUnsigned("-alpha_coverage_aa") == 0)
            value = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE,
                _QUALIFY_BY_ANTI_ALIAS_ENABLE, _DISABLE, value);
        else
            value = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE,
                 _QUALIFY_BY_ANTI_ALIAS_ENABLE, _ENABLE, value);

        if (params->ParamPresent("-alpha_coverage_psmask") > 0 &&
            params->ParamUnsigned("-alpha_coverage_psmask") == 0)
            value = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE,
                _QUALIFY_BY_PS_SAMPLE_MASK_OUTPUT, _DISABLE, value);
        else
            value = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE,
                _QUALIFY_BY_PS_SAMPLE_MASK_OUTPUT, _ENABLE, value);

        DebugPrintf(MSGID(), "Writing method LW9097_SET_ALPHA_TO_COVERAGE_OVERRIDE with value: 0x%x\n",
            value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ALPHA_TO_COVERAGE_OVERRIDE, value));
    }

    if (params->ParamPresent("-alpha_coverage_dither") > 0)
    {
        switch (params->ParamUnsigned("-alpha_coverage_dither"))
        {
            case 0:
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ALPHA_TO_COVERAGE_DITHER_CONTROL,
                            DRF_DEF(9097, _SET_ALPHA_TO_COVERAGE_DITHER_CONTROL, _DITHER_FOOTPRINT, _PIXELS_1X1)));
                break;
            case 1:
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ALPHA_TO_COVERAGE_DITHER_CONTROL,
                            DRF_DEF(9097, _SET_ALPHA_TO_COVERAGE_DITHER_CONTROL, _DITHER_FOOTPRINT, _PIXELS_2X2)));
                break;
            case 2:
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ALPHA_TO_COVERAGE_DITHER_CONTROL,
                            DRF_DEF(9097, _SET_ALPHA_TO_COVERAGE_DITHER_CONTROL, _DITHER_FOOTPRINT, _PIXELS_1X1_VIRTUAL_SAMPLES)));
                break;
            default:
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-hybrid_passes") > 0 || params->ParamPresent("-hybrid_centroid") > 0)
    {
        UINT32 passes = params->ParamUnsigned("-hybrid_passes", 1);
        UINT32 value = DRF_VAL(9097, _SET_HYBRID_ANTI_ALIAS_CONTROL, _PASSES, passes & 0xF);

        // check if -hybrid_centroid specified, default to 0=FRAGMENT
        if (params->ParamPresent("-hybrid_centroid") > 0 && params->ParamUnsigned("-hybrid_centroid") == 1)
        {
            value = FLD_SET_DRF(9097, _SET_HYBRID_ANTI_ALIAS_CONTROL, _CENTROID, _PER_PASS, value);
        }

        if (GetGrSubChannel()->GetClass() < MAXWELL_B)
        {
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_HYBRID_ANTI_ALIAS_CONTROL, value));
        }
        else
        {
            if (passes >= 16)
            {
                value = FLD_SET_DRF_NUM(B197, _SET_HYBRID_ANTI_ALIAS_CONTROL, _PASSES_EXTENDED, 1, value);
            }

            CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_HYBRID_ANTI_ALIAS_CONTROL, value));
        }
    }

    if (params->ParamPresent("-srgb_write"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SRGB_WRITE,
            DRF_DEF(9097, _SET_SRGB_WRITE, _ENABLE, _TRUE)));
    }

    if ((ZLwllEnum)params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED) == ZLWLL_NONE)
    {
        // Use an inline falcon firmware method to set 0x00000000 to the
        // LW_PGPC_PRI_ZLWLL_REGION_PERMISSION register to turn off Zlwll activity
        // related to clears and Zlwll load/stores.
        UINT32 i, mask;
        mask = regHal.LookupMask(MODS_PGRAPH_PRI_GPCS_ZLWLL_REGION_PERMISSION_ALLOCATION_MASK) |
               regHal.LookupMask(MODS_PGRAPH_PRI_GPCS_ZLWLL_REGION_PERMISSION_MASK);

        CHECK_RC(pSubch->MethodWriteRC(LW9097_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(0), 0));   // flag
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(1), 0));   // Pri_data
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(2), mask));// Pri_mask
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_FALCON04,
            regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_ZLWLL_REGION_PERMISSION))); // Pri_addr
        for(i=0; i<10; i++) CHECK_RC(pSubch->MethodWriteRC(LW9097_NO_OPERATION, 0));
    }

    UINT32 subdevNum = pGpuDev->GetNumSubdevices();
    for (UINT32 subdev = 0; subdev < subdevNum; ++subdev)
    {
        GpuSubdevice *pGpuSubdev = pGpuDev->GetSubdevice(subdev);
        UINT32 regVal = 0;
        if (params->ParamPresent("-visible_early_z") > 0)
        {
            CHECK_RC(pGpuSubdev->CtxRegRd32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PROP_DEBUG1), &regVal, GetLwRmPtr()));
            regHal.SetField(&regVal, MODS_PGRAPH_PRI_GPCS_PROP_DEBUG1_VISIBLE_EARLY_Z_ENABLE);
            CHECK_RC(pGpuSubdev->CtxRegWr32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PROP_DEBUG1), regVal, GetLwRmPtr()));

            if (regHal.IsSupported(MODS_PGRAPH_PRI_GPCS_TPCS_WWDX_CONFIG_FORCE_PRIMS_TO_ALL_GPCS))
            {
                CHECK_RC(pGpuSubdev->CtxRegRd32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_WWDX_CONFIG), &regVal, GetLwRmPtr()));
                regHal.SetField(&regVal, MODS_PGRAPH_PRI_GPCS_TPCS_WWDX_CONFIG_FORCE_PRIMS_TO_ALL_GPCS_TRUE);
                CHECK_RC(pGpuSubdev->CtxRegWr32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_WWDX_CONFIG), regVal, GetLwRmPtr()));
            }
        }

        if (params->ParamPresent("-visible_early_z_nobroadcast") > 0)
        {
            CHECK_RC(pGpuSubdev->CtxRegRd32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PROP_DEBUG1), &regVal, GetLwRmPtr()));
            regHal.SetField(&regVal, MODS_PGRAPH_PRI_GPCS_PROP_DEBUG1_VISIBLE_EARLY_Z_ENABLE);
            CHECK_RC(pGpuSubdev->CtxRegWr32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PROP_DEBUG1), regVal, GetLwRmPtr()));
        }

        if (params->ParamPresent("-disable_zpass_pixel_count") > 0)
        {
            if (regHal.IsSupported(MODS_PGRAPH_PRI_GPCS_PROP_DEBUG2_ZLAT_ZPASS_CNT_ENABLE))
            {
                CHECK_RC(pGpuSubdev->CtxRegRd32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PROP_DEBUG2), &regVal, GetLwRmPtr()));
                regHal.SetField(&regVal, MODS_PGRAPH_PRI_GPCS_PROP_DEBUG2_ZLAT_ZPASS_CNT_ENABLE_DISABLE);
                CHECK_RC(pGpuSubdev->CtxRegWr32(hCtx, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_PROP_DEBUG2), regVal, GetLwRmPtr()));
            }
            else
            {
                WarnPrintf("-disable_zpass_pixel_count is deprecated since Turing, ignored.\n");
            }
        }
    }

    if (params->ParamPresent("-blend_zero_times_x_is_zero"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_BLEND_FLOAT_OPTION,
            DRF_DEF(9097, _SET_BLEND_FLOAT_OPTION, _ZERO_TIMES_ANYTHING_IS_ZERO, _TRUE)));
    }
    if (params->ParamPresent("-blend_allow_float_pixel_kills"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_BLEND_OPT_CONTROL,
            DRF_DEF(9097, _SET_BLEND_OPT_CONTROL, _ALLOW_FLOAT_PIXEL_KILLS, _TRUE)));
    }

    if (params->ParamPresent("-early_z_hysteresis"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_OPPORTUNISTIC_EARLY_Z_HYSTERESIS,
            DRF_NUM(9097, _SET_OPPORTUNISTIC_EARLY_Z_HYSTERESIS, _ACLWMULATED_PRIM_AREA_THRESHOLD,
                params->ParamUnsigned("-early_z_hysteresis", 0))));
    }

    if (params->ParamPresent("-zt_count_0") > 0)
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_SELECT,
                    DRF_NUM(9097, _SET_ZT_SELECT, _TARGET_COUNT, 0)));
    }

    if (params->ParamPresent("-ps_smask_aa") > 0 )
    {
        UINT32 value = 0x0;
        if (params->ParamUnsigned("-ps_smask_aa") == 0)
        {
            value = FLD_SET_DRF(9097, _SET_PS_OUTPUT_SAMPLE_MASK_USAGE,
                _QUALIFY_BY_ANTI_ALIAS_ENABLE, _DISABLE, value);
        }
        else
        {
            value = FLD_SET_DRF(9097, _SET_PS_OUTPUT_SAMPLE_MASK_USAGE,
                _QUALIFY_BY_ANTI_ALIAS_ENABLE, _ENABLE, value);
        }

        DebugPrintf(MSGID(), "Writing method LW9097_SET_PS_OUTPUT_SAMPLE_MASK_USAGE with value: 0x%x\n",
            value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_PS_OUTPUT_SAMPLE_MASK_USAGE, value));
    }
    if (params->ParamPresent("-compression_threshold"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COMPRESSION_THRESHOLD,
            params->ParamUnsigned("-compression_threshold")));
    }
    if (params->ParamPresent("-blend_s8u16s16") > 0 )
    {
        UINT32 value = 0x0;
        if (params->ParamUnsigned("-blend_s8u16s16") == 0)
            value = FLD_SET_DRF(9097, _SET_BLEND_PER_FORMAT_ENABLE,
                _SNORM8_UNORM16_SNORM16, _FALSE, value);
        else
            value = FLD_SET_DRF(9097, _SET_BLEND_PER_FORMAT_ENABLE,
                _SNORM8_UNORM16_SNORM16, _TRUE, value);

        DebugPrintf(MSGID(), "Writing method LW9097_SET_BLEND_PER_FORMAT_ENABLE with value: 0x%x\n",
            value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_BLEND_PER_FORMAT_ENABLE, value));
    }
    if (params->ParamPresent("-max_quads_per_subtile") > 0)
    {
        LwU32 value = DRF_NUM(9097, _SET_SUBTILING_PERF_KNOB_A, _FRACTION_OF_SPM_REGISTER_FILE_PER_SUBTILE, 16);
        value = FLD_SET_DRF_NUM(9097, _SET_SUBTILING_PERF_KNOB_A, _FRACTION_OF_SPM_PIXEL_OUTPUT_BUFFER_PER_SUBTILE, 64, value);
        value = FLD_SET_DRF_NUM(9097, _SET_SUBTILING_PERF_KNOB_A, _FRACTION_OF_SPM_TRIANGLE_RAM_PER_SUBTILE, 22, value);
        value = FLD_SET_DRF_NUM(9097, _SET_SUBTILING_PERF_KNOB_A, _FRACTION_OF_MAX_QUADS_PER_SUBTILE,
                                params->ParamUnsigned("-max_quads_per_subtile"), value);
        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_SUBTILING_PERF_KNOB_A, value));
    }

    // unsupported args on Fermi
    const char* bad_args[] =
    {
        "-thread_mem_throttle",
        "-thread_mem_throttle_method_hw",
        "-thread_stack_throttle",
        "-thread_stack_throttle_method_hw",
        "-lwmcovg_control",
        "-inter_tpc_arbitration_control",
        "-line_snap_grid",
        "-mrt_segment_length",
        "-non_line_snap_grid",
        "-shader_scheduling",
        "-swath_height",
        "-tick_control",
        "-tick_control_earlyz",
        "-tick_control_latez",
        "-tpc_mask_wait",
        "-tpc_partition_width",
        "-tpc_partition_height",
        "-tpc_partition_width_zonly",
        "-tpc_partition_height_zonly",
        "-sem_to_zt",
        "-sem_to_ct0",
        "-notify_to_ct0",
        "-stream_to_color",
        "-vs_wait",
        "-phase_id_window_size",
        "-phase_id_lock_phase",
        "-reset_ieee_clean",
        "-fastclear",
        "-set_da_attribute_cache_line",
        "-support_planar_quad_clip",
        "-set_aa_alpha_ctrl_coverage",
        "-fp32_blend",
        "-vcaa_smask",
        "-yilw",
        "-yilw_mod",
        "-timeslice",
        "-blendreduce",
    };

    const char** bad_arg = find_if(bad_args, bad_args+sizeof(bad_args)/sizeof(const char*),
                           [&](const char* arg) -> bool
                           { return params->ParamPresent(arg); });
    if (bad_arg != bad_args + sizeof(bad_args)/sizeof(const char*))
    {
        ErrPrintf("%s is unsupported on Fermi\n", *bad_arg);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    GpuSubdevice *pGpuSubdev = pGpuDev->GetSubdevice(0);

    LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS m_FbRegionInfo;
    memset(&m_FbRegionInfo, 0, sizeof(m_FbRegionInfo));

    rc = pLwRm->ControlBySubdevice(pGpuSubdev,
        LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO, &m_FbRegionInfo,
        sizeof(m_FbRegionInfo));

    if (OK == rc)
    {
        Printf(Tee::PriDebug, "FB region count = %u\n", m_FbRegionInfo.numFBRegions);

        for (UINT32 i = 0; i < m_FbRegionInfo.numFBRegions; ++i)
        {
            Printf(Tee::PriDebug, "FB region %u performance = %u\n", i, m_FbRegionInfo.fbRegion[i].performance);
            Printf(Tee::PriDebug, "FB region %u minAddress = %llx\n", i, m_FbRegionInfo.fbRegion[i].base);
            Printf(Tee::PriDebug, "FB region %u maxAddress = %llx\n", i, m_FbRegionInfo.fbRegion[i].limit);
        }
    }
    else if (RC::LWRM_NOT_SUPPORTED == rc)
    {
        rc.Clear();

        Printf(Tee::PriDebug, "No FB region info\n");
    }

    return rc;
}

RC TraceChannel::SetZbcXChannelReg(LWGpuSubChannel *subchannel)
{
    RC rc;

    // For now the old ZBC registers should be written even for Pascal.
    // Eventually a switch will be made and then the old registers will
    // be removed for Pascal.  (See bug 1539494).
    SetZbcXChannelRegFermi(subchannel);

    if (EngineClasses::IsGpuFamilyClassOrLater(
        TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
    {
        SetZbcXChannelRegPascal(subchannel);
    }

    return rc;
}

RC TraceChannel::SetZbcXChannelRegFermi(LWGpuSubChannel *subchannel)
{
    RC rc = OK;
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

    if (regHal.IsSupported(MODS_PGRAPH_PRI_DS_ROP_DEBUG_X_CHANNEL_ONLY_MATCH_0)) 
    {
        DebugPrintf(MSGID(), "GenericTraceModule::SetZbcXChannelReg - setting LW_PGRAPH_PRI_DS_ROP_DEBUG\n");

        // Use an inline falcon firmware to set the LW_PGRAPH_PRI_DS_ROP_DEBUG
        // register properly on a per-context basis.  See bug 561737.
        UINT32 data = regHal.SetField(MODS_PGRAPH_PRI_DS_ROP_DEBUG_X_CHANNEL_ONLY_MATCH_0_ENABLE);
        UINT32 mask = regHal.LookupMask(MODS_PGRAPH_PRI_DS_ROP_DEBUG_X_CHANNEL_ONLY_MATCH_0);
        CHECK_RC(subchannel->MethodWriteRC(LW9097_WAIT_FOR_IDLE, 0));
        CHECK_RC(subchannel->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(subchannel->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(subchannel->MethodWriteRC(LW9097_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(subchannel->MethodWriteRC(LW9097_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_DS_ROP_DEBUG)));

        // According to hardware, these NOP methods are needed to ensure that
        // the MME data is good when the firmware method is handled.
        for (UINT32 i = 0; i < 10; ++i)
        {
            CHECK_RC(subchannel->MethodWriteRC(LW9097_NO_OPERATION, 0));
        }
    }

    return rc;
}

RC TraceChannel::SendRenderTargetMethods(LWGpuSubChannel *pSubch)
{
    RC rc = OK;
    MdiagSurf* surf;
    // Get the number of subdevices.
    GpuDevice* pGpuDev = m_Test->GetBoundGpuDevice();
    MASSERT(pGpuDev);
    for (UINT32 s = 0; s < pGpuDev->GetNumSubdevices(); ++s)
    {
        Channel *ch = pSubch->channel()->GetModsChannel();

        for (UINT32 i = 0; i < TESLA_RENDER_TARGETS; i++)
        {
            if ((i == 0) && params->ParamPresent("-prog_zt_as_ct0"))
                surf = m_Test->surfZ;
            else
                surf = m_Test->surfC[i];

            if (pGpuDev->GetNumSubdevices() > 1 && surf->GetLocation() != Memory::Fb)
                CHECK_RC(ch->WriteSetSubdevice(1 << s));

            if (s == 0 || surf->GetLocation() != Memory::Fb)
            {
                if (m_Test->GetSurfaceMgr()->GetValid(surf))
                {
                    UINT64 Offset = surf->GetCtxDmaOffsetGpu() + surf->GetExtraAllocSize();
                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_A(i), LwU64_HI32(Offset)));
                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_B(i), LwU64_LO32(Offset)));
                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_FORMAT(i),
                        m_Test->GetSurfaceMgr()->DeviceFormatFromFormat(
                            m_Test->surfC[i]->GetColorFormat())));
                    UINT32 layout = surf->GetLayout() == Surface2D::BlockLinear ?
                                    LW9097_SET_COLOR_TARGET_MEMORY_LAYOUT_BLOCKLINEAR :
                                    LW9097_SET_COLOR_TARGET_MEMORY_LAYOUT_PITCH;

                    UINT32 thirdDimensionControl = LW9097_SET_COLOR_TARGET_MEMORY_THIRD_DIMENSION_CONTROL_THIRD_DIMENSION_DEFINES_ARRAY_SIZE;

                    if ((surf->GetDepth() > 1) && (surf->GetArraySize() > 1))
                    {
                        ErrPrintf("Fermi doesn't support 3D render target arrays\n");
                        return RC::BAD_COMMAND_LINE_ARGUMENT;
                    }
                    else if (m_Test->surfC[0]->GetDepth() > 1) {
                        thirdDimensionControl = LW9097_SET_COLOR_TARGET_MEMORY_THIRD_DIMENSION_CONTROL_THIRD_DIMENSION_DEFINES_DEPTH_SIZE;
                        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(i),
                                DRF_NUM(9097, _SET_COLOR_TARGET_THIRD_DIMENSION, _V, surf->GetDepth())));

                    }
                    else {
                        thirdDimensionControl = LW9097_SET_COLOR_TARGET_MEMORY_THIRD_DIMENSION_CONTROL_THIRD_DIMENSION_DEFINES_ARRAY_SIZE;
                        CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(i),
                                DRF_NUM(9097, _SET_COLOR_TARGET_THIRD_DIMENSION, _V, surf->GetArraySize())));

                    }

                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_MEMORY(i),
                        DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_WIDTH, surf->GetLogBlockWidth()) |
                        DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_HEIGHT, surf->GetLogBlockHeight()) |
                        DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_DEPTH, surf->GetLogBlockDepth()) |
                        DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, layout) |
                        DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _THIRD_DIMENSION_CONTROL, thirdDimensionControl)));
                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_ARRAY_PITCH(i),
                                                   (UINT32)(surf->GetArrayPitch() >> 2)));
                    if (surf->GetLayout() == Surface2D::BlockLinear)
                    {
                        CHECK_RC(pSubch->MethodWriteRC(
                            LW9097_SET_COLOR_TARGET_WIDTH(i),
                            DRF_NUM(9097, _SET_COLOR_TARGET_WIDTH, _V,
                            surf->GetAllocWidth())));
                    }
                    else
                    {
                        MASSERT(surf->GetLayout() == Surface2D::Pitch);

                        CHECK_RC(pSubch->MethodWriteRC(
                            LW9097_SET_COLOR_TARGET_WIDTH(i),
                            DRF_NUM(9097, _SET_COLOR_TARGET_WIDTH, _V,
                            surf->GetPitch())));
                    }

                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_HEIGHT(i),
                        DRF_NUM(9097, _SET_COLOR_TARGET_HEIGHT, _V,
                        surf->GetAllocHeight())));
                }
                else
                {
                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_COLOR_TARGET_FORMAT(i),
                        LW9097_SET_COLOR_TARGET_FORMAT_V_DISABLED));
                }
            }
            if (pGpuDev->GetNumSubdevices() > 1 && surf->GetLocation() != Memory::Fb)
                CHECK_RC(ch->WriteSetSubdevice(Channel::AllSubdevicesMask));
        }

        if (pGpuDev->GetNumSubdevices() > 1 && m_Test->surfZ->GetLocation() != Memory::Fb)
            CHECK_RC(ch->WriteSetSubdevice(1 << s));
        if (s == 0 || m_Test->surfZ->GetLocation() != Memory::Fb)
        {
            surf = m_Test->surfZ;
            if (m_Test->GetSurfaceMgr()->GetValid(surf))
            {
                UINT64 Offset = surf->GetCtxDmaOffsetGpu() + surf->GetExtraAllocSize();
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_A, LwU64_HI32(Offset)));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_B, LwU64_LO32(Offset)));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_FORMAT,
                    m_Test->GetSurfaceMgr()->DeviceFormatFromFormat(surf->GetColorFormat())));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_SELECT, DRF_NUM(9097, _SET_ZT_SELECT, _TARGET_COUNT, 1)));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_BLOCK_SIZE,
                            DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _WIDTH, surf->GetLogBlockWidth()) |
                            DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _HEIGHT, surf->GetLogBlockHeight()) |
                            DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _DEPTH, surf->GetLogBlockDepth())));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_ARRAY_PITCH,
                                               (UINT32)(surf->GetArrayPitch() >> 2)));

                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_SIZE_A,
                            DRF_NUM(9097, _SET_ZT_SIZE_A, _WIDTH, surf->GetAllocWidth())));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_SIZE_B,
                            DRF_NUM(9097, _SET_ZT_SIZE_B, _HEIGHT, surf->GetAllocHeight())));
                if (surf->GetDepth() > 1)
                {
                    ErrPrintf("Fermi doesn't support 3D Z buffers\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
                else
                {
                    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ZT_SIZE_C,
                        DRF_NUM(9097, _SET_ZT_SIZE_C, _THIRD_DIMENSION, surf->GetArraySize()) |
                        DRF_DEF(9097, _SET_ZT_SIZE_C, _CONTROL, _THIRD_DIMENSION_DEFINES_ARRAY_SIZE)));
                }
            }
            else
            {
               InfoPrintf("Setting Zlwll region to 0x3f since no valid Z surf\n");
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ACTIVE_ZLWLL_REGION, 0x3f));
                CHECK_RC(pSubch->MethodWriteRC(LW9097_CLEAR_SURFACE,
                            DRF_DEF(9097, _CLEAR_SURFACE, _Z_ENABLE, _FALSE) |
                            DRF_DEF(9097, _CLEAR_SURFACE, _STENCIL_ENABLE, _FALSE)));

            }
        }
        if (pGpuDev->GetNumSubdevices() > 1 && m_Test->surfZ->GetLocation() != Memory::Fb)
            CHECK_RC(ch->WriteSetSubdevice(Channel::AllSubdevicesMask));
    } // Subdev

    // Configure the AA mode

    // This function will only be called if all of the AA modes of the
    // color/z surfaces are the same.  Use the Z buffer to get the mode.
    surf = m_Test->surfZ;
    UINT32 AAMode;
    switch (GetAAModeFromSurface(surf))
    {
        case AAMODE_1X1:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_1X1);
            break;
        case AAMODE_2X1:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X1);
            break;
        case AAMODE_2X2:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2);
            break;
        case AAMODE_4X2:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2);
            break;
        case AAMODE_4X4:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X4);
            break;
        case AAMODE_2X1_D3D:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X1_D3D);
            break;
        case AAMODE_4X2_D3D:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_D3D);
            break;
        case AAMODE_2X2_VC_4:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2_VC_4);
            break;
        case AAMODE_2X2_VC_12:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2_VC_12);
            break;
        case AAMODE_4X2_VC_8:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_VC_8);
            break;
        case AAMODE_4X2_VC_24:
            AAMode = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_VC_24);
            break;
        default:
            ErrPrintf("Illegal AA mode\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_ANTI_ALIAS, AAMode));

    return rc;
}

UINT32 GenericTraceModule::MassageFermiMethod(UINT32 subdev, UINT32 Method, UINT32 Data)
{
    Method <<= 2;
    MASSERT(params);

    const float round = SUBPIXEL_PRECISION;

    switch( Method )
    {
        case LW9097_SET_CT_SELECT:
        {
            // For older traces which have no ALLOC_SURFACE commands present in the
            // trace header, MODS needs to scan the pushbuffer to determine which
            // render targets should be enabled.
            if (!m_Test->GetAllocSurfaceCommandPresent())
            {
                UINT32 TargetCount = DRF_VAL(9097, _SET_CT_SELECT, _TARGET_COUNT, Data);
                UINT32 Targets[8];
                Targets[0] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET0, Data);
                Targets[1] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET1, Data);
                Targets[2] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET2, Data);
                Targets[3] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET3, Data);
                Targets[4] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET4, Data);
                Targets[5] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET5, Data);
                Targets[6] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET6, Data);
                Targets[7] = DRF_VAL(9097, _SET_CT_SELECT, _TARGET7, Data);

                // Enable each selected color target
                for (UINT32 i = 0; i < TargetCount; i++)
                {
                    if(Targets[i] < TESLA_RENDER_TARGETS)
                    {
                        m_Test->GetSurfaceMgr()->SetSurfaceUsed(ColorSurfaceTypes[Targets[i]], true,
                            subdev);
                    }
                }
            }
            break;
        }
        case LW9097_SET_ZT_SELECT:
        {
            // For older traces which have no ALLOC_SURFACE commands present in the
            // trace header, MODS needs to scan the pushbuffer to determine whether
            // or not the Z buffer should be enabled.
            if (!m_Test->GetAllocSurfaceCommandPresent())
            {
                if (params->ParamPresent("-zt_count_0"))
                    Data = FLD_SET_DRF_NUM(9097, _SET_ZT_SELECT, _TARGET_COUNT, 0, Data);

                UINT32 TargetCount = DRF_VAL(9097, _SET_ZT_SELECT, _TARGET_COUNT, Data);
                if (TargetCount > 0)
                {
                    if (m_Test->GetSurfaceMgr()->GetGlobalSurfaceLayout() == Surface2D::Pitch)
                    {
                        WarnPrintf("SetZtSelect method tried to enable a pitch Z buffer, ignoring\n");
                    }
                    else
                    {
                        m_Test->GetSurfaceMgr()->SetSurfaceNull(SURFACE_TYPE_Z, false, subdev);
                        m_Test->GetSurfaceMgr()->SetSurfaceUsed(SURFACE_TYPE_Z, true, subdev);
                    }
                }
            }
            break;
        }
        case LW9097_SET_ZLWLL_REGION_LOCATION:
        {
            // Scanning SetZlwllRegionLocation to do sanity check for zlwll
            // configuration, bug 743764
            UINT32 aliquot_count = DRF_VAL(9097, _SET_ZLWLL_REGION_LOCATION,
                _ALIQUOT_COUNT, Data);
            bool ctxsw_enabled = m_Test->IsTestCtxSwitching();
            ZLwllEnum zlwll_mode = static_cast<ZLwllEnum>(
                params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED));
            if (aliquot_count > 0 &&
                ctxsw_enabled &&
                (zlwll_mode != ZLWLL_CTXSW_SEPERATE_BUFFER &&
                 zlwll_mode != ZLWLL_CTXSW_NOCTXSW))
            {
                ErrPrintf("Zlwll is activated in context switch mode, you need "
                    "to specify either -zlwll_ctxsw_seperate_buffer or "
                    "-zlwll_ctxsw_noctxsw for trace_3d instances, see known "
                    "issue at bug 743764\n");
                MASSERT(0);
            }
            break;
        }
        default:
            break;
    }

    if(m_NoPbMassage)
    {
        return Data;
    }

    switch (Method)
    {
        default:
            // leave it alone by default
            break;

        case LW9097_NOTIFY:
            // tests that use notify typically write to Color Target 0
            m_Test->GetSurfaceMgr()->SetSurfaceUsed(ColorSurfaceTypes[0], true,
                                                    subdev);
            if (params->ParamPresent("-notifyAwaken") > 0)
            {
                Data = FLD_SET_DRF(9097, _NOTIFY, _TYPE, _WRITE_THEN_AWAKEN, Data);
                InfoPrintf("Patching NOTIFY for AWAKEN\n");
            }
            break;
        case LWC797_REPORT_SEMAPHORE_EXELWTE:
            if (params->ParamPresent("-semaphoreAwaken") > 0)
            {
                Data = FLD_SET_DRF(C797, _REPORT_SEMAPHORE_EXELWTE,
                                   _AWAKEN_ENABLE, _TRUE, Data);
                InfoPrintf("Patching SEMAPHORE for AWAKEN\n");
            }
            break;
        case LW9097_SET_REPORT_SEMAPHORE_D:
            if (params->ParamPresent("-semaphoreAwaken") > 0)
            {
                Data = FLD_SET_DRF(9097, _SET_REPORT_SEMAPHORE_D,
                                   _AWAKEN_ENABLE, _TRUE, Data);
                InfoPrintf("Patching SEMAPHORE for AWAKEN\n");
            }
            break;
        case LW9097_SET_SCISSOR_HORIZONTAL(0): // XXX replicated method
        case LW9097_SET_VIEWPORT_CLIP_HORIZONTAL(0): // XXX replicated method
        case LW9097_SET_SURFACE_CLIP_HORIZONTAL:
        case LW9097_SET_CLEAR_RECT_HORIZONTAL:
            if (Method == LW9097_SET_SURFACE_CLIP_HORIZONTAL)
            {
                UINT32 x = DRF_VAL(9097, _SET_SURFACE_CLIP_HORIZONTAL, _X, Data);
                x += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetX();
                if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
                {
                    UINT32 fullWidth = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
                    UINT32 offsetX = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
                    x += fullWidth - m_Test->GetSurfaceMgr()->GetWidth() - offsetX;
                }
                Data = FLD_SET_DRF_NUM(9097, _SET_SURFACE_CLIP_HORIZONTAL, _X, x, Data);
            }
            if (m_Test->GetSurfaceMgr()->GetWidth() != m_Test->GetSurfaceMgr()->GetTargetDisplayWidth())
            {
                UINT32 min = Data & 0xFFFF;
                UINT32 max = (Data >> 16) & 0xFFFF;
                // Do this in integer, to avoid precision problems
                min = (min * m_Test->GetSurfaceMgr()->GetWidth()) / (long)m_Test->GetSurfaceMgr()->GetTargetDisplayWidth();
                max = (max * m_Test->GetSurfaceMgr()->GetWidth()) / (long)m_Test->GetSurfaceMgr()->GetTargetDisplayWidth();
                Data = min | (max << 16);
            }
            if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
            {
                UINT32 fullWidth = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
                UINT32 offsetX = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
                UINT32 min = fullWidth - offsetX - m_Test->GetSurfaceMgr()->GetWidth();
                UINT32 max = fullWidth - offsetX ;
                MASSERT(max <= 0xFFFF);
                MASSERT(min <= 0xFFFF);
                Data = min | (max << 16);
            }
            // XXX Handle window offset
            if (m_Test->scissorEnable && Method == LW9097_SET_SCISSOR_HORIZONTAL(0))
            {
                UINT32 scmin = DRF_VAL(9097, _SET_SCISSOR_HORIZONTAL, _XMIN, Data);
                UINT32 scmax = DRF_VAL(9097, _SET_SCISSOR_HORIZONTAL, _XMAX, Data);

                // compute the intersection of scissor override and previous scissor
                // Note that XMAX is exclusive.
                scmin = max(scmin, m_Test->scissorXmin);
                scmax = min(scmax, m_Test->scissorXmin+m_Test->scissorWidth);
                if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
                {
                    UINT32 fullWidth = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
                    UINT32 offsetX = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
                    UINT32 tmp;
                    tmp = fullWidth-m_Test->GetSurfaceMgr()->GetWidth() - offsetX;
                    scmin = max(scmin, tmp);
                    tmp = fullWidth - offsetX;
                    scmax = min(scmin, tmp);
                }

                // If the min value is greater than the max value, then the
                // the intersection is empty.  Set the max value to be equal
                // to the min value so that fmodel won't assert.
                if (scmin > scmax) scmax = scmin;

                Data = FLD_SET_DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMIN, scmin, Data);
                Data = FLD_SET_DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMAX, scmax, Data);
            }
            if (params->ParamPresent("-viewport_clip") > 0 && Method == LW9097_SET_VIEWPORT_CLIP_HORIZONTAL(0))
            {
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_CLIP_HORIZONTAL, _X0,
                        params->ParamNUnsigned("-viewport_clip", 0), Data);
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_CLIP_HORIZONTAL, _WIDTH,
                        params->ParamNUnsigned("-viewport_clip", 1), Data);
            }
            break;
        case LW9097_SET_SCISSOR_VERTICAL(0): // XXX replicated method
        case LW9097_SET_VIEWPORT_CLIP_VERTICAL(0): // XXX replicated method
        case LW9097_SET_SURFACE_CLIP_VERTICAL:
        case LW9097_SET_CLEAR_RECT_VERTICAL:
            if (Method == LW9097_SET_SURFACE_CLIP_VERTICAL)
            {
                UINT32 y = DRF_VAL(9097, _SET_SURFACE_CLIP_VERTICAL, _Y, Data);
                y += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetY();
                if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
                {
                    UINT32 fullHeight = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
                    UINT32 offsetY = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
                    y += fullHeight - m_Test->GetSurfaceMgr()->GetHeight() - offsetY;
                }
                Data = FLD_SET_DRF_NUM(9097, _SET_SURFACE_CLIP_VERTICAL, _Y, y, Data);
            }
            if (m_Test->GetSurfaceMgr()->GetHeight() != m_Test->GetSurfaceMgr()->GetTargetDisplayHeight())
            {
                unsigned int min = Data & 0xFFFF;
                unsigned int max = (Data >> 16) & 0xFFFF;
                // Do this in integer, to avoid precision problems
                min = (min * m_Test->GetSurfaceMgr()->GetHeight()) / (long)m_Test->GetSurfaceMgr()->GetTargetDisplayHeight();
                max = (max * m_Test->GetSurfaceMgr()->GetHeight()) / (long)m_Test->GetSurfaceMgr()->GetTargetDisplayHeight();
                Data = min | (max << 16);
            }
            if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
            {
                UINT32 fullHeight = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
                UINT32 offsetY = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
                UINT32 max = fullHeight - offsetY;
                UINT32 min = fullHeight - m_Test->GetSurfaceMgr()->GetHeight() - offsetY;
                MASSERT(max <= 0xFFFF);
                MASSERT(min <= 0xFFFF);
                Data = min | (max << 16);
            }
            // XXX Handle window offset
            if (m_Test->scissorEnable && Method == LW9097_SET_SCISSOR_VERTICAL(0))
            {
                UINT32 scmin = DRF_VAL(9097, _SET_SCISSOR_VERTICAL, _YMIN, Data);
                UINT32 scmax = DRF_VAL(9097, _SET_SCISSOR_VERTICAL, _YMAX, Data);

                if (params->ParamPresent("-scissor_correct")==0)
                {
                    // due to y-ilwersion
                    if (m_Test->scissorYmin + m_Test->scissorHeight > m_Test->GetSurfaceMgr()->GetHeight())
                    {
                        // Scissor size bigger than the surface size, just set to 0
                        m_Test->scissorYmin = 0;
                    }
                    else
                    {
                        m_Test->scissorYmin = m_Test->GetSurfaceMgr()->GetHeight() -
                            m_Test->scissorYmin - m_Test->scissorHeight;
                    }
                }

                // compute the intersection of scissor override and previous scissor
                // Note that XMAX is exclusive.
                scmin = max(scmin, m_Test->scissorYmin);
                scmax = min(scmax, m_Test->scissorYmin+m_Test->scissorHeight);

                if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
                {
                        UINT32 fullHeight = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
                        UINT32 offsetY = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
                        UINT32 tmp = fullHeight - m_Test->GetSurfaceMgr()->GetHeight() - offsetY;
                        scmin = max(scmin, tmp);
                        tmp = fullHeight - offsetY;
                        scmax = min(scmin, tmp);
                }
                // If the min value is greater than the max value, then the
                // the intersection is empty.  Set the max value to be equal
                // to the min value so that fmodel won't assert.
                if (scmin > scmax) scmax = scmin;

                Data = FLD_SET_DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMIN, scmin, Data);
                Data = FLD_SET_DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMAX, scmax, Data);
            }
            if (params->ParamPresent("-viewport_clip") > 0 && LW9097_SET_VIEWPORT_CLIP_VERTICAL(0))
            {
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_CLIP_VERTICAL, _Y0,
                        params->ParamNUnsigned("-viewport_clip", 2), Data);
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_CLIP_VERTICAL, _HEIGHT,
                        params->ParamNUnsigned("-viewport_clip", 3), Data);
            }
            break;
        case LW9097_SET_SCISSOR_ENABLE(0): // XXX replicated method
            if (m_Test->scissorEnable)
                Data = FLD_SET_DRF(9097, _SET_SCISSOR_ENABLE, _V, _TRUE, Data);
            break;

        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(0):
        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(1):
        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(2):
        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(3):
        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(4):
        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(5):
        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(6):
        case LW9097_SET_WINDOW_CLIP_HORIZONTAL(7):
            // scale this similarly to the clear rect
            if (m_Test->GetSurfaceMgr()->GetWidth() != m_Test->GetSurfaceMgr()->GetTargetDisplayWidth())
            {
                unsigned int min = Data & 0xFFF;
                unsigned int max = (Data >> 16) & 0xFFF;
                unsigned int size = max - min + 1;
                // Do this in integer, to avoid precision problems
                min = (min * m_Test->GetSurfaceMgr()->GetWidth()) / m_Test->GetSurfaceMgr()->GetTargetDisplayWidth();
                size = (size * m_Test->GetSurfaceMgr()->GetWidth()) / m_Test->GetSurfaceMgr()->GetTargetDisplayWidth();
                if (size < 1) size = 1;
                max = min + size - 1;
                if (max > 0xFFF) max = 0xFFF;
                Data = min | (max << 16);
            }
            if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
            {
                UINT32 fullWidth = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
                UINT32 offsetX = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
                UINT32 min = fullWidth - offsetX - m_Test->GetSurfaceMgr()->GetWidth();
                UINT32 max = fullWidth - offsetX ;
                MASSERT(max <= 0xFFFF);
                MASSERT(min <= 0xFFFF);
                Data = min | (max << 16);
            }
            break;
        case LW9097_SET_WINDOW_CLIP_VERTICAL(0):
        case LW9097_SET_WINDOW_CLIP_VERTICAL(1):
        case LW9097_SET_WINDOW_CLIP_VERTICAL(2):
        case LW9097_SET_WINDOW_CLIP_VERTICAL(3):
        case LW9097_SET_WINDOW_CLIP_VERTICAL(4):
        case LW9097_SET_WINDOW_CLIP_VERTICAL(5):
        case LW9097_SET_WINDOW_CLIP_VERTICAL(6):
        case LW9097_SET_WINDOW_CLIP_VERTICAL(7):
            // rounding and scaling similar to the clear rect
            if (m_Test->GetSurfaceMgr()->GetHeight() != m_Test->GetSurfaceMgr()->GetTargetDisplayHeight())
            {
                unsigned int min = Data & 0xFFF;
                unsigned int max = (Data >> 16) & 0xFFF;
                unsigned int size = max - min + 1;
                // Do this in integer, to avoid precision problems
                min = (min * m_Test->GetSurfaceMgr()->GetHeight()) / (long)m_Test->GetSurfaceMgr()->GetTargetDisplayHeight();
                size = (size * m_Test->GetSurfaceMgr()->GetHeight()) / (long)m_Test->GetSurfaceMgr()->GetTargetDisplayHeight();
                if (size < 1) size = 1;
                max = min + size - 1;
                if (max > 0xFFF) max = 0xFFF;
                Data = min | (max << 16);
            }
            if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
            {
                UINT32 fullHeight = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
                UINT32 offsetY = params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
                unsigned int min = fullHeight - m_Test->GetSurfaceMgr()->GetHeight() - offsetY;
                unsigned int max = fullHeight - offsetY;
                MASSERT(max <= 0xFFFF);
                MASSERT(min <= 0xFFFF);
                Data = min | (max << 16);
            }
            break;

        case LW9097_SET_CLIP_ID_CLEAR_RECT_HORIZONTAL:
        {
            UINT32 MinX = DRF_VAL(9097, _SET_CLIP_ID_CLEAR_RECT_HORIZONTAL, _XMIN, Data);
            UINT32 MaxX = DRF_VAL(9097, _SET_CLIP_ID_CLEAR_RECT_HORIZONTAL, _XMAX, Data);
            MinX += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetX();
            MaxX += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetX();
            Data = DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_HORIZONTAL, _XMIN, MinX) |
                   DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_HORIZONTAL, _XMAX, MaxX);
            break;
        }
        case LW9097_SET_CLIP_ID_CLEAR_RECT_VERTICAL:
        {
            UINT32 MinY = DRF_VAL(9097, _SET_CLIP_ID_CLEAR_RECT_VERTICAL, _YMIN, Data);
            UINT32 MaxY = DRF_VAL(9097, _SET_CLIP_ID_CLEAR_RECT_VERTICAL, _YMAX, Data);
            MinY += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetY();
            MaxY += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetY();
            Data = DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_VERTICAL, _YMIN, MinY) |
                   DRF_NUM(9097, _SET_CLIP_ID_CLEAR_RECT_VERTICAL, _YMAX, MaxY);
            break;
        }
        case LW9097_SET_SURFACE_CLIP_ID_WIDTH:
        {
            UINT32 Width = DRF_VAL(9097, _SET_SURFACE_CLIP_ID_WIDTH, _V,Data);
            Width = (Width + m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetX() + 15) & ~15;
            Data = DRF_NUM(9097, _SET_SURFACE_CLIP_ID_WIDTH, _V,Width);
            break;
        }
        case LW9097_SET_SURFACE_CLIP_ID_HEIGHT:
        {
            UINT32 Height = DRF_VAL(9097, _SET_SURFACE_CLIP_ID_HEIGHT, _V, Data);
            Height = (Height + m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetY() + 3) & ~3;
            Data = DRF_NUM(9097, _SET_SURFACE_CLIP_ID_HEIGHT, _V, Height);
            break;
        }
        case LW9097_SET_CLIP_ID_EXTENT_X(0):
        case LW9097_SET_CLIP_ID_EXTENT_X(1):
        case LW9097_SET_CLIP_ID_EXTENT_X(2):
        case LW9097_SET_CLIP_ID_EXTENT_X(3):
        {
            UINT32 MinX = DRF_VAL(9097, _SET_CLIP_ID_EXTENT_X, _MINX, Data);
            UINT32 Width = DRF_VAL(9097, _SET_CLIP_ID_EXTENT_X, _WIDTH, Data);
            UINT32 MaxX;
            MinX += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetX();
            MaxX = (MinX + Width + 0xF) & ~0xF;
            MinX &= ~0xF;
            Width = MaxX - MinX;
            Data = FLD_SET_DRF_NUM(9097, _SET_CLIP_ID_EXTENT_X, _MINX, MinX, Data);
            Data = FLD_SET_DRF_NUM(9097, _SET_CLIP_ID_EXTENT_X, _WIDTH, Width, Data);
            break;
        }
        case LW9097_SET_CLIP_ID_EXTENT_Y(0):
        case LW9097_SET_CLIP_ID_EXTENT_Y(1):
        case LW9097_SET_CLIP_ID_EXTENT_Y(2):
        case LW9097_SET_CLIP_ID_EXTENT_Y(3):
        {
            UINT32 MinY = DRF_VAL(9097, _SET_CLIP_ID_EXTENT_Y, _MINY, Data);
            UINT32 Height = DRF_VAL(9097, _SET_CLIP_ID_EXTENT_Y, _HEIGHT, Data);
            UINT32 MaxY;
            MinY += m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetY();
            MaxY = (MinY + Height + 0x3) & ~0x3;
            MinY &= ~0x3;
            Height = MaxY - MinY;
            Data = FLD_SET_DRF_NUM(9097, _SET_CLIP_ID_EXTENT_Y, _MINY, MinY, Data);
            Data = FLD_SET_DRF_NUM(9097, _SET_CLIP_ID_EXTENT_Y, _HEIGHT, Height, Data);
            break;
        }
        case LW9097_SET_WINDOW_OFFSET_X:
        {
            if ((m_Test->GetSurfaceMgr()->GetWidth() != m_Test->GetSurfaceMgr()->GetTargetDisplayWidth()) ||
                (m_Test->GetSurfaceMgr()->IsUserSpecifiedWindowOffset()))
            {
                // use short to get negative numbers
                INT16 offx = Data & 0xffff;
                // Do this in integer, to avoid precision problems
                // use long here to catch overflow
                INT32 offx_l = ((INT32)offx * (INT32)m_Test->GetSurfaceMgr()->GetWidth()) / (INT32)m_Test->GetSurfaceMgr()->GetTargetDisplayWidth();
                // Handle commandline window offset
                if (m_Test->GetSurfaceMgr()->IsUserSpecifiedWindowOffset()) {
                    INT32 woffx = m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetX();
                    offx_l = (((INT32)offx+woffx)*(INT32)m_Test->GetSurfaceMgr()->GetWidth())/(INT32)m_Test->GetSurfaceMgr()->GetTargetDisplayWidth();
                }
                if (offx_l > 0xffff)
                    offx_l = 0xffff;
                if (offx_l < -0xfffe)
                    offx_l = -0xfffe;
                Data = DRF_NUM(9097, _SET_WINDOW_OFFSET_X, _V, offx_l);
            }
            break;
        }
        case LW9097_SET_WINDOW_OFFSET_Y:
        {
            if ((m_Test->GetSurfaceMgr()->GetHeight() != m_Test->GetSurfaceMgr()->GetTargetDisplayHeight()) ||
                (m_Test->GetSurfaceMgr()->IsUserSpecifiedWindowOffset()))
            {
                // use short to get negative numbers
                INT16 offy = Data & 0xffff;
                // Do this in integer, to avoid precision problems
                // use long here to catch overflow
                INT32 offy_l = ((INT32)offy * (INT32)m_Test->GetSurfaceMgr()->GetHeight()) / (INT32)m_Test->GetSurfaceMgr()->GetTargetDisplayHeight();
                // Handle commandline window offset
                if (m_Test->GetSurfaceMgr()->IsUserSpecifiedWindowOffset()) {
                    INT32 woffy = m_Test->GetSurfaceMgr()->GetUnscaledWindowOffsetY();
                    offy_l = (((INT32)offy+woffy)*(INT32)m_Test->GetSurfaceMgr()->GetHeight())/(INT32)m_Test->GetSurfaceMgr()->GetTargetDisplayHeight();
                }
                if (offy_l > 0x1ffff)
                    offy_l = 0x1ffff;
                if (offy_l < -0x1fffe)
                    offy_l = -0x1fffe;
                Data = DRF_NUM(9097, _SET_WINDOW_OFFSET_Y, _V, offy_l);
            }
            if (params->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0 && m_WindowOrigin == LOWER_LEFT)
            {
                    Data  = 2*params->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1)  - m_Test->GetSurfaceMgr()->GetHeight() - (m_Test->GetSurfaceMgr()->GetHeight() - Data);
            }
            break;
        }
        case LW9097_SET_COLOR_TARGET_MEMORY(0):
        {
            Data = m_Test->GetSurfaceMgr()->GetGlobalSurfaceLayout() == Surface2D::BlockLinear ?
                FLD_SET_DRF(9097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, _BLOCKLINEAR, Data) :
                FLD_SET_DRF(9097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, _PITCH, Data);
        }
        break;
        case LW9097_SET_COLOR_TARGET_WIDTH(0):
            Data = FLD_SET_DRF_NUM(9097, _SET_COLOR_TARGET_WIDTH, _V, m_Test->surfZ->GetWidth(), Data);
        break;
        case LW9097_SET_COLOR_TARGET_HEIGHT(0):
            Data = FLD_SET_DRF_NUM(9097, _SET_COLOR_TARGET_HEIGHT, _V, m_Test->surfZ->GetHeight(), Data);
        break;
        case LW9097_CLEAR_SURFACE:
        {
            if (params->ParamPresent("-nullz") > 0 ||
                    m_Test->surfZ->GetLayout() != Surface2D::BlockLinear)
            {
                Data = FLD_SET_DRF(9097, _CLEAR_SURFACE, _Z_ENABLE, _FALSE, Data);
                Data = FLD_SET_DRF(9097, _CLEAR_SURFACE, _STENCIL_ENABLE, _FALSE, Data);
            }
        }
        break;
        case LW9097_SET_VIEWPORT_OFFSET_X(0): // XXX replicated method
        {
            if (params->ParamPresent("-viewport_offset") > 0 )
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_OFFSET_X, _V,
                        params->ParamNUnsigned("-viewport_offset", 0), Data);
            else
            {
                float fdata = (*(float *)&Data - round);
                Data = m_Test->GetSurfaceMgr()->ScaleWidth(fdata, 0.0f, round);
            }
        }
        break;
        case LW9097_SET_VIEWPORT_OFFSET_Y(0): // XXX replicated method
        {
            if (params->ParamPresent("-viewport_offset") > 0 )
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_OFFSET_Y, _V,
                        params->ParamNUnsigned("-viewport_offset", 1), Data);
            else
            {
                float fdata = (*(float *)&Data - round);
                Data = m_Test->GetSurfaceMgr()->ScaleHeight(fdata, 0.0f, round);
            }
        }
        break;
        case LW9097_SET_VIEWPORT_OFFSET_Z(0): // XXX replicated method
        {
            if (params->ParamPresent("-viewport_offset") > 0 )
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_OFFSET_Z, _V,
                        params->ParamNUnsigned("-viewport_offset", 2), Data);
        }
        break;
        case LW9097_SET_VIEWPORT_SCALE_X(0): // XXX replicated method
            if (params->ParamPresent("-viewport_scale") > 0 )
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_SCALE_X, _V,
                        params->ParamNUnsigned("-viewport_scale", 0), Data);
            else
                Data = m_Test->GetSurfaceMgr()->ScaleWidth(*(float*)&Data, 0.0f, 0.0);
        break;
        case LW9097_SET_VIEWPORT_SCALE_Y(0): // XXX replicated method
            if (params->ParamPresent("-viewport_scale") > 0 )
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_SCALE_Y, _V,
                        params->ParamNUnsigned("-viewport_scale", 1), Data);
            else
                Data = m_Test->GetSurfaceMgr()->ScaleHeight(*(float*)&Data, 0.0f, 0.0);
        break;
        case LW9097_SET_VIEWPORT_SCALE_Z(0): // XXX replicated method
            if (params->ParamPresent("-viewport_scale") > 0 )
                Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_SCALE_Z, _V,
                        params->ParamNUnsigned("-viewport_scale", 2), Data);
        break;

        // bug 357946, options for SET_VIEWPORT_CLIP_CONTROL
        case LW9097_SET_VIEWPORT_CLIP_CONTROL:
        {
            if ((params->ParamPresent("-geom_clip") > 0) ||
                (params->ParamPresent("-geometry_clip") > 0) ||
                (params->ParamPresent("-pixel_min_z") > 0) ||
                (params->ParamPresent("-pixel_max_z") > 0))
            {
                // set pixel_min_z field, default value CLAMP = 1
                if (params->ParamPresent("-pixel_min_z") > 0)
                {
                    if (params->ParamUnsigned("-pixel_min_z") == 0)
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _PIXEL_MIN_Z, _CLIP, Data);
                    else
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _PIXEL_MIN_Z, _CLAMP, Data);
                }

                // set pixel_max_z field, default value CLAMP = 1
                if (params->ParamPresent("-pixel_max_z") > 0)
                {
                    if (params->ParamUnsigned("-pixel_max_z") == 0)
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _PIXEL_MAX_Z, _CLIP, Data);
                    else
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _PIXEL_MAX_Z, _CLAMP, Data);
                }

                // set geometry_clip field, default value FRUSTUM_Z_CLIP = 5
                if (params->ParamPresent("-geom_clip") > 0)
                {
                    switch(params->ParamUnsigned("-geom_clip"))
                    {
                    case 0:
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _GEOMETRY_CLIP, _WZERO_CLIP, Data);
                        break;
                    case 1:
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _GEOMETRY_CLIP, _PASSTHRU, Data);
                        break;
                    case 2:
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _GEOMETRY_CLIP, _FRUSTUM_XY_CLIP,
                                           Data);
                        break;
                    case 3:
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _GEOMETRY_CLIP, _FRUSTUM_XYZ_CLIP,
                                           Data);
                        break;
                    case 4:
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _GEOMETRY_CLIP,
                                           _WZERO_CLIP_NO_Z_LWLL, Data);
                        break;
                    case 5:
                        Data = FLD_SET_DRF(9097, _SET_VIEWPORT_CLIP_CONTROL,
                                           _GEOMETRY_CLIP, _FRUSTUM_Z_CLIP,
                                           Data);
                        break;
                    }
                }

                // another way to set geometry clip field, for backwards compatibility
                if (params->ParamPresent("-geometry_clip"))
                {
                    Data = FLD_SET_DRF_NUM(9097, _SET_VIEWPORT_CLIP_CONTROL, _GEOMETRY_CLIP,
                        params->ParamUnsigned("-geometry_clip"), Data);
                }
            }
            break;
        }

        case LW9097_SET_ANTI_ALIAS_ENABLE:
        if (m_Test->GetSurfaceMgr()->GetMultiSampleOverride())
            Data = FLD_SET_DRF_NUM(9097, _SET_ANTI_ALIAS_ENABLE, _V,
                    m_Test->GetSurfaceMgr()->GetMultiSampleOverrideValue(), Data);
        break;

    // bug# 357796, reduction threshold options
    case LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM8:
        if (params->ParamPresent("-reduce_thresh_u8") > 0)
        {
            UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_u8", 0);
            UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_u8", 1);

            Data = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM8, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
            Data |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM8, _ALL_COVERED, consVal);
        }
        break;

    case LW9097_SET_REDUCE_COLOR_THRESHOLDS_FP16:
        if (params->ParamPresent("-reduce_thresh_fp16") > 0)
        {
            UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_fp16", 0);
            UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_fp16", 1);
            Data = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP16, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
            Data |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP16, _ALL_COVERED, consVal);
        }
        break;

    case LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM10:
        if (params->ParamPresent("-reduce_thresh_u10") > 0)
        {
            UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_u10", 0);
            UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_u10", 1);
            Data = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM10, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
            Data |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM10, _ALL_COVERED, consVal);
        }
        break;

    case LW9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM16:
        if (params->ParamPresent("-reduce_thresh_u16") > 0)
        {
            UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_u16", 0);
            UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_u16", 1);
            Data = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM16, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
            Data |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_UNORM16, _ALL_COVERED, consVal);
        }
        break;

    case LW9097_SET_REDUCE_COLOR_THRESHOLDS_FP11:
        if (params->ParamPresent("-reduce_thresh_fp11") > 0)
        {
            UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_fp11", 0);
            UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_fp11", 1);
            Data = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP11, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
            Data |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_FP11, _ALL_COVERED, consVal);
        }
        break;

    case LW9097_SET_REDUCE_COLOR_THRESHOLDS_SRGB8:
        if (params->ParamPresent("-reduce_thresh_srgb8") > 0)
        {
            UINT32 aggrVal = params->ParamNUnsigned("-reduce_thresh_srgb8", 0);
            UINT32 consVal = params->ParamNUnsigned("-reduce_thresh_srgb8", 1);
            Data = DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_SRGB8, _ALL_COVERED_ALL_HIT_ONCE, aggrVal);
            Data |= DRF_NUM(9097, _SET_REDUCE_COLOR_THRESHOLDS_SRGB8, _ALL_COVERED, consVal);
        }
        break;

    case LW9097_SET_ALPHA_TO_COVERAGE_OVERRIDE:
        // bug# 383085, SetAlphaToCoverageOverride support for fermi
        if (params->ParamPresent("-alpha_coverage_aa") > 0 ||
            params->ParamPresent("-alpha_coverage_psmask") > 0)
        {
            if (params->ParamPresent("-alpha_coverage_aa") > 0)
            {
                if (params->ParamUnsigned("-alpha_coverage_aa") == 0)
                {
                    Data = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE, _QUALIFY_BY_ANTI_ALIAS_ENABLE,
                        _DISABLE, Data);
                }
                else
                {
                    Data = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE, _QUALIFY_BY_ANTI_ALIAS_ENABLE,
                        _ENABLE, Data);
                }
            }

            if (params->ParamPresent("-alpha_coverage_psmask") > 0)
            {
                if (params->ParamUnsigned("-alpha_coverage_psmask") == 0)
                {
                    Data = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE, _QUALIFY_BY_PS_SAMPLE_MASK_OUTPUT,
                        _DISABLE, Data);
                }
                else
                {
                    Data = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_OVERRIDE, _QUALIFY_BY_PS_SAMPLE_MASK_OUTPUT,
                        _ENABLE, Data);
                }
            }
        }
        break;
    case LW9097_SET_ALPHA_TO_COVERAGE_DITHER_CONTROL:
        if (params->ParamPresent("-alpha_coverage_dither") > 0)
        {
            switch (params->ParamUnsigned("-alpha_coverage_dither"))
            {
                case 0:
                    Data = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_DITHER_CONTROL, _DITHER_FOOTPRINT,
                            _PIXELS_1X1, Data);
                    break;
                case 1:
                    Data = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_DITHER_CONTROL, _DITHER_FOOTPRINT,
                            _PIXELS_2X2, Data);
                    break;
                case 2:
                    Data = FLD_SET_DRF(9097, _SET_ALPHA_TO_COVERAGE_DITHER_CONTROL, _DITHER_FOOTPRINT,
                            _PIXELS_1X1_VIRTUAL_SAMPLES, Data);
                    break;
                default:
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
        break;
    case LW9097_SET_HYBRID_ANTI_ALIAS_CONTROL:
        if (params->ParamPresent("-hybrid_passes") > 0 || params->ParamPresent("-hybrid_centroid") > 0)
        {
            UINT32 passes = params->ParamUnsigned("-hybrid_passes", 1);
            Data = DRF_VAL(9097, _SET_HYBRID_ANTI_ALIAS_CONTROL, _PASSES, passes & 0xF);

            // check if -hybrid_centroid specified, default to 0=FRAGMENT
            if (params->ParamPresent("-hybrid_centroid") > 0 && params->ParamUnsigned("-hybrid_centroid") == 1)
            {
                Data = FLD_SET_DRF(9097, _SET_HYBRID_ANTI_ALIAS_CONTROL, _CENTROID, _PER_PASS, Data);
            }

            if (GetTraceChannel()->GetGrSubChannel()->GetClass() >= MAXWELL_B)
            {
                if (passes >= 16)
                {
                    Data = FLD_SET_DRF_NUM(B197, _SET_HYBRID_ANTI_ALIAS_CONTROL, _PASSES_EXTENDED, 1, Data);
                }
            }
        }
        break;
    case LW9097_SET_SAMPLE_MASK_X0_Y0:
    case LW9097_SET_SAMPLE_MASK_X1_Y0:
    case LW9097_SET_SAMPLE_MASK_X0_Y1:
    case LW9097_SET_SAMPLE_MASK_X1_Y1:
        if( params->ParamPresent("-samplemask_d3d_swizzle") >0 )
        {
            const static UINT32 storage_map_4_2[] = {5, 3, 4, 1, 0, 7, 6, 2};
            UINT32 OrigData = Data;
            MdiagSurf *surface = m_Test->surfZ;

            switch (GetAAModeFromSurface(surface))
            {
            case AAMODE_2X1_D3D:
                UtilBitCopy(OrigData, &Data, 0, 1);
                UtilBitCopy(OrigData, &Data, 1, 0);
                break;
            case AAMODE_4X2_D3D:
                for (int i=0; i<8; ++i)
                {
                    UtilBitCopy(OrigData, &Data, i, storage_map_4_2[i]);
                }
                break;
            default:
                WarnPrintf("Option '-samplemask_d3d_swizzle' used without D3D AA, ignored\n");
                break;
            }
        }

        if( params->ParamPresent("-samplemask_4x2_vc8_swizzle") >0 )
        {
            const static UINT32 storage_map_4_2[] = {7,1,6,2,4,3,0,5};
            UINT32 OrigData = Data;
            MdiagSurf *surface = m_Test->surfZ;

            switch (GetAAModeFromSurface(surface))
            {
            case AAMODE_4X2_VC_8:
                for (int i=0; i<8; ++i)
                {
                    UtilBitCopy(OrigData, &Data, i, storage_map_4_2[i]);
                }
                break;
            default:
                WarnPrintf("Option '-samplemask_4x2_vc8_swizzle' used without 4x2_VC_8 AA, ignored\n");
                break;
            }
        }
        break;
    case LW9097_SET_SRGB_WRITE:
        if (params->ParamPresent("-srgb_write"))
        {
            Data = DRF_DEF(9097, _SET_SRGB_WRITE, _ENABLE, _TRUE);
        }
        break;
    case LW9097_SET_OPPORTUNISTIC_EARLY_Z_HYSTERESIS:
        if (params->ParamPresent("-early_z_hysteresis"))
            Data = params->ParamUnsigned("-early_z_hysteresis");
        break;
    case LW9097_SET_PS_OUTPUT_SAMPLE_MASK_USAGE:
        // bug# 417993
        if (params->ParamPresent("-ps_smask_aa") > 0 )
        {
            if (params->ParamUnsigned("-ps_smask_aa") == 0)
                Data = FLD_SET_DRF(9097, _SET_PS_OUTPUT_SAMPLE_MASK_USAGE,
                   _QUALIFY_BY_ANTI_ALIAS_ENABLE, _DISABLE, Data);
            else
                Data = FLD_SET_DRF(9097, _SET_PS_OUTPUT_SAMPLE_MASK_USAGE,
                   _QUALIFY_BY_ANTI_ALIAS_ENABLE, _ENABLE, Data);
        }
        break;
    case LW9097_SET_BLEND_PER_FORMAT_ENABLE:
        if (params->ParamPresent("-blend_s8u16s16") > 0 )
        {
            if (params->ParamUnsigned("-blend_s8u16s16") == 0)
                Data = FLD_SET_DRF(9097, _SET_BLEND_PER_FORMAT_ENABLE,
                    _SNORM8_UNORM16_SNORM16, _FALSE, Data);
            else
                Data = FLD_SET_DRF(9097, _SET_BLEND_PER_FORMAT_ENABLE,
                    _SNORM8_UNORM16_SNORM16, _TRUE, Data);
        }
        break;
    case LW9097_SET_SUBTILING_PERF_KNOB_A:
        if (params->ParamPresent("-max_quads_per_subtile") > 0)
        {
            Data = FLD_SET_DRF_NUM(9097, _SET_SUBTILING_PERF_KNOB_A, _FRACTION_OF_MAX_QUADS_PER_SUBTILE,
                                   params->ParamUnsigned("-max_quads_per_subtile"), Data);
        }
        break;
    case LW9097_SET_ZLWLL:
        if ((ZLwllEnum)params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED) == ZLWLL_NONE)
        {
            Data = DRF_DEF(9097, _SET_ZLWLL, _Z_ENABLE, _FALSE) |
                   DRF_DEF(9097, _SET_ZLWLL, _STENCIL_ENABLE, _FALSE);
        }
        break;
    case  LW9097_SET_WINDOW_ORIGIN:
        {
            UINT32 mode = DRF_VAL(9097, _SET_WINDOW_ORIGIN, _MODE, Data);
            if(mode)
                m_WindowOrigin = LOWER_LEFT;
            else
                m_WindowOrigin = UPPER_LEFT;
            break;
        }
    } // end of switch

    return Data;
}

RC GenericTraceModule::SendFermiMethodSegment(SegmentH h)
{
    MASSERT(m_FileType == GpuTrace::FT_PUSHBUFFER);
    MASSERT(h < m_SendSegments.size());

    RC rc;

    LWGpuChannel * pCh = m_pTraceChannel->GetCh();
    TraceSubChannel *ptracesubch = m_pTraceChannel->GetSubChannel("");
    UINT32 class_num = ptracesubch->GetClass();
    LWGpuSubChannel * pSubch = ptracesubch->GetSubCh();
    LWGpuResource * lwgpu = m_Test->GetGpuResource();
    UINT32 header;
    UINT32 opcode2, method_offset, num_dwords;
    UINT32 StartOffset = m_SendSegments[h].Start;
    UINT32 EndOffset   = StartOffset + m_SendSegments[h].Size;
    UINT32 method_flushed = 0;
    bool dumpBufRequested = m_Trace->DumpBufRequested() || m_Trace->DumpSurfaceRequested();
    RC traceEventRC = OK;
    bool skipThisHeader = false;
    T3dPlugin::EventAction trEventAction = T3dPlugin::NoAction;
    UINT32 trEventData = 0;

    pCh->BeginTestMethods();

    for (UINT32 ptr = StartOffset; ptr < EndOffset; /* */ )
    {
        if ( m_Test->PushbufferHeader_traceEvent( ptr, m_FileToLoad,
                                                  &trEventAction,
                                                  &trEventData ) )
        {
            if ( trEventAction == T3dPlugin::Jump )
            {
                ptr = trEventData;
                continue;
            }
            else if ( trEventAction == T3dPlugin::Skip )
                skipThisHeader = true;
            else if ( trEventAction == T3dPlugin::Exit )
                return OK;
        }

        // Decode the header:
        header = Get032(ptr, 0);
        num_dwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header);

        opcode2 = REF_VAL(LW_FIFO_DMA_SEC_OP, header);
        UINT32 opcode3 = REF_VAL(LW_FIFO_DMA_TERT_OP, header);
        method_offset = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS, header) << 2;

        if (header != LW_FIFO_DMA_NOP)
        {
            if ((opcode2 == LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT) &&
                (opcode3 == LW_FIFO_DMA_TERT_OP_GRP0_INC_METHOD))
            {
                method_offset = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS_OLD, header) << 2;
            }
            else if (opcode2 == LW_FIFO_DMA_SEC_OP_GRP2_USE_TERT)
            {
                method_offset = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS_OLD, header) << 2;
            }
        }

        if ((m_Test->GetTrace()->GetSyncPmTriggerPairNum() > 0) &&
            IsPmTriggerMethod(method_offset))
        {
            ErrPrintf("test %s: trace should not have any PmTrigger methods "
                      "if PMTRIGGER_SYNC_EVENT is used\n",
                      m_Test->GetTestName());
            return RC::SOFTWARE_ERROR;
        }

        // save the original pushbuffer offset so we can send this
        // as a parameter in the AfterMethodWrite traceEvent
        //
        UINT32 origOffset = ptr;
        UINT32 * pData = 0;

        if (m_pTraceChannel->HasMultipleSubChs())
        {
            // Get the subchannel by default.
            bool getSubchannel = true;

            // Host methods do not need the subchannel.
            if ((opcode2 == LW_FIFO_DMA_SEC_OP_END_PB_SEGMENT) ||
                (header == LW_FIFO_DMA_NOP))
            {
                getSubchannel = false;
            }
            // Subdevice mask methods do not need the subchannel.
            else if (opcode2 == LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT)
            {
                if ((opcode3 == LW_FIFO_DMA_TERT_OP_GRP0_SET_SUB_DEV_MASK) ||
                    (opcode3 == LW_FIFO_DMA_TERT_OP_GRP0_STORE_SUB_DEV_MASK) ||
                    (opcode3 == LW_FIFO_DMA_TERT_OP_GRP0_USE_SUB_DEV_MASK))
                {
                    getSubchannel = false;
                }
            }

            if (getSubchannel)
            {
                UINT32 opcode_subch = REF_VAL(LW_FIFO_DMA_METHOD_SUBCHANNEL, header);
                ptracesubch = m_pTraceChannel->GetSubChannel(opcode_subch);
                MASSERT( ptracesubch );
                pSubch = ptracesubch->GetSubCh();
            }
        }

        // skip header
        ptr += 4;

        if ( skipThisHeader )
        {
            ptr += 4 * num_dwords;
            skipThisHeader = false;
            continue;
        }

        UINT32 channelNumber = pSubch->channel()->ChannelNum();
        UINT32 subchannelClass = ptracesubch->GetClass();
        string channelName = m_pTraceChannel->GetName();

        // writing header without data
        if (ptr >= EndOffset)
        {
            UINT32 data = 0;
            if (header == LW_FIFO_DMA_NOP)
            {
                traceEventRC = m_Test->BeforeMethodWriteNop_traceEvent(
                    origOffset, channelNumber, subchannelClass,
                    channelName.c_str() );
                if ( traceEventRC != OK )
                    return RC::SOFTWARE_ERROR;

                rc = pSubch->channel()->GetModsChannel()->WriteNop();
                num_dwords = 0;
            }
            else switch (opcode2)
            {
                case LW_FIFO_DMA_SEC_OP_INC_METHOD:
                    traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                        origOffset, method_offset, num_dwords, &data,
                        "INCREMENTING", channelNumber, subchannelClass,
                        channelName.c_str() );
                    if ( traceEventRC != OK )
                        return RC::SOFTWARE_ERROR;

                    rc = pSubch->WriteHeader(method_offset, num_dwords);
                    break;
                case LW_FIFO_DMA_SEC_OP_NON_INC_METHOD:
                    traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                        origOffset, method_offset, num_dwords, &data,
                        "NON_INCREMENTING", channelNumber,
                        subchannelClass, channelName.c_str() );
                    if ( traceEventRC != OK )
                        return RC::SOFTWARE_ERROR;

                    rc = pSubch->WriteNonIncHeader(method_offset, num_dwords);
                    break;
                case LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD:
                    data = REF_VAL(LW_FIFO_DMA_IMMD_DATA, header);
                    traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                        origOffset, method_offset, 1, &data,
                        "INCREMENTING_IMM", channelNumber,
                        subchannelClass, channelName.c_str() );
                    if ( traceEventRC != OK )
                        return RC::SOFTWARE_ERROR;

                    rc = pSubch->MethodWriteN_RC(method_offset, 1, &data, INCREMENTING_IMM);
                    break;
                case LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT:
                    switch (opcode3)
                    {
                    case LW_FIFO_DMA_TERT_OP_GRP0_SET_SUB_DEV_MASK:
                        rc = pSubch->WriteSetSubdevice(
                            REF_VAL(LW_FIFO_DMA_SET_SUBDEVICE_MASK_VALUE, header));
                        break;
                    case LW_FIFO_DMA_TERT_OP_GRP0_STORE_SUB_DEV_MASK:
                    case LW_FIFO_DMA_TERT_OP_GRP0_USE_SUB_DEV_MASK:
                        MASSERT(!"MODS does not support STORE/USE subdevice mask yet!\n");
                        break;
                    case LW_FIFO_DMA_TERT_OP_GRP0_INC_METHOD:
                    default:
                        ErrPrintf("Wrong header 0x%08x @0x%08x\n", header, ptr-4);
                        return RC::SOFTWARE_ERROR;
                    }
                    break;
                case LW_FIFO_DMA_SEC_OP_END_PB_SEGMENT:
                    // Do nothing for pb end
                    break;
                case LW_FIFO_DMA_SEC_OP_GRP2_USE_TERT:
                case LW_FIFO_DMA_SEC_OP_ONE_INC:
                    ErrPrintf("Wrong header 0x%08x @0x%08x\n", header, ptr-4);
                    return RC::SOFTWARE_ERROR;
                default:
                    ErrPrintf("Don't know how to write header 0x%08x @0x%08x\n", header, ptr-4);
                    return RC::SOFTWARE_ERROR;
            }
            if (rc != OK)
            {
                ErrPrintf("trace_3d: WriteHeader failed: %s\n", rc.Message());
                return rc;
            }

            if ((m_Test->pmMode == Trace3DTest::PM_MODE_PM_SB_FILE) &&
                IsPmTriggerMethod(method_offset))
            {
                if (m_Test->numPmTriggers & 0x1)
                {
                    pCh->MethodFlush();
                    pCh->WaitForIdle();
                    lwgpu->PerfMonEnd(pCh, pSubch->subchannel_number(),
                        class_num);
                    XD->XMLEndLwrrent();
                }
                m_Test->numPmTriggers++;
            }

            if (dumpBufRequested &&
                (ptracesubch->GrSubChannel() ||
                 ptracesubch->GetClass() == FERMI_COMPUTE_A ||
                 ptracesubch->GetClass() == FERMI_COMPUTE_B))
                // For gr and compute subchannels
            {
                BufferDumper *dumper = m_Trace->GetDumper();
                dumper->Execute(ptr - 4, method_offset, m_CachedSurface, m_ModuleName, EndOffset, class_num);
            }

            //////////////////////////// Begin Trace Event ////////////////////
            //
            // Trace3dPlugin event: "AfterMethodWrite".
            //
            // TRACE_3D FLOW: We're in a loop writing data from a method segment
            // to its associated channel, and have just sent some data from the
            // the segment to the channel
            //
            // MEANING: This event keeps notifies the plugin about the progress
            // of sending data from a method segment to a channel.  The plugin
            // could inject more methods or wait for idle, etc.
            //
            // NOTE: if the implementation of sending method segment data to a
            // channel changes, then this event will likely need to change as
            // well.
            //
            traceEventRC = m_Test->traceEvent(Trace3DTest::TraceEventType::AfterMethodWrite,
                                               "offset", origOffset );
            if ( traceEventRC != OK )
                return RC::SOFTWARE_ERROR;
            //
            ///////////////////////////  End Trace Event /////////////////////////

            return OK;
        }

        // CPLU: does not seem to be right to put BeforeEachMethod here.
        // The body of the loop walks through a group of methods.
        m_Test->BeforeEachMethodGroup(num_dwords);

        if ((m_Test->pmMode == Trace3DTest::PM_MODE_PM_SB_FILE) &&
            ((method_offset == LW9097_PM_TRIGGER ) ||
                (method_offset == LWA097_PM_TRIGGER_WFI )) &&
            !(m_Test->numPmTriggers&0x1))
        {
            pCh->MethodFlush();
            pCh->WaitForIdle();

            MASSERT(gXML);
            XD->XMLStartElement("<PmTrigger");
            XD->outputAttribute("index",  m_Test->numPmTriggers >> 1);
            XD->endAttributes();

            lwgpu->PerfMonStart(pCh, pSubch->subchannel_number(), class_num);
        }

        if (header == LW_FIFO_DMA_NOP)
        {
            //////////////////////////// Begin Trace Event ////////////////////
            //
            traceEventRC = m_Test->BeforeMethodWriteNop_traceEvent(
                origOffset, channelNumber, subchannelClass,
                channelName.c_str() );
            if ( traceEventRC != OK )
                return RC::SOFTWARE_ERROR;
            //
            ///////////////////////////  End Trace Event /////////////////////////

            rc = pSubch->channel()->GetModsChannel()->WriteNop();
            num_dwords = 0;
        }
        else switch (opcode2)
        {
         case LW_FIFO_DMA_SEC_OP_INC_METHOD:
            pData = (UINT32*)Get032Addr(ptr, 0);

            //////////////////////////// Begin Trace Event ////////////////////
            //
            traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                origOffset, method_offset, num_dwords, pData,
                "INCREMENTING", channelNumber, subchannelClass,
                channelName.c_str() );

            if ( traceEventRC != OK )
                return RC::SOFTWARE_ERROR;
            //
            ///////////////////////////  End Trace Event /////////////////////////

            rc = pSubch->MethodWriteN_RC(method_offset, num_dwords,
                                         pData, INCREMENTING);
            ptr += 4*num_dwords;
            break;
         case LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD:
         {
             UINT32 data = REF_VAL(LW_FIFO_DMA_IMMD_DATA, header);

             //////////////////////////// Begin Trace Event ////////////////////
             //
             traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                 origOffset, method_offset, 1, &data, "INCREMENTING_IMM",
                 channelNumber, subchannelClass, channelName.c_str() );

             if ( traceEventRC != OK )
                 return RC::SOFTWARE_ERROR;
             //
             ///////////////////////////  End Trace Event /////////////////////////

             rc = pSubch->MethodWriteN_RC(method_offset, 1, &data, INCREMENTING_IMM);
             num_dwords = 0; // data in header
         }
         break;
         case LW_FIFO_DMA_SEC_OP_ONE_INC:

            pData = (UINT32*)Get032Addr(ptr, 0);

            //////////////////////////// Begin Trace Event ////////////////////
            //
            traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                origOffset, method_offset, num_dwords, pData,
                "INCREMENTING_ONCE", channelNumber, subchannelClass,
                channelName.c_str() );

            if ( traceEventRC != OK )
                return RC::SOFTWARE_ERROR;
            //
            ///////////////////////////  End Trace Event /////////////////////////

            rc = pSubch->MethodWriteN_RC(method_offset, num_dwords,
                                         pData, INCREMENTING_ONCE);
            ptr += 4*num_dwords;
            break;
         case LW_FIFO_DMA_SEC_OP_NON_INC_METHOD:

            pData = (UINT32*)Get032Addr(ptr, 0);
            //////////////////////////// Begin Trace Event ////////////////////
            //
            traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                origOffset, method_offset, num_dwords, pData,
                "NON_INCREMENTING", channelNumber, subchannelClass,
                channelName.c_str() );

            if ( traceEventRC != OK )
                return RC::SOFTWARE_ERROR;
            //
            ///////////////////////////  End Trace Event /////////////////////////

            rc = pSubch->MethodWriteN_RC(method_offset, num_dwords,
                                         pData, NON_INCREMENTING);
            ptr += 4*num_dwords;
            break;
         case LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT:
         {
             UINT32 opcode3 = REF_VAL(LW_FIFO_DMA_TERT_OP, header);
             switch (opcode3)
             {
              case LW_FIFO_DMA_TERT_OP_GRP0_INC_METHOD:

                 pData = (UINT32*)Get032Addr(ptr, 0);
                 num_dwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);

                 //////////////////////////// Begin Trace Event ////////////////////
                 //
                 traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                     origOffset, method_offset, num_dwords, pData,
                     "INCREMENTING", channelNumber, subchannelClass,
                     channelName.c_str() );

                 if ( traceEventRC != OK )
                     return RC::SOFTWARE_ERROR;
                 //
                 ///////////////////////////  End Trace Event /////////////////////////

                 rc = pSubch->MethodWriteN_RC(method_offset, num_dwords,
                                              pData, INCREMENTING);
                 ptr += 4*num_dwords;
                 break;
              case LW_FIFO_DMA_TERT_OP_GRP0_SET_SUB_DEV_MASK:
                 rc = pSubch->WriteSetSubdevice(REF_VAL(LW_FIFO_DMA_SET_SUBDEVICE_MASK_VALUE, header));
                 num_dwords = 0;
                 break;
              default:
                 // LW_FIFO_DMA_TERT_OP_GRP0_STORE_SUB_DEV_MASK:
                 // LW_FIFO_DMA_TERT_OP_GRP0_USE_SUB_DEV_MASK:
                 MASSERT(!"MODS does not support STORE/USE subdevice mask yet!");
             }
             break;
         }
         case LW_FIFO_DMA_SEC_OP_GRP2_USE_TERT:

            pData = (UINT32*)Get032Addr(ptr, 0);
            num_dwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);

            //////////////////////////// Begin Trace Event ////////////////////
            //
            traceEventRC = m_Test->BeforeMethodWriteN_traceEvent(
                origOffset, method_offset, num_dwords, pData,
                "NON_INCREMENTING", channelNumber, subchannelClass,
                channelName.c_str() );

            if ( traceEventRC != OK )
                return RC::SOFTWARE_ERROR;
            //
            ///////////////////////////  End Trace Event /////////////////////////

            rc = pSubch->MethodWriteN_RC(method_offset, num_dwords,
                                         pData, NON_INCREMENTING);
            ptr += 4*num_dwords;
            break;
         default:
            ErrPrintf("Unsupported PB entry: 0x%08x @0x%08x\n", header, ptr-4);
            return RC::SOFTWARE_ERROR;
        }

        if (rc != OK)
        {
            ErrPrintf("method writeN (0x%08x <- 0x%08x) @0x%08x failed!\n",
                      method_offset, Get032(ptr - 4*num_dwords, 0), ptr - 4*num_dwords);
            return rc;
        }

        method_flushed++;

        if ((m_Test->pmMode == Trace3DTest::PM_MODE_PM_SB_FILE) &&
            IsPmTriggerMethod(method_offset))
        {
            if ( m_Test->numPmTriggers&0x1 ) {
                pCh->MethodFlush();
                pCh->WaitForIdle();
                lwgpu->PerfMonEnd(pCh, pSubch->subchannel_number(), class_num);
                XD->XMLEndLwrrent();
            }
            m_Test->numPmTriggers++;
        }

        if (dumpBufRequested &&
            (ptracesubch->GrSubChannel() ||
             ptracesubch->GetClass() == FERMI_COMPUTE_A ||
             ptracesubch->GetClass() == FERMI_COMPUTE_B))
                // For gr and compute subchannels
        {
            BufferDumper *dumper = m_Trace->GetDumper();
            dumper->Execute(ptr - 4*(num_dwords+1), method_offset,
                            m_CachedSurface, m_ModuleName, EndOffset, class_num);
        }

        //////////////////////////// Begin Trace Event ////////////////////
        //
        // Trace3dPlugin event: "AfterMethodWrite".
        //
        // TRACE_3D FLOW: We're in a loop writing data from a method segment
        // to its associated channel, and have just sent some data from the
        // the segment to the channel
        //
        // MEANING: This event keeps notifies the plugin about the progress
        // of sending data from a method segment to a channel.  The plugin
        // could inject more methods or wait for idle, etc.
        //
        // NOTE: if the implementation of sending method segment data to a
        // channel changes, then this event will likely need to change as
        // well.
        //
        traceEventRC = m_Test->traceEvent(Trace3DTest::TraceEventType::AfterMethodWrite,
                                           "offset", origOffset );
        if ( traceEventRC != OK )
            return RC::SOFTWARE_ERROR;
        //
        ///////////////////////////  End Trace Event /////////////////////////

    }

    pCh->EndTestMethods();

    return rc;
}

bool BufferDumperFermi::IsBeginOp(UINT32 class_id, UINT32 op) const
{
    if ((EngineClasses::IsClassType("Gr", class_id) && op == LW9097_BEGIN) ||
        (class_id == FERMI_COMPUTE_A && op == LW90C0_BEGIN_GRID) ||
        (class_id == FERMI_COMPUTE_B && op == LW91C0_BEGIN_GRID))
    {
        return true;
    }
    return false;
}

bool BufferDumperFermi::IsEndOp(UINT32 class_id, UINT32 op) const
{
    if ((EngineClasses::IsClassType("Gr", class_id) && op == LW9097_END) ||
        (class_id == FERMI_COMPUTE_A && op == LW90C0_END_GRID) ||
        (class_id == FERMI_COMPUTE_B && op == LW91C0_END_GRID))
    {
        return true;
    }
    return false;
}

bool BufferDumperFermi::IsSetColorTargetOp(UINT32 classId, UINT32 op,
    int* colorNum) const
{
    if (!EngineClasses::IsClassType("Gr", classId))
    {
        return false;
    }

    for (UINT32 i=0; i<MAX_RENDER_TARGETS; i++)
    {
        *colorNum = i;
        if (LW9097_SET_COLOR_TARGET_A(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_B(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_WIDTH(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_HEIGHT(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_FORMAT(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_MEMORY(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_ARRAY_PITCH(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_LAYER(i) == op)
            return true;
        if (LW9097_SET_COLOR_TARGET_MARK(i) == op)
            return true;
    }

    *colorNum = -1;
    return false;
}

bool BufferDumperFermi::IsSetZetaTargetOp(UINT32 classId, UINT32 op) const
{
    if (!EngineClasses::IsClassType("Gr", classId))
    {
        return false;
    }

    if( LW9097_SET_ZT_MARK==op )
        return true;
    if( LW9097_SET_ZT_A==op )
        return true;
    if( LW9097_SET_ZT_B==op )
        return true;
    if( LW9097_SET_ZT_FORMAT==op )
        return true;
    if( LW9097_SET_ZT_BLOCK_SIZE==op )
        return true;
    if( LW9097_SET_ZT_ARRAY_PITCH==op )
        return true;
    if( LW9097_SET_ZT_SIZE_A==op )
        return true;
    if( LW9097_SET_ZT_SIZE_B==op )
        return true;
    if( LW9097_SET_ZT_SIZE_C==op )
        return true;
    if( LW9097_SET_ZT_SELECT==op )
        return true;
    if( LW9097_SET_ZT_LAYER==op )
        return true;
    //if( LW9097_SET_ZT_READ_ONLY==op )
    //    return true;

    return false;
}

bool BufferDumperFermi::IsSetCtCtrlOp(UINT32 classId, UINT32 methodOffset) const
{
    if (!EngineClasses::IsClassType("Gr", classId))
    {
        return false;
    }

    switch (methodOffset)
    {
        case LW9097_SET_CT_SELECT:
        case LW9097_SET_CT_WRITE(0):
        case LW9097_SET_CT_WRITE(1):
        case LW9097_SET_CT_WRITE(2):
        case LW9097_SET_CT_WRITE(3):
        case LW9097_SET_CT_WRITE(4):
        case LW9097_SET_CT_WRITE(5):
        case LW9097_SET_CT_WRITE(6):
        case LW9097_SET_CT_WRITE(7):
            return true;
        default:
            return false;
    }
}

void BufferDumperFermi::SetCtCtrl(UINT32 method_offset, CachedSurface *cs, UINT32 ptr, UINT32 pbSize)
{
    UINT32 header = Get032(cs, ptr, 0);

    UINT32 opcode2 = REF_VAL(LW_FIFO_DMA_SEC_OP, header);
    UINT32 data;

    if( ((ptr + 4) >= pbSize) && (LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD != opcode2) )
    {
        ErrPrintf( "BufferDumperFermi::SetCtCtrl failed: the header %x is the end of Pushbuffer "
                    "and opcode2 is %d.\n", header, opcode2);
        return;
    }

    ptr += 4;

    if (LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD == opcode2)
    {
        data = REF_VAL(LW_FIFO_DMA_IMMD_DATA, header);
    }
    else
    {
        data = Get032(cs,  ptr, 0);
        ptr += 4;
    }

    UINT32 ct_num = 0;
    bool write_mask = false;
    switch (method_offset)
    {
    case LW9097_SET_CT_SELECT:
        m_CtCount = DRF_VAL(9097, _SET_CT_SELECT, _TARGET_COUNT, data);
        m_Ct[0]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET0, data));
        m_Ct[1]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET1, data));
        m_Ct[2]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET2, data));
        m_Ct[3]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET3, data));
        m_Ct[4]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET4, data));
        m_Ct[5]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET5, data));
        m_Ct[6]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET6, data));
        m_Ct[7]->SetSelect(DRF_VAL(9097, _SET_CT_SELECT, _TARGET7, data));
        break;
    case LW9097_SET_CT_WRITE(0):
    case LW9097_SET_CT_WRITE(1):
    case LW9097_SET_CT_WRITE(2):
    case LW9097_SET_CT_WRITE(3):
    case LW9097_SET_CT_WRITE(4):
    case LW9097_SET_CT_WRITE(5):
    case LW9097_SET_CT_WRITE(6):
    case LW9097_SET_CT_WRITE(7):
        ct_num = (method_offset - LW9097_SET_CT_WRITE(0)) /
                (LW9097_SET_CT_WRITE(1) - LW9097_SET_CT_WRITE(0));
        write_mask = FLD_TEST_DRF(9097, _SET_CT_WRITE, _R_ENABLE, _TRUE, data) ||
                     FLD_TEST_DRF(9097, _SET_CT_WRITE, _G_ENABLE, _TRUE, data) ||
                     FLD_TEST_DRF(9097, _SET_CT_WRITE, _B_ENABLE, _TRUE, data) ||
                     FLD_TEST_DRF(9097, _SET_CT_WRITE, _A_ENABLE, _TRUE, data);
        m_Ct[ct_num]->SetWriteEnabled(write_mask);
        break;
    default:
        MASSERT(!"Unknown method offset");
        break;
    }
    return;
}

void BufferDumperFermi::SetActiveCSurface
(
    CachedSurface *cs,  // Pushbuffer to be parsed
    UINT32 ptr,         // Start addr of pb to be parsed
    int color_num,      // Ct#
    UINT32 pbSize       // Pushbuffer Size
)
{
    MASSERT((color_num>=0) && (color_num<MAX_RENDER_TARGETS));

    RenderTarget *target = m_Ct[color_num];
    UINT32 header = Get032(cs,  ptr, 0);

    UINT32 method_offset = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS, header) << 2;
    UINT32 num_dwords = max(REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header), (UINT32)1);
    UINT32 opcode2 = REF_VAL(LW_FIFO_DMA_SEC_OP, header);
    UINT32 data = 0;
    UINT32 color_format = 0;

    if( ((ptr + 4) >= pbSize) && (LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD != opcode2) )
    {
        ErrPrintf( "BufferDumperFermi::SetActiveCSurface failed: the header %x is the end of Pushbuffer "
                    "and opcode2 is %d.\n", header, opcode2);
        return;
    }

    ptr += 4;

    for (UINT32 i=0; i<num_dwords; ++i, method_offset+=4)
    {
        if (LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD == opcode2)
        {
            data = REF_VAL(LW_FIFO_DMA_IMMD_DATA, header);
        }
        else
        {
            data = Get032(cs, ptr, 0);
            ptr += 4;
        }
        switch (method_offset)
        {
        case LW9097_SET_COLOR_TARGET_A(0):
        case LW9097_SET_COLOR_TARGET_A(1):
        case LW9097_SET_COLOR_TARGET_A(2):
        case LW9097_SET_COLOR_TARGET_A(3):
        case LW9097_SET_COLOR_TARGET_A(4):
        case LW9097_SET_COLOR_TARGET_A(5):
        case LW9097_SET_COLOR_TARGET_A(6):
        case LW9097_SET_COLOR_TARGET_A(7):
            target->SetOffsetUpper(DRF_VAL(9097, _SET_COLOR_TARGET_A,
                _OFFSET_UPPER, data));
            break;
        case LW9097_SET_COLOR_TARGET_B(0):
        case LW9097_SET_COLOR_TARGET_B(1):
        case LW9097_SET_COLOR_TARGET_B(2):
        case LW9097_SET_COLOR_TARGET_B(3):
        case LW9097_SET_COLOR_TARGET_B(4):
        case LW9097_SET_COLOR_TARGET_B(5):
        case LW9097_SET_COLOR_TARGET_B(6):
        case LW9097_SET_COLOR_TARGET_B(7):
            target->SetOffsetLower(DRF_VAL(9097, _SET_COLOR_TARGET_B,
                _OFFSET_LOWER, data));
            break;
        case LW9097_SET_COLOR_TARGET_WIDTH(0):
        case LW9097_SET_COLOR_TARGET_WIDTH(1):
        case LW9097_SET_COLOR_TARGET_WIDTH(2):
        case LW9097_SET_COLOR_TARGET_WIDTH(3):
        case LW9097_SET_COLOR_TARGET_WIDTH(4):
        case LW9097_SET_COLOR_TARGET_WIDTH(5):
        case LW9097_SET_COLOR_TARGET_WIDTH(6):
        case LW9097_SET_COLOR_TARGET_WIDTH(7):
            target->SetWidth(DRF_VAL(9097, _SET_COLOR_TARGET_WIDTH, _V, data));
            target->SetOverwriteWidth(true);
            break;
        case LW9097_SET_COLOR_TARGET_HEIGHT(0):
        case LW9097_SET_COLOR_TARGET_HEIGHT(1):
        case LW9097_SET_COLOR_TARGET_HEIGHT(2):
        case LW9097_SET_COLOR_TARGET_HEIGHT(3):
        case LW9097_SET_COLOR_TARGET_HEIGHT(4):
        case LW9097_SET_COLOR_TARGET_HEIGHT(5):
        case LW9097_SET_COLOR_TARGET_HEIGHT(6):
        case LW9097_SET_COLOR_TARGET_HEIGHT(7):
            target->SetHeight(DRF_VAL(9097, _SET_COLOR_TARGET_HEIGHT, _V, data));
            target->SetOverwriteHeight(true);
            break;
        case LW9097_SET_COLOR_TARGET_FORMAT(0):
        case LW9097_SET_COLOR_TARGET_FORMAT(1):
        case LW9097_SET_COLOR_TARGET_FORMAT(2):
        case LW9097_SET_COLOR_TARGET_FORMAT(3):
        case LW9097_SET_COLOR_TARGET_FORMAT(4):
        case LW9097_SET_COLOR_TARGET_FORMAT(5):
        case LW9097_SET_COLOR_TARGET_FORMAT(6):
        case LW9097_SET_COLOR_TARGET_FORMAT(7):
            color_format = DRF_VAL(9097, _SET_COLOR_TARGET_FORMAT, _V, data);
            if(color_format)
            {
                target->SetColorFormat(m_Test->GetSurfaceMgr()->FormatFromDeviceFormat(color_format));
                target->SetOverwriteColorFormat(true);
            }
            break;
        case LW9097_SET_COLOR_TARGET_MEMORY(0):
        case LW9097_SET_COLOR_TARGET_MEMORY(1):
        case LW9097_SET_COLOR_TARGET_MEMORY(2):
        case LW9097_SET_COLOR_TARGET_MEMORY(3):
        case LW9097_SET_COLOR_TARGET_MEMORY(4):
        case LW9097_SET_COLOR_TARGET_MEMORY(5):
        case LW9097_SET_COLOR_TARGET_MEMORY(6):
        case LW9097_SET_COLOR_TARGET_MEMORY(7):
            target->SetLogBlockWidth(DRF_VAL(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_WIDTH, data));
            target->SetLogBlockHeight(DRF_VAL(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_HEIGHT, data));
            target->SetLogBlockDepth(DRF_VAL(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_DEPTH, data));
            target->SetLayout((FLD_TEST_DRF(9097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, _BLOCKLINEAR,
                data))?Surface2D::BlockLinear:Surface2D::Pitch);
            target->SetOverwriteLogBlockWidth(true);
            target->SetOverwriteLogBlockHeight(true);
            target->SetOverwriteLogBlockDepth(true);
            target->SetOverwriteLayout(true);
            target->SetIsSettingDepth(FLD_TEST_DRF(9097, _SET_COLOR_TARGET_MEMORY, _THIRD_DIMENSION_CONTROL,
                _THIRD_DIMENSION_DEFINES_DEPTH_SIZE, data)); // Check it's representing depth or arraysize
            break;
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(0):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(1):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(2):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(3):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(4):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(5):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(6):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(7):
            if (target->GetIsSettingDepth())
            {
                target->SetDepth(DRF_VAL(9097, _SET_COLOR_TARGET_THIRD_DIMENSION, _V, data));
                target->SetOverwriteDepth(true);
            }
            else
            {
                target->SetArraySize(DRF_VAL(9097, _SET_COLOR_TARGET_THIRD_DIMENSION, _V, data));
                target->SetOverwriteArraySize(true);
            }
            break;
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(0):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(1):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(2):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(3):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(4):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(5):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(6):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(7):
            if (data != 0)
            {
                target->SetArrayPitch(DRF_VAL(9097, _SET_COLOR_TARGET_ARRAY_PITCH, _V, data) << 2);
                target->SetOverwriteArrayPitch(true);
            }
            break;
        case LW9097_SET_COLOR_TARGET_LAYER(0):
        case LW9097_SET_COLOR_TARGET_LAYER(1):
        case LW9097_SET_COLOR_TARGET_LAYER(2):
        case LW9097_SET_COLOR_TARGET_LAYER(3):
        case LW9097_SET_COLOR_TARGET_LAYER(4):
        case LW9097_SET_COLOR_TARGET_LAYER(5):
        case LW9097_SET_COLOR_TARGET_LAYER(6):
        case LW9097_SET_COLOR_TARGET_LAYER(7):
            // do nothing
            break;
        case LW9097_SET_COLOR_TARGET_MARK(0):
        case LW9097_SET_COLOR_TARGET_MARK(1):
        case LW9097_SET_COLOR_TARGET_MARK(2):
        case LW9097_SET_COLOR_TARGET_MARK(3):
        case LW9097_SET_COLOR_TARGET_MARK(4):
        case LW9097_SET_COLOR_TARGET_MARK(5):
        case LW9097_SET_COLOR_TARGET_MARK(6):
        case LW9097_SET_COLOR_TARGET_MARK(7):
            // do nothing
            break;
        default:
            // Should not got here
            ErrPrintf("SetActiveCSurface: Invalid offset %d\n", method_offset);
            MASSERT(0);
        }
    }
}

// Set current/active z surface
void BufferDumperFermi::SetActiveZSurface
(
    CachedSurface *cs,  // Pushbuffer to be parsed
    UINT32 ptr,         // Start addr of pb to be parsed
    UINT32 pbSize       // Pushbuffer size
)
{
    RenderTarget *target = m_Zt;
    UINT32 header = Get032(cs, ptr, 0);

    UINT32 method_offset = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS, header)<<2;
    UINT32 num_dwords = max(REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header), (UINT32)1);
    UINT32 opcode2 = REF_VAL(LW_FIFO_DMA_SEC_OP, header);
    UINT32 data = 0;

    if( ((ptr + 4) >= pbSize) && (LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD != opcode2) )
    {
         ErrPrintf( "BufferDumperFermi::SetActiveZSurface failed: the header %x is the end of Pushbuffer "
                    "and opcode2 is %d.\n", header, opcode2);
         return;
    }

    ptr += 4;

    for (UINT32 i=0; i<num_dwords; ++i, method_offset+=4)
    {
        if (LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD == opcode2)
        {
            data = REF_VAL(LW_FIFO_DMA_IMMD_DATA, header);
        }
        else
        {
            data = Get032(cs, ptr, 0);
            ptr += 4;
        }

        switch (method_offset)
        {
        case LW9097_SET_ZT_A:
            target->SetOffsetUpper(DRF_VAL(9097, _SET_ZT_A, _OFFSET_UPPER, data));
            break;
        case LW9097_SET_ZT_B:
            target->SetOffsetLower(DRF_VAL(9097, _SET_ZT_B, _OFFSET_LOWER, data));
            break;
        case LW9097_SET_ZT_FORMAT:
            target->SetColorFormat(
                m_Test->GetSurfaceMgr()->FormatFromDeviceFormat(
                DRF_VAL(9097, _SET_ZT_FORMAT, _V, data)));
            target->SetOverwriteColorFormat(true);
            break;
        case LW9097_SET_ZT_BLOCK_SIZE:
            target->SetLogBlockWidth(
                DRF_VAL(9097, _SET_ZT_BLOCK_SIZE, _WIDTH, data));
            target->SetLogBlockHeight(
                DRF_VAL(9097, _SET_ZT_BLOCK_SIZE, _HEIGHT, data));
            target->SetLogBlockDepth(
                DRF_VAL(9097, _SET_ZT_BLOCK_SIZE, _DEPTH, data));
            target->SetOverwriteLogBlockWidth(true);
            target->SetOverwriteLogBlockHeight(true);
            target->SetOverwriteLogBlockDepth(true);
            break;
        case LW9097_SET_ZT_ARRAY_PITCH:
            if (data != 0)
            {
                target->SetArrayPitch(DRF_VAL(9097, _SET_ZT_ARRAY_PITCH, _V, data) << 2);
                target->SetOverwriteArrayPitch(true);
            }
            break;
        case LW9097_SET_ZT_MARK:
            // Do nothing
            DebugPrintf(MSGID(), "SetActiveZSurface: do nothing for SetZtMark\n");
            break;
        case LW9097_SET_ZT_SIZE_A:
            target->SetWidth(DRF_VAL(9097, _SET_ZT_SIZE_A, _WIDTH, data));
            target->SetLayout(Surface2D::BlockLinear);
            target->SetOverwriteWidth(true);
            target->SetOverwriteLayout(true);
            break;
        case LW9097_SET_ZT_SIZE_B:
            target->SetHeight(DRF_VAL(9097, _SET_ZT_SIZE_B, _HEIGHT, data));
            target->SetOverwriteHeight(true);
            break;
        case LW9097_SET_ZT_SIZE_C:
            // do nothing
            DebugPrintf(MSGID(), "SetActiveZSurface: do nothing for SetZtSizeC\n");
            break;
        case LW9097_SET_ZT_SELECT:
            m_ZtCount = DRF_VAL(9097, _SET_ZT_SELECT, _TARGET_COUNT, data);
            break;
        case LW9097_SET_ZT_LAYER:
            // do nothing
            DebugPrintf(MSGID(), "SetActiveZSurface: do nothing for SetZtLayer\n");
            break;
        default:
            // Should not got here
            ErrPrintf("SetActiveZSurface: Invalid offset %d\n", method_offset);
            MASSERT(0);
        }
    }
}

RC TraceRelocationClear::DoOffset(TraceModule *module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT32 orig_val = module->Get032(m_Offset, subdev);
    orig_val = FLD_SET_DRF_NUM(_FIFO, _DMA_METHOD, _ADDRESS, LW9097_NO_OPERATION >> 2, orig_val);

    module->Put032(m_Offset, orig_val, subdev);
    return OK;
}

void GenericTraceModule::SkipFermiGeometry(UINT32 start_geometry, UINT32 end_geometry)
{
    NullifyBeginEndMethodsFermi(FERMI_A, LW9097_BEGIN, LW9097_END,
                                LW9097_NO_OPERATION, start_geometry,
                                end_geometry);
}

UINT32 Trace3DTest::GetFermiMethodResetValue(UINT32 Method) const
{
    int i;
    MdiagSurf *surf;
    switch (Method)
    {
        case LW9097_SET_COLOR_TARGET_FORMAT(0):
        case LW9097_SET_COLOR_TARGET_FORMAT(1):
        case LW9097_SET_COLOR_TARGET_FORMAT(2):
        case LW9097_SET_COLOR_TARGET_FORMAT(3):
        case LW9097_SET_COLOR_TARGET_FORMAT(4):
        case LW9097_SET_COLOR_TARGET_FORMAT(5):
        case LW9097_SET_COLOR_TARGET_FORMAT(6):
        case LW9097_SET_COLOR_TARGET_FORMAT(7):
            i = (Method - LW9097_SET_COLOR_TARGET_FORMAT(0)) /
                (LW9097_SET_COLOR_TARGET_FORMAT(1) - LW9097_SET_COLOR_TARGET_FORMAT(0));
            surf = surfC[i];
            if (gsm->GetValid(surf))
                return gsm->DeviceFormatFromFormat(surf->GetColorFormat());
            else
                return LW9097_SET_COLOR_TARGET_FORMAT_V_DISABLED;
        case LW9097_SET_COLOR_TARGET_MEMORY(0):
        case LW9097_SET_COLOR_TARGET_MEMORY(1):
        case LW9097_SET_COLOR_TARGET_MEMORY(2):
        case LW9097_SET_COLOR_TARGET_MEMORY(3):
        case LW9097_SET_COLOR_TARGET_MEMORY(4):
        case LW9097_SET_COLOR_TARGET_MEMORY(5):
        case LW9097_SET_COLOR_TARGET_MEMORY(6):
        case LW9097_SET_COLOR_TARGET_MEMORY(7):
        {
            i = (Method - LW9097_SET_COLOR_TARGET_MEMORY(0)) /
                (LW9097_SET_COLOR_TARGET_MEMORY(1) - LW9097_SET_COLOR_TARGET_MEMORY(0));
            surf = surfC[i];
            UINT32 layout = surf->GetLayout() == Surface2D::BlockLinear ?
                DRF_DEF(9097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, _BLOCKLINEAR) :
                DRF_DEF(9097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, _PITCH);
            UINT32 third_dimension = surf->GetArraySize() > 1 ?
                DRF_DEF(9097, _SET_COLOR_TARGET_MEMORY, _THIRD_DIMENSION_CONTROL, _THIRD_DIMENSION_DEFINES_ARRAY_SIZE) :
                DRF_DEF(9097, _SET_COLOR_TARGET_MEMORY, _THIRD_DIMENSION_CONTROL, _THIRD_DIMENSION_DEFINES_DEPTH_SIZE);
            return layout | third_dimension |
                DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_WIDTH, surf->GetLogBlockWidth()) |
                DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_HEIGHT, surf->GetLogBlockHeight()) |
                DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_DEPTH, surf->GetLogBlockDepth())
                ;
        }
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(0):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(1):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(2):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(3):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(4):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(5):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(6):
        case LW9097_SET_COLOR_TARGET_ARRAY_PITCH(7):
            i = (Method - LW9097_SET_COLOR_TARGET_ARRAY_PITCH(0)) /
                (LW9097_SET_COLOR_TARGET_ARRAY_PITCH(1) - LW9097_SET_COLOR_TARGET_ARRAY_PITCH(0));
            surf = surfC[i];
            return (UINT32)(surf->GetArrayPitch() >> 2);

        case LW9097_SET_COLOR_TARGET_WIDTH(0):
        case LW9097_SET_COLOR_TARGET_WIDTH(1):
        case LW9097_SET_COLOR_TARGET_WIDTH(2):
        case LW9097_SET_COLOR_TARGET_WIDTH(3):
        case LW9097_SET_COLOR_TARGET_WIDTH(4):
        case LW9097_SET_COLOR_TARGET_WIDTH(5):
        case LW9097_SET_COLOR_TARGET_WIDTH(6):
        case LW9097_SET_COLOR_TARGET_WIDTH(7):
            i = (Method - LW9097_SET_COLOR_TARGET_WIDTH(0)) /
                (LW9097_SET_COLOR_TARGET_WIDTH(1) - LW9097_SET_COLOR_TARGET_WIDTH(0));
            surf = surfC[i];
            return surf->GetWidth();

        case LW9097_SET_COLOR_TARGET_HEIGHT(0):
        case LW9097_SET_COLOR_TARGET_HEIGHT(1):
        case LW9097_SET_COLOR_TARGET_HEIGHT(2):
        case LW9097_SET_COLOR_TARGET_HEIGHT(3):
        case LW9097_SET_COLOR_TARGET_HEIGHT(4):
        case LW9097_SET_COLOR_TARGET_HEIGHT(5):
        case LW9097_SET_COLOR_TARGET_HEIGHT(6):
        case LW9097_SET_COLOR_TARGET_HEIGHT(7):
            i = (Method - LW9097_SET_COLOR_TARGET_HEIGHT(0)) /
                (LW9097_SET_COLOR_TARGET_HEIGHT(1) - LW9097_SET_COLOR_TARGET_HEIGHT(0));
            surf = surfC[i];
            return surf->GetHeight();

        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(0):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(1):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(2):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(3):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(4):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(5):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(6):
        case LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(7):
            i = (Method - LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(0)) /
                (LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(1) - LW9097_SET_COLOR_TARGET_THIRD_DIMENSION(0));
            surf = surfC[i];
            return surf->GetDepth();
        case LW9097_SET_ZT_SELECT:
            return gsm->GetValid(surfZ) ? 1 : 0;
        case LW9097_SET_ZT_FORMAT:
            if (gsm->GetValid(surfZ))
                return gsm->DeviceFormatFromFormat(surfZ->GetColorFormat());
            else
            {
                ErrPrintf("GetFermiMethodResetValue: Z surface is disabled, reloc method SetZtFormat to 0\n");
                return 0;
            }
        case LW9097_SET_ZT_BLOCK_SIZE:
            if (gsm->GetValid(surfZ))
            {
                return
                    DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _WIDTH, surfZ->GetLogBlockWidth()) |
                    DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _HEIGHT, surfZ->GetLogBlockHeight()) |
                    DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _DEPTH, surfZ->GetLogBlockDepth());
            }
            else
            {
                ErrPrintf("GetFermiMethodResetValue: Z surface is disabled, reloc method SetZtBlockSize to 0\n");
                return 0;
            }
        case LW9097_SET_ZT_ARRAY_PITCH:
            if (gsm->GetValid(surfZ))
                return (UINT32)(surfZ->GetArrayPitch() >> 2);
            else
            {
                ErrPrintf("GetFermiMethodResetValue: Z surface is disabled, reloc method SetZtArrayPitch to 0\n");
                return 0;
            }
        case LW9097_SET_ZT_SIZE_A:
            if (gsm->GetValid(surfZ))
                return DRF_NUM(9097, _SET_ZT_SIZE_A, _WIDTH, surfZ->GetAllocWidth());
            else
            {
                ErrPrintf("GetFermiMethodResetValue: Z surface is disabled, reloc method SetZtSizeA to 0\n");
                return 0;
            }
        case LW9097_SET_ZT_SIZE_B:
            if (gsm->GetValid(surfZ))
                return DRF_NUM(9097, _SET_ZT_SIZE_B, _HEIGHT, surfZ->GetAllocHeight());
            else
            {
                ErrPrintf("GetFermiMethodResetValue: Z surface is disabled, reloc method SetZtSizeB to 0\n");
                return 0;
            }
        case LW9097_SET_ZT_SIZE_C:
            if (gsm->GetValid(surfZ))
            {
                return DRF_NUM(9097, _SET_ZT_SIZE_C, _THIRD_DIMENSION, surfZ->GetDepth()) |
                       DRF_DEF(9097, _SET_ZT_SIZE_C, _CONTROL, _ARRAY_SIZE_IS_ONE);
            }
            else
            {
                ErrPrintf("GetFermiMethodResetValue: Z surface is disabled, reloc method SetZtSizeC to 0\n");
                return 0;
            }
        case LW9097_SET_ANTI_ALIAS:
            switch(GetAAModeFromSurface(surfZ))
            {
                case AAMODE_1X1            :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_1X1);
                case AAMODE_2X1            :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X1);
                case AAMODE_2X2            :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2);
                case AAMODE_4X2            :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2);
                case AAMODE_4X4            :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X4);
                case AAMODE_2X1_D3D        :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X1_D3D);
                case AAMODE_4X2_D3D        :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_D3D);
                case AAMODE_2X2_VC_4       :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2_VC_4);
                case AAMODE_2X2_VC_12      :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2_VC_12);
                case AAMODE_4X2_VC_8       :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_VC_8);
                case AAMODE_4X2_VC_24      :
                    return DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_VC_24);
                default:
                    ErrPrintf("Invalid AA mode when resetting method with offset 0x%x\n", Method);
                    MASSERT(0);
                    return 0;
            }
        case LW9097_SET_ANTI_ALIAS_ENABLE:
            if (gsm->GetMultiSampleOverride())
                return DRF_NUM(9097, _SET_ANTI_ALIAS_ENABLE, _V, gsm->GetMultiSampleOverrideValue());
            else
                return DRF_DEF(9097, _SET_ANTI_ALIAS_ENABLE, _V, _FALSE);
        default:
            ErrPrintf("Don't know how to reset method with offset 0x%x\n", Method);
            MASSERT(0);
            return 0;
    }
}

MdiagSurf* GpuTrace::GetZLwllStorage(UINT32 index, LwRm::Handle hVASpace)
{
    RC rc;
    ZLwllStorage::const_iterator iter = m_ZLwllStorage.find(index);

    if (iter == m_ZLwllStorage.end())
    {
        MdiagSurf* buff = new MdiagSurf;
        m_ZLwllStorage[index] = buff;
        LwRm* pLwRm = m_Test->GetLwRmPtr();
        SmcEngine* pSmcEngine = m_Test->GetSmcEngine();
        LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(
            m_Test->GetBoundGpuDevice()->GetSubdevice(0));

        // bug 365606, remove obsolete references to LW_PTOP_FS_NUM_GPCS
        LW2080_CTRL_GR_INFO grInfo[2];
        grInfo[0].index = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT;
        grInfo[1].index = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_GPCS;

        LW2080_CTRL_GR_GET_INFO_PARAMS params = {0};
        params.grInfoListSize = 2;
        params.grInfoList = (LwP64)&grInfo[0];

        rc = pLwRm->Control(subdevHandle,
            LW2080_CTRL_CMD_GR_GET_INFO,
            &params,
            sizeof(params));

        if (rc != OK)
        {
            ErrPrintf("Error reading number of GPC's from RM: %s\n", rc.Message());
            return NULL;
        }

        // callwlate size and allocate zlwll storage
        DebugPrintf(MSGID(), "Number of GPC's: %d\n", grInfo[0].data);

        LwU32 aliquotTotal;

        if (Platform::GetSimulationMode() != Platform::Amodel)
        {
            LW2080_CTRL_GR_GET_ZLWLL_INFO_PARAMS grZlwllParams = {0};

            rc = pLwRm->Control(subdevHandle,
                LW2080_CTRL_CMD_GR_GET_ZLWLL_INFO,
                (void *) &grZlwllParams,
                sizeof(grZlwllParams));

            if (rc != OK)
            {
                ErrPrintf("Error reading ZLWLL Info from RM: %s\n", rc.Message());
                return NULL;
            }

            aliquotTotal = grZlwllParams.aliquotTotal;
        }
        else
        {
            // Can get zlwll params with Amodel.  But Amodel
            // doesn't implement zlwll anyway, so using
            // zero for the number of aliquots is fine.
            aliquotTotal = 0;
        }

        RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, pLwRm, pSmcEngine);
        UINT64 size = (grInfo[0].data) * (aliquotTotal *
             regHal.LookupAddress(MODS_PGRAPH_ZLWLL_BYTES_PER_ALIQUOT_PER_GPC)) +
             (grInfo[1].data *
             regHal.LookupAddress(MODS_PGRAPH_ZLWLL_SAVE_RESTORE_HEADER_BYTES_PER_GPC));
        if (size == 0) size = 1;
        buff->SetArrayPitch(size);
        buff->SetColorFormat(ColorUtils::Y8);
        buff->SetForceSizeAlloc(true);
        buff->SetLocation(Memory::Fb);
        buff->SetGpuVASpace(hVASpace);
        if (this->params->ParamPresent("-va_reverse") ||
            this->params->ParamPresent("-va_reverse_zlwll"))
        {
            buff->SetVaReverse(true);
        }
        else
        {
            buff->SetVaReverse(false);
        }
        if (this->params->ParamPresent("-pa_reverse") ||
            this->params->ParamPresent("-pa_reverse_zlwll"))
        {
            buff->SetPaReverse(true);
        }
        else
        {
            buff->SetPaReverse(false);
        }
        buff->Alloc(m_Test->GetBoundGpuDevice(), pLwRm);
        PrintDmaBufferParams(*buff);

        return buff;
    }
    else
    {
        return iter->second;
    }
}

RC GenericTraceModule::RemoveFermiMethods(UINT32 subdev)
{
    RC rc;
    UINT32 header, opcode2;

    // If none of the color/z surfaces will be cleared due a GPU method
    // (usually the result of a compressed surface or a -gpu_clear argument),
    // then a backdoor clear will be used.  In this case, all of the methods
    // marked with the CLEAR_INIT command in the trace need to be removed.
    if (!GetTest()->GetSurfaceMgr()->UsingGpuClear())
    {
        DebugPrintf("Removing CLEAR_INIT methods.\n");

        m_RemoveMethods.insert(m_RemoveMethods.end(),
            m_ClearInitMethods.begin(), m_ClearInitMethods.end());
    }

    vector<UINT32>::const_iterator end = m_RemoveMethods.end();
    for (vector<UINT32>::const_iterator iter = m_RemoveMethods.begin(); iter != end; ++iter)
    {
        header = Get032(*iter, subdev);
        opcode2 = REF_VAL(LW_FIFO_DMA_SEC_OP, header);

        header = FLD_SET_DRF_NUM(_FIFO, _DMA, _METHOD_ADDRESS, LW9097_NO_OPERATION >> 2, header);

        switch (opcode2)
        {
            case LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD:
                header = FLD_SET_DRF_NUM(_FIFO, _DMA, _IMMD_DATA, 0, header);
                break;
            case LW_FIFO_DMA_SEC_OP_INC_METHOD:
            case LW_FIFO_DMA_SEC_OP_ONE_INC:
            case LW_FIFO_DMA_SEC_OP_NON_INC_METHOD:
            default:
                MASSERT(!"Not supported yet");
                return RC::SOFTWARE_ERROR;
        }

        Put032(*iter, header, subdev);
    }
    return rc;
}

RC TraceChannel::SetFermiMethodCount(LWGpuChannel* ch)
{
    // It's invalid for channel without pb module to call this function
    MASSERT(m_PbufModules.begin() != m_PbufModules.end());

    // If we are context switching, we want to allow context switches to occur durring the setup methods,
    // as well as the methods in the trace. So we add the number of methods sent in setup to the number
    // in the trace and report it here to the LWGPUChannel class via SetMethodCount().
    RC rc;
    TraceSubChannel *ptracesubch = GetSubChannel("");
    bool ctxsw = m_Test->IsTestCtxSwitching();

    // if context switching or using policy manager or subchswitch,
    // count the number of methods sent
    if (ctxsw || PolicyManager::Instance()->IsInitialized() ||
        Utl::HasInstance() ||
        params->ParamPresent("-plugin") ||
        GetChHelper()->subchannel_switching_enabled())
    {
        bool bFound = false;
        UINT32 num_methods = 0;
        UINT32 num_writes = 0;
        UINT32 scannedMethodOffset = 0;
        bool bCtxswScanMethod = params->ParamPresent("-ctxsw_scan_method") > 0;
        UINT32 ctxswMethod = params->ParamUnsigned("-ctxsw_scan_method", 0xFFFFFFFF);

        ModuleIter iMod = m_PbufModules.begin();
        for (; iMod != m_PbufModules.end(); iMod ++)
        {
            TraceModule::SendSegments& sendSegments = *((*iMod)->GetSegments());

            UINT32 header, opcode2, num_dwords;
            UINT32 ptr = 0;
            UINT32 lwrrent_segment=0, segment_count=sendSegments.size();
            UINT32 end_segment = sendSegments[0].Start + sendSegments[0].Size;
            UINT32 end_all     = sendSegments[segment_count-1].Start +
                sendSegments[segment_count-1].Size;

            MethodType mode = NON_INCREMENTING;
            UINT32 lwrrentMethodOffset = 0;

            while (ptr < end_all)
            {
                header = (*iMod)->Get032(ptr, 0);
                num_dwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header);
                opcode2 = REF_VAL(LW_FIFO_DMA_SEC_OP, header);
                lwrrentMethodOffset = num_methods;

                ptr += 4; // header

                //
                // Only those methods sent by MethodWriteN_RC() need to be counted
                // Please keep consistence with SendFermiMethodSegment()
                // num_dwords = 0 means no method to be counted
                //
                if (header == LW_FIFO_DMA_NOP)
                {
                    num_dwords = 0;
                }
                else if (ptr >= end_segment)
                {
                    // writing header without data
                    if (opcode2 == LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD)
                    {
                        ++num_methods;
                        ++num_writes;
                        num_dwords = 1;
                        mode = INCREMENTING_IMM;
                    }
                    else
                    {
                        num_dwords = 0;
                    }
                }
                else
                {
                    switch (opcode2)
                    {
                    case LW_FIFO_DMA_SEC_OP_NON_INC_METHOD:
                        num_methods += num_dwords;
                        ++num_writes;
                        ptr += 4*num_dwords;
                        mode = NON_INCREMENTING;
                        break;
                    case LW_FIFO_DMA_SEC_OP_ONE_INC:
                        num_methods += num_dwords;
                        ++num_writes;
                        ptr += 4*num_dwords;
                        mode = INCREMENTING_ONCE;
                        break;
                    case LW_FIFO_DMA_SEC_OP_INC_METHOD:
                        num_methods += num_dwords;
                        ++num_writes;
                        ptr += 4*num_dwords;
                        mode = INCREMENTING;
                        break;
                    case LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD:
                        ++num_methods;
                        ++num_writes;
                        num_dwords = 1;
                        mode = INCREMENTING_IMM;
                        break;
                    case LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT:
                        {
                            UINT32 opcode3 = REF_VAL(LW_FIFO_DMA_TERT_OP, header);
                            if (opcode3 == LW_FIFO_DMA_TERT_OP_GRP0_INC_METHOD)
                            {
                                num_dwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);
                                num_methods += num_dwords;
                                ++num_writes;
                                ptr += 4*num_dwords;
                                mode = INCREMENTING;
                            }
                            else
                            {
                                // Not count because it's not sent by MethodWriteN_RC
                                num_dwords = 0;
                            }
                            break;
                        }
                    case LW_FIFO_DMA_SEC_OP_GRP2_USE_TERT:
                        num_dwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);
                        num_methods += num_dwords;
                        ++num_writes;
                        ptr += 4*num_dwords;
                        mode = NON_INCREMENTING;
                        break;
                    case LW_FIFO_DMA_SEC_OP_END_PB_SEGMENT:
                        num_dwords = 0;
                        break;
                    default:
                        ErrPrintf("Can't parse pb\n");
                        return RC::SOFTWARE_ERROR;
                    }
                }

                if (bCtxswScanMethod && !bFound && num_dwords > 0)
                {
                    UINT32 methodStart =
                        REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS, header) << 2;
                    UINT32 methodEnd = ch->GetMethodAddr(methodStart,
                                                         num_dwords - 1, mode);

                    if (methodStart <= ctxswMethod &&
                        ctxswMethod <= methodEnd)
                    {
                        bFound = true;
                        scannedMethodOffset = lwrrentMethodOffset +
                                              ((ctxswMethod - methodStart) >> 2);
                    }
                }

                if (ptr >= end_segment) // end of segment
                {
                    if (++lwrrent_segment < segment_count)
                    {
                        end_segment = sendSegments[lwrrent_segment].Start +
                            sendSegments[lwrrent_segment].Size;
                        ptr = sendSegments[lwrrent_segment].Start;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        if (bCtxswScanMethod)
        {
            if (bFound)
            {
                // suspend ctxsw until sending methods segment
                ch->SetSuspendCtxSw(true);
                ch->SetCtxswPointsBase(scannedMethodOffset);
            }
            else
            {
                ErrPrintf("Can't find the method specified by option "
                    "-ctxsw_scan_method 0x%x.\n", ctxswMethod);
                return RC::SOFTWARE_ERROR;
            }
        }

        // Add the number of methods sent in setup to the number of methods
        // in the trace
        // HACK!  assume about 15 methods sent in set up.  really depends on
        // args
        // Bug401670: In most cases, MODS only need to inject couples of
        // methods for gr channel
        if (ptracesubch->GrSubChannel())
        {
            const UINT32 grChannelInitMethodsCount = 15;
            ch->SetChannelInitMethodCount(grChannelInitMethodsCount);
            num_methods += grChannelInitMethodsCount;
        }

        //
        // GpuChannelHelper::SetMethodCount will
        //    set MethodCount to LWGpuChannel for ctxswitch
        // GpuChannelHelper::SetMethodWriteCount
        //    setup subchswitch points if subchswitch is enabled.
        //    Unlike ctxswitch, subchswitch will not split a write
        GetChHelper()->SetMethodCount(num_methods);
        GetChHelper()->SetMethodWriteCount(num_writes);

        InfoPrintf("Total number of methods this test will send is %d,"
                "Total number of methods writes this test will send is %d\n"
                , num_methods, num_writes);
    }

    return rc;
}

// ----------------------------------------------------------------------------
// Since Tesla & Fermi has different MACROes, so add this function to return
// data count in DW after the header, tesla verion is at Tesla_support.cpp
UINT32 GenericTraceModule::GetNumDWFermi(UINT32 header) const
{
    return REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header);
}

// ----------------------------------------------------------------------------
// Modify SPH fields before downloading to chips
// We support global and per-file update, the later has higher priority
RC GenericTraceModule::UpdateSph(UINT32 gpuSubdevIdx)
{
    // Lwrrently only fields in the first 5 dwords will be modified
    // And VS/PS have same SPH struct in that fields, so share the
    // struct is OK for now, IMAP/OMAP update has individual  implemantation
    // see bug 446642
    FermiGraphicsStructs::StructSphType01Version03Flat *fields =
        (FermiGraphicsStructs::StructSphType01Version03Flat*)
        (m_CachedSurface->GetPtr(gpuSubdevIdx));

    // FakeGL uses sph_offset to indicate the sph real start, for fermi, it's 0,
    // for kepler, it's 48 bytes. Bug 698005.
    if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_KEPLER) &&
        !EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_TURING))
    {
        fields = (FermiGraphicsStructs::StructSphType01Version03Flat*)
            ((UINT08*)fields + 48);
    }

    typedef map<string, unsigned long long> UpdateMap; // Store the final value
    UpdateMap update_table;

    // Update according to global options
    if (params->ParamPresent("-sph_force_attr_bits") > 0)
    {
        // The IMAP/OMAP update relies on many factors in hw detail,
        // so some logic is from fmodel implementation, see:
        // //hw/fermi1_gf100/fmod/ds/ds.h
        // //hw/fermi1_gf100/fmod/ds/ds_main.cpp
        // //hw/fermi1_gf100/fmod/ds/ds_m_pipe.cpp

        // Parse argument
        UINT32 attr_mask[DWORDS_PER_MAP] = {0}; // For each IMAP/OMAP has 240 bits
        string bits_str = params->ParamStr("-sph_force_attr_bits");
        while (bits_str != "")
        {
            UINT32 begin = 0, end = 0;
            string one_pair;
            size_t pos = bits_str.find_first_of(',');
            if (pos == string::npos)
            {
                // Last pair
                one_pair = bits_str;
                bits_str = "";
            }
            else
            {
                one_pair = bits_str.substr(0, pos);
                bits_str = bits_str.substr(pos + 1, bits_str.size());
            }

            if (2 != sscanf(one_pair.c_str(), "%d..%d", &begin, &end) ||
                (begin > end))
            {
                ErrPrintf("Unrecognized -sph_force_attr_bits argument, "
                    "format: -sph_force_attr_bits ##..##,##..##\n");
                return RC::BAD_PARAMETER;
            }
            // Fix me: hack to discard all setting to bits over 140
            // See bug 738132
            for (UINT32 i = begin; i <= min(140u, end); i++)
            {
                SET_BMAP_BIT(attr_mask, i);
            }
        }

        // Begin to update bits
        // We can operate bit fields directly since it's already swapped on
        // big-endian platform
        UINT32 cbsize = 0;
        if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
        {
            cbsize = GetPascalCbsize(gpuSubdevIdx);
        }
        else if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_KEPLER))
        {
            cbsize = GetKeplerCbsize(gpuSubdevIdx);
        }
        else
        {
            cbsize = GetFermiCbsize(gpuSubdevIdx);
        }

        const UINT32 max_attr = (cbsize >> 2) - 6;
        UINT32* const map_start = (UINT32*)fields + 5;
        const FermiGraphicsStructs::TYPEDEF_SPH_VTG_ODMAP *AamOdMap[] =
        {
            FermiGraphicsStructs::AamOdmapLwllBeforeFetchVersion02,
            FermiGraphicsStructs::AamOdmapVertexVersion02,
            FermiGraphicsStructs::AamOdmapTessellationInitVersion02,
            FermiGraphicsStructs::AamOdmapTessellatiolwersion02,
            FermiGraphicsStructs::AamOdmapGeometryVersion02
        };
        const FermiGraphicsStructs::TYPEDEF_SPH_VTG_IDMAP *AamIdMap[] =
        {
            FermiGraphicsStructs::AamIdmapLwllBeforeFetchVersion02,
            FermiGraphicsStructs::AamIdmapVertexVersion02,
            FermiGraphicsStructs::AamIdmapTessellationInitVersion02,
            FermiGraphicsStructs::AamIdmapTessellatiolwersion02,
            FermiGraphicsStructs::AamIdmapGeometryVersion02,
            FermiGraphicsStructs::AamIdmapPixelVersion03
        };

        const FermiGraphicsStructs::TYPEDEF_SHADER_TYPE type =
            (FermiGraphicsStructs::TYPEDEF_SHADER_TYPE)(fields->ShaderType);
        if (type < FermiGraphicsStructs::PIXEL)
        {
            InfoPrintf("Updating SPH masks for %s: %s\n", m_ModuleName.c_str(),
                params->ParamStr("-sph_force_attr_bits"));

            UINT32 imap[DWORDS_PER_MAP];
            UINT32 omap[DWORDS_PER_MAP];
            for (int i = 0; i < DWORDS_PER_MAP; i++)
            {
                imap[i] = map_start[i];
                omap[i] = map_start[DWORDS_PER_MAP - 1  + i];
            }
            for (int i = 0; i < DWORDS_PER_MAP-1; i++)
            {
                omap[i] = (omap[i] >> 16) | (omap[i+1] << 16);
            }
            omap[DWORDS_PER_MAP-1] = omap[DWORDS_PER_MAP-1] >> 16;
            imap[DWORDS_PER_MAP-1]&= 0xffff;

            // Original values in file
            string imap_str, omap_str;
            for (int i = 0; i < DWORDS_PER_MAP; i++)
            {
                imap_str += Utility::StrPrintf("%08x ", imap[i]);
                omap_str += Utility::StrPrintf("%08x ", omap[i]);
            }
            InfoPrintf("SPH IMAP before update %s: 0x%s\n",
                m_ModuleName.c_str(), imap_str.c_str());
            InfoPrintf("SPH OMAP before update %s: 0x%s\n",
                m_ModuleName.c_str(), omap_str.c_str());

            // OR the masks into IMAP
            UINT32 popcount = PopCountMap(imap);
            for (int i = 0; i < SPH_ATTRIBUTE_SIZE && popcount < max_attr; i++)
            {
                if (GET_BMAP_BIT(attr_mask, i) &&
                    !GET_BMAP_BIT(imap,i) &&
                    (AamIdMap[type][i] !=
                     FermiGraphicsStructs::TYPEDEF_SPH_VTG_IDMAPDEFAULT))
                {
                    SET_BMAP_BIT(imap, i);
                    popcount++;
                }
            }
            // OR the masks into OMAP
            popcount = PopCountMap(omap);
            for (int i = 0; i < SPH_ATTRIBUTE_SIZE && popcount < max_attr &&
                (type != FermiGraphicsStructs::GEOMETRY ||
                ((popcount+1) * fields->MaxOutputVertexCount) <= 1024); i++)
            {
                if (GET_BMAP_BIT(attr_mask, i) &&
                    !GET_BMAP_BIT(omap,i) &&
                    (AamOdMap[type][i] !=
                     FermiGraphicsStructs::TYPEDEF_SPH_VTG_ODMAPDISCARD))
                {
                    SET_BMAP_BIT(omap, i);
                    popcount++;
                }
            }

            // The new value
            imap_str = "", omap_str = "";
            for (int i = 0; i < DWORDS_PER_MAP; i++)
            {
                imap_str += Utility::StrPrintf("%08x ", imap[i]);
                omap_str += Utility::StrPrintf("%08x ", omap[i]);
            }
            InfoPrintf("SPH IMAP after update %s: 0x%s\n",
                m_ModuleName.c_str(), imap_str.c_str());
            InfoPrintf("SPH OMAP after update %s: 0x%s\n",
                m_ModuleName.c_str(), omap_str.c_str());

            // Move 16bit to next, get ready for writing back
            for (int i = DWORDS_PER_MAP-1; i > 0; i--)
            {
                omap[i] = (omap[i] << 16) | (omap[i-1] >> 16);
            }
            omap[0] = omap[0] << 16;

            // Write back
            for (int i = 0; i < 2*DWORDS_PER_MAP - 1; i++)
            {
                // Put imap & omap in one for iteration is not so efficient,
                // but it looks clearer to show the 16bit overlap
                if (i < DWORDS_PER_MAP - 1)
                    map_start[i] = imap[i];
                else if (i == DWORDS_PER_MAP - 1)
                    map_start[i] = (imap[i] & 0xffff) |
                        (omap[0] & 0xffff0000);
                else
                    map_start[i] = omap[i - DWORDS_PER_MAP + 1];
            }
        } // None PS
        else
        {
            // PS
            InfoPrintf("Updating SPH masks for %s: %s\n", m_ModuleName.c_str(),
                params->ParamStr("-sph_force_attr_bits"));

            UINT32 imap[13];
            bool is_two_sided = false;
            if (params->ParamPresent("-sph_is_two_sided") > 0)
            {
                string str_two_sided = params->ParamStr("-sph_is_two_sided");
                if ("true" == str_two_sided)
                    is_two_sided = true;
                else if ("false" == str_two_sided)
                    is_two_sided = false;
                else
                {
                    ErrPrintf("Unrecognized -sph_is_two_sided argument, "
                        "format: -sph_is_two_sided true/false\n");
                    return RC::BAD_PARAMETER;
                }
            }
            for (int i = 0; i < 13; i++)
            {
                imap[i] = map_start[i];
            }

            // Original values in file
            string imap_str;
            for (int i = 0; i < 13; i++)
            {
                imap_str += Utility::StrPrintf("%08x ", imap[i]);
            }
            InfoPrintf("SPH IMAP before update %s: 0x%s\n",
                m_ModuleName.c_str(), imap_str.c_str());

            UINT32 full_popcount = PopCountPsMap(imap);
            if (is_two_sided)
            {
                // For PS IMAP, adhere to special case of (4):
                // count the colors twice for back (bug 431005)
                for (int i = FermiGraphicsStructs::AAM_VERSION_02__COLOR_FRONT_DIFFUSE_RED;
                    i <= FermiGraphicsStructs::AAM_VERSION_02__COLOR_FRONT_SPELWLAR_ALPHA;
                    i++)
                {
                    if (AccessSphPsImapBit(imap, i, READ))
                        full_popcount++;
                }
            }
            // Update imap
            for(int i=0; (i < SPH_ATTRIBUTE_SIZE-8) && (full_popcount < max_attr); i++)
            {
                if (GET_BMAP_BIT(attr_mask, i) &&
                    !AccessSphPsImapBit(imap, i, READ) &&
                    (AamIdMap[FermiGraphicsStructs::PIXEL][i] !=
                     FermiGraphicsStructs::TYPEDEF_SPH_VTG_IDMAPDEFAULT) &&
                    (!is_two_sided ||
                     full_popcount < max_attr - 1 ||
                     i < FermiGraphicsStructs::AAM_VERSION_02__COLOR_FRONT_DIFFUSE_RED ||
                     i > FermiGraphicsStructs::AAM_VERSION_02__COLOR_FRONT_SPELWLAR_ALPHA))
                {
                    UINT32 bits = FermiGraphicsStructs::AamInterpolationTypeMapVersion02[i];
                    if(bits == FermiGraphicsStructs::TYPEDEF_SPH_INTERPOLATION_TYPESET_BY_SPH)
                    {
                        bits = FermiGraphicsStructs::TYPEDEF_SPH_INTERPOLATION_TYPEPERSPECTIVE;
                    }
                    AccessSphPsImapBit(imap, i, WRITE);
                    full_popcount++;
                }
            }

            // The new value
            imap_str = "";
            for (int i = 0; i < 13; i++)
            {
                imap_str += Utility::StrPrintf("%08x ", imap[i]);
            }
            InfoPrintf("SPH IMAP after update %s: 0x%s\n",
                m_ModuleName.c_str(), imap_str.c_str());

            // Write back
            for (int i = 0; i < 13; i++)
            {
                map_start[i] = imap[i];
            }
        } // PS
    }

    bool hasLocalMem = !((fields->ShaderLocalMemoryLowSize  == 0) &&
                         (fields->ShaderLocalMemoryHighSize == 0) &&
                         (fields->ShaderLocalMemoryCrsSize  == 0));
    if (params->ParamPresent("-sph_MrtEnable") > 0)
    {
        fields->MrtEnable = max((UINT32)(fields->MrtEnable),
            params->ParamUnsigned("-sph_MrtEnable"));
        update_table["MrtEnable"] = fields->MrtEnable;
    }
    if (params->ParamPresent("-sph_KillsPixels") > 0)
    {
        fields->KillsPixels = max((UINT32)(fields->KillsPixels),
            params->ParamUnsigned("-sph_KillsPixels"));
        update_table["KillsPixels"] = fields->KillsPixels;
    }
    if (params->ParamPresent("-sph_DoesGlobalStore") > 0)
    {
        fields->DoesGlobalStore = max((UINT32)(fields->DoesGlobalStore),
            params->ParamUnsigned("-sph_DoesGlobalStore"));
        update_table["DoesGlobalStore"] = fields->DoesGlobalStore;
    }
    if (params->ParamPresent("-sph_SassVersion") > 0)
    {
        fields->SassVersion = max((UINT32)(fields->SassVersion),
            params->ParamUnsigned("-sph_SassVersion"));
        update_table["SassVersion"] = fields->SassVersion;
    }
    if (params->ParamPresent("-sph_DoesLoadOrStore") > 0)
    {
        fields->DoesLoadOrStore = max((UINT32)(fields->DoesLoadOrStore),
            params->ParamUnsigned("-sph_DoesLoadOrStore"));
        update_table["DoesLoadOrStore"] = fields->DoesLoadOrStore;
    }
    if (params->ParamPresent("-sph_DoesFp64") > 0)
    {
        fields->DoesFp64 = max((UINT32)(fields->DoesFp64),
            params->ParamUnsigned("-sph_DoesFp64"));
        update_table["DoesFp64"] = fields->DoesFp64;
    }
    if (params->ParamPresent("-sph_StreamOutMask") > 0)
    {
        fields->StreamOutMask |= params->ParamUnsigned("-sph_StreamOutMask");
        update_table["StreamOutMask"] = fields->StreamOutMask;
    }
    if (hasLocalMem && params->ParamPresent("-sph_ShaderLocalMemoryLowSize") > 0)
    {
        fields->ShaderLocalMemoryLowSize = max(
            (UINT32)(fields->ShaderLocalMemoryLowSize),
            params->ParamUnsigned("-sph_ShaderLocalMemoryLowSize"));
        update_table["ShaderLocalMemoryLowSize"] = fields->ShaderLocalMemoryLowSize;
    }
    if (params->ParamPresent("-sph_PerPatchAttributeCount") > 0)
    {
        fields->PerPatchAttributeCount = max(
            (UINT32)(fields->PerPatchAttributeCount),
            params->ParamUnsigned("-sph_PerPatchAttributeCount"));
        update_table["PerPatchAttributeCount"] = fields->PerPatchAttributeCount;
    }
    if (hasLocalMem && params->ParamPresent("-sph_ShaderLocalMemoryHighSize") > 0)
    {
        fields->ShaderLocalMemoryHighSize = max(
            (UINT32)(fields->ShaderLocalMemoryHighSize),
            params->ParamUnsigned("-sph_ShaderLocalMemoryHighSize"));
        update_table["ShaderLocalMemoryHighSize"] = fields->ShaderLocalMemoryHighSize;
    }
    if (params->ParamPresent("-sph_ThreadsPerInputPrimitive") > 0)
    {
        fields->ThreadsPerInputPrimitive = params->ParamUnsigned("-sph_ThreadsPerInputPrimitive");
        update_table["ThreadsPerInputPrimitive"] = fields->ThreadsPerInputPrimitive;
    }
    if (hasLocalMem && params->ParamPresent("-sph_ShaderLocalMemoryCrsSize") > 0)
    {
        fields->ShaderLocalMemoryCrsSize = max(
            (UINT32)(fields->ShaderLocalMemoryCrsSize),
            params->ParamUnsigned("-sph_ShaderLocalMemoryCrsSize"));
        update_table["ShaderLocalMemoryCrsSize"] = fields->ShaderLocalMemoryCrsSize;
    }
    if (params->ParamPresent("-sph_OutputTopology") > 0)
    {
        fields->OutputTopology = params->ParamUnsigned("-sph_OutputTopology");
        update_table["OutputTopology"] = fields->OutputTopology;
    }
    if (params->ParamPresent("-sph_MaxOutputVertexCount") > 0)
    {
        fields->MaxOutputVertexCount = max((UINT32)(fields->MaxOutputVertexCount),
            params->ParamUnsigned("-sph_MaxOutputVertexCount"));
        update_table["MaxOutputVertexCount"] = fields->MaxOutputVertexCount;
    }
    if (params->ParamPresent("-sph_StoreReqStart") > 0)
    {
        fields->StoreReqStart = min((UINT32)(fields->StoreReqStart),
            params->ParamUnsigned("-sph_StoreReqStart"));
        update_table["StoreReqStart"] = fields->StoreReqStart;
    }
    if (params->ParamPresent("-sph_StoreReqEnd") > 0)
    {
        fields->StoreReqEnd = max((UINT32)(fields->StoreReqEnd),
            params->ParamUnsigned("-sph_StoreReqEnd"));
        update_table["StoreReqEnd"] = fields->StoreReqEnd;
    }

    // Update SPH one by one
    typedef GpuTrace::Attr2ValMap Attr2ValMap;
    typedef GpuTrace::File2SphMap File2SphMap;
    const File2SphMap& sphInfo = m_Trace->GetSphInfo();
    File2SphMap::const_iterator fileIt = sphInfo.find(m_ModuleName);
    if (fileIt != sphInfo.end())
    {
        Attr2ValMap setting = (*fileIt).second;
        Attr2ValMap::iterator end = setting.end();
        for (Attr2ValMap::iterator iter = setting.begin(); iter != end; ++iter)
        {
            string arg = (*iter).first;
            UINT32 val = (*iter).second;

           // We can't use setting[arg] here
            if ("-sph_MrtEnable_one" == arg)
            {
                fields->MrtEnable = max((UINT32)(fields->MrtEnable), val);
                update_table["MrtEnable"] = fields->MrtEnable;
            }
            else if ("-sph_KillsPixels_one" == arg)
            {
                fields->KillsPixels = max((UINT32)(fields->KillsPixels), val);
                update_table["KillsPixels"] = fields->KillsPixels;
            }
            else if ("-sph_DoesGlobalStore_one" == arg)
            {
                fields->DoesGlobalStore = max((UINT32)(fields->DoesGlobalStore), val);
                update_table["DoesGlobalStore"] = fields->DoesGlobalStore;
            }
            else if ("-sph_SassVersion_one" == arg)
            {
                fields->SassVersion = max((UINT32)(fields->SassVersion), val);
                update_table["SassVersion"] = fields->SassVersion;
            }
            else if ("-sph_DoesLoadOrStore_one" == arg)
            {
                fields->DoesLoadOrStore = max((UINT32)(fields->DoesLoadOrStore), val);
                update_table["DoesLoadOrStore"] = fields->DoesLoadOrStore;
            }
            else if ("-sph_DoesFp64_one" == arg)
            {
                fields->DoesFp64 = max((UINT32)(fields->DoesFp64), val);
                update_table["DoesFp64"] = fields->DoesFp64;
            }
            else if ("-sph_StreamOutMask_one" == arg)
            {
                fields->StreamOutMask |= val;
                update_table["StreamOutMask"] = fields->StreamOutMask;
            }
            else if ("-sph_ShaderLocalMemoryLowSize_one" == arg && hasLocalMem)
            {
                fields->ShaderLocalMemoryLowSize = max(
                    (UINT32)(fields->ShaderLocalMemoryLowSize), val);
                update_table["ShaderLocalMemoryLowSize"] = fields->ShaderLocalMemoryLowSize;
            }
            else if ("-sph_PerPatchAttributeCount_one" == arg)
            {
                fields->PerPatchAttributeCount = max(
                    (UINT32)(fields->PerPatchAttributeCount), val);
                update_table["PerPatchAttributeCount"] = fields->PerPatchAttributeCount;
            }
            else if ("-sph_ShaderLocalMemoryHighSize_one" == arg && hasLocalMem)
            {
                fields->ShaderLocalMemoryHighSize = max(
                    (UINT32)(fields->ShaderLocalMemoryHighSize), val);
                update_table["ShaderLocalMemoryHighSize"] = fields->ShaderLocalMemoryHighSize;
            }
            else if ("-sph_ThreadsPerInputPrimitive_one" == arg)
            {
                fields->ThreadsPerInputPrimitive = val;
                update_table["ThreadsPerInputPrimitive"] = fields->ThreadsPerInputPrimitive;
            }
            else if ("-sph_ShaderLocalMemoryCrsSize_one" == arg && hasLocalMem)
            {
                fields->ShaderLocalMemoryCrsSize = max(
                    (UINT32)(fields->ShaderLocalMemoryCrsSize), val);
                update_table["ShaderLocalMemoryCrsSize"] = fields->ShaderLocalMemoryCrsSize;
            }
            else if ("-sph_OutputTopology_one" == arg)
            {
                fields->OutputTopology = val;
                update_table["OutputTopology"] = fields->OutputTopology;
            }
            else if ("-sph_MaxOutputVertexCount_one" == arg)
            {
                fields->MaxOutputVertexCount = max(
                    (UINT32)(fields->MaxOutputVertexCount), val);
                update_table["MaxOutputVertexCount"] = fields->MaxOutputVertexCount;
            }
            else if ("-sph_StoreReqStart_one" == arg)
            {
                fields->StoreReqStart = min((UINT32)(fields->StoreReqStart), val);
                update_table["StoreReqStart"] = fields->StoreReqStart;
            }
            else if ("-sph_StoreReqEnd_one" == arg)
            {
                fields->StoreReqEnd = max((UINT32)(fields->StoreReqEnd), val);
                update_table["StoreReqEnd"] = fields->StoreReqEnd;
            }
            else
            {
                MASSERT(!"Unrecognize SPH option\n");
                return RC::BAD_PARAMETER;
            }
        } // end of for
    } // end of if

    // Give some debug info
    if (!update_table.empty())
    {
        string msg("Updating SPH fields for ");
        msg += m_ModuleName + ":";
        UpdateMap::iterator end = update_table.end();
        for (UpdateMap::iterator it = update_table.begin(); it != end; ++it)
        {
            char v[16] = {'\0'};
            sprintf(v, "%d", (UINT32)((*it).second));
            msg += " " + (*it).first + " = " + v;
        }
        InfoPrintf("%s\n", msg.c_str());
    }

    return OK;
}

UINT32 GenericTraceModule::AccessSphPsImapBit
(
    UINT32 *imap,
    UINT32 _bit,
    SPH_PS_IMAP_ACCESS mode
)
{
    MASSERT(_bit < 232);

    UINT32 bit, hi = 0;
    if (_bit < FermiGraphicsStructs::AAM_VERSION_02__GENERIC_ATTRIBUTE_00_X)
        bit = _bit;
    else if (_bit >= FermiGraphicsStructs::AAM_VERSION_02__GENERIC_ATTRIBUTE_00_X &&
             _bit < FermiGraphicsStructs::AAM_VERSION_02__COLOR_BACK_DIFFUSE_RED)
    {
        bit = FermiGraphicsStructs::AAM_VERSION_02__GENERIC_ATTRIBUTE_00_X +
            (_bit - FermiGraphicsStructs::AAM_VERSION_02__GENERIC_ATTRIBUTE_00_X) * 2 + 1; // High bit in 2-bit fields
        hi = 1; // two bits
    }
    else if (_bit >= FermiGraphicsStructs::AAM_VERSION_02__COLOR_BACK_DIFFUSE_RED &&
        _bit < FermiGraphicsStructs::AAM_VERSION_02__CLIP_DISTANCE_0)
        return 0; // COLOR_BACK not present
    else if (_bit >= FermiGraphicsStructs::AAM_VERSION_02__CLIP_DISTANCE_0 &&
             _bit < FermiGraphicsStructs::AAM_VERSION_02__FIXED_FNC_TEXTURE_0_S)
        bit = 304 + (_bit - FermiGraphicsStructs::AAM_VERSION_02__CLIP_DISTANCE_0);
    else if (_bit >= FermiGraphicsStructs::AAM_VERSION_02__FIXED_FNC_TEXTURE_0_S &&
             _bit < FermiGraphicsStructs::AAM_VERSION_02__SYSTEM_VALUE_RESERVED_18)
    {
        bit = 320 + (_bit - FermiGraphicsStructs::AAM_VERSION_02__FIXED_FNC_TEXTURE_0_S) * 2 + 1; // High bit in 2-bit fields
        hi = 1; // two bits
    }
    else
        return 0; // out of range

    if (mode == READ)
    {
        if (hi > 0)
            return ((GET_BMAP_BIT(imap, (bit-1)) & hi) | (GET_BMAP_BIT(imap, bit) << hi));
        else
            return GET_BMAP_BIT(imap, bit);
    }
    else
        return SET_BMAP_BIT(imap, bit);
}

UINT32 GenericTraceModule::PopCountMap(UINT32 *map)
{
    UINT32 popcount = 0;
    for (int i = 0; i < SPH_ATTRIBUTE_SIZE; i++)
    {
        if (GET_BMAP_BIT(map, i))
            popcount++;
    }
    return popcount;
}

UINT32 GenericTraceModule::PopCountPsMap(UINT32 *map)
{
    UINT32 full_popcount = 0;
    for (int i = 0; i < SPH_ATTRIBUTE_SIZE-8; i++)
    {
        if (AccessSphPsImapBit(map, i, READ))
            full_popcount++;
    }
    return full_popcount;
}

// NullifyBeginEndMethodsFermi
//
// Assumes (asserts) that this is a pushbuffer module.
//
// Replaces all instances of <beginMethod> and <endMethod> method writes
// belonging to class <hwClass> throughout all segments in this pushbuffer
// with a <nopMethod> method write, for all begin/ends
// outside of the [ <startCount>, <endCount> ] range.  The counts are zero-
// based and both start/end counts are inclusive.   The data payload of the
// <nopMethod> is the current begin/end count, for aid in debugging or
// wave analysis.
//
// The "Fermi" in the function name indicates this is for a Fermi pushbuffer,
// there are no other fermi dependencies other than pushbuffer format.
//
// Called from fermi's DoSkipComputeGrid() and fermi's DoSkipGeometry() as well.
//
// The presence of multiple instances of the hwClass in multiple channels in
// the trace can confuse the counting.   This function does not address trying
// to count in global trace replay method write order.  Instead we do this:
//   + make sure this pushbuffer's channel contains an instance of hwClass
//       (if not, then just return)
//   + check with the global test structure if this hwClass has already
//        undergone NullifyBeginEnd treatment.  If so, then print a warning and
//        return
//   + otherwise, perform the NullifyBeginEnd transformation for all segments
//        within this pushbuffer, and make a note that this hwClass
//        has undergone NullifyBeginEnd treatment if any Begin/End is nullified
//
// As a result, this function's begin/end nullifying works best when used with
// a trace where there is only one instance of <hwClass> in the trace.
//
// Another simplyfying assumption is that the beginMethod and endMethods only
// occur in single method write pushbuffer entries.   This holds true for the
// current lwstomers of this function (3d begin/end and compute
// beginGrid/endGrid), and makes the code much simpler than having to crack
// open and rewrite multi-dword headers, which would probably involve
// changing the size of the pushbuffer, which is a whole other can o' worms.
// To handle transformation of that complexity, the user would rather be using
// TDBG, which has the ability to unroll multi-dword headers if needed.
//
//

void GenericTraceModule::NullifyBeginEndMethodsFermi
(
    UINT32 hwClass,     // only nullify methods of this class
    UINT32 beginMethod, // "begin" method address to nullify
    UINT32 endMethod,   // "end" method address to nullify
    UINT32 nopMethod,   // "nop" method addr to replace nullified begin/end
    UINT32 startCount,  // 0-based count of first begin/end to keep
    UINT32 endCount     // 0-based count of last begin/end to keep (inclusive)
)
{
    MASSERT(m_FileType == GpuTrace::FT_PUSHBUFFER);

    // subchannel containing hwClass.
    //
    UINT32 hwClassSubch;
    bool hwClassSubchValid = false;

    if (!ChannelHasHwClass(hwClass, &hwClassSubch, &hwClassSubchValid))
        return;

    // if ignoreSubchannel is true, then we don't pay any attention
    // to subchannel when parsing pushbuffer entries below.  If it is
    // false, then we will skip all method write pushbuffer entries to
    // subchannels different from hwClassSubch
    //
    bool ignoreSubchannel = !hwClassSubchValid;

    if (startCount > endCount)
        WarnPrintf("NullifyBeginEndMethodsFermi: startCount(%d) > endCount"
                   "(%d)\n", startCount, endCount);

    // if we've run before the same combination of hwClass
    // and beginMethod then don't do it again
    //
    if (m_Test->GetDidNullifyBeginEnd(hwClass, beginMethod))
    {
        WarnPrintf("NullifyBeginEndMethodsFermi: already ran a nullify pass"
            " for hwClass=0x%08x, beginMethod=0x%08x, not running again\n",
            hwClass, beginMethod);
        return;
    }

    // note if we ever nullify a method.  If we do, when this function returns
    // it will make a note in the Trace3DTest that this class/beginMethod
    // combination has already had a nullify pass.
    //
    bool nullifiedMethod = false;

    UINT32 header, newHeader;
    UINT32 opcode2, methodAddr, numDwords, subch;
    bool isImmediate, isOld;

    UINT32 count = 0;
    for (size_t iSeg = 0; iSeg < m_SendSegments.size(); ++iSeg)
    {
        UINT32 ptr      = m_SendSegments[iSeg].Start;
        UINT32 ptrEnd   = ptr + m_SendSegments[iSeg].Size;
        UINT32 headerPtr;

        while (ptr < ptrEnd)
        {
            // Decode the header:
            header = Get032(ptr, 0);
            headerPtr = ptr;
            ptr += 4 ;

            // extract method write parameters assuming header is a "normal"
            // method write pushbuffer entry
            //
            subch = REF_VAL(LW_FIFO_DMA_METHOD_SUBCHANNEL, header);
            numDwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header);
            methodAddr = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS, header) << 2;

            opcode2 = REF_VAL(LW_FIFO_DMA_SEC_OP, header);

            isImmediate = false;
            isOld = false;

            if (header == LW_FIFO_DMA_NOP)
                continue;
            switch (opcode2)
            {
            case LW_FIFO_DMA_SEC_OP_END_PB_SEGMENT:
                continue;
            case LW_FIFO_DMA_SEC_OP_INC_METHOD:
            case LW_FIFO_DMA_SEC_OP_ONE_INC:
            case LW_FIFO_DMA_SEC_OP_NON_INC_METHOD:
                // "normal" method write pushbuffer entries,
                // we've already extracted the data for this case above
                //
                break;
            case LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD:
                isImmediate = true;
                numDwords = 0;
                break;
            case LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT:
                {
                    UINT32 opcode3 = REF_VAL(LW_FIFO_DMA_TERT_OP, header);
                    switch (opcode3)
                    {
                    case LW_FIFO_DMA_TERT_OP_GRP0_INC_METHOD:
                        isOld = true;
                        numDwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);
                        methodAddr = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS_OLD, header) << 2;
                        break;
                    case LW_FIFO_DMA_TERT_OP_GRP0_SET_SUB_DEV_MASK:
                        continue;
                    default:
                        // LW_FIFO_DMA_TERT_OP_GRP0_STORE_SUB_DEV_MASK:
                        // LW_FIFO_DMA_TERT_OP_GRP0_USE_SUB_DEV_MASK:
                        MASSERT(!"MODS does not support STORE/USE subdevice mask yet!");
                    }
                    break;
                }
                break;
            case LW_FIFO_DMA_SEC_OP_GRP2_USE_TERT:
                isOld = true;
                numDwords = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);
                methodAddr = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS_OLD, header) << 2;
                break;

            default:
                ErrPrintf("Unsupported PB entry: 0x%08x @0x%08x\n", header, ptr-4);
                MASSERT(!"Unknown pushbuffer entry");
            }

            ptr += 4 * numDwords;

            // we have a method write pushbuffer entry.  If it doesn't
            // meet our nullifying criteria (it must be writing just one method
            // and it must be to the right subchannel, and it must be either
            // the begin or the end method address), then skip it.
            //
            if ((numDwords > 1) ||
                (!ignoreSubchannel && (subch != hwClassSubch)) ||
                ((methodAddr != beginMethod) && (methodAddr != endMethod)))
            {
                continue;
            }

            // we have a candidate "begin" or "end" method write, see if
            // we're within the nullifying range or not
            //
            if ((count < startCount) || (count > endCount))
            {
                // nullify this method by replacing its method address
                // with nopMethod
                //
                nullifiedMethod = true;

                if (isOld)
                    newHeader = FLD_SET_DRF_NUM(_FIFO, _DMA,
                                                _METHOD_ADDRESS_OLD,
                                                nopMethod >> 2, header);
                else
                    newHeader = FLD_SET_DRF_NUM(_FIFO, _DMA, _METHOD_ADDRESS,
                                                nopMethod >> 2, header);

                // set the data payload of the nopMethod to be the current
                // count.  data is in the header for immediate, and in the
                // next dword after the header for non-immediate
                //
                if (isImmediate)
                    newHeader = FLD_SET_DRF_NUM(_FIFO, _DMA, _IMMD_DATA,
                                                count, newHeader);
                else
                    Put032(headerPtr + 4, count, 0);

                // write the nullified method header back to the pushbuffer
                //
                Put032(headerPtr, newHeader, 0);
            }

            // count only on encountering the endMethod -- assumes that all
            // begin/ends appear in balanced, non-nested pairs
            //
            if (methodAddr == endMethod)
                ++count;
        }
    }

    if (nullifiedMethod)
        m_Test->SetDidNullifyBeginEnd(hwClass, beginMethod);
}

RC TraceChannel::SetupFermiVabBuffer(LWGpuSubChannel* pSubch)
{
    RC rc;
    MdiagSurf &vabBuffer = GetVABBuffer();

    if (!vabBuffer.GetMemHandle())
    {
        vabBuffer.SetProtect(Memory::ReadWrite);
        if (params->ParamPresent("-vab_size") > 0)
        {
            switch (params->ParamUnsigned("-vab_size"))
            {
            case 0:
                ErrPrintf("32K VAB is not supported\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
                break;
            case 1:
                vabBuffer.SetWidth(64*1024);
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_C,
                    DRF_DEF(9097, _SET_VAB_MEMORY_AREA_C, _SIZE, _BYTES_64K)));
                break;
            case 2:
                vabBuffer.SetWidth(128*1024);
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_C,
                    DRF_DEF(9097, _SET_VAB_MEMORY_AREA_C, _SIZE, _BYTES_128K)));
                break;
            case 3:
                vabBuffer.SetWidth(256*1024);
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_C,
                    DRF_DEF(9097, _SET_VAB_MEMORY_AREA_C, _SIZE, _BYTES_256K)));
                break;
            case 4:
                // in this case we're supposed to skip this function entirely
                //
                ErrPrintf("SetupFermiVabBuffer: internal error: shouldn't get"
                    "-vab_none case in this function\n");
                return RC::SOFTWARE_ERROR;
                break;
            case 5:
                // When running in L2 Cache only mode for FBIST, FB space is a scarce resource because it
                // is limited to the size of L2 cache.  In order to conserve space in this specialized
                // only allocate 4K for the VAB buffer.
                // NOTE: this is a special case only and the must still set VAB_MEMORY_AREA_C to 64K
                vabBuffer.SetWidth(4*1024);
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_C,
                    DRF_DEF(9097, _SET_VAB_MEMORY_AREA_C, _SIZE, _BYTES_64K)));
                InfoPrintf("-vab_cache_only_4k specified, setting VAB size to 4K for L2 cache only mode\n");
                break;

            case 6:
                // When running in L2 Cache only mode for FBIST, FB space is a scarce resource because it
                // is limited to the size of L2 cache.  In order to conserve space in this specialized
                // only allocate 8K for the VAB buffer -- the minimum size supported by Fermi & Kepler
                // This was originally 4K, but that was found to be insufficient (bug #640168).
                // NOTE: this is a special case only and  must still set VAB_MEMORY_AREA_C to 64K
                vabBuffer.SetWidth(8*1024);
                CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_C,
                    DRF_DEF(9097, _SET_VAB_MEMORY_AREA_C, _SIZE, _BYTES_64K)));
                InfoPrintf("-vab_cache_min specified, setting VAB size to 8K for L2 cache only mode\n");
                break;

            default:
                ErrPrintf("Wrong VAB size\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
        else
        {
            // Set default VAB size
            vabBuffer.SetWidth(64*1024);
            CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_C,
                DRF_DEF(9097, _SET_VAB_MEMORY_AREA_C, _SIZE, _BYTES_64K)));
        }
        vabBuffer.SetHeight(1);
        vabBuffer.SetColorFormat(ColorUtils::Y8);
        vabBuffer.SetForceSizeAlloc(true);

        SetPteKind(vabBuffer, GetVABPteKindName(),
            m_Test->GetBoundGpuDevice());

        // VAB buffer must be 256B aligned, but don't override
        // command-line argument.
        if (vabBuffer.GetAlignment() == 0)
        {
            vabBuffer.SetAlignment(256);
        }

        vabBuffer.Alloc(m_Test->GetBoundGpuDevice(), GetLwRmPtr());
        PrintDmaBufferParams(vabBuffer);
        m_Test->m_BuffInfo.SetDmaBuff("VAB", vabBuffer, 0, 0 );
    }

    const UINT64 VABOffset = vabBuffer.GetCtxDmaOffsetGpu()
        + vabBuffer.GetExtraAllocSize();
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_A,
        DRF_NUM(9097, _SET_VAB_MEMORY_AREA_A, _OFFSET_UPPER, LwU64_HI32(VABOffset))));
    CHECK_RC(pSubch->MethodWriteRC(LW9097_SET_VAB_MEMORY_AREA_B,
        DRF_NUM(9097, _SET_VAB_MEMORY_AREA_B, _OFFSET_LOWER, LwU64_LO32(VABOffset))));

    return rc;
}

UINT32 GenericTraceModule::GetFermiCbsize(UINT32 gpuSubdevIdx) const
{
    RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, m_Test->GetLwRmPtr(), m_Test->GetSmcEngine());

    if (regHal.IsSupported(MODS_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC))
    {
        return regHal.Read32(MODS_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_CBSIZE);
    }
    return 0;
}
