/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndTexturing: OpenGL Randomizer for textures, texture units, etc.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"
#include "core/include/imagefil.h"
#include "gpu/include/gpusbdev.h"
#include <math.h>       // atan()
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "core/include/utility.h"
#include "core/include/crc.h"
#include "opengl/tests/astccache.h"
#include "core/include/coverage.h"
#include "opengl/mglcoverage.h"
using namespace GLRandom;
using namespace TestCoverage;
//------------------------------------------------------------------------------
// Some const lookup tables, indexed by TexIndex
//------------------------------------------------------------------------------
namespace TexGen
{
    extern const UINT32       hndlIdx[TiNUM_Indicies];
    int AttrToTxHndlIdx(UINT32 Attr);
};

const UINT32 TexGen::hndlIdx[TiNUM_Indicies] =
{
    Is1d             ,
    Is2d             ,
    Is3d             ,
    IsLwbe           ,
    IsArray1d        ,
    IsArray2d        ,
    IsArrayLwbe
};

int TexGen::AttrToTxHndlIdx( UINT32 Attr)
{
    for (int i = Ti1d; i < TiNUM_Indicies; i++)
    {
        if (Attr == TexGen::hndlIdx[i])
            return( i);
    }
    MASSERT(!"Invalid Attr for TxHndlIdx");
    return(0);
}

//------------------------------------------------------------------------------
RndTexturing::RndTexturing(GLRandomTest * pglr)
 :  GLRandomHelper(pglr, RND_TX_NUM_PICKERS, "Texturing")
    ,m_Textures(NULL)
    ,m_NumTextures(0)
    ,m_MaxTxBytes(0)
    ,m_MaxTextures(0)
    ,m_MaxFilterableFloatDepth(16)
    ,m_HasF32BorderColor(false)
{
    memset (&m_Txu, 0, sizeof(m_Txu));
    memset(&m_SparseTextures[0], 0, sizeof(m_SparseTextures));
}

//------------------------------------------------------------------------------
RndTexturing::~RndTexturing()
{
}

//------------------------------------------------------------------------------
// get picker tables from JavaScript
//
RC RndTexturing::SetDefaultPicker(int picker)
{
    if (picker < 0 || picker >= RND_TX_NUM_PICKERS)
        return RC::BAD_PICKER_INDEX;

    FancyPicker & fp = m_Pickers[picker];
    switch (picker)
    {
        case RND_TXU_ENABLE_MASK:
            fp.ConfigRandom();
            // all textures enabled
            fp.AddRandItem(3, 255);
            // random textures enabled
            fp.AddRandRange(1, 0, 255);
            fp.CompileRandom();
            break;

        case RND_TXU_WRAPMODE:
            // Which texture units should we enable?
            // How to handle out-of-range texture accesses in the S direction.
            fp.ConfigRandom();
            fp.AddRandItem(6, GL_REPEAT);
            fp.AddRandItem(4, GL_MIRRORED_REPEAT_IBM);
            fp.AddRandItem(2, GL_CLAMP);
            fp.AddRandItem(2, GL_CLAMP_TO_EDGE);
            fp.AddRandItem(2, GL_CLAMP_TO_BORDER);
            fp.AddRandItem(1, GL_MIRROR_CLAMP_EXT);
            fp.AddRandItem(1, GL_MIRROR_CLAMP_TO_EDGE_EXT);
            fp.AddRandItem(1, GL_MIRROR_CLAMP_TO_BORDER_EXT);
            fp.CompileRandom();
            break;

        case RND_TXU_MIN_FILTER:
            // How to filter many texels down to few pixels.
            fp.ConfigRandom();
            fp.AddRandItem(1, GL_NEAREST);
            fp.AddRandItem(2, GL_LINEAR);
            fp.AddRandItem(1, GL_NEAREST_MIPMAP_NEAREST);
            fp.AddRandItem(1, GL_NEAREST_MIPMAP_LINEAR);
            fp.AddRandItem(1, GL_LINEAR_MIPMAP_NEAREST);
            fp.AddRandItem(3, GL_LINEAR_MIPMAP_LINEAR);
            fp.CompileRandom();
            break;

        case RND_TXU_MAG_FILTER:
            // How to expand few texels up to more pixels.
            fp.ConfigRandom();
            fp.AddRandItem(1, GL_NEAREST);
            fp.AddRandItem(2, GL_LINEAR);
            fp.CompileRandom();
            break;

        // Clamp lambda(mipmap factor) (offset from min mipmap level of texture)
        case RND_TXU_MIN_LAMBDA:
            fp.FConfigRandom();
            fp.FAddRandItem(1, 0.0);
            fp.FAddRandRange(1, 0.0, 1.0);
            fp.CompileRandom();
            break;

        case RND_TXU_MAX_LAMBDA:
            // Clamp lambda (offset from max mipmap level of texture).
            fp.FConfigRandom();
            fp.FAddRandItem(1, 0.0);
            fp.FAddRandRange(1, -1.0, 0.0);
            fp.CompileRandom();
            break;

        case RND_TXU_BIAS_LAMBDA:
            // Offset lambda.
            fp.FConfigRandom();
            fp.FAddRandItem(1, 0.0);
            fp.FAddRandRange(1, -1.0, 1.0);
            fp.CompileRandom();
            break;

        case RND_TXU_MAX_ANISOTROPY:
            // Texture anisotropy
            //(difference in power-of-two minification across polygon).
            fp.FConfigRandom();
            fp.FAddRandItem(8, 1.0);
            fp.FAddRandItem(2, 2.0);
            fp.FAddRandItem(1, 4.0);
            fp.FAddRandItem(1, 6.0);
            fp.FAddRandItem(1, 8.0);
            fp.FAddRandItem(1, 10.0);
            fp.FAddRandItem(1, 12.0);
            fp.FAddRandItem(1, 16.0);
            fp.CompileRandom();
            break;

        case RND_TXU_BORDER_COLOR:
            fp.FConfigRandom();
            fp.FAddRandRange(1, 0.0f, 1.0f);
            fp.CompileRandom();
            break;

        case RND_TXU_BORDER_ALPHA:
            fp.FConfigRandom();
            fp.FAddRandRange(1, 0.0f, 1.0f);
            fp.CompileRandom();
            break;

        case RND_TX_DEPTH_TEX_MODE:
            fp.ConfigRandom();
            fp.AddRandItem(40, GL_LUMINANCE);
            fp.AddRandItem(30, GL_ALPHA);
            fp.AddRandItem(30, GL_INTENSITY);
            fp.CompileRandom();
            break;

        case RND_TXU_COMPARE_MODE:
            fp.ConfigRandom();
            fp.AddRandItem(90, GL_COMPARE_R_TO_TEXTURE);
            fp.AddRandItem(10, GL_NONE);
            fp.CompileRandom();
            break;

        case RND_TXU_SHADOW_FUNC:
            fp.ConfigRandom();
            fp.AddRandItem( 5, GL_NEVER);
            fp.AddRandItem(10, GL_LESS);
            fp.AddRandItem( 5, GL_EQUAL);
            fp.AddRandItem(30, GL_LEQUAL);
            fp.AddRandItem(10, GL_GREATER);
            fp.AddRandItem( 5, GL_NOTEQUAL);
            fp.AddRandItem(30, GL_GEQUAL);
            fp.AddRandItem( 5, GL_ALWAYS);
            fp.CompileRandom();
            break;

        case RND_TX_DIM:
            // Texture dimensionality: 2D, 3D, lwbe-map, etc.
            fp.ConfigRandom();
            fp.AddRandItem(5, GL_TEXTURE_2D);
            fp.AddRandItem(2, GL_TEXTURE_RENDERBUFFER_LW);
            fp.AddRandItem(1, GL_TEXTURE_3D);
            fp.AddRandItem(2, GL_TEXTURE_LWBE_MAP_EXT);
            fp.AddRandItem(1, GL_TEXTURE_1D);
            fp.AddRandItem(1, GL_TEXTURE_1D_ARRAY_EXT);
            fp.AddRandItem(1, GL_TEXTURE_2D_ARRAY_EXT);
            fp.AddRandItem(1, GL_TEXTURE_LWBE_MAP_ARRAY_ARB);
            fp.CompileRandom();
            break;

        case RND_TX_WIDTH:
            // Texture width (S coord) in texels, at base mipmap level
            fp.ConfigRandom();
            fp.AddRandItem( 1, 1);
            fp.AddRandItem( 1, 2);
            fp.AddRandItem( 1, 4);
            fp.AddRandItem( 2, 8);
            fp.AddRandItem( 3, 16);
            fp.AddRandItem( 4, 32);
            fp.AddRandItem( 3, 64);
            fp.AddRandItem( 2, 128);
            fp.AddRandItem( 1, 256);
            fp.AddRandItem( 1, 512);
            fp.AddRandItem( 1, 1*1024);
            fp.AddRandItem( 1, 2*1024);
            fp.AddRandItem( 1, 4*1024); //max 2d/lwbe Lwrie
            fp.AddRandItem( 1, 8*1024); //max 2d/lwbe Tesla
            fp.AddRandItem( 1, 16*1024);//max 2d/lwbe Fermi
            fp.CompileRandom();
            break;

        case RND_TX_HEIGHT:
            // Texture height (T coord) in texels, at base mipmap level
            fp.ConfigRandom();
            fp.AddRandItem( 1, 1);
            fp.AddRandItem( 1, 2);
            fp.AddRandItem( 1, 4);
            fp.AddRandItem( 2, 8);
            fp.AddRandItem( 3, 16);
            fp.AddRandItem( 4, 32);
            fp.AddRandItem( 3, 64);
            fp.AddRandItem( 2, 128);
            fp.AddRandItem( 1, 256);
            fp.AddRandItem( 1, 512);
            fp.AddRandItem( 1, 1*1024);
            fp.AddRandItem( 1, 2*1024);
            fp.AddRandItem( 1, 4*1024);//max 2d/lwbe Lwrie
            fp.AddRandItem( 1, 8*1024);//max 2d/lwbe Tesla
            fp.AddRandItem( 1, 16*1024);//max 2d/lwbe Fermi
            fp.CompileRandom();
            break;

        case RND_TX_BASE_MM:
            // Base mipmap level
            fp.ConfigRandom();
            fp.AddRandItem(9, 0);
            fp.AddRandRange(1, 0, 10);
            fp.CompileRandom();
            break;

        case RND_TX_MAX_MM:
            // Max mipmap level
            fp.ConfigRandom();
            fp.AddRandItem(9, 100);
            fp.AddRandRange(1, 0, 100);
            fp.CompileRandom();
            break;

        case RND_TXU_REDUCTION_MODE:
            fp.ConfigRandom();
            fp.AddRandItem(1, GL_WEIGHTED_AVERAGE_EXT);
            fp.AddRandItem(1, GL_MIN);
            fp.AddRandItem(1, GL_MAX);
            fp.CompileRandom();
            break;

        case RND_TX_NAME:
            // Which pretty image to load into the texture memory?
            fp.ConfigRandom();
            fp.AddRandItem(  2, RND_TX_NAME_Checkers);
            fp.AddRandItem( 10, RND_TX_NAME_Random);
            fp.AddRandRange(20, RND_TX_NAME_ShortMats, RND_TX_NAME_SlowRising);
            fp.AddRandRange( 4, RND_TX_NAME_MarchingZeroes, RND_TX_NAME_RunSwizzle);
            fp.CompileRandom();
            break;

        case RND_TX_INTERNAL_FORMAT:
            // how the driver should store the texture in memory: RGB, A, RGBA,
            // bits per color, etc.
            fp.ConfigRandom();
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_5x4_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_5x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_6x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_6x6_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_8x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_8x6_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_8x8_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_10x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_10x6_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_10x8_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_10x10_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_12x10_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_RGBA_ASTC_12x12_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR);
            fp.AddRandItem(10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR);
            fp.AddRandItem(5, GL_RGB5);
            fp.AddRandItem(5, GL_RGB5_A1);
            fp.AddRandItem(5, GL_ALPHA8);
            fp.AddRandItem(5, GL_R11F_G11F_B10F_EXT);
            fp.AddRandItem(5, GL_RGB10_A2);
            fp.AddRandItem(5, GL_DEPTH_COMPONENT16);
            fp.AddRandItem(5, GL_DEPTH_COMPONENT24);
            fp.AddRandItem(5, GL_RGBA8);
            fp.AddRandItem(5, GL_RGB8);
            fp.AddRandItem(5, GL_LUMINANCE8_ALPHA8);
            fp.AddRandItem(5, GL_LUMINANCE8);
            fp.AddRandItem(5, GL_RGBA16);
            fp.AddRandItem(5, GL_RGB16);
            fp.AddRandItem(5, GL_LUMINANCE16_ALPHA16);
            fp.AddRandItem(5, GL_LUMINANCE16);
            fp.AddRandItem(5, GL_RGBA16F_ARB);
            fp.AddRandItem(5, GL_RGB16F_ARB);
            fp.AddRandItem(5, GL_LUMINANCE_ALPHA16F_ARB);
            fp.AddRandItem(5, GL_LUMINANCE16F_ARB);
            fp.AddRandItem(5, GL_RGBA32F_ARB);
            fp.AddRandItem(5, GL_RGB32F_ARB);
            fp.AddRandItem(5, GL_LUMINANCE_ALPHA32F_ARB);
            fp.AddRandItem(5, GL_LUMINANCE32F_ARB);
            fp.AddRandItem(5, GL_SIGNED_RGBA8_LW);
            fp.AddRandItem(5, GL_SIGNED_RGB8_LW);
            fp.AddRandItem(5, GL_SIGNED_LUMINANCE8_ALPHA8_LW);
            fp.AddRandItem(5, GL_SIGNED_LUMINANCE8_LW);
            fp.AddRandItem(5, GL_RGBA16_SNORM);
            fp.AddRandItem(5, GL_RGB16_SNORM);
            fp.AddRandItem(5, GL_RG16_SNORM);
            fp.AddRandItem(5, GL_R16_SNORM);
            fp.AddRandItem(1, GL_RGBA8I_EXT);
            fp.AddRandItem(1, GL_RGB8I_EXT);
            fp.AddRandItem(1, GL_LUMINANCE_ALPHA8I_EXT);
            fp.AddRandItem(1, GL_LUMINANCE8I_EXT);
            fp.AddRandItem(2, GL_RGBA16I_EXT);
            fp.AddRandItem(2, GL_RGB16I_EXT);
            fp.AddRandItem(2, GL_LUMINANCE_ALPHA16I_EXT);
            fp.AddRandItem(2, GL_LUMINANCE16I_EXT);
            fp.AddRandItem(3, GL_RGBA32I_EXT);
            fp.AddRandItem(3, GL_RGB32I_EXT);
            fp.AddRandItem(3, GL_LUMINANCE_ALPHA32I_EXT);
            fp.AddRandItem(3, GL_LUMINANCE32I_EXT);
            fp.AddRandItem(1, GL_RGBA8UI_EXT);
            fp.AddRandItem(1, GL_RGB8UI_EXT);
            fp.AddRandItem(1, GL_LUMINANCE_ALPHA8UI_EXT);
            fp.AddRandItem(1, GL_LUMINANCE8UI_EXT);
            fp.AddRandItem(2, GL_RGBA16UI_EXT);
            fp.AddRandItem(2, GL_RGB16UI_EXT);
            fp.AddRandItem(2, GL_LUMINANCE_ALPHA16UI_EXT);
            fp.AddRandItem(2, GL_LUMINANCE16UI_EXT);
            fp.AddRandItem(3, GL_RGBA32UI_EXT);
            fp.AddRandItem(3, GL_RGB32UI_EXT);
            fp.AddRandItem(3, GL_LUMINANCE_ALPHA32UI_EXT);
            fp.AddRandItem(3, GL_LUMINANCE32UI_EXT);
            fp.AddRandItem(3, GL_ALPHA16);
            fp.AddRandItem(3, GL_ALPHA16F_ARB);
            fp.AddRandItem(3, GL_ALPHA32F_ARB);
            fp.AddRandItem(3, GL_INTENSITY16F_ARB);
            fp.AddRandItem(3, GL_INTENSITY32F_ARB);
            fp.AddRandItem(5, GL_RGBA4);
            fp.AddRandItem(5, GL_RGB9_E5_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_LUMINANCE_LATC1_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT);
            fp.AddRandItem(5, GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT);
            fp.AddRandItem(2, GL_COMPRESSED_R11_EAC);
            fp.AddRandItem(2, GL_COMPRESSED_SIGNED_R11_EAC);
            fp.AddRandItem(2, GL_COMPRESSED_RG11_EAC);
            fp.AddRandItem(2, GL_COMPRESSED_SIGNED_RG11_EAC);
            fp.AddRandItem(2, GL_COMPRESSED_RGB8_ETC2);
            fp.AddRandItem(2, GL_COMPRESSED_SRGB8_ETC2);
            fp.AddRandItem(2, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
            fp.AddRandItem(2, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
            fp.AddRandItem(2, GL_COMPRESSED_RGBA8_ETC2_EAC);
            fp.AddRandItem(2, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
            fp.AddRandItem(2, GL_STENCIL_INDEX8);
            fp.CompileRandom();
            break;

        case RND_TX_DEPTH:
            // depth in pixels of 3D textures
            fp.ConfigRandom();
            fp.AddRandItem( 1, 1);
            fp.AddRandItem( 1, 2);
            fp.AddRandItem( 1, 4);
            fp.AddRandItem( 1, 8);
            fp.AddRandItem( 2, 16);
            fp.AddRandItem( 3, 32);
            fp.AddRandItem( 4, 64);
            fp.AddRandItem( 3, 128);
            fp.AddRandItem( 2, 256);
            fp.AddRandItem( 1, 512); // Max 3d on Lwrie
            fp.AddRandItem( 1, 1024);
            fp.AddRandItem( 1, 2*1024);//Max 3d on Tesla/Fermi
            fp.AddRandItem( 1, 4*1024);
            fp.AddRandItem( 1, 8*1024);
            //Proposed Max 3d depth on GF11x.
            fp.AddRandItem( 1, 16*1024);
            fp.CompileRandom();
            break;

        case RND_TX_NUM_OBJS:
            // Number of texture objects to load at each restart point
            fp.ConfigRandom();
            fp.AddRandItem(1, 25);
            fp.CompileRandom();
            break;

        case RND_TX_MAX_TOTAL_BYTES:
            // Max total bytes of texture memory to consume when loading
            // textures at a restart point.
            fp.ConfigRandom();
            fp.AddRandItem(1, 1*1024*1024);
            fp.CompileRandom();
            break;

        case RND_TX_CONST_COLOR:
            // Color for use by RND_TX_NAME_SolidColor.
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, 0xffffffff);
            fp.CompileRandom();
            break;

        case RND_TX_IS_NP2:
            // Should the texture have non-power-of-two dimensions?
            fp.ConfigRandom();
            fp.AddRandItem(90, GL_FALSE);
            fp.AddRandItem(10, GL_TRUE);
            fp.CompileRandom();
            break;

        case RND_TX_MS_INTERNAL_FORMAT:
            fp.ConfigRandom();
            fp.AddRandItem(1, GL_RGB5);
            fp.AddRandItem(1, GL_RGB5_A1);
            fp.AddRandItem(1, GL_RGB8);
            fp.AddRandItem(1, GL_RGBA8);
            fp.AddRandItem(1, GL_RGBA16F);
            fp.AddRandItem(1, GL_RGB16F);
            fp.AddRandItem(1, GL_RGB32F);
            fp.AddRandItem(1, GL_RGBA32F);
            fp.CompileRandom();
            break;

        case RND_TX_MS_MAX_SAMPLES:
            fp.ConfigRandom();
            fp.AddRandRange(1, 2, 16);
            fp.CompileRandom();
            break;

        case RND_TX_MS_NAME:
            fp.ConfigRandom();
            fp.AddRandItem(4, RND_TX_NAME_MsCircles);
            fp.AddRandItem(4, RND_TX_NAME_MsSquares);
            fp.AddRandItem(1, RND_TX_NAME_MsStripes);
            fp.AddRandItem(1, RND_TX_NAME_MsRandom );
            fp.CompileRandom();
            break;

        case RND_TX_HAS_BORDER:
            // Should the texture have a 1 texel border?
            fp.ConfigRandom();
            fp.AddRandItem(90, GL_FALSE);
            fp.AddRandItem(10, GL_TRUE);
            fp.CompileRandom();
            break;

        case RND_TX_LWBEMAP_SEAMLESS:
            fp.ConfigRandom();
            fp.AddRandItem(1, GL_TRUE);
            fp.AddRandItem(1, GL_FALSE);
            fp.CompileRandom();
            break;

        case RND_TX_MSTEX_DEBUG_FILL:
            fp.ConfigConst(GL_FALSE);
            break;

        case RND_TX_SPARSE_ENABLE:
            fp.ConfigConst(GL_TRUE);
            break;

        case RND_TX_SPARSE_TEX_INT_FMT_CLASS:
            fp.ConfigRandom();
            fp.AddRandItem(4, GL_VIEW_CLASS_128_BITS);
            fp.AddRandItem(1, GL_VIEW_CLASS_96_BITS);
            fp.AddRandItem(1, GL_VIEW_CLASS_64_BITS);
            fp.AddRandItem(1, GL_VIEW_CLASS_48_BITS);
            fp.AddRandItem(4, GL_VIEW_CLASS_32_BITS);
            fp.AddRandItem(1, GL_VIEW_CLASS_24_BITS);
            fp.CompileRandom();
            break;

        case RND_TX_SPARSE_TEX_NUM_MMLEVELS:
            fp.ConfigRandom();
            fp.AddRandRange(1, 8, 10);
            fp.CompileRandom();
            break;

        case RND_TX_SPARSE_TEX_NUM_LAYERS:
            fp.ConfigRandom();
            fp.AddRandItem(1, 6);  // 6x helps in using view as lwbe array
            fp.AddRandItem(1, 12);
            fp.CompileRandom();
            break;

        case RND_TX_VIEW_ENABLE:
            fp.ConfigRandom();
            fp.AddRandItem(70, GL_FALSE);
            fp.AddRandItem(30, GL_TRUE);
            fp.CompileRandom();
            break;

        case RND_TX_VIEW_BASE_MMLEVEL:
            fp.ConfigRandom();
            fp.AddRandRange(1, 3, 6);  // if base view texture is too large, it might be a wastage of tex memory
            fp.CompileRandom();        // if its too small, it may not be used as view often
            break;

        case RND_TX_VIEW_NUM_LAYERS:
            fp.ConfigRandom();
            fp.AddRandItem(3, 6);  // 6x helps in using view as lwbe array
            fp.AddRandItem(1, 12);
            fp.CompileRandom();
            break;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Set the max bit-depth of float textures that is filterable. (default 16)
void RndTexturing::SetMaxFilterableFloatDepth(int depth)
{
    m_MaxFilterableFloatDepth = depth;
}

//------------------------------------------------------------------------------
void RndTexturing::SetHasF32BorderColor (bool isTeslaPlus)
{
    m_HasF32BorderColor = isTeslaPlus;
}

//------------------------------------------------------------------------------
// Do once-per-restart picks, prints, & sends.
//
RC RndTexturing::Restart()
{
    RC rc;
    m_Pickers.Restart();
    memset(&m_Txu, 0, sizeof(m_Txu));

    m_UseBindlessTextures = m_pGLRandom->m_GpuPrograms.UseBindlessTextures();

    // Surveys of undergrads reveal that 37 is the most-random small integer.
    // So long as we use a consistent seed, anthing will do here.
    m_PatternClass.FillRandomPattern(37);

    CHECK_RC(GenerateTextures());

    if ( m_UseBindlessTextures )
    {
        CHECK_RC(LoadBindlessTextures());
    }
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5))
    {
        for ( GLuint i = 0; i < (GLuint)MaxImageUnits; i++)
        {   // unbind all Image Units
            glBindImageTextureEXT( i,   // imageUnit
                    0,                  // textureId
                    0,                  // level
                    false,              //layered
                    0,                  // layer
                    GL_READ_WRITE,      // access
                    GL_RGBA32F_ARB);    // format
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
// Initialize the Shader Buffer Object data with handles to the texture objects.
// Note1: When using bindless textures we don't know which one of the 4 texture
// objects will be used until runtime. So get all 4 handles for each type of
// referenced texture object.
RC RndTexturing::LoadBindlessTextures()
{
    RC rc;
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_bindless_texture))
    {
        return OK;
    }

    // Create a Shader Buffer Object to store the texture object handles.
    // Also picks the bindless texture properites
    // lookup table to map the index value to a texture attribute
    memset( &m_TxHandles[0], 0, sizeof(m_TxHandles));

    for( UINT32 sboRegionIdx = 0;
         sboRegionIdx < GLRandom::TiNUM_Indicies; sboRegionIdx++)
    {
        UINT32 attr = TexGen::hndlIdx[sboRegionIdx]; // Is1d,Is2d,etc.

        // If no texture is needed, required attribs will be 0.
        if ((m_pGLRandom->m_GpuPrograms.BindlessTxAttrRef(attr) == 0) &&
            (m_pGLRandom->m_Pixels.BindlessTxAttrRef(attr) == 0))
        {
            continue;
        }

        int sboRegionOffset = 0;
        // load the texture object's handle & pick the texture object attributes
        for( UINT32 txobjIdx = 0;
             (txobjIdx < m_NumTextures) && (sboRegionOffset < NumTxHandles);
             txobjIdx++)
        {
            if ( attr == (attr & m_Textures[txobjIdx].Attribs))
            {
                // Use the same index for handles/fetchers.  It is possible that
                // there are more textures than fetchers, but the maximum number
                // of fetchers that will ever be used will be then number of
                // allocated handles
                UINT32 sboIdx = sboRegionIdx*NumTxHandles+sboRegionOffset;
                MASSERT(sboIdx < GLRandom::MaxTxFetchers);

                TextureFetcherState * pTxf = m_Txu.Fetchers + sboIdx;
                TextureObjectState * pTx = m_Textures + txobjIdx;

                // Pick the texture attribute properties for this texture object
                PickTextureParameters(pTxf, pTx, false,false);

                CHECK_RC(SetTextureParameters(txobjIdx, pTxf, pTx));
                CHECK_RC(BindImageUnit(txobjIdx, pTx));

                // Once you get the handle the texture object become immutable
                // and subsequent glTexParameter() calls will fail!
                pTx->Handle  = glGetTextureHandleLW(pTx->Id);
                m_TxHandles[sboIdx] = pTx->Handle;
                sboRegionOffset++;
            }
        }
        MASSERT( sboRegionOffset == NumTxHandles);
    }
    CHECK_RC(LoadBindlessTextureHandles());
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Load the bindless texture handles into framebuffer memory and make them accessable to the
// shaders.
RC RndTexturing::LoadBindlessTextureHandles()
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_bindless_texture) &&
        m_UseBindlessTextures &&
        (m_TxSbo.Id != 0))
    {
        glBindBuffer(GL_ARRAY_BUFFER_ARB, m_TxSbo.Id);
        // Wait for all shaders to drain before updating with new handles.
        glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
        for (size_t idx = 0; idx < NUMELEMS(m_TxHandles); idx++)
        {
            if (m_TxHandles[idx] != 0)
            {
                glMakeTextureHandleResidentLW( m_TxHandles[idx]);
            }
        }

        glBufferSubData(GL_ARRAY_BUFFER,    // target
                0,                          // offset
                sizeof(m_TxHandles),        //sboSize,
                (GLvoid *)&m_TxHandles);    // data

        // release the current binding for now
        glBindBuffer(GL_ARRAY_BUFFER,0);

        return(mglGetRC());
    }
    return OK;
}

