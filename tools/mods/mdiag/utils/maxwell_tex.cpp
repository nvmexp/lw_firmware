/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013,2015-2016,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "maxwell_tex.h"
#include "mdiag/sysspec.h"
#include "class/clb097tex.h" // maxwell_a
#include "lwos.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/floorsweepimpl.h"

TextureHeader* MaxwellTextureHeader::CreateTextureHeader(const TextureHeaderState* state,
    GpuSubdevice* pGpuSubdev)
{
    TextureHeader *header;
    MaxwellTextureHeaderState maxwellState(state);

    UINT32 headerVersion = DRF_VAL_MW(B097, _TEXHEAD_BL, _HEADER_VERSION, maxwellState.m_Data);

    switch (headerVersion)
    {
        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_BLOCKLINEAR:
            header = new MaxwellBlocklinearTextureHeader(state, pGpuSubdev);
            break;

        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_BLOCKLINEAR_COLOR_KEY:
            header = new MaxwellBlocklinearColorkeyTextureHeader(state, pGpuSubdev);
            break;

        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_ONE_D_BUFFER:
            header = new MaxwellOneDBufferTextureHeader(state, pGpuSubdev);
            break;

        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_PITCH:
            header = new MaxwellPitchTextureHeader(state, pGpuSubdev);
            break;

        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_PITCH_COLOR_KEY:
            header = new MaxwellPitchColorkeyTextureHeader(state, pGpuSubdev);
            break;

        default:
            MASSERT(!"Unexpected Maxwell texture header version");
            header = NULL;
            break;
    }

    return header;
}

MaxwellTextureHeader::MaxwellTextureHeader
(
    const TextureHeaderState* state,
    GpuSubdevice* gpuSubdevice
)
:   TextureHeader(gpuSubdevice)
{
    MASSERT(state);
    m_State = new MaxwellTextureHeaderState(state);
}

MaxwellTextureHeader::~MaxwellTextureHeader()
{
    if (m_State != 0)
    {
        delete m_State;
    }
}

UINT32 MaxwellTextureHeader::GetHeapAttr() const
{
    UINT32 attr = 0;

    UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32_B32_A32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32_B32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT23:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT45:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC7U:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EACX2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _128);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16_G16_B16_A16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_B24G8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EAC:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R1:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A8B8G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A2B10G10R10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16_G16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8R24:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G24R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_E5B9G9R9_SHAREDEXP:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BF10GF11RF11:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8B8G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B8G8R8G8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV8R24V:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A4B4G4R4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A5B5G5R1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A1B5G5R5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B5G6R5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B6G5R5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _16);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G4R4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Y8_VIDEO:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _8);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Z16:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _16);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z16);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_S8Z24:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _S8Z24);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Z24S8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z24S8);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8B8G8R8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _32);
            attr |= DRF_DEF(OS32, _ATTR, _COLOR_PACKING, _X8R8G8B8);
            break;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X24S8:
            attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _64);
            attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
            attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
            break;

        default:
            ErrPrintf("Unknown texel format %02X\n", format);
            MASSERT(0);
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
        switch (format)
        {
            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV4R4V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV8R8V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV4R12V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV8R24V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV4R4V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV8R8V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV4R12V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV8R24V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _X8Z24_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV4R4V:
            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV4R4V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV8R8V:
            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV8R8V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV4R12V:
            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV4R12V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                break;

            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV8R24V:
            case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV8R24V:
                attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
                attr |= DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32_X24S8);
                attr |= DRF_DEF(OS32, _ATTR, _Z_TYPE, _FLOAT);
                break;

            default:
                MASSERT(!"Unknown VCAA component type!\n");
        }
    }
    else
    {
        attr |= GetMultiSampleAttr();
    }

    return attr;
}

bool MaxwellTextureHeader::VCAADepthBuffer() const
{
    UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV8R24V:
            return true;
    }

    return false;
}

bool MaxwellTextureHeader::AllSourceInB() const
{
    return ((DRF_VAL_MW(B097, _TEXHEAD_BL, _X_SOURCE, m_State->m_Data) == LWB097_TEXHEAD_BL_X_SOURCE_IN_B) &&
        (DRF_VAL_MW(B097, _TEXHEAD_BL, _Y_SOURCE, m_State->m_Data) == LWB097_TEXHEAD_BL_Y_SOURCE_IN_B) &&
        (DRF_VAL_MW(B097, _TEXHEAD_BL, _Z_SOURCE, m_State->m_Data) == LWB097_TEXHEAD_BL_Z_SOURCE_IN_B) &&
        (DRF_VAL_MW(B097, _TEXHEAD_BL, _W_SOURCE, m_State->m_Data) == LWB097_TEXHEAD_BL_W_SOURCE_IN_B));
}

UINT32 MaxwellTextureHeader::GetHeapType() const
{
    UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_S8Z24:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Z24S8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Z16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X24S8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV8R24V:
            return LWOS32_TYPE_DEPTH;

        default:
            return LWOS32_TYPE_IMAGE;
    }

    return LWOS32_TYPE_IMAGE;
}

bool MaxwellTextureHeader::Compressible() const
{
    switch (GetHeapType())
    {
        case LWOS32_TYPE_DEPTH:
            return true;

        case LWOS32_TYPE_IMAGE:
            {
                UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

                switch (format)
                {
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32_B32_A32:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16_G16_B16_A16:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_B24G8:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A8B8G8R8:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A2B10G10R10:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16_G16:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8R24:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G24R8:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_E5B9G9R9_SHAREDEXP:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BF10GF11RF11:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8B8G8R8:
                    case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B8G8R8G8:
                        return true;

                    default:
                        return false;
                }
                break;
            }

        default:
            MASSERT(!"Unexpected heap type");
            break;
    }

    return false;
}

