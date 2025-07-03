/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013,2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "mdiag/tests/stdtest.h"

#include "verif2d.h"

#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/chanwmgr.h"
#include "mdiag/utils/gr_random.h"
#include "class/cl9097.h" // FERMI_A
#include "ctrl/ctrl0080/ctrl0080fb.h" // LW0080_CTRL_DMA_FLUSH_PARAMS

#include "mdiag/resource/lwgpu/dmard.h"
#include "mdiag/resource/lwgpu/gpucrccalc.h"

Verif2d::Verif2d( LWGpuChannel *ch, LWGpuResource *lwgpu,
                  Test *test, ArgReader *params)
{
    m_ch = ch;
    m_lwgpu = lwgpu;
    m_test = test;
    m_dstSurface = 0;
    m_srcSurface = 0;
    m_gpu = 0;
    m_lastSubchAllocObj = 0;
    m_grRandom = 0;
    m_buf = 0;
    m_subchannelChangeCount = 0;
    m_lastSubchannel = 0xffff;  // illegal value
    m_imageDumpFormat = V2dSurface::NONE;
    m_crcMode = CRC_MODE_NOTHING;
    m_crcProfile = NULL;
    m_methodWriteCount = 0;
    m_sectorPromotion = "none";
    m_dstFlags = V2dSurface::LINEAR;
    m_pParams = params;
    m_pDmaReader = 0;
    m_pGpuCrcCallwlator = 0;
}

Verif2d::Verif2d( LWGpuChannel *ch, LWGpuResource *lwgpu,
                  V2dSurface::Flags flags,
                  Test *test, ArgReader *pParams)
{
    m_ch = ch;
    m_lwgpu = lwgpu;
    m_test = test;
    m_dstSurface = 0;
    m_srcSurface = 0;
    m_gpu = 0;
    m_lastSubchAllocObj = 0;
    m_grRandom = 0;
    m_buf = 0;
    m_subchannelChangeCount = 0;
    m_lastSubchannel = 0xffff;  // illegal value
    m_imageDumpFormat = V2dSurface::NONE;
    m_crcMode = CRC_MODE_NOTHING;
    m_crcProfile = NULL;
    m_methodWriteCount = 0;
    m_sectorPromotion = "none";
    m_dstFlags = flags;
    m_pParams = pParams;
    m_pDmaReader = 0;
    m_pGpuCrcCallwlator = 0;
}

Verif2d::~Verif2d( void )
{
    delete m_dstSurface;
    if ( m_srcSurface != m_dstSurface )
        delete m_srcSurface;

    for (unsigned int i=0; i<m_surfaces.size(); i++)
        delete m_surfaces[i];

    if (m_pDmaReader)
        delete m_pDmaReader;

    if (m_pGpuCrcCallwlator)
        delete m_pGpuCrcCallwlator;
}

