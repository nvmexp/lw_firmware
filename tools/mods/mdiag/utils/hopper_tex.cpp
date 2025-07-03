/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "hopper_tex.h"
#include "mdiag/sysspec.h"
#include "class/clcb97tex.h"
#include "lwos.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"

TextureHeader* HopperTextureHeader::CreateTextureHeader
(
    const TextureHeaderState* state,
    GpuSubdevice* pGpuSubdev
)
{
    TextureHeader *header;
    HopperTextureHeaderState hopperState(state);

    UINT32 headerVersion = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _HEADER_VERSION,
        hopperState.m_Data);

    switch (headerVersion)
    {
        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_PITCH_COLOR_KEY_V2:
            header = new HopperPitchColorkeyTextureHeader(state, pGpuSubdev);
            break;

        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_PITCH_V2:
            header = new HopperPitchTextureHeader(state, pGpuSubdev);
            break;

        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_BLOCKLINEAR_V2:
            header = new HopperBlocklinearTextureHeader(state, pGpuSubdev);
            break;

        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_BLOCKLINEAR_COLOR_KEY_V2:
            header = new HopperBlocklinearColorkeyTextureHeader(state, pGpuSubdev);
            break;

        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_ONE_D_RAW_TYPED:
            header = new HopperOneDRawTypedTextureHeader(state, pGpuSubdev);
            break;

        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_ONE_D_STRUCT_BUF:
            header = new HopperOneDStructBufTextureHeader(state, pGpuSubdev);
            break;

        default:
            MASSERT(!"Unexpected Hopper texture header version");
            header = nullptr;
            break;
    }

    return header;
}

HopperTextureHeader::HopperTextureHeader
(
    const TextureHeaderState* state,
    GpuSubdevice* gpuSubdevice
)
:   TextureHeader(gpuSubdevice)
{
    MASSERT(state);
    MASSERT(gpuSubdevice);
    m_State = new HopperTextureHeaderState(state);
}

HopperTextureHeader::~HopperTextureHeader()
{
    if (m_State != 0)
    {
        delete m_State;
    }
}

UINT32 HopperTextureHeader::GetHeapAttr() const
{
    UINT32 attr = GetAttrDepth();

    if (IsBlocklinear())
    {
        attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);
    }
    else
    {
        attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH);
    }

    attr |= GetMultiSampleAttr();

    return attr;
}

bool HopperTextureHeader::AllSourceInB() const
{
    return ((DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, m_State->m_Data) ==
            LWCB97_TEXHEAD_V2_BL_X_SOURCE_IN_B) &&
        (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, m_State->m_Data) ==
            LWCB97_TEXHEAD_V2_BL_Y_SOURCE_IN_B) &&
        (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, m_State->m_Data) ==
            LWCB97_TEXHEAD_V2_BL_Z_SOURCE_IN_B) &&
        (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, m_State->m_Data) ==
            LWCB97_TEXHEAD_V2_BL_W_SOURCE_IN_B));
}

bool HopperTextureHeader::Compressible() const
{
    switch (GetHeapType())
    {
        case LWOS32_TYPE_DEPTH:
            return true;

        case LWOS32_TYPE_IMAGE:
            return IsImageCompressible();

        default:
            MASSERT(!"Unexpected heap type");
            break;
    }

    return false;
}

