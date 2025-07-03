/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "kepler_tex.h"
#include "mdiag/sysspec.h"
#include "class/cl9097tex.h" // fermi_a
#include "class/cl9297tex.h" // fermi_c
#include "class/cla097tex.h" // kepler_a
#include "class/cla297tex.h" // kepler_c
#include "lwos.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/floorsweepimpl.h"

KeplerTextureHeader::KeplerTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice)
: TextureHeader(gpuSubdevice), m_state(0)
{
    MASSERT(state);
    m_state = new TextureHeaderState(*state);
}

KeplerTextureHeader::~KeplerTextureHeader()
{
    if (m_state != 0)
    {
        delete m_state;
    }
}

UINT32 KeplerTextureHeader::GetHeapAttr() const
{
    UINT32 attr = 0;

    if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
    {
        switch (DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
        {
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32_A32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT23:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT45:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN2:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_SF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_UF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC7U:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGBA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EACX2:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _128);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R16_G16_B16_A16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_B24G8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV8R24V:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB_PTA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EAC:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R1:
                if (1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
                {
                    // Fermi does not support this format, treat it like A8R8G8B8
                    attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
                }
                else
                {
                    attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
                }
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A8B8G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A2B10G10R10:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R16_G16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G8R24:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G24R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_E5B9G9R9_SHAREDEXP:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BF10GF11RF11:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G8B8G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_B8G8R8G8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV8R24V:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A4B4G4R4:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A5B5G5R1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A1B5G5R5:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_B5G6R5:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_B6G5R5:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R16:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _16);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G4R4:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Y8_VIDEO:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _8);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Z16:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _16);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z16);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_S8Z24:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _S8Z24);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Z24S8:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z24S8);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8B8G8R8:
                InfoPrintf("C24 kind is requested\n");
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
                attr |= DRF_DEF(OS32, _ATTR, _COLOR_PACKING, _X8R8G8B8);
                break;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X24S8:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                break;
            default:
                ErrPrintf("Unknown texel format %02X\n",
                    DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0));
                MASSERT(0);
        }
    }
    else
    {
        UINT32 format = DRF_VAL(A297,_TEXHEADV3_0,_COMPONENT_SIZES,m_state->DWORD0);

        switch (format)
        {
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_4X4:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X4:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X8:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X8:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X10:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X10:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X12:
                attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _128);
                break;
            default:
                ErrPrintf("Unknown texel format %02X\n", format);
                MASSERT(0);
        }
    }

    if (IsBlocklinear())
    {
        attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);
    }
    else
    {
        attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH);
    }

    if (VCAADepthBuffer())
    {
        if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
        {
            switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
            {
                case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV4R4V       :
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV8R8V       :
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV4R12V        :
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV8R24V:
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV4R4V :
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV8R8V :
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV4R12V:
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV8R24V:
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV4R4V  :
                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV4R4V  :
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV8R8V  :
                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV8R8V  :
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV4R12V:
                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV4R12V:
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                    break;

                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV8R24V:
                case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV8R24V:
                    attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
                    attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                    attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                    break;

                default:
                    MASSERT(!"Unknown VCAA component type!\n");
            }
        }
    }
    else if (1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
    {
        switch(DRF_VAL(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT, m_state->DWORD7))
        {
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_1X1:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X1:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _2);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X4:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _16);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
                break;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
                break;
            default:
                MASSERT(!"Unknown AA mode");
        }
    }

    return attr;
}

bool KeplerTextureHeader::VCAADepthBuffer() const
{
    if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
    {
        switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
        {
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV8R24V:
                return true;
            default:
                return false;
        }
    }

    return false;
}

