/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndFragment: OpenGL Randomizer for vertex data formats.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"
#include "core/include/coreutility.h"
#include "random.h"

using namespace GLRandom;

//------------------------------------------------------------------------------
RndFragment::RndFragment(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_FRAG_NUM_PICKERS, "Fragment")
{
    memset(&m_Fragment, 0, sizeof(m_Fragment));

    m_MaxBlendableFloatSize = 0;
    m_CBufPicks = 0;

    m_ScissorOverrideXY[0] = -1;
    m_ScissorOverrideXY[1] = -1;
    m_ScissorOverrideWH[0] = 0xffffffff;
    m_ScissorOverrideWH[1] = 0xffffffff;

    m_VportXIntersect       = 0;
    m_VportYIntersect       = 0;
    m_VportsSeparation      = 0;
    m_VportSwizzleX         = GL_VIEWPORT_SWIZZLE_POSITIVE_X_LW;
    m_VportSwizzleY         = GL_VIEWPORT_SWIZZLE_POSITIVE_Y_LW;
    m_VportSwizzleZ         = GL_VIEWPORT_SWIZZLE_POSITIVE_Z_LW;
    m_VportSwizzleW         = GL_VIEWPORT_SWIZZLE_POSITIVE_W_LW;

    m_EnableConservativeRaster = false;
    m_EnableConservativeRasterDilate = false;
    m_EnableConservativeRasterMode = false;
    m_DilateValueNormalized  = 0;
    m_RasterMode             = 0;
    m_SubpixelPrecisionBiasX = 0;
    m_SubpixelPrecisionBiasY = 0;
}

//------------------------------------------------------------------------------
RndFragment::~RndFragment()
{
}