UINT32 MaxwellTextureHeader::TexelSize() const
{
    UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32_B32_A32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT23:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT45:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC7U:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EACX2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            return 16;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16_G16_B16_A16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_B24G8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4X8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X20V4S8__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X24S8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8X8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32_X16V8S8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24_X16V8S8__COV8R24V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EAC:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R1:
            return 8;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8B8G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A8B8G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A2B10G10R10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16_G16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8R24:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G24R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_S8Z24:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X8Z24:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Z24S8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ZF32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_E5B9G9R9_SHAREDEXP:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BF10GF11RF11:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8B8G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B8G8R8G8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32_B32:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV4R4V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_X4V4Z24__COV8R8V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV4R12V:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_V8Z24__COV8R24V:
            return 4;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Z16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A4B4G4R4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A5B5G5R1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_A1B5G5R5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B5G6R5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B6G5R5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R16:
            return 2;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G4R4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_Y8_VIDEO:
            return 1;

        default:
            ErrPrintf("Unknown texel format: 0x%x\n", format);
            MASSERT(0);
    }

    return 0;
}

UINT32 MaxwellTextureHeader::TexelScale() const
{
    UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_R32_G32_B32:
            return 3;
    }

    return 1;
}

UINT32 MaxwellTextureHeader::TextureWidthScale() const
{
    UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            return 12;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
            return 10;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
            return 8;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
            return 6;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
            return 5;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT23:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT45:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC7U:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EAC:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EACX2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
            return 4;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_G8B8G8R8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_B8G8R8G8:
            return 2;
    }

    return 1;
}

UINT32 MaxwellTextureHeader::TextureHeightScale() const
{
    UINT32 format = DRF_VAL_MW(B097, _TEXHEAD_BL, _COMPONENTS, m_State->m_Data);

    switch (format)
    {
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X12:
            return 12;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_12X10:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X10:
            return 10;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X8:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X8:
            return 8;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X6:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X6:
            return 6;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_10X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_8X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_6X5:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X5:
            return 5;

        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT23:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXT45:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN1:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_DXN2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_SF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC6H_UF16:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_BC7U:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGB_PTA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ETC2_RGBA:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EAC:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_EACX2:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_5X4:
        case LWB097_TEXHEAD_BL_COMPONENTS_SIZES_ASTC_2D_4X4:
            return 4;
    }

    return 1;
}

UINT32 MaxwellTextureHeader::DimSize() const
{
    switch (DRF_VAL_MW(B097, _TEXHEAD_BL, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_LWBEMAP:
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 6;
    }

    return 1;
}

UINT32 MaxwellTextureHeader::BorderWidth() const
{
    return BorderSize();
}

UINT32 MaxwellTextureHeader::BorderHeight() const
{
    UINT32 textureType = DRF_VAL_MW(B097, _TEXHEAD_BLCK, _TEXTURE_TYPE, m_State->m_Data);

    switch (textureType)
    {
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D_BUFFER:
            return 0;

        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_THREE_D:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_LWBEMAP:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D_NO_MIPMAP:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return BorderSize();

        default:
            ErrPrintf("Unrecognized texture type %u\n", textureType);
            MASSERT(0);
            return 0;
    }
}

UINT32 MaxwellTextureHeader::BorderDepth() const
{
    UINT32 textureType = DRF_VAL_MW(B097, _TEXHEAD_BLCK, _TEXTURE_TYPE, m_State->m_Data);

    switch (textureType)
    {
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_LWBEMAP:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D_BUFFER:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D_NO_MIPMAP:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 0;

        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_THREE_D:
            return BorderSize();

        default:
            ErrPrintf("Unrecognized texture type %u\n", textureType);
            MASSERT(0);
            return 0;
    }
}

RC MaxwellTextureHeader::RelocTexture
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
    UINT32 headerVersion = DRF_VAL_MW(B097, _TEXHEAD_BL, _HEADER_VERSION, m_State->m_Data);

    // Make sure the surface layout isn't incompatible with the texture header version.
    switch (headerVersion)
    {
        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_BLOCKLINEAR:
        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_BLOCKLINEAR_COLOR_KEY:
            if (surface->GetLayout() != Surface2D::BlockLinear)
            {
                ErrPrintf("layout for RELOC_D surface is incompatible with texture header type");
                return RC::SOFTWARE_ERROR;
            }
            break;

        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_ONE_D_BUFFER:
        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_PITCH:
        case LWB097_TEXHEAD_BL_HEADER_VERSION_SELECT_PITCH_COLOR_KEY:
            if (surface->GetLayout() != Surface2D::Pitch)
            {
                ErrPrintf("layout for RELOC_D surface is incompatible with texture header type");
                return RC::SOFTWARE_ERROR;
            }
            break;

        default:
            MASSERT(!"Unexpected Maxwell texture header version");
            break;
    }

    raster->SetMaxwellTextureHeaderFormat(m_State->m_Data,
        GetAAModeFromSurface(surface), textureMode);

    if (useOffset)
    {
        ReplaceTextureAddress(offset);
    }

    if (!useSurfaceAttr)
    {
        return OK;
    }

    RelocLayout(surface, gpuDevice);

    UINT32 isSrgb = raster->IsSrgb();
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BL, _S_R_G_B_COLWERSION, isSrgb, m_State->m_Data);

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