void Verif2d::InitSurfStringMaps( void )
{
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_AF32 ] = "RF32_GF32_BF32_AF32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_AS32 ] = "RS32_GS32_BS32_AS32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_AU32 ] = "RU32_GU32_BU32_AU32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_X32 ]  = "RF32_GF32_BF32_X32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_X32 ]  = "RS32_GS32_BS32_X32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_X32 ]  = "RU32_GU32_BU32_X32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16_B16_A16 ]     = "R16_G16_B16_A16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16_BN16_AN16 ] = "RN16_GN16_BN16_AN16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16_BS16_AS16 ] = "RS16_GS16_BS16_AS16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16_BU16_AU16 ] = "RU16_GU16_BU16_AU16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_AF16 ] = "RF16_GF16_BF16_AF16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32 ]           = "RF32_GF32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32 ]           = "RS32_GS32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32 ]           = "RU32_GU32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_X16 ]  = "RF16_GF16_BF16_X16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A8R8G8B8 ]            = "A8R8G8B8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A8RL8GL8BL8 ]         = "A8RL8GL8BL8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A2B10G10R10 ]         = "A2B10G10R10";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_AU2BU10GU10RU10 ]     = "AU2BU10GU10RU10";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A8B8G8R8 ]            = "A8B8G8R8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A8BL8GL8RL8 ]         = "A8BL8GL8RL8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_AN8BN8GN8RN8 ]        = "AN8BN8GN8RN8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_AS8BS8GS8RS8 ]        = "AS8BS8GS8RS8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_AU8BU8GU8RU8 ]        = "AU8BU8GU8RU8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16 ]             = "R16_G16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16 ]           = "RN16_GN16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16 ]           = "RS16_GS16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16 ]           = "RU16_GU16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16 ]           = "RF16_GF16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A2R10G10B10 ]         = "A2R10G10B10";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_BF10GF11RF11 ]        = "BF10GF11RF11";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS32 ]                = "RS32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU32 ]                = "RU32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF32 ]                = "RF32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_X8R8G8B8 ]            = "X8R8G8B8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_X8RL8GL8BL8 ]         = "X8RL8GL8BL8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_X8B8G8R8 ]            = "X8B8G8R8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_X8BL8GL8RL8 ]         = "X8BL8GL8RL8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_R32 ]                 = "R32";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_R5G6B5 ]              = "R5G6B5";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A1R5G5B5 ]            = "A1R5G5B5";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_G8R8 ]                = "G8R8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_GN8RN8 ]              = "GN8RN8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_GS8RS8 ]              = "GS8RS8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_GU8RU8 ]              = "GU8RU8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_R16 ]                 = "R16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RN16 ]                = "RN16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS16 ]                = "RS16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU16 ]                = "RU16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RF16 ]                = "RF16";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_X1R5G5B5 ]            = "X1R5G5B5";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_Z1R5G5B5 ]            = "Z1R5G5B5";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_O1R5G5B5 ]            = "O1R5G5B5";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_R8 ]                  = "R8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RN8 ]                 = "RN8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RS8 ]                 = "RS8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_RU8 ]                 = "RU8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_A8 ]                  = "A8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_Z8R8G8B8 ]            = "Z8R8G8B8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_O8R8G8B8 ]            = "O8R8G8B8";
    m_surf3d_C_enum_to_string[ LW9097_SET_COLOR_TARGET_FORMAT_V_DISABLED ]            = "DISABLED";

    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_Z24S8 ]               = "Z24S8";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_X8Z24 ]               = "X8Z24";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_S8Z24 ]               = "S8Z24";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_V8Z24 ]               = "V8Z24";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_ZF32  ]               = "ZF32";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_ZF32_X24S8 ]          = "ZF32_X24S8";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_X8Z24_X16V8S8 ]       = "X8Z24_X16V8S8";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_ZF32_X16V8X8 ]        = "ZF32_X16V8X8";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_ZF32_X16V8S8 ]        = "ZF32_X16V8S8";
    m_surf3d_Z_enum_to_string[ LW9097_SET_ZT_FORMAT_V_Z16 ]                 = "Z16";

    m_SurfStringMap[ "RF32_GF32_BF32_AF32" ] = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_AF32;
    m_SurfStringMap[ "RS32_GS32_BS32_AS32" ] = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_AS32;
    m_SurfStringMap[ "RU32_GU32_BU32_AU32" ] = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_AU32;
    m_SurfStringMap[ "RF32_GF32_BF32_X32" ]  = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_X32;
    m_SurfStringMap[ "RS32_GS32_BS32_X32" ]  = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_X32;
    m_SurfStringMap[ "RU32_GU32_BU32_X32" ]  = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_X32;
    m_SurfStringMap[ "R16_G16_B16_A16" ]     = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16_B16_A16;
    m_SurfStringMap[ "RN16_GN16_BN16_AN16" ] = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16_BN16_AN16;
    m_SurfStringMap[ "RS16_GS16_BS16_AS16" ] = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16_BS16_AS16;
    m_SurfStringMap[ "RU16_GU16_BU16_AU16" ] = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16_BU16_AU16;
    m_SurfStringMap[ "RF16_GF16_BF16_AF16" ] = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_AF16;
    m_SurfStringMap[ "RF32_GF32" ]           = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32;
    m_SurfStringMap[ "RS32_GS32" ]           = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32;
    m_SurfStringMap[ "RU32_GU32" ]           = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32;
    m_SurfStringMap[ "RF16_GF16_BF16_X16" ]  = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_X16;
    m_SurfStringMap[ "A8R8G8B8" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A8R8G8B8;
    m_SurfStringMap[ "A8RL8GL8BL8" ]         = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A8RL8GL8BL8;
    m_SurfStringMap[ "A2B10G10R10" ]         = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A2B10G10R10;
    m_SurfStringMap[ "AU2BU10GU10RU10" ]     = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_AU2BU10GU10RU10;
    m_SurfStringMap[ "A8B8G8R8" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A8B8G8R8;
    m_SurfStringMap[ "A8BL8GL8RL8" ]         = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A8BL8GL8RL8;
    m_SurfStringMap[ "AN8BN8GN8RN8" ]        = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_AN8BN8GN8RN8;
    m_SurfStringMap[ "AS8BS8GS8RS8" ]        = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_AS8BS8GS8RS8;
    m_SurfStringMap[ "AU8BU8GU8RU8" ]        = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_AU8BU8GU8RU8;
    m_SurfStringMap[ "R16_G16" ]             = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16;
    m_SurfStringMap[ "RN16_GN16" ]           = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16;
    m_SurfStringMap[ "RS16_GS16" ]           = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16;
    m_SurfStringMap[ "RU16_GU16" ]           = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16;
    m_SurfStringMap[ "RF16_GF16" ]           = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16;
    m_SurfStringMap[ "A2R10G10B10" ]         = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A2R10G10B10;
    m_SurfStringMap[ "BF10GF11RF11" ]        = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_BF10GF11RF11;
    m_SurfStringMap[ "RS32" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS32;
    m_SurfStringMap[ "RU32" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU32;
    m_SurfStringMap[ "RF32" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF32;
    m_SurfStringMap[ "X8R8G8B8" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_X8R8G8B8;
    m_SurfStringMap[ "X8RL8GL8BL8" ]         = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_X8RL8GL8BL8;
    m_SurfStringMap[ "X8B8G8R8" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_X8B8G8R8;
    m_SurfStringMap[ "X8BL8GL8RL8" ]         = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_X8BL8GL8RL8;
    m_SurfStringMap[ "R32" ]                 = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_R32;
    m_SurfStringMap[ "R5G6B5" ]              = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_R5G6B5;
    m_SurfStringMap[ "A1R5G5B5" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A1R5G5B5;
    m_SurfStringMap[ "G8R8" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_G8R8;
    m_SurfStringMap[ "GN8RN8" ]              = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_GN8RN8;
    m_SurfStringMap[ "GS8RS8" ]              = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_GS8RS8;
    m_SurfStringMap[ "GU8RU8" ]              = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_GU8RU8;
    m_SurfStringMap[ "R16" ]                 = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_R16;
    m_SurfStringMap[ "RN16" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RN16;
    m_SurfStringMap[ "RS16" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS16;
    m_SurfStringMap[ "RU16" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU16;
    m_SurfStringMap[ "RF16" ]                = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RF16;
    m_SurfStringMap[ "X1R5G5B5" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_X1R5G5B5;
    m_SurfStringMap[ "Z1R5G5B5" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_Z1R5G5B5;
    m_SurfStringMap[ "O1R5G5B5" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_O1R5G5B5;
    m_SurfStringMap[ "R8" ]                  = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_R8;
    m_SurfStringMap[ "RN8" ]                 = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RN8;
    m_SurfStringMap[ "RS8" ]                 = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RS8;
    m_SurfStringMap[ "RU8" ]                 = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_RU8;
    m_SurfStringMap[ "A8" ]                  = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_A8;
    m_SurfStringMap[ "Z8R8G8B8" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_Z8R8G8B8;
    m_SurfStringMap[ "O8R8G8B8" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_O8R8G8B8;
    m_SurfStringMap[ "DISABLED" ]            = (enum V2dSurface::ColorFormat) LW9097_SET_COLOR_TARGET_FORMAT_V_DISABLED;

    m_SurfStringMap[ "Z24S8" ]               = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_Z24S8 + 0x100);
    m_SurfStringMap[ "X8Z24" ]               = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_X8Z24 + 0x100);
    m_SurfStringMap[ "S8Z24" ]               = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_S8Z24 + 0x100);
    m_SurfStringMap[ "V8Z24" ]               = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_V8Z24 + 0x100);
    m_SurfStringMap[ "ZF32"  ]               = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_ZF32  + 0x100);
    m_SurfStringMap[ "ZF32_X24S8" ]          = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_ZF32_X24S8 + 0x100);
    m_SurfStringMap[ "X8Z24_X16V8S8" ]       = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_X8Z24_X16V8S8 + 0x100);
    m_SurfStringMap[ "ZF32_X16V8X8" ]        = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_ZF32_X16V8X8 + 0x100);
    m_SurfStringMap[ "ZF32_X16V8S8" ]        = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_ZF32_X16V8S8 + 0x100);
    m_SurfStringMap[ "Z16" ]                 = (enum V2dSurface::ColorFormat) (LW9097_SET_ZT_FORMAT_V_Z16 + 0x100);

    // these override the above string -> enumerate mapping
    // surface color format:
    m_SurfStringMap[ "Y8" ] = V2dSurface::Y8;
    m_SurfStringMap[ "X1R5G5B5_Z1R5G5B5" ] = V2dSurface::X1R5G5B5_Z1R5G5B5;
    m_SurfStringMap[ "X1R5G5B5_O1R5G5B5" ] = V2dSurface::X1R5G5B5_O1R5G5B5;
    m_SurfStringMap[ "R5G6B5" ] = V2dSurface::R5G6B5;
    m_SurfStringMap[ "Y16" ] = V2dSurface::Y16;
    m_SurfStringMap[ "X8R8G8B8_Z8R8G8B8" ] = V2dSurface::X8R8G8B8_Z8R8G8B8;
    m_SurfStringMap[ "X8R8G8B8_O8R8G8B8" ] = V2dSurface::X8R8G8B8_O8R8G8B8;
    m_SurfStringMap[ "X1A7R8G8B8_Z1A7R8G8B8" ] = V2dSurface::X1A7R8G8B8_Z1A7R8G8B8;
    m_SurfStringMap[ "X1A7R8G8B8_O1A7R8G8B8" ] = V2dSurface::X1A7R8G8B8_O1A7R8G8B8;
    m_SurfStringMap[ "A8R8G8B8" ] = V2dSurface::A8R8G8B8;
    m_SurfStringMap[ "Y32" ] = V2dSurface::Y32;
    m_SurfStringMap[ "X8B8G8R8_Z8B8G8R8" ] = V2dSurface::X8B8G8R8_Z8B8G8R8;
    m_SurfStringMap[ "X8B8G8R8_O8B8G8R8" ] = V2dSurface::X8B8G8R8_O8B8G8R8;
    m_SurfStringMap[ "A8B8G8R8" ] = V2dSurface::A8B8G8R8;
    m_SurfStringMap[ "CR8YB8CB8YA8" ] = V2dSurface::CR8YB8CB8YA8;
    m_SurfStringMap[ "YB8CR8YA8CB8" ] = V2dSurface::YB8CR8YA8CB8;
    m_SurfStringMap[ "A8CR8CB8Y8" ] = V2dSurface::A8CR8CB8Y8;
    m_SurfStringMap[ "A4CR6YB6A4CB6YA6" ] = V2dSurface::A4CR6YB6A4CB6YA6;
    m_SurfStringMap[ "A2R10G10B10" ] = V2dSurface::A2R10G10B10;
    m_SurfStringMap[ "A2B10G10R10" ] = V2dSurface::A2B10G10R10;

    m_SurfStringMap[ "X1R5G5B5" ] = V2dSurface::X1R5G5B5_Z1R5G5B5;
    m_SurfStringMap[ "Z1R5G5B5" ] = V2dSurface::X1R5G5B5_Z1R5G5B5;
    m_SurfStringMap[ "O1R5G5B5" ] = V2dSurface::X1R5G5B5_O1R5G5B5;
    m_SurfStringMap[ "A1R5G5B5" ] = V2dSurface::A1R5G5B5;

    m_SurfStringMap[ "X8B8G8R8" ] = V2dSurface::X8B8G8R8_Z8B8G8R8;
    m_SurfStringMap[ "Z8B8G8R8" ] = V2dSurface::X8B8G8R8_Z8B8G8R8;
    m_SurfStringMap[ "O8B8G8R8" ] = V2dSurface::X8B8G8R8_O8B8G8R8;

    m_SurfStringMap[ "X8R8G8B8" ] = V2dSurface::X8R8G8B8_Z8R8G8B8;
    m_SurfStringMap[ "Z8R8G8B8" ] = V2dSurface::X8R8G8B8_Z8R8G8B8;
    m_SurfStringMap[ "O8R8G8B8" ] = V2dSurface::X8R8G8B8_O8R8G8B8;

    m_SurfStringMap[ "AY8" ] = V2dSurface::AY8;

    m_SurfStringMap[ "Y1_8X8" ] = V2dSurface::Y1_8X8;
    m_SurfStringMap[ "A8RL8GL8BL8" ] = V2dSurface::A8RL8GL8BL8;
    m_SurfStringMap[ "A8BL8GL8RL8" ] = V2dSurface::A8BL8GL8RL8;
    m_SurfStringMap[ "X8RL8GL8BL8" ] = V2dSurface::X8RL8GL8BL8;
    m_SurfStringMap[ "X8BL8GL8RL8" ] = V2dSurface::X8BL8GL8RL8;
}

string Verif2d::get_Csurface_format_string( int surf3d_C_surface_format ) const
{
    std::map<int,string>::const_iterator i = m_surf3d_C_enum_to_string.find( surf3d_C_surface_format );
    if( i == m_surf3d_C_enum_to_string.end() ) {
        ErrPrintf( "Verif2d::get_surface_format_string() surface format not found\n" );
        return "<< Verif2d::get_surface_format_string() surface format not found >>";
    }
    return i->second;
}

string Verif2d::get_Zsurface_format_string( int surf3d_Z_surface_format ) const
{
    std::map<int,string>::const_iterator i = m_surf3d_Z_enum_to_string.find( surf3d_Z_surface_format );
    if( i == m_surf3d_Z_enum_to_string.end() ) {
        ErrPrintf( "Verif2d::get_surface_format_string() surface format not found\n" );
        return "<< Verif2d::get_surface_format_string() surface format not found >>";
    }
    return i->second;
}

void Verif2d::InitClassnameMap( void )
{
    m_classNameMap[ "fermi_twod_a" ] = V2dClassObj::_FERMI_TWOD_A;
    m_classNameMap[ "FERMI_TWOD_A" ] = V2dClassObj::_FERMI_TWOD_A;
}

void Verif2d::Init(V2dGpu::GpuType gpuType)
{
    m_classObjects.clear();
    m_subchannelObjects.clear();

    m_gpu = new V2dGpu;
    m_gpu->SetGpuType( gpuType );

    InitSurfStringMaps();
    InitClassnameMap();

    m_grRandom = new GrRandom;
    m_buf = new Buf;

    m_function.SetV2d( this );
}

//
// new-style (for use from scripts) .   Handles creation of both RenderClass and ContextState objects.
//
V2dClassObj * Verif2d::NewClassObj( string className )
{
    map<string,V2dClassObj::ClassId>::iterator i = m_classNameMap.find( className );
    if ( i == m_classNameMap.end() )
        V2D_THROW( "trying to create an instance of an illegal class name: " << className );

    V2dClassObj::ClassId classId = (*i).second;

    if ( ! m_gpu )
        V2D_THROW( "can't create a class until you've chosen a gpu" );

    V2dClassObj *newObj;

    switch (classId)
    {
    case V2dClassObj::_FERMI_TWOD_A:
        newObj = new V2dTwoD( this );
        break;

    default:
        V2D_THROW( "Verif2d::NewClassObj(): unknown class " << HEX(4,classId) );
        return 0;
    }

    // record classId for future use
    this->m_classId = classId;
    newObj->SetWFIMode((WfiMethod)m_pParams->ParamUnsigned("-wfi_method", WFI_POLL));
    if (newObj->GetWFIMode() == WFI_POLL &&
        ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        V2D_THROW( "Cannot use -wfi_poll with -runlist (RM restriction)" );
    }
    newObj->Init( classId );

    return newObj;
}

void Verif2d::SetSrcRect( UINT32 x, UINT32 y, UINT32 width, UINT32 height )
{
    m_srcRect.x = x;
    m_srcRect.y = y;
    m_srcRect.width = width;
    m_srcRect.height = height;
}

void Verif2d::SetDstRect( UINT32 x, UINT32 y, UINT32 width, UINT32 height )
{
    m_dstRect.x = x;
    m_dstRect.y = y;
    m_dstRect.width = width;
    m_dstRect.height = height;
}

void Verif2d::LoseLastSubchAllocObjSubchannel()
{
    if (m_lastSubchAllocObj)
        m_lastSubchAllocObj->LoseSubChannel();
}

UINT32 Verif2d::AllocSubchannel( V2dClassObj *obj )
{
    if ( m_lastSubchAllocObj && (obj != m_lastSubchAllocObj) )
        m_lastSubchAllocObj->LoseSubChannel();

    UINT32 subch_id = 0;
    if (m_ch->GetSubChannelNumClass(obj->GetHandle(), &subch_id, 0) != OK)
    {
        V2D_THROW("Verif2d::AllocSubchannel: Failed to alloc subchannel num!");
    }

    m_lastSubchAllocObj = obj;
    return (subch_id);
}

void Verif2d::SaveDstSurfaceToTGAfile( string fileName )
{
    m_dstSurface->SaveToTGAfile( fileName, 0 );
}

void Verif2d::ClearDstSurface( string clearPatternStr, UINT32 clearPatternData )
{
    vector<UINT32> data;
    data.push_back( clearPatternData );

    m_dstSurface->Clear( clearPatternStr, data );
}

void Verif2d::FlushGpuCache( void )
{
    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS param;
    memset(&param, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _YES, param.flags);
    if (!m_pParams->ParamPresent("-no_l2_flush"))
    {
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES, param.flags);
        DebugPrintf("Flushing L2...\n");
    }

    LwRm* pLwRm = m_ch->GetLwRmPtr();
    DebugPrintf("Sending UFLUSH...\n");
    GpuDevice *pGpuDev = m_lwgpu->GetGpuDevice();
    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        RC rc = pLwRm->Control(
            pLwRm->GetSubdeviceHandle(pGpuDev->GetSubdevice(subdev)),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

        if (rc != OK)
        {
            V2D_THROW("Failed to flush while WFI");
        }
    }
    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY, param.flags);
    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        RC rc = pLwRm->Control(
            pLwRm->GetSubdeviceHandle(pGpuDev->GetSubdevice(subdev)),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

        if (rc != OK)
        {
            V2D_THROW("Failed to flush while WFI");
        }
    }
}

void Verif2d::WaitForIdle( void )
{
    int rv = m_ch->WaitForIdle();
    if ( ! rv )
        V2D_THROW(" wait for idle failed" );

    FlushGpuCache();    
}

bool Verif2d::CompareDstSurfaceToCRC( string fileName, string crcStr )
{
    UINT32 screenCrc = m_dstSurface->CalcCRC();
    InfoPrintf( "dst surface CRC: 0x%08x\n", screenCrc );

    UINT32 crc;
    sscanf( crcStr.c_str(), "%x", &crc );

    if ( screenCrc != crc )
    {
        m_dstSurface->SaveToTGAfile( fileName, 0 );
        ErrPrintf( "CRC mismatch, expected 0x%08x, got 0x%08x)",
                   crc, screenCrc );
        return false;
    }
    else
    {
        InfoPrintf( "Command-line CRC matches!\n" );
        return true;
    }
}

void Verif2d::SetCtxSurfType( string surfType )
{
    if ( (surfType == "lw04") || (surfType == "LW04") )
        surfType = "lw4";

    if ( (surfType != "") && (surfType != "lw4") && (surfType != "LW4") &&
         (surfType != "lw10") && (surfType != "LW10") && (surfType != "lw15") &&
         (surfType != "LW15") && (surfType != "lw30") && (surfType != "LW30") &&
         (surfType != "newest") && (surfType != "oldest") )
    {
        V2D_THROW( "ctxsurf type " << surfType << " illegal: must be one of lw4, lw10, lw15, lw30, oldest, newest" );
    }
}

//
// is there an existing object with this handle?
//
bool Verif2d::HandleExists( UINT32 handle )
{
    map<UINT32,V2dClassObj*>::iterator i = m_handleToObjectMap.find( handle );
    if ( i == m_handleToObjectMap.end() )
        return false;

    return true;
}

bool Verif2d::HandleExistsSurf( UINT32 handle )
{
    map<UINT32,V2dSurface*>::iterator i = m_handleToObjectMapSurf.find( handle );
    if ( i == m_handleToObjectMapSurf.end() )
        return false;

    return true;
}

void Verif2d::AddHandleObjectPair( UINT32 handle, V2dClassObj *obj )
{
    // add the handle to the handle->obj map.   Make sure another handle of the same value doesn't already exist!
    //
    if ( HandleExists( handle ) == true )
        V2D_THROW( "internal error: adding handle/object pair, handle = " << handle << " already exists" );

    m_handleToObjectMap[ handle ] = obj;
}

void Verif2d::AddHandleObjectPairSurf( UINT32 handle, V2dSurface *obj )
{
    // add the handle to the handle->obj map.   Make sure another handle of the same value doesn't already exist!
    //
    if ( HandleExistsSurf( handle ) == true )
        V2D_THROW( "internal error: adding handle/object surf pair, handle = " << handle << " already exists" );

    m_handleToObjectMapSurf[ handle ] = obj;
}

V2dClassObj * Verif2d::GetObjectFromHandle( UINT32 handle )
{
    map<UINT32,V2dClassObj*>::iterator i = m_handleToObjectMap.find( handle );
    if ( i == m_handleToObjectMap.end() )
        V2D_THROW( "internal error: handle " << handle << " does not exist" );

    V2dClassObj *obj = (*i).second;
    if ( !obj )
        V2D_THROW( "internal error: GetObjectFromHandle: object pointer is null" );

    return obj;
}

V2dSurface * Verif2d::GetObjectFromHandleSurf( UINT32 handle )
{
    map<UINT32,V2dSurface*>::iterator i = m_handleToObjectMapSurf.find( handle );
    if ( i == m_handleToObjectMapSurf.end() )
        V2D_THROW( "internal error: surface handle " << handle << " does not exist" );

    return (*i).second;
}

V2dSurface::ColorFormat Verif2d::GetColorFormatFromString( string format )
{
    map<string, V2dSurface::ColorFormat>::iterator i = m_SurfStringMap.find( format );
    if ( i == m_SurfStringMap.end() )
    {
        string legalFormats="";
        map<string,V2dSurface::ColorFormat>::const_iterator j = m_SurfStringMap.begin();
        for(j=j; j!= m_SurfStringMap.end(); j++)
            {
            legalFormats = legalFormats + (*j).first;
            legalFormats = legalFormats + ",";
            }
        V2D_THROW( "illegal surface format string: unsupported format: " << format <<". Legal formats are"<<legalFormats);

    }
    return (*i).second;
}

// return a 32-bit random number
UINT32 Verif2d::Random( void )
{
    return m_grRandom->LWRandom();
}

void Verif2d::SetImageDumpFormat( V2dSurface::ImageDumpFormat f )
{
    m_imageDumpFormat = f;
}

V2dSurface::ImageDumpFormat Verif2d::GetImageDumpFormat()
{
    return m_imageDumpFormat;
}

DmaReader* Verif2d::GetGpuCrcCallwlator()
{
    if (!m_pGpuCrcCallwlator)
    {

        // create dmaReader
        m_pGpuCrcCallwlator = GpuCrcCallwlator::CreateGpuCrcCallwlator(
            DmaReader::GetWfiType(m_pParams), m_lwgpu,
            m_ch, 4,
            m_pParams, m_ch->GetLwRmPtr(), m_ch->GetSmcEngine(), 0/*subdev*/);

        if (m_pGpuCrcCallwlator == NULL)     {
            ErrPrintf("Error oclwred while allocating GpuCrcCallwlator!\n");
            return 0;
        }

        // allocate notifier
        if (!m_pGpuCrcCallwlator->AllocateNotifier(GetPageLayoutFromParams(m_pParams, NULL)))
        {
            ErrPrintf("Error oclwred while allocating GpuCrcCallwlator notifier!\n");
            return 0;
        }

        // allocate dst dma buffer
        if (m_pGpuCrcCallwlator->AllocateDstBuffer(GetPageLayoutFromParams(m_pParams, NULL), (UINT32)16, _DMA_TARGET_VIDEO) == false)     {
            ErrPrintf("Error oclwred while allocating GpuCrcCallwlator dst buffer!\n");
            return 0;
        }

        // allocate dma reader handles
        if (m_pGpuCrcCallwlator->AllocateObject() == false)
        {
            ErrPrintf("Error oclwred while allocating GpuCrcCallwlator handles!\n");
            return 0;
        }

    }
    return m_pGpuCrcCallwlator;
}

DmaReader* Verif2d::GetDmaReader()
{
    if (!m_pDmaReader)
    {
        m_pDmaReader = DmaReader::CreateDmaReader(
            DmaReader::CE, DmaReader::GetWfiType(m_pParams), m_lwgpu, m_ch,
            1<<18, m_pParams, m_ch->GetLwRmPtr(), m_ch->GetSmcEngine(), 0/*subdev*/ );

        if (m_pDmaReader == NULL)     {
            ErrPrintf("Error oclwred while allocating DmaReader!\n");
            return 0;
        }

        // allocate notifier
        if (!m_pDmaReader->AllocateNotifier(GetPageLayoutFromParams(m_pParams, NULL)))
        {
            ErrPrintf("Error oclwred while allocating DmaReader notifier!\n");
            return 0;
        }

        // allocate dst dma buffer
        if (m_pDmaReader->AllocateDstBuffer(GetPageLayoutFromParams(m_pParams, NULL), 1<<18, _DMA_TARGET_COHERENT) == false)
        {
            ErrPrintf("Error oclwred while allocating DmaReader dst buffer!\n");
            return 0;
        }

        // allocate dma reader handles
        if (m_pDmaReader->AllocateObject() == false)
        {
            ErrPrintf("Error oclwred while allocating DmaReader handles!\n");
            return 0;
        }

        if (GetDoDmaCheckCE() && !GetDoGpuCrcCalc())
        {
            m_pDmaReader->SetCopyLocation(Memory::Coherent);
        }
    }
    return m_pDmaReader;
}