//------------------------------------------------------------------------------
RC RndFragment::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICK_ITEMS;

        case RND_FRAG_ALPHA_FUNC:
            // fragment alpha test function
            m_Pickers[RND_FRAG_ALPHA_FUNC].ConfigRandom();
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem(20, GL_FALSE);    // i.e. disable alpha test
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem(20, GL_ALWAYS);
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem(20, GL_NOTEQUAL);
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem(10, GL_LESS);
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem(10, GL_LEQUAL);
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem(10, GL_GEQUAL);
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem(10, GL_GREATER);
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem( 5, GL_EQUAL);
            m_Pickers[RND_FRAG_ALPHA_FUNC].AddRandItem( 1, GL_NEVER);
            m_Pickers[RND_FRAG_ALPHA_FUNC].CompileRandom();
            break;

        case RND_FRAG_BLEND_ENABLE:
            m_Pickers[RND_FRAG_BLEND_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_BLEND_ENABLE].AddRandItem(98, GL_TRUE);
            m_Pickers[RND_FRAG_BLEND_ENABLE].AddRandItem( 2, GL_FALSE);
            m_Pickers[RND_FRAG_BLEND_ENABLE].CompileRandom();
            break;

        case RND_FRAG_BLEND_FUNC:
            // scaling function for color in blending
            m_Pickers[RND_FRAG_BLEND_FUNC].ConfigRandom();
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem( 1, GL_ZERO);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem( 1, GL_ONE);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_SRC_COLOR);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_ONE_MINUS_SRC_COLOR);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_SRC_ALPHA);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_ONE_MINUS_SRC_ALPHA);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_DST_COLOR);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_ONE_MINUS_DST_COLOR);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_DST_ALPHA);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem(10, GL_ONE_MINUS_DST_ALPHA);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem( 5, GL_SRC_ALPHA_SATURATE);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem( 2, GL_CONSTANT_COLOR_EXT);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem( 2, GL_ONE_MINUS_CONSTANT_COLOR_EXT);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem( 2, GL_CONSTANT_ALPHA_EXT);
            m_Pickers[RND_FRAG_BLEND_FUNC].AddRandItem( 2, GL_ONE_MINUS_CONSTANT_ALPHA_EXT);
            m_Pickers[RND_FRAG_BLEND_FUNC].CompileRandom();
            break;

        case RND_FRAG_BLEND_EQN:
            // what op to do with the scaled source and dest (normally addition)
            m_Pickers[RND_FRAG_BLEND_EQN].ConfigRandom();
            m_Pickers[RND_FRAG_BLEND_EQN].AddRandItem(10, GL_FUNC_ADD_EXT);
            m_Pickers[RND_FRAG_BLEND_EQN].AddRandItem(1, GL_MAX_EXT);
            m_Pickers[RND_FRAG_BLEND_EQN].AddRandItem(1, GL_MIN_EXT);
            m_Pickers[RND_FRAG_BLEND_EQN].AddRandItem(1, GL_FUNC_SUBTRACT_EXT);
            m_Pickers[RND_FRAG_BLEND_EQN].AddRandItem(1, GL_FUNC_REVERSE_SUBTRACT_EXT);
            m_Pickers[RND_FRAG_BLEND_EQN].CompileRandom();
            break;

        case RND_FRAG_ADV_BLEND_ENABLE:
            // enable advanced blending by default.
            m_Pickers[RND_FRAG_ADV_BLEND_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_ADV_BLEND_ENABLE].AddRandItem(50,GL_TRUE);
            m_Pickers[RND_FRAG_ADV_BLEND_ENABLE].AddRandItem(50,GL_FALSE);
            m_Pickers[RND_FRAG_ADV_BLEND_ENABLE].CompileRandom();
            break;

        case RND_FRAG_ADV_BLEND_PREMULTIPLIED_SRC:
            // specify if the fragment's RGB values have already been multiplied
            // by alpha.
            m_Pickers[RND_FRAG_ADV_BLEND_PREMULTIPLIED_SRC].ConfigRandom();
            m_Pickers[RND_FRAG_ADV_BLEND_PREMULTIPLIED_SRC].AddRandItem(50,GL_TRUE);
            m_Pickers[RND_FRAG_ADV_BLEND_PREMULTIPLIED_SRC].AddRandItem(50,GL_FALSE);
            m_Pickers[RND_FRAG_ADV_BLEND_PREMULTIPLIED_SRC].CompileRandom();
            break;

        case RND_FRAG_ADV_BLEND_OVERLAP_MODE:
            //This parameter determines the weights for the weighting functions
            //in the blend equation.
            m_Pickers[RND_FRAG_ADV_BLEND_OVERLAP_MODE].ConfigRandom();
            m_Pickers[RND_FRAG_ADV_BLEND_OVERLAP_MODE].AddRandItem(30,GL_UNCORRELATED_LW);
            m_Pickers[RND_FRAG_ADV_BLEND_OVERLAP_MODE].AddRandItem(30,GL_DISJOINT_LW);
            m_Pickers[RND_FRAG_ADV_BLEND_OVERLAP_MODE].AddRandItem(30,GL_CONJOINT_LW);
            m_Pickers[RND_FRAG_ADV_BLEND_OVERLAP_MODE].CompileRandom();
            break;

        case RND_FRAG_ADV_BLEND_EQN:
            // what type of advanced blending equation to use on the src & dest
            // (not supported with MRT)
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].ConfigRandom();
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_ZERO               );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_SRC_LW            );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_DST_LW            );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_SRC_OVER_LW       );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_DST_OVER_LW       );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_SRC_IN_LW         );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_DST_IN_LW         );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_SRC_OUT_LW        );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_DST_OUT_LW        );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_SRC_ATOP_LW       );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_DST_ATOP_LW       );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_XOR               );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_PLUS_LW           );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_PLUS_CLAMPED_LW   );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_PLUS_CLAMPED_ALPHA_LW   );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_MULTIPLY_LW       );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_SCREEN_LW         );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_OVERLAY_LW        );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_DARKEN_LW         );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_LIGHTEN_LW        );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_COLORDODGE_LW     );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_COLORBURN_LW      );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_HARDLIGHT_LW      );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_SOFTLIGHT_LW      );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_DIFFERENCE_LW     );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_MINUS_LW          );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_MINUS_CLAMPED_LW  );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_EXCLUSION_LW      );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_CONTRAST_LW       );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_ILWERT            );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_ILWERT_RGB_LW     );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_LINEARDODGE_LW    );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_LINEARBURN_LW     );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_VIVIDLIGHT_LW     );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_LINEARLIGHT_LW    );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 3, GL_PINLIGHT_LW       );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_HARDMIX_LW        );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_RED               );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_GREEN             );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_BLUE              );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_HSL_HUE_LW        );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_HSL_SATURATION_LW );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_HSL_COLOR_LW      );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].AddRandItem( 1, GL_HSL_LUMINOSITY_LW );
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].CompileRandom();
            break;

        case RND_FRAG_COLOR_MASK:
            // do/don't write each color channel (4 bit mask)
            m_Pickers[RND_FRAG_COLOR_MASK].ConfigRandom();
            m_Pickers[RND_FRAG_COLOR_MASK].AddRandItem(90, 0xf);   // all channels written
            m_Pickers[RND_FRAG_COLOR_MASK].AddRandItem( 4, 0x0);   // all channels masked
            m_Pickers[RND_FRAG_COLOR_MASK].AddRandItem( 3, 0x7);   // rgb only
            m_Pickers[RND_FRAG_COLOR_MASK].AddRandItem( 2, 0x8);   // alpha only
            m_Pickers[RND_FRAG_COLOR_MASK].AddRandRange(1, 0, 0xf);
            m_Pickers[RND_FRAG_COLOR_MASK].CompileRandom();
            break;

        case RND_FRAG_DEPTH_FUNC:
            // fragment Z test function
            m_Pickers[RND_FRAG_DEPTH_FUNC].ConfigRandom();
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(5, GL_FALSE);    // i.e. disable Z test
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(65, GL_LESS);
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(10, GL_ALWAYS);
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(5,  GL_NOTEQUAL);
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(2,  GL_LEQUAL);
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(5,  GL_GEQUAL);
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(2,  GL_GREATER);
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(2,  GL_EQUAL);
            m_Pickers[RND_FRAG_DEPTH_FUNC].AddRandItem(2,  GL_NEVER);
            m_Pickers[RND_FRAG_DEPTH_FUNC].CompileRandom();
            break;

        case RND_FRAG_DEPTH_MASK:
            m_Pickers[RND_FRAG_DEPTH_MASK].ConfigRandom();
            m_Pickers[RND_FRAG_DEPTH_MASK].AddRandItem(90, GL_TRUE);
            m_Pickers[RND_FRAG_DEPTH_MASK].AddRandItem(10, GL_FALSE);
            m_Pickers[RND_FRAG_DEPTH_MASK].CompileRandom();
            break;

        case RND_FRAG_DITHER_ENABLE:
            m_Pickers[RND_FRAG_DITHER_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_DITHER_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_FRAG_DITHER_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_FRAG_DITHER_ENABLE].CompileRandom();
            break;

        case RND_FRAG_LOGIC_OP:
            // fragment logic op
            m_Pickers[RND_FRAG_LOGIC_OP].ConfigRandom();
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(44, GL_FALSE);      // i.e. disable color logic op
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_COPY);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_CLEAR);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_SET);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_COPY_ILWERTED);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_NOOP);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_ILWERT);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_AND);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_NAND);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_OR);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_NOR);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_XOR);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_EQUIV);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_AND_REVERSE);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_AND_ILWERTED);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_OR_REVERSE);
            m_Pickers[RND_FRAG_LOGIC_OP].AddRandItem(1, GL_OR_ILWERTED);
            m_Pickers[RND_FRAG_LOGIC_OP].CompileRandom();
            break;

        case RND_FRAG_STENCIL_FUNC:
            // comparison with stencil buffer for fragment testing
            m_Pickers[RND_FRAG_STENCIL_FUNC].ConfigRandom();
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(5, GL_FALSE);  // i.e. disable stencil test
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(15, GL_ALWAYS);
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(10, GL_NOTEQUAL);
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(1, GL_LESS);
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(1, GL_LEQUAL);
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(1, GL_GEQUAL);
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(1, GL_GREATER);
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(1, GL_EQUAL);
            m_Pickers[RND_FRAG_STENCIL_FUNC].AddRandItem(1, GL_NEVER);
            m_Pickers[RND_FRAG_STENCIL_FUNC].CompileRandom();
            break;

        case RND_FRAG_STENCIL_OP:
            // what to do to the stencil buffer when op passes/fail
            m_Pickers[RND_FRAG_STENCIL_OP].ConfigRandom();
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(5, GL_INCR_WRAP_EXT);
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(3, GL_DECR_WRAP_EXT);
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(2, GL_ILWERT);
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(1, GL_KEEP);
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(1, GL_ZERO);
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(1, GL_REPLACE);
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(1, GL_INCR);
            m_Pickers[RND_FRAG_STENCIL_OP].AddRandItem(1, GL_DECR);
            m_Pickers[RND_FRAG_STENCIL_OP].CompileRandom();
            break;

        case RND_FRAG_SCISSOR_ENABLE:
            // enable scissor box that prevents drawing to the outer 5 pixels of the screen.
            m_Pickers[RND_FRAG_SCISSOR_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_SCISSOR_ENABLE].AddRandItem(4, GL_FALSE);
            m_Pickers[RND_FRAG_SCISSOR_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_FRAG_SCISSOR_ENABLE].CompileRandom();
            break;

        case RND_FRAG_BLEND_COLOR:
            // color
            m_Pickers[RND_FRAG_BLEND_COLOR].FConfigRandom();
            m_Pickers[RND_FRAG_BLEND_COLOR].FAddRandRange(1, 0.0, 1.0);
            m_Pickers[RND_FRAG_BLEND_COLOR].CompileRandom();
            break;

        case RND_FRAG_BLEND_ALPHA:
            // transparency
            m_Pickers[RND_FRAG_BLEND_ALPHA].FConfigRandom();
            m_Pickers[RND_FRAG_BLEND_ALPHA].FAddRandRange(1, 0.0, 1.0);
            m_Pickers[RND_FRAG_BLEND_ALPHA].CompileRandom();
            break;

        case RND_FRAG_STENCIL_2SIDE:
            // use the EXT_stencil_two_side extension (if present)
            m_Pickers[RND_FRAG_STENCIL_2SIDE].ConfigRandom();
            m_Pickers[RND_FRAG_STENCIL_2SIDE].AddRandItem(4, GL_TRUE);
            m_Pickers[RND_FRAG_STENCIL_2SIDE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_FRAG_STENCIL_2SIDE].CompileRandom();
            break;

        case RND_FRAG_DEPTH_CLAMP:
            // use the LW_depth_clamp extension to clamp at the near/far planes instead of clipping.
            m_Pickers[RND_FRAG_DEPTH_CLAMP].ConfigRandom();
            m_Pickers[RND_FRAG_DEPTH_CLAMP].AddRandItem(4, GL_FALSE);
            m_Pickers[RND_FRAG_DEPTH_CLAMP].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_FRAG_DEPTH_CLAMP].CompileRandom();
            break;

        case RND_FRAG_VIEWPORT_OFFSET_X:
            // Offset and reduce the viewport by this many pixels in x.
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_X].ConfigRandom();
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_X].AddRandItem(19, 0);
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_X].AddRandRange(1, 1, 32);
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_X].CompileRandom();
            break;

        case RND_FRAG_VIEWPORT_OFFSET_Y:
            // Offset and reduce the viewport by this many pixels in y.
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_Y].ConfigRandom();
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_Y].AddRandItem(19, 0);
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_Y].AddRandRange(1, 1, 32);
            m_Pickers[RND_FRAG_VIEWPORT_OFFSET_Y].CompileRandom();
            break;

        case RND_FRAG_DEPTH_BOUNDS_TEST:
            // do/don't enable GL_EXT_depth_bounds_test (lw35+)
            m_Pickers[RND_FRAG_DEPTH_BOUNDS_TEST].ConfigRandom();
            m_Pickers[RND_FRAG_DEPTH_BOUNDS_TEST].AddRandItem(95, GL_FALSE);
            m_Pickers[RND_FRAG_DEPTH_BOUNDS_TEST].AddRandItem( 5, GL_TRUE);
            m_Pickers[RND_FRAG_DEPTH_BOUNDS_TEST].CompileRandom();
            break;

        case RND_FRAG_MULTISAMPLE_ENABLE:
            m_Pickers[RND_FRAG_MULTISAMPLE_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_MULTISAMPLE_ENABLE].AddRandItem(95, GL_TRUE);
            m_Pickers[RND_FRAG_MULTISAMPLE_ENABLE].AddRandItem( 5, GL_FALSE);
            m_Pickers[RND_FRAG_MULTISAMPLE_ENABLE].CompileRandom();
            break;

        // Used by tests that don't use per-sample shading or XFB.
        case RND_FRAG_SHADING_RATE_CONTROL_ENABLE:
            m_Pickers[RND_FRAG_SHADING_RATE_CONTROL_ENABLE].ConfigConst(GL_FALSE);
            break;
            
        // relies on SHADING_RATE_CONTROL_ENABLE
        case RND_FRAG_SHADING_RATE_IMAGE_ENABLE:
            m_Pickers[RND_FRAG_SHADING_RATE_IMAGE_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_SHADING_RATE_IMAGE_ENABLE].AddRandItem(50, GL_TRUE);
            m_Pickers[RND_FRAG_SHADING_RATE_IMAGE_ENABLE].AddRandItem(50, GL_FALSE);
            m_Pickers[RND_FRAG_SHADING_RATE_IMAGE_ENABLE].CompileRandom();
            break;
            
        case RND_FRAG_ALPHA_TO_COV_ENABLE:
            m_Pickers[RND_FRAG_ALPHA_TO_COV_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_ALPHA_TO_COV_ENABLE].AddRandItem(95, GL_FALSE);
            m_Pickers[RND_FRAG_ALPHA_TO_COV_ENABLE].AddRandItem( 5, GL_TRUE);
            m_Pickers[RND_FRAG_ALPHA_TO_COV_ENABLE].CompileRandom();
            break;

        case RND_FRAG_ALPHA_TO_ONE_ENABLE:
            m_Pickers[RND_FRAG_ALPHA_TO_ONE_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_ALPHA_TO_ONE_ENABLE].AddRandItem(95, GL_FALSE);
            m_Pickers[RND_FRAG_ALPHA_TO_ONE_ENABLE].AddRandItem( 5, GL_TRUE);
            m_Pickers[RND_FRAG_ALPHA_TO_ONE_ENABLE].CompileRandom();
            break;

        case RND_FRAG_SAMPLE_COVERAGE_ENABLE:
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ENABLE].AddRandItem(95, GL_FALSE);
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ENABLE].AddRandItem( 5, GL_TRUE);
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ENABLE].CompileRandom();
            break;

        case RND_FRAG_SAMPLE_COVERAGE:
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE].FConfigRandom();
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE].FAddRandRange(1, 0.0, 1.0);
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE].CompileRandom();
            break;

        case RND_FRAG_SAMPLE_COVERAGE_ILWERT:
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ILWERT].ConfigRandom();
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ILWERT].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ILWERT].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ILWERT].CompileRandom();
            break;

        case RND_FRAG_STENCIL_WRITE_MASK:
            // glStencilMask, write mask
            m_Pickers[RND_FRAG_STENCIL_WRITE_MASK].ConfigRandom();
            m_Pickers[RND_FRAG_STENCIL_WRITE_MASK].AddRandItem(18, 0xffffffff);
            m_Pickers[RND_FRAG_STENCIL_WRITE_MASK].AddRandItem( 1, 0);
            m_Pickers[RND_FRAG_STENCIL_WRITE_MASK].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_FRAG_STENCIL_WRITE_MASK].CompileRandom();
            break;

        case RND_FRAG_STENCIL_READ_MASK:
            // glStencilFunc, read/compare mask
            m_Pickers[RND_FRAG_STENCIL_READ_MASK].ConfigRandom();
            m_Pickers[RND_FRAG_STENCIL_READ_MASK].AddRandItem(1, 0xff);
            m_Pickers[RND_FRAG_STENCIL_READ_MASK].AddRandItem(1, 0x3);
            m_Pickers[RND_FRAG_STENCIL_READ_MASK].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_FRAG_STENCIL_READ_MASK].CompileRandom();
            break;

        case RND_FRAG_STENCIL_REF:
            // glStencilFunc, compare value
            m_Pickers[RND_FRAG_STENCIL_REF].ConfigRandom();
            m_Pickers[RND_FRAG_STENCIL_REF].AddRandItem(1, 0);
            m_Pickers[RND_FRAG_STENCIL_REF].AddRandItem(1, 1);
            m_Pickers[RND_FRAG_STENCIL_REF].AddRandItem(1, 0xffffffff);
            m_Pickers[RND_FRAG_STENCIL_REF].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_FRAG_STENCIL_REF].CompileRandom();
            break;

        case RND_FRAG_RASTER_DISCARD:
            // GL_RASTERIZER_DISCARD_LW, used for debugging mostly
            m_Pickers[RND_FRAG_RASTER_DISCARD].ConfigConst(0);
            break;

        case RND_FRAG_VPORT_X:
            m_Pickers[RND_FRAG_VPORT_X].FConfigRandom();
            m_Pickers[RND_FRAG_VPORT_X].FAddRandRange(1, 0.0,
                            (static_cast<float>(m_pGLRandom->GetFboWidth())));
            m_Pickers[RND_FRAG_VPORT_X].CompileRandom();
            break;

        case RND_FRAG_VPORT_Y:
            m_Pickers[RND_FRAG_VPORT_Y].FConfigRandom();
            m_Pickers[RND_FRAG_VPORT_Y].FAddRandRange(1, 0.0,
                            (static_cast<float>(m_pGLRandom->GetFboHeight())));
            m_Pickers[RND_FRAG_VPORT_Y].CompileRandom();
            break;

        case RND_FRAG_VPORT_SEP:
            m_Pickers[RND_FRAG_VPORT_SEP].FConfigRandom();
            // A separation of 0 - 5 pixels between viewports
            m_Pickers[RND_FRAG_VPORT_SEP].FAddRandItem(30, 0.0F);
            m_Pickers[RND_FRAG_VPORT_SEP].FAddRandRange(70, 0.0F, 5.0f);
            m_Pickers[RND_FRAG_VPORT_SEP].CompileRandom();
            break;

        case RND_FRAG_VPORT_SWIZZLE_X:
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_X].ConfigRandom();
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_X].AddRandItem(4, GL_VIEWPORT_SWIZZLE_POSITIVE_X_LW);
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_X].AddRandItem(1, GL_VIEWPORT_SWIZZLE_NEGATIVE_X_LW);
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_X].CompileRandom();
            break;

        case RND_FRAG_VPORT_SWIZZLE_Y:
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Y].ConfigRandom();
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Y].AddRandItem(4, GL_VIEWPORT_SWIZZLE_POSITIVE_Y_LW);
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Y].AddRandItem(1, GL_VIEWPORT_SWIZZLE_NEGATIVE_Y_LW);
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Y].CompileRandom();
            break;

        case RND_FRAG_VPORT_SWIZZLE_Z:
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Z].ConfigRandom();
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Z].AddRandItem(4, GL_VIEWPORT_SWIZZLE_POSITIVE_Z_LW);
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Z].AddRandItem(1, GL_VIEWPORT_SWIZZLE_NEGATIVE_Z_LW);
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_Z].CompileRandom();
            break;

        case RND_FRAG_VPORT_SWIZZLE_W:
            m_Pickers[RND_FRAG_VPORT_SWIZZLE_W].ConfigConst(GL_VIEWPORT_SWIZZLE_POSITIVE_W_LW);
            break;

        case RND_FRAG_CONS_RASTER_ENABLE:
            m_Pickers[RND_FRAG_CONS_RASTER_ENABLE].ConfigRandom();
            m_Pickers[RND_FRAG_CONS_RASTER_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_FRAG_CONS_RASTER_ENABLE].AddRandItem(9, GL_FALSE);
            m_Pickers[RND_FRAG_CONS_RASTER_ENABLE].CompileRandom();
            break;

        case RND_FRAG_CONS_RASTER_DILATE_VALUE:
            m_Pickers[RND_FRAG_CONS_RASTER_DILATE_VALUE].FConfigRandom();
            m_Pickers[RND_FRAG_CONS_RASTER_DILATE_VALUE].FAddRandItem(1, 0.0);
            m_Pickers[RND_FRAG_CONS_RASTER_DILATE_VALUE].FAddRandRange(1, 0.0, 1.0);
            m_Pickers[RND_FRAG_CONS_RASTER_DILATE_VALUE].CompileRandom();
            break;

        case RND_FRAG_CONS_RASTER_UNDERESTIMATE:
            m_Pickers[RND_FRAG_CONS_RASTER_UNDERESTIMATE].ConfigRandom();
            m_Pickers[RND_FRAG_CONS_RASTER_UNDERESTIMATE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_FRAG_CONS_RASTER_UNDERESTIMATE].AddRandItem(9, GL_FALSE);
            m_Pickers[RND_FRAG_CONS_RASTER_UNDERESTIMATE].CompileRandom();
            break;

        case RND_FRAG_CONS_RASTER_SNAP_MODE:
            m_Pickers[RND_FRAG_CONS_RASTER_SNAP_MODE].ConfigRandom();
            m_Pickers[RND_FRAG_CONS_RASTER_SNAP_MODE].AddRandItem(1, GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_TRIANGLES_LW);
            m_Pickers[RND_FRAG_CONS_RASTER_SNAP_MODE].AddRandItem(1, GL_CONSERVATIVE_RASTER_MODE_POST_SNAP_LW);
            m_Pickers[RND_FRAG_CONS_RASTER_SNAP_MODE].AddRandItem(1, GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_LW);
            m_Pickers[RND_FRAG_CONS_RASTER_SNAP_MODE].CompileRandom();
            break;

        case RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_X:
            m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_X].FConfigRandom();
            m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_X].FAddRandRange(1, 0.0, 1.0);
            m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_X].CompileRandom();
            break;

        case RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_Y:
            m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_Y].FConfigRandom();
            m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_Y].FAddRandRange(1, 0.0, 1.0);
            m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_Y].CompileRandom();
            break;

    }
    return OK;
}