RC MaxwellTextureHeader::ModifySectorPromotion(MaxwellTextureHeaderState* state, L1_PROMOTION newPromotion)
{
    switch (newPromotion)
    {
        case L1_PROMOTION_NONE:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _SECTOR_PROMOTION, _NO_PROMOTION, state->m_Data);
            break;

        case L1_PROMOTION_2V:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _SECTOR_PROMOTION, _PROMOTE_TO_2_V, state->m_Data);
            break;

        case L1_PROMOTION_2H:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _SECTOR_PROMOTION, _PROMOTE_TO_2_H, state->m_Data);
            break;

        case L1_PROMOTION_ALL:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _SECTOR_PROMOTION, _PROMOTE_TO_4, state->m_Data);
            break;

        default:
            ErrPrintf("Unrecognized L1 promotion: %u\n", newPromotion);
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

ColorUtils::Format MaxwellTextureHeader::GetColorFormat() const
{
    switch (DRF_VAL(B097, _TEXHEAD0, _COMPONENT_SIZES, m_State->m_Data[0]))
    {
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32_A32:
        return ColorUtils::R32_G32_B32_A32;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R32_G32_B32:
        return ColorUtils::R32_G32_B32;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R16_G16_B16_A16:
        return ColorUtils::R16_G16_B16_A16;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R32_G32:
        return ColorUtils::R32_G32;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R32_B24G8:
        return ColorUtils::R32_B24G8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X8B8G8R8:
        return ColorUtils::X8B8G8R8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_A8B8G8R8:
        return ColorUtils::A8B8G8R8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_A2B10G10R10:
        return ColorUtils::A2B10G10R10;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R16_G16:
        return ColorUtils::R16_G16;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_G8R24:
        return ColorUtils::G8R24;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_G24R8:
        return ColorUtils::G24R8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R32:
        return ColorUtils::R32;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_A4B4G4R4:
        return ColorUtils::A4B4G4R4;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_A5B5G5R1:
        return ColorUtils::A5B5G5R1;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_A1B5G5R5:
        return ColorUtils::A1B5G5R5;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_B5G6R5:
        return ColorUtils::B5G6R5;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_B6G5R5:
        return ColorUtils::B6G5R5;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_G8R8:
        return ColorUtils::G8R8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R16:
        return ColorUtils::R16;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_Y8_VIDEO:
        return ColorUtils::Y8_VIDEO;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R8:
        return ColorUtils::R8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_G4R4:
        return ColorUtils::G4R4;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_R1:
        return ColorUtils::R1;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_E5B9G9R9_SHAREDEXP:
        return ColorUtils::E5B9G9R9_SHAREDEXP;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_BF10GF11RF11:
        return ColorUtils::BF10GF11RF11;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_G8B8G8R8:
        return ColorUtils::G8B8G8R8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_B8G8R8G8:
        return ColorUtils::B8G8R8G8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_DXT1:
        return ColorUtils::DXT1;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_DXT23:
        return ColorUtils::DXT23;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_DXT45:
        return ColorUtils::DXT45;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_DXN1:
        return ColorUtils::DXN1;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_DXN2:
        return ColorUtils::DXN2;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_BC6H_SF16:
        return ColorUtils::BC6H_SF16;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_BC6H_UF16:
        return ColorUtils::BC6H_UF16;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_BC7U:
        return ColorUtils::BC7U;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB:
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ETC2_RGB_PTA:
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ETC2_RGBA:
    case LWB097_TEXHEAD0_COMPONENT_SIZES_EAC:
    case LWB097_TEXHEAD0_COMPONENT_SIZES_EACX2:
        // FIXME: Need to add above texture color formats in ColorUtils.
        return ColorUtils::LWFMT_NONE;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_Z24S8:
        return ColorUtils::Z24S8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X8Z24:
        return ColorUtils::X8Z24;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_S8Z24:
        return ColorUtils::S8Z24;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV4R4V:
        return ColorUtils::X4V4Z24__COV4R4V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X4V4Z24__COV8R8V:
        return ColorUtils::X4V4Z24__COV8R8V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV4R12V:
        return ColorUtils::V8Z24__COV4R12V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32:
        return ColorUtils::ZF32;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X24S8:
        return ColorUtils::ZF32_X24S8;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV4R4V:
        return ColorUtils::X8Z24_X20V4S8__COV4R4V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X20V4S8__COV8R8V:
        return ColorUtils::X8Z24_X20V4S8__COV8R8V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV4R4V:
        return ColorUtils::ZF32_X20V4X8__COV4R4V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4X8__COV8R8V:
        return ColorUtils::ZF32_X20V4X8__COV8R8V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV4R4V:
        return ColorUtils::ZF32_X20V4S8__COV4R4V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X20V4S8__COV8R8V:
        return ColorUtils::ZF32_X20V4S8__COV8R8V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV4R12V:
        return ColorUtils::X8Z24_X16V8S8__COV4R12V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV4R12V:
        return ColorUtils::ZF32_X16V8X8__COV4R12V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV4R12V:
        return ColorUtils::ZF32_X16V8S8__COV4R12V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_Z16:
        return ColorUtils::Z16;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_V8Z24__COV8R24V:
        return ColorUtils::V8Z24__COV8R24V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_X8Z24_X16V8S8__COV8R24V:
        return ColorUtils::X8Z24_X16V8S8__COV8R24V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8X8__COV8R24V:
        return ColorUtils::ZF32_X16V8X8__COV8R24V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_ZF32_X16V8S8__COV8R24V:
        return ColorUtils::ZF32_X16V8S8__COV8R24V;
    case LWB097_TEXHEAD0_COMPONENT_SIZES_CS_BITFIELD_SIZE:
    default:
        return ColorUtils::LWFMT_NONE;
    }
}

/////