//------------------------------------------------------------------------------
// Unload any memory resident texture handles for bindless textures to keep them from being
// accessed by shaders from subsequent frames.
RC RndTexturing::UnloadBindlessTextureHandles()
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_bindless_texture) &&
        m_UseBindlessTextures &&
        (m_TxSbo.Id != 0))
    {
       // Wait for all shaders to drain before updating the handles.
       glBindBuffer(GL_ARRAY_BUFFER_ARB, m_TxSbo.Id);
       glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);

       for (size_t tx = 0; tx < NUMELEMS(m_TxHandles); tx++)
       {
           if (m_TxHandles[tx] != 0)
           {
               glMakeTextureHandleNonResidentLW( m_TxHandles[tx]);
               m_TxHandles[tx] = 0;
           }
       }

       glBufferSubData(GL_ARRAY_BUFFER,    // target
               0,                          // offset
               sizeof(m_TxHandles),        //sboSize,
               (GLvoid *)&m_TxHandles);    // data

       // release the current binding for now
       glBindBuffer(GL_ARRAY_BUFFER,0);

       return(mglGetRC());
    }
    return OK;
}

//-----------------------------------------------------------------------------
// Check if the texture meets all the image unit requirements.
bool RndTexturing::TextureMatchesRequirements
(
    const GLRandom::TxfReq& TxfReq,
    UINT32 TxIdx
)
{
    // This attribute check should be the same as the check in
    // RndGpuPrograms::CheckPgmRequirements
    //
    // If a non-dimension bit is 0 in TxfReq.Attrs then it is considered
    // "don't care"
    unsigned int reqBits = TxfReq.Attrs & SASMNbDimFlags;
    // Basic attributes match?
    if (reqBits != (m_Textures[TxIdx].Attribs & reqBits))
        return false;

    bool bMatchesRequirements = false;
    const int rwAccess = m_Textures[TxIdx].Access;
    if (TxfReq.IU.unit >= 0)
    {
        //Texture image must have the correct access & internal fmt.
        if ((m_Textures[TxIdx].LoadMaxMM >= (GLuint)TxfReq.IU.mmLevel) &&
            ((rwAccess == TxfReq.IU.access) || rwAccess == iuaNone))
        {
            if (TxfReq.Format == 0)
            {
                switch (m_Textures[TxIdx].InternalFormat)
                {
                    case GL_RGBA32I_EXT:
                    case GL_RGBA32UI_EXT:
                    case GL_RGBA32F_ARB:
                        bMatchesRequirements = (TxfReq.IU.elems == 4);
                        break;
                    case GL_RG32I:
                    case GL_RG32UI:
                    case GL_RG32F:
                        bMatchesRequirements = (TxfReq.IU.elems == 2);
                        break;
                    case GL_R32I:
                    case GL_R32UI:
                    case GL_R32F:
                        bMatchesRequirements = (TxfReq.IU.elems == 1);
                        break;
                    default:
                        break;
                }
            }
            else if (m_Textures[TxIdx].InternalFormat == TxfReq.Format)
            {
                bMatchesRequirements = true;
            }
        }
    }
    else if ((rwAccess == iuaNone || rwAccess == TxfReq.IU.access) &&
             ((TxfReq.Format == 0) ||
              (m_Textures[TxIdx].InternalFormat == TxfReq.Format)))
    {
        // Texture image has the correct access
        bMatchesRequirements = true;
    }

    return bMatchesRequirements;
}

//-----------------------------------------------------------------------------
// Validate this texture meets all the image unit requirements.  Ensure that
// image unit parameters are compatible if using the same texture on multiple
// image units
bool RndTexturing::ValidateTexture
(   int Txf,
    GLRandom::TxfReq& TxfReq,
    UINT32 TxIdx
)
{
    bool bFound = TextureMatchesRequirements(TxfReq, TxIdx);

    if (bFound && (TxfReq.IU.unit >= 0) && (TxfReq.IU.access != iuaRead))
    {
        // Final check, make sure this texture is not being accessed by
        // another image unit with the same configuration.
        for (GLint txf = 0; txf < Txf; txf++)
        {
            if (m_Txu.Fetchers[txf].BoundTexture == (int)TxIdx)
            {
                // At this point two fetchers bound to the same texture
                // must have the same access & elems. If the mmLevel is
                // the same we have potential for write collisions.
                GLRandom::TxfReq txfReq;
                m_pGLRandom->m_GpuPrograms.TxAttrNeeded(txf,&txfReq);
                if( txfReq.IU.mmLevel == TxfReq.IU.mmLevel)
                    bFound = false;
            }
        }
    }

    if (bFound)
    {
        m_Txu.Fetchers[Txf].BoundTexture = TxIdx;
        m_Textures[TxIdx].Access = TxfReq.IU.access;
    }
    return bFound;
}
//-----------------------------------------------------------------------------
// Pick new random state.
//
void RndTexturing::Pick()
{
    // Bindless texture picking is done in Restart();
    // However we do need to consistency check our picked data.
    if (m_UseBindlessTextures)
    {
        m_pGLRandom->LogData(RND_TX, &m_Txu, sizeof(m_Txu));
        return;
    }

    int TxUnit;
    //
    // Set defaults to the Fetchers arrays
    //
    memset(&m_Txu, 0, sizeof(m_Txu));

    for (TxUnit = 0; TxUnit < MaxTxFetchers; TxUnit++)
    {
        m_Txu.Fetchers[TxUnit].BoundTexture = -1;
    }

    // Pick the overall texturing mode:
    //   Traditional      (any)
    //   Fragment Program (lw3x or later) (controlled by RndPrograms)

    UINT32 txuEnableMask = m_Pickers[ RND_TXU_ENABLE_MASK  ].Pick();

    txuEnableMask &= ((1 << m_pGLRandom->NumTxUnits())-1);

    if (m_pGLRandom->m_GpuPrograms.FrProgEnabled())
    {
        txuEnableMask        = 0;
    }

    // Bind random texture-objects to meet the requirements of whatever
    // fragment and/or vertex programs are enabled.
    // Note that we may have lw4x vertex programs binding textures
    // _along_with_ one of traditional/frag-prog per-pixel texturing.
    vector<UINT32> TxObjIdxs(m_NumTextures,0);

    for (UINT32 i = 0; i < m_NumTextures; i++)
    {
        TxObjIdxs[i] = i;
        m_Textures[i].Access = m_Textures[i].RsvdAccess;
    }

    for (GLint txf = 0; txf < m_pGLRandom->NumTxFetchers(); txf++)
    {
        GLRandom::TxfReq txfReq;
        if ((m_pGLRandom->m_GpuPrograms.TxAttrNeeded(txf,&txfReq) == 0) &&
            (m_pGLRandom->m_Pixels.TxAttrNeeded(txf,&txfReq) == 0))

        {
            // If no texture is needed, required attribs will be 0.
            continue;
        }

        // If a texture is needed by a program, mark that txu as disabled
        // for traditional texturing.
        txuEnableMask &= ~(1<<txf);

        m_pFpCtx->Rand.Shuffle (m_NumTextures, &TxObjIdxs[0]);
        UINT32 ii;
        for (ii = 0; ii < m_NumTextures; ii++)
        {
            if (ValidateTexture(txf, txfReq, TxObjIdxs[ii]))
                break;
        }
        // If this happens it means we couldn't find a matching tex-obj for a
        // fetcher!
        MASSERT(ii < m_NumTextures);
    }

    // Traditional OpenGL texturing mode.

    for (UINT32 i = 0; i < m_NumTextures; i++)
        TxObjIdxs[i] = i;

    for (TxUnit = 0; TxUnit < m_pGLRandom->NumTxUnits(); TxUnit++)
    {
        if (txuEnableMask & (1 << TxUnit))
        {
            m_pFpCtx->Rand.Shuffle (m_NumTextures, &TxObjIdxs[0]);

            UINT32 ii;
            for (ii = 0; ii < m_NumTextures; ii++)
            {
                GLuint TxObj = TxObjIdxs[ii];

                if (m_Textures[TxObj].Attribs & IsLegacy)
                {
                    m_Txu.Fetchers[TxUnit].BoundTexture = TxObj;
                    m_Txu.Fetchers[TxUnit].Enable       = GL_TRUE;
                    break;  // can use this texture
                }
            }
            if (ii >= m_NumTextures)
            {
                // There are no valid textures loaded!
                // Disable texturing.
                txuEnableMask = 0;
                break;  // can use this texture
            }
        }
    }

    //
    // Pick random stuff for each texture fetcher.
    //
    for (TxUnit = 0; TxUnit < m_pGLRandom->NumTxFetchers(); TxUnit++)
    {
        TextureFetcherState * pTxf = m_Txu.Fetchers + TxUnit;
        const TextureObjectState * pTx = m_Textures + pTxf->BoundTexture;

        if (pTxf->BoundTexture < 0)
            continue;

        UINT32 txAttrNeeded = m_pGLRandom->m_GpuPrograms.TxAttrNeeded(TxUnit) |
                              m_pGLRandom->m_Pixels.TxAttrNeeded(TxUnit, nullptr);

        const bool bIsNoClamp = (0 !=  (IsNoClamp & txAttrNeeded));

        const bool bNeedShadow = (0 != (IsShadowMask & txAttrNeeded));

        PickTextureParameters( pTxf, pTx, bIsNoClamp, bNeedShadow);

    } // for each texture fetcher

    m_pGLRandom->LogData(RND_TX, &m_Txu, sizeof(m_Txu));
}

//------------------------------------------------------------------------------
void RndTexturing::PickTextureParameters(
    TextureFetcherState * pTxf,
    const TextureObjectState * pTx,
    bool bIsNoClamp, bool bNeedShadow
)
{
    pTxf->Wrap[0]       = m_Pickers[RND_TXU_WRAPMODE].Pick();
    pTxf->Wrap[1]       = m_Pickers[RND_TXU_WRAPMODE].Pick();
    pTxf->Wrap[2]       = m_Pickers[RND_TXU_WRAPMODE].Pick();
    pTxf->MinFilter     = m_Pickers[RND_TXU_MIN_FILTER].Pick();
    pTxf->MagFilter     = m_Pickers[RND_TXU_MAG_FILTER].Pick();
    pTxf->CompareMode   = m_Pickers[RND_TXU_COMPARE_MODE].Pick();
    pTxf->ShadowFunc    = m_Pickers[RND_TXU_SHADOW_FUNC].Pick();
    pTxf->MinLambda     = pTx->BaseMM + m_Pickers[RND_TXU_MIN_LAMBDA].FPick();
    pTxf->MaxLambda     = pTx->MaxMM  + m_Pickers[RND_TXU_MAX_LAMBDA].FPick();
    pTxf->BiasLambda    = m_Pickers[RND_TXU_BIAS_LAMBDA].FPick();
    pTxf->MaxAnisotropy = m_Pickers[RND_TXU_MAX_ANISOTROPY].FPick();

    pTxf->BorderValues[0] = m_Pickers[RND_TXU_BORDER_COLOR].FPick();
    pTxf->BorderValues[1] = m_Pickers[RND_TXU_BORDER_COLOR].FPick();
    pTxf->BorderValues[2] = m_Pickers[RND_TXU_BORDER_COLOR].FPick();
    pTxf->BorderValues[3] = m_Pickers[RND_TXU_BORDER_ALPHA].FPick();
    pTxf->UseSeamlessLwbe = (m_Pickers[RND_TX_LWBEMAP_SEAMLESS].Pick() != 0);
    if (m_pGLRandom->HasExt(GLRandomTest::ExtEXT_texture_filter_minmax))
    {
        pTxf->ReductionMode = m_Pickers[RND_TXU_REDUCTION_MODE].Pick();
    }

    if (bIsNoClamp)
    {
        //wrap S/T modes can't be CLAMP or MIRROR_CLAMP_EXT
        if( pTxf->Wrap[0] == GL_CLAMP || pTxf->Wrap[0] == GL_MIRROR_CLAMP_EXT)
        {
            pTxf->Wrap[0] = GL_REPEAT;
        }
        if( pTxf->Wrap[1] == GL_CLAMP || pTxf->Wrap[1] == GL_MIRROR_CLAMP_EXT)
        {
            pTxf->Wrap[1] = GL_REPEAT;
        }
    }

    if (m_HasF32BorderColor)
    {
        // Tesla (G80/LW50) and later have true f32 for a,r,g,b
        // texture borders, so expand the border-color range here.

        float borderMin = 0.0;
        float borderMax = 1.0;

        if (ColorUtils::IsNormalized(pTx->HwFmt))
        {
            if (ColorUtils::IsSigned(pTx->HwFmt))
                borderMin = -1.0F;
        }
        else
        {
            // Float or texture_integer format, use wide range.
            borderMin = -10.0;
            borderMax =  10.0;
        }

        float borderRange = borderMax - borderMin;
        for (int i = 0; i < 4; i++)
            pTxf->BorderValues[i] =
                pTxf->BorderValues[i] * borderRange - borderMin;
    }

    bool bZeroBorder = !!(pTx->Attribs & IsNoBorder);

    // LW50 doesn't support integer textures with non-zero border color
    // components, except for 32-bit formats.
    if (m_pGLRandom->GetBoundGpuDevice()->GetFamily() < GpuDevice::Fermi)
    {
        const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(pTx->InternalFormat);
        MASSERT(pFmtInfo);
        if (!strcmp(pFmtInfo->TexExtension, "GL_EXT_texture_integer") &&
            (pTx->LoadType != GL_UNSIGNED_INT) && (pTx->LoadType != GL_INT))
        {
            bZeroBorder = true;
        }
    }
    if (bZeroBorder)
    {
        for (int i = 0; i < 4; i++)
            pTxf->BorderValues[i] =  0.0F;
    }

    // Make sure texture anisotropy is in range.
    pTxf->MaxAnisotropy =
            MINMAX(1.0F, pTxf->MaxAnisotropy, m_pGLRandom->MaxMaxAnisotropy());

    if (pTx->Dim == GL_TEXTURE_3D)
        pTxf->MaxAnisotropy = 1.0;

    switch (pTxf->MinFilter)
    {
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        {
            // GL error if you use mipmap filtering on non-mipmapped texture.
            if (pTx->MaxMM == 0)
                pTxf->MinFilter = GL_NEAREST;
            break;
        }

        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
        {
            // GL error if you use mipmap filtering on non-mipmapped texture.
            if (pTx->MaxMM == 0)
                pTxf->MinFilter = GL_LINEAR;
            break;
        }

        case GL_LINEAR:
        case GL_NEAREST:
        {
            break;
        }

        default: MASSERT(0);
    }

    const bool isDepth    = (0 != (IsDepth & pTx->Attribs));

    // SHADOW texture fetchers must bind to depth texture objects.
    // other texture fetchers may bind to depth or color texture objects.
    MASSERT((bNeedShadow && isDepth) || !bNeedShadow);

    if (bNeedShadow)
    {
        // TEXTURE_COMPARE_MODE must be enabled when a shader is using
        // this fetcher as a SHADOW target, see ARB_fragment_program_shadow
        // and http://lwbugs/564095.
        pTxf->CompareMode = GL_COMPARE_R_TO_TEXTURE;
    }
    else
    {
        // If we enable TEXTURE_COMPARE_MODE with a texture that's not
        // used by a program as a SHADOW target, the shader optimizer might
        // not set up the required R coord correctly.
        // Play it safe and disable TEXTURE_COMPARE_MODE.
        pTxf->CompareMode = GL_NONE;
    }
    if (!isDepth)
    {
        // TEXTURE_COMPARE_MODE must be disabled when the texture object
        // is not a depth format.
        pTxf->CompareMode = GL_NONE;
    }
    // The other possibility is  (!needShadow && isDepth && GL_NONE)
    // This is safe, depth is treated as one of luma, alpha, or intensity
    // based on the RND_TX_DEPTH_TEX_MODE picker.

    // Force a supported shadow func
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtEXT_shadow_funcs))
    {
        switch (pTxf->ShadowFunc)
        {
            case GL_NEVER:
            case GL_LESS:
            case GL_EQUAL:
                pTxf->ShadowFunc = GL_LEQUAL;
                break;
            case GL_GREATER:
            case GL_NOTEQUAL:
            case GL_ALWAYS:
                pTxf->ShadowFunc = GL_GEQUAL;
                break;
        }
    }

    // Some textures can't be filtered in HW.
    if (pTx->Attribs & IsNoFilter)
    {
        switch (pTxf->MinFilter)
        {
            case GL_NEAREST_MIPMAP_NEAREST:
            case GL_LINEAR_MIPMAP_NEAREST:
            case GL_NEAREST_MIPMAP_LINEAR:
            case GL_LINEAR_MIPMAP_LINEAR:
                pTxf->MinFilter = GL_NEAREST_MIPMAP_NEAREST;
                break;
            default:
                pTxf->MinFilter = GL_NEAREST;
                break;
        }
        pTxf->MagFilter = GL_NEAREST;
        pTxf->MaxAnisotropy = 1.0;
    }

    // MinLambda must be between base and max MM level.
    pTxf->MinLambda =
            MINMAX((GLfloat)pTx->BaseMM, pTxf->MinLambda, (GLfloat)pTx->MaxMM);

    // MaxLambda must be between MinLambda and max MM level.
    pTxf->MaxLambda =
            MINMAX(pTxf->MinLambda, pTxf->MaxLambda, (GLfloat)pTx->MaxMM);

    // BiasLambda shouldn't "hit the rails"
    if ( fabs(pTxf->BiasLambda) > (pTxf->MaxLambda - pTxf->MinLambda))
        pTxf->BiasLambda = 0.0f;

    // Due to funny validation code for lw4 (__glLW4UpdateTextureSupported)
    // when we aren't trying to bias lambda we need to set the limits wide.
    if (pTxf->MinLambda <= (GLfloat)pTx->BaseMM)
        pTxf->MinLambda = -1000.0f;
    if (pTxf->MaxLambda >= (GLfloat)pTx->MaxMM)
        pTxf->MaxLambda = 1000.0f;

    // Make edge repeat/clamp modes compatible with the bound texture.
    GLint i;
    for (i = 0; i < 3; i++)
    {
        GLenum *pMode = &pTxf->Wrap[i];

        if (!m_pGLRandom->HasExt(GLRandomTest::ExtEXT_texture_mirror_clamp))
        {
            if (*pMode == GL_MIRROR_CLAMP_EXT)
                *pMode = GL_CLAMP;
            if (*pMode == GL_MIRROR_CLAMP_TO_EDGE_EXT)
                *pMode = GL_CLAMP_TO_EDGE;
            if (*pMode == GL_MIRROR_CLAMP_TO_BORDER_EXT)
                *pMode = GL_CLAMP_TO_BORDER;
        }
        if (!m_pGLRandom->HasExt(GLRandomTest::ExtEXT_texture_edge_clamp))
        {
            if (*pMode == GL_CLAMP_TO_EDGE)
                *pMode = GL_CLAMP;
        }
        if (!m_pGLRandom->HasExt(GLRandomTest::ExtARB_texture_border_clamp))
        {
            if (*pMode == GL_CLAMP_TO_BORDER)
                *pMode = GL_CLAMP;
        }
        if (!m_pGLRandom->HasExt(GLRandomTest::ExtIBM_texture_mirrored_repeat))
        {
            if (*pMode == GL_MIRRORED_REPEAT_IBM)
                *pMode = GL_REPEAT;
        }

        if (pTx->Attribs & IsNoBorder)
        {
            if (*pMode == GL_CLAMP_TO_BORDER)
                *pMode = GL_CLAMP_TO_EDGE;
            if (*pMode == GL_MIRROR_CLAMP_TO_BORDER_EXT)
                *pMode = GL_MIRROR_CLAMP_TO_EDGE_EXT;
        }
    } // for wrap S,T,R

    if ((0 == (pTx->Attribs & IsLwbe)) ||
        (! m_pGLRandom->HasExt(GLRandomTest::ExtAMD_seamless_lwbemap_per_texture)))
    {
        pTxf->UseSeamlessLwbe = false;
    }
}