//------------------------------------------------------------------------------
// Set the max bit-depth of float surface that is blendable. (default 0)
void RndFragment::SetMaxBlendableFloatSize(int depth)
{
    m_MaxBlendableFloatSize = depth;
}

//------------------------------------------------------------------------------
void RndFragment::SetScissorOverride
(
    GLint x,
    GLint y,
    GLsizei w,
    GLsizei h
)
{
    m_ScissorOverrideXY[0] = x;
    m_ScissorOverrideXY[1] = y;
    m_ScissorOverrideWH[0] = w;
    m_ScissorOverrideWH[1] = h;
}

//------------------------------------------------------------------------------
RC RndFragment::InitOpenGL()
{
    RC rc;
    glGetIntegerv(GL_SCISSOR_BOX, m_WindowRect);
    m_CBufPicks = new CBufPicks[ m_pGLRandom->NumColorSurfaces() ];
    memset(m_CBufPicks, 0, sizeof(CBufPicks)*m_pGLRandom->NumColorSurfaces() );

    // Apply scissor override (a debugging feature) now before the
    // clear is applied, to reduce gpu cycles even during clears.
    if (m_ScissorOverrideXY[0] >= 0)
    {
        glScissor(
            m_ScissorOverrideXY[0],
            m_ScissorOverrideXY[1],
            m_ScissorOverrideWH[0],
            m_ScissorOverrideWH[1]
            );
        glEnable(GL_SCISSOR_TEST);
    }

    m_EnableConservativeRaster = 
        m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_conservative_raster) == GL_TRUE;
    m_EnableConservativeRasterDilate = m_EnableConservativeRaster &&
        m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_conservative_raster_dilate);
    m_EnableConservativeRasterMode = m_EnableConservativeRaster &&
        m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_conservative_raster_pre_snap);

    if (m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_shading_rate_image))
    {
        CHECK_RC(SetupShadingRateTexture());
    }
    return OK;
}