void MaxwellBlocklinearTextureHeader::FixBlockSize() const
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
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_WIDTH, _ONE_GOB, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block width");
    }

    switch (block_size[1])
    {
        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _ONE_GOB, m_State->m_Data);
            break;
        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _TWO_GOBS, m_State->m_Data);
            break;
        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _FOUR_GOBS, m_State->m_Data);
            break;
        case 8:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _EIGHT_GOBS, m_State->m_Data);
            break;
        case 16:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _SIXTEEN_GOBS, m_State->m_Data);
            break;
        case 32:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _THIRTYTWO_GOBS, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block height");
    }

    switch (block_size[2])
    {
        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _ONE_GOB, m_State->m_Data);
            break;
        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _TWO_GOBS, m_State->m_Data);
            break;
        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _FOUR_GOBS, m_State->m_Data);
            break;
        case 8:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _EIGHT_GOBS, m_State->m_Data);
            break;
        case 16:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _SIXTEEN_GOBS, m_State->m_Data);
            break;
        case 32:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _THIRTYTWO_GOBS, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block depth");
    }
}

UINT32 MaxwellBlocklinearTextureHeader::GetMultiSampleAttr() const
{
    UINT32 attr = 0;
    UINT32 multiSampleCount = DRF_VAL_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_1X1:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1_CENTER:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _2);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_CENTER:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_CENTER:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X4:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _16);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
            break;

        default:
            MASSERT(!"Unknown AA mode");
    }

    return attr;
}

UINT32 MaxwellBlocklinearTextureHeader::BlockWidth() const
{
    switch (DRF_VAL_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_WIDTH, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_WIDTH_ONE_GOB:
            return 1;
        default:
            MASSERT(!"Unexpected block width\n");
    }

    return 0;
}

UINT32 MaxwellBlocklinearTextureHeader::BlockHeight() const
{
    switch (DRF_VAL_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_HEIGHT_ONE_GOB:
            return 1;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_HEIGHT_TWO_GOBS:
            return 2;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_HEIGHT_FOUR_GOBS:
            return 4;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_HEIGHT_EIGHT_GOBS:
            return 8;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_HEIGHT_SIXTEEN_GOBS:
            return 16;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_HEIGHT_THIRTYTWO_GOBS:
            return 32;
        default:
            MASSERT(!"Unexpected block height\n");
    }

    return 0;
}

UINT32 MaxwellBlocklinearTextureHeader::BlockDepth() const
{
    switch (DRF_VAL_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_DEPTH_ONE_GOB:
            return 1;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_DEPTH_TWO_GOBS:
            return 2;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_DEPTH_FOUR_GOBS:
            return 4;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_DEPTH_EIGHT_GOBS:
            return 8;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_DEPTH_SIXTEEN_GOBS:
            return 16;
        case LWB097_TEXHEAD_BL_GOBS_PER_BLOCK_DEPTH_THIRTYTWO_GOBS:
            return 32;
        default:
            MASSERT(!"Unexpected block depth\n");
    }

    return 0;
}

UINT32 MaxwellBlocklinearTextureHeader::TextureWidth() const
{
    UINT32 width = DRF_VAL_MW(B097, _TEXHEAD_BL, _WIDTH_MINUS_ONE, m_State->m_Data) + 1;
    UINT32 multiSampleCount = DRF_VAL_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_1X1:
            // No width scaling due to multi-sampling
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1_CENTER:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_CENTER:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
            width *= 2;
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_CENTER:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X4:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            width *= 4;
            break;

        default:
            MASSERT(!"Unknown AA mode");
    }

    return width;
}

UINT32 MaxwellBlocklinearTextureHeader::TextureHeight() const
{
    UINT32 height = DRF_VAL_MW(B097, _TEXHEAD_BL, _HEIGHT_MINUS_ONE, m_State->m_Data) + 1;
    UINT32 multiSampleCount = DRF_VAL_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_1X1:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X1_CENTER:
            // No height scaling due to multi-sampling
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_CENTER:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_CENTER:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            height *= 2;
            break;

        case LWB097_TEXHEAD_BL_MULTI_SAMPLE_COUNT_MODE_4X4:
            height *= 4;
            break;

        default:
            MASSERT(!"Unknown AA mode");
    }

    return height;
}