RC HopperTextureHeader::RelocTexture
(
    MdiagSurf *surface,
    UINT64 offset,
    LW50Raster* raster,
    TextureMode textureMode,
    GpuDevice *gpuDevice,
    bool useOffset,
    bool useSurfaceAttr,
    bool optimalPromotion,
    bool centerSpoof,
    TextureHeaderState *newState
)
{
    RC rc;

    // Make sure the surface layout matches the texture header layout.
    if (IsBlocklinear())
    {
        if (surface->GetLayout() != Surface2D::BlockLinear)
        {
            ErrPrintf("texture header uses blocklinear layout, but the RELOC_D surface is not blocklinear.\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        if (surface->GetLayout() != Surface2D::Pitch)
        {
            ErrPrintf("texture header uses pitch layout, but the RELOC_D surface is not pitch.\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    raster->SetHopperTextureHeaderFormat(m_State->m_Data,
        GetAAModeFromSurface(surface), textureMode);

    if (useOffset)
    {
        RelocAddress(offset);
    }

    if (!useSurfaceAttr)
    {
        return OK;
    }

    RelocLayout(surface, gpuDevice);

    UINT32 isSrgb = raster->IsSrgb();
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_BL, _S_R_G_B_COLWERSION, isSrgb, m_State->m_Data);

    CHECK_RC(RelocAAMode(surface, centerSpoof));

    if (optimalPromotion)
    {
        CHECK_RC(ModifySectorPromotion(m_State, GetOptimalSectorPromotion()));
    }

    newState->DWORD0 = m_State->m_Data[0];
    newState->DWORD1 = m_State->m_Data[1];
    newState->DWORD2 = m_State->m_Data[2];
    newState->DWORD3 = m_State->m_Data[3];
    newState->DWORD4 = m_State->m_Data[4];
    newState->DWORD5 = m_State->m_Data[5];
    newState->DWORD6 = m_State->m_Data[6];
    newState->DWORD7 = m_State->m_Data[7];

    return OK;
}

RC HopperTextureHeader::ModifySectorPromotion
(
    HopperTextureHeaderState* state,
    L1_PROMOTION newPromotion
)
{
    RC rc;
    UINT32 headerVersion = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _HEADER_VERSION,
        state->m_Data);

    switch (headerVersion)
    {
        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_PITCH_COLOR_KEY_V2:
        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_PITCH_V2:
        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_BLOCKLINEAR_V2:
        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_BLOCKLINEAR_COLOR_KEY_V2:
            CHECK_RC(HopperTwoDTextureImpl::ModifySectorPromotion(state, newPromotion));
            break;

        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_ONE_D_RAW_TYPED:
        case LWCB97_TEXHEAD_V2_BL_HEADER_VERSION_SELECT_ONE_D_STRUCT_BUF:
            CHECK_RC(HopperOneDTextureImpl::ModifySectorPromotion(state, newPromotion));
            break;

        default:
            MASSERT(!"Unexpected Hopper texture header version");
            rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

UINT32 HopperTwoDTextureImpl::GetAttrDepth() const
{
    UINT32 attr = 0;
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32_A32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT23:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT45:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC7U:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EACX2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _128);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16_B16_A16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_B24G8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EAC:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R1:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A2B10G10R10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8R24:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G24R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_E5B9G9R9_SHAREDEXP:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BF10GF11RF11:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B8G8R8G8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A4B4G4R4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A5B5G5R1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A1B5G5R5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B5G6R5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B6G5R5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _16);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G4R4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Y8_VIDEO:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _8);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z16:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _16);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z16);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_S8Z24:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _S8Z24);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z24S8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z24S8);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_X8Z24:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_X8B8G8R8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _COLOR_PACKING, _X8R8G8B8);
            break;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32_X24S8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
            break;

        default:
            ErrPrintf("Unknown TwoD COMPONENTS value %02X\n", format);
            MASSERT(0);
    }

    return attr;
}

bool HopperTwoDTextureImpl::IsImageCompressible() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32_A32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16_B16_A16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_B24G8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A2B10G10R10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8R24:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G24R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_E5B9G9R9_SHAREDEXP:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BF10GF11RF11:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B8G8R8G8:
            return true;
    }

    return false;
}

UINT32 HopperTwoDTextureImpl::GetHeapType() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_S8Z24:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z24S8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_X8Z24:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32_X24S8:
            return LWOS32_TYPE_DEPTH;

        default:
            return LWOS32_TYPE_IMAGE;
    }

    return LWOS32_TYPE_IMAGE;
}

UINT32 HopperTwoDTextureImpl::TexelSize() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32_A32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT23:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT45:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC7U:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EACX2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            return 16;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16_B16_A16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_B24G8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32_X24S8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EAC:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R1:
            return 8;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_X8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A2B10G10R10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8R24:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G24R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_S8Z24:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_X8Z24:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z24S8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_E5B9G9R9_SHAREDEXP:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BF10GF11RF11:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B8G8R8G8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32:
            return 4;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A4B4G4R4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A5B5G5R1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A1B5G5R5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B5G6R5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B6G5R5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16:
            return 2;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G4R4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Y8_VIDEO:
            return 1;

        default:
            ErrPrintf("Unknown texel format: 0x%x\n", format);
            MASSERT(0);
    }

    return 0;
}