//-------------------------------------------------------------------------------------------------
// Create and initialize a 2D texture that is used to reference the specific shading rate for each
// 16x16 pixel quadrant in the framebuffer.
// Note: The typical application of a variable shading rate is in Virtual reality. When the eye is
// folwsed on a particular section of the image we want to shade that area with higher frequency
// (multiple samples) and shade the peripheral areas with a lower frequence(smaller number of 
// samples).
RC RndFragment::SetupShadingRateTexture()
{
    // Setup a Shading Rate texture with more shading towards the center of the framebuffer.
    const GLint srHeight = ALIGN_UP(m_pGLRandom->DispHeight(), (UINT32)16) / 16;
    const GLint srWidth = ALIGN_UP(m_pGLRandom->DispWidth(), (UINT32)16) / 16;
    GLint numLayers = m_pGLRandom->GetNumLayersInFBO();
    // The pixel format for the shading image is 8bits (although only 4 are technically
    // required). We need to make sure this GL state is set so that we dont try to access
    // uninitialized data at the bottom of the texure (which would translate to corruption
    // at the top of the screen).
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // The default size of the shading rate palette is 16 so make sure we fill in all of the 
    // entries with valid rates. Setup the pallet as follows:
    // [0] reserved for the default case when the texel coordinates (u, v, w) are outside the
    // bounds of the shading rate texture.
    // [1..11] have decreasing level of sampling
    // [13..15] use default case of single sample
    // This palette returns the *base* shading rate. The final shading rate is obtained as
    // described in the GL_LW_shading_rate_image.txt extension. 
    const GLenum srPalette[] =
    {
        GL_SHADING_RATE_1_ILWOCATION_PER_PIXEL_LW, // default case
        GL_SHADING_RATE_16_ILWOCATIONS_PER_PIXEL_LW, // "supersample" rates
        GL_SHADING_RATE_8_ILWOCATIONS_PER_PIXEL_LW,
        GL_SHADING_RATE_4_ILWOCATIONS_PER_PIXEL_LW,
        GL_SHADING_RATE_2_ILWOCATIONS_PER_PIXEL_LW,
        GL_SHADING_RATE_1_ILWOCATION_PER_PIXEL_LW,
        GL_SHADING_RATE_1_ILWOCATION_PER_1X2_PIXELS_LW,
        GL_SHADING_RATE_1_ILWOCATION_PER_2X1_PIXELS_LW,
        GL_SHADING_RATE_1_ILWOCATION_PER_2X2_PIXELS_LW,
        GL_SHADING_RATE_1_ILWOCATION_PER_2X4_PIXELS_LW,
        GL_SHADING_RATE_1_ILWOCATION_PER_4X2_PIXELS_LW,
        GL_SHADING_RATE_1_ILWOCATION_PER_4X4_PIXELS_LW, // "coarse" rates.
        GL_SHADING_RATE_1_ILWOCATION_PER_PIXEL_LW, // default case
        GL_SHADING_RATE_1_ILWOCATION_PER_PIXEL_LW, // default case
        GL_SHADING_RATE_1_ILWOCATION_PER_PIXEL_LW, // default case
        GL_SHADING_RATE_1_ILWOCATION_PER_PIXEL_LW, // default case
    };
        
    // Initialize every viewport with the same pallet. Note that this pallet will not take 
    // affect util the shading rate image is enabled.
    for (int i = 0; i < MAX_VIEWPORTS; i++)
    {
        GLsizei count = static_cast<GLsizei>(NUMELEMS(srPalette));
        glShadingRateImagePaletteLW(i, 0, count, srPalette);
    }

    // Initialize the texture so that sampling is increases as we move towards the center of the
    // screen.
    m_ShadingRates.resize(srWidth*srHeight, 0);
    const float deltaX = static_cast<float>(srWidth)/20.0f;
    const float deltaY = static_cast<float>(srHeight)/20.0f;
    const int xCenter = srWidth/2;
    const int yCenter = srHeight/2;
    for (int y = 0; y < srHeight; y++)
    {
        UINT32 yIndex = abs(y - yCenter);
        yIndex = static_cast<UINT32>((static_cast<float>(yIndex) / deltaY));
        for (int x = 0; x < srWidth; x++)
        {
            // callwlate the delta from the center.
            UINT32 xIndex = abs(x - xCenter);
            xIndex = static_cast<UINT32>((static_cast<float>(xIndex) / deltaX));
            GLubyte srIndex = max(xIndex, yIndex);
            // add 1 to jump passed the default entry.                                     
            m_ShadingRates[y * srWidth + x] = srIndex+1;
        }
    }
    glGenTextures(1, &m_SRTex);
    if (numLayers)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_SRTex);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8UI, srWidth, srHeight, numLayers);
        for (int layer = 0; layer < numLayers; layer++)
        {   // populate all of the layers with the same shading rates.
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY // dim
                            ,0                  // mmLevel
                            ,0                  // xOffset
                            ,0                  // yOffset
                            ,layer              // zOffset
                            ,srWidth
                            ,srHeight
                            ,1                  // depth
                            ,GL_RED_INTEGER     // format
                            ,GL_UNSIGNED_BYTE   // type
                            ,m_ShadingRates.data()); //data
        }
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, m_SRTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, srWidth, srHeight);
        glTexSubImage2D(GL_TEXTURE_2D
                        ,0
                        ,0
                        ,0 
                        ,srWidth
                        ,srHeight 
                        ,GL_RED_INTEGER
                        ,GL_UNSIGNED_BYTE
                        ,m_ShadingRates.data());
    }
    glShadingRateImageBarrierLW(GL_TRUE);
    //unbind the current texture object.
    numLayers ? glBindTexture(GL_TEXTURE_2D_ARRAY, 0) : glBindTexture(GL_TEXTURE_2D, 0);

    return mglGetRC();
}

void RndFragment::DisplayShadingRateImage(GLuint width, GLuint height)
{
    Printf(Tee::PriNormal, "ShadingRate Image: width=%d, height=%d\n", width, height);
    for (UINT32 y = 0; y < height; y++)
    {
        for (UINT32 x = 0; x < width; x++)
        {
            Printf(Tee::PriNormal, "%02d ", m_ShadingRates[y*width+x]);
        }
        Printf(Tee::PriNormal, "\n");
    }
    
}

void RndFragment::PickAdvancedBlending(UINT32 mrtIdx)
{
    // This extension has several limitations worth noting.  First, the new blend
    // equations are not supported while rendering to more than one color buffer
    // at once; an ILWALID_OPERATION will be generated if an application attempts
    // to render any primitives in this unsupported configuration.  Additionally,
    // blending precision may be limited to 16-bit floating-point, which could
    // result in a loss of precision and dynamic range for framebuffer formats
    // with 32-bit floating-point components, and in a loss of precision for
    // formats with 12- and 16-bit signed or unsigned normalized integer
    // components.
    // We only use the _coherent version of this extension, because glrandom
    // draws overlapping geometry that will render unpredictably on
    // gpus that only support the non-coherent version.

    if (m_pGLRandom->NumColorSurfaces() == 1 && mrtIdx == 0)
    {
        m_CBufPicks[0].AdvBlendEnabled =
            m_Pickers[RND_FRAG_ADV_BLEND_ENABLE].Pick();
        m_CBufPicks[0].AdvBlendOverlapMode =
            m_Pickers[RND_FRAG_ADV_BLEND_OVERLAP_MODE].Pick();
        m_CBufPicks[0].AdvBlendPremultipliedSrc =
            m_Pickers[RND_FRAG_ADV_BLEND_PREMULTIPLIED_SRC].Pick();
        m_CBufPicks[0].AdvBlendEquation =
            m_Pickers[RND_FRAG_ADV_BLEND_EQN].Pick();

        if (!m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_blend_equation_advanced_coherent))
        {
            m_CBufPicks[0].AdvBlendEnabled = GL_FALSE;
            m_CBufPicks[0].AdvBlendOverlapMode = GL_UNCORRELATED_LW;
            m_CBufPicks[0].AdvBlendPremultipliedSrc = GL_FALSE;
            m_CBufPicks[0].AdvBlendEquation = GL_ZERO;
        }
    }
    else
    {
        m_CBufPicks[mrtIdx].AdvBlendEnabled = GL_FALSE;
        m_CBufPicks[mrtIdx].AdvBlendOverlapMode = GL_UNCORRELATED_LW;
        m_CBufPicks[mrtIdx].AdvBlendPremultipliedSrc = GL_FALSE;
        m_CBufPicks[mrtIdx].AdvBlendEquation = GL_ZERO;
    }
}

//------------------------------------------------------------------------------
RC RndFragment::Restart()
{
    RC rc;
    m_Pickers.Restart();
    // Set the full scree viewport in case multiple viewports is not supported.
    // This will prevent inconsistent results when regressing frames with a single loop.
    SetFullScreelwiewport();

    // Select intersection point of 4 viewports for this frame.
    m_VportXIntersect = m_Pickers[RND_FRAG_VPORT_X].FPick();
    m_VportYIntersect = m_Pickers[RND_FRAG_VPORT_Y].FPick();
    GLfloat separation = m_Pickers[RND_FRAG_VPORT_SEP].FPick();
    SetVportSeparation(separation);

    // Pick viewport swizzling parameters for this frame
    m_VportSwizzleX = m_Pickers[RND_FRAG_VPORT_SWIZZLE_X].Pick();
    m_VportSwizzleY = m_Pickers[RND_FRAG_VPORT_SWIZZLE_Y].Pick();
    m_VportSwizzleZ = m_Pickers[RND_FRAG_VPORT_SWIZZLE_Z].Pick();
    m_VportSwizzleW = m_Pickers[RND_FRAG_VPORT_SWIZZLE_W].Pick();

    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2))
    {   // restore back to default. 
        m_VportSwizzleX = GL_VIEWPORT_SWIZZLE_POSITIVE_X_LW;
        m_VportSwizzleY = GL_VIEWPORT_SWIZZLE_POSITIVE_Y_LW;
        m_VportSwizzleZ = GL_VIEWPORT_SWIZZLE_POSITIVE_Z_LW;
        m_VportSwizzleW = GL_VIEWPORT_SWIZZLE_POSITIVE_W_LW;
    }
    // set any multiple viewports if needed for any GLRandom subtests
    CHECK_RC(SetMultipleViewports());

    return OK;
}