UINT32 MaxwellBlocklinearTextureHeader::TextureDepth() const
{
    // The depth field is used as the array size for arrays, so return 1 here.
    switch (DRF_VAL_MW(B097, _TEXHEAD_BL, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 1;
    }

    return DRF_VAL_MW(B097, _TEXHEAD_BL, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellBlocklinearTextureHeader::ArraySize() const
{
    // The depth field is used as the array size for arrays.
    switch (DRF_VAL_MW(B097, _TEXHEAD_BL, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_BL_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return DRF_VAL_MW(B097, _TEXHEAD_BL, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
    }

    return 1;
}

UINT32 MaxwellBlocklinearTextureHeader::BorderSize() const
{
    UINT32 borderSize = DRF_VAL_MW(B097, _TEXHEAD_BL, _BORDER_SIZE, m_State->m_Data);

    switch (borderSize)
    {
        case LWB097_TEXHEAD_BL_BORDER_SIZE_BORDER_SAMPLER_COLOR:
            return 0;
        case LWB097_TEXHEAD_BL_BORDER_SIZE_BORDER_SIZE_ONE:
            return 1;
        case LWB097_TEXHEAD_BL_BORDER_SIZE_BORDER_SIZE_TWO:
            return 2;
        case LWB097_TEXHEAD_BL_BORDER_SIZE_BORDER_SIZE_FOUR:
            return 4;
        case LWB097_TEXHEAD_BL_BORDER_SIZE_BORDER_SIZE_EIGHT:
            return 8;

        default:
            ErrPrintf("Unrecognized border size %u\n", borderSize);
            MASSERT(0);
            return 0;
    }
}

UINT32 MaxwellBlocklinearTextureHeader::TexturePitch() const
{
    MASSERT(!"Can't get pitch value for MaxwellBlocklinearTextureHeader");
    return 0;
}

UINT32 MaxwellBlocklinearTextureHeader::MaxMipLevel() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_BL, _MAX_MIP_LEVEL, m_State->m_Data);
}

void MaxwellBlocklinearTextureHeader::ReplaceTextureAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    // Make sure the address is aligned properly.
    MASSERT((addressLsb & 0x1FF) == 0);
    addressLsb >>= 9;

    // Make sure the address doesn't exceed the maximum number of bits.
    MASSERT((addressMsb & 0xFFFF0000) == 0);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BL, _ADDRESS_BITS31TO9, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BL, _ADDRESS_BITS47TO32, addressMsb, m_State->m_Data);
}

void MaxwellBlocklinearTextureHeader::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    switch (surface->GetLogBlockWidth())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_WIDTH, _ONE_GOB, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block width");
    }

    switch (surface->GetLogBlockHeight())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _ONE_GOB, m_State->m_Data);
            break;

        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _TWO_GOBS, m_State->m_Data);
            break;

        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _FOUR_GOBS, m_State->m_Data);
            break;

        case 3:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _EIGHT_GOBS, m_State->m_Data);
            break;

        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _SIXTEEN_GOBS, m_State->m_Data);
            break;

        case 5:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_HEIGHT, _THIRTYTWO_GOBS, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block height");
    }

    switch (surface->GetLogBlockDepth())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _ONE_GOB, m_State->m_Data);
            break;

        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _TWO_GOBS, m_State->m_Data);
            break;

        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _FOUR_GOBS, m_State->m_Data);
            break;

        case 3:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _EIGHT_GOBS, m_State->m_Data);
            break;

        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _SIXTEEN_GOBS, m_State->m_Data);
            break;

        case 5:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _GOBS_PER_BLOCK_DEPTH, _THIRTYTWO_GOBS, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block depth");
    }

    BlockShrinkingSanityCheck(surface, gpuDevice);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BL, _WIDTH_MINUS_ONE, surface->GetWidth() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BL, _HEIGHT_MINUS_ONE, surface->GetHeight() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BL, _DEPTH_MINUS_ONE, surface->GetDepth() - 1, m_State->m_Data);
}

RC MaxwellBlocklinearTextureHeader::RelocAAMode(MdiagSurf* surface, bool centerSpoof)
{
    switch (GetAAModeFromSurface(surface))
    {
        case AAMODE_1X1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_1X1, m_State->m_Data);
            break;

        case AAMODE_2X1:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X1_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X1, m_State->m_Data);
            }
            break;

        case AAMODE_2X2:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X2_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X2, m_State->m_Data);
            }
            break;

        case AAMODE_4X2:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2, m_State->m_Data);
            }
            break;

        case AAMODE_4X4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_4X4, m_State->m_Data);
            break;

        case AAMODE_2X1_D3D:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X1_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X1_D3D, m_State->m_Data);
            }
            break;

        case AAMODE_4X2_D3D:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_D3D, m_State->m_Data);
            }
            break;

        case AAMODE_2X2_VC_4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X2_VC_4, m_State->m_Data);
            break;

        case AAMODE_2X2_VC_12:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_2X2_VC_12, m_State->m_Data);
            break;

        case AAMODE_4X2_VC_8:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_VC_8, m_State->m_Data);
            break;

        case AAMODE_4X2_VC_24:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _MULTI_SAMPLE_COUNT, _MODE_4X2_VC_24, m_State->m_Data);
            break;

        default:
            ErrPrintf("Unknown AA Mode %u\n", GetAAModeFromSurface(surface));
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

/////