bool KeplerTextureHeader::AllSourceInB() const
{
    return ((DRF_VAL(9097, _TEXHEAD0, _X_SOURCE, m_state->DWORD0) == LW9097_TEXHEAD0_X_SOURCE_IN_B) &&
        (DRF_VAL(9097, _TEXHEAD0, _Y_SOURCE, m_state->DWORD0) == LW9097_TEXHEAD0_Y_SOURCE_IN_B) &&
        (DRF_VAL(9097, _TEXHEAD0, _Z_SOURCE, m_state->DWORD0) == LW9097_TEXHEAD0_Z_SOURCE_IN_B) &&
        (DRF_VAL(9097, _TEXHEAD0, _W_SOURCE, m_state->DWORD0) == LW9097_TEXHEAD0_W_SOURCE_IN_B));
}

bool KeplerTextureHeader::IsBlocklinear() const
{
    return DRF_VAL(9097, _TEXHEAD2, _MEMORY_LAYOUT, m_state->DWORD2) ==
        LW9097_TEXHEAD2_MEMORY_LAYOUT_BLOCKLINEAR;
}

void KeplerTextureHeader::FixBlockSize() const
{
    if (!IsBlocklinear())
        return;

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
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_WIDTH, _ONE_GOB, m_state->DWORD2);
            break;
        default:
            MASSERT(!"Wrong block width");
    }

    switch (block_size[1])
    {
        case 1:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT, _ONE_GOB, m_state->DWORD2);
            break;
        case 2:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT, _TWO_GOBS, m_state->DWORD2);
            break;
        case 4:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT, _FOUR_GOBS, m_state->DWORD2);
            break;
        case 8:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT, _EIGHT_GOBS, m_state->DWORD2);
            break;
        case 16:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT, _SIXTEEN_GOBS, m_state->DWORD2);
            break;
        case 32:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT, _THIRTYTWO_GOBS, m_state->DWORD2);
            break;
        default:
            MASSERT(!"Wrong block height");
    }

    switch (block_size[2])
    {
        case 1:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH, _ONE_GOB, m_state->DWORD2);
            break;
        case 2:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH, _TWO_GOBS, m_state->DWORD2);
            break;
        case 4:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH, _FOUR_GOBS, m_state->DWORD2);
            break;
        case 8:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH, _EIGHT_GOBS, m_state->DWORD2);
            break;
        case 16:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH, _SIXTEEN_GOBS, m_state->DWORD2);
            break;
        case 32:
            m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH, _THIRTYTWO_GOBS, m_state->DWORD2);
            break;
        default:
            MASSERT(!"Wrong block depth");
    }
}

UINT32 KeplerTextureHeader::GetHeapType() const
{
    if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
    {
        switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
        {
            case LW9097_TEXHEAD0_COMPONENT_SIZES_S8Z24:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Z24S8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Z16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X24S8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV8R24V:
                return LWOS32_TYPE_DEPTH;
            default:
                return LWOS32_TYPE_IMAGE;
        }
    }

    return LWOS32_TYPE_IMAGE;
}