//------------------------------------------------------------------------------
void RndFragment::Pick()
{

    // Pick random alpha test state:
    m_Fragment.AlphaTestFunc  = m_Pickers[RND_FRAG_ALPHA_FUNC].Pick();
    m_Fragment.AlphaTestRef   = m_pFpCtx->Rand.GetRandomFloat(0.0, 1.0);

    // Pick per-color-buffer state:
    for (UINT32 mrtIdx = 0; mrtIdx < m_pGLRandom->NumColorSurfaces(); mrtIdx++)
    {
        if (mrtIdx > 0)
        {
            // Copy color-buffer 0's picks as reasonable defaults for platforms
            // where the draw_buffersN extensions are missing.
            memcpy(m_CBufPicks + mrtIdx, m_CBufPicks + 0, sizeof(CBufPicks));
        }

        //always pick then conditionally use.
        GLboolean blendEnable = m_Pickers[RND_FRAG_BLEND_ENABLE].Pick();
        UINT32 argbMask = 0xf & m_Pickers[RND_FRAG_COLOR_MASK].Pick();
        if (mrtIdx == 0 || m_pGLRandom->HasExt(GLRandomTest::ExtEXT_draw_buffers2))
        {
            m_CBufPicks[mrtIdx].BlendEnabled = blendEnable;
            m_CBufPicks[mrtIdx].ARGBMask = argbMask;
        }

        // No blending or alpha testing with floating point on certain chips
        ColorUtils::Format surfCF = mglTexFmt2Color(
                m_pGLRandom->ColorSurfaceFormat(mrtIdx));

        if (ColorUtils::IsFloat(surfCF))
        {
            UINT32 floatBits = ColorUtils::PixelBits(surfCF) /
                    ColorUtils::PixelElements(surfCF);

            if (int(floatBits) > m_MaxBlendableFloatSize)
            {
                m_Fragment.AlphaTestFunc = GL_FALSE;
                if (m_pGLRandom->HasExt(GLRandomTest::ExtEXT_draw_buffers2))
                {
                    m_CBufPicks[mrtIdx].BlendEnabled = GL_FALSE;
                }
                else
                {
                    m_CBufPicks[0].BlendEnabled = GL_FALSE;
                }
            }
        }

        if (mrtIdx == 0 || m_pGLRandom->HasExt(GLRandomTest::ExtARB_draw_buffers_blend))
        {
            // LW4x and later support EXT_blend_equation_separate.
            // Mods has dropped support for pre-LW40 parts.
            MASSERT(m_pGLRandom->HasExt(GLRandomTest::ExtEXT_blend_equation_separate));
            MASSERT(m_pGLRandom->HasExt(GLRandomTest::ExtEXT_blend_func_separate));

            m_CBufPicks[mrtIdx].BlendSourceFunc[0] =
                    m_Pickers[RND_FRAG_BLEND_FUNC].Pick();
            m_CBufPicks[mrtIdx].BlendSourceFunc[1] =
                    m_Pickers[RND_FRAG_BLEND_FUNC].Pick();

            m_CBufPicks[mrtIdx].BlendDestFunc[0] =
                    m_Pickers[RND_FRAG_BLEND_FUNC].Pick();
            m_CBufPicks[mrtIdx].BlendDestFunc[1] =
                    m_Pickers[RND_FRAG_BLEND_FUNC].Pick();

            m_CBufPicks[mrtIdx].BlendEquation[0] =
                    m_Pickers[RND_FRAG_BLEND_EQN].Pick();
            m_CBufPicks[mrtIdx].BlendEquation[1] =
                    m_Pickers[RND_FRAG_BLEND_EQN].Pick();
        }

        PickAdvancedBlending(mrtIdx);
    }

    m_Fragment.BlendColor[0] = m_Pickers[RND_FRAG_BLEND_COLOR].FPick();
    m_Fragment.BlendColor[1] = m_Pickers[RND_FRAG_BLEND_COLOR].FPick();
    m_Fragment.BlendColor[2] = m_Pickers[RND_FRAG_BLEND_COLOR].FPick();
    m_Fragment.BlendColor[3] = m_Pickers[RND_FRAG_BLEND_ALPHA].FPick();

    m_Fragment.DitherEnabled = m_Pickers[RND_FRAG_DITHER_ENABLE].Pick();

    // Pick a random logic op:
    m_Fragment.LogicOp         = m_Pickers[RND_FRAG_LOGIC_OP].Pick();

    // Pick random depth test function:
    m_Fragment.DepthFunc       = m_Pickers[RND_FRAG_DEPTH_FUNC].Pick();
    m_Fragment.DepthMask       = m_Pickers[RND_FRAG_DEPTH_MASK].Pick();
    m_Fragment.DepthClamp      = m_Pickers[RND_FRAG_DEPTH_CLAMP].Pick();
    m_Fragment.DepthBoundsTest = m_Pickers[RND_FRAG_DEPTH_BOUNDS_TEST].Pick();

    // Get depth-bounds min between 0 and 1, but hack the probability to force
    // frequent use of the two bounds conditions, especially 0.0.
    m_Fragment.DepthBounds[0]  = m_pFpCtx->Rand.GetRandomFloat(0.0, 2.0) - 0.75F;
    if (m_Fragment.DepthBounds[0] < 0.0)
        m_Fragment.DepthBounds[0] = 0.0;
    else if (m_Fragment.DepthBounds[0] > 1.0)
        m_Fragment.DepthBounds[0] = 1.0;

    // Get depth-bounds max between min and 1, but hack the probability to force
    // frequent use of the two boundary cases, especially 1.0.
    m_Fragment.DepthBounds[1]  = m_pFpCtx->Rand.GetRandomFloat(0.0, 1.5)
                                 + m_Fragment.DepthBounds[0] - 0.25F;
    if (m_Fragment.DepthBounds[1] < m_Fragment.DepthBounds[0])
        m_Fragment.DepthBounds[1] = m_Fragment.DepthBounds[0];
    else if (m_Fragment.DepthBounds[1] > 1.0)
        m_Fragment.DepthBounds[1] = 1.0;

    // Pick stencil stuff:
    m_Fragment.Stencil[0].Func     = m_Pickers[RND_FRAG_STENCIL_FUNC].Pick();
    m_Fragment.Stencil[0].OpFail   = m_Pickers[RND_FRAG_STENCIL_OP].Pick();
    m_Fragment.Stencil[0].OpZFail  = m_Pickers[RND_FRAG_STENCIL_OP].Pick();
    m_Fragment.Stencil[0].OpZPass  = m_Pickers[RND_FRAG_STENCIL_OP].Pick();
    m_Fragment.Stencil[0].WriteMask = m_Pickers[RND_FRAG_STENCIL_WRITE_MASK].Pick();
    m_Fragment.Stencil[0].ReadMask = m_Pickers[RND_FRAG_STENCIL_READ_MASK].Pick();
    m_Fragment.Stencil[0].Ref      = m_Pickers[RND_FRAG_STENCIL_REF].Pick();

    // always pick then conditionally use to keep the picks consistent.
    m_Fragment.Stencil2Side        = m_Pickers[RND_FRAG_STENCIL_2SIDE].Pick();
    m_Fragment.Stencil[1].Func     = m_Pickers[RND_FRAG_STENCIL_FUNC].Pick();
    m_Fragment.Stencil[1].OpFail   = m_Pickers[RND_FRAG_STENCIL_OP].Pick();
    m_Fragment.Stencil[1].OpZFail  = m_Pickers[RND_FRAG_STENCIL_OP].Pick();
    m_Fragment.Stencil[1].OpZPass  = m_Pickers[RND_FRAG_STENCIL_OP].Pick();
    m_Fragment.Stencil[1].WriteMask = m_Pickers[RND_FRAG_STENCIL_WRITE_MASK].Pick();
    m_Fragment.Stencil[1].ReadMask = m_Pickers[RND_FRAG_STENCIL_READ_MASK].Pick();
    m_Fragment.Stencil[1].Ref      = m_Pickers[RND_FRAG_STENCIL_REF].Pick();

    if (!m_pGLRandom->HasExt(GLRandomTest::ExtEXT_stencil_two_side))
    {   // override with front facing values.
        m_Fragment.Stencil2Side        = GL_FALSE;

        m_Fragment.Stencil[1].Func     = m_Fragment.Stencil[0].Func;
        m_Fragment.Stencil[1].OpFail   = m_Fragment.Stencil[0].OpFail;
        m_Fragment.Stencil[1].OpZFail  = m_Fragment.Stencil[0].OpZFail;
        m_Fragment.Stencil[1].OpZPass  = m_Fragment.Stencil[0].OpZPass;
        m_Fragment.Stencil[1].WriteMask = 0xffffffff;
        m_Fragment.Stencil[1].ReadMask = 0xffffffff;
        m_Fragment.Stencil[1].Ref      = 0;
    }

    // Special multisample features:
    m_Fragment.MultisampleEnabled     = m_Pickers[RND_FRAG_MULTISAMPLE_ENABLE].Pick();
    m_Fragment.AlphaToCoverageEnabled = m_Pickers[RND_FRAG_ALPHA_TO_COV_ENABLE].Pick();
    m_Fragment.AlphaToOneEnabled      = m_Pickers[RND_FRAG_ALPHA_TO_ONE_ENABLE].Pick();
    m_Fragment.SampleCoverageEnabled  = m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ENABLE].Pick();
    m_Fragment.SampleCoverage         = m_Pickers[RND_FRAG_SAMPLE_COVERAGE].FPick();
    m_Fragment.SampleCoverageIlwert   = m_Pickers[RND_FRAG_SAMPLE_COVERAGE_ILWERT].Pick();

    // Pick coarse shading. Note we pick here because the fragment shaders need to correlate
    // the proper end_strategy when coarse shading is enabled.
    // Shading Rate Image allows each texel in a special shading rate texture to specify the 
    // shading rate for a portion of the framebuffer. 
    m_Fragment.ShadingRateControlEnable = m_Pickers[RND_FRAG_SHADING_RATE_CONTROL_ENABLE].Pick();
    m_Fragment.ShadingRateImageEnable = m_Pickers[RND_FRAG_SHADING_RATE_IMAGE_ENABLE].Pick();
    if (m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_shading_rate_image))
    {
        // The coarse EndStrategy requires ShadingRateImage. However ShadingRateImage does not 
        // technically require the coarse EndStrategy.
        if (m_pGLRandom->m_GpuPrograms.FrProgEnabled() &&
            m_pGLRandom->m_GpuPrograms.EndStrategy() == RND_GPU_PROG_FR_END_STRATEGY_coarse)
        {
            m_Fragment.ShadingRateControlEnable = true;
        }
    }
    else
    {
        m_Fragment.ShadingRateControlEnable = false;
        m_Fragment.ShadingRateImageEnable = false;
    }

    // Scissor:
    m_Fragment.Scissor = m_Pickers[RND_FRAG_SCISSOR_ENABLE].Pick();

    // Viewport:
    m_Fragment.ViewportOffset[0] = m_Pickers[RND_FRAG_VIEWPORT_OFFSET_X].Pick();
    m_Fragment.ViewportOffset[1] = m_Pickers[RND_FRAG_VIEWPORT_OFFSET_Y].Pick();

    m_Fragment.RasterDiscard = (0 != m_Pickers[RND_FRAG_RASTER_DISCARD].Pick());

    // Fixup the picks to be self-consistent, and try to increase the
    // odds of something visible showing up on the screen.

    switch ( m_Fragment.AlphaTestFunc )
    {
        case GL_FALSE:    //  (disable alpha test)
        case GL_NEVER:
        case GL_ALWAYS:
        case GL_EQUAL:
        case GL_NOTEQUAL:
            break;

        case GL_LESS:
        case GL_LEQUAL:
            if (m_Fragment.AlphaTestRef < 0.5)
            {
                m_Fragment.AlphaTestRef *= 2.0;
            }
            break;

        case GL_GEQUAL:
        case GL_GREATER:
            if ( m_Fragment.AlphaTestRef > 0.5 )
                m_Fragment.AlphaTestRef *= 0.5;
            break;

        default:    MASSERT(0); // shouldn't get here.
    }

    for (UINT32 mrtIdx = 0; mrtIdx < m_pGLRandom->NumColorSurfaces(); mrtIdx++)
    {
        for (int i = 0; i < 2; i++)  // for both alpha and color blend functions
        {
            // SRC_ALPHA_SATURATE is not a legal destination blend
            if (m_CBufPicks[mrtIdx].BlendDestFunc[i] == GL_SRC_ALPHA_SATURATE)
            {
                m_CBufPicks[mrtIdx].BlendDestFunc[i] = GL_SRC_ALPHA;
            }
        }
    }

    // Depth
    if (! m_pGLRandom->HasExt(GLRandomTest::ExtLW_depth_clamp))
        m_Fragment.DepthClamp = GL_FALSE;
    if (! m_pGLRandom->HasExt(GLRandomTest::ExtEXT_depth_bounds_test))
        m_Fragment.DepthBoundsTest = GL_FALSE;
    if (!m_Fragment.DepthBoundsTest)
    {
        m_Fragment.DepthBounds[0] = 0.0;
        m_Fragment.DepthBounds[1] = 1.0;
    }

    // Stencil

    if (GL_FALSE == m_Fragment.Stencil[0].Func)
        m_Fragment.StencilEnabled = GL_FALSE;
    else
        m_Fragment.StencilEnabled = GL_TRUE;

    if (! m_pGLRandom->HasExt(GLRandomTest::ExtEXT_stencil_two_side))
        m_Fragment.Stencil2Side = GL_FALSE;

    if (!m_pGLRandom->HasExt(GLRandomTest::ExtARB_multisample))
    {
        m_Fragment.MultisampleEnabled     = GL_TRUE;
        m_Fragment.AlphaToCoverageEnabled = GL_FALSE;
        m_Fragment.AlphaToOneEnabled      = GL_FALSE;
        m_Fragment.SampleCoverageEnabled  = GL_FALSE;
        m_Fragment.SampleCoverage         = 0.0f;
        m_Fragment.SampleCoverageIlwert   = GL_FALSE;
    }

    for (int i = 0; i < 1 + (GL_FALSE != m_Fragment.Stencil2Side); i++)
    {
        switch ( m_Fragment.Stencil[i].Func )
        {
            case GL_FALSE:
            m_Fragment.Stencil[i].Func = GL_ALWAYS;
            // fall through

            case GL_ALWAYS:
            case GL_NEVER:
            case GL_LEQUAL:
            case GL_LESS:
            case GL_GREATER:
            case GL_GEQUAL:
            case GL_EQUAL:
            case GL_NOTEQUAL:
                break;

            default:  MASSERT(0);   // shouldn't get here
        }
    }

    // Viewport:
    if (m_Fragment.ViewportOffset[0] >= m_WindowRect[2]/2)
        m_Fragment.ViewportOffset[0] = m_WindowRect[2]/2;

    if (m_Fragment.ViewportOffset[1] >= m_WindowRect[3]/2)
        m_Fragment.ViewportOffset[1] = m_WindowRect[3]/2;

    // Conservative raster
    m_EnableConservativeRaster = m_Pickers[RND_FRAG_CONS_RASTER_ENABLE].Pick() &&
                                 m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_conservative_raster);
    m_DilateValueNormalized = m_Pickers[RND_FRAG_CONS_RASTER_DILATE_VALUE].FPick();
    m_RasterMode = m_Pickers[RND_FRAG_CONS_RASTER_SNAP_MODE].Pick();
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_conservative_raster_pre_snap_triangles) &&
        m_RasterMode == GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_TRIANGLES_LW)
    {
        m_RasterMode = GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_LW;
    }

    m_SubpixelPrecisionBiasX = m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_X].FPick();
    m_SubpixelPrecisionBiasY = m_Pickers[RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_Y].FPick();

    m_pGLRandom->LogData(RND_FRAG, &m_Fragment, sizeof(m_Fragment));
    m_pGLRandom->LogData(RND_FRAG, m_ShadingRates.data(), m_ShadingRates.size());
    m_pGLRandom->LogData(RND_FRAG, m_CBufPicks,
            sizeof(CBufPicks) * m_pGLRandom->NumColorSurfaces());
    m_pGLRandom->LogData(RND_FRAG, &m_VportXIntersect, sizeof(m_VportXIntersect));
    m_pGLRandom->LogData(RND_FRAG, &m_VportYIntersect, sizeof(m_VportYIntersect));
    m_pGLRandom->LogData(RND_FRAG, &m_VportsSeparation, sizeof(m_VportsSeparation));

    m_pGLRandom->LogData(RND_FRAG, &m_VportSwizzleX, sizeof(m_VportSwizzleX));
    m_pGLRandom->LogData(RND_FRAG, &m_VportSwizzleY, sizeof(m_VportSwizzleY));
    m_pGLRandom->LogData(RND_FRAG, &m_VportSwizzleZ, sizeof(m_VportSwizzleZ));
    m_pGLRandom->LogData(RND_FRAG, &m_VportSwizzleW, sizeof(m_VportSwizzleW));

    m_pGLRandom->LogData(RND_FRAG, &m_EnableConservativeRaster, sizeof(m_EnableConservativeRaster));
    m_pGLRandom->LogData(RND_FRAG, &m_EnableConservativeRasterDilate, sizeof(m_EnableConservativeRasterDilate));
    m_pGLRandom->LogData(RND_FRAG, &m_EnableConservativeRasterMode, sizeof(m_EnableConservativeRasterMode));
    m_pGLRandom->LogData(RND_FRAG, &m_DilateValueNormalized, sizeof(m_DilateValueNormalized));
    m_pGLRandom->LogData(RND_FRAG, &m_RasterMode, sizeof(m_RasterMode));
    m_pGLRandom->LogData(RND_FRAG, &m_SubpixelPrecisionBiasX, sizeof(m_SubpixelPrecisionBiasX));
    m_pGLRandom->LogData(RND_FRAG, &m_SubpixelPrecisionBiasY, sizeof(m_SubpixelPrecisionBiasY));
}

