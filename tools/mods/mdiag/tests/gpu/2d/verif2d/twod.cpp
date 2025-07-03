/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013,2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "mdiag/tests/stdtest.h"

#include "verif2d.h"

#include "core/include/lwrm.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/smc/smcengine.h"

#include "class/cl902d.h"
#include "ctrl/ctrl0080.h"

// Bug 806564: SetCompression is a perf knob taking the NOOP00 slot
// to avoid class revision. Thus, class/cl902d.h doesn't have its
// definition. Here we explicitly define the method address to that
// of NOOP00 (0x02d4).
#if !defined(LW902D_SET_COMPRESSION)
#define LW902D_SET_COMPRESSION  LW902D_SET_SPARE_NOOP00
#endif

#define ADD_METHOD_ARRAY(n,name,NAME)                 \
    { for (int i = 0; i < n; i++ )                    \
    {                                                 \
        stringstream name_str;                        \
        stringstream ss;                              \
        name_str<<name;                               \
        ss << i;                                      \
        AddMethod( name_str.str()+ "_" + ss.str(), NAME( i ) ); \
    }}

void V2dTwoD::Init( ClassId classId )
{
    if ( ! m_gpu->SupportedClass( classId ) )
        V2D_THROW( "unsupported class: " << HEX(4,classId) );

    // do init common to all render class objects
    V2dClassObj::Init( classId );

    AddMethod( "WaitForIdle",       LW902D_WAIT_FOR_IDLE            );

    AddMethod( "PMTrigger",         LW902D_PM_TRIGGER               );
    AddMethod( "SetSrcOffsetUpper", LW902D_SET_SRC_OFFSET_UPPER     );
    AddMethod( "SetSrcOffsetLower", LW902D_SET_SRC_OFFSET_LOWER     );
    AddMethod( "SetDstOffsetUpper", LW902D_SET_DST_OFFSET_UPPER     );
    AddMethod( "SetDstOffsetLower", LW902D_SET_DST_OFFSET_LOWER     );
    AddMethod( "SetSrcPitch"      , LW902D_SET_SRC_PITCH            );
    AddMethod( "SetDstPitch"      , LW902D_SET_DST_PITCH            );

    map<string,UINT32> * srcMemoryLayout = new map<string,UINT32>;
    (*srcMemoryLayout)[ "PITCH" ] = LW902D_SET_SRC_MEMORY_LAYOUT_V_PITCH;
    (*srcMemoryLayout)[ "BLOCKLINEAR" ] = LW902D_SET_SRC_MEMORY_LAYOUT_V_BLOCKLINEAR;
    AddMethod( "SetSrcMemoryLayout", LW902D_SET_SRC_MEMORY_LAYOUT, srcMemoryLayout );

    AddMethod( "SetSrcBlockSize", LW902D_SET_SRC_BLOCK_SIZE );
    map<string,UINT32> * MemoryLayoutGobs = new map<string,UINT32>;
    (*MemoryLayoutGobs)[ "ONE_GOB"        ] = LW902D_SET_SRC_BLOCK_SIZE_HEIGHT_ONE_GOB       ;
    (*MemoryLayoutGobs)[ "TWO_GOBS"       ] = LW902D_SET_SRC_BLOCK_SIZE_HEIGHT_TWO_GOBS      ;
    (*MemoryLayoutGobs)[ "FOUR_GOBS"      ] = LW902D_SET_SRC_BLOCK_SIZE_HEIGHT_FOUR_GOBS     ;
    (*MemoryLayoutGobs)[ "EIGHT_GOBS"     ] = LW902D_SET_SRC_BLOCK_SIZE_HEIGHT_EIGHT_GOBS    ;
    (*MemoryLayoutGobs)[ "SIXTEEN_GOBS"   ] = LW902D_SET_SRC_BLOCK_SIZE_HEIGHT_SIXTEEN_GOBS  ;
    (*MemoryLayoutGobs)[ "THIRTYTWO_GOBS" ] = LW902D_SET_SRC_BLOCK_SIZE_HEIGHT_THIRTYTWO_GOBS;
    AddField( "SetSrcBlockSize_Height", LW902D_SET_SRC_BLOCK_SIZE, FIELD_RANGE( LW902D_SET_SRC_BLOCK_SIZE_HEIGHT ), MemoryLayoutGobs );
    AddField( "SetSrcBlockSize_Depth",  LW902D_SET_SRC_BLOCK_SIZE, FIELD_RANGE( LW902D_SET_SRC_BLOCK_SIZE_DEPTH ), MemoryLayoutGobs );
    AddMethod( "SetSrcWidth", LW902D_SET_SRC_WIDTH );
    AddMethod( "SetSrcHeight", LW902D_SET_SRC_HEIGHT );
    AddMethod( "SetSrcDepth", LW902D_SET_SRC_DEPTH );

    map<string,UINT32> * dstMemoryLayout = new map<string,UINT32>;
    (*dstMemoryLayout)[ "PITCH" ] = LW902D_SET_DST_MEMORY_LAYOUT_V_PITCH;
    (*dstMemoryLayout)[ "BLOCKLINEAR" ] = LW902D_SET_DST_MEMORY_LAYOUT_V_BLOCKLINEAR;
    AddMethod( "SetDstMemoryLayout", LW902D_SET_DST_MEMORY_LAYOUT,dstMemoryLayout );

    AddMethod( "SetDstBlockSize", LW902D_SET_DST_BLOCK_SIZE );
    AddField( "SetDstBlockSize_Height", LW902D_SET_DST_BLOCK_SIZE, FIELD_RANGE( LW902D_SET_DST_BLOCK_SIZE_HEIGHT ), MemoryLayoutGobs );
    AddField( "SetDstBlockSize_Depth",  LW902D_SET_DST_BLOCK_SIZE, FIELD_RANGE( LW902D_SET_DST_BLOCK_SIZE_DEPTH ), MemoryLayoutGobs );
    AddMethod( "SetDstWidth", LW902D_SET_DST_WIDTH );
    AddMethod( "SetDstHeight", LW902D_SET_DST_HEIGHT );
    AddMethod( "SetDstDepth", LW902D_SET_DST_DEPTH );
    AddMethod( "SetDstLayer", LW902D_SET_DST_LAYER );

    AddMethod( "SetClipX0"      , LW902D_SET_CLIP_X0     );
    AddMethod( "SetClipY0"      , LW902D_SET_CLIP_Y0     );
    AddMethod( "SetClipWidth"   , LW902D_SET_CLIP_WIDTH  );
    AddMethod( "SetClipHeight"  , LW902D_SET_CLIP_HEIGHT );
    AddMethod( "SetClipEnable"  , LW902D_SET_CLIP_ENABLE );

    map<string,UINT32> * colorKeyFormat = new map<string,UINT32>;
    (*colorKeyFormat)[ "A16R5G6B5"   ] = LW902D_SET_COLOR_KEY_FORMAT_V_A16R5G6B5  ;
    (*colorKeyFormat)[ "A1R5G5B5" ] = LW902D_SET_COLOR_KEY_FORMAT_V_A1R5G5B5;
    (*colorKeyFormat)[ "A8R8G8B8"    ] = LW902D_SET_COLOR_KEY_FORMAT_V_A8R8G8B8   ;
    (*colorKeyFormat)[ "A2R10G10B10"   ] = LW902D_SET_COLOR_KEY_FORMAT_V_A2R10G10B10  ;
    (*colorKeyFormat)[ "Y8" ] = LW902D_SET_COLOR_KEY_FORMAT_V_Y8;
    (*colorKeyFormat)[ "Y16"    ] = LW902D_SET_COLOR_KEY_FORMAT_V_Y16   ;
    (*colorKeyFormat)[ "Y32"    ] = LW902D_SET_COLOR_KEY_FORMAT_V_Y32   ;

    AddMethod( "SetColorKeyFormat" , LW902D_SET_COLOR_KEY_FORMAT, colorKeyFormat);

    AddMethod( "SetColorKey"       , LW902D_SET_COLOR_KEY        );
    AddMethod( "SetColorKeyEnable" , LW902D_SET_COLOR_KEY_ENABLE );

    map<string,UINT32> * monochromePatternColorFormat = new map<string,UINT32>;
    (*monochromePatternColorFormat)[ "A8X8R5G6B5" ] = LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8R5G6B5  ;
    (*monochromePatternColorFormat)[ "A1R5G5B5"   ] = LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A1R5G5B5;
    (*monochromePatternColorFormat)[ "A8R8G8B8"   ] = LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8   ;
    (*monochromePatternColorFormat)[ "A8Y8" ] = LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8Y8  ;
    (*monochromePatternColorFormat)[ "A8X8Y16"   ] = LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8Y16;
    (*monochromePatternColorFormat)[ "Y32"   ] = LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32   ;
    (*monochromePatternColorFormat)[ "BYTE_EXPAND"   ] = LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_BYTE_EXPAND;

    AddMethod( "SetMonochromePatternColorFormat" , LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT, monochromePatternColorFormat);

    AddMethod( "SetMonochromePatternColor0" , LW902D_SET_MONOCHROME_PATTERN_COLOR0);
    AddMethod( "SetMonochromePatternColor1" , LW902D_SET_MONOCHROME_PATTERN_COLOR1);

    map<string,UINT32> * monoOpacity = new map<string,UINT32>;
    (*monoOpacity)[ "TRANSPARENT" ] = LW902D_SET_PIXELS_FROM_CPU_MONO_OPACITY_V_TRANSPARENT;
    (*monoOpacity)[ "OPAQUE"      ] = LW902D_SET_PIXELS_FROM_CPU_MONO_OPACITY_V_OPAQUE;
    AddMethod( "SetPixelsFromCpuMonoOpacity" , LW902D_SET_PIXELS_FROM_CPU_MONO_OPACITY, monoOpacity);

    map<string,UINT32> * monochromePatternFormat = new map<string,UINT32>;
    (*monochromePatternFormat)[ "CGA6_M1"   ] = LW902D_SET_MONOCHROME_PATTERN_FORMAT_V_CGA6_M1;
    (*monochromePatternFormat)[ "LE_M1"     ] = LW902D_SET_MONOCHROME_PATTERN_FORMAT_V_LE_M1;
    AddMethod( "SetMonochromePatternFormat" , LW902D_SET_MONOCHROME_PATTERN_FORMAT, monochromePatternFormat);

    AddMethod( "SetMonochromePattern0" , LW902D_SET_MONOCHROME_PATTERN0);
    AddMethod( "SetMonochromePattern1" , LW902D_SET_MONOCHROME_PATTERN1);

    ADD_METHOD_ARRAY( 16, "ColorPatternY8", LW902D_COLOR_PATTERN_Y8  );
    ADD_METHOD_ARRAY( 32, "ColorPatternR5G6B5", LW902D_COLOR_PATTERN_R5G6B5  );
    ADD_METHOD_ARRAY( 32, "ColorPatternX1R5G5B5", LW902D_COLOR_PATTERN_X1R5G5B5  );
    ADD_METHOD_ARRAY( 64, "ColorPatternX8R8G8B8", LW902D_COLOR_PATTERN_X8R8G8B8  );

    map<string,UINT32> * patternSelect = new map<string,UINT32>;
    (*patternSelect)[ "MONOCHROME_8x8" ] = LW902D_SET_PATTERN_SELECT_V_MONOCHROME_8x8 ;
    (*patternSelect)[ "MONOCHROME_64x1"] = LW902D_SET_PATTERN_SELECT_V_MONOCHROME_64x1;
    (*patternSelect)[ "MONOCHROME_1x64"] = LW902D_SET_PATTERN_SELECT_V_MONOCHROME_1x64;
    (*patternSelect)[ "COLOR"    ] = LW902D_SET_PATTERN_SELECT_V_COLOR          ;
    AddMethod( "SetPatternSelect", LW902D_SET_PATTERN_SELECT, patternSelect);

    AddMethod( "SetPatternOffset", LW902D_SET_PATTERN_OFFSET );
    AddField( "SetPatternOffsetX", LW902D_SET_PATTERN_OFFSET, FIELD_RANGE( LW902D_SET_PATTERN_OFFSET_X ) );
    AddField( "SetPatternOffsetY", LW902D_SET_PATTERN_OFFSET, FIELD_RANGE( LW902D_SET_PATTERN_OFFSET_Y ) );

    AddMethod( "SetRop", LW902D_SET_ROP);
    AddMethod( "SetBeta1", LW902D_SET_BETA1);
    AddMethod( "SetBeta4", LW902D_SET_BETA4);

    map<string,UINT32> * operation = new map<string,UINT32>;
    (*operation)[ "SRCCOPY_AND"     ] = LW902D_SET_OPERATION_V_SRCCOPY_AND;
    (*operation)[ "ROP_AND"         ] = LW902D_SET_OPERATION_V_ROP_AND;
    (*operation)[ "BLEND_AND"       ] = LW902D_SET_OPERATION_V_BLEND_AND;
    (*operation)[ "SRCCOPY"         ] = LW902D_SET_OPERATION_V_SRCCOPY;
    (*operation)[ "ROP"             ] = LW902D_SET_OPERATION_V_ROP;
    (*operation)[ "SRCCOPY_PREMULT" ] = LW902D_SET_OPERATION_V_SRCCOPY_PREMULT;
    (*operation)[ "BLEND_PREMULT"   ] = LW902D_SET_OPERATION_V_BLEND_PREMULT;
    AddMethod("SetOperation",LW902D_SET_OPERATION, operation);

    map<string,UINT32> * renderEnableMode = new map<string,UINT32>;
    (*renderEnableMode)[ "TRUE"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_TRUE;
    (*renderEnableMode)[ "true"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_TRUE;
    (*renderEnableMode)[ "FALSE"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_FALSE;
    (*renderEnableMode)[ "false"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_FALSE;
    (*renderEnableMode)[ "CONDITIONAL"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_CONDITIONAL;
    (*renderEnableMode)[ "conditional"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_CONDITIONAL;
    (*renderEnableMode)[ "RENDER_IF_EQUAL"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_EQUAL;
    (*renderEnableMode)[ "render_if_equal"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_EQUAL;
    (*renderEnableMode)[ "RENDER_IF_NOT_EQUAL"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_NOT_EQUAL;
    (*renderEnableMode)[ "render_if_not_equal"   ] = LW902D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_NOT_EQUAL;
    AddMethod( "SetRenderEnableMode" , LW902D_SET_RENDER_ENABLE_C,renderEnableMode);

    AddMethod("SetRenderEnableOffsetUpper",  LW902D_SET_RENDER_ENABLE_A);
    AddMethod("SetRenderEnableOffsetLower",  LW902D_SET_RENDER_ENABLE_B);

    ///following is added by South YANG, for Fermi twod global render enable test
    ///date  Jun 6. 2008
    map<string,UINT32> * renderOverrideMode = new map<string,UINT32>;
    (*renderOverrideMode)[ "ALWAYS"   ] = LW902D_SET_RENDER_ENABLE_OVERRIDE_MODE_ALWAYS_RENDER ;
    (*renderOverrideMode)[ "NEVER"    ] = LW902D_SET_RENDER_ENABLE_OVERRIDE_MODE_NEVER_RENDER ;
    (*renderOverrideMode)[ "USER"     ] = LW902D_SET_RENDER_ENABLE_OVERRIDE_MODE_USE_RENDER_ENABLE ;
    AddMethod( "SetGlobalRenderEnableA",    LW902D_SET_GLOBAL_RENDER_ENABLE_A  );
    AddMethod( "SetGlobalRenderEnableB",    LW902D_SET_GLOBAL_RENDER_ENABLE_B  );
    AddMethod( "SetGlobalRenderEnableC",    LW902D_SET_GLOBAL_RENDER_ENABLE_C, renderEnableMode  );
    AddMethod( "SetRenderEnableA",    LW902D_SET_RENDER_ENABLE_A  );
    AddMethod( "SetRenderEnableB",    LW902D_SET_RENDER_ENABLE_B  );
    AddMethod( "SetRenderEnableC",    LW902D_SET_RENDER_ENABLE_C , renderEnableMode );
    AddMethod( "SetRenderEnableOverride",    LW902D_SET_RENDER_ENABLE_OVERRIDE, renderOverrideMode );
    ///date  Jun 6. 2008   ---- over----

    map<string,UINT32> * renderSolidPrimColorFormat = new map<string,UINT32>;
    (*renderSolidPrimColorFormat)[ "A8R8G8B8"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8   ;
    (*renderSolidPrimColorFormat)[ "A2R10G10B10" ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2R10G10B10;
    (*renderSolidPrimColorFormat)[ "A8B8G8R8"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8B8G8R8   ;
    (*renderSolidPrimColorFormat)[ "A2B10G10R10" ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2B10G10R10;
    (*renderSolidPrimColorFormat)[ "X8R8G8B8"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8R8G8B8   ;
    (*renderSolidPrimColorFormat)[ "X8B8G8R8"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8B8G8R8   ;
    (*renderSolidPrimColorFormat)[ "R5G6B5"      ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_R5G6B5     ;
    (*renderSolidPrimColorFormat)[ "A1R5G5B5"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A1R5G5B5   ;
    (*renderSolidPrimColorFormat)[ "X1R5G5B5"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X1R5G5B5   ;
    (*renderSolidPrimColorFormat)[ "Y8"          ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y8         ;
    (*renderSolidPrimColorFormat)[ "Y16"         ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y16        ;
    (*renderSolidPrimColorFormat)[ "Y32"         ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y32        ;
    (*renderSolidPrimColorFormat)[ "Z1R5G5B5"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z1R5G5B5   ;
    (*renderSolidPrimColorFormat)[ "O1R5G5B5"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O1R5G5B5   ;
    (*renderSolidPrimColorFormat)[ "Z8R8G8B8"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z8R8G8B8   ;
    (*renderSolidPrimColorFormat)[ "O8R8G8B8"    ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O8R8G8B8   ;
    (*renderSolidPrimColorFormat)[ "RF32_GF32_BF32_AF32"  ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32_BF32_AF32;
    (*renderSolidPrimColorFormat)[ "RF16_GF16_BF16_AF16"  ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF16_GF16_BF16_AF16;
    (*renderSolidPrimColorFormat)[ "RF32_GF32"  ] = LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32;
    AddMethod( "SetRenderSolidPrimColorFormat", LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT, renderSolidPrimColorFormat);

    AddMethod(    "RenderSolidPrimColor0",LW902D_SET_RENDER_SOLID_PRIM_COLOR0);
    AddMethod(    "RenderSolidPrimColor1",LW902D_SET_RENDER_SOLID_PRIM_COLOR1);
    AddMethod(    "RenderSolidPrimColor2",LW902D_SET_RENDER_SOLID_PRIM_COLOR2);
    AddMethod(    "RenderSolidPrimColor3",LW902D_SET_RENDER_SOLID_PRIM_COLOR3);
    AddMethod(    "RenderSolidPrimColor",LW902D_SET_RENDER_SOLID_PRIM_COLOR);
    AddMethod( "SetRenderSolidPrimColor",LW902D_SET_RENDER_SOLID_PRIM_COLOR);

    map<string,UINT32> * renderSolidPrimMode = new map<string,UINT32>;
    (*renderSolidPrimMode)[ "POINTS"    ] = LW902D_RENDER_SOLID_PRIM_MODE_V_POINTS      ;
    (*renderSolidPrimMode)[ "LINES"     ] = LW902D_RENDER_SOLID_PRIM_MODE_V_LINES       ;
    (*renderSolidPrimMode)[ "POLYLINE"  ] = LW902D_RENDER_SOLID_PRIM_MODE_V_POLYLINE    ;
    (*renderSolidPrimMode)[ "TRIANGLES" ] = LW902D_RENDER_SOLID_PRIM_MODE_V_TRIANGLES   ;
    (*renderSolidPrimMode)[ "RECTS"     ] = LW902D_RENDER_SOLID_PRIM_MODE_V_RECTS       ;
    AddMethod( "RenderSolidPrimMode", LW902D_RENDER_SOLID_PRIM_MODE,renderSolidPrimMode );

    AddMethod( "RenderSolidPrimPointXY", LW902D_RENDER_SOLID_PRIM_POINT_X_Y );
    AddField( "SolidPrimPointX", LW902D_RENDER_SOLID_PRIM_POINT_X_Y, FIELD_RANGE( LW902D_RENDER_SOLID_PRIM_POINT_X_Y_X ) );
    AddField( "SolidPrimPointY", LW902D_RENDER_SOLID_PRIM_POINT_X_Y, FIELD_RANGE( LW902D_RENDER_SOLID_PRIM_POINT_X_Y_Y ) );

    ADD_METHOD_ARRAY( 64, "x", LW902D_RENDER_SOLID_PRIM_POINT_SET_X  );
    ADD_METHOD_ARRAY( 64, "y", LW902D_RENDER_SOLID_PRIM_POINT_Y  );

    AddMethod( "SetRenderSolidLineTieBreakBits", LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS );
    AddField( "LineTieBreakBitsXmajXincYinc", LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS, FIELD_RANGE( LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS_XMAJ__XINC__YINC ) );
    AddField( "LineTieBreakBitsXmajXdecYinc", LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS, FIELD_RANGE( LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS_XMAJ__XDEC__YINC ) );
    AddField( "LineTieBreakBitsYmajXincYinc", LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS, FIELD_RANGE( LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS_YMAJ__XINC__YINC ) );
    AddField( "LineTieBreakBitsYmajXdecYinc", LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS, FIELD_RANGE( LW902D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS_YMAJ__XDEC__YINC ) );

    AddMethod( "SetPixelsFromCpuDstX0Frac", LW902D_SET_PIXELS_FROM_CPU_DST_X0_FRAC  );
    AddMethod( "SetPixelsFromCpuDstX0Int", LW902D_SET_PIXELS_FROM_CPU_DST_X0_INT   );
    AddMethod( "SetPixelsFromCpuDstY0Frac", LW902D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC  );
    AddMethod( "SetPixelsFromCpuDstY0Int", LW902D_SET_PIXELS_FROM_CPU_DST_Y0_INT       );
    AddMethod( "PixelsFromCpuDstY0Int", LW902D_SET_PIXELS_FROM_CPU_DST_Y0_INT       );
    AddMethod( "SetPixelsFromCpuDxDuFrac", LW902D_SET_PIXELS_FROM_CPU_DX_DU_FRAC   );
    AddMethod( "SetPixelsFromCpuDxDuInt", LW902D_SET_PIXELS_FROM_CPU_DX_DU_INT    );
    AddMethod( "SetPixelsFromCpuDyDvFrac", LW902D_SET_PIXELS_FROM_CPU_DY_DV_FRAC   );
    AddMethod( "SetPixelsFromCpuDyDvInt", LW902D_SET_PIXELS_FROM_CPU_DY_DV_INT    );
    AddMethod( "SetPixelsFromCpuSrcWidth", LW902D_SET_PIXELS_FROM_CPU_SRC_WIDTH    );
    AddMethod( "SetPixelsFromCpuSrcHeight", LW902D_SET_PIXELS_FROM_CPU_SRC_HEIGHT   );
    AddMethod( "FlushIlwalidRopMiniCache", LW902D_FLUSH_AND_ILWALIDATE_ROP_MINI_CACHE );

    map<string,UINT32> * pixelsFromCpuColorFormat = new map<string,UINT32>;

    (*pixelsFromCpuColorFormat)[ "A8R8G8B8"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8;
    (*pixelsFromCpuColorFormat)[ "A2R10G10B10" ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2R10G10B10;
    (*pixelsFromCpuColorFormat)[ "A8B8G8R8"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8B8G8R8;
    (*pixelsFromCpuColorFormat)[ "A2B10G10R10" ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2B10G10R10;
    (*pixelsFromCpuColorFormat)[ "X8R8G8B8"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X8R8G8B8;
    (*pixelsFromCpuColorFormat)[ "X8B8G8R8"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X8B8G8R8;
    (*pixelsFromCpuColorFormat)[ "R5G6B5"      ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_R5G6B5;
    (*pixelsFromCpuColorFormat)[ "A1R5G5B5"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A1R5G5B5;
    (*pixelsFromCpuColorFormat)[ "X1R5G5B5"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X1R5G5B5;
    (*pixelsFromCpuColorFormat)[ "Y8"          ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y8;
    (*pixelsFromCpuColorFormat)[ "Y16"         ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y16;
    (*pixelsFromCpuColorFormat)[ "Y32"         ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y32;
    (*pixelsFromCpuColorFormat)[ "Z1R5G5B5"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Z1R5G5B5;
    (*pixelsFromCpuColorFormat)[ "O1R5G5B5"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_O1R5G5B5;
    (*pixelsFromCpuColorFormat)[ "Z8R8G8B8"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Z8R8G8B8;
    (*pixelsFromCpuColorFormat)[ "O8R8G8B8"    ] = LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_O8R8G8B8;
    AddMethod( "SetPixelsFromCpuColorFormat", LW902D_SET_PIXELS_FROM_CPU_COLOR_FORMAT, pixelsFromCpuColorFormat);

    AddMethod( "SetPixelsFromCpuColor0", LW902D_SET_PIXELS_FROM_CPU_COLOR0  );
    AddMethod( "SetPixelsFromCpuColor1", LW902D_SET_PIXELS_FROM_CPU_COLOR1   );

    map<string,UINT32> * pixelsFromCpuIndexFormat = new map<string,UINT32>;
    (*pixelsFromCpuIndexFormat)[ "I1" ] = LW902D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1;
    (*pixelsFromCpuIndexFormat)[ "I4" ] = LW902D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I4;
    (*pixelsFromCpuIndexFormat)[ "I8" ] = LW902D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8;
    AddMethod( "SetPixelsFromCpuIndexFormat", LW902D_SET_PIXELS_FROM_CPU_INDEX_FORMAT, pixelsFromCpuIndexFormat);

    map<string,UINT32> * pixelsFromCpuMonoFormat = new map<string,UINT32>;
    (*pixelsFromCpuMonoFormat)[ "CGA6_M1" ] = LW902D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_CGA6_M1;
    (*pixelsFromCpuMonoFormat)[ "LE_M1" ] = LW902D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_LE_M1;
    AddMethod( "SetPixelsFromCpuMonoFormat", LW902D_SET_PIXELS_FROM_CPU_MONO_FORMAT, pixelsFromCpuMonoFormat);

    /*
    map<string,UINT32> * pixelsFromCpuIndexWrap = new map<string,UINT32>;
    (*pixelsFromCpuIndexWrap)[ "NO_WRAP" ] = LW902D_SET_PIXELS_FROM_CPU_INDEX_WRAP_V_NO_WRAP;
    (*pixelsFromCpuIndexWrap)[ "WRAP"    ] = LW902D_SET_PIXELS_FROM_CPU_INDEX_WRAP_V_WRAP   ;
    AddMethod( "SetPixelsFromCpuIndexWrap", LW902D_SET_PIXELS_FROM_CPU_INDEX_WRAP, pixelsFromCpuIndexWrap );
    */

    map<string,UINT32> * pixelsFromCpuWrap = new map<string,UINT32>;
    (*pixelsFromCpuWrap)[ "WRAP_PIXEL" ] = LW902D_SET_PIXELS_FROM_CPU_WRAP_V_WRAP_PIXEL;
    (*pixelsFromCpuWrap)[ "WRAP_BYTE"  ] = LW902D_SET_PIXELS_FROM_CPU_WRAP_V_WRAP_BYTE;
    (*pixelsFromCpuWrap)[ "WRAP_DWORD" ] = LW902D_SET_PIXELS_FROM_CPU_WRAP_V_WRAP_DWORD;
    AddMethod( "SetPixelsFromCpuWrap", LW902D_SET_PIXELS_FROM_CPU_WRAP, pixelsFromCpuWrap );

    map<string,UINT32> * pixelsFromCpuDataType = new map<string,UINT32>;
    (*pixelsFromCpuDataType)[ "COLOR" ] = LW902D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR;
    (*pixelsFromCpuDataType)[ "INDEX" ] = LW902D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_INDEX;
    AddMethod( "SetPixelsFromCpuDataType", LW902D_SET_PIXELS_FROM_CPU_DATA_TYPE, pixelsFromCpuDataType);

    AddMethod( "PixelsFromCpuData", LW902D_PIXELS_FROM_CPU_DATA  );

    AddMethod( "SetPixelsFromMemoryDstX0", LW902D_SET_PIXELS_FROM_MEMORY_DST_X0  );
    AddMethod( "SetPixelsFromMemoryDstY0", LW902D_SET_PIXELS_FROM_MEMORY_DST_Y0  );
    AddMethod( "SetPixelsFromMemoryDuDxFrac", LW902D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC   );
    AddMethod( "SetPixelsFromMemoryDuDxInt", LW902D_SET_PIXELS_FROM_MEMORY_DU_DX_INT    );
    AddMethod( "SetPixelsFromMemoryDvDyFrac", LW902D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC   );
    AddMethod( "SetPixelsFromMemoryDvDyInt", LW902D_SET_PIXELS_FROM_MEMORY_DV_DY_INT    );
    AddMethod( "SetPixelsFromMemoryDstWidth", LW902D_SET_PIXELS_FROM_MEMORY_DST_WIDTH    );
    AddMethod( "SetPixelsFromMemoryDstHeight", LW902D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT   );
    AddMethod( "SetPixelsFromMemorySrcX0Frac", LW902D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC   );
    AddMethod( "SetPixelsFromMemorySrcX0Int", LW902D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT    );
    AddMethod( "SetPixelsFromMemorySrcY0Frac", LW902D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC   );
    AddMethod( "PixelsFromMemorySrcY0Int", LW902D_PIXELS_FROM_MEMORY_SRC_Y0_INT    );

    AddMethod( "SetPixelsFromMemorySampleMode", LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE);
    map<string,UINT32> * pixelsFromMemorySampleModeOrigin = new map<string,UINT32>;
    (*pixelsFromMemorySampleModeOrigin)[ "CENTER" ] = LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CENTER;
    (*pixelsFromMemorySampleModeOrigin)[ "CORNER" ] = LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CORNER;
    AddField( "PixelsFromMemorySampleModeOrigin", LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE, FIELD_RANGE( LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN ), pixelsFromMemorySampleModeOrigin );
    map<string,UINT32> * pixelsFromMemorySampleModeFilter = new map<string,UINT32>;
    (*pixelsFromMemorySampleModeFilter)[ "POINT"    ] = LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT   ;
    (*pixelsFromMemorySampleModeFilter)[ "BILINEAR" ] = LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_BILINEAR;
    AddField( "PixelsFromMemorySampleModeFilter", LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE, FIELD_RANGE( LW902D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER ), pixelsFromMemorySampleModeFilter );

    AddMethod( "SetPixelsFromMemoryDirection", LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION);
    map<string,UINT32> * pixelsFromMemoryDirectionHorizontal = new map<string,UINT32>;
    (*pixelsFromMemoryDirectionHorizontal)[ "HW_DECIDES" ] = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_HW_DECIDES;
    (*pixelsFromMemoryDirectionHorizontal)[ "LEFT_TO_RIGHT" ] = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_LEFT_TO_RIGHT;
    (*pixelsFromMemoryDirectionHorizontal)[ "RIGHT_TO_LEFT" ] = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT;
    AddField( "PixelsFromMemoryDirectionHorizontal", LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION, FIELD_RANGE( LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL ), pixelsFromMemoryDirectionHorizontal );
    map<string,UINT32> * pixelsFromMemoryDirectiolwertical = new map<string,UINT32>;
    (*pixelsFromMemoryDirectiolwertical)[ "HW_DECIDES" ] = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL_HW_DECIDES;
    (*pixelsFromMemoryDirectiolwertical)[ "TOP_TO_BOTTOM" ] = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL_TOP_TO_BOTTOM;
    (*pixelsFromMemoryDirectiolwertical)[ "BOTTOM_TO_TOP" ] = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL_BOTTOM_TO_TOP;
    AddField( "PixelsFromMemoryDirectiolwertical", LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION, FIELD_RANGE( LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL ), pixelsFromMemoryDirectiolwertical );

    map<string,UINT32> * pixelsFromMemorySafeOverlap = new map<string,UINT32>;
    (*pixelsFromMemorySafeOverlap)[ "FALSE" ] = LW902D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP_V_FALSE;
    (*pixelsFromMemorySafeOverlap)[ "TRUE " ] = LW902D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP_V_TRUE ;
    (*pixelsFromMemorySafeOverlap)[ "TRUE" ] = LW902D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP_V_TRUE ;
    AddMethod( "SetPixelsFromMemorySafeOverlap", LW902D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP,pixelsFromMemorySafeOverlap );

    map<string,UINT32> * numTpcs = new map<string,UINT32>;
    (*numTpcs)[ "ALL" ] =  LW902D_SET_NUM_PROCESSING_CLUSTERS_V_ALL;
    (*numTpcs)[ "ONE" ] =  LW902D_SET_NUM_PROCESSING_CLUSTERS_V_ONE;
    AddMethod( "SetNumTpcs", LW902D_SET_NUM_PROCESSING_CLUSTERS, numTpcs );

    map<string,UINT32> * numProcessingClusters = new map<string,UINT32>;
    (*numProcessingClusters)[ "ALL" ] =  LW902D_SET_NUM_PROCESSING_CLUSTERS_V_ALL;
    (*numProcessingClusters)[ "ONE" ] =  LW902D_SET_NUM_PROCESSING_CLUSTERS_V_ONE;
    AddMethod( "SetNumProcessingClusters", LW902D_SET_NUM_PROCESSING_CLUSTERS, numProcessingClusters );

    map<string,UINT32> * ilwalidateLevels = new map<string,UINT32>;
    (*ilwalidateLevels)[ "L1_ONLY" ] =  LW902D_TWOD_ILWALIDATE_TEXTURE_DATA_CACHE_V_L1_ONLY;
    (*ilwalidateLevels)[ "L2_ONLY" ] =  LW902D_TWOD_ILWALIDATE_TEXTURE_DATA_CACHE_V_L2_ONLY;
    (*ilwalidateLevels)[ "L1_AND_L2" ] =  LW902D_TWOD_ILWALIDATE_TEXTURE_DATA_CACHE_V_L1_AND_L2;
    AddMethod( "IlwalidateTextureDataCache", LW902D_TWOD_ILWALIDATE_TEXTURE_DATA_CACHE, ilwalidateLevels );

    map<string,UINT32> * pixelsFromMemoryBlockShape = new map<string,UINT32>;
    (*pixelsFromMemoryBlockShape)[ "SHAPE_16X4"  ] = LW902D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_SHAPE_16X4;
    (*pixelsFromMemoryBlockShape)[ "SHAPE_8X8"   ] = LW902D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_SHAPE_8X8;
    AddMethod( "SetPixelsFromMemoryBlockShape", LW902D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE,pixelsFromMemoryBlockShape);

    AddMethod( "SetPixelsFromMemoryCorralSize", LW902D_SET_PIXELS_FROM_MEMORY_CORRAL_SIZE  );

    map<string,UINT32> * sectorPromotion = new map<string,UINT32>;
    (*sectorPromotion)[ "none" ] = LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION_V_NO_PROMOTION;
    (*sectorPromotion)[ "2v"   ] = LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION_V_PROMOTE_TO_2_V;
    (*sectorPromotion)[ "2h"   ] = LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION_V_PROMOTE_TO_2_H;
    (*sectorPromotion)[ "all"  ] = LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION_V_PROMOTE_TO_4;

    AddMethod( "SetSectorPromotion", LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION, sectorPromotion );

    map<string,UINT32> * dstSurfColorFormat = new map<string,UINT32>;
    (*dstSurfColorFormat)[ "A8R8G8B8"       ] = LW902D_SET_DST_FORMAT_V_A8R8G8B8;
    (*dstSurfColorFormat)[ "A8RL8GL8BL8"    ] = LW902D_SET_DST_FORMAT_V_A8RL8GL8BL8;

    (*dstSurfColorFormat)[ "A2R10G10B10"    ] = LW902D_SET_DST_FORMAT_V_A2R10G10B10;
    (*dstSurfColorFormat)[ "A8B8G8R8"       ] = LW902D_SET_DST_FORMAT_V_A8B8G8R8;
    (*dstSurfColorFormat)[ "A8BL8GL8RL8"    ] = LW902D_SET_DST_FORMAT_V_A8BL8GL8RL8;

    (*dstSurfColorFormat)[ "A2B10G10R10"    ] = LW902D_SET_DST_FORMAT_V_A2B10G10R10;
    (*dstSurfColorFormat)[ "X8R8G8B8"       ] = LW902D_SET_DST_FORMAT_V_X8R8G8B8;
    (*dstSurfColorFormat)[ "X8RL8GL8BL8"    ] = LW902D_SET_DST_FORMAT_V_X8RL8GL8BL8;

    (*dstSurfColorFormat)[ "X8B8G8R8"       ] = LW902D_SET_DST_FORMAT_V_X8B8G8R8;
    (*dstSurfColorFormat)[ "X8BL8GL8RL8"    ] = LW902D_SET_DST_FORMAT_V_X8BL8GL8RL8;

    (*dstSurfColorFormat)[ "R5G6B5"         ] = LW902D_SET_DST_FORMAT_V_R5G6B5;
    (*dstSurfColorFormat)[ "A1R5G5B5"       ] = LW902D_SET_DST_FORMAT_V_A1R5G5B5;
    (*dstSurfColorFormat)[ "X1R5G5B5"       ] = LW902D_SET_DST_FORMAT_V_X1R5G5B5;
    (*dstSurfColorFormat)[ "Y8"             ] = LW902D_SET_DST_FORMAT_V_Y8;
    (*dstSurfColorFormat)[ "Y16"            ] = LW902D_SET_DST_FORMAT_V_Y16;
    (*dstSurfColorFormat)[ "Y32"            ] = LW902D_SET_DST_FORMAT_V_Y32;

    (*dstSurfColorFormat)[ "Z1R5G5B5"    ] = LW902D_SET_DST_FORMAT_V_Z1R5G5B5;
    (*dstSurfColorFormat)[ "O1R5G5B5"    ] = LW902D_SET_DST_FORMAT_V_O1R5G5B5;
    (*dstSurfColorFormat)[ "Z8R8G8B8"    ] = LW902D_SET_DST_FORMAT_V_Z8R8G8B8;
    (*dstSurfColorFormat)[ "O8R8G8B8"    ] = LW902D_SET_DST_FORMAT_V_O8R8G8B8;

    (*dstSurfColorFormat)[ "Y1_8X8"       ] = LW902D_SET_DST_FORMAT_V_Y1_8X8;
    (*dstSurfColorFormat)[ "RF16"         ] = LW902D_SET_DST_FORMAT_V_RF16;
    (*dstSurfColorFormat)[ "RF32"         ] = LW902D_SET_DST_FORMAT_V_RF32;
    (*dstSurfColorFormat)[ "RF32_GF32"    ] = LW902D_SET_DST_FORMAT_V_RF32_GF32;
    (*dstSurfColorFormat)[ "RF16_GF16_BF16_AF16"    ] = LW902D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16;
    (*dstSurfColorFormat)[ "RF16_GF16_BF16_X16"    ] = LW902D_SET_DST_FORMAT_V_RF16_GF16_BF16_X16;
    (*dstSurfColorFormat)[ "RF32_GF32_BF32_AF32"    ] = LW902D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32;
    (*dstSurfColorFormat)[ "RF32_GF32_BF32_X32"    ] = LW902D_SET_DST_FORMAT_V_RF32_GF32_BF32_X32;
    (*dstSurfColorFormat)[ "R16_G16_B16_A16"      ] = LW902D_SET_DST_FORMAT_V_R16_G16_B16_A16;
    (*dstSurfColorFormat)[ "RN16_GN16_BN16_AN16"      ] = LW902D_SET_DST_FORMAT_V_RN16_GN16_BN16_AN16;
    (*dstSurfColorFormat)[ "BF10GF11RF11"      ] = LW902D_SET_DST_FORMAT_V_BF10GF11RF11;
    (*dstSurfColorFormat)[ "AN8BN8GN8RN8"      ] = LW902D_SET_DST_FORMAT_V_AN8BN8GN8RN8;
    (*dstSurfColorFormat)[ "RF16_GF16"      ] = LW902D_SET_DST_FORMAT_V_RF16_GF16;
    (*dstSurfColorFormat)[ "R16_G16"      ] = LW902D_SET_DST_FORMAT_V_R16_G16;
    (*dstSurfColorFormat)[ "RN16_GN16"      ] = LW902D_SET_DST_FORMAT_V_RN16_GN16;
    (*dstSurfColorFormat)[ "G8R8"      ] = LW902D_SET_DST_FORMAT_V_G8R8;
    (*dstSurfColorFormat)[ "GN8RN8"      ] = LW902D_SET_DST_FORMAT_V_GN8RN8;
    (*dstSurfColorFormat)[ "RN16"      ] = LW902D_SET_DST_FORMAT_V_RN16;
    (*dstSurfColorFormat)[ "RN8"      ] = LW902D_SET_DST_FORMAT_V_RN8;
    (*dstSurfColorFormat)[ "A8"      ] = LW902D_SET_DST_FORMAT_V_A8;

    AddMethod( "SetDstFormat", LW902D_SET_DST_FORMAT,dstSurfColorFormat );

    map<string,UINT32> * srcSurfColorFormat = new map<string,UINT32>;
    (*srcSurfColorFormat)[ "A8R8G8B8"       ] = LW902D_SET_SRC_FORMAT_V_A8R8G8B8;
    (*srcSurfColorFormat)[ "A8RL8GL8BL8"    ] = LW902D_SET_SRC_FORMAT_V_A8RL8GL8BL8;

    (*srcSurfColorFormat)[ "A2R10G10B10"    ] = LW902D_SET_SRC_FORMAT_V_A2R10G10B10;
    (*srcSurfColorFormat)[ "A8B8G8R8"       ] = LW902D_SET_SRC_FORMAT_V_A8B8G8R8;
    (*srcSurfColorFormat)[ "A8BL8GL8RL8"    ] = LW902D_SET_SRC_FORMAT_V_A8BL8GL8RL8;

    (*srcSurfColorFormat)[ "A2B10G10R10"    ] = LW902D_SET_SRC_FORMAT_V_A2B10G10R10;
    (*srcSurfColorFormat)[ "X8R8G8B8"       ] = LW902D_SET_SRC_FORMAT_V_X8R8G8B8;
    (*srcSurfColorFormat)[ "X8RL8GL8BL8"    ] = LW902D_SET_SRC_FORMAT_V_X8RL8GL8BL8;

    (*srcSurfColorFormat)[ "X8B8G8R8"       ] = LW902D_SET_SRC_FORMAT_V_X8B8G8R8;
    (*srcSurfColorFormat)[ "X8BL8GL8RL8"    ] = LW902D_SET_SRC_FORMAT_V_X8BL8GL8RL8;

    (*srcSurfColorFormat)[ "R5G6B5"         ] = LW902D_SET_SRC_FORMAT_V_R5G6B5;
    (*srcSurfColorFormat)[ "A1R5G5B5"       ] = LW902D_SET_SRC_FORMAT_V_A1R5G5B5;
    (*srcSurfColorFormat)[ "X1R5G5B5"       ] = LW902D_SET_SRC_FORMAT_V_X1R5G5B5;

    (*srcSurfColorFormat)[ "AY8"            ] = LW902D_SET_SRC_FORMAT_V_AY8;
    (*srcSurfColorFormat)[ "Y8"             ] = LW902D_SET_SRC_FORMAT_V_Y8;
    (*srcSurfColorFormat)[ "Y16"            ] = LW902D_SET_SRC_FORMAT_V_Y16;
    (*srcSurfColorFormat)[ "Y32"            ] = LW902D_SET_SRC_FORMAT_V_Y32;
    (*srcSurfColorFormat)[ "Z1R5G5B5"       ] = LW902D_SET_SRC_FORMAT_V_Z1R5G5B5;
    (*srcSurfColorFormat)[ "O1R5G5B5"       ] = LW902D_SET_SRC_FORMAT_V_O1R5G5B5;
    (*srcSurfColorFormat)[ "Z8R8G8B8"       ] = LW902D_SET_SRC_FORMAT_V_Z8R8G8B8;
    (*srcSurfColorFormat)[ "O8R8G8B8"       ] = LW902D_SET_SRC_FORMAT_V_O8R8G8B8;
    (*srcSurfColorFormat)[ "Y1_8X8"         ] = LW902D_SET_SRC_FORMAT_V_Y1_8X8;
    (*srcSurfColorFormat)[ "RF16"           ] = LW902D_SET_SRC_FORMAT_V_RF16;
    (*srcSurfColorFormat)[ "RF32"           ] = LW902D_SET_SRC_FORMAT_V_RF32;
    (*srcSurfColorFormat)[ "RF32_GF32"      ] = LW902D_SET_SRC_FORMAT_V_RF32_GF32;

    (*srcSurfColorFormat)[ "RF16_GF16_BF16_AF16"     ] = LW902D_SET_SRC_FORMAT_V_RF16_GF16_BF16_AF16;
    (*srcSurfColorFormat)[ "RF16_GF16_BF16_X16"      ] = LW902D_SET_SRC_FORMAT_V_RF16_GF16_BF16_X16;
    (*srcSurfColorFormat)[ "RF32_GF32_BF32_AF32"     ] = LW902D_SET_SRC_FORMAT_V_RF32_GF32_BF32_AF32;
    (*srcSurfColorFormat)[ "RF32_GF32_BF32_X32"      ] = LW902D_SET_SRC_FORMAT_V_RF32_GF32_BF32_X32;

    (*srcSurfColorFormat)[ "R16_G16_B16_A16"      ] = LW902D_SET_SRC_FORMAT_V_R16_G16_B16_A16;
    (*srcSurfColorFormat)[ "RN16_GN16_BN16_AN16"      ] = LW902D_SET_SRC_FORMAT_V_RN16_GN16_BN16_AN16;
    (*srcSurfColorFormat)[ "BF10GF11RF11"      ] = LW902D_SET_SRC_FORMAT_V_BF10GF11RF11;
    (*srcSurfColorFormat)[ "AN8BN8GN8RN8"      ] = LW902D_SET_SRC_FORMAT_V_AN8BN8GN8RN8;
    (*srcSurfColorFormat)[ "RF16_GF16"      ] = LW902D_SET_SRC_FORMAT_V_RF16_GF16;
    (*srcSurfColorFormat)[ "R16_G16"      ] = LW902D_SET_SRC_FORMAT_V_R16_G16;
    (*srcSurfColorFormat)[ "RN16_GN16"      ] = LW902D_SET_SRC_FORMAT_V_RN16_GN16;
    (*srcSurfColorFormat)[ "G8R8"      ] = LW902D_SET_SRC_FORMAT_V_G8R8;
    (*srcSurfColorFormat)[ "GN8RN8"      ] = LW902D_SET_SRC_FORMAT_V_GN8RN8;
    (*srcSurfColorFormat)[ "RN16"      ] = LW902D_SET_SRC_FORMAT_V_RN16;
    (*srcSurfColorFormat)[ "RN8"      ] = LW902D_SET_SRC_FORMAT_V_RN8;
    (*srcSurfColorFormat)[ "A8"      ] = LW902D_SET_SRC_FORMAT_V_A8;

    AddMethod( "SetSrcFormat", LW902D_SET_SRC_FORMAT,srcSurfColorFormat );

    // In case folks want to enable compression inside v2d tests
    // This method only works for GK20A and will be a noop with
    // other chips.
    AddMethod("SetCompression", LW902D_SET_COMPRESSION);

    // Enable 2D Color Compression
    if (m_v2d->GetParams()->ParamPresent("-enable_color_compression"))
    {
        MethodWrite(LW902D_SET_COMPRESSION, true);
        WaitForIdle();

        InfoPrintf("2D Color Compression enabled successfully.\n");
    }

    if (m_v2d->GetParams()->ParamPresent("-c_render_to_z_2d"))
    {
        MethodWrite(LW902D_SET_DST_COLOR_RENDER_TO_ZETA_SURFACE,
            DRF_DEF(902D, _SET_DST_COLOR_RENDER_TO_ZETA_SURFACE, _V, _TRUE));
    }
}

//
// determine the src and destination surfaces from the arg map and
// write all the necessary methods for the source and destination surface
//

void V2dTwoD::SetSurfaces( map<string,V2dSurface*> & argMap )
{
    V2dSurface *src = 0;
    V2dSurface *dst = 0;
    map<string,V2dSurface*>::iterator i;

    for ( i = argMap.begin(); i != argMap.end(); i++ )
    {
        if ( (*i).first == "src" )
            src = (*i).second;
        else if ( (*i).first == "dst" )
            dst = (*i).second;
        else
            V2D_THROW( "SetSurfaces: unsupported surface tag string: " << (*i).first );
    }

    if ( (src == 0) || (dst == 0) )
        V2D_THROW( "Twod::SetSurfaces(): must supply both a source and destination surface" );

    GpuDevice *gpuDevice = m_v2d->GetLWGpu()->GetGpuDevice();
    GpuSubdevice *gpuSubdevice = gpuDevice->GetSubdevice(0);

    if (m_v2d->GetParams()->ParamPresent("-pte_kind_src"))
    {
        const char *pteKindStr = m_v2d->GetParams()->ParamStr("-pte_kind_src");
        UINT32 pteKind;

        if (gpuSubdevice->GetPteKindFromName(pteKindStr, &pteKind))
        {
            RC rc = src->ChangePteKind(pteKind);

            if (OK != rc)
            {
                V2D_THROW("SetSurfaces: src PTE kind change failed");
            }

            InfoPrintf("v2d src PTE kind has been changed to 0x%02x\n", pteKind);
        }
        else
        {
            ErrPrintf("Unrecognized PTE kind for -pte_kind_src argument: %s\n",
                pteKindStr);
        }
    }

    if (m_v2d->GetParams()->ParamPresent("-pte_kind_dst"))
    {
        const char *pteKindStr = m_v2d->GetParams()->ParamStr("-pte_kind_dst");
        UINT32 pteKind;

        if (gpuSubdevice->GetPteKindFromName(pteKindStr, &pteKind))
        {
            RC rc = dst->ChangePteKind(pteKind);

            if (OK != rc)
            {
                V2D_THROW("SetSurfaces: dst PTE kind change failed");
            }

            InfoPrintf("v2d dst PTE kind has been changed to 0x%02x\n", pteKind);
        }
        else
        {
            ErrPrintf("Unrecognized PTE kind for -pte_kind_dst argument: %s\n",
                pteKindStr);
        }
    }

    int src_width  = src->GetWidth();
    int src_height = src->GetHeight();
    int dst_width  = dst->GetWidth();
    int dst_height = dst->GetHeight();

    if (src->GetFormatString() == "Y1_8X8") {
        src_width *= 8;
        src_height *= 8;
    }
    if (dst->GetFormatString() == "Y1_8X8") {
        dst_width *= 8;
        dst_height *= 8;
    }

    NamedMethodOrFieldWriteShadow( "SetSrcFormat" , src->GetFormatString() );
    NamedMethodOrFieldWriteShadow( "SetDstFormat", dst->GetFormatString() );

    NamedMethodOrFieldWriteShadow( "SetSrcPitch" ,  src->GetPitch () );
    NamedMethodOrFieldWriteShadow( "SetSrcOffsetLower",  src->GetOffset() );
    NamedMethodOrFieldWriteShadow( "SetSrcOffsetUpper",  src->GetOffset()>>32 );
    NamedMethodOrFieldWriteShadow( "SetDstPitch" , dst->GetPitch () );
    NamedMethodOrFieldWriteShadow( "SetDstOffsetLower", dst->GetOffset() );
    NamedMethodOrFieldWriteShadow( "SetDstOffsetUpper", dst->GetOffset()>>32 );
    NamedMethodOrFieldWriteShadow( "SetSrcWidth", src_width );
    NamedMethodOrFieldWriteShadow( "SetSrcHeight",src_height );
    NamedMethodOrFieldWriteShadow( "SetSrcDepth",src->GetDepth() );
    NamedMethodOrFieldWriteShadow( "SetDstWidth", dst_width );
    NamedMethodOrFieldWriteShadow( "SetDstHeight",dst_height );
    NamedMethodOrFieldWriteShadow( "SetDstDepth",dst->GetDepth() );

    NamedMethodOrFieldWriteShadow( "SetSrcFormat", src->GetFormatString() );
    NamedMethodOrFieldWriteShadow( "SetDstFormat", dst->GetFormatString() );

    if(src->isBlocklinear())
         {
         NamedMethodOrFieldWriteShadow( "SetSrcMemoryLayout","BLOCKLINEAR" );
         NamedMethodOrFieldWriteShadow( "SetSrcBlockSize_Height",src->Log2BlockHeightGob() );
         NamedMethodOrFieldWriteShadow( "SetSrcBlockSize_Depth",src->Log2BlockDepthGob() );
         }
    else
         {
         NamedMethodOrFieldWriteShadow( "SetSrcMemoryLayout","PITCH" );
         NamedMethodOrFieldWriteShadow( "SetSrcPitch" ,  src->GetPitch () );
         }

    if(dst->isBlocklinear())
         {
         NamedMethodOrFieldWriteShadow( "SetDstMemoryLayout","BLOCKLINEAR" );
         NamedMethodOrFieldWriteShadow( "SetDstBlockSize_Height",dst->Log2BlockHeightGob() );
         NamedMethodOrFieldWriteShadow( "SetDstBlockSize_Depth",dst->Log2BlockDepthGob() );
         }
    else
         {
         NamedMethodOrFieldWriteShadow( "SetDstMemoryLayout","PITCH" );
         NamedMethodOrFieldWriteShadow( "SetDstPitch" ,  dst->GetPitch () );
         }

    string sector_promotion = src->GetV2d()->GetSectorPromotion();
    if (sector_promotion != "none") {

        LW0080_CTRL_GR_GET_CAPS_V2_PARAMS params = {0};
        LwRm* pLwRm = m_ch->GetLwRmPtr();

        params.bCapsPopulated = LW_FALSE;

        if (pLwRm->Control(pLwRm->GetDeviceHandle(gpuDevice),
                           LW0080_CTRL_CMD_GR_GET_CAPS_V2,
                           &params,
                           sizeof (params)) != OK)
            V2D_THROW( "SetSurfaces: pLwRm->Control failed" );

        if (!LW0080_CTRL_GR_GET_CAP(
                params.capsTbl, LW0080_CTRL_GR_CAPS_ENABLE_SECTOR_PROMOTION))
            V2D_THROW( "SetSurfaces: -l1_promotion specified but chip doesn't support SetSectorPromotion method (caps bit not set)" );

        NamedMethodOrFieldWriteShadow( "SetSectorPromotion" , sector_promotion );
        InfoPrintf( "v2d setting l1 sector promotion: %s\n", sector_promotion.c_str() );
    }

    // flush writes out to GPU
    FlushShadowWritesToGpu();
}