bool KeplerTextureHeader::Compressible() const
{
    switch (GetHeapType())
    {
        case LWOS32_TYPE_DEPTH:
            return true;
        case LWOS32_TYPE_IMAGE:
            if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
            {
                if (1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
                {
                    // on fermi any render format that can fit in C32, C64, or C128 pte kinds
                    // can be compressed
                    switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
                    {
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32_A32:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R16_G16_B16_A16:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_B24G8:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_A8B8G8R8:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_A2B10G10R10:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R16_G16:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_G8R24:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_G24R8:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R32:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_E5B9G9R9_SHAREDEXP:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_BF10GF11RF11:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_G8B8G8R8:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_B8G8R8G8:
                            return true;
                        default:
                            return false;
                    }
                }
                else
                {
                    switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
                    {
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R16_G16_B16_A16:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_A2B10G10R10:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_A8B8G8R8:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_BF10GF11RF11:
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_X8B8G8R8:
                            return true;
                        case LW9097_TEXHEAD0_COMPONENT_SIZES_R32:
                            return( DRF_VAL(9097,_TEXHEAD0,_R_DATA_TYPE,m_state->DWORD0)
                                == LW9097_TEXHEAD0_R_DATA_TYPE_NUM_FLOAT );
                        default:
                            return false;
                    }
                }
            }
            break;
    }
    return false;
}

UINT32 KeplerTextureHeader::BlockWidth() const
{
    switch(DRF_VAL(9097,_TEXHEAD2,_GOBS_PER_BLOCK_WIDTH,m_state->DWORD2))
    {
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_WIDTH_ONE_GOB:
            return 1;
        default:
            ErrPrintf("Wrong block width\n");
    }
    return 0;
}

UINT32 KeplerTextureHeader::BlockHeight() const
{
    switch(DRF_VAL(9097,_TEXHEAD2,_GOBS_PER_BLOCK_HEIGHT,m_state->DWORD2))
    {
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_HEIGHT_ONE_GOB:
            return 1;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_HEIGHT_TWO_GOBS:
            return 2;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_HEIGHT_FOUR_GOBS:
            return 4;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_HEIGHT_EIGHT_GOBS:
            return 8;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_HEIGHT_SIXTEEN_GOBS:
            return 16;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_HEIGHT_THIRTYTWO_GOBS:
            return 32;
        default:
                ErrPrintf("Wrong block height\n");
    }
    return 0;
}

UINT32 KeplerTextureHeader::BlockDepth() const
{
    switch (DRF_VAL(9097,_TEXHEAD2,_GOBS_PER_BLOCK_DEPTH,m_state->DWORD2))
    {
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_DEPTH_ONE_GOB:
            return 1;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_DEPTH_TWO_GOBS:
            return 2;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_DEPTH_FOUR_GOBS:
            return 4;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_DEPTH_EIGHT_GOBS:
            return 8;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_DEPTH_SIXTEEN_GOBS:
            return 16;
        case LW9097_TEXHEAD2_GOBS_PER_BLOCK_DEPTH_THIRTYTWO_GOBS:
            return 32;
        default:
            ErrPrintf("Wrong block height\n");
    }
    return 0;
}

UINT32 KeplerTextureHeader::TexelSize() const
{
    if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
    {
        switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
        {
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32_A32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT23:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT45:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN2:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_SF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_UF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC7U:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGBA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EACX2:
                return 16;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R16_G16_B16_A16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_B24G8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X24S8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV8R24V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV8R24V:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB_PTA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EAC:
                return 8;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R1:
                if (1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
                {
                    // Fermi does not support this format, treat it like A8R8G8B8
                    return 4;
                }
                else
                {
                    return 8;
                }
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8B8G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A8B8G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A2B10G10R10:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R16_G16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G8R24:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G24R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_S8Z24:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X8Z24:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Z24S8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_ZF32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_E5B9G9R9_SHAREDEXP:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BF10GF11RF11:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G8B8G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_B8G8R8G8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV4R4V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV8R8V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV4R12V:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV8R24V:
                return 4;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Z16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A4B4G4R4:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A5B5G5R1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_A1B5G5R5:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_B5G6R5:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_B6G5R5:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R16:
                return 2;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G4R4:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_Y8_VIDEO:
                return 1;
            default:
                ErrPrintf("Unknown texel format: 0x%x\n",
                    DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0));
                MASSERT(0);
        }
    }
    else
    {
        UINT32 format = DRF_VAL(A297,_TEXHEADV3_0,_COMPONENT_SIZES,m_state->DWORD0);
        switch (format)
        {
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_4X4:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X4:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X8:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X8:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X10:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X10:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X12:
                return 16;
            default:
                ErrPrintf("Unknown texel format: 0x%x\n", format);
                MASSERT(0);
        }
    }

    return 0;
}

UINT32 KeplerTextureHeader::TexelScale() const
{
    if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
    {
        switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
        {
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32:
                return 3;
            default:
                return 1;
        }
    }
    else
    {
        return 1;
    }
}

UINT32 KeplerTextureHeader::TextureWidth() const
{
    if (  1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
    {
        switch(DRF_VAL(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT, m_state->DWORD7))
        {
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_1X1:
                return DRF_VAL(9097,_TEXHEAD4,_WIDTH,m_state->DWORD4);
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X1:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
                return 2 * DRF_VAL(9097,_TEXHEAD4,_WIDTH,m_state->DWORD4);
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X4:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
                return 4 * DRF_VAL(9097,_TEXHEAD4,_WIDTH,m_state->DWORD4);
            default:
                MASSERT(!"Unknown AA mode");
                return 0;
        }
    }
    else
        return DRF_VAL(9097,_TEXHEAD4,_WIDTH,m_state->DWORD4);
}

UINT32 KeplerTextureHeader::TextureWidthScale() const
{
    if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
    {
        switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
        {
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT23:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT45:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN2:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_SF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_UF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC7U:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB_PTA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGBA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EAC:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EACX2:
                return 4;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R1:
                if (1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
                {
                    // Fermi does not support this format, treat it like A8R8G8B8
                    return 1;
                }
                return 8;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_G8B8G8R8:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_B8G8R8G8:
                return 2;
            default:
                return 1;
        }
    }
    else
    {
        UINT32 format = DRF_VAL(A297,_TEXHEADV3_0,_COMPONENT_SIZES,m_state->DWORD0);

        switch (format)
        {
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X10:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X12:
                return 12;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X8:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X10:
                return 10;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X8:
                return 8;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X6:
                return 6;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X4:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X5:
                return 5;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_4X4:
                return 4;

            default:
                ErrPrintf("Unknown texel format %02X\n", format);
                MASSERT(0);
                return 0;
        }
    }
}

UINT32 KeplerTextureHeader::TextureHeight() const
{
    if (  1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
    {
        UINT32 height =  DRF_VAL(9097, _TEXHEADV2_7, _HEIGHT_MSB, m_state->DWORD7) << 16 |
                         DRF_VAL(9097, _TEXHEADV2_7, _HEIGHT_MSB_RESERVED, m_state->DWORD7) << 17 |
                         DRF_VAL(9097, _TEXHEAD5,_HEIGHT,m_state->DWORD5);
        switch(DRF_VAL(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT, m_state->DWORD7))
        {
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_1X1:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X1:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
                return height;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
                return 2 * height;
            case LW9097_TEXHEADV2_7_MULTI_SAMPLE_COUNT_MODE_4X4:
                return 4 * height;
            default:
                MASSERT(!"Unknown AA mode");
                return 0;
        }
    }
    else
        return DRF_VAL(9097,_TEXHEAD5,_HEIGHT,m_state->DWORD5);
}

UINT32 KeplerTextureHeader::TextureHeightScale() const
{
    if (DRF_VAL(A297, _TEXHEAD0, _USE_COMPONENT_SIZES_EXTENDED, m_state->DWORD0) == 0)
    {
        switch(DRF_VAL(9097,_TEXHEAD0,_COMPONENT_SIZES,m_state->DWORD0))
        {
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT23:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXT45:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN1:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_DXN2:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_SF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC6H_UF16:
            case LW9097_TEXHEAD0_COMPONENT_SIZES_BC7U:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB_PTA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_ETC2_RGBA:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EAC:
            case LWA297_TEXHEAD0_COMPONENT_SIZES_EACX2:
                return 4;
            case LW9097_TEXHEAD0_COMPONENT_SIZES_R1:
                if (1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
                {
                    // Fermi does not support this format, treat it like A8R8G8B8
                    return 1;
                }
                return 8;
            default:
                return 1;
        }
    }
    else
    {
        UINT32 format = DRF_VAL(A297,_TEXHEADV3_0,_COMPONENT_SIZES,m_state->DWORD0);

        switch (format)
        {
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X12:
                return 12;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_12X10:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X10:
                return 10;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X8:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X8:
                return 8;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X6:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X6:
                return 6;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_10X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_8X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_6X5:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X5:
                return 5;

            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_5X4:
            case LWA297_TEXHEADV3_0_COMPONENT_SIZES_ASTC_2D_4X4:
                return 4;

            default:
                ErrPrintf("Unknown texel format %02X\n", format);
                MASSERT(0);
                return 0;
        }
    }
}

UINT32 KeplerTextureHeader::TextureDepth() const
{
    switch (DRF_VAL(9097,_TEXHEAD2,_TEXTURE_TYPE, m_state->DWORD2))
    {
        case LW9097_TEXHEAD2_TEXTURE_TYPE_ONE_D_ARRAY:
        case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D_ARRAY:
        case LW9097_TEXHEAD2_TEXTURE_TYPE_ONE_D_BUFFER:
        case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 1;
        default:
            if (1 == DRF_VAL(9097, _TEXHEAD4, _USE_TEXTURE_HEADER_VERSION2, m_state->DWORD4))
            {
                return (DRF_VAL(9297, _TEXHEADV2_7, _DEPTH_MSB, m_state->DWORD7) << 12 |
                    DRF_VAL(9297, _TEXHEAD5,_DEPTH,m_state->DWORD5));
            }
            return DRF_VAL(9097,_TEXHEAD5,_DEPTH,m_state->DWORD5);
    }
}

UINT32 KeplerTextureHeader::ArraySize() const
{
    UINT32 depth = DRF_VAL(9097,_TEXHEAD5,_DEPTH, m_state->DWORD5);
    UINT32 type = DRF_VAL(9097,_TEXHEAD2,_TEXTURE_TYPE, m_state->DWORD2);

    switch (type)
    {
        case LW9097_TEXHEAD2_TEXTURE_TYPE_ONE_D_ARRAY:
        case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D_ARRAY:
        case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return depth;
        default:
            return 1;
    }
}

UINT32 KeplerTextureHeader::BorderWidth() const
{
    UINT32 border = DRF_VAL(9097,_TEXHEAD2, _BORDER_SOURCE, m_state->DWORD2);

    switch (border)
    {
        case LW9097_TEXHEAD2_BORDER_SOURCE_BORDER_COLOR:
            return 0;
        case LW9097_TEXHEAD2_BORDER_SOURCE_BORDER_TEXTURE:
            return 1;
        default:
            ErrPrintf("Wrong border value\n");
    }
    return 0;
}

UINT32 KeplerTextureHeader::BorderHeight() const
{
    UINT32 border = DRF_VAL(9097,_TEXHEAD2, _BORDER_SOURCE, m_state->DWORD2);
    UINT32 type = DRF_VAL(9097,_TEXHEAD2, _TEXTURE_TYPE, m_state->DWORD2);

    switch (border)
    {
        case LW9097_TEXHEAD2_BORDER_SOURCE_BORDER_COLOR:
            return 0;
        case LW9097_TEXHEAD2_BORDER_SOURCE_BORDER_TEXTURE:
        {
            switch (type)
            {
                case LW9097_TEXHEAD2_TEXTURE_TYPE_ONE_D:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_ONE_D_ARRAY:
                    return 0;
                case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D_ARRAY:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D_NO_MIPMAP:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP_ARRAY:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_THREE_D:
                    return 1;
                default:
                    ErrPrintf("Wrong bordered texture type\n");
                    break;
            }
        }
        break;
        default:
            ErrPrintf("Wrong border value\n");
    }
    return 0;
}

UINT32 KeplerTextureHeader::BorderDepth() const
{
    UINT32 border = DRF_VAL(9097,_TEXHEAD2, _BORDER_SOURCE, m_state->DWORD2);
    UINT32 type = DRF_VAL(9097,_TEXHEAD2, _TEXTURE_TYPE, m_state->DWORD2);

    switch (border)
    {
        case LW9097_TEXHEAD2_BORDER_SOURCE_BORDER_COLOR:
            return 0;
        case LW9097_TEXHEAD2_BORDER_SOURCE_BORDER_TEXTURE:
        {
            switch (type)
            {
                case LW9097_TEXHEAD2_TEXTURE_TYPE_ONE_D:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_ONE_D_ARRAY:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D_ARRAY:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_TWO_D_NO_MIPMAP:
                case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP_ARRAY:
                    return 0;
                case LW9097_TEXHEAD2_TEXTURE_TYPE_THREE_D:
                    return 1;
                default:
                    ErrPrintf("Wrong bordered texture type\n");
                    break;
            }
        }
        break;
        default:
            ErrPrintf("Wrong border value\n");
    }
    return 0;
}

UINT32 KeplerTextureHeader::DimSize() const
{
    UINT32 type = DRF_VAL(9097,_TEXHEAD2,_TEXTURE_TYPE, m_state->DWORD2);

    switch (type)
    {
        case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP:
        case LW9097_TEXHEAD2_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 6;
        default:
            return 1;
    }
}

UINT32 KeplerTextureHeader::TexturePitch() const
{
    return DRF_VAL(9097,_TEXHEAD3,_PITCH,m_state->DWORD3);
}

UINT32 KeplerTextureHeader::MaxMipLevel() const
{
    return DRF_VAL(9097,_TEXHEAD5,_MAX_MIP_LEVEL,m_state->DWORD5);
}

RC KeplerTextureHeader::RelocTexture
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

    m_state->DWORD0 = raster->GetTextureHeaderDword0(
        GetAAModeFromSurface(surface), textureMode, gpuDevice);

    if (useOffset)
    {
        m_state->DWORD1 = FLD_SET_DRF_NUM(9097, _TEXHEAD1, _OFFSET_LOWER,
            LwU64_LO32(offset), m_state->DWORD1);

        m_state->DWORD2 = FLD_SET_DRF_NUM(9097, _TEXHEAD2, _OFFSET_UPPER,
            LwU64_HI32(offset), m_state->DWORD2);
    }

    if (!useSurfaceAttr)
    {
        return OK;
    }

    if (surface->GetLayout() == Surface2D::BlockLinear)
    {
        m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _MEMORY_LAYOUT,
            _BLOCKLINEAR, m_state->DWORD2);

        switch (surface->GetLogBlockWidth())
        {
            case 0:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_WIDTH,
                    _ONE_GOB, m_state->DWORD2);
                break;
            default:
                MASSERT(!"unknown block width");
        }

        switch (surface->GetLogBlockHeight())
        {
            case 0:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT,
                    _ONE_GOB, m_state->DWORD2);
                break;
            case 1:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT,
                    _TWO_GOBS, m_state->DWORD2);
                break;
            case 2:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT,
                    _FOUR_GOBS, m_state->DWORD2);
                break;
            case 3:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT,
                    _EIGHT_GOBS, m_state->DWORD2);
                break;
            case 4:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT,
                    _SIXTEEN_GOBS, m_state->DWORD2);
                break;
            case 5:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_HEIGHT,
                    _THIRTYTWO_GOBS, m_state->DWORD2);
                break;
            default:
                MASSERT(!"unknown block height");
        }

        switch (surface->GetLogBlockDepth())
        {
            case 0:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH,
                    _ONE_GOB, m_state->DWORD2);
                break;
            case 1:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH,
                    _TWO_GOBS, m_state->DWORD2);
                break;
            case 2:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH,
                    _FOUR_GOBS, m_state->DWORD2);
                break;
            case 3:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH,
                    _EIGHT_GOBS, m_state->DWORD2);
                break;
            case 4:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH,
                    _SIXTEEN_GOBS, m_state->DWORD2);
                break;
            case 5:
                m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _GOBS_PER_BLOCK_DEPTH,
                    _THIRTYTWO_GOBS, m_state->DWORD2);
                break;
            default:
                MASSERT(!"unknown block depth");
        }

        BlockShrinkingSanityCheck(surface, gpuDevice);
    }
    else if (surface->GetLayout() == Surface2D::Pitch)
    {
        m_state->DWORD2 = FLD_SET_DRF(9097, _TEXHEAD2, _MEMORY_LAYOUT, _PITCH, m_state->DWORD2);
    }
    else
    {
        MASSERT(!"unknown surface format");
    }

    if (raster->IsSrgb())
    {
        m_state->DWORD2 = FLD_SET_DRF_NUM(9097, _TEXHEAD2,
            _S_R_G_B_COLWERSION, 1, m_state->DWORD2);
    }

    m_state->DWORD3 = FLD_SET_DRF_NUM(9097, _TEXHEAD3, _PITCH,
        surface->GetPitch(), m_state->DWORD3);

    m_state->DWORD4 = FLD_SET_DRF_NUM(9097, _TEXHEAD4, _WIDTH,
        surface->GetWidth(), m_state->DWORD4);

    m_state->DWORD5 = FLD_SET_DRF_NUM(9097, _TEXHEAD5, _HEIGHT,
        surface->GetHeight(), m_state->DWORD5);

    m_state->DWORD5 = FLD_SET_DRF_NUM(9097, _TEXHEAD5, _DEPTH,
        surface->GetDepth(), m_state->DWORD5);

    switch (GetAAModeFromSurface(surface))
    {
        case AAMODE_1X1:
            m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                _MODE_1X1, m_state->DWORD7);
            break;
        case AAMODE_2X1:
            if (centerSpoof)
            {
                m_state->DWORD7 = FLD_SET_DRF(A097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_2X1_CENTER, m_state->DWORD7);
            }
            else
            {
                m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_2X1, m_state->DWORD7);
            }
            break;
        case AAMODE_2X2:
            if (centerSpoof)
            {
                m_state->DWORD7 = FLD_SET_DRF(A097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_2X2_CENTER, m_state->DWORD7);
            }
            else
            {
                m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_2X2, m_state->DWORD7);
            }
            break;
        case AAMODE_4X2:
            if (centerSpoof)
            {
                m_state->DWORD7 = FLD_SET_DRF(A097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_4X2_CENTER, m_state->DWORD7);
            }
            else
            {
                m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_4X2, m_state->DWORD7);
            }
            break;
        case AAMODE_4X4:
            m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                _MODE_4X4, m_state->DWORD7);
            break;
        case AAMODE_2X1_D3D:
            if (centerSpoof)
            {
                m_state->DWORD7 = FLD_SET_DRF(A097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_2X1_CENTER, m_state->DWORD7);
            }
            else
            {
                m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_2X1_D3D, m_state->DWORD7);
            }
            break;
        case AAMODE_4X2_D3D:
            if (centerSpoof)
            {
                m_state->DWORD7 = FLD_SET_DRF(A097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_4X2_CENTER, m_state->DWORD7);
            }
            else
            {
                m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                    _MODE_4X2_D3D, m_state->DWORD7);
            }
            break;
        case AAMODE_2X2_VC_4:
            m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                _MODE_2X2_VC_4, m_state->DWORD7);
            break;
        case AAMODE_2X2_VC_12:
            m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                _MODE_2X2_VC_12, m_state->DWORD7);
            break;
        case AAMODE_4X2_VC_8:
            m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                _MODE_4X2_VC_8, m_state->DWORD7);
            break;
        case AAMODE_4X2_VC_24:
            m_state->DWORD7 = FLD_SET_DRF(9097, _TEXHEADV2_7, _MULTI_SAMPLE_COUNT,
                _MODE_4X2_VC_24, m_state->DWORD7);
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    // edit promotion value for dynamic texture
    if (optimalPromotion)
    {
        CHECK_RC(ModifySectorPromotion(m_state, GetOptimalSectorPromotion()));
    }

    *newState = *m_state;

    return OK;
}