void MaxwellBlocklinearColorkeyTextureHeader::FixBlockSize() const
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
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_WIDTH, _ONE_GOB, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block width");
    }

    switch (block_size[1])
    {
        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _ONE_GOB, m_State->m_Data);
            break;
        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _TWO_GOBS, m_State->m_Data);
            break;
        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _FOUR_GOBS, m_State->m_Data);
            break;
        case 8:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _EIGHT_GOBS, m_State->m_Data);
            break;
        case 16:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _SIXTEEN_GOBS, m_State->m_Data);
            break;
        case 32:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _THIRTYTWO_GOBS, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block height");
    }

    switch (block_size[2])
    {
        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _ONE_GOB, m_State->m_Data);
            break;
        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _TWO_GOBS, m_State->m_Data);
            break;
        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _FOUR_GOBS, m_State->m_Data);
            break;
        case 8:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _EIGHT_GOBS, m_State->m_Data);
            break;
        case 16:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _SIXTEEN_GOBS, m_State->m_Data);
            break;
        case 32:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _THIRTYTWO_GOBS, m_State->m_Data);
            break;
        default:
            MASSERT(!"Wrong block depth");
    }
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::BlockWidth() const
{
    switch (DRF_VAL_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_WIDTH, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_WIDTH_ONE_GOB:
            return 1;
        default:
            MASSERT(!"Unexpected block width\n");
    }

    return 0;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::BlockHeight() const
{
    switch (DRF_VAL_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_HEIGHT_ONE_GOB:
            return 1;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_HEIGHT_TWO_GOBS:
            return 2;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_HEIGHT_FOUR_GOBS:
            return 4;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_HEIGHT_EIGHT_GOBS:
            return 8;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_HEIGHT_SIXTEEN_GOBS:
            return 16;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_HEIGHT_THIRTYTWO_GOBS:
            return 32;
        default:
            MASSERT(!"Unexpected block height\n");
    }

    return 0;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::BlockDepth() const
{
    switch (DRF_VAL_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_DEPTH_ONE_GOB:
            return 1;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_DEPTH_TWO_GOBS:
            return 2;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_DEPTH_FOUR_GOBS:
            return 4;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_DEPTH_EIGHT_GOBS:
            return 8;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_DEPTH_SIXTEEN_GOBS:
            return 16;
        case LWB097_TEXHEAD_BLCK_GOBS_PER_BLOCK_DEPTH_THIRTYTWO_GOBS:
            return 32;
        default:
            MASSERT(!"Unexpected block depth\n");
    }

    return 0;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::TextureWidth() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_BLCK, _WIDTH_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::TextureHeight() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_BLCK, _HEIGHT_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::TextureDepth() const
{
    // The depth field is used as the array size for arrays, so return 1 here.
    switch (DRF_VAL_MW(B097, _TEXHEAD_BLCK, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 1;
    }

    return DRF_VAL_MW(B097, _TEXHEAD_BLCK, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::ArraySize() const
{
    // The depth field is used as the array size for arrays.
    switch (DRF_VAL_MW(B097, _TEXHEAD_BLCK, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_BLCK_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return DRF_VAL_MW(B097, _TEXHEAD_BLCK, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
    }

    return 1;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::BorderSize() const
{
    UINT32 borderSize = DRF_VAL_MW(B097, _TEXHEAD_BLCK, _BORDER_SIZE, m_State->m_Data);

    switch (borderSize)
    {
        case LWB097_TEXHEAD_BLCK_BORDER_SIZE_BORDER_SAMPLER_COLOR:
            return 0;
        case LWB097_TEXHEAD_BLCK_BORDER_SIZE_BORDER_SIZE_ONE:
            return 1;
        case LWB097_TEXHEAD_BLCK_BORDER_SIZE_BORDER_SIZE_TWO:
            return 2;
        case LWB097_TEXHEAD_BLCK_BORDER_SIZE_BORDER_SIZE_FOUR:
            return 4;
        case LWB097_TEXHEAD_BLCK_BORDER_SIZE_BORDER_SIZE_EIGHT:
            return 8;

        default:
            ErrPrintf("Unrecognized border size %u\n", borderSize);
            MASSERT(0);
            return 0;
    }
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::TexturePitch() const
{
    MASSERT(!"Can't get pitch value for MaxwellBlocklinearColorkeyTextureHeader");
    return 0;
}

UINT32 MaxwellBlocklinearColorkeyTextureHeader::MaxMipLevel() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_BLCK, _MAX_MIP_LEVEL, m_State->m_Data);
}

void MaxwellBlocklinearColorkeyTextureHeader::ReplaceTextureAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    // Make sure the address is aligned properly.
    MASSERT((addressLsb & 0x1FF) == 0);
    addressLsb >>= 9;

    // Make sure the address doesn't exceed the maximum number of bits.
    MASSERT((addressMsb & 0xFFFF0000) == 0);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BLCK, _ADDRESS_BITS31TO9, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BLCK, _ADDRESS_BITS47TO32, addressMsb, m_State->m_Data);
}

void MaxwellBlocklinearColorkeyTextureHeader::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    switch (surface->GetLogBlockWidth())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_WIDTH, _ONE_GOB, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block width");
    }

    switch (surface->GetLogBlockHeight())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _ONE_GOB, m_State->m_Data);
            break;

        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _TWO_GOBS, m_State->m_Data);
            break;

        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _FOUR_GOBS, m_State->m_Data);
            break;

        case 3:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _EIGHT_GOBS, m_State->m_Data);
            break;

        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _SIXTEEN_GOBS, m_State->m_Data);
            break;

        case 5:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_HEIGHT, _THIRTYTWO_GOBS, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block height");
    }

    switch (surface->GetLogBlockDepth())
    {
        case 0:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _ONE_GOB, m_State->m_Data);
            break;

        case 1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _TWO_GOBS, m_State->m_Data);
            break;

        case 2:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _FOUR_GOBS, m_State->m_Data);
            break;

        case 3:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _EIGHT_GOBS, m_State->m_Data);
            break;

        case 4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _SIXTEEN_GOBS, m_State->m_Data);
            break;

        case 5:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BLCK, _GOBS_PER_BLOCK_DEPTH, _THIRTYTWO_GOBS, m_State->m_Data);
            break;

        default:
            MASSERT(!"unknown block depth");
    }

    BlockShrinkingSanityCheck(surface, gpuDevice);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BLCK, _WIDTH_MINUS_ONE, surface->GetWidth() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BLCK, _HEIGHT_MINUS_ONE, surface->GetHeight() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_BLCK, _DEPTH_MINUS_ONE, surface->GetDepth() - 1, m_State->m_Data);
}

RC MaxwellBlocklinearColorkeyTextureHeader::RelocAAMode(MdiagSurf* surface, bool centerSpoof)
{
    if (AAMODE_1X1 != GetAAModeFromSurface(surface))
    {
        ErrPrintf("The blocklinear color key texture header does not support multi-sampling.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

/////

UINT32 MaxwellOneDBufferTextureHeader::TextureWidth() const
{
    UINT32 width = DRF_VAL_MW(B097, _TEXHEAD_1D, _WIDTH_MINUS_ONE_BITS31TO16, m_State->m_Data) << 16;
    width += DRF_VAL_MW(B097, _TEXHEAD_1D, _WIDTH_MINUS_ONE_BITS15TO0, m_State->m_Data);
    width += 1;

    return width;
}

UINT32 MaxwellOneDBufferTextureHeader::TexturePitch() const
{
    return 0;
}

void MaxwellOneDBufferTextureHeader::ReplaceTextureAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    // Make sure the address doesn't exceed the maximum number of bits.
    MASSERT((addressMsb & 0xFFFF0000) == 0);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_1D, _ADDRESS_BITS31TO0, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_1D, _ADDRESS_BITS47TO32, addressMsb, m_State->m_Data);
}

void MaxwellOneDBufferTextureHeader::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    UINT32 widthLsb = (surface->GetWidth() - 1) & 0xFF;
    UINT32 widthMsb = (surface->GetWidth() - 1) >> 8;

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_1D, _WIDTH_MINUS_ONE_BITS15TO0, widthLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_1D, _WIDTH_MINUS_ONE_BITS31TO16, widthMsb, m_State->m_Data);
}