//------------------------------------------------------------------------------
void RndFragment::Print(Tee::Priority pri)
{
    const char * TF[2] = { "disabled", "enabled" };
    // Skip when drawing pixels, allow 3d pixels/vertexs to print.
    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_pGLRandom->m_Misc.DrawAction())
        return;

    Printf(pri, "depthTest: %s%s%s [%.1f,%.1f]  alphaTest: %s %f %s viewport: %dx%d\n",
         StrAlphaTestFunc(m_Fragment.DepthFunc),
         m_Fragment.DepthMask ? "" : ",nowrite",
         m_Fragment.DepthClamp ? ",clamp" : "",
         m_Fragment.DepthBounds[0],
         m_Fragment.DepthBounds[1],
         StrAlphaTestFunc(m_Fragment.AlphaTestFunc), m_Fragment.AlphaTestRef,
         m_Fragment.Scissor ? " scissor" : "",
         m_WindowRect[2]-m_Fragment.ViewportOffset[0],
         m_WindowRect[3]-m_Fragment.ViewportOffset[1]);

    if (m_Fragment.StencilEnabled)
    {
        const char * sideNames[] = { "Front", "Back" };
        int side;
        for (side = 0; side < (1 + m_Fragment.Stencil2Side); side++)
        {
            Printf(pri, "Stencil %s: %s %s,%s,%s rmask,ref %x,%x wmask %x\n",
               sideNames[side],
               StrAlphaTestFunc(m_Fragment.Stencil[side].Func),
               StrStencilOp(m_Fragment.Stencil[side].OpFail),
               StrStencilOp(m_Fragment.Stencil[side].OpZFail),
               StrStencilOp(m_Fragment.Stencil[side].OpZPass),
               m_Fragment.Stencil[side].ReadMask,
               m_Fragment.Stencil[side].Ref,
               m_Fragment.Stencil[side].WriteMask
               );
        }
    }
    else
    {
        Printf(pri, "Stencil: DISABLED\n");
    }

    for (UINT32 mrtIdx = 0; mrtIdx < m_pGLRandom->NumColorSurfaces(); mrtIdx++)
    {
        Printf(pri, "C%d Blend: ", mrtIdx);

        if (m_CBufPicks[mrtIdx].BlendEnabled)
        {
            Printf(pri, "rgb:%s,%s,%s a:%s,%s,%s",
                     StrBlendEquation(m_CBufPicks[mrtIdx].BlendEquation[0]),
                     StrBlendFunction(m_CBufPicks[mrtIdx].BlendSourceFunc[0]),
                     StrBlendFunction(m_CBufPicks[mrtIdx].BlendDestFunc[0]),
                     StrBlendEquation(m_CBufPicks[mrtIdx].BlendEquation[1]),
                     StrBlendFunction(m_CBufPicks[mrtIdx].BlendSourceFunc[1]),
                     StrBlendFunction(m_CBufPicks[mrtIdx].BlendDestFunc[1]));
        }
        else
        {
                Printf(pri, "DISABLED");
        }

        Printf(pri, "  ColorMask: %x\n",
                  m_CBufPicks[mrtIdx].ARGBMask);

        Printf(pri, "C%d AdvBlend: ", mrtIdx);
        if (m_CBufPicks[mrtIdx].AdvBlendEnabled)
        {
            Printf(pri,"premultsrc:%s overlap:%s eq:%s\n",
                mglEnumToString(m_CBufPicks[mrtIdx].AdvBlendPremultipliedSrc),
                mglEnumToString(m_CBufPicks[mrtIdx].AdvBlendOverlapMode),
                mglEnumToString(m_CBufPicks[mrtIdx].AdvBlendEquation));
        }
        else
        {
                Printf(pri, "DISABLED\n");
        }
    }
    Printf(pri, "LogicOp: %s\n   Dither: %s%s\n",
          StrLogicOp(m_Fragment.LogicOp),
          m_Fragment.DitherEnabled ? "on" : "off",
          m_Fragment.RasterDiscard ? "  GL_RASTERIZER_DISCARD_LW" : "");

    if (m_pGLRandom->GetNumLayersInFBO() > 0)
    {
        Printf(pri, "  VportXIntersect: %f\n", m_VportXIntersect);
        Printf(pri, "  VportYIntersect: %f\n", m_VportYIntersect);
        Printf(pri, "  VportsSeparation: %f\n", m_VportsSeparation);
    }

    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2))
    {
        Printf(pri, "  VportSwizzleX:%s VportSwizzleY:%s VportSwizzleZ:%s VportSwizzleW:%s\n",
           mglEnumToString(m_VportSwizzleX),
           mglEnumToString(m_VportSwizzleY),
           mglEnumToString(m_VportSwizzleZ),
           mglEnumToString(m_VportSwizzleW));
    }
    Printf(pri, "Multisample:%s AlphaToCoverage:%s AlphaToOne:%s SampleCoverage:%s %f SampleCoverageIlwert:%s\n",
        TF[m_Fragment.MultisampleEnabled],
        TF[m_Fragment.AlphaToCoverageEnabled],
        TF[m_Fragment.AlphaToOneEnabled],
        TF[m_Fragment.SampleCoverageEnabled],
        m_Fragment.SampleCoverage,
        mglEnumToString(m_Fragment.SampleCoverageIlwert));

    //TODO: implement underestimate
    Printf(pri, "Conservative Raster:%s dilate:%1.5f snap mode:%s subpixel_biasX:%1.5f subpixel_biasY:%1.5f\n", //$
           TF[m_EnableConservativeRaster],
           m_DilateValueNormalized,
           mglEnumToString(m_RasterMode),
           m_SubpixelPrecisionBiasX,
           m_SubpixelPrecisionBiasY);
    Printf(pri, "CoarseShading:%s CoarseTexture:%s\n", 
           TF[m_Fragment.ShadingRateControlEnable],
           TF[m_Fragment.ShadingRateImageEnable]);
}