UINT32 HopperTwoDTextureImpl::TexelScale() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32:
            return 3;
    }

    return 1;
}

UINT32 HopperTwoDTextureImpl::TextureWidthScale() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            return 12;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
            return 10;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
            return 8;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
            return 6;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
            return 5;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT23:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT45:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC7U:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EAC:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EACX2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
            return 4;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B8G8R8G8:
            return 2;
    }

    return 1;
}

UINT32 HopperTwoDTextureImpl::TextureHeightScale() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            return 12;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
            return 10;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
            return 8;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
            return 6;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
            return 5;

        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT23:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT45:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN1:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC7U:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EAC:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EACX2:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
            return 4;
    }

    return 1;
}

UINT32 HopperTwoDTextureImpl::DimSize() const
{
    switch (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_LWBEMAP:
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 6;
    }

    return 1;
}

ColorUtils::Format HopperTwoDTextureImpl::GetColorFormat() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32_A32:
        return ColorUtils::R32_G32_B32_A32;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32_B32:
        return ColorUtils::R32_G32_B32;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16_B16_A16:
        return ColorUtils::R16_G16_B16_A16;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_G32:
        return ColorUtils::R32_G32;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32_B24G8:
        return ColorUtils::R32_B24G8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_X8B8G8R8:
        return ColorUtils::X8B8G8R8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A8B8G8R8:
        return ColorUtils::A8B8G8R8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A2B10G10R10:
        return ColorUtils::A2B10G10R10;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16_G16:
        return ColorUtils::R16_G16;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8R24:
        return ColorUtils::G8R24;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G24R8:
        return ColorUtils::G24R8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R32:
        return ColorUtils::R32;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A4B4G4R4:
        return ColorUtils::A4B4G4R4;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A5B5G5R1:
        return ColorUtils::A5B5G5R1;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_A1B5G5R5:
        return ColorUtils::A1B5G5R5;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B5G6R5:
        return ColorUtils::B5G6R5;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B6G5R5:
        return ColorUtils::B6G5R5;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8R8:
        return ColorUtils::G8R8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R16:
        return ColorUtils::R16;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Y8_VIDEO:
        return ColorUtils::Y8_VIDEO;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R8:
        return ColorUtils::R8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G4R4:
        return ColorUtils::G4R4;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_R1:
        return ColorUtils::R1;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_E5B9G9R9_SHAREDEXP:
        return ColorUtils::E5B9G9R9_SHAREDEXP;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BF10GF11RF11:
        return ColorUtils::BF10GF11RF11;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_G8B8G8R8:
        return ColorUtils::G8B8G8R8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_B8G8R8G8:
        return ColorUtils::B8G8R8G8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT1:
        return ColorUtils::DXT1;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT23:
        return ColorUtils::DXT23;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXT45:
        return ColorUtils::DXT45;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN1:
        return ColorUtils::DXN1;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_DXN2:
        return ColorUtils::DXN2;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_SF16:
        return ColorUtils::BC6H_SF16;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC6H_UF16:
        return ColorUtils::BC6H_UF16;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_BC7U:
        return ColorUtils::BC7U;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB:
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ETC2_RGBA:
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EAC:
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_EACX2:
        // FIXME: Need to add above texture color formats in ColorUtils.
        return ColorUtils::LWFMT_NONE;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z24S8:
        return ColorUtils::Z24S8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_X8Z24:
        return ColorUtils::X8Z24;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_S8Z24:
        return ColorUtils::S8Z24;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32:
        return ColorUtils::ZF32;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_ZF32_X24S8:
        return ColorUtils::ZF32_X24S8;
    case LWCB97_TEXHEAD_V2_BL_COMPONENTS_SIZES_Z16:
        return ColorUtils::Z16;
    default:
        MASSERT(!"Unrecognized color format");
    }
    return ColorUtils::LWFMT_NONE;
}

UINT32 HopperTwoDTextureImpl::TextureWidth() const
{
    UINT32 width = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _WIDTH_MINUS_ONE, m_State->m_Data) + 1;
    width *= GetMultiSampleWidthScale();

    return width;
}

UINT32 HopperTwoDTextureImpl::TextureHeight() const
{
    UINT32 height = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _HEIGHT_MINUS_ONE, m_State->m_Data) + 1;
    height *= GetMultiSampleHeightScale();

    return height;
}