//------------------------------------------------------------------------------
// Print current picks to screen & logfile.
//
void RndTexturing::Print(Tee::Priority pri)
{
    // Only print when we are drawing vertexes/3dpixels, not pixels.
    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_pGLRandom->m_Misc.DrawAction())
        return;

    if ( m_UseBindlessTextures )
    {
        PrintBindlessTextures(pri);
    }
    else
    {
        GLint txu;
        for (txu = 0; txu < m_pGLRandom->NumTxFetchers(); txu++)
            PrintTextureUnit(pri, txu);
    }
}

//------------------------------------------------------------------------------
// Send current picks to library/HW.
//
RC RndTexturing::Send()
{
    StickyRC rc;
    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_pGLRandom->m_Misc.DrawAction())
    {
        // Dont do anything with pixels.
        return OK;
    }
    if (m_UseBindlessTextures)
    {
        // In bindless textures mode, all texture setup was done in Restart.
        return OK;
    }

    GLint displayListId = 0;
    glGetIntegerv(GL_LIST_INDEX, &displayListId);
    if (displayListId)
    {
        // We are lwrrently recording a display-list.
        // Don't try to set texture params, it's illegal now.
        return OK;
    }

    MASSERT_NO_GLERROR();
    GLint txf;
    for (txf = 0; (txf < m_pGLRandom->NumTxFetchers()) && (rc == OK); txf++)
    {
        const TextureFetcherState * pTxf = m_Txu.Fetchers + txf;
        TextureObjectState *  pTx  = m_Textures + pTxf->BoundTexture;

        if (pTxf->BoundTexture >= 0)
        {
            glActiveTexture(GL_TEXTURE0_ARB + txf);

            if (pTxf->Enable)
                glEnable(pTx->Dim);

            glBindTexture(pTx->Dim, pTx->Id);

            CHECK_RC(SetTextureParameters(txf,pTxf, pTx));
            CHECK_RC(BindImageUnit(txf, pTx));
        } // if txf enabled
    } // for each txf
    return rc;
}