RC MaxwellOneDBufferTextureHeader::RelocAAMode(MdiagSurf* surface, bool centerSpoof)
{
    if (AAMODE_1X1 != GetAAModeFromSurface(surface))
    {
        ErrPrintf("The One-D buffer texture header does not support multi-sampling.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

/////

UINT32 MaxwellPitchTextureHeader::GetMultiSampleAttr() const
{
    UINT32 attr = 0;
    UINT32 multiSampleCount = DRF_VAL_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_1X1:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1_CENTER:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _2);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_CENTER:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_CENTER:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X4:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _16);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_8);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _4_VIRTUAL_16);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_16);
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            attr |= DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _8_VIRTUAL_32);
            break;

        default:
            MASSERT(!"Unknown AA mode");
    }

    return attr;
}

UINT32 MaxwellPitchTextureHeader::TextureWidth() const
{
    UINT32 width = DRF_VAL_MW(B097, _TEXHEAD_PITCH, _WIDTH_MINUS_ONE, m_State->m_Data) + 1;
    UINT32 multiSampleCount = DRF_VAL_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_1X1:
            // No width scaling due to multi-sampling
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1_CENTER:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_CENTER:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
            width *= 2;
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_CENTER:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X4:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            width *= 4;
            break;

        default:
            MASSERT(!"Unknown AA mode");
    }

    return width;
}

UINT32 MaxwellPitchTextureHeader::TextureHeight() const
{
    UINT32 height = DRF_VAL_MW(B097, _TEXHEAD_PITCH, _HEIGHT_MINUS_ONE, m_State->m_Data) + 1;
    UINT32 multiSampleCount = DRF_VAL_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, m_State->m_Data);

    switch (multiSampleCount)
    {
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_1X1:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1_D3D:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X1_CENTER:
            // No height scaling due to multi-sampling
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_CENTER:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_VC_4:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_2X2_VC_12:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_D3D:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_CENTER:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_VC_8:
        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X2_VC_24:
            height *= 2;
            break;

        case LWB097_TEXHEAD_PITCH_MULTI_SAMPLE_COUNT_MODE_4X4:
            height *= 4;
            break;

        default:
            MASSERT(!"Unknown AA mode");
    }

    return height;
}

UINT32 MaxwellPitchTextureHeader::TextureDepth() const
{
    // The depth field is used as the array size for arrays, so return 1 here.
    switch (DRF_VAL_MW(B097, _TEXHEAD_PITCH, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_PITCH_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_PITCH_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_PITCH_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 1;
    }

    return DRF_VAL_MW(B097, _TEXHEAD_PITCH, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellPitchTextureHeader::ArraySize() const
{
    // The depth field is used as the array size for arrays.
    switch (DRF_VAL_MW(B097, _TEXHEAD_PITCH, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_PITCH_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_PITCH_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_PITCH_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return DRF_VAL_MW(B097, _TEXHEAD_PITCH, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
    }

    return 1;
}

UINT32 MaxwellPitchTextureHeader::BorderSize() const
{
    UINT32 borderSize = DRF_VAL_MW(B097, _TEXHEAD_PITCH, _BORDER_SIZE, m_State->m_Data);

    switch (borderSize)
    {
        case LWB097_TEXHEAD_PITCH_BORDER_SIZE_BORDER_SAMPLER_COLOR:
            return 0;
        case LWB097_TEXHEAD_PITCH_BORDER_SIZE_BORDER_SIZE_ONE:
            return 1;
        case LWB097_TEXHEAD_PITCH_BORDER_SIZE_BORDER_SIZE_TWO:
            return 2;
        case LWB097_TEXHEAD_PITCH_BORDER_SIZE_BORDER_SIZE_FOUR:
            return 4;
        case LWB097_TEXHEAD_PITCH_BORDER_SIZE_BORDER_SIZE_EIGHT:
            return 8;

        default:
            ErrPrintf("Unrecognized border size %u\n", borderSize);
            MASSERT(0);
            return 0;
    }
}

UINT32 MaxwellPitchTextureHeader::TexturePitch() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_PITCH, _PITCH_BITS20TO5, m_State->m_Data) << 5;
}

UINT32 MaxwellPitchTextureHeader::MaxMipLevel() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_PITCH, _MAX_MIP_LEVEL, m_State->m_Data);
}

void MaxwellPitchTextureHeader::ReplaceTextureAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    // Make sure the address is aligned properly.
    MASSERT((addressLsb & 0x1F) == 0);
    addressLsb >>= 5;

    // Make sure the address doesn't exceed the maximum number of bits.
    MASSERT((addressMsb & 0xFFFF0000) == 0);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCH, _ADDRESS_BITS31TO5, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCH, _ADDRESS_BITS47TO32, addressMsb, m_State->m_Data);
}

void MaxwellPitchTextureHeader::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    UINT32 pitch = surface->GetPitch();
    // Make sure the pitch is aligned correctly;
    MASSERT((pitch & 0x1F) == 0);
    pitch >>= 5;
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCH, _PITCH_BITS20TO5, pitch, m_State->m_Data);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCH, _WIDTH_MINUS_ONE, surface->GetWidth() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCH, _HEIGHT_MINUS_ONE, surface->GetHeight() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCH, _DEPTH_MINUS_ONE, surface->GetDepth() - 1, m_State->m_Data);
}