UINT32 HopperTwoDTextureImpl::MaxMipLevel() const
{
    return DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _MAX_MIP_LEVEL, m_State->m_Data);
}

RC HopperTwoDTextureImpl::ModifySectorPromotion
(
    HopperTextureHeaderState* state,
    L1_PROMOTION newPromotion
)
{
    switch (newPromotion)
    {
        case L1_PROMOTION_NONE:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _SECTOR_PROMOTION,
                _NO_PROMOTION, state->m_Data);
            break;

        case L1_PROMOTION_2V:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _SECTOR_PROMOTION,
                _PROMOTE_TO_2_V, state->m_Data);
            break;

        case L1_PROMOTION_2H:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _SECTOR_PROMOTION,
                _PROMOTE_TO_2_H, state->m_Data);
            break;

        case L1_PROMOTION_ALL:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _SECTOR_PROMOTION,
                _PROMOTE_TO_4, state->m_Data);
            break;

        default:
            ErrPrintf("Unrecognized L1 promotion: %u\n", newPromotion);
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

UINT32 HopperOneDTextureImpl::GetAttrDepth() const
{
    UINT32 attr = 0;
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_1DSB, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R32_G32_B32_A32:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R32_G32_B32:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _128);
            break;

        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R16_G16_B16_A16:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R32_G32:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R32_B24G8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
            break;

        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_A8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_A8B8G8R8_SRGB:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_A2B10G10R10:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R16_G16:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_G8R24:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_G24R8:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R32:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_E5B9G9R9_SHAREDEXP:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_BF10GF11RF11:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_B8G8R8G8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            break;

        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_A4B4G4R4:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_A5B5G5R1:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_A1B5G5R5:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_B5G6R5:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_B6G5R5:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_G8R8:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_G8R8_SRGB:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R16:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _16);
            break;

        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R8:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_R8_SRGB:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_G4R4:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_Y8_VIDEO:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _8);
            break;

        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_X8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DSB_COMPONENTS_SIZES_1D_X8B8G8R8_SRGB:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _COLOR_PACKING, _X8R8G8B8);
            break;

        default:
            ErrPrintf("Unknown OneD COMPONENTS value %02X\n", format);
            MASSERT(0);
    }

    return attr;
}

bool HopperOneDTextureImpl::IsImageCompressible() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_1DRT, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32_B32_A32:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16_G16_B16_A16:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_B24G8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A8B8G8R8_SRGB:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A2B10G10R10:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16_G16:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8R24:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G24R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_E5B9G9R9_SHAREDEXP:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_BF10GF11RF11:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B8G8R8G8:
            return true;
    }

    return false;
}

UINT32 HopperOneDTextureImpl::TexelSize() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_1DRT, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32_B32_A32:
            return 16;

        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16_G16_B16_A16:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_B24G8:
            return 8;

        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_X8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A2B10G10R10:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16_G16:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8R24:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G24R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_E5B9G9R9_SHAREDEXP:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_BF10GF11RF11:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B8G8R8G8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32_B32:
            return 4;

        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A4B4G4R4:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A5B5G5R1:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A1B5G5R5:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B5G6R5:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B6G5R5:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16:
            return 2;

        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G4R4:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_Y8_VIDEO:
            return 1;

        default:
            ErrPrintf("Unknown texel format: 0x%x\n", format);
            MASSERT(0);
    }

    return 0;
}

UINT32 HopperOneDTextureImpl::TexelScale() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_1DRT, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32_B32:
            return 3;
    }

    return 1;
}

UINT32 HopperOneDTextureImpl::TextureWidthScale() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_1DRT, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8B8G8R8:
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B8G8R8G8:
            return 2;
    }

    return 1;
}