RC KeplerTextureHeader::ModifySectorPromotion(TextureHeaderState* state, L1_PROMOTION newPromotion)
{
    state->DWORD2 = FLD_SET_DRF_NUM(9097, _TEXHEAD2, _SECTOR_PROMOTION, newPromotion, state->DWORD2);

    return OK;
}

ColorUtils::Format KeplerTextureHeader::GetColorFormat() const
{
    switch (DRF_VAL(A097, _TEXHEAD0, _COMPONENT_SIZES, m_state->DWORD0))
    {
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32_A32:
        return ColorUtils::R32_G32_B32_A32;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32:
        return ColorUtils::R32_G32_B32;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R16_G16_B16_A16:
        return ColorUtils::R16_G16_B16_A16;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R32_G32:
        return ColorUtils::R32_G32;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R32_B24G8:
        return ColorUtils::R32_B24G8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X8B8G8R8:
        return ColorUtils::X8B8G8R8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_A8B8G8R8:
        return ColorUtils::A8B8G8R8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_A2B10G10R10:
        return ColorUtils::A2B10G10R10;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R16_G16:
        return ColorUtils::R16_G16;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_G8R24:
        return ColorUtils::G8R24;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_G24R8:
        return ColorUtils::G24R8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R32:
        return ColorUtils::R32;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_A4B4G4R4:
        return ColorUtils::A4B4G4R4;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_A5B5G5R1:
        return ColorUtils::A5B5G5R1;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_A1B5G5R5:
        return ColorUtils::A1B5G5R5;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_B5G6R5:
        return ColorUtils::B5G6R5;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_B6G5R5:
        return ColorUtils::B6G5R5;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_G8R8:
        return ColorUtils::G8R8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R16:
        return ColorUtils::R16;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_Y8_VIDEO:
        return ColorUtils::Y8_VIDEO;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R8:
        return ColorUtils::R8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_G4R4:
        return ColorUtils::G4R4;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_R1:
        return ColorUtils::R1;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_E5B9G9R9_SHAREDEXP:
        return ColorUtils::E5B9G9R9_SHAREDEXP;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_BF10GF11RF11:
        return ColorUtils::BF10GF11RF11;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_G8B8G8R8:
        return ColorUtils::G8B8G8R8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_B8G8R8G8:
        return ColorUtils::B8G8R8G8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_DXT1:
        return ColorUtils::DXT1;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_DXT23:
        return ColorUtils::DXT23;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_DXT45:
        return ColorUtils::DXT45;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_DXN1:
        return ColorUtils::DXN1;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_DXN2:
        return ColorUtils::DXN2;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_BC6H_SF16:
        return ColorUtils::BC6H_SF16;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_BC6H_UF16:
        return ColorUtils::BC6H_UF16;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_BC7U:
        return ColorUtils::BC7U;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_Z24S8:
        return ColorUtils::Z24S8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X8Z24:
        return ColorUtils::X8Z24;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_S8Z24:
        return ColorUtils::S8Z24;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV4R4V:
        return ColorUtils::X4V4Z24__COV4R4V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV8R8V:
        return ColorUtils::X4V4Z24__COV8R8V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV4R12V:
        return ColorUtils::V8Z24__COV4R12V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32:
        return ColorUtils::ZF32;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X24S8:
        return ColorUtils::ZF32_X24S8;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV4R4V:
        return ColorUtils::X8Z24_X20V4S8__COV4R4V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV8R8V:
        return ColorUtils::X8Z24_X20V4S8__COV8R8V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV4R4V:
        return ColorUtils::ZF32_X20V4X8__COV4R4V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV8R8V:
        return ColorUtils::ZF32_X20V4X8__COV8R8V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV4R4V:
        return ColorUtils::ZF32_X20V4S8__COV4R4V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV8R8V:
        return ColorUtils::ZF32_X20V4S8__COV8R8V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV4R12V:
        return ColorUtils::X8Z24_X16V8S8__COV4R12V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV4R12V:
        return ColorUtils::ZF32_X16V8X8__COV4R12V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV4R12V:
        return ColorUtils::ZF32_X16V8S8__COV4R12V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_Z16:
        return ColorUtils::Z16;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV8R24V:
        return ColorUtils::V8Z24__COV8R24V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV8R24V:
        return ColorUtils::X8Z24_X16V8S8__COV8R24V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV8R24V:
        return ColorUtils::ZF32_X16V8X8__COV8R24V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV8R24V:
        return ColorUtils::ZF32_X16V8S8__COV8R24V;
    case LWA097_TEXHEAD0_COMPONENT_SIZES_CS_BITFIELD_SIZE:
    default:
        return ColorUtils::LWFMT_NONE;
    }
}