//------------------------------------------------------------------------------
// Bind Image units
RC RndTexturing::BindImageUnit(GLint txf, TextureObjectState * pTx)
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5) &&
       (!m_UseBindlessTextures))
    {
        // If there is an image unit associated with this fetcher, then
        // bind it to the texture object too.
        GLRandom::TxfReq txfReq;
        m_pGLRandom->m_GpuPrograms.TxAttrNeeded(txf,&txfReq);

        if(txfReq.IU.unit >= 0)
        {
            MASSERT(txfReq.IU.access >= iuaRead &&
                    txfReq.IU.access <= iuaReadWrite);

            glBindImageTextureEXT( txfReq.IU.unit,
                    pTx->Id,
                    txfReq.IU.mmLevel,
                    true,
                    0,
                    GL_READ_ONLY+txfReq.IU.access-1,
                    pTx->InternalFormat);

            if (txfReq.IU.access > iuaRead)
            {
                // Textures using view are read only
                MASSERT(pTx->TextureType == IsDefault);
                pTx->DirtyLevelMask |= (1 << txfReq.IU.mmLevel);
            }
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
// Setup the texture mapping parameters.
RC RndTexturing::SetTextureParameters(
    GLint txf,
    const TextureFetcherState * pTxf,
    TextureObjectState * pTx
)
{
    // Colwentional texture parameters don't apply to multisampled textures.
    if (pTx->Dim != GL_TEXTURE_RENDERBUFFER_LW)
    {
        glTexParameteri(pTx->Dim, GL_TEXTURE_WRAP_S, pTxf->Wrap[0]);
        glTexParameteri(pTx->Dim, GL_TEXTURE_WRAP_T, pTxf->Wrap[1]);
        glTexParameteri(pTx->Dim, GL_TEXTURE_WRAP_R, pTxf->Wrap[2]);
        glTexParameteri(pTx->Dim, GL_TEXTURE_MIN_FILTER, pTxf->MinFilter);
        glTexParameteri(pTx->Dim, GL_TEXTURE_MAG_FILTER, pTxf->MagFilter);

        if (pTx->InternalFormat == GL_STENCIL_INDEX8)
        {
            // does not support filtering on mipmaps
            glTexParameteri(pTx->Dim, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(pTx->Dim, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glTexParameteri(pTx->Dim, GL_DEPTH_TEXTURE_MODE, pTx->DepthTexMode);
        glTexParameteri(pTx->Dim, GL_TEXTURE_COMPARE_MODE, pTxf->CompareMode);
        glTexParameteri(pTx->Dim, GL_TEXTURE_COMPARE_FUNC, pTxf->ShadowFunc);
        if (pTxf->UseSeamlessLwbe)
            glTexParameteri(pTx->Dim, GL_TEXTURE_LWBE_MAP_SEAMLESS, GL_TRUE);
        if(m_pGLRandom->HasExt(GLRandomTest::ExtEXT_texture_filter_anisotropic))
            glTexParameterf(pTx->Dim, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                pTxf->MaxAnisotropy);
        glTexParameterfv(pTx->Dim, GL_TEXTURE_BORDER_COLOR,pTxf->BorderValues);
        glTexParameteri(pTx->Dim, GL_TEXTURE_BASE_LEVEL, pTx->BaseMM);
        glTexParameteri(pTx->Dim, GL_TEXTURE_MAX_LEVEL, pTx->MaxMM);
        glTexParameterf(pTx->Dim, GL_TEXTURE_MIN_LOD, pTxf->MinLambda);
        glTexParameterf(pTx->Dim, GL_TEXTURE_MAX_LOD, pTxf->MaxLambda);
        if (m_pGLRandom->HasExt(GLRandomTest::ExtEXT_texture_filter_minmax))
        {
            glTexParameteri(pTx->Dim, GL_TEXTURE_REDUCTION_MODE_EXT, pTxf->ReductionMode);
        }

        //BUG 1515374: This call is causing miscompares when used in conjuction with bindless textures.
        //It looks like the GLState for this feature is bleeding over into subsequent frames.
        //I confirmed with OpenGL that they are not saving off this variable with glPushAttrib()
        if ( m_pGLRandom->HasExt(GLRandomTest::ExtEXT_texture_lod_bias) )
            glTexElwf(GL_TEXTURE_FILTER_CONTROL_EXT,GL_TEXTURE_LOD_BIAS_EXT,
                pTxf->BiasLambda);
    }

    return mglGetRC();
}

RC RndTexturing::CleanupGLState()
{
    RC rc = UnloadBindlessTextureHandles();

    FreeTextures();

    return rc;
}

//------------------------------------------------------------------------------
// Free all allocated resources that are created for the life time of the entire
// test.
//
RC RndTexturing::Cleanup()
{
    RC rc;

    // Delete the SBO used to store the bindless texture handles.
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_bindless_texture) &&
        m_TxSbo.Id != 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_TxSbo.Id);
        glMakeBufferNonResidentLW (GL_ARRAY_BUFFER_ARB);
        glDeleteBuffers(1, &m_TxSbo.Id);
        m_TxSbo.GpuAddr = 0;
    }

    // Delete sparse textures
    for (UINT32 i = 0; i < RND_TX_NUM_SPARSE_TEX; i++)
    {
        if (m_SparseTextures[i].Id != 0)
        {
            glDeleteTextures(1, &m_SparseTextures[i].Id);
            memset(&m_SparseTextures[i], 0, sizeof(TextureObjectState));
        }
    }
    rc = mglGetRC();
    return rc;
}

//------------------------------------------------------------------------------
// Find max log2 (i.e. max mip-map level) of each of 3 dimensions: w,h,d
//
static GLint FindMaxMM
(
    const GLuint * pWhd
)
{
    // For this algorithm, we really only need the highest-set-bit
    // of any of w,h,d.
    GLuint size = pWhd[RndTexturing::txWidth] |
                  pWhd[RndTexturing::txHeight] |
                  pWhd[RndTexturing::txDepth];

    GLint maxmm = 0;

    while (size > 1)
    {
        maxmm++;
        size >>= 1;
    }
    return maxmm;
}

//------------------------------------------------------------------------------
static void CalcMipSize
(
    const RndTexturing::TextureObjectState * pTx
    ,GLuint mmLevel
    ,GLuint * pWhd
)
{
    MASSERT(mmLevel <= pTx->LoadMaxMM);

    for (GLuint d = 0; d < RndTexturing::txMaxDims; d++)
    {
        if (d < pTx->NumDims)
        {
            pWhd[d] = pTx->BaseSize[d] >> mmLevel;
            if (pWhd[d] < 1)
                pWhd[d] = 1;
            if (pTx->HasBorder)
                pWhd[d] += 2;
        }
        else
        {
            // Set unused dimensions 1 (so the multiplication works later).
            pWhd[d] = 1;
        }
    }

    // Now, plug in the array-texture layers count (and faces for array-lwbe).

    // Only lwbe and lwbe-array textures can have faces >1.
    MASSERT((pTx->NumDims == 2) || (pTx->Faces == 1));

    if (pTx->Layers > 0)
    {
        // 1d-array tex hold layers as height.
        // 2d-array and lwbe-array hold layers as depth.
        // There is no such thing as a 3d-array texture.
        // There is no such thing as a 2d or lwbe tex with depth > 1.
        MASSERT((pTx->NumDims == 1) ||   // 1d or 1d-array
                (pTx->NumDims == 2));    // 2d or lwbe or 2d-array or lwbe-array

        pWhd[pTx->NumDims] = pTx->Layers * pTx->Faces;
    }
}

//------------------------------------------------------------------------------
void RndTexturing::ShrinkTexDimToUnity(
    const TextureObjectState * pTx
    ,GLuint * mipsize) const
{
    UINT32 leastMipmapSize = 1;
    if (pTx->HasBorder)
        leastMipmapSize += 2;  // add border (1 texel on both sides)

    for (GLuint d = 0; d < RndTexturing::txMaxDims; d++)
    {
        if (d < pTx->NumDims)
        {
            mipsize[d] = leastMipmapSize;
        }
    }
}

//------------------------------------------------------------------------------
bool RndTexturing::SkippingTxLoads
(
    const TextureObjectState * pTx
    ,GLuint * mipsize
    ,GLuint mmLevel
) const
{
    if (m_pGLRandom->m_Misc.RestartSkipMask() & RND_TSTCTRL_RESTART_SKIP_txload)
    {
        // DEBUG: download a 1-texel version of textures only.

        if (mmLevel)
            return true;  // don't load any more levels

        ShrinkTexDimToUnity(pTx, &mipsize[0]);
    }
    return false;
}

//------------------------------------------------------------------------------
RC RndTexturing::AllocateTexture(const TextureObjectState * pTx)
{
    if(pTx->HasBorder || pTx->TextureType == IsView)
    {
        // do not use texStorage API for tex with borders
        // storage for view is already allocated
        return OK;
    }

    UINT32 numLevelsToLoad = pTx->LoadMaxMM - pTx->LoadMinMM + 1;
    GLuint mipsize[txMaxDims];
    CalcMipSize(pTx, 0, &mipsize[0]);
    if (m_pGLRandom->m_Misc.RestartSkipMask() & RND_TSTCTRL_RESTART_SKIP_txload)
    {
        ShrinkTexDimToUnity(pTx, &mipsize[0]);
        numLevelsToLoad = 1;
    }

    switch ( pTx->Dim )
    {
        case GL_TEXTURE_1D:
            glTexStorage1D (pTx->Dim, numLevelsToLoad, pTx->InternalFormat,
                            mipsize[txWidth]);
            break;

        case GL_TEXTURE_1D_ARRAY_EXT:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_LWBE_MAP_EXT:
            glTexStorage2D (pTx->Dim, numLevelsToLoad, pTx->InternalFormat,
                            mipsize[txWidth], mipsize[txHeight]);
            break;

        case GL_TEXTURE_2D_ARRAY_EXT:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_LWBE_MAP_ARRAY_ARB:
            glTexStorage3D (pTx->Dim, numLevelsToLoad, pTx->InternalFormat,
                        mipsize[txWidth], mipsize[txHeight], mipsize[txDepth]);
            break;

        default:
            MASSERT(0); // unsupported texture dim
    }

    GLint result = GL_FALSE;
    glGetTexParameteriv(pTx->Dim, GL_TEXTURE_IMMUTABLE_FORMAT, &result);
    MASSERT(result == GL_TRUE);
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndTexturing::FillTex3D
(
    const TextureObjectState * pTx
    ,GLubyte * pBuf
    ,TxFill * pTxFill
)
{
    RC rc;
    CHECK_RC(AllocateTexture(pTx));
    for (GLuint mmLevel = pTx->LoadMinMM; mmLevel <= pTx->LoadMaxMM; mmLevel++)
    {
        GLuint mipsize[txMaxDims];
        CalcMipSize(pTx, mmLevel, &mipsize[0]);
        const GLuint layerPitch = mipsize[txWidth] * mipsize[txHeight] *
                pTx->LoadBpt;

        for (GLuint layer = 0; layer < mipsize[txDepth]; ++layer)
        {
          pTxFill->Fill(pBuf + layer * layerPitch,
                    mipsize[txWidth], mipsize[txHeight]);
        }

        m_pGLRandom->LogData(pTx->Index, pBuf, layerPitch * mipsize[txDepth]);

        if (SkippingTxLoads(pTx, &mipsize[0], mmLevel))
        {
            continue;
        }

        if(pTx->HasBorder)
        {
            glTexImage3D (pTx->Dim, mmLevel, pTx->InternalFormat,
                mipsize[txWidth], mipsize[txHeight], mipsize[txDepth],
                pTx->HasBorder,pTx->LoadFormat, pTx->LoadType, pBuf);
        }
        else
        {
            if(pTx->TextureType == IsView)
            {
                // commit memory of sparse texture
                glTexturePageCommitmentEXT(pTx->Id,
                                        mmLevel,
                                        0, 0, 0,
                                        mipsize[txWidth],
                                        mipsize[txHeight],
                                        mipsize[txDepth],
                                        true);
            }

            glTexSubImage3D (pTx->Dim, mmLevel, 0, 0, 0,
                mipsize[txWidth], mipsize[txHeight], mipsize[txDepth],
                pTx->LoadFormat, pTx->LoadType, pBuf);
        }
    }
    return mglGetRC();
}
//------------------------------------------------------------------------------
RC RndTexturing::FillTex2D
(
    const TextureObjectState * pTx
    ,GLubyte * pBuf
    ,TxFill * pTxFill
)
{
    RC rc;
    CHECK_RC(AllocateTexture(pTx));
    for (GLuint mmLevel = pTx->LoadMinMM; mmLevel <= pTx->LoadMaxMM; mmLevel++)
    {
        GLuint mipsize[txMaxDims];
        CalcMipSize(pTx, mmLevel, &mipsize[0]);

        pTxFill->Fill(pBuf, mipsize[txWidth], mipsize[txHeight]);

        m_pGLRandom->LogData(pTx->Index, pBuf, mipsize[txWidth] *
                mipsize[txHeight] * pTx->LoadBpt);

        if (SkippingTxLoads(pTx, &mipsize[0], mmLevel))
            continue;

        if(pTx->HasBorder)
        {
            glTexImage2D (pTx->Dim, mmLevel, pTx->InternalFormat,
                mipsize[txWidth], mipsize[txHeight],
                pTx->HasBorder, pTx->LoadFormat, pTx->LoadType, pBuf);
        }
        else
        {
            glTexSubImage2D (pTx->Dim, mmLevel, 0, 0,
                mipsize[txWidth], mipsize[txHeight],
                pTx->LoadFormat, pTx->LoadType, pBuf);
        }
    }
    return mglGetRC();
}
//------------------------------------------------------------------------------
RC RndTexturing::FillTex1D
(
    const TextureObjectState * pTx
    ,GLubyte * pBuf
    ,TxFill * pTxFill
)
{
    RC rc;
    CHECK_RC(AllocateTexture(pTx));
    for (GLuint mmLevel = pTx->LoadMinMM; mmLevel <= pTx->LoadMaxMM; mmLevel++)
    {
        GLuint mipsize[txMaxDims];
        CalcMipSize(pTx, mmLevel, &mipsize[0]);

        pTxFill->Fill(pBuf, mipsize[txWidth], 1);

        m_pGLRandom->LogData(pTx->Index, pBuf, mipsize[txWidth] * pTx->LoadBpt);

        if (SkippingTxLoads(pTx, &mipsize[0], mmLevel))
            continue;

        if(pTx->HasBorder)
        {
            glTexImage1D (pTx->Dim, mmLevel, pTx->InternalFormat, mipsize[txWidth],
                pTx->HasBorder,pTx->LoadFormat, pTx->LoadType, pBuf);
        }
        else
        {
            glTexSubImage1D (pTx->Dim, mmLevel, 0,  mipsize[txWidth],
                pTx->LoadFormat, pTx->LoadType, pBuf);
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndTexturing::FillTexLwbeArray
(
    const TextureObjectState * pTx
    ,GLubyte * pBuf
    ,TxFill * pTxFill
)
{
    RC rc;
    CHECK_RC(AllocateTexture(pTx));
    for (GLuint mmLevel = pTx->LoadMinMM; mmLevel <= pTx->LoadMaxMM; mmLevel++)
    {
        GLuint mipsize[txMaxDims];
        CalcMipSize(pTx, mmLevel, &mipsize[0]);

        const GLuint facePitch = mipsize[txWidth] * mipsize[txHeight] *
                pTx->LoadBpt;

        for (GLuint layer = 0; layer < pTx->Layers; layer++)
        {
            for (GLuint face = 0; face < pTx->Faces; face++)
            {
                pTxFill->Fill (pBuf + (face + layer*pTx->Faces) * facePitch,
                        mipsize[txWidth], mipsize[txHeight]);
            }
        }

        m_pGLRandom->LogData(pTx->Index, pBuf,
                facePitch * pTx->Layers * pTx->Faces);

        if (SkippingTxLoads(pTx, &mipsize[0], mmLevel))
            continue;

        if(pTx->HasBorder)
        {
            glTexImage3D (pTx->Dim, mmLevel, pTx->InternalFormat,
                mipsize[txWidth], mipsize[txHeight], pTx->Layers * pTx->Faces,
                pTx->HasBorder,pTx->LoadFormat, pTx->LoadType, pBuf);
        }
        else
        {
            glTexSubImage3D (pTx->Dim, mmLevel, 0, 0, 0,
                mipsize[txWidth], mipsize[txHeight], mipsize[txDepth],
                pTx->LoadFormat, pTx->LoadType, pBuf);
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndTexturing::FillTexLwbe
(
    const TextureObjectState * pTx
    ,GLubyte * pBuf
    ,TxFill * pTxFill
)
{
    RC rc;
    CHECK_RC(AllocateTexture(pTx));
    for (GLuint mmLevel = pTx->LoadMinMM; mmLevel <= pTx->LoadMaxMM; mmLevel++)
    {
        GLuint mipsize[txMaxDims];
        CalcMipSize(pTx, mmLevel, &mipsize[0]);

        for (GLuint face = 0; face < pTx->Faces; face++)
        {
            pTxFill->Fill (pBuf, mipsize[txWidth], mipsize[txHeight]);

            m_pGLRandom->LogData(pTx->Index, pBuf, mipsize[txWidth] *
                    mipsize[txHeight] * pTx->LoadBpt);

            if (SkippingTxLoads(pTx, &mipsize[0], mmLevel))
                continue;

            if(pTx->HasBorder)
            {
                glTexImage2D (GL_TEXTURE_LWBE_MAP_POSITIVE_X_EXT + face, mmLevel,
                    pTx->InternalFormat, mipsize[txWidth], mipsize[txHeight],
                    pTx->HasBorder,pTx->LoadFormat, pTx->LoadType, pBuf);
            }
            else
            {
                glTexSubImage2D (GL_TEXTURE_LWBE_MAP_POSITIVE_X_EXT + face,
                    mmLevel, 0, 0, mipsize[txWidth], mipsize[txHeight],
                    pTx->LoadFormat, pTx->LoadType, pBuf);
            }
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
static UINT64 TexBytesLevel
(
    const RndTexturing::TextureObjectState * pTx
    ,GLuint mmLevel
)
{
    GLuint mipsize[RndTexturing::txMaxDims];
    CalcMipSize(pTx, mmLevel, &mipsize[0]);

    return (UINT64)mipsize[RndTexturing::txWidth] *
           (UINT64)mipsize[RndTexturing::txHeight] *
           (UINT64)mipsize[RndTexturing::txDepth] *
           (UINT64)pTx->LoadBpt;
}

//------------------------------------------------------------------------------
static UINT64 TexBytesTotal
(
    const RndTexturing::TextureObjectState * pTx
)
{
    UINT64 sum = 0;

    for (GLuint mmLevel = 0; mmLevel <= pTx->LoadMaxMM; mmLevel++)
    {
        sum += TexBytesLevel (pTx, mmLevel);
    }

    if (pTx->Dim == GL_TEXTURE_LWBE_MAP_EXT)
        sum *= 6;

    return sum;
}

//------------------------------------------------------------------------------
// Render 3 different circles of decreasing size. Each circle will have several
// slices of different colors, with each color blending into the next.
// Note: You must have GL_DEPTH_TEST disabled for this geometry to render
// properly.
RC RndTexturing::RenderCircles(TextureObjectState * pTx)
{
    static const GLfloat fColors[8][4] =
    {
        {0.0,0.0,0.0,1.0},   // opaque black
        {1.0,0.0,0.0,1.0},   // opaque red
        {1.0,1.0,0.0,1.0},   // opaque yellow
        {0.0,1.0,0.0,1.0},   // opaque green
        {0.0,1.0,1.0,1.0},   // opaque cyan
        {0.0,0.0,1.0,1.0},   // opaque blue
        {1.0,0.0,1.0,1.0},   // opaque majenta
        {1.0,1.0,1.0,1.0},   // opaque white
    };

    const double w = 1.0;
    const double h = 1.0;
    GLfloat X;
    GLfloat Y;
    GLfloat rad = static_cast<GLfloat>(LOW_PREC_PI/180);
    GLint   circle;
    GLfloat radius[3];
    GLfloat centerX;
    GLfloat centerY;
    GLuint  clrIdx = m_pFpCtx->Rand.GetRandom();

    // 1st circle needs to cover the corners of the texture image
    radius[0] = (float) (0.5 * sqrt(h*h + w*w));

    // 2nd circle extends to larger edge
    radius[1] = (float) (max(w, h)/2.0);

        // 3rd circle extends to the smaller edge or 1/2 of square
    if (w == h)
        radius[2] = (float)(w/4.0);
    else
        radius[2] = (float) (min(w, h)/2.0);

    centerX = (float)(w/2);
    centerY = (float)(h/2);
    glFrontFace(GL_CW);
    for( circle = 0; circle < 3; circle++)
    {
        glBegin(GL_TRIANGLE_FAN);
        glColor4fv(fColors[clrIdx %8]);
        glVertex2f(centerX, centerY);
        for(int i = 0; i <= 360; i+= 10)
        {
            X = centerX + radius[circle] * cos(i*rad);
            Y = centerY + radius[circle] * sin(i*rad);
            glColor4fv(fColors[clrIdx %8]);
            glVertex2f(X, Y);
            clrIdx++;
        }
        glEnd();
        clrIdx++;
    }

    return mglGetRC();
}

//-----------------------------------------------------------------------------
RC RndTexturing::RenderPixels
(
    TextureObjectState * pTx
    ,GLuint fillMode
)
{
    static const GLfloat fColors[9][4] =
    {
        {1.0,0.0,0.0,1.0},   // opaque red
        {1.0,1.0,0.0,1.0},   // opaque yellow
        {0.0,1.0,0.0,1.0},   // opaque green
        {0.0,1.0,1.0,1.0},   // opaque cyan
        {0.0,0.0,1.0,1.0},   // opaque blue
        {1.0,0.0,1.0,1.0},   // opaque majenta
        {1.0,1.0,1.0,1.0},   // opaque white
        {1.0,0.5,1.0,1.0},   // opaque pink
        {0.5,0.5,0.5,1.0}    // opaque grey
    };

    // fill the rect with floats using a solid,striped, or random pattern
    const GLuint w = pTx->BaseSize[txWidth];
    const GLuint h = pTx->BaseSize[txHeight];
    vector<GLfloat> data;

    data.reserve(w*h*4);

    GLuint index = 0;

    if( fillMode == RND_PIXEL_FILL_MODE_solid)
        index = m_pFpCtx->Rand.GetRandom() % 9;

    for (GLuint row = 0; row < h; row++)
    {
        if( fillMode == RND_PIXEL_FILL_MODE_stripes)
            index = m_pFpCtx->Rand.GetRandom() % 9;

        for (GLuint col = 0; col < w; col++)
        {
            if( fillMode == RND_PIXEL_FILL_MODE_random)
                index = m_pFpCtx->Rand.GetRandom() % 9;

            for(int i = 0; i< 4; i++)
                data.push_back(fColors[index][i]);
        }
    }

    glWindowPos3i(0, 0, 0);
    glDrawPixels(w,
                 h,
                 GL_RGBA,   // dataFormat
                 GL_FLOAT,  // dataType
                 &data[0]);

    return mglGetRC();
}

//-----------------------------------------------------------------------------
RC RndTexturing::RenderDebugFill
(
    TextureObjectState * pTx
)
{
    const GLuint w = pTx->BaseSize[txWidth];
    const GLuint h = pTx->BaseSize[txHeight];
    const GLuint samps = pTx->MsSamples;

    for (GLuint samp = 0; samp < samps; samp++)
    {
        vector<GLbyte> data;
        data.reserve(w*h*4);

        for (GLuint row = 0; row < h; row++)
        {
            for (GLuint col = 0; col < w; col++)
            {
                data.push_back((GLbyte)col); // red = texture U coord
                data.push_back((GLbyte)row); // green == texture V coord
                data.push_back((GLbyte)samp); // blue == sample
                data.push_back((GLbyte)0x55); // alpha == magic value
            }
        }
        glWindowPos3i(0, 0, 0);
        glEnable(GL_SAMPLE_MASK_LW);
        glSampleMaskIndexedLW(0, (GLbitfield)(1<<samp));
        glDrawPixels(w,
                     h,
                     GL_RGBA,   // dataFormat
                     GL_BYTE,  // dataType
                     &data[0]);
    }
    glDisable(GL_SAMPLE_MASK_LW);
    glSampleMaskIndexedLW(0, (GLbitfield)~0);

    return mglGetRC();
}

//-----------------------------------------------------------------------------
// Render a series of squares, decreasing in size with each vertex having a
// different color.
RC RndTexturing::RenderSquares
(
    TextureObjectState * pTx
)
{
    static const GLfloat fColors[8][4] =
    {
        {0.0,0.0,0.0,1.0},   // opaque black
        {1.0,0.0,0.0,1.0},   // opaque red
        {1.0,1.0,0.0,1.0},   // opaque yellow
        {0.0,1.0,0.0,1.0},   // opaque green
        {0.0,1.0,1.0,1.0},   // opaque cyan
        {0.0,0.0,1.0,1.0},   // opaque blue
        {1.0,0.0,1.0,1.0},   // opaque majenta
        {1.0,1.0,1.0,1.0},   // opaque white
    };

    const int numSquares = min(pTx->BaseSize[txWidth], pTx->BaseSize[txHeight]);
    const GLdouble Ystep = 0.5 / numSquares;
    const GLdouble Xstep = 0.5 / numSquares;

    glFrontFace(GL_CCW);

    for(int i = 0; i < numSquares; i++)
    {
        glBegin(GL_QUADS);

        GLuint  clrIdx;
        clrIdx = m_pFpCtx->Rand.GetRandom() % 8;
        glColor4fv(fColors[clrIdx]);
        glVertex2d(0.0 + (i+1)*Xstep, 1.0 - i*Ystep);  // top left

        clrIdx = m_pFpCtx->Rand.GetRandom() % 8;
        glColor4fv(fColors[clrIdx]);
        glVertex2d(0.0 + i*Xstep, 0.0 + (i+1)*Ystep);  // bottom left

        clrIdx = m_pFpCtx->Rand.GetRandom() % 8;
        glColor4fv(fColors[clrIdx]);
        glVertex2d(1.0 - (i+1)*Xstep, 0.0 + i*Ystep);  // bottom right

        clrIdx = m_pFpCtx->Rand.GetRandom() % 8;
        glColor4fv(fColors[clrIdx]);
        glVertex2d(1.0 - i*Xstep, 1.0 - (i+1)*Ystep);  // top right

        glEnd();
    }
    return mglGetRC();
}

//-----------------------------------------------------------------------------
// Create the required buffer objects and datastore to render a multisampled
// texture image.
RC RndTexturing::SetupMsFbo
(
    TextureObjectState * pTx
)
{
    MASSERT( pTx->MsColorRboId == 0);
    MASSERT( pTx->MsFboId == 0);
    MASSERT( pTx->Id == 0);

    const GLuint w = pTx->BaseSize[txWidth];
    const GLuint h = pTx->BaseSize[txHeight];

    GLint maxSamples = INT_MAX;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    pTx->MsSamples = min(m_Pickers[RND_TX_MS_MAX_SAMPLES].Pick(),
            static_cast<UINT32>(maxSamples));

    if (pTx->LoadType == GL_FLOAT)
    {
        // Tesla will not support FP32 formats with more that 4 samples.
        // Reexamine for Fermi!
        pTx->MsSamples = min(pTx->MsSamples, 4);
    }
    else
    {
        pTx->MsSamples = min(pTx->MsSamples, 16);
    }

    //-------------------------------
    // Setup the color renderbuffer
    //-------------------------------
    glGenRenderbuffersEXT(1,&pTx->MsColorRboId);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,pTx->MsColorRboId);

    // create a multisample datastore for the "lwrrently bound" color
    // renderbuffer object
    glRenderbufferStorageMultisampleEXT (GL_RENDERBUFFER_EXT, pTx->MsSamples,
            pTx->InternalFormat, w, h);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

    //------------------
    // Setup the texture
    //------------------
    glGenTextures(1, &pTx->Id);
    glBindTexture(GL_TEXTURE_RENDERBUFFER_LW, pTx->Id);

    // attach the renderbuffer object "pTx->MsColorRboId" to the active
    // renderbuffer texture "pTx->Id"
    glTexRenderbufferLW(GL_TEXTURE_RENDERBUFFER_LW, pTx->MsColorRboId);
    glBindTexture(GL_TEXTURE_RENDERBUFFER_LW, 0);

    //----------------------
    // Setup the framebuffer
    //----------------------
    glGenFramebuffersEXT(1, &pTx->MsFboId);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pTx->MsFboId);

    // Attach the renderbuffer object to the framebuffer object at the
    // GL_COLOR_ATTACHMENT0_EXT attachment point
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,      // target
                                 GL_COLOR_ATTACHMENT0_EXT,// attachment
                                 GL_RENDERBUFFER_EXT,     // renderbuffertarget
                                 pTx->MsColorRboId        // renderbuffer
                                );

    // specify what color buffer we are going to render to.
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT );
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT );

    // OK at this point the texture object and the renderbuffer datastore should
    // be bound to the renderbuffer, and the renderbuffer should be bound to the
    // framebuffer.

    // Did we set up the framebuffer correctly?
    return mglCheckFbo();
}

//------------------------------------------------------------------------------
// Create multisample GL texture object.
// Note: Creating a multisample texture object requires enabling multisampling
//       and then rendering some geometry to a RENDERBUFFER object.
RC RndTexturing::GenerateMsTexture(TextureObjectState * pTx)
{
    RC rc;
    GLint lwrFbo    = 0;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_ALL_ATTRIB_BITS);

    // Capture the current fbo and create all Multisample objects
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &lwrFbo);
    CHECK_RC(SetupMsFbo(pTx));

    // Bind to the Multisample buffer object
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, pTx->MsFboId);

    glEnable(GL_MULTISAMPLE);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glMatrixMode(GL_PROJECTION);            // setup a simple world to match
    glPushMatrix();                         // the size of the texture object
    glLoadIdentity();

    glOrtho(0,1,0,1,-1,1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glClearColor(1,0.5,1,1);
    glClear(GL_COLOR_BUFFER_BIT);

    // render some geometry to the texture object
    switch (pTx->PatternCode)
    {
        default:
        case RND_TX_NAME_MsCircles:
            CHECK_RC(RenderCircles(pTx));
            break;
        case RND_TX_NAME_MsSquares:
            CHECK_RC(RenderSquares(pTx));
            break;
        case RND_TX_NAME_MsRandom:
            CHECK_RC(RenderPixels(pTx,RND_PIXEL_FILL_MODE_random));
            break;
        case RND_TX_NAME_MsStripes:
            CHECK_RC(RenderPixels(pTx,RND_PIXEL_FILL_MODE_stripes));
            break;
    }
    if (m_Pickers[RND_TX_MSTEX_DEBUG_FILL].Pick())
        CHECK_RC(RenderDebugFill(pTx));

    glFinish();

    // cleanup
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, lwrFbo);

    glPopClientAttrib();
    glPopAttrib();

    return mglGetRC();
}

//------------------------------------------------------------------------------
static bool isTexTargetCompatibleWithView(GLenum texDim)
{
    // TEXTURE_2D_ARRAY is compatible with TEXTURE_2D, 2D_ARRAY, LWBE, LWBE ARRAY
    if (texDim == GL_TEXTURE_2D ||
        texDim == GL_TEXTURE_2D_ARRAY_EXT ||
        texDim == GL_TEXTURE_LWBE_MAP_EXT ||
        texDim == GL_TEXTURE_LWBE_MAP_ARRAY_ARB)
        return true;
    else
        return false;
}

//------------------------------------------------------------------------------
UINT32 RndTexturing::NumLayersForView (const TextureObjectState * pTx)
{
    // View treats faces of a lwbe/lwbe array as layers
    UINT32 numLayers = pTx->Layers ? pTx->Layers : 1;
    if (pTx->Dim == GL_TEXTURE_LWBE_MAP_ARRAY_ARB ||
            pTx->Dim == GL_TEXTURE_LWBE_MAP_EXT)
    {
        numLayers *= pTx->Faces;
    }
    return numLayers;
}

//------------------------------------------------------------------------------
bool RndTexturing::CanUseTextureView (const TextureObjectState * pTx, UINT32 viewID)
{
    // is this is a valid view?
    if (m_SparseTextures[viewID].TextureType != IsView)
        return false;

    // read only textures must be used for 'view'
    // textures with border are not supported as they are not created using texStorage
    if ((!isTexTargetCompatibleWithView(pTx->Dim)) ||
       (pTx->RsvdAccess != iuaRead) ||
       (pTx->HasBorder))
        return false;

    // texture must match view's class size
    INT64 viewTexSizeClass = 0, lwrrentTexSizeClass = 0;
    glGetInternalformati64v(m_SparseTextures[viewID].Dim,
                            m_SparseTextures[viewID].InternalFormat, GL_VIEW_COMPATIBILITY_CLASS,
                            sizeof(viewTexSizeClass), &viewTexSizeClass);
    glGetInternalformati64v(pTx->Dim, pTx->InternalFormat, GL_VIEW_COMPATIBILITY_CLASS,
                            sizeof(lwrrentTexSizeClass), &lwrrentTexSizeClass);
    if (lwrrentTexSizeClass != viewTexSizeClass)
        return false;

    if (lwrrentTexSizeClass == 0 || viewTexSizeClass == 0)  //unsupported formats for texture viewing
        return false;

    // Size must be lesser than that of the view
    GLuint mipsize[txMaxDims];
    CalcMipSize(&m_SparseTextures[viewID], m_SparseTextures[viewID].LoadMinMM, &mipsize[0]);
    if((pTx->BaseSize[txWidth] > mipsize[txWidth]) ||
       (pTx->BaseSize[txHeight] > mipsize[txHeight]) ||
       (pTx->BaseSize[txDepth] > mipsize[txDepth]))
    {
        return false;
    }

    // for lwbe maps to use views, the view texture must have 6 layers
    if (pTx->Dim == GL_TEXTURE_LWBE_MAP_EXT &&
        (m_SparseTextures[viewID].Layers != pTx->Faces))
    {
        return false;
    }

    UINT32 numLayers = NumLayersForView(pTx);
    if (numLayers > m_SparseTextures[viewID].Layers)
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
RC RndTexturing::UseTextureViews (TextureObjectState * pTx)
{
    RC rc;

    for (UINT32 viewID = 0; viewID < RND_TX_NUM_SPARSE_TEX; viewID++)
    {
        if (CanUseTextureView(pTx, viewID))
        {
            UINT32 numLayers = NumLayersForView(pTx);

            // Find what mipmap levels of the view we must use
            UINT32 minMMLevel = 0;
            for (minMMLevel = m_SparseTextures[viewID].LoadMinMM;
                    minMMLevel <= m_SparseTextures[viewID].LoadMaxMM; minMMLevel++)
            {
                if ((m_SparseTextures[viewID].BaseSize[txWidth] >> minMMLevel) <= pTx->BaseSize[txWidth] ||
                    (m_SparseTextures[viewID].BaseSize[txHeight] >> minMMLevel) <= pTx->BaseSize[txHeight])
                {
                    break;
                }
            }

            pTx->BaseSize[txWidth] = m_SparseTextures[viewID].BaseSize[txWidth] >> minMMLevel;
            pTx->BaseSize[txHeight] = m_SparseTextures[viewID].BaseSize[txHeight] >> minMMLevel;
            MASSERT(pTx->LoadMaxMM <= (m_SparseTextures[viewID].LoadMaxMM - minMMLevel));
            pTx->LoadMaxMM = m_SparseTextures[viewID].LoadMaxMM - minMMLevel;

            glTextureView(pTx->Id,               // current texture ID
                          pTx->Dim,              // current texture target
                          m_SparseTextures[viewID].Id,  // original texture ID
                          pTx->InternalFormat,   // internal format
                          minMMLevel,            // min level
                          pTx->LoadMaxMM  + 1,   // num levels
                          0,                     // min layer
                          numLayers);            // num layers

            GLint result = GL_FALSE;
            glBindTexture(pTx->Dim, pTx->Id);
            glGetTexParameteriv(pTx->Dim, GL_TEXTURE_IMMUTABLE_FORMAT, &result);
            glBindTexture(pTx->Dim, 0);
            if(result == GL_TRUE)
            {
                // going to use view for this texture
                // copy common tex properties from sparse texture and return
                pTx->PatternCode = m_SparseTextures[viewID].PatternCode;
                pTx->DepthTexMode = m_SparseTextures[viewID].DepthTexMode;
                pTx->TextureType = IsUsingView;
                pTx->SparseTexID = viewID;
                return mglGetRC();
            }
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndTexturing::LoadAstcTexture (TextureObjectState * pTx)
{
    RC rc;

    ModsGL::BindTexture tex(pTx->Dim, pTx->Id);

    glTexParameteri(pTx->Dim, GL_TEXTURE_BASE_LEVEL, pTx->LoadMinMM);
    glTexParameteri(pTx->Dim, GL_TEXTURE_MAX_LEVEL, pTx->LoadMaxMM);

    Utility::ASTCData *astcData;
    CHECK_RC(AstcCache::GetInstance().GetData(pTx->InternalFormat, &astcData));

    UINT08 blockX = mglGetAstcFormatBlockX(pTx->InternalFormat);
    UINT08 blockY = mglGetAstcFormatBlockY(pTx->InternalFormat);
    UINT08 blockZ = 1;

    if ((blockX == 0) || (blockY == 0))
        return RC::SOFTWARE_ERROR;

    GLenum target = (pTx->Dim == GL_TEXTURE_LWBE_MAP_EXT ? GL_TEXTURE_LWBE_MAP_POSITIVE_X : pTx->Dim);

    // We'll reuse the same texture or parts of it when loading mipmaps/lwbemaps
    for(GLuint face = 0; face < pTx->Faces; face++)
    {
        UINT32 mmWidth = pTx->BaseSize[txWidth];
        UINT32 mmHeight = pTx->BaseSize[txHeight];
        UINT32 mmDepth = pTx->BaseSize[txDepth];

        for(GLuint mmLevel = pTx->LoadMinMM; mmLevel <= pTx->LoadMaxMM; mmLevel++)
        {
            GLsizei compressedFileSize = CEIL_DIV(mmWidth, blockX) *
                                        CEIL_DIV(mmHeight, blockY) *
                                        CEIL_DIV(mmDepth, blockZ) *
                                        Utility::ASTC_BLOCK_SIZE;

            glCompressedTexImage2D(target + face,
                mmLevel,
                pTx->InternalFormat,
                mmWidth, mmHeight,
                0,
                compressedFileSize,
                &astcData->imageData[0]);

            mmWidth = (mmWidth > 1 ? (mmWidth >> 1) : 1 );
            mmHeight = (mmHeight > 1 ? (mmHeight >> 1) : 1);
        }
    }

    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndTexturing::LoadTexture (TextureObjectState * pTx)
{
    RC rc;
    glBindTexture(pTx->Dim, pTx->Id);

    glTexParameteri(pTx->Dim, GL_TEXTURE_BASE_LEVEL, pTx->LoadMinMM);
    glTexParameteri(pTx->Dim, GL_TEXTURE_MAX_LEVEL, pTx->LoadMaxMM);

    CHECK_RC(mglGetRC());

    //Alloc a buffer to use for the largest layer/face/mmlevel
    size_t bufNeeded = (size_t) TexBytesLevel (pTx, pTx->LoadMinMM);

#if defined(DEBUG)
    const GLuint checkOverrun = 1;
#else
    const GLuint checkOverrun = 0;
#endif

    vector<GLubyte> Buf(bufNeeded + checkOverrun, 0);

    const GLubyte bufOverrunGuard = 42;
    if (checkOverrun)
        Buf[bufNeeded] = bufOverrunGuard;

    // Allocate an appropriate pattern-filler object.
    unique_ptr<TxFill> pTxFill (TxFillFactory (pTx));

    // Load each mmlevel/layer/face/etc to the GL driver.
    switch ( pTx->Dim )
    {
        case GL_TEXTURE_1D:
            CHECK_RC(FillTex1D (pTx, &Buf[0], pTxFill.get()));
            break;

        case GL_TEXTURE_1D_ARRAY_EXT:
        case GL_TEXTURE_2D:
            CHECK_RC(FillTex2D (pTx, &Buf[0], pTxFill.get()));
            break;

        case GL_TEXTURE_2D_ARRAY_EXT:
        case GL_TEXTURE_3D:
            CHECK_RC(FillTex3D (pTx, &Buf[0], pTxFill.get()));
            break;

        case GL_TEXTURE_LWBE_MAP_EXT:
            CHECK_RC(FillTexLwbe(pTx, &Buf[0], pTxFill.get()));
            break;

        case GL_TEXTURE_LWBE_MAP_ARRAY_ARB:
            CHECK_RC(FillTexLwbeArray(pTx, &Buf[0], pTxFill.get()));
            break;

        default:
            MASSERT(0); // unsupported texture dim
    }

    glBindTexture(pTx->Dim, 0);

    MASSERT(Buf[bufNeeded] == bufOverrunGuard);

    CHECK_RC(mglGetRC());
    return rc;
}

//------------------------------------------------------------------------------
static GLenum GetProxyDim (GLenum dim)
{
    switch (dim)
    {
        case GL_TEXTURE_1D:             return GL_PROXY_TEXTURE_1D;
        case GL_TEXTURE_1D_ARRAY_EXT:   return GL_PROXY_TEXTURE_1D_ARRAY_EXT;
        case GL_TEXTURE_2D:             return GL_PROXY_TEXTURE_2D;
        case GL_TEXTURE_LWBE_MAP_EXT:   return GL_PROXY_TEXTURE_LWBE_MAP_EXT;
        case GL_TEXTURE_2D_ARRAY_EXT:   return GL_PROXY_TEXTURE_2D_ARRAY_EXT;
        case GL_TEXTURE_LWBE_MAP_ARRAY_ARB:
                                    return GL_PROXY_TEXTURE_LWBE_MAP_ARRAY_ARB;
        case GL_TEXTURE_3D:             return GL_PROXY_TEXTURE_3D;

        default:
            MASSERT(!"Invalid tx dim enum");
            return 0;
    }
}

//------------------------------------------------------------------------------
static void TexFixSize
(
    RndTexturing::TextureObjectState * pTx
)
{
    if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
        pTx->LoadMaxMM = 0;
    else
        pTx->LoadMaxMM = FindMaxMM (pTx->BaseSize);

    pTx->LoadMinMM = 0;

    if (pTx->MaxMM == 0)
    {
        pTx->MaxMM = pTx->LoadMaxMM;
    }

    pTx->MaxMM  = min(pTx->MaxMM, pTx->LoadMaxMM);
    pTx->BaseMM = min(pTx->BaseMM, pTx->MaxMM);
}

//------------------------------------------------------------------------------
void RndTexturing::TexSetDim
(
    RndTexturing::TextureObjectState * pTx
)
{
    GLuint minSize   = 1;
    GLuint maxSize   = m_pGLRandom->Max2dTextureSize()-(2*pTx->HasBorder);

    pTx->Layers = 0;  // 0 for non-array tex
    pTx->Faces  = 1;  // 1 for non-lwbe tex

    switch (pTx->Dim)
    {
        case GL_TEXTURE_1D:
            pTx->NumDims = 1;
            pTx->Attribs = Is1d;
            break;

        case GL_TEXTURE_1D_ARRAY_EXT:
            pTx->NumDims = 1;
            pTx->Layers  = pTx->BaseSize[txHeight];
            pTx->Attribs = IsArray1d;
            break;

        case GL_TEXTURE_2D:
            pTx->NumDims = 2;
            pTx->Attribs = Is2d;
            break;

        case GL_TEXTURE_2D_ARRAY_EXT:
            pTx->NumDims = 2;
            pTx->Layers  = pTx->BaseSize[txDepth];
            pTx->Attribs = IsArray2d;
            break;

        case GL_TEXTURE_3D:
            maxSize = m_pGLRandom->Max3dTextureSize()-(2*pTx->HasBorder);
            pTx->NumDims = 3;
            pTx->Attribs = Is3d;
            break;

        case GL_TEXTURE_LWBE_MAP_EXT:
            maxSize = m_pGLRandom->MaxLwbeTextureSize()-(2*pTx->HasBorder);
            pTx->NumDims = 2;
            pTx->Faces   = 6;
            pTx->Attribs = IsLwbe;
            break;

        case GL_TEXTURE_LWBE_MAP_ARRAY_ARB:
            maxSize = m_pGLRandom->MaxLwbeTextureSize()-(2*pTx->HasBorder);
            pTx->NumDims = 2;
            pTx->Faces   = 6;
            pTx->Layers  = pTx->BaseSize[txDepth];
            pTx->Attribs = IsArrayLwbe;
            break;

        case GL_TEXTURE_RENDERBUFFER_LW:
            // Multisample textures can get very large very fast depending on
            // the number of samples.
            maxSize = 64;
            minSize = 2;
            pTx->NumDims = 2;
            pTx->Attribs = Is2dMultisample;
            break;

        default:
            MASSERT(0); // unsupported texture dim
    }

    for (UINT32 d = 0; d < 3; d++)
    {
        if (d >= pTx->NumDims)
        {
            pTx->BaseSize[d] = 1;
        }
        else
        {
            pTx->BaseSize[d] = MINMAX(minSize, pTx->BaseSize[d], maxSize);
        }
    }

    if (pTx->Faces > 1)
    {
        // Lwbe-maps must have square faces.
        if (mglIsFormatAstc(pTx->InternalFormat))
        {
            pTx->BaseSize[txHeight] = MIN(pTx->BaseSize[txHeight], pTx->BaseSize[txWidth]);
            pTx->BaseSize[txWidth] = MIN(pTx->BaseSize[txHeight], pTx->BaseSize[txWidth]);
        }
        else
        {
            pTx->BaseSize[txHeight] = pTx->BaseSize[txWidth];
        }
    }

    if (pTx->Attribs & ADimFlags)
    {
        pTx->Layers = max(pTx->Layers, 1U);

        if (pTx->Faces * pTx->Layers > m_pGLRandom->MaxArrayTextureLayerSize())
            pTx->Layers = m_pGLRandom->MaxArrayTextureLayerSize() / pTx->Faces;
    }
    else
    {
        pTx->Layers = 0;
    }

    TexFixSize(pTx);
}

//------------------------------------------------------------------------------
static RC TexShrinkBaseAndLayers
(
    RndTexturing::TextureObjectState * pTx,
    const INT32 requiredMMLevel
)
{
    UINT64 origSize = TexBytesTotal(pTx);
    // We'd like to retain test coverage of the high bits of the texture coord.
    //
    // So we'll preferentially preserve the *largest dim* which will tend to
    // make textures less "square" and give us some chance of using max-width
    // or height or depth or layers.

    // Find the largest shrinkable dim value (1 is not shrinkable!).

    GLuint largestDimValue = max (2U, pTx->Layers);

    for (int d = 0; d < RndTexturing::txMaxDims; d++)
    {
        largestDimValue = max (largestDimValue, pTx->BaseSize[d]);
    }

    // Shrink all "medium" dims.
    // Shrink all but one "large" dim.
    //  - In array textures, prefer to keep Layers large (and shrink w,h)
    //  - In 2d textures, prefer to keep height large (and shrink w)
    //  - In 3d textures, prefer to keep depth large (and shrink w,h)

    GLuint numLarge = 0;        // Count where (1 < dim && dim == largest)
    GLuint numMedium = 0;       // Count where (1 < dim && dim < largest)

    if (pTx->Layers > 1)
    {
        if (pTx->Layers == largestDimValue)
        {
            ++numLarge;
        }
        else
        {
            ++numMedium;
            pTx->Layers >>= 1;
        }
    }
    const GLuint minForMM = (requiredMMLevel > 0) ? (1<<requiredMMLevel) : 1;
    for (int d = RndTexturing::txMaxDims-1; d >= 0; --d)
    {
        if (pTx->BaseSize[d] > minForMM)
        {
            if (pTx->BaseSize[d] == largestDimValue)
            {
                ++numLarge;
                if (numLarge > 1)
                {
                    pTx->BaseSize[d] >>= 1;
                }
            }
            else
            {
                ++numMedium;
                pTx->BaseSize[d] >>= 1;
            }
        }
    }

    if (0 == numMedium)
    {
        if (0 == numLarge)
        {
            // Can't shrink, all dims are <= 1.
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        else if (1 == numLarge)
        {
            // If there are no medium dims and only one large dim, we must
            // shrink the large dim.
            if (pTx->Layers == largestDimValue)
            {
                pTx->Layers >>= 1;
            }
            for (int d = 0; d < RndTexturing::txMaxDims; d++)
            {
                if (pTx->BaseSize[d] == largestDimValue)
                    pTx->BaseSize[d] >>= 1;
            }
        }
        // Else numLarge is >1 and we have succeeded in shrinking something.
    }

    // In lwbe or lwbe-array textures, force w=h (use smaller of them).
    if (pTx->Faces > 1)
    {
        const GLuint wh = min (pTx->BaseSize[0], pTx->BaseSize[1]);
        pTx->BaseSize[0]  = wh;
        pTx->BaseSize[1] = wh;
    }

    bool bMeetsMMReq = (requiredMMLevel < 0);
    for( int d = 0; d < RndTexturing::txMaxDims && !bMeetsMMReq; d++)
    {
        bMeetsMMReq = (pTx->BaseSize[d] >= (GLuint)(1 << requiredMMLevel));
    }
    if(!bMeetsMMReq)
    {
        pTx->BaseSize[0] = (1 << requiredMMLevel);
        if(pTx->Faces > 1)
        {
            pTx->BaseSize[1] = pTx->BaseSize[0];
        }
    }
    TexFixSize(pTx);

    // Return success if we were able to reduce the size.
    if( origSize <= TexBytesTotal(pTx))
        return RC::CANNOT_ALLOCATE_MEMORY;
    else
        return OK;
}

//------------------------------------------------------------------------------
RC RndTexturing::ShrinkTexToFit
(
    TextureObjectState * pTx
    ,const UINT64 txMemoryAvailable
    ,const INT32 requiredMMLevel
    ,bool * usingNewTxMemory
)
{
    RC rc;

    if (mglIsFormatAstc(pTx->InternalFormat))
    {
        *usingNewTxMemory = false;
        return OK;
    }

    // Shrink to fit the artificial GLRandom total-texture-memory limit.
    while (txMemoryAvailable < TexBytesTotal(pTx))
    {
        CHECK_RC( TexShrinkBaseAndLayers(pTx,requiredMMLevel) );
    }

    if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
        return OK;

    // if we can use an existing view, dont care about creating new tex memory
    for (UINT32 i = 0; i < RND_TX_NUM_SPARSE_TEX; i++)
    {
        if (CanUseTextureView(pTx, i))
        {
            *usingNewTxMemory = false;
            return OK;
        }
    }
    const GLenum proxyDim = GetProxyDim (pTx->Dim);

    // Shrink to fit into FB memory (as reported by GL driver).
    while (1)
    {
        GLint queryLod = pTx->LoadMaxMM ? 1 : 0;    //in proxy mechanism, setting mmLevel > 0 checks
                                                    //memory for all mipmap levels

        GLuint mipsize[txMaxDims];
        CalcMipSize(pTx, queryLod, &mipsize[0]);

        switch ( pTx->Dim )
        {
            case GL_TEXTURE_1D:
                glTexImage1D (proxyDim, queryLod, pTx->InternalFormat,
                            mipsize[txWidth],pTx->HasBorder,
                            pTx->LoadFormat, pTx->LoadType, NULL);
                break;

            case GL_TEXTURE_1D_ARRAY_EXT:
            case GL_TEXTURE_2D:
                glTexImage2D (proxyDim, queryLod, pTx->InternalFormat,
                            mipsize[txWidth], mipsize[txHeight],pTx->HasBorder,
                            pTx->LoadFormat, pTx->LoadType, NULL);
                break;

            case GL_TEXTURE_LWBE_MAP_EXT:
                glTexImage2D (proxyDim, queryLod, pTx->InternalFormat,
                            mipsize[txWidth], mipsize[txHeight],pTx->HasBorder,
                            pTx->LoadFormat, pTx->LoadType, NULL);
                break;

            case GL_TEXTURE_2D_ARRAY_EXT:
            case GL_TEXTURE_LWBE_MAP_ARRAY_ARB:
            case GL_TEXTURE_3D:
                glTexImage3D (proxyDim, queryLod, pTx->InternalFormat,
                    mipsize[txWidth], mipsize[txHeight], mipsize[txDepth],
                    pTx->HasBorder,pTx->LoadFormat, pTx->LoadType,NULL);
                break;

            default:
                MASSERT(0); // unsupported texture type
                break;
        }

        GLint texWidth = 0;
        glGetTexLevelParameteriv(proxyDim,queryLod,GL_TEXTURE_WIDTH,&texWidth);

        CHECK_RC(mglGetRC());

        if ( texWidth )
        {
            // Success, the texture will fit.
            break;
        }
        else
        {
            // The texture fits within our script-defined limit, but the
            // OpenGL driver says it can't fit it into memory.
            //
            // This sort of thing can cause different golden values between
            // boards that have differing amounts of RAM.
            Printf(Tee::PriWarn, "!GL says texture too big, shrinking it.\n");
            PrintTextureObject(Tee::PriNormal, pTx);

            CHECK_RC(TexShrinkBaseAndLayers(pTx,requiredMMLevel));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void RndTexturing::PickTextureObject
(
    TextureObjectState * pTx,
    const GLRandom::TxfReq &RqdTxAttrs
)
{
    if (RqdTxAttrs.Attrs)
        pTx->Dim = MapTxAttrToTxDim(RqdTxAttrs.Attrs);
    else
        pTx->Dim = m_Pickers[RND_TX_DIM].Pick();

    pTx->BaseSize[txWidth]  = m_Pickers[RND_TX_WIDTH].Pick();
    pTx->BaseSize[txHeight] = m_Pickers[RND_TX_HEIGHT].Pick();
    pTx->BaseSize[txDepth]  = m_Pickers[RND_TX_DEPTH].Pick();
    // lowest mmap level to actually use
    pTx->BaseMM         = m_Pickers[RND_TX_BASE_MM].Pick();
    // highest (smallest) mmap level to actually use
    pTx->MaxMM          = m_Pickers[RND_TX_MAX_MM].Pick();
    // which pattern to fill with (see below)
    pTx->PatternCode    = m_Pickers[RND_TX_NAME].Pick();
    pTx->IsNP2          = m_Pickers[RND_TX_IS_NP2].Pick();
    pTx->DepthTexMode   = m_Pickers[RND_TX_DEPTH_TEX_MODE].Pick();

    // Should we use texel borders in the texture image data?
    pTx->HasBorder      = m_Pickers[RND_TX_HAS_BORDER].Pick();
    // Force valid pattern, in case of bad RND_TX_NAME picker at runtime.
    pTx->PatternCode %= RND_TX_NAME_NUM;

    pTx->TextureType = IsDefault;  // Do not use views by default

    // Instructions that use LOADIM,STOREIM,ATOMIM will access a texture image
    // from a specific MM level. So make sure this texture supports that level.
    if (RqdTxAttrs.Attrs)
    {
        if (RqdTxAttrs.IU.unit >= 0)
        {
            GLuint minWidth = 1 << RqdTxAttrs.IU.mmLevel;
            if (pTx->IsNP2)
                minWidth = minWidth << 1;

            pTx->BaseSize[txWidth] = max(pTx->BaseSize[txWidth], minWidth);
        }
    }
    //
    // Force a valid texture dim for this hardware.
    //
    switch (pTx->Dim)
    {
        default:
            pTx->Dim = GL_TEXTURE_2D;
            break;

        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_LWBE_MAP_EXT:
            break;

        case GL_TEXTURE_1D_ARRAY_EXT:
        case GL_TEXTURE_2D_ARRAY_EXT:
            if (!m_pGLRandom->HasExt(GLRandomTest::ExtEXT_texture_array))
                pTx->Dim = GL_TEXTURE_2D;
            break;

        case GL_TEXTURE_LWBE_MAP_ARRAY_ARB:
            if (!m_pGLRandom->HasExt(GLRandomTest::ExtARB_texture_lwbe_map_array))
                pTx->Dim = GL_TEXTURE_2D;
            break;

        case GL_TEXTURE_RENDERBUFFER_LW:
            if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_explicit_multisample))
                pTx->Dim = GL_TEXTURE_2D;
            break;
    }

    if (!m_pGLRandom->HasExt(GLRandomTest::ExtARB_texture_non_power_of_two))
        pTx->IsNP2 = GL_FALSE;

    if (pTx->IsNP2)
    {
        // Give the texture random NP2 dimensions
        for (GLuint d = 0; d < txMaxDims; d++)
        {
            pTx->BaseSize[d] -= m_pFpCtx->Rand.GetRandom(0, pTx->BaseSize[d]/2);
        }
    }
    else
    {
        // Force power-of-2 size (in case pickers are badly configured by user).
        for (GLuint d = 0; d < txMaxDims; d++)
        {
            if (pTx->BaseSize[d] & (pTx->BaseSize[d]-1))
                pTx->BaseSize[d] = 64;
        }
    }

    // Update derived properties based on Dim (1d, 2d, lwbe, etc).
    TexSetDim (pTx);

    if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
        pTx->PatternCode = m_Pickers[RND_TX_MS_NAME].Pick();

    //
    // Pick a valid internal format for the texture:
    //
    // Only try once before defaulting if there is a required format
    const int MaxTries = (RqdTxAttrs.Format == 0) ? 5 : 1;
    for (int attempt = 0; attempt <= MaxTries; attempt++)
    {
        if (attempt < MaxTries)
        {
            if (RqdTxAttrs.Format)
            {
                pTx->InternalFormat = RqdTxAttrs.Format;
            }
            else if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
            {
                // Force a valid format renderbuffer multisample texures.
                pTx->InternalFormat = m_Pickers[RND_TX_MS_INTERNAL_FORMAT].Pick();
            }
            else
            {
                // Random format.
                pTx->InternalFormat = m_Pickers[RND_TX_INTERNAL_FORMAT].Pick();
                if (pTx->InternalFormat == GL_STENCIL_INDEX8)
                {
                   // spec does not support stencil format with 3d textures
                   // TODO: bindless textures failing with stencil format
                   if (pTx->Dim == GL_TEXTURE_3D || m_UseBindlessTextures)
                       continue;
                }
            }
        }
        else
        {
            if (RqdTxAttrs.Format)
            {
                Printf(Tee::PriLow,
                       "%s : Required format not supported : 0x%04x!\n",
                       __FUNCTION__, RqdTxAttrs.Format);
            }
            // Force an always-valid format -- should only get here if the
            // user set up a bad picker from JavaScript.
            pTx->InternalFormat = GL_RGB5;
        }

        const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo (pTx->InternalFormat);
        if (0 == pFmtInfo || (0 == pFmtInfo->InternalFormat))
        {
            // Not a color format known to mods.
            continue;
        }
        if (! mglExtensionSupported(pFmtInfo->TexExtension))
        {
            // Not supported on this HW.
            continue;
        }

        const char * pIFStr = mglEnumToString(pTx->InternalFormat,
                                              "BadInternalFormat");
        if (mglIsFormatAstc(pTx->InternalFormat))
        {
            switch (pTx->Dim)
            {
                // ASTC lwrrently only supports 2D and lwbe map textures
                case GL_TEXTURE_2D:
                case GL_TEXTURE_LWBE_MAP_EXT:
                    break;

                default:
                    continue;
            }

            // Force texture properties according to loaded ASTC image data
            // rather than using a random picker
            AstcCache::GetInstance().GetImageSizes(pTx->InternalFormat,
                                                   &pTx->BaseSize[txWidth],
                                                   &pTx->BaseSize[txHeight],
                                                   &pTx->BaseSize[txDepth]);
            pTx->HasBorder = false;
            TexSetDim(pTx);
        }
        else if (strstr(pIFStr, "COMPRESSED"))
        {
            switch (pTx->Dim)
            {
                // 4x4 block compression formats must be 2d or 3d.
                case GL_TEXTURE_2D:
                case GL_TEXTURE_3D:
                case GL_TEXTURE_2D_ARRAY_EXT:
                    break;

                default:
                    continue;
            }
        }

        pTx->HwFmt = pFmtInfo->ColorFormat;

        if (RqdTxAttrs.Attrs)
        {
            // Force matching shadow or not for the "forced" textures.
            bool wantDepth = 0 != (RqdTxAttrs.Attrs & IsShadowMask);
            bool isDepth   = ColorUtils::IsDepth(pTx->HwFmt);

            if (wantDepth && !isDepth)
            {
                pTx->InternalFormat = GL_DEPTH_COMPONENT16;
            }
            if (isDepth && !wantDepth)
            {
                pTx->InternalFormat = GL_RGB5;
            }
            if (RqdTxAttrs.IU.unit >= 0)
            {
                // Pick the proper internal format based on the numElems
                switch (RqdTxAttrs.IU.elems){
                    default: MASSERT(!"RqdTxAttrs with invalid ImageUnit.elems");
                    case 1: pTx->InternalFormat = GL_R32UI; break;
                    case 2: pTx->InternalFormat = GL_RG32UI; break;
                    case 4: pTx->InternalFormat = GL_RGBA32F; break;
                }
            }

            // Reserve this texture for this type of access.
            pTx->RsvdAccess = RqdTxAttrs.IU.access;
            pFmtInfo = mglGetTexFmtInfo (pTx->InternalFormat);
            pTx->HwFmt = pFmtInfo->ColorFormat;
        }

        if (ColorUtils::IsDepth(pTx->HwFmt))
        {
            GLRandomTest::ExtensionId ExtensionNeeded;

            switch (pTx->Dim)
            {
                case GL_TEXTURE_1D:
                case GL_TEXTURE_1D_ARRAY_EXT:
                case GL_TEXTURE_2D:
                case GL_TEXTURE_2D_ARRAY_EXT:
                    ExtensionNeeded = GLRandomTest::ExtARB_depth_texture;
                    break;

                case GL_TEXTURE_LWBE_MAP_EXT:
                    ExtensionNeeded = GLRandomTest::ExtLW_gpu_program4;
                    break;

                case GL_TEXTURE_LWBE_MAP_ARRAY_ARB:
                    ExtensionNeeded = GLRandomTest::ExtLW_gpu_program4_1;
                    break;

                case GL_TEXTURE_RENDERBUFFER_LW:
                case GL_TEXTURE_3D:
                default:
                    // Not supported by any extension so far.
                    ExtensionNeeded = GLRandomTest::ExtNO_SUCH_EXTENSION;
                    break;
            }
            if (! m_pGLRandom->HasExt(ExtensionNeeded))
                continue;

            // we need to colwert the attrib to the appropriate shadow version.
            switch (pTx->Attribs & SASMDimFlags)
            {
                default:
                    MASSERT(!"Invalid base tex attrib for GL_DEPTH_COMPONENT");
                    pTx->Attribs |= IsShadow2d;
                    break;
                case Is1d:        pTx->Attribs |= IsShadow1d;      break;
                case Is2d:        pTx->Attribs |= IsShadow2d;      break;
                case IsLwbe:      pTx->Attribs |= IsShadowLwbe;    break;
                case IsArray1d:   pTx->Attribs |= IsShadowArray1d; break;
                case IsArray2d:   pTx->Attribs |= IsShadowArray2d; break;
                case IsArrayLwbe: pTx->Attribs |= IsShadowArrayLwbe; break;
            }
            // clear out old DIM flags
            pTx->Attribs &= ~(SDimFlags|IsArray1d | IsArray2d | IsArrayLwbe);
        }
        // If we get here, format is valid.
        break;
    }

    GLuint attr;

    GetFormatProperties (pTx->InternalFormat, &pTx->LoadFormat, &pTx->LoadType,
            &pTx->LoadBpt, &attr);
    pTx->Attribs |= attr;

    if (pTx->Attribs & (Is2dMultisample | ADimFlags))
    {
        // Don't use MS,array or shadow-array textures with legacy
        // texture enables.
        pTx->Attribs &= ~IsLegacy;
    }

    if ((RqdTxAttrs.Attrs & IsNoBorder) || !pTx->HasBorder)
    {
        pTx->Attribs |= IsNoBorder;
    }

    if (pTx->Attribs & IsNoBorder)
    {
        pTx->HasBorder = false;
    }
    // Report coverage only on the textures we know are going to be used by the shaders.
    if (RqdTxAttrs.Attrs)
    {
        ReportCoverage(pTx->InternalFormat);
    }
}

//------------------------------------------------------------------------------
// Report the texture format to the test coverage object.
RC RndTexturing::ReportCoverage(GLenum fmt)
{
    if (m_pGLRandom->GetCoverageLevel() != 0)
    {
        CoverageDatabase * pDB = CoverageDatabase::Instance();
        if (pDB)
        {
            RC rc;
            CoverageObject *pCov = nullptr;
            pDB->GetCoverageObject(Hardware,hwTexture, &pCov);
            if (!pCov)
            {
                pCov = new HwTextureCoverageObject;
                pDB->RegisterObject(Hardware,hwTexture, pCov);
            }
            pCov->AddCoverage(fmt, m_pGLRandom->GetName());
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC RndTexturing::CreateSparseTextures()
{
    RC rc;
    if (!m_Pickers[RND_TX_SPARSE_ENABLE].Pick() ||
        !m_pGLRandom->HasExt(GLRandomTest::ExtARB_sparse_texture) ||
        !m_pGLRandom->HasExt(GLRandomTest::ExtARB_texture_view))
    {
        return OK;
    }

    for (UINT32 i = 0; i < RND_TX_NUM_SPARSE_TEX; i++)
    {
        if(m_SparseTextures[i].Id)      // Already Created.
            continue;

        // Create and set Sparse texture parameters
        glGenTextures(1, &m_SparseTextures[i].Id);

        // Setting dim = 2d array is not so random. But this target is compatible
        // with many other targets( 2d, 2darray, lwbe, lwbe array) for max coverage
        m_SparseTextures[i].Dim          = GL_TEXTURE_2D_ARRAY_EXT;

        m_SparseTextures[i].NumDims      = 2;
        m_SparseTextures[i].Faces        = 1;
        m_SparseTextures[i].BaseMM       = 0;
        m_SparseTextures[i].DepthTexMode = m_Pickers[RND_TX_DEPTH_TEX_MODE].Pick();
        m_SparseTextures[i].IsNP2        = GL_FALSE;
        m_SparseTextures[i].RsvdAccess   = iuaRead;
        m_SparseTextures[i].Access       = iuaRead;

        glBindTexture(m_SparseTextures[i].Dim, m_SparseTextures[i].Id);

        //Pick an internal Format for sparse texture
        GLint numVirtualPageSize = 0;
        UINT32 tries = 5;
        while (tries > 0)
        {
            GLenum internalFormatClass = m_Pickers[RND_TX_SPARSE_TEX_INT_FMT_CLASS].Pick();
            switch (internalFormatClass)
            {
                case GL_VIEW_CLASS_24_BITS:
                    m_SparseTextures[i].InternalFormat = GL_RGB8;
                    break;
                case GL_VIEW_CLASS_32_BITS:
                    m_SparseTextures[i].InternalFormat = GL_RGBA8;
                    break;
                case GL_VIEW_CLASS_48_BITS:
                    m_SparseTextures[i].InternalFormat = GL_RGB16;
                    break;
                case GL_VIEW_CLASS_64_BITS:
                    m_SparseTextures[i].InternalFormat = GL_RGBA16;
                    break;
                case GL_VIEW_CLASS_96_BITS:
                    m_SparseTextures[i].InternalFormat = GL_RGB32F;
                    break;
                case GL_VIEW_CLASS_128_BITS:
                    m_SparseTextures[i].InternalFormat = GL_RGBA32F;
                    break;
                default:
                    m_SparseTextures[i].InternalFormat = GL_RGBA8;
                    break;
            }
            GetFormatProperties (m_SparseTextures[i].InternalFormat,
                    &m_SparseTextures[i].LoadFormat, &m_SparseTextures[i].LoadType,
                    &m_SparseTextures[i].LoadBpt, &m_SparseTextures[i].Attribs);
            glGetInternalformativ(m_SparseTextures[i].Dim,
                                  m_SparseTextures[i].InternalFormat,
                                  GL_NUM_VIRTUAL_PAGE_SIZES_ARB,
                                  1, &numVirtualPageSize);
            if (numVirtualPageSize > 0)
            {
                // Exit loop as this internal format is supported as sparse tex
                break;
            }

            tries--;
        }

        const mglTexFmtInfo * pFmtInfo =
                            mglGetTexFmtInfo (m_SparseTextures[i].InternalFormat);
        if ((numVirtualPageSize == 0) || (tries == 0) ||
            (pFmtInfo == 0) || (pFmtInfo->InternalFormat == 0))
        {
            //Did not get a valid internal format/ HW format
            continue;
        }
        m_SparseTextures[i].HwFmt = pFmtInfo->ColorFormat;

        vector<GLint> pageSize (numVirtualPageSize, 0);
        // Select a random page size
        INT32 pageIndex =  m_pFpCtx->Rand.GetRandom(0, numVirtualPageSize - 1);
        glTexParameteri(m_SparseTextures[i].Dim, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, pageIndex);

        //Get page width/height for this page size
        int bufSize = static_cast<int>(pageSize.size());
        glGetInternalformativ(m_SparseTextures[i].Dim,
                              m_SparseTextures[i].InternalFormat,
                              GL_VIRTUAL_PAGE_SIZE_X_ARB,
                              bufSize, pageSize.data());
        UINT32 pageWidth = pageSize[pageIndex];
        glGetInternalformativ(m_SparseTextures[i].Dim,
                              m_SparseTextures[i].InternalFormat,
                              GL_VIRTUAL_PAGE_SIZE_Y_ARB,
                              bufSize, pageSize.data());
        UINT32 pageHeight = pageSize[pageIndex];

        // Set texture dimensions
        GLint maxWorH = INT_MAX;
        glGetIntegerv(GL_MAX_SPARSE_TEXTURE_SIZE_ARB, &maxWorH);
        UINT32 numMMLevels = m_Pickers[RND_TX_SPARSE_TEX_NUM_MMLEVELS].Pick();

        m_SparseTextures[i].BaseSize[txWidth] =
            min<GLuint>(1 << numMMLevels, maxWorH);
        m_SparseTextures[i].BaseSize[txWidth] =
            ALIGN_DOWN(m_SparseTextures[i].BaseSize[txWidth], pageWidth);

        m_SparseTextures[i].BaseSize[txHeight] =
            min<GLuint>(1 << numMMLevels, maxWorH);
        m_SparseTextures[i].BaseSize[txHeight] =
            ALIGN_DOWN(m_SparseTextures[i].BaseSize[txHeight], pageHeight);

        GLint maxSparseLayers = INT_MAX;
        glGetIntegerv(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB, &maxSparseLayers);
        m_SparseTextures[i].SparseLayers = min(
                m_Pickers[RND_TX_SPARSE_TEX_NUM_LAYERS].Pick(),
                static_cast<UINT32>(maxSparseLayers));
        m_SparseTextures[i].Layers = m_SparseTextures[i].SparseLayers;
        m_SparseTextures[i].BaseSize[txDepth] = m_SparseTextures[i].Layers;
        m_SparseTextures[i].PatternCode = m_Pickers[RND_TX_NAME].Pick();
        m_SparseTextures[i].HasBorder = false;

        // Set MM Levels
        TexFixSize(&m_SparseTextures[i]);

        // Check format is immutable
        GLint result = GL_FALSE;
        glGetTexParameteriv(m_SparseTextures[i].Dim,
                            GL_TEXTURE_IMMUTABLE_FORMAT, &result);
        MASSERT(result == GL_FALSE);

        // Allocate sparse storage(virtual data store only, no physical data store yet.)
        glTexParameteri(m_SparseTextures[i].Dim,
                        GL_TEXTURE_SPARSE_ARB, GL_TRUE);

        glTexStorage3D(m_SparseTextures[i].Dim,
                    m_SparseTextures[i].LoadMaxMM + 1,
                    m_SparseTextures[i].InternalFormat,
                    m_SparseTextures[i].BaseSize[txWidth],
                    m_SparseTextures[i].BaseSize[txHeight],
                    m_SparseTextures[i].BaseSize[txDepth]);

        // Error checking
        result = GL_FALSE;
        glGetTexParameteriv(m_SparseTextures[i].Dim,
                            GL_TEXTURE_IMMUTABLE_FORMAT, &result);
        if(result == GL_FALSE)
            continue;

        result = GL_FALSE;
        glGetTexParameteriv(m_SparseTextures[i].Dim, GL_TEXTURE_SPARSE_ARB, &result);
        rc = mglGetRC();

        if(result == GL_FALSE || rc != OK)
        {
            rc.Clear();
            continue;
        }

        glBindTexture(m_SparseTextures[i].Dim, 0);
        m_SparseTextures[i].TextureType = IsSparse;
        Printf(Tee::PriDebug, "Created Sparse Texture: ");
        PrintTextureObject(Tee::PriDebug, &m_SparseTextures[i]);
    }

    glBindTexture(m_SparseTextures[RND_TX_NUM_SPARSE_TEX - 1].Dim, 0);
    return mglGetRC();
}

//------------------------------------------------------------------------------
// View Textures are created by populating some layers/levels of the sparse texture
RC RndTexturing::InitViewTextures()
{
    RC rc;

    // Using texture views in this frame?
    if (!m_Pickers[RND_TX_VIEW_ENABLE].Pick())
        return OK;

    for (UINT32 i = 0; i < RND_TX_NUM_SPARSE_TEX; i++)
    {
        if (m_SparseTextures[i].TextureType == IsSparse)
        {
            TextureObjectState tempSparseTexCopy = m_SparseTextures[i];
            // View textures use a subset of layers/levels of sparse texture
            m_SparseTextures[i].LoadMinMM = min<GLuint>(
                    m_Pickers[RND_TX_VIEW_BASE_MMLEVEL].Pick(),
                    m_SparseTextures[i].LoadMaxMM);

            m_SparseTextures[i].Layers = min<GLuint>(
                    m_SparseTextures[i].SparseLayers,
                    m_Pickers[RND_TX_VIEW_NUM_LAYERS].Pick());
            m_SparseTextures[i].BaseSize[txDepth] = m_SparseTextures[i].Layers;

            m_SparseTextures[i].TextureType = IsView;
            rc = LoadTexture(&m_SparseTextures[i]);
            if (rc != OK)
            {
                rc.Clear();
                m_SparseTextures[i] = tempSparseTexCopy;
            }
            else
            {
                Printf(Tee::PriDebug, "Initialized sparse texture %d as View: ", i);
                PrintTextureObject(Tee::PriDebug, &m_SparseTextures[i]);
            }

        }
    }
    return mglGetRC();
}
string TextureItemToString(uint32 item)
{
    return Utility::StrPrintf("Texture_%d", item);
}
//------------------------------------------------------------------------------
// Create some random GL texture objects, load them  (including loading
//   mipmap reduced versions).
RC RndTexturing::GenerateTextures()
{
    StickyRC rc;
    GLRandom::TxfSet   RefTxAttribs;

    // Use a multiset to deal with bindless textures which need 'numTxHandles'
    // copies of the same type texture image.
    multiset<GLRandom::TxfReq> TxAttrs;

    const int numTxHandles = m_UseBindlessTextures ? NumTxHandles : 1;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glActiveTexture(GL_TEXTURE0_ARB);
    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_3D);
    glDisable(GL_TEXTURE_LWBE_MAP_EXT);

    // Get a list of all the referenced texture targets from all the lwrrently
    // loaded shader programs. We'll want to make sure we genearte at least one
    // texture for each target in the list.
    m_pGLRandom->m_GpuPrograms.GetAllRefTextureAttribs(&RefTxAttribs);
    for( int i = 0; i < numTxHandles; i++)
    {
        TxfSetItr it;
        for( it = RefTxAttribs.begin(); it != RefTxAttribs.end(); it++)
            TxAttrs.insert(*it);
    }

    // Append all textures referenced by the pixel drawing (3d pixels use
    // textures)
    m_pGLRandom->m_Pixels.GetAllRefTextureAttribs(&RefTxAttribs);
    for( int i = 0; i < numTxHandles; i++)
    {
        TxfSetItr it;
        for( it = RefTxAttribs.begin(); it != RefTxAttribs.end(); it++)
            TxAttrs.insert(*it);
    }

    //
    // Pick a few limits for num tx objects and tx memory used.
    // Load new textures, up to the limits.
    m_MaxTextures  = max(m_Pickers[RND_TX_NUM_OBJS].Pick(),
                         (UINT32)TxAttrs.size());

    // Use the Log feature to verify that we generate textures repeatably.
    m_pGLRandom->LogBegin(m_MaxTextures, TextureItemToString);

    m_MaxTxBytes   = m_Pickers[RND_TX_MAX_TOTAL_BYTES].Pick();

     // Init view textures that other textures can use as 'view'
    CHECK_RC(InitViewTextures());

    m_Textures = new TextureObjectState [m_MaxTextures];
    if ( 0 == m_Textures )
        return RC::CANNOT_ALLOCATE_MEMORY;

    //
    // Pick textures until we run out of objects or tx memory.
    //
    UINT64 txMemoryAvailable = m_MaxTxBytes;
    const UINT64 txMemoryTooLittle = 4096;
    m_NumTextures = 0;
    int retry = 0;

    while ((retry < 10) &&
           (m_NumTextures < m_MaxTextures) &&
           ((txMemoryAvailable > txMemoryTooLittle) ||
            (TxAttrs.size() > 0)))
    {
        TextureObjectState * pTx = m_Textures + m_NumTextures;
        memset(pTx, 0, sizeof(TextureObjectState));
        pTx->Index = m_NumTextures;
        m_NumTextures++;

        GLRandom::TxfReq rqdTxAttrs;

        if (TxAttrs.size())
        {
            // We haven't generated textures to match all the requirements of
            // the already-generated shader programs.
            //
            // Force this texture to fill one outstanding requirement, and
            // make sure we don't give up for too-little memory.

            rqdTxAttrs = *(TxAttrs.begin());
            if (txMemoryAvailable <= txMemoryTooLittle)
                txMemoryAvailable = 2*txMemoryTooLittle;
        }

        // Pick texture dim, format, size, etc.
        PickTextureObject (pTx, rqdTxAttrs);

        // Reduce size to fit, fails if too large even at 1x1.
        bool usingNewTxMemory = true;
        rc = ShrinkTexToFit(pTx, txMemoryAvailable, rqdTxAttrs.IU.mmLevel, &usingNewTxMemory);
        if (rc != OK)
        {
            if (TxAttrs.size())
            {
                // We can't allow required textures to fail. So increase memory
                // and retry.
                // PrintTextureObject(Tee::PriNormal,
                //      pTx, rqdTxAttrs, txMemoryAvailable, "Fail ");
                rc.Clear();
                retry++;
                m_NumTextures--;
                txMemoryAvailable *= 2;
                continue;
            }
            else
            {
                Printf(Tee::PriError, "GenerateTextures() failed\n");
                PrintTextureObject(Tee::PriNormal, pTx);
                return rc;
            }
        }

        // Update for the final chosen size.
        if (usingNewTxMemory)
        {
            txMemoryAvailable -= TexBytesTotal(pTx);
        }

        PrintTextureObject(Tee::PriDebug, pTx);

        m_pGLRandom->LogData(pTx->Index, pTx, sizeof(TextureObjectState));

        if(TxAttrs.size())
        {
            TxAttrs.erase(TxAttrs.begin());
        }
        retry = 0;
    }

    // Load textures.
    UINT32 numTexturesToLoad = 0;
    while (numTexturesToLoad < m_NumTextures)
    {
        TextureObjectState * pTx = m_Textures + numTexturesToLoad;
        if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
            rc = GenerateMsTexture(pTx);
        else
        {
            glGenTextures(1, &pTx->Id);
            if (mglIsFormatAstc(pTx->InternalFormat))
            {
                rc = LoadAstcTexture(pTx);
            }
            else
            {
                //Try texture views first, if successful its TextureType is set to IsUsingView
                UseTextureViews(pTx);

                if (pTx->TextureType != IsUsingView)
                {
                    rc = LoadTexture(pTx);
                }
            }
        }

        if (OK != rc)
        {
            Printf(Tee::PriError, "GenerateTextures() failed\n");
            PrintTextureObject(Tee::PriNormal, pTx);
            return rc;
        }
        numTexturesToLoad++;
    }

    rc = m_pGLRandom->LogFinish("Texture");
    // This should never happen.
    if( TxAttrs.size() != 0)
    {
        Printf(Tee::PriError,
            "Unable to generate a texture to meet the shader's requirements\n");
        Printf(Tee::PriError, "txMemoryAvailable= %lld\n", txMemoryAvailable);
        Printf(Tee::PriError, "rqdTxAttrs.IU.mmLevel=%d\n",
               (TxAttrs.begin())->IU.mmLevel);
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//------------------------------------------------------------------------------
// Delete the GL texture objects.
void RndTexturing::FreeTextures()
{
    if (! m_Textures)
        return;

    for (UINT32 tx = 0; tx < m_NumTextures; tx++)
    {
        if (m_Textures[tx].Id != 0)
            glDeleteTextures(1, &m_Textures[tx].Id);
        if (m_Textures[tx].MsColorRboId != 0)
            glDeleteRenderbuffersEXT(1, &m_Textures[tx].MsColorRboId);
        if (m_Textures[tx].MsFboId != 0)
            glDeleteFramebuffersEXT(1, &m_Textures[tx].MsFboId);
    }

    // de-commit memory of sparse texture
    for (UINT32 i = 0; i < RND_TX_NUM_SPARSE_TEX; i++)
    {
        if (m_SparseTextures[i].TextureType == IsView)
        {
            glBindTexture(m_SparseTextures[i].Dim, m_SparseTextures[i].Id);
            for (GLuint mmLevel = m_SparseTextures[i].LoadMinMM;
                        mmLevel <= m_SparseTextures[i].LoadMaxMM; mmLevel++)
            {
                GLuint mipsize[txMaxDims];
                CalcMipSize(&m_SparseTextures[i], mmLevel, &mipsize[0]);
                glTexturePageCommitmentEXT(m_SparseTextures[i].Id,
                                        mmLevel,
                                        0, 0, 0,
                                        mipsize[txWidth],
                                        mipsize[txHeight],
                                        mipsize[txDepth],
                                        false);
            }
            m_SparseTextures[i].LoadMinMM = m_SparseTextures[i].BaseMM;
            m_SparseTextures[i].Layers = m_SparseTextures[i].SparseLayers;
            m_SparseTextures[i].BaseSize[txDepth] = m_SparseTextures[i].Layers;
            m_SparseTextures[i].TextureType = IsSparse;
            glBindTexture(m_SparseTextures[i].Dim, 0);
        }
    }

    delete [] m_Textures;
    m_Textures = 0;
    m_NumTextures = 0;
}

//------------------------------------------------------------------------------
void RndTexturing::GetFormatProperties
(
    GLenum InternalFormat
    ,GLenum * pLoadFormat
    ,GLenum * pLoadType
    ,GLenum * pLoadBpt
    ,GLuint * pAttribs
) const
{
    const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(InternalFormat);
    *pLoadFormat = pFmtInfo->ExtFormat;
    *pLoadType   = pFmtInfo->Type;
    *pLoadBpt    = ColorUtils::PixelBytes(pFmtInfo->ExtColorFormat);
    *pAttribs    = 0;

    if (ColorUtils::IsRgb (pFmtInfo->ColorFormat))
    {
        *pAttribs |= IsLegacy;
    }
    if (ColorUtils::IsSigned (pFmtInfo->ColorFormat))
    {
        *pAttribs |= IsSigned;
    }
    else
    {
        *pAttribs |= IsUnSigned;
    }

    if (ColorUtils::IsDepth (pFmtInfo->ColorFormat))
    {
        *pAttribs |= IsDepth;
        *pAttribs |= IsLegacy;
    }

    const char * pIFStr = mglEnumToString(pFmtInfo->InternalFormat,
                                          "BadInternalFormat");
    if (strstr(pIFStr, "COMPRESSED"))
    {
        *pAttribs |= IsNoBorder;
    }

    if (ColorUtils::IsFloat (pFmtInfo->ColorFormat))
    {
        *pAttribs |= IsFloat;
        if (pFmtInfo->Type == GL_FLOAT)
        {
            if (m_MaxFilterableFloatDepth < 32)
                *pAttribs |= IsNoFilter;

            if ( ! m_pGLRandom->GetBoundGpuSubdevice()->HasFeature(
                        Device::GPUSUB_SUPPORTS_CLAMP_GT_32BPT_TEXTURES))
            {
                *pAttribs |= IsNoBorder;
            }
        }
        else if (m_MaxFilterableFloatDepth < 16)
        {
            *pAttribs |= IsNoFilter;
        }
    }
    else if ( ! ColorUtils::IsNormalized (pFmtInfo->ColorFormat))
    {
        // Here's a comment from the GL driver (gf100sharedutil.c):

        //   EXT_texture_integer
        //   We use FLOAT types for the 32bit/component formats instead of INT
        //   because FLOAT supports a border color and we don't rely on the
        //   hardware to do sign extension. 8- and 16-bit/component INT formats
        //   have to fallback to software if the border color is used and is
        //   non-zero. Filtering isn't allowed for these types, so nothing
        //   depends on the actual type (aside from the sign-extension for
        //   smaller types).

        *pAttribs |= IsNoFilter;

        if (pFmtInfo->Type == GL_BYTE ||
            pFmtInfo->Type == GL_UNSIGNED_BYTE ||
            pFmtInfo->Type == GL_SHORT ||
            pFmtInfo->Type == GL_UNSIGNED_SHORT)
        {
            *pAttribs |= IsNoBorder;
        }

        // Legacy fixed-function texturing emulation doesn't support unnorm int.
        *pAttribs &= ~IsLegacy;
    }
}

//------------------------------------------------------------------------------
//  Do any of the loaded texture objects meet these requirements?
//
// Returns true if any of the loaded texture objects have all the
// texture-attribute bits in the mask rqdTxAttrs.
// Used by the vx/fr program picker to make sure we will be able to
// bind a texture later.
bool RndTexturing::AnyMatchingTxObjLoaded(const GLRandom::TxfReq & rqdTxAttrs)
{
    if ((0 == rqdTxAttrs.Attrs) && (rqdTxAttrs.Format == 0))
        return true;

    for (UINT32 i = 0; i < m_NumTextures; i++)
    {
        if ((rqdTxAttrs.Attrs == (rqdTxAttrs.Attrs & m_Textures[i].Attribs)) &&
            ((rqdTxAttrs.Format == 0) ||
             (rqdTxAttrs.Format == m_Textures[i].InternalFormat)))
        {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
//  How many of s,t,r,q are needed by this texture unit.
//
// Called by RndPrograms when vertex and fragment programs are disabled.
//
int RndTexturing::TxuComponentsNeeded(int txu)
{
    if (txu >= m_pGLRandom->NumTxUnits())
        return 0;

    // Old-fashioned texturing.

    if (! m_Txu.Fetchers[txu].Enable)
        return 0;

    GLenum dim = m_Textures[m_Txu.Fetchers[txu].BoundTexture].Dim;

    if (dim == GL_TEXTURE_LWBE_MAP_EXT)
    {
        // RndVertexes::GenerateTextureCoords knows to generate normals
        // if we return 4.
        return 4;
    }
    if ((dim == GL_TEXTURE_3D) || (m_Txu.Fetchers[txu].CompareMode))
    {
        // Generate the R coord proportional to relative depth in the bbox.
        // Note that ARB_shadow requires an R coord even for 2d textures.
        return 3;
    }
    if (dim == GL_TEXTURE_1D)
    {
        return 1;
    }
    // Else, 2D or rect.
    return 2;
}

//------------------------------------------------------------------------------
void RndTexturing::ScaleSTRQ
(
    int txfIdx
    ,GLfloat * pScaleS
    ,GLfloat * pScaleT
    ,GLfloat * pScaleR
    ,GLfloat * pScaleQ
)
{
    MASSERT(txfIdx >= 0);
    GLfloat scales[4] = { 1.0F,1.0F,1.0F,1.0F };
    enum { s,t,r,q };

    if ((txfIdx < m_pGLRandom->NumTxFetchers()) &&
        (m_Txu.Fetchers[txfIdx].BoundTexture >= 0))
    {
        const TextureObjectState * pTx =
                &m_Textures[m_Txu.Fetchers[txfIdx].BoundTexture];

        if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
        {
            // We are using a rectangle texture or a renderbuffer texture.
            // Instead of S,T from 0.0 to 1.0, the coords should be scaled by
            // the actual texel size of the bound texture and the Q need to be
            // scaled by the number of samples.
            scales[s] = (GLfloat)(pTx->BaseSize[txWidth]-1);
            scales[t] = (GLfloat)(pTx->BaseSize[txHeight]-1);
            scales[q] = (GLfloat)(pTx->MsSamples-1);
        }
        else if (pTx->Layers)
        {
            if (pTx->Faces == 1)
            {
                // For 1d and 2d array textures, the "extra" dim is array-index
                // (i.e. layer index) which is not normalized.
                scales[pTx->NumDims] = (GLfloat) pTx->Layers;
            }
            else
            {
                // For lwbe-array textures, q indexes a layer.
                // Our vertex-gen code tends to put boring data (i.e. 1.0) into
                // texture q coords, so we'll introduce some randomness here.
                scales[q] = (GLfloat)m_pFpCtx->Rand.GetRandom(0, pTx->Layers-1);
            }
        }
    }
    *pScaleS = scales[s];
    *pScaleT = scales[t];
    *pScaleR = scales[r];
    *pScaleQ = scales[q];
}

void RndTexturing::PrintTextureObject
(
    Tee::Priority pri,
    const RndTexturing::TextureObjectState * pTx,
    GLRandom::TxfReq rqdTxAttrs,
    UINT64 txMemoryAvailable,
    const char * msg
)
{
    Printf(pri, "%s txMemAvail:%lld Bytes:%lld Rqd(dim:%s: mm:%d elems:%d) ",
        msg,
        txMemoryAvailable,
        TexBytesTotal(pTx),
        rqdTxAttrs.Attrs==0 ? "none" : StrTexDim(MapTxAttrToTxDim(rqdTxAttrs.Attrs)),
        rqdTxAttrs.IU.mmLevel,
        rqdTxAttrs.IU.elems);
    PrintTextureObject(pri,pTx);
}
//------------------------------------------------------------------------------
void RndTexturing::PrintTextureObject
(
    Tee::Priority pri,
    const RndTexturing::TextureObjectState * pTx
) const
{
    MASSERT(pTx);

    Printf(pri, "Tx%d ID:%d  Hndl:0x%llx %s %d",
            pTx->Index,
            pTx->Id,
            pTx->Handle,
            StrTexDim(pTx->Dim),
            pTx->BaseSize[txWidth]);

    for (GLuint d = 1; d < pTx->NumDims; d++)
    {
        Printf(pri, "x%d", pTx->BaseSize[d]);
    }

    if (pTx->Layers > 0)
    {
        Printf(pri, "[%d layers]", pTx->Layers);
    }

    if (pTx->HasBorder)
    {
        Printf(pri, " with border");
    }

    switch (pTx->TextureType)
    {
        case IsUsingView:
            Printf(pri, " TextureType: IsUsingView(from SparseTexture %d)", pTx->SparseTexID);
            break;
        case IsView:
            Printf(pri, " TextureType: IsView");
            break;
        case IsSparse:
            Printf(pri, " TextureType: IsSparse");
            break;
        default:
            Printf(pri, " TextureType: IsDefault");
            MASSERT(pTx->TextureType == IsDefault);
            break;
    }

    if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
    {
        Printf(pri, " samples:%d", pTx->MsSamples);
    }
    else
    {
        Printf(pri, " MMLevels: %d->%d", pTx->BaseMM, pTx->MaxMM);
        Printf(pri, " Loaded MMLevels: %d->%d", pTx->LoadMinMM, pTx->LoadMaxMM);
    }

    Printf(pri, " %s", StrTexInternalFormat(pTx->InternalFormat));

    if (pTx->Attribs & IsDepth)
    {
        Printf(pri, " (%s)",
                mglEnumToString(pTx->DepthTexMode,"BadDepthTexMode",false));
    }

    Printf(pri, " %s\n", StrPatternCode(pTx->PatternCode).c_str());
}

//------------------------------------------------------------------------------
void RndTexturing::PrintBindlessTextures(Tee::Priority pri)
{
    // get a list of all the referenced texture objects for the lwrrenly
    // bound shaders.
    TextureFetcherState * pTxf = NULL;
    TextureObjectState * pTx = NULL;

    for ( int sboRegionIdx = Ti1d;
          sboRegionIdx < TiNUM_Indicies; sboRegionIdx++)
    {
        if ((m_pGLRandom->m_GpuPrograms.BindlessTxAttrNeeded(TexGen::hndlIdx[sboRegionIdx])) ||
            (m_pGLRandom->m_Pixels.BindlessTxAttrNeeded(TexGen::hndlIdx[sboRegionIdx])))
        {
            for (int sboRegionOffset = 0; sboRegionOffset < NumTxHandles; sboRegionOffset++)
            {
                GetTxState(sboRegionIdx*NumTxHandles+sboRegionOffset,
                           &pTxf, &pTx);
                PrintTextureData(pri,pTxf,pTx);
            }
        }
    }

    // Print out all the referenced texture handles stored in the SBO:
    Printf(pri, "SBO Texture Handles:\nOffset\tHandle\t\t\t\tTarget\n");
    for ( int sboRegionIdx = Ti1d;
          sboRegionIdx < TiNUM_Indicies; sboRegionIdx++)
    {
        if ((m_pGLRandom->m_GpuPrograms.BindlessTxAttrNeeded(TexGen::hndlIdx[sboRegionIdx])) ||
            (m_pGLRandom->m_Pixels.BindlessTxAttrNeeded(TexGen::hndlIdx[sboRegionIdx])))
        {
            for (int sboRegionOffset = 0; sboRegionOffset < NumTxHandles; sboRegionOffset++)
            {
                GetTxState(sboRegionIdx*NumTxHandles+sboRegionOffset, &pTxf, &pTx);
                Printf(pri, "0x%04x\t0x%016llx\t%s\n",
                    (unsigned int)((sboRegionIdx * NumTxHandles * sizeof(GLuint64)) +
                    sboRegionOffset*sizeof(GLuint64)),
                    m_TxHandles[sboRegionIdx*NumTxHandles+sboRegionOffset],
                    StrTexDim(pTx->Dim));
            }
        }
    }
}
//-----------------------------------------------------------------------------
// Find the texture state that matches the handle at txHandleIdx. Return
// pointers to both the TextureFetcherState and TextureObjectState.
void RndTexturing::GetTxState(
    UINT32 sboIdx,                  //in
    TextureFetcherState ** ppTxf,   //out
    TextureObjectState ** ppTx)     //out
{
    for (UINT32 txObjIdx = 0; txObjIdx < m_NumTextures; txObjIdx++)
    {
        if ( m_TxHandles[sboIdx] == m_Textures[txObjIdx].Handle )
        {
            *ppTxf = m_Txu.Fetchers + sboIdx;
            *ppTx = m_Textures + txObjIdx;
            return;
        }
    }
}

//------------------------------------------------------------------------------
void RndTexturing::PrintTextureUnit(Tee::Priority pri, GLint txu) const
{
    if (txu >= m_pGLRandom->NumTxFetchers())
        return;
    if (m_Txu.Fetchers[txu].BoundTexture < 0)
        return;

    const TextureFetcherState * pTxf = m_Txu.Fetchers + txu;

    Printf(pri, "Txu%d ", txu);

   if (pTxf->BoundTexture >= 0)
   {
      const TextureObjectState * pTx = &m_Textures[pTxf->BoundTexture];
      PrintTextureData(pri,pTxf,pTx);
   }
}

//------------------------------------------------------------------------------
void RndTexturing::PrintTextureData(Tee::Priority pri,
                                    const TextureFetcherState * pTxf,
                                    const TextureObjectState * pTx) const
{
    PrintTextureObject(pri, pTx);
    if (pTx->Dim != GL_TEXTURE_RENDERBUFFER_LW)
    {
        Printf(pri, "\tedge:%s,%s,%s\n",
            StrTexEdge(pTxf->Wrap[0]),
            StrTexEdge(pTxf->Wrap[1]),
            StrTexEdge(pTxf->Wrap[2]));
        Printf(pri, "\tfilter:%s,%s aniso:%d%s\n",
            StrTxFilter(pTxf->MinFilter),
            StrTxFilter(pTxf->MagFilter),
            (GLint)(0.5 + pTxf->MaxAnisotropy),
            pTxf->UseSeamlessLwbe ? " seamless" : "");
        Printf(pri, "\tlod:%.2f->%.2f%s%.2f\n",
            pTxf->MinLambda, pTxf->MaxLambda,
            pTxf->BiasLambda >= 0.0 ? "+" : "",
            pTxf->BiasLambda);
        if (pTxf->CompareMode != GL_NONE)
        {
            Printf(pri, "\tCompareMode:%s ShadowFunc:%s\n",
                mglEnumToString(pTxf->CompareMode, "BadCompareMode",false,
                    glezom_None),
                mglEnumToString(pTxf->ShadowFunc, "BadShadowFunc",false));
        }
        Printf(pri, "\tborder:%.3f,%.3f,%.3f,%.3f\n",
               pTxf->BorderValues[0],pTxf->BorderValues[1],pTxf->BorderValues[2],pTxf->BorderValues[3]);
    }
}
//------------------------------------------------------------------------------
string RndTexturing::StrPatternCode(GLint PatternCode) const
{
   switch (PatternCode)
   {
      case RND_TX_NAME_Checkers:       return "Checkers";
      case RND_TX_NAME_Random:         return "Random";
      case RND_TX_NAME_SolidColor:     return "SolidColor";
      case RND_TX_NAME_Tga:            return "Tga";
      case RND_TX_NAME_MsCircles:      return "MsCircles";
      case RND_TX_NAME_MsSquares:      return "MsSquares";
      case RND_TX_NAME_MsRandom:       return "MsRandom";
      case RND_TX_NAME_MsStripes:      return "MsStripes";

      default:
         if ((PatternCode >= RND_TX_NAME_FIRST_MATS_PATTERN) &&
             (PatternCode <  RND_TX_NAME_NUM))
         {
             string patternName = "bad mats pattern";
             m_PatternClass.GetPatternName(
                     PatternCode - RND_TX_NAME_FIRST_MATS_PATTERN,
                     &patternName);
             return patternName;
         }
         else
         {
             MASSERT(!"unsupported texture pattern");
             return "TxNameUnknown";
         }
   }
}

//------------------------------------------------------------------------------
const char * RndTexturing::StrTexInternalFormat(GLenum ifmt) const
{
    switch ( ifmt )
    {
        case 0:     return "none";
        default:    return mglEnumToString(ifmt,"BadTxIFmt",true);
    }

}

//------------------------------------------------------------------------------
const char * RndTexturing::StrTxFilter(GLenum filt) const
{
    return mglEnumToString(filt,"IlwalidFilter",true);
}

//------------------------------------------------------------------------------
const char * RndTexturing::StrTexDim(GLenum dim) const
{
    return mglEnumToString(dim,"IlwalidTexDim",false);
}

//------------------------------------------------------------------------------
const char * RndTexturing::StrTexEdge(GLenum mode) const
{
    return mglEnumToString(mode,"IlwalidTexEdge",false);
}

//------------------------------------------------------------------------------
const char * RndTexturing::StrDataType(GLenum t) const
{
    return mglEnumToString(t,"IlwalidDataType",false);
}

//------------------------------------------------------------------------------
const char * RndTexturing::StrLwllMode(GLenum c) const
{
    return mglEnumToString(c,"IlwalidLwllMode",false);
}

/**
 * @brief Set the filename for the .TGA file to load for texturing.
 */
void RndTexturing::SetTgaTexImageFileName (string fname)
{
    m_TgaTexImageName = fname;
}

//------------------------------------------------------------------------------
class TxFillBase : public RndTexturing::TxFill
{
public:
    TxFillBase
    (
        const RndTexturing::TextureObjectState * pTx
    )
      : m_pTx(pTx)
       ,m_FillCount(0)
    {
    }
    virtual ~TxFillBase() {}

protected:
    GLuint FillCount() const
    {
        return m_FillCount;
    }

    GLuint NextFillCount()
    {
        return m_FillCount++;
    }

    GLuint LoadBpt () const
    {
        return m_pTx->LoadBpt;
    }

    GLenum LoadType () const
    {
        return m_pTx->LoadType;
    }

    ColorUtils::Format HwFmt () const
    {
        return m_pTx->HwFmt;
    }

    bool IsLwbe () const
    {
        return ((m_pTx->Dim == GL_TEXTURE_LWBE_MAP_EXT) ||
                (m_pTx->Dim == GL_TEXTURE_LWBE_MAP_ARRAY_ARB));
    }

    GLuint MMLevel (GLuint w, GLuint h) const
    {
        GLuint mm = 0;

        while ((w>>mm) > m_pTx->BaseSize[RndTexturing::txWidth])
            ++mm;

        while ((h>>mm) > m_pTx->BaseSize[RndTexturing::txHeight])
            ++mm;

        return mm;
    }

    void Rgba8ToTxLoadFormat
    (
        const GLubyte * rgba8Pixel,
        GLubyte *       outputPixel
    ) const;

private:
    const RndTexturing::TextureObjectState * m_pTx;
    GLuint m_FillCount;
};

//------------------------------------------------------------------------------
void TxFillBase::Rgba8ToTxLoadFormat
(
    const GLubyte * rgba8Pixel,
    GLubyte *       outputPixel
) const
{
    // First, try standard data-types per-component.
    switch (LoadType())
    {
        case GL_HALF_FLOAT:
        {
            UINT16 * p = reinterpret_cast<UINT16*>(outputPixel);
            const GLuint numElem = m_pTx->LoadBpt/2;

            for (GLuint i = 0; i < numElem; i++)
                p[i] = Utility::Float32ToFloat16(rgba8Pixel[i] * 1/255.0F);
            break;
        }
        case GL_FLOAT:
        {
            GLfloat * p = reinterpret_cast<GLfloat*>(outputPixel);
            const GLuint numElem = m_pTx->LoadBpt/4;

            for (GLuint i = 0; i < numElem; i++)
                p[i] = rgba8Pixel[i] * 1/255.0F;
            break;
        }
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        {
            memcpy (outputPixel, rgba8Pixel, m_pTx->LoadBpt);
            break;
        }
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        {
            UINT16 * p = reinterpret_cast<UINT16*>(outputPixel);
            const GLuint numElem = m_pTx->LoadBpt/2;

            for (GLuint i = 0; i < numElem; i++)
                p[i] = rgba8Pixel[i] << 8 | rgba8Pixel[i];
            break;
        }
        case GL_INT:
        case GL_UNSIGNED_INT:
        {
            UINT32 * p = reinterpret_cast<UINT32*>(outputPixel);
            const GLuint numElem = m_pTx->LoadBpt/4;

            for (GLuint i = 0; i < numElem; i++)
                p[i] = rgba8Pixel[i] << 24 |
                       rgba8Pixel[i] << 16 |
                       rgba8Pixel[i] <<  8 |
                       rgba8Pixel[i];
            break;
        }
        default:
        {
            // Non-standard data type (i.e. GL_UNSIGNED_INT_2_10_10_10_REV).
            // We require that ColorUtils be able to handle these.
            const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(m_pTx->HwFmt);

            ColorUtils::Colwert(
                    reinterpret_cast<const char*>(rgba8Pixel),
                    ColorUtils::A8B8G8R8,
                    reinterpret_cast<char*>(outputPixel),
                    pFmtInfo->ExtColorFormat,
                    1);
            break;
        }
    }
}

//------------------------------------------------------------------------------
class TxFillCheckers : public TxFillBase
{
public:
    TxFillCheckers
    (
        const RndTexturing::TextureObjectState * pTx
    )
      : TxFillBase(pTx)
    {
    }
    virtual ~TxFillCheckers() {}

    virtual void Fill
    (
        GLubyte * pBuf,
        GLuint    w,
        GLuint    h
    );

private:
    enum ColorIdx
    {
        CiBlack
        ,CiDarkBlue
        ,CiDarkGreen
        ,CiDarkCyan
        ,CiDarkRed
        ,CiDarkPurple
        ,CiWhite
        ,CiLightYellow
        ,CiLightPurple
        ,CiLightRed
        ,CiLightCyan
        ,CiLightGreen
        ,CiLightBlue
        ,CiLightGray
        ,CiLightGray1
        ,CiLightGray2
        ,CiLightGray3
        ,CiLightGray4
        ,CiLightGray5
        ,CiLightGray6
        ,CiLightGray7
        ,CiRed
        ,CiGreen

        ,CiNumColors
    };
    static const GLubyte s_RgbaColors[CiNumColors][4];

    GLuint Face () const
    {
        if (IsLwbe())
        {
            // Assumes that the innermost loop in both FillTexLwbe and
            // FillTexLwbeArray is the per-face loop (which is true).
            return FillCount() % 6;
        }
        else
        {
            return 0;
        }
    }
};

const GLubyte TxFillCheckers::s_RgbaColors[TxFillCheckers::CiNumColors][4] =
{ //  red green blue alpha
    {   0,   0,   0,  64},  // CiBlack
    {   0,   0, 128,  64},  // CiDarkBlue
    {   0, 128,   0,  64},  // CiDarkGreen
    {   0, 128, 128,  64},  // CiDarkCyan
    { 128,   0,   0,  64},  // CiDarkRed
    { 128,   0, 128,  64},  // CiDarkPurple
    { 255, 255, 255, 196},  // CiWhite
    { 255, 255, 196, 196},  // CiLightYellow
    { 255, 196, 255, 196},  // CiLightPurple
    { 255, 196, 196, 196},  // CiLightRed
    { 196, 255, 255, 196},  // CiLightCyan
    { 196, 255, 196, 196},  // CiLightGreen
    { 196, 196, 255, 196},  // CiLightBlue
    { 196, 196, 196, 196},  // CiLightGray
    { 190, 190, 190, 190},  // CiLightGray1
    { 185, 185, 185, 185},  // CiLightGray2
    { 180, 180, 180, 180},  // CiLightGray3
    { 175, 175, 175, 175},  // CiLightGray4
    { 170, 170, 170, 170},  // CiLightGray5
    { 165, 165, 165, 165},  // CiLightGray6
    { 160, 160, 160, 160},  // CiLightGray7
    { 255,   0,   0, 255},  // CiRed
    {   0, 255,   0, 255},  // CiGreen
};

/*virtual*/
void TxFillCheckers::Fill
(
    GLubyte * pBuf,
    GLuint    w,
    GLuint    h
)
{
    const GLuint fc = NextFillCount();
    GLuint bpt = LoadBpt();

    const GLubyte * const rgba8Dark  =
            s_RgbaColors[CiBlack + Face()];

    const GLubyte * const rgba8Light =
            s_RgbaColors[(CiWhite + MMLevel(w, h)) % CiNumColors];

    const GLubyte * const rgba8Gray  =
            s_RgbaColors[fc % CiNumColors];

    GLubyte Dark[16];
    GLubyte Light[16];
    GLubyte Gray[16];

    Rgba8ToTxLoadFormat(rgba8Dark,  Dark);
    Rgba8ToTxLoadFormat(rgba8Light, Light);
    Rgba8ToTxLoadFormat(rgba8Gray,  Gray);

    GLuint halfWidth  = w >> 1;
    GLuint halfHeight = h >> 1;

    if (0 == halfWidth)
        halfWidth = 1;

    if (0 == halfHeight)
        halfHeight = 1;

    for (GLuint t = 0; t < h; t++)
    {
        GLubyte * p = pBuf + t*w*bpt;

        for (GLuint s = 0; s < w; s++)
        {
            // Divide into 4 quadrants, alternating black/while.
            GLuint tdist = t;
            GLuint sdist = s;
            GLubyte * color = Dark;

            if (t >= halfHeight)
            {
                tdist -= halfHeight;

                if (s >= halfWidth)
                    sdist -= halfWidth;
                else
                    color = Light;
            }
            else
            {
                if (s >= halfWidth)
                {
                    sdist -= halfWidth;
                    color = Light;
                }
            }

            // Just for fun, put thin diagonal lines of gray
            // in the center of each quadrant.

            if ((tdist > 3) &&
                    (sdist > 3) &&
                    (halfHeight - tdist > 3) &&
                    (halfWidth  - sdist > 3) &&
                    (((tdist ^ sdist) & 3) == 3))
            {
                color = Gray;
            }

            memcpy(p, color, bpt);
            p += bpt;
        }
    }
}

//------------------------------------------------------------------------------
class TxFillRandom : public TxFillBase
{
public:
    TxFillRandom
    (
        const RndTexturing::TextureObjectState * pTx,
        Random * pRand
    )
      : TxFillBase(pTx)
       ,m_pRand(pRand)
    {
    }
    virtual ~TxFillRandom() {}

    virtual void Fill
    (
        GLubyte * pBuf,
        GLuint    w,
        GLuint    h
    );

private:
    Random * m_pRand;
};

/*virtual*/
void TxFillRandom::Fill
(
    GLubyte * pBuf,
    GLuint    w,
    GLuint    h
)
{
    const GLuint bpt = LoadBpt();
    const GLenum loadType = LoadType();

    GLuint rnd;
    GLubyte * p = pBuf;
    const GLuint bytes = w*h*bpt;
    const GLubyte * pEnd = p + bytes;
    const GLubyte * pEndAligned = p + (bytes & ~3);

    if (loadType == GL_FLOAT || loadType == GL_HALF_FLOAT)
    {
        // In floating-point math, precision is lost if two operands are far
        // apart in magnitude, so it's helpful to keep most values near 0.0.

        while ( p < pEndAligned )
        {
            GLfloat tmp;

            switch (m_pRand->GetRandom(0,8))
            {
            case 0:
            case 1:
            case 2:  // small unsigned
                tmp = m_pRand->GetRandomFloat(0.0, 1.0);
                break;
            case 3:
            case 4:
            case 5: // small signed
                tmp = m_pRand->GetRandomFloat(-1.0, 1.0);
                break;
            case 6:
            case 7: // large signed
                tmp = m_pRand->GetRandomFloat(-100.0, 100.0);
                break;
            default:
            case 8:
                {   // fully random, even NAN, INF, etc.
                    UINT32 x = m_pRand->GetRandom();
                    GLfloat * pfx = reinterpret_cast<GLfloat*>(&x);
                    tmp = *pfx;
                    break;
                }
            }
            if (loadType == GL_FLOAT)
            {
                GLfloat * pF = reinterpret_cast<GLfloat*>(p);
                *pF = tmp;
                p += 4;
            }
            else
            {
                UINT16 * pHf = reinterpret_cast<UINT16*>(p);
                *pHf = Utility::Float32ToFloat16 (tmp);
                p += 2;
            }
        }
    }
    else
    {
        while ( p < pEndAligned )
        {
            rnd = m_pRand->GetRandom();

            p[0] = (UINT08)rnd;
            p[1] = (UINT08)(rnd >> 8);
            p[2] = (UINT08)(rnd >> 16);
            p[3] = (UINT08)(rnd >> 24);
            p += 4;
        }
    }
    while ( p < pEnd )
    {
        rnd = m_pRand->GetRandom();

        *p++ = (UINT08)(rnd >> 12);
    }
}

//------------------------------------------------------------------------------
class TxFillSolidColor : public TxFillBase
{
public:
    TxFillSolidColor
    (
        const RndTexturing::TextureObjectState * pTx,
        GLubyte rgbaColor[4]
    )
      : TxFillBase(pTx)
    {
        Rgba8ToTxLoadFormat(rgbaColor, m_Color);
    }
    virtual ~TxFillSolidColor() {}

    virtual void Fill
    (
        GLubyte * pBuf,
        GLuint    w,
        GLuint    h
    );

private:
    GLubyte m_Color[16];
};

/*virtual*/
void TxFillSolidColor::Fill
(
    GLubyte * pBuf,
    GLuint    w,
    GLuint    h
)
{
    const GLuint bpt = LoadBpt();
    GLubyte * p = pBuf;
    const GLubyte * pEnd = p + w*h*bpt;

    while (p < pEnd)
    {
        memcpy(p, m_Color, bpt);
        p += bpt;
    }
}

//------------------------------------------------------------------------------
class TxFillTga : public TxFillBase
{
public:
    TxFillTga
    (
        const RndTexturing::TextureObjectState * pTx,
        const string &tgaName
    )
      : TxFillBase(pTx)
       ,m_TgaName(tgaName)
    {
    }
    virtual ~TxFillTga() {}

    virtual void Fill
    (
        GLubyte * pBuf,
        GLuint    w,
        GLuint    h
    );

private:
    const string m_TgaName;
};

/*virtual*/
void TxFillTga::Fill
(
    GLubyte * pBuf,
    GLuint    w,
    GLuint    h
)
{
    const GLuint bpt = LoadBpt();
    RC rc = ImageFile::ReadTga (m_TgaName.c_str(), true, HwFmt(),
            pBuf, w, h, w*bpt);
    if (rc)
    {
        Printf(Tee::PriNormal,
            "Failed to read file \"%s\" for texture input (%d)\n",
            m_TgaName.c_str(), UINT32(rc));
    }
}

//------------------------------------------------------------------------------
class TxFillMats : public TxFillBase
{
public:
    TxFillMats
    (
        const RndTexturing::TextureObjectState * pTx,
        PatternClass * pPatClass,
        UINT32 patNum
    )
      : TxFillBase(pTx)
       ,m_pPatClass(pPatClass)
       ,m_PatNum(patNum)
       ,m_AltPatNum(patNum == 0 ? patNum+1 : patNum-1)
    {
    }
    virtual ~TxFillMats() {}

    virtual void Fill
    (
        GLubyte * pBuf,
        GLuint    w,
        GLuint    h
    );

private:
    PatternClass * m_pPatClass;
    const UINT32 m_PatNum;
    const UINT32 m_AltPatNum;
};

/*virtual*/
void TxFillMats::Fill
(
    GLubyte * pBuf,
    GLuint    w,
    GLuint    h
)
{
    const GLuint bpt = LoadBpt();
    UINT32 dummy;

    // This function is called N times for each layer of a 3d or array
    // texture, and each face of a lwbe texture, and produces the exact
    // same image each time.
    // This is bad for detecting errors, so we offset the texture one pixel
    // based on the fill-count, which increments for each layer/face.

    // Since we are telling PatternClass to use 32-bit writes, make sure that
    // starting address and length are 4-byte aligned.
    // Note that bpt is often not a multiple of 4 (i.e. RGB8 is 3 bytes).

    // Note that bpt*w*h may be as small as 1 byte, i.e. for the last mipmap
    // level of a GL_ALPHA8 texture.

    // We assume pBuf itself is 4-byte aligned by the heap manager.

    const UINT32 fc = NextFillCount();
    const UINT32 size        = w*h*bpt;
    const UINT32 alignedSize = size & ~3;
    const UINT32 extraBytes  = size & 3;
    const UINT32 startOffset = min(fc * bpt, alignedSize) & ~3;
    RC rc;

    if (alignedSize > startOffset)
    {
        rc = m_pPatClass->FillMemoryWithPattern (
            pBuf + startOffset,
            alignedSize - startOffset,
            1,
            alignedSize - startOffset,
            32,
            m_PatNum,
            &dummy);
    }

    if (OK == rc && startOffset > 0)
    {
        rc = m_pPatClass->FillMemoryWithPattern (
            pBuf,
            startOffset,
            1,
            startOffset,
            32,
            m_AltPatNum,
            &dummy);

        MASSERT(OK == rc);
    }
    MASSERT(OK == rc);

    if (extraBytes > 0)
    {
        memset(pBuf + alignedSize, fc, extraBytes);
    }
}

//-----------------------------------------------------------------------------
RndTexturing::TxFill * RndTexturing::TxFillFactory (TextureObjectState * pTx)
{
    TxFill * pTxFill = 0;

    if(m_pGLRandom->m_Misc.RestartSkipMask() & RND_TSTCTRL_RESTART_SKIP_txblack)
    {
        // DEBUG: force all texture images to be black.
        GLubyte black[4] = { 0,0,0,0 };

        return new TxFillSolidColor(pTx, black);
    }

    switch (pTx->PatternCode)
    {
        case RND_TX_NAME_Checkers:
            pTxFill = new TxFillCheckers(pTx);
            break;

        case RND_TX_NAME_Random:
            pTxFill = new TxFillRandom(pTx, &m_pFpCtx->Rand);
            break;

        case RND_TX_NAME_SolidColor:
        {
            union
            {
                UINT32 word;
                GLubyte bytes[4];
            } rgbaColor;

            rgbaColor.word = m_Pickers[RND_TX_CONST_COLOR].Pick();

            pTxFill = new TxFillSolidColor(pTx, rgbaColor.bytes);
            break;
        }
        case RND_TX_NAME_Tga:
            pTxFill = new TxFillTga(pTx, m_TgaTexImageName);
            break;

        default:
            pTxFill = new TxFillMats(pTx, &m_PatternClass,
                    (pTx->PatternCode - RND_TX_NAME_FIRST_MATS_PATTERN) %
                    RND_TX_NAME_NUM_MATS_PATTERNS);
            break;
    }

    return pTxFill;
}

//------------------------------------------------------------------------------
// Read back each dirty mipmap level of each texture and callwlate CRC values.
// Since we separately check our texture images before download, a miscompare
// here implies either a GL driver bug or a framebuffer ram problem.
//
UINT32 RndTexturing::CheckTextureImageLayers()
{

    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5))
        return 0;

    UINT32  crc = 0;
    vector <GLubyte> buf;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT |
                       GL_TEXTURE_UPDATE_BARRIER_BIT_EXT |
                       GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);

    for (GLint tx = 0; tx < (GLint)m_NumTextures; tx++)
    {
        const TextureObjectState *  pTx  = m_Textures + tx;

        if(pTx->DirtyLevelMask)
        {
            // we cant get access to multisampled textures.
            if (pTx->Dim == GL_TEXTURE_RENDERBUFFER_LW)
                continue;

            Printf(Tee::PriDebug, "\tReading back texture %d(ID:%d): DirtyLevelMask:0x%x\n",
                tx, pTx->Id, pTx->DirtyLevelMask);
            PrintTextureObject(Tee::PriDebug, pTx);

            // Tell the GL driver which texture object to look at.
            glActiveTexture(GL_TEXTURE0_ARB);

            // fixed function texturing not supported for 1D,2D, and lwbemap
            // array textures
            if (pTx->Dim != GL_TEXTURE_1D_ARRAY_EXT &&
                pTx->Dim != GL_TEXTURE_2D_ARRAY_EXT &&
                pTx->Dim != GL_TEXTURE_LWBE_MAP_ARRAY)
                glEnable(pTx->Dim);

            glBindTexture(pTx->Dim, pTx->Id);

            GLuint NumFaces = 1;
            GLenum Target   = pTx->Dim;
            if (pTx->Dim == GL_TEXTURE_LWBE_MAP_EXT)
            {
                NumFaces = 6;
                Target   = GL_TEXTURE_LWBE_MAP_POSITIVE_X_EXT;
            }

            // Only checksum the mipmap levels that are going to get modified.
            GLuint mm;
            for (mm = 0; mm <= pTx->LoadMaxMM; mm++)
            {
                if ( pTx->DirtyLevelMask & (1 << mm))
                {
                    GLuint w = pTx->BaseSize[txWidth]  >> mm;
                    GLuint h = pTx->BaseSize[txHeight] >> mm;
                    // GL_TEXTURE_LWBE_MAP_ARRAY_LW targets the depth must
                    // remain constant for all mipmap levels
                    GLuint d = (pTx->Dim == GL_TEXTURE_LWBE_MAP_ARRAY) ?
                        pTx->BaseSize[txDepth] : pTx->BaseSize[txDepth] >> mm;

                    w = w ? w : 1;
                    h = h ? h : 1;
                    d = d ? d : 1;

                    const size_t levelSize = (size_t)TexBytesLevel(pTx,mm);

                    if (levelSize > buf.size())
                    {
                        buf.resize(levelSize);
                    }

                    // For each lwbe-map face of the mipmap level
                    // (just 1 face, except for lwbe maps)
                    GLuint face;
                    for (face = 0; face < NumFaces; face++)
                    {
                        glGetTexImage(
                          Target + face,
                          mm,
                          pTx->LoadFormat,
                          pTx->LoadType,
                          &buf[0]
                          );

                        crc = Crc::Append(&buf[0], (UINT32)levelSize, crc);
                        Printf(Tee::PriDebug,
                            "\tLoop:%d Reading mm:%d face:%d w:%d h:%d d:%d, crc:%08x\n",
                            m_pFpCtx->LoopNum, mm,face,w,h,d,crc);
                        PrintTextureImageRaw(mm, w, h, d, &buf[0], pTx);
                    }
                }
            }
            if (pTx->Dim != GL_TEXTURE_1D_ARRAY_EXT &&
                    pTx->Dim != GL_TEXTURE_2D_ARRAY_EXT &&
                    pTx->Dim != GL_TEXTURE_LWBE_MAP_ARRAY)
                glDisable(pTx->Dim);
        }
    }

    if (mglGetRC() == OK)
        return crc;
    else
    {
        return (m_pGLRandom->GetLogMode() == GLRandomTest::Store ?
                (UINT32)-1 : (UINT32)-2);
    }
}

//-------------------------------------------------------------------------------------------------
void RndTexturing::PrintTextureImageRaw(GLuint mm,
                                     GLuint w,
                                     GLuint h,
                                     GLuint d,
                                     GLubyte *pBuf,
                                     const TextureObjectState * pTx)
{
    Tasker::DetachThread detach;
    constexpr auto pri = Tee::PriDebug;
    GLuint offset = 0;
    UINT32 texel = 0;
    Printf(pri, "mm:%d, w:%d, h:%d, d:%d\n", mm, w, h, d);
    for (GLuint depth = 0; depth < d; depth++)
    {
        for (GLuint height = 0; height < h; height++)
        {
            for (GLuint width = 0; width < w; width++, offset += pTx->LoadBpt)
            {
                texel = 0;
                switch (pTx->LoadBpt)
                {
                    case 16:
                        Printf(pri, "0x%x 0x%x 0x%x 0x%x ",
                               *((UINT32*)&pBuf[offset]),
                               *((UINT32*)&pBuf[offset+4]),
                               *((UINT32*)&pBuf[offset+8]),
                               *((UINT32*)&pBuf[offset+12]));
                        break;
                    case 8:
                        Printf(pri, "0x%x 0x%x ",
                               *((UINT32*)&pBuf[offset]),
                               *((UINT32*)&pBuf[offset+4]));
                        break;
                    case 4: texel |= (UINT32)pBuf[offset+3] << 24;
                    case 3: texel |= (UINT32)pBuf[offset+2] << 16;
                    case 2: texel |= (UINT32)pBuf[offset+1] << 8;
                    case 1: texel |= (UINT32)pBuf[offset];
                        Printf(pri, "0x%x ", texel);
                        break;
                    default: // strange format, just skip it.
                        break;
                }
                if (width && (width %100 == 0))
                {   // give the mods console a chance to display.
                    Printf(pri, "\n");
#ifndef USE_NEW_TASKER
                    Tasker::Yield();
#endif
                }
            }
            Printf(pri, "\n");
        }
        Printf(pri, "\n");
    }
    Printf(pri, "\n");
}

//-------------------------------------------------------------------------------------------------
RC RndTexturing::InitOpenGL()
{
    RC rc;
    //Attention this API is called 2x once by RndGpuPrograms::InitOpenGL() and again
    // by GLRandomTest::Setup() because RndGpuPrograms needs the SBO for the texture
    // handles created before loading the programs.
    // TODO: Fix this by creating a new API to create the TextureSBO.
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_bindless_texture) &&
        (0 == m_TxSbo.GpuAddr))
    {
        // Create a Shader Buffer Object to store the texture object handles.
        glGenBuffers( 1, &m_TxSbo.Id);
        glBindBuffer(GL_ARRAY_BUFFER, m_TxSbo.Id);   // read-write Sbo
        m_TxSbo.Rect[sboHeight] = TiNUM_Indicies;
        m_TxSbo.Rect[sboWidth] = 4;
        m_TxSbo.Rect[sboOffsetX] = 0;
        m_TxSbo.Rect[sboOffsetY] = 0;
        const GLint size = m_TxSbo.Rect[sboWidth] * m_TxSbo.Rect[sboHeight] *
                           sizeof(GLuint64);

        // allocate storage
        glBufferData(GL_ARRAY_BUFFER,   // target
                     size,              //sboSize,
                     NULL,              // data
                     GL_DYNAMIC_COPY);  // hint
        // get gpu address
        glGetBufferParameterui64vLW(
                    GL_ARRAY_BUFFER_ARB,
                    GL_BUFFER_GPU_ADDRESS_LW,
                    &m_TxSbo.GpuAddr);

        // make it accessable to all the shaders.
        glMakeBufferResidentLW(GL_ARRAY_BUFFER_ARB, GL_READ_WRITE);

        // release the current binding for now
        glBindBuffer(GL_ARRAY_BUFFER,0);
    }

    // We MUST check for OpenGL errors before calling CreateSparseTextures because that routine
    // will throw away any previous errors.
    CHECK_RC(mglGetRC());
    // Create sparse textures that other textures will use as view
    CHECK_RC(CreateSparseTextures());

    return rc;
}

GLRandom::SboState * RndTexturing::GetBindlessTextureState()
{
    return &m_TxSbo;
}

// Get the id of the texture bound to the specified fetcher.
RC RndTexturing::GetTextureId(int txfetcher, GLuint *pId)
{
    if (m_UseBindlessTextures)
        return RC::SOFTWARE_ERROR;

    const TextureFetcherState * pTxf = m_Txu.Fetchers + txfetcher;
    if (pTxf->BoundTexture >= 0)
    {
        *pId = m_Textures[pTxf->BoundTexture].Id;
        return OK;
    }
    return RC::SOFTWARE_ERROR;
}

// Find a texture that had a bindless handle created that matches the
// requirements (only textures that have a valid handle will also have
// picked and programmed texture parameters)
RC RndTexturing::GetBindlessTextureId
(
    const GLRandom::TxfReq &txfReq,
    GLuint *                pId
)
{
    UINT32 sboIdx = 0;
    switch (txfReq.Attrs & SAMDimFlags)
    {
        case Is1d:
            sboIdx = Ti1d;
            break;
        case Is2d:
            sboIdx = Ti2d;
            break;
        case Is3d:
            sboIdx = Ti3d;
            break;
        case IsLwbe:
            sboIdx = TiLwbe;
            break;
        case IsArray1d:
            sboIdx = TiArray1d;
            break;
        case IsArray2d:
            sboIdx = TiArray2d;
            break;
        case IsArrayLwbe:
            sboIdx = TiArrayLwbe;
            break;
        default:
            MASSERT(!"Bindless texture id requested for unsupported bindless "
                     "texture type");
            return RC::SOFTWARE_ERROR;
    }
    sboIdx *= NumTxHandles;

    for(UINT32 i = 0; i < NumTxHandles; i++, sboIdx++)
    {
        TextureFetcherState * pTxf = NULL;
        TextureObjectState * pTx = NULL;
        GetTxState(sboIdx, &pTxf, &pTx);
        if (TextureMatchesRequirements(txfReq, pTx->Index))
        {
            *pId = pTx->Id;
            return OK;
        }
    }

    return RC::SOFTWARE_ERROR;
}