ColorUtils::Format HopperOneDTextureImpl::GetColorFormat() const
{
    UINT32 format = DRF_VAL_MW(CB97, _TEXHEAD_V2_1DRT, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32_B32_A32:
            return ColorUtils::R32_G32_B32_A32;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32_B32:
            return ColorUtils::R32_G32_B32;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16_G16_B16_A16:
            return ColorUtils::R16_G16_B16_A16;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_G32:
            return ColorUtils::R32_G32;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32_B24G8:
            return ColorUtils::R32_B24G8;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_X8B8G8R8:
            return ColorUtils::X8B8G8R8;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A8B8G8R8:
            return ColorUtils::A8B8G8R8;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A2B10G10R10:
            return ColorUtils::A2B10G10R10;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16_G16:
            return ColorUtils::R16_G16;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8R24:
            return ColorUtils::G8R24;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G24R8:
            return ColorUtils::G24R8;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R32:
            return ColorUtils::R32;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A4B4G4R4:
            return ColorUtils::A4B4G4R4;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A5B5G5R1:
            return ColorUtils::A5B5G5R1;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_A1B5G5R5:
            return ColorUtils::A1B5G5R5;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B5G6R5:
            return ColorUtils::B5G6R5;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B6G5R5:
            return ColorUtils::B6G5R5;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8R8:
            return ColorUtils::G8R8;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R16:
            return ColorUtils::R16;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_Y8_VIDEO:
            return ColorUtils::Y8_VIDEO;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_R8:
            return ColorUtils::R8;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G4R4:
            return ColorUtils::G4R4;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_E5B9G9R9_SHAREDEXP:
            return ColorUtils::E5B9G9R9_SHAREDEXP;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_BF10GF11RF11:
            return ColorUtils::BF10GF11RF11;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_G8B8G8R8:
            return ColorUtils::G8B8G8R8;
        case LWCB97_TEXHEAD_V2_1DRT_COMPONENTS_SIZES_1D_B8G8R8G8:
            return ColorUtils::B8G8R8G8;
        default:
            MASSERT(!"Unrecognized color format");
    }
    return ColorUtils::LWFMT_NONE;
}

UINT32 HopperOneDTextureImpl::TextureWidth() const
{
    return DRF_VAL_MW(CB97, _TEXHEAD_V2_1DRT, _WIDTH_MINUS_ONE, m_State->m_Data) + 1;
}

void HopperOneDTextureImpl::RelocAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_1DRT, _ADDRESS_BITS31TO0, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_1DRT, _ADDRESS_BITS63TO32, addressMsb, m_State->m_Data);
}

void HopperOneDTextureImpl::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    UINT32 width = surface->GetWidth() - 1;
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_1DRT, _WIDTH_MINUS_ONE, width, m_State->m_Data);
}

RC HopperOneDTextureImpl::ModifySectorPromotion
(
    HopperTextureHeaderState* state,
    L1_PROMOTION newPromotion
)
{
    switch (newPromotion)
    {
        case L1_PROMOTION_NONE:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_1DRT, _SECTOR_PROMOTION,
                _NO_PROMOTION, state->m_Data);
            break;

        case L1_PROMOTION_2V:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_1DRT, _SECTOR_PROMOTION,
                _PROMOTE_TO_2_V, state->m_Data);
            break;

        case L1_PROMOTION_2H:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_1DRT, _SECTOR_PROMOTION,
                _PROMOTE_TO_2_H, state->m_Data);
            break;

        case L1_PROMOTION_ALL:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_1DRT, _SECTOR_PROMOTION,
                _PROMOTE_TO_4, state->m_Data);
            break;

        default:
            ErrPrintf("Unrecognized L1 promotion: %u\n", newPromotion);
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

void HopperBlocklinearTextureImpl::FixBlockSize() const
{
    UINT32 block_size[3] = {1, 1, 1};

    BlockLinear::OptimalBlockSize3D(
        block_size,
        TexelSize(),
        TextureHeight(),
        TextureDepth(),
        9,
        Utility::CountBits(m_pGpuSubdevice->GetFsImpl()->FbMask()),
        m_pGpuSubdevice->GetParentDevice());

    switch (block_size[0])
    {
        case 1:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_WIDTH, _ONE_GOB, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block width");
    }

    switch (block_size[1])
    {
        case 1:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _ONE_GOB, m_State->m_Data);
            break;
        case 2:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _TWO_GOBS, m_State->m_Data);
            break;
        case 4:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _FOUR_GOBS, m_State->m_Data);
            break;
        case 8:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _EIGHT_GOBS, m_State->m_Data);
            break;
        case 16:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _SIXTEEN_GOBS, m_State->m_Data);
            break;
        case 32:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _THIRTYTWO_GOBS, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block height");
    }

    switch (block_size[2])
    {
        case 1:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _ONE_GOB, m_State->m_Data);
            break;
        case 2:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _TWO_GOBS, m_State->m_Data);
            break;
        case 4:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _FOUR_GOBS, m_State->m_Data);
            break;
        case 8:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _EIGHT_GOBS, m_State->m_Data);
            break;
        case 16:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _SIXTEEN_GOBS, m_State->m_Data);
            break;
        case 32:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _THIRTYTWO_GOBS, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block depth");
    }
}