//------------------------------------------------------------------------------
#if !(defined(GL_EXT_draw_buffers2) && defined(GL_ARB_draw_buffers_blend))
#error RndFragment::Send assumes draw_buffersN
// Of course, we still must check at runtime for HW that supports these
// extensions, but the code is sure cleaner w/o the compile-time checks.
#endif

RC RndFragment::Send()
{
    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_pGLRandom->m_Misc.DrawAction())
    {
        return OK;    // We're drawing pixels on this loop.

        // @@@ Need to figure out what blend/etc operations are HW accellerated
        // for glDrawPixels to get better test coverage here.
    }
    // We need to set this each time in case we set the Golden.SkipCount = 1.
    SetMultipleViewports();

    if ( m_Fragment.AlphaTestFunc != GL_FALSE )
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(m_Fragment.AlphaTestFunc, m_Fragment.AlphaTestRef);
    }

    // Update blend color if any MRT color buffer enabled blending.
    for (UINT32 mrtIdx = 0; mrtIdx < m_pGLRandom->NumColorSurfaces(); mrtIdx++)
    {
        if (m_CBufPicks[mrtIdx].BlendEnabled)
        {
            glBlendColor(m_Fragment.BlendColor[0],
                    m_Fragment.BlendColor[1],
                    m_Fragment.BlendColor[2],
                    m_Fragment.BlendColor[3]);
            break;
        }
    }

    if (m_pGLRandom->HasExt(GLRandomTest::ExtEXT_draw_buffers2))
    {
        // Per-color-buffer (MRT) blend enables and color masks.
        for (UINT32 mrtIdx = 0; mrtIdx < m_pGLRandom->NumColorSurfaces(); mrtIdx++)
        {
            if (m_CBufPicks[mrtIdx].BlendEnabled)
            {
                glEnableIndexedEXT(GL_BLEND, mrtIdx);
            }
            glColorMaskIndexedEXT(mrtIdx,
                    0 != (m_CBufPicks[mrtIdx].ARGBMask & 1),
                    0 != (m_CBufPicks[mrtIdx].ARGBMask & 2),
                    0 != (m_CBufPicks[mrtIdx].ARGBMask & 4),
                    0 != (m_CBufPicks[mrtIdx].ARGBMask & 8));
        }
    }
    else
    {
        // Global blend enable and color mask.
        if (m_CBufPicks[0].BlendEnabled)
        {
            glEnable(GL_BLEND);
        }
        glColorMask(
                0 != (m_CBufPicks[0].ARGBMask & 1),
                0 != (m_CBufPicks[0].ARGBMask & 2),
                0 != (m_CBufPicks[0].ARGBMask & 4),
                0 != (m_CBufPicks[0].ARGBMask & 8));
    }

    if (m_pGLRandom->HasExt(GLRandomTest::ExtARB_draw_buffers_blend))
    {
        // Per-color-buffer (MRT) blend functions and equations.
        for (UINT32 mrtIdx = 0; mrtIdx < m_pGLRandom->NumColorSurfaces(); mrtIdx++)
        {
            if (m_CBufPicks[mrtIdx].BlendEnabled)
            {
                glBlendEquationSeparateiARB(
                        mrtIdx,
                        m_CBufPicks[mrtIdx].BlendEquation[0],
                        m_CBufPicks[mrtIdx].BlendEquation[1]);
                glBlendFuncSeparateiARB(
                        mrtIdx,
                        m_CBufPicks[mrtIdx].BlendSourceFunc[0],
                        m_CBufPicks[mrtIdx].BlendDestFunc[0],
                        m_CBufPicks[mrtIdx].BlendSourceFunc[1],
                        m_CBufPicks[mrtIdx].BlendDestFunc[1]
                        );
            }
        }
    }
    else
    {
        // Global blend enable and color mask.
        if (m_CBufPicks[0].BlendEnabled)
        {
            glBlendEquationSeparateEXT(
                    m_CBufPicks[0].BlendEquation[0],
                    m_CBufPicks[0].BlendEquation[1]);
            glBlendFuncSeparate(
                    m_CBufPicks[0].BlendSourceFunc[0],
                    m_CBufPicks[0].BlendDestFunc[0],
                    m_CBufPicks[0].BlendSourceFunc[1],
                    m_CBufPicks[0].BlendDestFunc[1]
                    );
        }
    }

    if (m_CBufPicks[0].AdvBlendEnabled)
    {
        glBlendParameteriLW(GL_BLEND_PREMULTIPLIED_SRC_LW,
            m_CBufPicks[0].AdvBlendPremultipliedSrc);
        glBlendParameteriLW(GL_BLEND_OVERLAP_LW,
            m_CBufPicks[0].AdvBlendOverlapMode);
        glBlendEquation(m_CBufPicks[0].AdvBlendEquation);
    }

    if (!m_Fragment.DitherEnabled)
    {
        glDisable(GL_DITHER);
    }

    if ( m_Fragment.LogicOp != GL_FALSE )
    {
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(m_Fragment.LogicOp);
    }

    if ( m_Fragment.DepthFunc != GL_FALSE )
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(m_Fragment.DepthFunc);
        glDepthMask(m_Fragment.DepthMask);
#if defined(GL_LW_depth_clamp)
        if (m_Fragment.DepthClamp)
            glEnable(GL_DEPTH_CLAMP_LW);
#endif
#if defined(GL_EXT_depth_bounds_test)
        if (m_Fragment.DepthBoundsTest)
        {
            glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
            glDepthBoundsEXT(m_Fragment.DepthBounds[0],m_Fragment.DepthBounds[1]);
        }