RC MaxwellPitchTextureHeader::RelocAAMode(MdiagSurf* surface, bool centerSpoof)
{
    switch (GetAAModeFromSurface(surface))
    {
        case AAMODE_1X1:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_1X1, m_State->m_Data);
            break;

        case AAMODE_2X1:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X1_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X1, m_State->m_Data);
            }
            break;

        case AAMODE_2X2:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X2_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X2, m_State->m_Data);
            }
            break;

        case AAMODE_4X2:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_4X2_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_4X2, m_State->m_Data);
            }
            break;

        case AAMODE_4X4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_4X4, m_State->m_Data);
            break;

        case AAMODE_2X1_D3D:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X1_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X1_D3D, m_State->m_Data);
            }
            break;

        case AAMODE_4X2_D3D:
            if (centerSpoof)
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_4X2_CENTER, m_State->m_Data);
            }
            else
            {
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_4X2_D3D, m_State->m_Data);
            }
            break;

        case AAMODE_2X2_VC_4:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X2_VC_4, m_State->m_Data);
            break;

        case AAMODE_2X2_VC_12:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_2X2_VC_12, m_State->m_Data);
            break;

        case AAMODE_4X2_VC_8:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_4X2_VC_8, m_State->m_Data);
            break;

        case AAMODE_4X2_VC_24:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_PITCH, _MULTI_SAMPLE_COUNT, _MODE_4X2_VC_24, m_State->m_Data);
            break;

        default:
            ErrPrintf("Unknown AA Mode %u\n", GetAAModeFromSurface(surface));
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

/////

UINT32 MaxwellPitchColorkeyTextureHeader::TextureWidth() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _WIDTH_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellPitchColorkeyTextureHeader::TextureHeight() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _HEIGHT_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellPitchColorkeyTextureHeader::TextureDepth() const
{
    // The depth field is used as the array size for arrays, so return 1 here.
    switch (DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_PITCHCK_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_PITCHCK_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_PITCHCK_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return 1;
    }

    return DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
}

UINT32 MaxwellPitchColorkeyTextureHeader::ArraySize() const
{
    // The depth field is used as the array size for arrays.
    switch (DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _TEXTURE_TYPE, m_State->m_Data))
    {
        case LWB097_TEXHEAD_PITCHCK_TEXTURE_TYPE_ONE_D_ARRAY:
        case LWB097_TEXHEAD_PITCHCK_TEXTURE_TYPE_TWO_D_ARRAY:
        case LWB097_TEXHEAD_PITCHCK_TEXTURE_TYPE_LWBEMAP_ARRAY:
            return DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _DEPTH_MINUS_ONE, m_State->m_Data) + 1;
    }

    return 1;
}

UINT32 MaxwellPitchColorkeyTextureHeader::BorderSize() const
{
    UINT32 borderSize = DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _BORDER_SIZE, m_State->m_Data);

    switch (borderSize)
    {
        case LWB097_TEXHEAD_PITCHCK_BORDER_SIZE_BORDER_SAMPLER_COLOR:
            return 0;
        case LWB097_TEXHEAD_PITCHCK_BORDER_SIZE_BORDER_SIZE_ONE:
            return 1;
        case LWB097_TEXHEAD_PITCHCK_BORDER_SIZE_BORDER_SIZE_TWO:
            return 2;
        case LWB097_TEXHEAD_PITCHCK_BORDER_SIZE_BORDER_SIZE_FOUR:
            return 4;
        case LWB097_TEXHEAD_PITCHCK_BORDER_SIZE_BORDER_SIZE_EIGHT:
            return 8;

        default:
            ErrPrintf("Unrecognized border size %u\n", borderSize);
            MASSERT(0);
            return 0;
    }
}

UINT32 MaxwellPitchColorkeyTextureHeader::TexturePitch() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _PITCH_BITS20TO5, m_State->m_Data) << 5;
}

UINT32 MaxwellPitchColorkeyTextureHeader::MaxMipLevel() const
{
    return DRF_VAL_MW(B097, _TEXHEAD_PITCHCK, _MAX_MIP_LEVEL, m_State->m_Data);
}

void MaxwellPitchColorkeyTextureHeader::ReplaceTextureAddress(UINT64 address)
{
    UINT32 addressLsb = LwU64_LO32(address);
    UINT32 addressMsb = LwU64_HI32(address);

    // Make sure the address is aligned properly.
    MASSERT((addressLsb & 0x1F) == 0);
    addressLsb >>= 5;

    // Make sure the address doesn't exceed the maximum number of bits.
    MASSERT((addressMsb & 0xFFFF0000) == 0);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCHCK, _ADDRESS_BITS31TO5, addressLsb, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCHCK, _ADDRESS_BITS47TO32, addressMsb, m_State->m_Data);
}

void MaxwellPitchColorkeyTextureHeader::RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice)
{
    UINT32 pitch = surface->GetPitch();
    // Make sure the pitch is aligned correctly;
    MASSERT((pitch & 0x1F) == 0);
    pitch >>= 5;
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCHCK, _PITCH_BITS20TO5, pitch, m_State->m_Data);

    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCHCK, _WIDTH_MINUS_ONE, surface->GetWidth() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCHCK, _HEIGHT_MINUS_ONE, surface->GetHeight() - 1, m_State->m_Data);
    FLD_SET_DRF_NUM_MW(B097, _TEXHEAD_PITCHCK, _DEPTH_MINUS_ONE, surface->GetDepth() - 1, m_State->m_Data);
}

RC MaxwellPitchColorkeyTextureHeader::RelocAAMode(MdiagSurf* surface, bool centerSpoof)
{
    if (AAMODE_1X1 != GetAAModeFromSurface(surface))
    {
        ErrPrintf("The pitch color key texture header does not support multi-sampling.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}