UINT32 HopperBlocklinearTextureImpl::BlockWidth() const
{
    switch (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_WIDTH, m_State->m_Data))
    {
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_WIDTH_ONE_GOB:
            return 1;
        default:
            MASSERT(!"Unexpected block width\n");
    }

    return 0;
}

UINT32 HopperBlocklinearTextureImpl::BlockHeight() const
{
    switch (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, m_State->m_Data))
    {
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_HEIGHT_ONE_GOB:
            return 1;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_HEIGHT_TWO_GOBS:
            return 2;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_HEIGHT_FOUR_GOBS:
            return 4;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_HEIGHT_EIGHT_GOBS:
            return 8;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_HEIGHT_SIXTEEN_GOBS:
            return 16;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_HEIGHT_THIRTYTWO_GOBS:
            return 32;
        default:
            MASSERT(!"Unexpected block height\n");
    }

    return 0;
}

UINT32 HopperBlocklinearTextureImpl::BlockDepth() const
{
    switch (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, m_State->m_Data))
    {
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_DEPTH_ONE_GOB:
            return 1;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_DEPTH_TWO_GOBS:
            return 2;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_DEPTH_FOUR_GOBS:
            return 4;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_DEPTH_EIGHT_GOBS:
            return 8;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_DEPTH_SIXTEEN_GOBS:
            return 16;
        case LWCB97_TEXHEAD_V2_BL_GOBS_PER_BLOCK_DEPTH_THIRTYTWO_GOBS:
            return 32;
        default:
            MASSERT(!"Unexpected block depth\n");
    }

    return 0;
}

UINT32 HopperBlocklinearTextureImpl::TextureDepth() const
{
    // The depth field is used as the array size for arrays, so return 1 here.
    switch (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 1;
    }

    return DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 HopperBlocklinearTextureImpl::ArraySize() const
{
    // The depth field is used as the array size for arrays.
    switch (DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWCB97_TEXHEAD_V2_BL_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
    }

    return 1;
}

void HopperBlocklinearTextureImpl::RelocAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    // Make sure the address is aligned properly.
    MASSERT((addressLsb & 0x1FF) == 0);
    addressLsb >>= 9;

    // Make sure the address doesn't exceed the maximum number of bits.
    MASSERT((addressMsb & 0xFE000000) == 0);

    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_BL, _ADDRESS_BITS31TO9, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_BL, _ADDRESS_BITS56TO32, addressMsb, m_State->m_Data);
}

void HopperBlocklinearTextureImpl::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    switch (surface->GetLogBlockWidth())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_WIDTH, _ONE_GOB, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block width");
    }

    switch (surface->GetLogBlockHeight())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _ONE_GOB, m_State->m_Data);
            break;

        case 1:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _TWO_GOBS, m_State->m_Data);
            break;

        case 2:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _FOUR_GOBS, m_State->m_Data);
            break;

        case 3:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _EIGHT_GOBS, m_State->m_Data);
            break;

        case 4:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _SIXTEEN_GOBS, m_State->m_Data);
            break;

        case 5:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_HEIGHT, _THIRTYTWO_GOBS, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block height");
    }

    switch (surface->GetLogBlockDepth())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _ONE_GOB, m_State->m_Data);
            break;

        case 1:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _TWO_GOBS, m_State->m_Data);
            break;

        case 2:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _FOUR_GOBS, m_State->m_Data);
            break;

        case 3:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _EIGHT_GOBS, m_State->m_Data);
            break;

        case 4:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _SIXTEEN_GOBS, m_State->m_Data);
            break;

        case 5:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _GOBS_PER_BLOCK_DEPTH, _THIRTYTWO_GOBS, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block depth");
    }

    BlockShrinkingSanityCheck(surface, gpuDevice);

    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_BL, _WIDTH_MINUS_ONE, surface->GetWidth() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_BL, _HEIGHT_MINUS_ONE, surface->GetHeight() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_BL, _DEPTH_MINUS_ONE, surface->GetDepth() - 1, m_State->m_Data);
}