#endif
    }

    if (m_Fragment.StencilEnabled)
    {
        glEnable(GL_STENCIL_TEST);

#if defined(GL_EXT_stencil_two_side)
        if (m_Fragment.Stencil2Side)
        {
            glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
            glActiveStencilFaceEXT(GL_BACK);
            glStencilFunc(m_Fragment.Stencil[1].Func, m_Fragment.Stencil[1].Ref, m_Fragment.Stencil[1].ReadMask);
            glStencilOp(m_Fragment.Stencil[1].OpFail, m_Fragment.Stencil[1].OpZFail, m_Fragment.Stencil[1].OpZPass);
            glStencilMask(m_Fragment.Stencil[1].WriteMask);
            glActiveStencilFaceEXT(GL_FRONT);
        }
#endif
        glStencilFunc(m_Fragment.Stencil[0].Func, m_Fragment.Stencil[0].Ref, m_Fragment.Stencil[0].ReadMask);
        glStencilOp(m_Fragment.Stencil[0].OpFail, m_Fragment.Stencil[0].OpZFail, m_Fragment.Stencil[0].OpZPass);
        glStencilMask(m_Fragment.Stencil[0].WriteMask);
    }

    if (!m_Fragment.MultisampleEnabled)
    {
        glDisable(GL_MULTISAMPLE);
    }

    if (m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_shading_rate_image))
    {
        if (m_Fragment.ShadingRateControlEnable)
        {
            glEnable(GL_SHADING_RATE_IMAGE_LW);
            glBindShadingRateImageLW(m_Fragment.ShadingRateImageEnable ? m_SRTex : 0);
            // The pixel format for the shading image is 8bits (although only 4 are technically
            // required). We need to make sure this GL state is set so that we dont try to access
            // uninitialized data at the bottom of the texure (which would translate to corruption
            // at the top of the screen).
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        else
        {
            glDisable(GL_SHADING_RATE_IMAGE_LW);
            glBindShadingRateImageLW(0);
        }
    }

    if (m_Fragment.AlphaToCoverageEnabled)
    {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    if (m_Fragment.AlphaToOneEnabled)
    {
        glEnable(GL_SAMPLE_ALPHA_TO_ONE);
    }
    if (m_Fragment.SampleCoverageEnabled)
    {
        glEnable(GL_SAMPLE_COVERAGE);
        glSampleCoverage(m_Fragment.SampleCoverage, m_Fragment.SampleCoverageIlwert);
    }

    //perturb viewport only in case of non-layered rendering
    if (m_pGLRandom->GetNumLayersInFBO() == 0)
    {
        if (m_Fragment.ViewportOffset[0] || m_Fragment.ViewportOffset[1])
        {
            glViewport(
                    m_WindowRect[0] + m_Fragment.ViewportOffset[0],
                    m_WindowRect[1] + m_Fragment.ViewportOffset[1],
                    m_WindowRect[2] - m_Fragment.ViewportOffset[0],
                    m_WindowRect[3] - m_Fragment.ViewportOffset[1]
                    );
        }

        if (m_ScissorOverrideXY[0] >= 0)
        {
            glScissor(
                    m_ScissorOverrideXY[0],
                    m_ScissorOverrideXY[1],
                    m_ScissorOverrideWH[0],
                    m_ScissorOverrideWH[1]
                    );
            glEnable(GL_SCISSOR_TEST);
        }
        else if ( m_Fragment.Scissor )
        {
            glScissor(
                    m_WindowRect[0] + 6  + m_Fragment.ViewportOffset[0],
                    m_WindowRect[1] + 6  + m_Fragment.ViewportOffset[1],
                    m_WindowRect[2] - 12 - m_Fragment.ViewportOffset[0],
                    m_WindowRect[3] - 12 - m_Fragment.ViewportOffset[1]
                    );
            glEnable(GL_SCISSOR_TEST);
        }

    }

    if (m_Fragment.RasterDiscard)
    {
        glEnable(GL_RASTERIZER_DISCARD_LW);
    }

    if (m_EnableConservativeRaster)
    {
        glEnable(GL_CONSERVATIVE_RASTERIZATION_LW);
        if (m_EnableConservativeRasterDilate)
        {
            float minMaxDilateValues[2] = {0, 0};
            float dilateGranularity = 0;
            glGetFloatv(GL_CONSERVATIVE_RASTER_DILATE_RANGE_LW, minMaxDilateValues);
            glGetFloatv(GL_CONSERVATIVE_RASTER_DILATE_GRANULARITY_LW, &dilateGranularity);
            float dilate = minMaxDilateValues[0] + dilateGranularity *
                static_cast<UINT32>(m_DilateValueNormalized *
                (minMaxDilateValues[1] - minMaxDilateValues[0]) / dilateGranularity);
            glConservativeRasterParameterfLW(GL_CONSERVATIVE_RASTER_DILATE_LW, dilate);
        }

        if (m_EnableConservativeRasterMode)
        {
            glConservativeRasterParameteriLW(GL_CONSERVATIVE_RASTER_MODE_LW, m_RasterMode);
        }

        GLint maxBias = 0;
        glGetIntegerv(GL_MAX_SUBPIXEL_PRECISION_BIAS_BITS_LW, &maxBias);
        UINT32 subPixelPrecisionBiasX = static_cast<UINT32>(m_SubpixelPrecisionBiasX * maxBias);
        UINT32 subPixelPrecisionBiasY = static_cast<UINT32>(m_SubpixelPrecisionBiasY * maxBias);
        glSubpixelPrecisionBiasLW(subPixelPrecisionBiasX, subPixelPrecisionBiasY);
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndFragment::CleanupGLState()
{
    return OK;
}

//------------------------------------------------------------------------------
RC RndFragment::Cleanup()
{
    if (m_CBufPicks)
        delete [] m_CBufPicks;
    m_CBufPicks = NULL;

    if (m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_shading_rate_image))
    {
        glDisable(GL_SHADING_RATE_IMAGE_LW);
        glBindShadingRateImageLW(0);
        glDeleteTextures(1, &m_SRTex);
    }

    return OK;
}

//------------------------------------------------------------------------------
const char * RndFragment::StrAlphaTestFunc(GLenum atf) const
{
    return mglEnumToString(atf,"IlwalidAlphaTestFunc",true);
}

//------------------------------------------------------------------------------
const char * RndFragment::StrBlendEquation(GLenum beq) const
{
    return mglEnumToString(beq,"IlwalidBlendEqn",true);
}

//------------------------------------------------------------------------------
const char * RndFragment::StrBlendFunction(GLenum bf) const
{
    return mglEnumToString( bf,          // enum to colwert
                            "IlwalidBlendFunc",// default string if not found
                            true,        // assert on error
                            glezom_Zero, // return GL_ZERO for 0
                            glezom_One); // return GL_ONE for 1
}

//------------------------------------------------------------------------------
const char * RndFragment::StrLogicOp(GLenum lop) const
{
    return mglEnumToString(lop,"IlwalidLogicOp",true);
}

//------------------------------------------------------------------------------
const char * RndFragment::StrStencilOp(GLenum sop) const
{
    return mglEnumToString( sop, // enum to colwert
                            "IlwalidStencilOp", // default string if not found
                            true,        // assert on error
                            glezom_Zero, // return GL_ZERO for 0
                            glezom_One); // return GL_ONE for 1
}

//------------------------------------------------------------------------------
bool RndFragment::IsStencilEnabled()
{
    // Return false if we are not going to call glEnable(GL_STENCIL_TEST)
    // during the Send() operation, true otherwise.
    // Note: This rule will change once we have this object unconditionally
    //       send its data to the HW.
    return (m_Fragment.StencilEnabled &&
            (RND_TSTCTRL_DRAW_ACTION_pixels != m_pGLRandom->m_Misc.DrawAction()));
}

//------------------------------------------------------------------------------
void RndFragment::SetFullScreelwiewport()
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2))
    {   // restore the swizzle back to defaults
        glViewportSwizzleLW(0
                ,GL_VIEWPORT_SWIZZLE_POSITIVE_X_LW
                ,GL_VIEWPORT_SWIZZLE_POSITIVE_Y_LW
                ,GL_VIEWPORT_SWIZZLE_POSITIVE_Z_LW
                ,GL_VIEWPORT_SWIZZLE_POSITIVE_W_LW);
    }

    // Ignore the effect of multiple viewports we might have configured earlier
    const UINT32 w = m_pGLRandom->GetFboWidth() ? m_pGLRandom->GetFboWidth() :
                                               m_pGLRandom->DispWidth();
    const UINT32 h = m_pGLRandom->GetFboHeight() ? m_pGLRandom->GetFboHeight() :
                                                m_pGLRandom->DispHeight();
    glViewport(0, 0, w, h);
}

//------------------------------------------------------------------------------
float RndFragment::GetVportXIntersect() const
{
    return m_VportXIntersect;
}
//------------------------------------------------------------------------------
float RndFragment::GetVportYIntersect() const
{
    return m_VportYIntersect;
}
//------------------------------------------------------------------------------
float RndFragment::GetVportSeparation() const
{
    return m_VportsSeparation;
}

//------------------------------------------------------------------------------
RC RndFragment::SetMultipleViewports()
{
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtARB_viewport_array) ||
        m_pGLRandom->GetNumLayersInFBO() == 0)
    {
        return OK;
    }

    GLfloat w = static_cast<float>(m_pGLRandom->GetFboWidth());
    GLfloat h = static_cast<float>(m_pGLRandom->GetFboHeight());
    GLfloat x = m_VportXIntersect;
    GLfloat y = m_VportYIntersect;
    GLfloat offset = m_VportsSeparation;

    // Number of viewports supported are hard coded to 4 with layout
    // as shown below. (x,y) and (offset) are chosen randomly per frame.

    //               +--------------+--+-+-+----------------(w,h)
    //               |              |      |                  |
    //               |  viewport 3  |      |   viewport 2     |
    //           -   |+-------------+      +-----------------+|
    //           |   |                                        |
    //   2*offset|   |                 +(x,y)                 |
    //           |   |                                        |
    //           -   +--------------+      +------------------+
    //               |              |      |                  |
    //               | viewport 0   |      |  viewport 1      |
    //              (0,0)-----------+------+------------------+
    //
    //                              |------|
    //                              2*offset

    GLfloat v[] = { 0,        0,        x-offset,   y-offset,
                    x+offset, 0,        w-x-offset, y-offset,
                    x+offset, y+offset, w-x-offset, h-y-offset,
                    0,        y+offset, x-offset,   h-y-offset};

    glViewportArrayv(0, MAX_VIEWPORTS, v);

    //set swizzling for all viewports
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_viewport_array2))
    {
        for (UINT32 i=0; i < MAX_VIEWPORTS; i++)
        {
            glViewportSwizzleLW(i, m_VportSwizzleX, m_VportSwizzleY, m_VportSwizzleZ, m_VportSwizzleW);
        }
    }
    return mglGetRC();
}

//--------------------------------------------------------------------------------------------------

void RndFragment::SetVportSeparation(float separation)
{
    if ((m_VportXIntersect - separation < 0) ||
        (m_VportYIntersect - separation < 0) ||
        (m_VportXIntersect + separation > (static_cast<float>(m_pGLRandom->GetFboWidth()))) ||
        (m_VportYIntersect + separation > (static_cast<float>(m_pGLRandom->GetFboHeight()))))
    {
        // disable separation between viewports, not worth the extra computation
        separation = 0;
    }
    m_VportsSeparation = separation;
}