UINT32 HopperPitchTextureImpl::TexturePitch() const
{
    return DRF_VAL_MW(CB97, _TEXHEAD_PITCH, _PITCH_BITS20TO5, m_State->m_Data) << 5;
}

void HopperPitchTextureImpl::RelocAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    // Make sure the address is aligned properly.
    MASSERT((addressLsb & 0x1F) == 0);
    addressLsb >>= 5;

    // Make sure the address doesn't exceed the maximum number of bits.
    MASSERT((addressMsb & 0xFE000000) == 0);

    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_PITCH, _ADDRESS_BITS31TO5, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_PITCH, _ADDRESS_BITS56TO32, addressMsb, m_State->m_Data);
}

void HopperPitchTextureImpl::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    UINT32 width = surface->GetWidth() - 1;
    UINT32 height = surface->GetHeight() - 1;

    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_PITCH, _WIDTH_MINUS_ONE, width, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_PITCH, _HEIGHT_MINUS_ONE, height, m_State->m_Data);

    UINT32 pitch = surface->GetPitch();
    
    // Make sure the pitch is aligned correctly;
    MASSERT((pitch & 0x1F) == 0);
    pitch >>= 5;

    FLD_SET_DRF_NUM_MW(CB97, _TEXHEAD_V2_PITCH, _PITCH_BITS21TO5, pitch, m_State->m_Data);
}

UINT32 HopperMultiSampleTextureImpl::GetMultiSampleAttr() const
{
    UINT32 attr = 0;
    UINT32 multiSampleCount = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_1X1:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _2);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X4:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _16);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
            break;

        default:
            MASSERT(!"Unknown AA mode");
    }

    return attr;
}

UINT32 HopperMultiSampleTextureImpl::GetMultiSampleWidthScale() const
{
    UINT32 scale;
    UINT32 multiSampleCount = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_1X1:
            scale = 1;
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
            scale = 2;
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X4:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            scale = 4;
            break;

        default:
            MASSERT(!"Unknown AA mode");
            scale = 0;
    }

    return scale;
}

UINT32 HopperMultiSampleTextureImpl::GetMultiSampleHeightScale() const
{
    UINT32 scale;
    UINT32 multiSampleCount = DRF_VAL_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_1X1:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
            scale = 1;
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            scale = 2;
            break;

        case LWCB97_TEXHEAD_V2_BL_MULTI_SAMPLE_COUNT_MODE_4X4:
            scale = 4;
            break;

        default:
            MASSERT(!"Unknown AA mode");
            scale = 0;
    }

    return scale;
}

RC HopperMultiSampleTextureImpl::RelocAAMode(MdiagSurf* surface, bool centerSpoof)
{
    // Center spoof is not supported in Hopper texture headers.
    MASSERT(!centerSpoof);

    switch (GetAAModeFromSurface(surface))
    {
        case AAMODE_1X1:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_1X1, m_State->m_Data);
            break;

        case AAMODE_2X1:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_2X1, m_State->m_Data);
            break;

        case AAMODE_2X2:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_2X2, m_State->m_Data);
            break;

        case AAMODE_4X2:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2, m_State->m_Data);
            break;

        case AAMODE_4X4:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_4X4, m_State->m_Data);
            break;

        case AAMODE_2X1_D3D:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_2X1_D3D, m_State->m_Data);
            break;

        case AAMODE_4X2_D3D:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_D3D, m_State->m_Data);
            break;

        case AAMODE_2X2_VC_4:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_2X2_VC_4, m_State->m_Data);
            break;

        case AAMODE_2X2_VC_12:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_2X2_VC_12, m_State->m_Data);
            break;

        case AAMODE_4X2_VC_8:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_VC_8, m_State->m_Data);
            break;

        case AAMODE_4X2_VC_24:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_VC_24, m_State->m_Data);
            break;

        default:
            ErrPrintf("Unknown AA Mode %u\n", GetAAModeFromSurface(surface));
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC HopperSingleSampleTextureImpl::RelocAAMode(MdiagSurf* surface, bool centerSpoof)
{
    if (AAMODE_1X1 != GetAAModeFromSurface(surface))
    {
        ErrPrintf("Texture header does not support the AA mode of the RELOC_D surface.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}
