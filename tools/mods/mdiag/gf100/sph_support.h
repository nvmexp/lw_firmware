/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _SPH_SUPPORT_H_
#define _SPH_SUPPORT_H_

#define DWORDS_PER_MAP 8
#define SPH_ATTRIBUTE_SIZE 240
#define SET_BMAP_BIT(map,bit) ((map)[(bit)>>5] |= (1 << ((bit) & 0x1F)))
#define GET_BMAP_BIT(map,bit) ((((map)[(bit)>>5]) >> ((bit) & 0x1F)) & 1)
#define SET_PS_IMAP_BIT(map,bit,val) (((map)[(bit)>>4]) |= ((val) << (((bit) & 0xF) << 1)))
#define GET_PS_IMAP_BIT(map,bit) ((((map)[(bit)>>4]) >> (((bit) & 0xF) << 1)) & 3)

namespace FermiGraphicsStructs  {

typedef unsigned char U007, U006, U005, U004, U003, U002, U001;
typedef unsigned short U015, U014, U013, U012, U011, U010, U009;

enum TYPEDEF_SPH_VTG_ODMAP {
    TYPEDEF_SPH_VTG_ODMAPST = 0,
    TYPEDEF_SPH_VTG_ODMAPST_REQ = 1,
    TYPEDEF_SPH_VTG_ODMAPST_LAST = 2,
    TYPEDEF_SPH_VTG_ODMAPDISCARD = 3
};

enum TYPEDEF_SAMPLE_MODES {
    MODE_1X1 = 0,
    MODE_2X1 = 1,
    MODE_2X2 = 2,
    MODE_4X2 = 3,
    MODE_4X2_D3D = 4,
    MODE_2X1_D3D = 5,
    MODE_4X4 = 6,
    MODE_2X2_VC_4 = 8,
    MODE_2X2_VC_12 = 9,
    MODE_4X2_VC_8 = 10,
    MODE_4X2_VC_24 = 11
};

enum GobsPerBlock {
    ONE_GOB = 0,
    TWO_GOBS = 1,
    FOUR_GOBS = 2,
    EIGHT_GOBS = 3,
    SIXTEEN_GOBS = 4,
    THIRTYTWO_GOBS = 5
};

enum NumericalDataType {
    NUM_SNORM = 1,
    NUM_UNORM = 2,
    NUM_SINT = 3,
    NUM_UINT = 4,
    NUM_SNORM_FORCE_FP16 = 5,
    NUM_UNORM_FORCE_FP16 = 6,
    NUM_FLOAT = 7
};

enum TYPEDEF_SHADER_IO_SLOTS_AAM_VERSION_02 {
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_28 = 0,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_29 = 1,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_30 = 2,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_31 = 3,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_32 = 4,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_33 = 5,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_34 = 6,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_35 = 7,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_36 = 8,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_37 = 9,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_00 = 10,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_01 = 11,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_02 = 12,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_03 = 13,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_04 = 14,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_05 = 15,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_06 = 16,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_07 = 17,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_08 = 18,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_09 = 19,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_10 = 20,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_11 = 21,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_12 = 22,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_13 = 23,
    AAM_VERSION_02__PRIMITIVE_ID = 24,
    AAM_VERSION_02__RT_ARRAY_INDEX = 25,
    AAM_VERSION_02__VIEWPORT_INDEX = 26,
    AAM_VERSION_02__POINT_SIZE = 27,
    AAM_VERSION_02__POSITION_X = 28,
    AAM_VERSION_02__POSITION_Y = 29,
    AAM_VERSION_02__POSITION_Z = 30,
    AAM_VERSION_02__POSITION_W = 31,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_00_X = 32,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_00_Y = 33,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_00_Z = 34,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_00_W = 35,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_01_X = 36,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_01_Y = 37,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_01_Z = 38,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_01_W = 39,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_02_X = 40,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_02_Y = 41,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_02_Z = 42,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_02_W = 43,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_03_X = 44,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_03_Y = 45,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_03_Z = 46,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_03_W = 47,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_04_X = 48,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_04_Y = 49,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_04_Z = 50,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_04_W = 51,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_05_X = 52,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_05_Y = 53,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_05_Z = 54,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_05_W = 55,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_06_X = 56,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_06_Y = 57,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_06_Z = 58,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_06_W = 59,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_07_X = 60,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_07_Y = 61,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_07_Z = 62,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_07_W = 63,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_08_X = 64,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_08_Y = 65,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_08_Z = 66,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_08_W = 67,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_09_X = 68,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_09_Y = 69,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_09_Z = 70,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_09_W = 71,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_10_X = 72,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_10_Y = 73,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_10_Z = 74,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_10_W = 75,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_11_X = 76,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_11_Y = 77,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_11_Z = 78,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_11_W = 79,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_12_X = 80,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_12_Y = 81,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_12_Z = 82,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_12_W = 83,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_13_X = 84,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_13_Y = 85,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_13_Z = 86,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_13_W = 87,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_14_X = 88,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_14_Y = 89,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_14_Z = 90,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_14_W = 91,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_15_X = 92,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_15_Y = 93,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_15_Z = 94,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_15_W = 95,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_16_X = 96,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_16_Y = 97,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_16_Z = 98,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_16_W = 99,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_17_X = 100,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_17_Y = 101,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_17_Z = 102,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_17_W = 103,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_18_X = 104,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_18_Y = 105,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_18_Z = 106,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_18_W = 107,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_19_X = 108,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_19_Y = 109,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_19_Z = 110,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_19_W = 111,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_20_X = 112,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_20_Y = 113,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_20_Z = 114,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_20_W = 115,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_21_X = 116,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_21_Y = 117,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_21_Z = 118,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_21_W = 119,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_22_X = 120,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_22_Y = 121,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_22_Z = 122,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_22_W = 123,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_23_X = 124,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_23_Y = 125,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_23_Z = 126,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_23_W = 127,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_24_X = 128,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_24_Y = 129,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_24_Z = 130,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_24_W = 131,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_25_X = 132,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_25_Y = 133,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_25_Z = 134,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_25_W = 135,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_26_X = 136,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_26_Y = 137,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_26_Z = 138,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_26_W = 139,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_27_X = 140,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_27_Y = 141,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_27_Z = 142,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_27_W = 143,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_28_X = 144,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_28_Y = 145,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_28_Z = 146,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_28_W = 147,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_29_X = 148,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_29_Y = 149,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_29_Z = 150,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_29_W = 151,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_30_X = 152,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_30_Y = 153,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_30_Z = 154,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_30_W = 155,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_31_X = 156,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_31_Y = 157,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_31_Z = 158,
    AAM_VERSION_02__GENERIC_ATTRIBUTE_31_W = 159,
    AAM_VERSION_02__COLOR_FRONT_DIFFUSE_RED = 160,
    AAM_VERSION_02__COLOR_FRONT_DIFFUSE_GREEN = 161,
    AAM_VERSION_02__COLOR_FRONT_DIFFUSE_BLUE = 162,
    AAM_VERSION_02__COLOR_FRONT_DIFFUSE_ALPHA = 163,
    AAM_VERSION_02__COLOR_FRONT_SPELWLAR_RED = 164,
    AAM_VERSION_02__COLOR_FRONT_SPELWLAR_GREEN = 165,
    AAM_VERSION_02__COLOR_FRONT_SPELWLAR_BLUE = 166,
    AAM_VERSION_02__COLOR_FRONT_SPELWLAR_ALPHA = 167,
    AAM_VERSION_02__COLOR_BACK_DIFFUSE_RED = 168,
    AAM_VERSION_02__COLOR_BACK_DIFFUSE_GREEN = 169,
    AAM_VERSION_02__COLOR_BACK_DIFFUSE_BLUE = 170,
    AAM_VERSION_02__COLOR_BACK_DIFFUSE_ALPHA = 171,
    AAM_VERSION_02__COLOR_BACK_SPELWLAR_RED = 172,
    AAM_VERSION_02__COLOR_BACK_SPELWLAR_GREEN = 173,
    AAM_VERSION_02__COLOR_BACK_SPELWLAR_BLUE = 174,
    AAM_VERSION_02__COLOR_BACK_SPELWLAR_ALPHA = 175,
    AAM_VERSION_02__CLIP_DISTANCE_0 = 176,
    AAM_VERSION_02__CLIP_DISTANCE_1 = 177,
    AAM_VERSION_02__CLIP_DISTANCE_2 = 178,
    AAM_VERSION_02__CLIP_DISTANCE_3 = 179,
    AAM_VERSION_02__CLIP_DISTANCE_4 = 180,
    AAM_VERSION_02__CLIP_DISTANCE_5 = 181,
    AAM_VERSION_02__CLIP_DISTANCE_6 = 182,
    AAM_VERSION_02__CLIP_DISTANCE_7 = 183,
    AAM_VERSION_02__POINT_SPRITE_S = 184,
    AAM_VERSION_02__POINT_SPRITE_T = 185,
    AAM_VERSION_02__FOG_COORDINATE = 186,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_17 = 187,
    AAM_VERSION_02__TESSELLATION_EVALUATION_POINT_U = 188,
    AAM_VERSION_02__TESSELLATION_EVALUATION_POINT_V = 189,
    AAM_VERSION_02__INSTANCE_ID = 190,
    AAM_VERSION_02__VERTEX_ID = 191,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_0_S = 192,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_0_T = 193,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_0_R = 194,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_0_Q = 195,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_1_S = 196,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_1_T = 197,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_1_R = 198,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_1_Q = 199,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_2_S = 200,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_2_T = 201,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_2_R = 202,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_2_Q = 203,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_3_S = 204,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_3_T = 205,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_3_R = 206,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_3_Q = 207,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_4_S = 208,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_4_T = 209,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_4_R = 210,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_4_Q = 211,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_5_S = 212,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_5_T = 213,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_5_R = 214,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_5_Q = 215,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_6_S = 216,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_6_T = 217,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_6_R = 218,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_6_Q = 219,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_7_S = 220,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_7_T = 221,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_7_R = 222,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_7_Q = 223,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_8_S = 224,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_8_T = 225,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_8_R = 226,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_8_Q = 227,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_9_S = 228,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_9_T = 229,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_9_R = 230,
    AAM_VERSION_02__FIXED_FNC_TEXTURE_9_Q = 231,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_18 = 232,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_19 = 233,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_20 = 234,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_21 = 235,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_22 = 236,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_23 = 237,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_24 = 238,
    AAM_VERSION_02__SYSTEM_VALUE_RESERVED_25 = 239,
    AAM_VERSION_02__ILLEGAL_16 = 240,
    AAM_VERSION_02__ILLEGAL_17 = 241,
    AAM_VERSION_02__ILLEGAL_18 = 242,
    AAM_VERSION_02__ILLEGAL_19 = 243,
    AAM_VERSION_02__ILLEGAL_20 = 244,
    AAM_VERSION_02__ILLEGAL_21 = 245,
    AAM_VERSION_02__ILLEGAL_22 = 246,
    AAM_VERSION_02__ILLEGAL_23 = 247,
    AAM_VERSION_02__ILLEGAL_24 = 248,
    AAM_VERSION_02__ILLEGAL_25 = 249,
    AAM_VERSION_02__ILLEGAL_26 = 250,
    AAM_VERSION_02__ILLEGAL_27 = 251,
    AAM_VERSION_02__ILLEGAL_28 = 252,
    AAM_VERSION_02__ILLEGAL_29 = 253,
    AAM_VERSION_02__ILLEGAL_30 = 254,
    AAM_VERSION_02__IS_FRONT_FACE = 255
};

enum FontFilterSize {
    size_1 = 0,
    size_2 = 1,
    size_3 = 2,
    size_4 = 3,
    size_5 = 4,
    size_6 = 5,
    size_7 = 6,
    size_8 = 7
};

enum SectorPromotion {
    NO_PROMOTION = 0,
    PROMOTE_TO_2_V = 1,
    PROMOTE_TO_2_H = 2,
    PROMOTE_TO_4 = 3
};

enum TYPEDEF_SPH_DESCRIPTOR_SIZE {
    TYPEDEF_SPH_DESCRIPTOR_SIZENONEXISTENT = 0,
    TYPEDEF_SPH_DESCRIPTOR_SIZEONE_BIT = 1,
    TYPEDEF_SPH_DESCRIPTOR_SIZETWO_BITS = 2,
    TYPEDEF_SPH_DESCRIPTOR_SIZETWELVE_BITS = 3
};

enum TYPEDEF_SPH_TYPE {
    TYPE_01_VTG = 1,
    TYPE_02_PS = 2,
    TYPE_03_VS_DX9_SM3 = 3,
    TYPE_04_PS_DX9_SM3 = 4
};

enum AddressMode {
    WRAP = 0,
    MIRROR = 1,
    CLAMP_TO_EDGE = 2,
    BORDER = 3,
    CLAMP_OGL = 4,
    MIRROR_ONCE_CLAMP_TO_EDGE = 5,
    MIRROR_ONCE_BORDER = 6,
    MIRROR_ONCE_CLAMP_OGL = 7
};

enum TextureType {
    ONE_D = 0,
    TWO_D = 1,
    THREE_D = 2,
    LWBEMAP = 3,
    ONE_D_ARRAY = 4,
    TWO_D_ARRAY = 5,
    ONE_D_BUFFER = 6,
    TWO_D_NO_MIPMAP = 7,
    LWBEMAP_ARRAY = 8,
    TT_BIT_FIELD_SIZE = 15
};

enum BorderSource {
    BORDER_TEXTURE = 0,
    BORDER_COLOR = 1
};

enum TYPEDEF_SPH_INTERPOLATION_TYPE {
    TYPEDEF_SPH_INTERPOLATION_TYPESET_BY_SPH = 0,
    TYPEDEF_SPH_INTERPOLATION_TYPECONSTANT = 1,
    TYPEDEF_SPH_INTERPOLATION_TYPEPERSPECTIVE = 2,
    TYPEDEF_SPH_INTERPOLATION_TYPESCREEN_LINEAR = 3
};

enum MaxAnisotropy {
    ANISO_1_TO_1 = 0,
    ANISO_2_TO_1 = 1,
    ANISO_4_TO_1 = 2,
    ANISO_6_TO_1 = 3,
    ANISO_8_TO_1 = 4,
    ANISO_10_TO_1 = 5,
    ANISO_12_TO_1 = 6,
    ANISO_16_TO_1 = 7
};

enum TYPEDEF_SPH_DEFAULT_TYPE {
    TYPEDEF_SPH_DEFAULT_TYPEFLOAT_ZERO = 0,
    TYPEDEF_SPH_DEFAULT_TYPEFLOAT_ONE = 1,
    TYPEDEF_SPH_DEFAULT_TYPEINT_ZERO = 2,
    TYPEDEF_SPH_DEFAULT_TYPEHEX_FFFFFFFF = 3,
    TYPEDEF_SPH_DEFAULT_TYPEMETHOD_BIT_00 = 8,
    TYPEDEF_SPH_DEFAULT_TYPEMETHOD_BIT_01 = 9,
    TYPEDEF_SPH_DEFAULT_TYPEMETHOD_BIT_02 = 10,
    TYPEDEF_SPH_DEFAULT_TYPEMETHOD_BIT_03 = 11,
    TYPEDEF_SPH_DEFAULT_TYPEMETHOD_BIT_04 = 12,
    TYPEDEF_SPH_DEFAULT_TYPEMETHOD_BIT_05 = 13
};

enum MipFilter {
    MIP_NONE = 1,
    MIP_POINT = 2,
    MIP_LINEAR = 3
};

enum LodQuality {
    LOD_QUALITY_LOW = 0,
    LOD_QUALITY_HIGH = 1
};

enum AnisoSpreadFunc {
    SPREAD_FUNC_HALF = 0,
    SPREAD_FUNC_ONE = 1,
    SPREAD_FUNC_TWO = 2,
    SPREAD_FUNC_MAX = 3
};

enum TYPEDEF_SPH_OUTPUT_TOPOLOGY {
    POINTLIST = 1,
    LINESTRIP = 6,
    TRIANGLESTRIP = 7
};

enum TYPEDEF_SPH_VTG_IDMAP {
    TYPEDEF_SPH_VTG_IDMAPLD = 0,
    TYPEDEF_SPH_VTG_IDMAPLD_REQ = 1,
    TYPEDEF_SPH_VTG_IDMAPDEFAULT = 2
};

enum ComponentSizes {
    R32_G32_B32_A32 = 1,
    R32_G32_B32 = 2,
    R16_G16_B16_A16 = 3,
    R32_G32 = 4,
    R32_B24G8 = 5,
    X8B8G8R8 = 7,
    A8B8G8R8 = 8,
    A2B10G10R10 = 9,
    R16_G16 = 12,
    G8R24 = 13,
    G24R8 = 14,
    R32 = 15,
    A4B4G4R4 = 18,
    A5B5G5R1 = 19,
    A1B5G5R5 = 20,
    B5G6R5 = 21,
    B6G5R5 = 22,
    G8R8 = 24,
    R16 = 27,
    Y8_VIDEO = 28,
    R8 = 29,
    G4R4 = 30,
    R1 = 31,
    E5B9G9R9_SHAREDEXP = 32,
    BF10GF11RF11 = 33,
    G8B8G8R8 = 34,
    B8G8R8G8 = 35,
    DXT1 = 36,
    DXT23 = 37,
    DXT45 = 38,
    DXN1 = 39,
    DXN2 = 40,
    BC6H_SF16 = 16,
    BC6H_UF16 = 17,
    BC7U = 23,
    Z24S8 = 41,
    X8Z24 = 42,
    S8Z24 = 43,
    X4V4Z24__COV4R4V = 44,
    X4V4Z24__COV8R8V = 45,
    V8Z24__COV4R12V = 46,
    ZF32 = 47,
    ZF32_X24S8 = 48,
    X8Z24_X20V4S8__COV4R4V = 49,
    X8Z24_X20V4S8__COV8R8V = 50,
    ZF32_X20V4X8__COV4R4V = 51,
    ZF32_X20V4X8__COV8R8V = 52,
    ZF32_X20V4S8__COV4R4V = 53,
    ZF32_X20V4S8__COV8R8V = 54,
    X8Z24_X16V8S8__COV4R12V = 55,
    ZF32_X16V8X8__COV4R12V = 56,
    ZF32_X16V8S8__COV4R12V = 57,
    Z16 = 58,
    V8Z24__COV8R24V = 59,
    X8Z24_X16V8S8__COV8R24V = 60,
    ZF32_X16V8X8__COV8R24V = 61,
    ZF32_X16V8S8__COV8R24V = 62,
    CS_BITFIELD_SIZE = 63
};

enum MemoryLayout {
    BLOCKLINEAR = 0,
    PITCH = 1
};

enum TYPEDEF_SPH_PIXEL_IMAP {
    UNUSED = 0,
    CONSTANT = 1,
    PERSPECTIVE = 2,
    SCREEN_LINEAR = 3
};

enum MagFilter {
    MAG_POINT = 1,
    MAG_LINEAR = 2,
    VCAA_4_TAP = 3,
    VCAA_8_TAP = 4
};

enum XYZWSource {
    IN_ZERO = 0,
    IN_R = 2,
    IN_G = 3,
    IN_B = 4,
    IN_A = 5,
    IN_ONE_INT = 6,
    IN_ONE_FLOAT = 7
};

enum AnisoSpreadModifier {
    SPREAD_MODIFIER_NONE = 0,
    SPREAD_MODIFIER_CONST_ONE = 1,
    SPREAD_MODIFIER_CONST_TWO = 2,
    SPREAD_MODIFIER_SQRT = 3
};

enum TYPEDEF_SHADER_TYPE {
    VERTEX_LWLL_BEFORE_FETCH = 0,
    VERTEX = 1,
    TESSELLATION_INIT = 2,
    TESSELLATION = 3,
    GEOMETRY = 4,
    PIXEL = 5
};

enum MinFilter {
    MIN_POINT = 1,
    MIN_LINEAR = 2,
    MIN_ANISO = 3
};

/* HACK: commented out due to naming conflict. It overlaps with GobsPerBlock
enum GobsPerBlockWidth {
    ONE_GOB = 0
}; */

enum DepthCompareFunc {
    ZC_NEVER = 0,
    ZC_LESS = 1,
    ZC_EQUAL = 2,
    ZC_LEQUAL = 3,
    ZC_GREATER = 4,
    ZC_NOTEQUAL = 5,
    ZC_GEQUAL = 6,
    ZC_ALWAYS = 7
};

struct ShaderIoSlotsAamVersion02 {

    unsigned long long Unused : 8;         // Enum(TYPEDEF_SHADER_IO_SLOTS_AAM_VERSION_02)
};

struct StructSphType01Version03 {

    struct StructSphCommonWord0 {
        unsigned long long SphType : 5;         // Enum(TYPEDEF_SPH_TYPE)
        unsigned long long Version : 5;         // Simple
        unsigned long long ShaderType : 4;         // Enum(TYPEDEF_SHADER_TYPE)
        unsigned long long MrtEnable : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long KillsPixels : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long DoesGlobalStore : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long SassVersion : 4;         // Simple
        unsigned long long ReservedCommonA_0 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_1 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_2 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_3 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_4 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long DoesLoadOrStore : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long DoesFp64 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long StreamOutMask : 4;         // Simple
    };
    unsigned long long StructSphCommonWord0 : 32;
    // NestedStruct(StructSphCommonWord0)

    struct StructSphCommonWord1 {
        unsigned long long ShaderLocalMemoryLowSize : 24;         // Simple
        unsigned long long PerPatchAttributeCount : 8;         // Simple
    };
    unsigned long long StructSphCommonWord1 : 32;
    // NestedStruct(StructSphCommonWord1)

    struct StructSphCommonWord2 {
        unsigned long long ShaderLocalMemoryHighSize : 24;         // Simple
        unsigned long long ThreadsPerInputPrimitive : 8;         // Simple
    };
    unsigned long long StructSphCommonWord2 : 32;
    // NestedStruct(StructSphCommonWord2)

    struct StructSphCommonWord3 {
        unsigned long long ShaderLocalMemoryCrsSize : 24;         // Simple
        unsigned long long OutputTopology : 4;         // Enum(TYPEDEF_SPH_OUTPUT_TOPOLOGY)
        unsigned long long ReservedCommonB : 4;         // Simple
    };
    unsigned long long StructSphCommonWord3 : 32;
    // NestedStruct(StructSphCommonWord3)

    struct StructSphCommonWord4 {
        unsigned long long MaxOutputVertexCount : 12;         // Simple
        unsigned long long StoreReqStart : 8;         // Simple
        unsigned long long ReservedCommonC_0 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonC_1 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonC_2 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonC_3 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long StoreReqEnd : 8;         // Simple
    };
    unsigned long long StructSphCommonWord4 : 32;
    // NestedStruct(StructSphCommonWord4)

    struct StructSphImapSystemValuesA {
        unsigned long long ImapSystemValuesReserved28 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved29 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved30 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved31 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodLeft : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodRight : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodBottom : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodTop : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationInteriorU : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationInteriorV : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved00 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved01 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved02 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved03 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved04 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved05 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved06 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved07 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved08 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved09 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved10 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved11 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved12 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved13 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapSystemValuesA : 24;
    // NestedStruct(StructSphImapSystemValuesA)

    struct StructSphImapSystemValuesB {
        unsigned long long ImapPrimitiveId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapRtArrayIndex : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapViewportIndex : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPointSize : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapSystemValuesB : 8;
    // NestedStruct(StructSphImapSystemValuesB)

    struct StructSphImapVector_0 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_0 : 4;
        // NestedStruct(StructSphImapVector_0)

    struct StructSphImapVector_1 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_1 : 4;
        // NestedStruct(StructSphImapVector_1)

    struct StructSphImapVector_2 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_2 : 4;
        // NestedStruct(StructSphImapVector_2)

    struct StructSphImapVector_3 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_3 : 4;
        // NestedStruct(StructSphImapVector_3)

    struct StructSphImapVector_4 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_4 : 4;
        // NestedStruct(StructSphImapVector_4)

    struct StructSphImapVector_5 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_5 : 4;
        // NestedStruct(StructSphImapVector_5)

    struct StructSphImapVector_6 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_6 : 4;
        // NestedStruct(StructSphImapVector_6)

    struct StructSphImapVector_7 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_7 : 4;
        // NestedStruct(StructSphImapVector_7)

    struct StructSphImapVector_8 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_8 : 4;
        // NestedStruct(StructSphImapVector_8)

    struct StructSphImapVector_9 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_9 : 4;
        // NestedStruct(StructSphImapVector_9)

    struct StructSphImapVector_10 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_10 : 4;
        // NestedStruct(StructSphImapVector_10)

    struct StructSphImapVector_11 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_11 : 4;
        // NestedStruct(StructSphImapVector_11)

    struct StructSphImapVector_12 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_12 : 4;
        // NestedStruct(StructSphImapVector_12)

    struct StructSphImapVector_13 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_13 : 4;
        // NestedStruct(StructSphImapVector_13)

    struct StructSphImapVector_14 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_14 : 4;
        // NestedStruct(StructSphImapVector_14)

    struct StructSphImapVector_15 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_15 : 4;
        // NestedStruct(StructSphImapVector_15)

    struct StructSphImapVector_16 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_16 : 4;
        // NestedStruct(StructSphImapVector_16)

    struct StructSphImapVector_17 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_17 : 4;
        // NestedStruct(StructSphImapVector_17)

    struct StructSphImapVector_18 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_18 : 4;
        // NestedStruct(StructSphImapVector_18)

    struct StructSphImapVector_19 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_19 : 4;
        // NestedStruct(StructSphImapVector_19)

    struct StructSphImapVector_20 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_20 : 4;
        // NestedStruct(StructSphImapVector_20)

    struct StructSphImapVector_21 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_21 : 4;
        // NestedStruct(StructSphImapVector_21)

    struct StructSphImapVector_22 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_22 : 4;
        // NestedStruct(StructSphImapVector_22)

    struct StructSphImapVector_23 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_23 : 4;
        // NestedStruct(StructSphImapVector_23)

    struct StructSphImapVector_24 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_24 : 4;
        // NestedStruct(StructSphImapVector_24)

    struct StructSphImapVector_25 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_25 : 4;
        // NestedStruct(StructSphImapVector_25)

    struct StructSphImapVector_26 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_26 : 4;
        // NestedStruct(StructSphImapVector_26)

    struct StructSphImapVector_27 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_27 : 4;
        // NestedStruct(StructSphImapVector_27)

    struct StructSphImapVector_28 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_28 : 4;
        // NestedStruct(StructSphImapVector_28)

    struct StructSphImapVector_29 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_29 : 4;
        // NestedStruct(StructSphImapVector_29)

    struct StructSphImapVector_30 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_30 : 4;
        // NestedStruct(StructSphImapVector_30)

    struct StructSphImapVector_31 {
        unsigned long long ImapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapVector_31 : 4;
        // NestedStruct(StructSphImapVector_31)

    struct StructSphImapColor {
        unsigned long long ImapColorFrontDiffuseRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorFrontDiffuseGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorFrontDiffuseBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorFrontDiffuseAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorFrontSpelwlarRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorFrontSpelwlarGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorFrontSpelwlarBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorFrontSpelwlarAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackDiffuseRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackDiffuseGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackDiffuseBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackDiffuseAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackSpelwlarRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackSpelwlarGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackSpelwlarBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapColorBackSpelwlarAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapColor : 16;
    // NestedStruct(StructSphImapColor)

    struct StructSphImapSystemValuesC {
        unsigned long long ImapClipDistance0 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance1 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance2 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance3 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance4 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance5 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance6 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance7 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPointSpriteS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPointSpriteT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapFogCoordinate : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved17 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationEvaluationPointU : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationEvaluationPointV : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapInstanceId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapVertexId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapSystemValuesC : 16;
    // NestedStruct(StructSphImapSystemValuesC)

    struct StructSphImapTexture_0 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_0 : 4;
        // NestedStruct(StructSphImapTexture_0)

    struct StructSphImapTexture_1 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_1 : 4;
        // NestedStruct(StructSphImapTexture_1)

    struct StructSphImapTexture_2 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_2 : 4;
        // NestedStruct(StructSphImapTexture_2)

    struct StructSphImapTexture_3 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_3 : 4;
        // NestedStruct(StructSphImapTexture_3)

    struct StructSphImapTexture_4 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_4 : 4;
        // NestedStruct(StructSphImapTexture_4)

    struct StructSphImapTexture_5 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_5 : 4;
        // NestedStruct(StructSphImapTexture_5)

    struct StructSphImapTexture_6 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_6 : 4;
        // NestedStruct(StructSphImapTexture_6)

    struct StructSphImapTexture_7 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_7 : 4;
        // NestedStruct(StructSphImapTexture_7)

    struct StructSphImapTexture_8 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_8 : 4;
        // NestedStruct(StructSphImapTexture_8)

    struct StructSphImapTexture_9 {
        unsigned long long ImapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapTexture_9 : 4;
        // NestedStruct(StructSphImapTexture_9)

    struct StructSphImapReserved {
        unsigned long long ImapSystemValuesReserved18 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved19 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved20 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved21 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved22 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved23 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved24 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved25 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapReserved : 8;
    // NestedStruct(StructSphImapReserved)

    struct StructSphOmapSystemValuesA {
        unsigned long long OmapSystemValuesReserved28 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved29 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved30 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved31 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationLodLeft : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationLodRight : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationLodBottom : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationLodTop : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationInteriorU : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationInteriorV : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved00 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved01 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved02 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved03 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved04 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved05 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved06 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved07 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved08 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved09 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved10 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved11 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved12 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved13 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapSystemValuesA : 24;
    // NestedStruct(StructSphOmapSystemValuesA)

    struct StructSphOmapSystemValuesB {
        unsigned long long OmapPrimitiveId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapRtArrayIndex : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapViewportIndex : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapPointSize : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapPositionX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapPositionY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapPositionZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapPositionW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapSystemValuesB : 8;
    // NestedStruct(StructSphOmapSystemValuesB)

    struct StructSphOmapVector_0 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_0 : 4;
        // NestedStruct(StructSphOmapVector_0)

    struct StructSphOmapVector_1 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_1 : 4;
        // NestedStruct(StructSphOmapVector_1)

    struct StructSphOmapVector_2 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_2 : 4;
        // NestedStruct(StructSphOmapVector_2)

    struct StructSphOmapVector_3 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_3 : 4;
        // NestedStruct(StructSphOmapVector_3)

    struct StructSphOmapVector_4 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_4 : 4;
        // NestedStruct(StructSphOmapVector_4)

    struct StructSphOmapVector_5 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_5 : 4;
        // NestedStruct(StructSphOmapVector_5)

    struct StructSphOmapVector_6 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_6 : 4;
        // NestedStruct(StructSphOmapVector_6)

    struct StructSphOmapVector_7 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_7 : 4;
        // NestedStruct(StructSphOmapVector_7)

    struct StructSphOmapVector_8 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_8 : 4;
        // NestedStruct(StructSphOmapVector_8)

    struct StructSphOmapVector_9 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_9 : 4;
        // NestedStruct(StructSphOmapVector_9)

    struct StructSphOmapVector_10 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_10 : 4;
        // NestedStruct(StructSphOmapVector_10)

    struct StructSphOmapVector_11 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_11 : 4;
        // NestedStruct(StructSphOmapVector_11)

    struct StructSphOmapVector_12 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_12 : 4;
        // NestedStruct(StructSphOmapVector_12)

    struct StructSphOmapVector_13 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_13 : 4;
        // NestedStruct(StructSphOmapVector_13)

    struct StructSphOmapVector_14 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_14 : 4;
        // NestedStruct(StructSphOmapVector_14)

    struct StructSphOmapVector_15 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_15 : 4;
        // NestedStruct(StructSphOmapVector_15)

    struct StructSphOmapVector_16 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_16 : 4;
        // NestedStruct(StructSphOmapVector_16)

    struct StructSphOmapVector_17 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_17 : 4;
        // NestedStruct(StructSphOmapVector_17)

    struct StructSphOmapVector_18 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_18 : 4;
        // NestedStruct(StructSphOmapVector_18)

    struct StructSphOmapVector_19 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_19 : 4;
        // NestedStruct(StructSphOmapVector_19)

    struct StructSphOmapVector_20 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_20 : 4;
        // NestedStruct(StructSphOmapVector_20)

    struct StructSphOmapVector_21 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_21 : 4;
        // NestedStruct(StructSphOmapVector_21)

    struct StructSphOmapVector_22 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_22 : 4;
        // NestedStruct(StructSphOmapVector_22)

    struct StructSphOmapVector_23 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_23 : 4;
        // NestedStruct(StructSphOmapVector_23)

    struct StructSphOmapVector_24 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_24 : 4;
        // NestedStruct(StructSphOmapVector_24)

    struct StructSphOmapVector_25 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_25 : 4;
        // NestedStruct(StructSphOmapVector_25)

    struct StructSphOmapVector_26 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_26 : 4;
        // NestedStruct(StructSphOmapVector_26)

    struct StructSphOmapVector_27 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_27 : 4;
        // NestedStruct(StructSphOmapVector_27)

    struct StructSphOmapVector_28 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_28 : 4;
        // NestedStruct(StructSphOmapVector_28)

    struct StructSphOmapVector_29 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_29 : 4;
        // NestedStruct(StructSphOmapVector_29)

    struct StructSphOmapVector_30 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_30 : 4;
        // NestedStruct(StructSphOmapVector_30)

    struct StructSphOmapVector_31 {
        unsigned long long OmapX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapVector_31 : 4;
        // NestedStruct(StructSphOmapVector_31)

    struct StructSphOmapColor {
        unsigned long long OmapColorFrontDiffuseRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorFrontDiffuseGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorFrontDiffuseBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorFrontDiffuseAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorFrontSpelwlarRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorFrontSpelwlarGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorFrontSpelwlarBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorFrontSpelwlarAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackDiffuseRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackDiffuseGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackDiffuseBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackDiffuseAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackSpelwlarRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackSpelwlarGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackSpelwlarBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapColorBackSpelwlarAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapColor : 16;
    // NestedStruct(StructSphOmapColor)

    struct StructSphOmapSystemValuesC {
        unsigned long long OmapClipDistance0 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapClipDistance1 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapClipDistance2 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapClipDistance3 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapClipDistance4 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapClipDistance5 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapClipDistance6 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapClipDistance7 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapPointSpriteS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapPointSpriteT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapFogCoordinate : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved17 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationEvaluationPointU : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapTessellationEvaluationPointV : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapInstanceId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapVertexId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapSystemValuesC : 16;
    // NestedStruct(StructSphOmapSystemValuesC)

    struct StructSphOmapTexture_0 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_0 : 4;
        // NestedStruct(StructSphOmapTexture_0)

    struct StructSphOmapTexture_1 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_1 : 4;
        // NestedStruct(StructSphOmapTexture_1)

    struct StructSphOmapTexture_2 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_2 : 4;
        // NestedStruct(StructSphOmapTexture_2)

    struct StructSphOmapTexture_3 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_3 : 4;
        // NestedStruct(StructSphOmapTexture_3)

    struct StructSphOmapTexture_4 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_4 : 4;
        // NestedStruct(StructSphOmapTexture_4)

    struct StructSphOmapTexture_5 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_5 : 4;
        // NestedStruct(StructSphOmapTexture_5)

    struct StructSphOmapTexture_6 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_6 : 4;
        // NestedStruct(StructSphOmapTexture_6)

    struct StructSphOmapTexture_7 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_7 : 4;
        // NestedStruct(StructSphOmapTexture_7)

    struct StructSphOmapTexture_8 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_8 : 4;
        // NestedStruct(StructSphOmapTexture_8)

    struct StructSphOmapTexture_9 {
        unsigned long long OmapS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapR : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapQ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapTexture_9 : 4;
        // NestedStruct(StructSphOmapTexture_9)

    struct StructSphOmapReserved {
        unsigned long long OmapSystemValuesReserved18 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved19 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved20 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved21 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved22 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved23 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved24 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapSystemValuesReserved25 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphOmapReserved : 8;
    // NestedStruct(StructSphOmapReserved)
};

struct TextureHeaderStateVersion2 {

    struct TextureHeaderDword0 {
        unsigned long long componentSizes : 6;         // Enum(ComponentSizes)
        unsigned long long rDataType : 3;         // Enum(NumericalDataType)
        unsigned long long gDataType : 3;         // Enum(NumericalDataType)
        unsigned long long bDataType : 3;         // Enum(NumericalDataType)
        unsigned long long aDataType : 3;         // Enum(NumericalDataType)
        unsigned long long xSource : 3;         // Enum(XYZWSource)
        unsigned long long ySource : 3;         // Enum(XYZWSource)
        unsigned long long zSource : 3;         // Enum(XYZWSource)
        unsigned long long wSource : 3;         // Enum(XYZWSource)
        unsigned long long packComponents : 1;         // Simple
        unsigned long long reserved4 : 1;         // Simple
    };
    unsigned long long TextureHeaderDword0 : 32;
    // NestedStruct(TextureHeaderDword0)

    unsigned long long offsetLower : 32;         // Simple
    struct TextureHeaderDword2 {
        unsigned long long offsetUpper : 8;         // Simple
        unsigned long long anisoSpreadMaxLog2LSB : 2;         // Simple
        unsigned long long sRGBColwersion : 1;         // Simple
        unsigned long long anisoSpreadMaxLog2MSB : 1;         // Simple
        unsigned long long lodAnisoQuality2 : 1;         // Simple
        unsigned long long colorKeyOp : 1;         // Simple
        unsigned long long textureType : 4;         // Enum(TextureType)
        unsigned long long memoryLayout : 1;         // Enum(MemoryLayout)
        unsigned long long gobsPerBlockWidth : 3;         // Enum(GobsPerBlockWidth)
        unsigned long long gobsPerBlockHeight : 3;         // Enum(GobsPerBlock)
        unsigned long long gobsPerBlockDepth : 3;         // Enum(GobsPerBlock)
        unsigned long long sectorPromotion : 2;         // Enum(SectorPromotion)
        unsigned long long borderSource : 1;         // Enum(BorderSource)
        unsigned long long normalizedCoords : 1;         // Simple
    };
    unsigned long long TextureHeaderDword2 : 32;
    // NestedStruct(TextureHeaderDword2)

    struct TextureHeaderDword3 {
        unsigned long long pitch : 20;         // Simple
        unsigned long long lodAnisoQuality : 1;         // Enum(LodQuality)
        unsigned long long lodIsoQuality : 1;         // Enum(LodQuality)
        unsigned long long anisoCoarseSpreadModifier : 2;         // Enum(AnisoSpreadModifier)
        unsigned long long anisoSpreadScale : 5;         // Simple
        unsigned long long useHeaderOptControl : 1;         // Simple
        unsigned long long anisoClampAtMaxLod : 1;         // Simple
        unsigned long long anisoPow2 : 1;         // Simple
    };
    unsigned long long TextureHeaderDword3 : 32;
    // NestedStruct(TextureHeaderDword3)

    struct TextureHeaderDword4 {
        unsigned long long width : 30;         // Simple
        unsigned long long depthTexture : 1;         // Simple
        unsigned long long useTextureHeaderVersion2 : 1;         // Simple
    };
    unsigned long long TextureHeaderDword4 : 32;
    // NestedStruct(TextureHeaderDword4)

    struct TextureHeaderDword5 {
        unsigned long long height : 16;         // Simple
        unsigned long long depth : 12;         // Simple
        unsigned long long maxMipLevel : 4;         // Simple
    };
    unsigned long long TextureHeaderDword5 : 32;
    // NestedStruct(TextureHeaderDword5)

    struct TextureHeaderDword6 {
        unsigned long long trilinOpt : 5;         // Simple
        unsigned long long mipLodBias : 13;         // Simple
        unsigned long long anisoRoundDown : 1;         // Simple
        unsigned long long anisoBias : 4;         // Simple
        unsigned long long anisoFineSpreadFunc : 2;         // Enum(AnisoSpreadFunc)
        unsigned long long anisoCoarseSpreadFunc : 2;         // Enum(AnisoSpreadFunc)
        unsigned long long maxAnisotropy : 3;         // Enum(MaxAnisotropy)
        unsigned long long anisoFineSpreadModifier : 2;         // Enum(AnisoSpreadModifier)
    };
    unsigned long long TextureHeaderDword6 : 32;
    // NestedStruct(TextureHeaderDword6)

    struct TextureHeaderDword7_Version2 {
        unsigned long long resViewMinMipLevel : 4;         // Simple
        unsigned long long resViewMaxMipLevel : 4;         // Simple
        unsigned long long heightMsb : 1;         // Simple
        unsigned long long heightMsbReserved : 3;         // Simple
        unsigned long long multiSampleCount : 4;         // Enum(TYPEDEF_SAMPLE_MODES)
        unsigned long long minLodClamp : 12;         // Simple
        unsigned long long reserved7a : 4;         // Simple
    };
    unsigned long long TextureHeaderDword7_Version2 : 32;
    // NestedStruct(TextureHeaderDword7_Version2)
};

struct StructSphType02Version03 {

    struct StructSphCommonWord0 {
        unsigned long long SphType : 5;         // Enum(TYPEDEF_SPH_TYPE)
        unsigned long long Version : 5;         // Simple
        unsigned long long ShaderType : 4;         // Enum(TYPEDEF_SHADER_TYPE)
        unsigned long long MrtEnable : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long KillsPixels : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long DoesGlobalStore : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long SassVersion : 4;         // Simple
        unsigned long long ReservedCommonA_0 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_1 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_2 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_3 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonA_4 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long DoesLoadOrStore : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long DoesFp64 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long StreamOutMask : 4;         // Simple
    };
    unsigned long long StructSphCommonWord0 : 32;
    // NestedStruct(StructSphCommonWord0)

    struct StructSphCommonWord1 {
        unsigned long long ShaderLocalMemoryLowSize : 24;         // Simple
        unsigned long long PerPatchAttributeCount : 8;         // Simple
    };
    unsigned long long StructSphCommonWord1 : 32;
    // NestedStruct(StructSphCommonWord1)

    struct StructSphCommonWord2 {
        unsigned long long ShaderLocalMemoryHighSize : 24;         // Simple
        unsigned long long ThreadsPerInputPrimitive : 8;         // Simple
    };
    unsigned long long StructSphCommonWord2 : 32;
    // NestedStruct(StructSphCommonWord2)

    struct StructSphCommonWord3 {
        unsigned long long ShaderLocalMemoryCrsSize : 24;         // Simple
        unsigned long long OutputTopology : 4;         // Enum(TYPEDEF_SPH_OUTPUT_TOPOLOGY)
        unsigned long long ReservedCommonB : 4;         // Simple
    };
    unsigned long long StructSphCommonWord3 : 32;
    // NestedStruct(StructSphCommonWord3)

    struct StructSphCommonWord4 {
        unsigned long long MaxOutputVertexCount : 12;         // Simple
        unsigned long long StoreReqStart : 8;         // Simple
        unsigned long long ReservedCommonC_0 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonC_1 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonC_2 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ReservedCommonC_3 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long StoreReqEnd : 8;         // Simple
    };
    unsigned long long StructSphCommonWord4 : 32;
    // NestedStruct(StructSphCommonWord4)

    struct StructSphImapSystemValuesA {
        unsigned long long ImapSystemValuesReserved28 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved29 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved30 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved31 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodLeft : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodRight : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodBottom : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationLodTop : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationInteriorU : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationInteriorV : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved00 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved01 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved02 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved03 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved04 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved05 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved06 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved07 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved08 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved09 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved10 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved11 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved12 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved13 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapSystemValuesA : 24;
    // NestedStruct(StructSphImapSystemValuesA)

    struct StructSphImapSystemValuesB {
        unsigned long long ImapPrimitiveId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapRtArrayIndex : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapViewportIndex : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPointSize : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionX : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionY : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionZ : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPositionW : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapSystemValuesB : 8;
    // NestedStruct(StructSphImapSystemValuesB)

    struct StructSphImapPixelVector_0 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_0 : 8;
        // NestedStruct(StructSphImapPixelVector_0)

    struct StructSphImapPixelVector_1 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_1 : 8;
        // NestedStruct(StructSphImapPixelVector_1)

    struct StructSphImapPixelVector_2 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_2 : 8;
        // NestedStruct(StructSphImapPixelVector_2)

    struct StructSphImapPixelVector_3 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_3 : 8;
        // NestedStruct(StructSphImapPixelVector_3)

    struct StructSphImapPixelVector_4 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_4 : 8;
        // NestedStruct(StructSphImapPixelVector_4)

    struct StructSphImapPixelVector_5 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_5 : 8;
        // NestedStruct(StructSphImapPixelVector_5)

    struct StructSphImapPixelVector_6 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_6 : 8;
        // NestedStruct(StructSphImapPixelVector_6)

    struct StructSphImapPixelVector_7 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_7 : 8;
        // NestedStruct(StructSphImapPixelVector_7)

    struct StructSphImapPixelVector_8 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_8 : 8;
        // NestedStruct(StructSphImapPixelVector_8)

    struct StructSphImapPixelVector_9 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_9 : 8;
        // NestedStruct(StructSphImapPixelVector_9)

    struct StructSphImapPixelVector_10 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_10 : 8;
        // NestedStruct(StructSphImapPixelVector_10)

    struct StructSphImapPixelVector_11 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_11 : 8;
        // NestedStruct(StructSphImapPixelVector_11)

    struct StructSphImapPixelVector_12 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_12 : 8;
        // NestedStruct(StructSphImapPixelVector_12)

    struct StructSphImapPixelVector_13 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_13 : 8;
        // NestedStruct(StructSphImapPixelVector_13)

    struct StructSphImapPixelVector_14 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_14 : 8;
        // NestedStruct(StructSphImapPixelVector_14)

    struct StructSphImapPixelVector_15 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_15 : 8;
        // NestedStruct(StructSphImapPixelVector_15)

    struct StructSphImapPixelVector_16 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_16 : 8;
        // NestedStruct(StructSphImapPixelVector_16)

    struct StructSphImapPixelVector_17 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_17 : 8;
        // NestedStruct(StructSphImapPixelVector_17)

    struct StructSphImapPixelVector_18 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_18 : 8;
        // NestedStruct(StructSphImapPixelVector_18)

    struct StructSphImapPixelVector_19 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_19 : 8;
        // NestedStruct(StructSphImapPixelVector_19)

    struct StructSphImapPixelVector_20 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_20 : 8;
        // NestedStruct(StructSphImapPixelVector_20)

    struct StructSphImapPixelVector_21 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_21 : 8;
        // NestedStruct(StructSphImapPixelVector_21)

    struct StructSphImapPixelVector_22 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_22 : 8;
        // NestedStruct(StructSphImapPixelVector_22)

    struct StructSphImapPixelVector_23 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_23 : 8;
        // NestedStruct(StructSphImapPixelVector_23)

    struct StructSphImapPixelVector_24 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_24 : 8;
        // NestedStruct(StructSphImapPixelVector_24)

    struct StructSphImapPixelVector_25 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_25 : 8;
        // NestedStruct(StructSphImapPixelVector_25)

    struct StructSphImapPixelVector_26 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_26 : 8;
        // NestedStruct(StructSphImapPixelVector_26)

    struct StructSphImapPixelVector_27 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_27 : 8;
        // NestedStruct(StructSphImapPixelVector_27)

    struct StructSphImapPixelVector_28 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_28 : 8;
        // NestedStruct(StructSphImapPixelVector_28)

    struct StructSphImapPixelVector_29 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_29 : 8;
        // NestedStruct(StructSphImapPixelVector_29)

    struct StructSphImapPixelVector_30 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_30 : 8;
        // NestedStruct(StructSphImapPixelVector_30)

    struct StructSphImapPixelVector_31 {
        unsigned long long ImapX : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapY : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapZ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapW : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelVector_31 : 8;
        // NestedStruct(StructSphImapPixelVector_31)

    struct StructSphPixelImapColor {
        unsigned long long ImapColorDiffuseRed : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapColorDiffuseGreen : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapColorDiffuseBlue : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapColorDiffuseAlpha : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapColorSpelwlarRed : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapColorSpelwlarGreen : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapColorSpelwlarBlue : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapColorSpelwlarAlpha : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphPixelImapColor : 16;
    // NestedStruct(StructSphPixelImapColor)

    struct StructSphImapSystemValuesC {
        unsigned long long ImapClipDistance0 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance1 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance2 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance3 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance4 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance5 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance6 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapClipDistance7 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPointSpriteS : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapPointSpriteT : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapFogCoordinate : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapSystemValuesReserved17 : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationEvaluationPointU : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapTessellationEvaluationPointV : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapInstanceId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long ImapVertexId : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphImapSystemValuesC : 16;
    // NestedStruct(StructSphImapSystemValuesC)

    struct StructSphImapPixelTexture_0 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_0 : 8;
        // NestedStruct(StructSphImapPixelTexture_0)

    struct StructSphImapPixelTexture_1 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_1 : 8;
        // NestedStruct(StructSphImapPixelTexture_1)

    struct StructSphImapPixelTexture_2 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_2 : 8;
        // NestedStruct(StructSphImapPixelTexture_2)

    struct StructSphImapPixelTexture_3 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_3 : 8;
        // NestedStruct(StructSphImapPixelTexture_3)

    struct StructSphImapPixelTexture_4 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_4 : 8;
        // NestedStruct(StructSphImapPixelTexture_4)

    struct StructSphImapPixelTexture_5 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_5 : 8;
        // NestedStruct(StructSphImapPixelTexture_5)

    struct StructSphImapPixelTexture_6 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_6 : 8;
        // NestedStruct(StructSphImapPixelTexture_6)

    struct StructSphImapPixelTexture_7 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_7 : 8;
        // NestedStruct(StructSphImapPixelTexture_7)

    struct StructSphImapPixelTexture_8 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_8 : 8;
        // NestedStruct(StructSphImapPixelTexture_8)

    struct StructSphImapPixelTexture_9 {
        unsigned long long ImapS : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapT : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapR : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapQ : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphImapPixelTexture_9 : 8;
        // NestedStruct(StructSphImapPixelTexture_9)

    struct StructSphPixelImapReserved {
        unsigned long long ImapSystemValuesReserved18 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapSystemValuesReserved19 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapSystemValuesReserved20 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapSystemValuesReserved21 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapSystemValuesReserved22 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapSystemValuesReserved23 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapSystemValuesReserved24 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
        unsigned long long ImapSystemValuesReserved25 : 2;         // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    };
    unsigned long long StructSphPixelImapReserved : 16;
    // NestedStruct(StructSphPixelImapReserved)

    struct StructSphTarget_0 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_0 : 4;
        // NestedStruct(StructSphTarget_0)

    struct StructSphTarget_1 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_1 : 4;
        // NestedStruct(StructSphTarget_1)

    struct StructSphTarget_2 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_2 : 4;
        // NestedStruct(StructSphTarget_2)

    struct StructSphTarget_3 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_3 : 4;
        // NestedStruct(StructSphTarget_3)

    struct StructSphTarget_4 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_4 : 4;
        // NestedStruct(StructSphTarget_4)

    struct StructSphTarget_5 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_5 : 4;
        // NestedStruct(StructSphTarget_5)

    struct StructSphTarget_6 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_6 : 4;
        // NestedStruct(StructSphTarget_6)

    struct StructSphTarget_7 {
        unsigned long long OmapRed : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapGreen : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapBlue : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
        unsigned long long OmapAlpha : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    };
    unsigned long long StructSphTarget_7 : 4;
        // NestedStruct(StructSphTarget_7)

    unsigned long long OmapSampleMask : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapDepth : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long Reserved_0 : 1;         // Simple
    unsigned long long Reserved_1 : 1;         // Simple
    unsigned long long Reserved_2 : 1;         // Simple
    unsigned long long Reserved_3 : 1;         // Simple
    unsigned long long Reserved_4 : 1;         // Simple
    unsigned long long Reserved_5 : 1;         // Simple
    unsigned long long Reserved_6 : 1;         // Simple
    unsigned long long Reserved_7 : 1;         // Simple
    unsigned long long Reserved_8 : 1;         // Simple
    unsigned long long Reserved_9 : 1;         // Simple
    unsigned long long Reserved_10 : 1;         // Simple
    unsigned long long Reserved_11 : 1;         // Simple
    unsigned long long Reserved_12 : 1;         // Simple
    unsigned long long Reserved_13 : 1;         // Simple
    unsigned long long Reserved_14 : 1;         // Simple
    unsigned long long Reserved_15 : 1;         // Simple
    unsigned long long Reserved_16 : 1;         // Simple
    unsigned long long Reserved_17 : 1;         // Simple
    unsigned long long Reserved_18 : 1;         // Simple
    unsigned long long Reserved_19 : 1;         // Simple
    unsigned long long Reserved_20 : 1;         // Simple
    unsigned long long Reserved_21 : 1;         // Simple
    unsigned long long Reserved_22 : 1;         // Simple
    unsigned long long Reserved_23 : 1;         // Simple
    unsigned long long Reserved_24 : 1;         // Simple
    unsigned long long Reserved_25 : 1;         // Simple
    unsigned long long Reserved_26 : 1;         // Simple
    unsigned long long Reserved_27 : 1;         // Simple
    unsigned long long Reserved_28 : 1;         // Simple
    unsigned long long Reserved_29 : 1;         // Simple
};

struct TextureSamplerState {

    struct TextureSamplerDword0 {
        unsigned long long addressU : 3;         // Enum(AddressMode)
        unsigned long long addressV : 3;         // Enum(AddressMode)
        unsigned long long addressP : 3;         // Enum(AddressMode)
        unsigned long long depthCompare : 1;         // Simple
        unsigned long long depthCompareFunc : 3;         // Enum(DepthCompareFunc)
        unsigned long long sRGBColwersion : 1;         // Simple
        unsigned long long fontFilterWidth : 3;         // Enum(FontFilterSize)
        unsigned long long fontFilterHeight : 3;         // Enum(FontFilterSize)
        unsigned long long maxAnisotropy : 3;         // Enum(MaxAnisotropy)
        unsigned long long padding_dword0 : 9;         // Simple
    };
    unsigned long long TextureSamplerDword0 : 32;
    // NestedStruct(TextureSamplerDword0)

    struct TextureSamplerDword1 {
        unsigned long long magFilter : 3;         // Enum(MagFilter)
        unsigned long long padding_dword1_0 : 1;         // Simple
        unsigned long long minFilter : 2;         // Enum(MinFilter)
        unsigned long long mipFilter : 2;         // Enum(MipFilter)
        unsigned long long reserved : 2;         // Simple
        unsigned long long padding_dword1_1 : 2;         // Simple
        unsigned long long mipLodBias : 13;         // Simple
        unsigned long long padding_dword1_3 : 1;         // Simple
        unsigned long long trilinOpt : 5;         // Simple
        unsigned long long padding_dword1_2 : 1;         // Simple
    };
    unsigned long long TextureSamplerDword1 : 32;
    // NestedStruct(TextureSamplerDword1)

    struct TextureSamplerDword2 {
        unsigned long long minLodClamp : 12;         // Simple
        unsigned long long maxLodClamp : 12;         // Simple
        unsigned long long sRGBBorderColorR : 8;         // Simple
    };
    unsigned long long TextureSamplerDword2 : 32;
    // NestedStruct(TextureSamplerDword2)

    struct TextureSamplerDword3 {
        unsigned long long reserved12 : 12;         // Simple
        unsigned long long sRGBBorderColorG : 8;         // Simple
        unsigned long long sRGBBorderColorB : 8;         // Simple
        unsigned long long padding_dword3 : 4;         // Simple
    };
    unsigned long long TextureSamplerDword3 : 32;
    // NestedStruct(TextureSamplerDword3)

    unsigned long long borderColorR : 32;         // Simple
    unsigned long long borderColorG : 32;         // Simple
    unsigned long long borderColorB : 32;         // Simple
    unsigned long long borderColorA : 32;         // Simple
};

struct TextureHeaderState {

    struct TextureHeaderDword0 {
        unsigned long long componentSizes : 6;         // Enum(ComponentSizes)
        unsigned long long rDataType : 3;         // Enum(NumericalDataType)
        unsigned long long gDataType : 3;         // Enum(NumericalDataType)
        unsigned long long bDataType : 3;         // Enum(NumericalDataType)
        unsigned long long aDataType : 3;         // Enum(NumericalDataType)
        unsigned long long xSource : 3;         // Enum(XYZWSource)
        unsigned long long ySource : 3;         // Enum(XYZWSource)
        unsigned long long zSource : 3;         // Enum(XYZWSource)
        unsigned long long wSource : 3;         // Enum(XYZWSource)
        unsigned long long packComponents : 1;         // Simple
        unsigned long long reserved4 : 1;         // Simple
    };
    unsigned long long TextureHeaderDword0 : 32;
    // NestedStruct(TextureHeaderDword0)

    unsigned long long offsetLower : 32;         // Simple
    struct TextureHeaderDword2 {
        unsigned long long offsetUpper : 8;         // Simple
        unsigned long long anisoSpreadMaxLog2LSB : 2;         // Simple
        unsigned long long sRGBColwersion : 1;         // Simple
        unsigned long long anisoSpreadMaxLog2MSB : 1;         // Simple
        unsigned long long lodAnisoQuality2 : 1;         // Simple
        unsigned long long colorKeyOp : 1;         // Simple
        unsigned long long textureType : 4;         // Enum(TextureType)
        unsigned long long memoryLayout : 1;         // Enum(MemoryLayout)
        unsigned long long gobsPerBlockWidth : 3;         // Enum(GobsPerBlockWidth)
        unsigned long long gobsPerBlockHeight : 3;         // Enum(GobsPerBlock)
        unsigned long long gobsPerBlockDepth : 3;         // Enum(GobsPerBlock)
        unsigned long long sectorPromotion : 2;         // Enum(SectorPromotion)
        unsigned long long borderSource : 1;         // Enum(BorderSource)
        unsigned long long normalizedCoords : 1;         // Simple
    };
    unsigned long long TextureHeaderDword2 : 32;
    // NestedStruct(TextureHeaderDword2)

    struct TextureHeaderDword3 {
        unsigned long long pitch : 20;         // Simple
        unsigned long long lodAnisoQuality : 1;         // Enum(LodQuality)
        unsigned long long lodIsoQuality : 1;         // Enum(LodQuality)
        unsigned long long anisoCoarseSpreadModifier : 2;         // Enum(AnisoSpreadModifier)
        unsigned long long anisoSpreadScale : 5;         // Simple
        unsigned long long useHeaderOptControl : 1;         // Simple
        unsigned long long anisoClampAtMaxLod : 1;         // Simple
        unsigned long long anisoPow2 : 1;         // Simple
    };
    unsigned long long TextureHeaderDword3 : 32;
    // NestedStruct(TextureHeaderDword3)

    struct TextureHeaderDword4 {
        unsigned long long width : 30;         // Simple
        unsigned long long depthTexture : 1;         // Simple
        unsigned long long useTextureHeaderVersion2 : 1;         // Simple
    };
    unsigned long long TextureHeaderDword4 : 32;
    // NestedStruct(TextureHeaderDword4)

    struct TextureHeaderDword5 {
        unsigned long long height : 16;         // Simple
        unsigned long long depth : 12;         // Simple
        unsigned long long maxMipLevel : 4;         // Simple
    };
    unsigned long long TextureHeaderDword5 : 32;
    // NestedStruct(TextureHeaderDword5)

    struct TextureHeaderDword6 {
        unsigned long long trilinOpt : 5;         // Simple
        unsigned long long mipLodBias : 13;         // Simple
        unsigned long long anisoRoundDown : 1;         // Simple
        unsigned long long anisoBias : 4;         // Simple
        unsigned long long anisoFineSpreadFunc : 2;         // Enum(AnisoSpreadFunc)
        unsigned long long anisoCoarseSpreadFunc : 2;         // Enum(AnisoSpreadFunc)
        unsigned long long maxAnisotropy : 3;         // Enum(MaxAnisotropy)
        unsigned long long anisoFineSpreadModifier : 2;         // Enum(AnisoSpreadModifier)
    };
    unsigned long long TextureHeaderDword6 : 32;
    // NestedStruct(TextureHeaderDword6)

    unsigned long long colorKeyValue : 32;         // Simple
};

struct ShaderIoSlotsAamVersion02Flat {

    unsigned long long Unused : 8;         // Enum(TYPEDEF_SHADER_IO_SLOTS_AAM_VERSION_02)
};

struct StructSphType01Version03Flat {

    // Start fields of StructSphCommonWord0
    unsigned long long SphType : 5;     // Enum(TYPEDEF_SPH_TYPE)
    unsigned long long Version : 5;     // Simple
    unsigned long long ShaderType : 4;     // Enum(TYPEDEF_SHADER_TYPE)
    unsigned long long MrtEnable : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long KillsPixels : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long DoesGlobalStore : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long SassVersion : 4;     // Simple
    unsigned long long ReservedCommonA_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long DoesLoadOrStore : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long DoesFp64 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long StreamOutMask : 4;     // Simple
    // End fields of StructSphCommonWord0
    // NestedStruct(StructSphCommonWord0)

    // Start fields of StructSphCommonWord1
    unsigned long long ShaderLocalMemoryLowSize : 24;     // Simple
    unsigned long long PerPatchAttributeCount : 8;     // Simple
    // End fields of StructSphCommonWord1
    // NestedStruct(StructSphCommonWord1)

    // Start fields of StructSphCommonWord2
    unsigned long long ShaderLocalMemoryHighSize : 24;     // Simple
    unsigned long long ThreadsPerInputPrimitive : 8;     // Simple
    // End fields of StructSphCommonWord2
    // NestedStruct(StructSphCommonWord2)

    // Start fields of StructSphCommonWord3
    unsigned long long ShaderLocalMemoryCrsSize : 24;     // Simple
    unsigned long long OutputTopology : 4;     // Enum(TYPEDEF_SPH_OUTPUT_TOPOLOGY)
    unsigned long long ReservedCommonB : 4;     // Simple
    // End fields of StructSphCommonWord3
    // NestedStruct(StructSphCommonWord3)

    // Start fields of StructSphCommonWord4
    unsigned long long MaxOutputVertexCount : 12;     // Simple
    unsigned long long StoreReqStart : 8;     // Simple
    unsigned long long ReservedCommonC_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonC_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonC_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonC_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long StoreReqEnd : 8;     // Simple
    // End fields of StructSphCommonWord4
    // NestedStruct(StructSphCommonWord4)

    // Start fields of StructSphImapSystemValuesA
    unsigned long long ImapSystemValuesReserved28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodLeft : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodRight : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodBottom : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodTop : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationInteriorU : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationInteriorV : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved00 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved01 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved02 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved03 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved04 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved05 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved06 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved07 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved08 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved09 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapSystemValuesA
    // NestedStruct(StructSphImapSystemValuesA)

    // Start fields of StructSphImapSystemValuesB
    unsigned long long ImapPrimitiveId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapRtArrayIndex : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapViewportIndex : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPointSize : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionX : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionY : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionZ : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionW : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapSystemValuesB
    // NestedStruct(StructSphImapSystemValuesB)

    // Start fields of StructSphImapVector_0
    unsigned long long ImapX_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_0
        // NestedStruct(StructSphImapVector_0)

    // Start fields of StructSphImapVector_1
    unsigned long long ImapX_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_1
        // NestedStruct(StructSphImapVector_1)

    // Start fields of StructSphImapVector_2
    unsigned long long ImapX_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_2
        // NestedStruct(StructSphImapVector_2)

    // Start fields of StructSphImapVector_3
    unsigned long long ImapX_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_3
        // NestedStruct(StructSphImapVector_3)

    // Start fields of StructSphImapVector_4
    unsigned long long ImapX_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_4
        // NestedStruct(StructSphImapVector_4)

    // Start fields of StructSphImapVector_5
    unsigned long long ImapX_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_5
        // NestedStruct(StructSphImapVector_5)

    // Start fields of StructSphImapVector_6
    unsigned long long ImapX_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_6
        // NestedStruct(StructSphImapVector_6)

    // Start fields of StructSphImapVector_7
    unsigned long long ImapX_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_7
        // NestedStruct(StructSphImapVector_7)

    // Start fields of StructSphImapVector_8
    unsigned long long ImapX_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_8
        // NestedStruct(StructSphImapVector_8)

    // Start fields of StructSphImapVector_9
    unsigned long long ImapX_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_9
        // NestedStruct(StructSphImapVector_9)

    // Start fields of StructSphImapVector_10
    unsigned long long ImapX_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_10
        // NestedStruct(StructSphImapVector_10)

    // Start fields of StructSphImapVector_11
    unsigned long long ImapX_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_11
        // NestedStruct(StructSphImapVector_11)

    // Start fields of StructSphImapVector_12
    unsigned long long ImapX_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_12
        // NestedStruct(StructSphImapVector_12)

    // Start fields of StructSphImapVector_13
    unsigned long long ImapX_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_13
        // NestedStruct(StructSphImapVector_13)

    // Start fields of StructSphImapVector_14
    unsigned long long ImapX_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_14
        // NestedStruct(StructSphImapVector_14)

    // Start fields of StructSphImapVector_15
    unsigned long long ImapX_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_15
        // NestedStruct(StructSphImapVector_15)

    // Start fields of StructSphImapVector_16
    unsigned long long ImapX_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_16
        // NestedStruct(StructSphImapVector_16)

    // Start fields of StructSphImapVector_17
    unsigned long long ImapX_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_17
        // NestedStruct(StructSphImapVector_17)

    // Start fields of StructSphImapVector_18
    unsigned long long ImapX_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_18
        // NestedStruct(StructSphImapVector_18)

    // Start fields of StructSphImapVector_19
    unsigned long long ImapX_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_19
        // NestedStruct(StructSphImapVector_19)

    // Start fields of StructSphImapVector_20
    unsigned long long ImapX_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_20
        // NestedStruct(StructSphImapVector_20)

    // Start fields of StructSphImapVector_21
    unsigned long long ImapX_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_21
        // NestedStruct(StructSphImapVector_21)

    // Start fields of StructSphImapVector_22
    unsigned long long ImapX_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_22
        // NestedStruct(StructSphImapVector_22)

    // Start fields of StructSphImapVector_23
    unsigned long long ImapX_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_23
        // NestedStruct(StructSphImapVector_23)

    // Start fields of StructSphImapVector_24
    unsigned long long ImapX_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_24
        // NestedStruct(StructSphImapVector_24)

    // Start fields of StructSphImapVector_25
    unsigned long long ImapX_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_25
        // NestedStruct(StructSphImapVector_25)

    // Start fields of StructSphImapVector_26
    unsigned long long ImapX_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_26
        // NestedStruct(StructSphImapVector_26)

    // Start fields of StructSphImapVector_27
    unsigned long long ImapX_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_27
        // NestedStruct(StructSphImapVector_27)

    // Start fields of StructSphImapVector_28
    unsigned long long ImapX_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_28
        // NestedStruct(StructSphImapVector_28)

    // Start fields of StructSphImapVector_29
    unsigned long long ImapX_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_29
        // NestedStruct(StructSphImapVector_29)

    // Start fields of StructSphImapVector_30
    unsigned long long ImapX_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_30
        // NestedStruct(StructSphImapVector_30)

    // Start fields of StructSphImapVector_31
    unsigned long long ImapX_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapY_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapZ_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapW_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapVector_31
        // NestedStruct(StructSphImapVector_31)

    // Start fields of StructSphImapColor
    unsigned long long ImapColorFrontDiffuseRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorFrontDiffuseGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorFrontDiffuseBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorFrontDiffuseAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorFrontSpelwlarRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorFrontSpelwlarGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorFrontSpelwlarBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorFrontSpelwlarAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackDiffuseRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackDiffuseGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackDiffuseBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackDiffuseAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackSpelwlarRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackSpelwlarGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackSpelwlarBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapColorBackSpelwlarAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapColor
    // NestedStruct(StructSphImapColor)

    // Start fields of StructSphImapSystemValuesC
    unsigned long long ImapClipDistance0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPointSpriteS : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPointSpriteT : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapFogCoordinate : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationEvaluationPointU : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationEvaluationPointV : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapInstanceId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapVertexId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapSystemValuesC
    // NestedStruct(StructSphImapSystemValuesC)

    // Start fields of StructSphImapTexture_0
    unsigned long long ImapS_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_0
        // NestedStruct(StructSphImapTexture_0)

    // Start fields of StructSphImapTexture_1
    unsigned long long ImapS_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_1
        // NestedStruct(StructSphImapTexture_1)

    // Start fields of StructSphImapTexture_2
    unsigned long long ImapS_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_2
        // NestedStruct(StructSphImapTexture_2)

    // Start fields of StructSphImapTexture_3
    unsigned long long ImapS_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_3
        // NestedStruct(StructSphImapTexture_3)

    // Start fields of StructSphImapTexture_4
    unsigned long long ImapS_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_4
        // NestedStruct(StructSphImapTexture_4)

    // Start fields of StructSphImapTexture_5
    unsigned long long ImapS_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_5
        // NestedStruct(StructSphImapTexture_5)

    // Start fields of StructSphImapTexture_6
    unsigned long long ImapS_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_6
        // NestedStruct(StructSphImapTexture_6)

    // Start fields of StructSphImapTexture_7
    unsigned long long ImapS_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_7
        // NestedStruct(StructSphImapTexture_7)

    // Start fields of StructSphImapTexture_8
    unsigned long long ImapS_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_8
        // NestedStruct(StructSphImapTexture_8)

    // Start fields of StructSphImapTexture_9
    unsigned long long ImapS_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapT_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapR_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapQ_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapTexture_9
        // NestedStruct(StructSphImapTexture_9)

    // Start fields of StructSphImapReserved
    unsigned long long ImapSystemValuesReserved18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapReserved
    // NestedStruct(StructSphImapReserved)

    // Start fields of StructSphOmapSystemValuesA
    unsigned long long OmapSystemValuesReserved28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationLodLeft : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationLodRight : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationLodBottom : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationLodTop : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationInteriorU : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationInteriorV : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved00 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved01 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved02 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved03 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved04 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved05 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved06 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved07 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved08 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved09 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapSystemValuesA
    // NestedStruct(StructSphOmapSystemValuesA)

    // Start fields of StructSphOmapSystemValuesB
    unsigned long long OmapPrimitiveId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapRtArrayIndex : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapViewportIndex : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapPointSize : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapPositionX : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapPositionY : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapPositionZ : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapPositionW : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapSystemValuesB
    // NestedStruct(StructSphOmapSystemValuesB)

    // Start fields of StructSphOmapVector_0
    unsigned long long OmapX_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_0
        // NestedStruct(StructSphOmapVector_0)

    // Start fields of StructSphOmapVector_1
    unsigned long long OmapX_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_1
        // NestedStruct(StructSphOmapVector_1)

    // Start fields of StructSphOmapVector_2
    unsigned long long OmapX_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_2
        // NestedStruct(StructSphOmapVector_2)

    // Start fields of StructSphOmapVector_3
    unsigned long long OmapX_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_3
        // NestedStruct(StructSphOmapVector_3)

    // Start fields of StructSphOmapVector_4
    unsigned long long OmapX_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_4
        // NestedStruct(StructSphOmapVector_4)

    // Start fields of StructSphOmapVector_5
    unsigned long long OmapX_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_5
        // NestedStruct(StructSphOmapVector_5)

    // Start fields of StructSphOmapVector_6
    unsigned long long OmapX_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_6
        // NestedStruct(StructSphOmapVector_6)

    // Start fields of StructSphOmapVector_7
    unsigned long long OmapX_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_7
        // NestedStruct(StructSphOmapVector_7)

    // Start fields of StructSphOmapVector_8
    unsigned long long OmapX_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_8
        // NestedStruct(StructSphOmapVector_8)

    // Start fields of StructSphOmapVector_9
    unsigned long long OmapX_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_9
        // NestedStruct(StructSphOmapVector_9)

    // Start fields of StructSphOmapVector_10
    unsigned long long OmapX_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_10
        // NestedStruct(StructSphOmapVector_10)

    // Start fields of StructSphOmapVector_11
    unsigned long long OmapX_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_11
        // NestedStruct(StructSphOmapVector_11)

    // Start fields of StructSphOmapVector_12
    unsigned long long OmapX_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_12
        // NestedStruct(StructSphOmapVector_12)

    // Start fields of StructSphOmapVector_13
    unsigned long long OmapX_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_13
        // NestedStruct(StructSphOmapVector_13)

    // Start fields of StructSphOmapVector_14
    unsigned long long OmapX_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_14 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_14
        // NestedStruct(StructSphOmapVector_14)

    // Start fields of StructSphOmapVector_15
    unsigned long long OmapX_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_15 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_15
        // NestedStruct(StructSphOmapVector_15)

    // Start fields of StructSphOmapVector_16
    unsigned long long OmapX_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_16 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_16
        // NestedStruct(StructSphOmapVector_16)

    // Start fields of StructSphOmapVector_17
    unsigned long long OmapX_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_17
        // NestedStruct(StructSphOmapVector_17)

    // Start fields of StructSphOmapVector_18
    unsigned long long OmapX_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_18
        // NestedStruct(StructSphOmapVector_18)

    // Start fields of StructSphOmapVector_19
    unsigned long long OmapX_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_19
        // NestedStruct(StructSphOmapVector_19)

    // Start fields of StructSphOmapVector_20
    unsigned long long OmapX_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_20
        // NestedStruct(StructSphOmapVector_20)

    // Start fields of StructSphOmapVector_21
    unsigned long long OmapX_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_21
        // NestedStruct(StructSphOmapVector_21)

    // Start fields of StructSphOmapVector_22
    unsigned long long OmapX_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_22
        // NestedStruct(StructSphOmapVector_22)

    // Start fields of StructSphOmapVector_23
    unsigned long long OmapX_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_23
        // NestedStruct(StructSphOmapVector_23)

    // Start fields of StructSphOmapVector_24
    unsigned long long OmapX_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_24
        // NestedStruct(StructSphOmapVector_24)

    // Start fields of StructSphOmapVector_25
    unsigned long long OmapX_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_25
        // NestedStruct(StructSphOmapVector_25)

    // Start fields of StructSphOmapVector_26
    unsigned long long OmapX_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_26 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_26
        // NestedStruct(StructSphOmapVector_26)

    // Start fields of StructSphOmapVector_27
    unsigned long long OmapX_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_27 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_27
        // NestedStruct(StructSphOmapVector_27)

    // Start fields of StructSphOmapVector_28
    unsigned long long OmapX_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_28
        // NestedStruct(StructSphOmapVector_28)

    // Start fields of StructSphOmapVector_29
    unsigned long long OmapX_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_29
        // NestedStruct(StructSphOmapVector_29)

    // Start fields of StructSphOmapVector_30
    unsigned long long OmapX_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_30
        // NestedStruct(StructSphOmapVector_30)

    // Start fields of StructSphOmapVector_31
    unsigned long long OmapX_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapY_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapZ_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapW_31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapVector_31
        // NestedStruct(StructSphOmapVector_31)

    // Start fields of StructSphOmapColor
    unsigned long long OmapColorFrontDiffuseRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorFrontDiffuseGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorFrontDiffuseBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorFrontDiffuseAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorFrontSpelwlarRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorFrontSpelwlarGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorFrontSpelwlarBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorFrontSpelwlarAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackDiffuseRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackDiffuseGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackDiffuseBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackDiffuseAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackSpelwlarRed : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackSpelwlarGreen : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackSpelwlarBlue : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapColorBackSpelwlarAlpha : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapColor
    // NestedStruct(StructSphOmapColor)

    // Start fields of StructSphOmapSystemValuesC
    unsigned long long OmapClipDistance0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapClipDistance1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapClipDistance2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapClipDistance3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapClipDistance4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapClipDistance5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapClipDistance6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapClipDistance7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapPointSpriteS : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapPointSpriteT : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapFogCoordinate : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationEvaluationPointU : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapTessellationEvaluationPointV : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapInstanceId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapVertexId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapSystemValuesC
    // NestedStruct(StructSphOmapSystemValuesC)

    // Start fields of StructSphOmapTexture_0
    unsigned long long OmapS_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_0
        // NestedStruct(StructSphOmapTexture_0)

    // Start fields of StructSphOmapTexture_1
    unsigned long long OmapS_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_1
        // NestedStruct(StructSphOmapTexture_1)

    // Start fields of StructSphOmapTexture_2
    unsigned long long OmapS_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_2
        // NestedStruct(StructSphOmapTexture_2)

    // Start fields of StructSphOmapTexture_3
    unsigned long long OmapS_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_3
        // NestedStruct(StructSphOmapTexture_3)

    // Start fields of StructSphOmapTexture_4
    unsigned long long OmapS_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_4
        // NestedStruct(StructSphOmapTexture_4)

    // Start fields of StructSphOmapTexture_5
    unsigned long long OmapS_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_5
        // NestedStruct(StructSphOmapTexture_5)

    // Start fields of StructSphOmapTexture_6
    unsigned long long OmapS_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_6
        // NestedStruct(StructSphOmapTexture_6)

    // Start fields of StructSphOmapTexture_7
    unsigned long long OmapS_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_7
        // NestedStruct(StructSphOmapTexture_7)

    // Start fields of StructSphOmapTexture_8
    unsigned long long OmapS_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_8 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_8
        // NestedStruct(StructSphOmapTexture_8)

    // Start fields of StructSphOmapTexture_9
    unsigned long long OmapS_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapT_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapR_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapQ_9 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapTexture_9
        // NestedStruct(StructSphOmapTexture_9)

    // Start fields of StructSphOmapReserved
    unsigned long long OmapSystemValuesReserved18 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved19 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved20 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved21 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved22 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved23 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved24 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapSystemValuesReserved25 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphOmapReserved
    // NestedStruct(StructSphOmapReserved)
};

struct TextureHeaderStateVersion2Flat {

    // Start fields of TextureHeaderDword0
    unsigned long long componentSizes : 6;     // Enum(ComponentSizes)
    unsigned long long rDataType : 3;     // Enum(NumericalDataType)
    unsigned long long gDataType : 3;     // Enum(NumericalDataType)
    unsigned long long bDataType : 3;     // Enum(NumericalDataType)
    unsigned long long aDataType : 3;     // Enum(NumericalDataType)
    unsigned long long xSource : 3;     // Enum(XYZWSource)
    unsigned long long ySource : 3;     // Enum(XYZWSource)
    unsigned long long zSource : 3;     // Enum(XYZWSource)
    unsigned long long wSource : 3;     // Enum(XYZWSource)
    unsigned long long packComponents : 1;     // Simple
    unsigned long long reserved4 : 1;     // Simple
    // End fields of TextureHeaderDword0
    // NestedStruct(TextureHeaderDword0)

    unsigned long long offsetLower : 32;         // Simple
    // Start fields of TextureHeaderDword2
    unsigned long long offsetUpper : 8;     // Simple
    unsigned long long anisoSpreadMaxLog2LSB : 2;     // Simple
    unsigned long long sRGBColwersion : 1;     // Simple
    unsigned long long anisoSpreadMaxLog2MSB : 1;     // Simple
    unsigned long long lodAnisoQuality2 : 1;     // Simple
    unsigned long long colorKeyOp : 1;     // Simple
    unsigned long long textureType : 4;     // Enum(TextureType)
    unsigned long long memoryLayout : 1;     // Enum(MemoryLayout)
    unsigned long long gobsPerBlockWidth : 3;     // Enum(GobsPerBlockWidth)
    unsigned long long gobsPerBlockHeight : 3;     // Enum(GobsPerBlock)
    unsigned long long gobsPerBlockDepth : 3;     // Enum(GobsPerBlock)
    unsigned long long sectorPromotion : 2;     // Enum(SectorPromotion)
    unsigned long long borderSource : 1;     // Enum(BorderSource)
    unsigned long long normalizedCoords : 1;     // Simple
    // End fields of TextureHeaderDword2
    // NestedStruct(TextureHeaderDword2)

    // Start fields of TextureHeaderDword3
    unsigned long long pitch : 20;     // Simple
    unsigned long long lodAnisoQuality : 1;     // Enum(LodQuality)
    unsigned long long lodIsoQuality : 1;     // Enum(LodQuality)
    unsigned long long anisoCoarseSpreadModifier : 2;     // Enum(AnisoSpreadModifier)
    unsigned long long anisoSpreadScale : 5;     // Simple
    unsigned long long useHeaderOptControl : 1;     // Simple
    unsigned long long anisoClampAtMaxLod : 1;     // Simple
    unsigned long long anisoPow2 : 1;     // Simple
    // End fields of TextureHeaderDword3
    // NestedStruct(TextureHeaderDword3)

    // Start fields of TextureHeaderDword4
    unsigned long long width : 30;     // Simple
    unsigned long long depthTexture : 1;     // Simple
    unsigned long long useTextureHeaderVersion2 : 1;     // Simple
    // End fields of TextureHeaderDword4
    // NestedStruct(TextureHeaderDword4)

    // Start fields of TextureHeaderDword5
    unsigned long long height : 16;     // Simple
    unsigned long long depth : 12;     // Simple
    unsigned long long maxMipLevel : 4;     // Simple
    // End fields of TextureHeaderDword5
    // NestedStruct(TextureHeaderDword5)

    // Start fields of TextureHeaderDword6
    unsigned long long trilinOpt : 5;     // Simple
    unsigned long long mipLodBias : 13;     // Simple
    unsigned long long anisoRoundDown : 1;     // Simple
    unsigned long long anisoBias : 4;     // Simple
    unsigned long long anisoFineSpreadFunc : 2;     // Enum(AnisoSpreadFunc)
    unsigned long long anisoCoarseSpreadFunc : 2;     // Enum(AnisoSpreadFunc)
    unsigned long long maxAnisotropy : 3;     // Enum(MaxAnisotropy)
    unsigned long long anisoFineSpreadModifier : 2;     // Enum(AnisoSpreadModifier)
    // End fields of TextureHeaderDword6
    // NestedStruct(TextureHeaderDword6)

    // Start fields of TextureHeaderDword7_Version2
    unsigned long long resViewMinMipLevel : 4;     // Simple
    unsigned long long resViewMaxMipLevel : 4;     // Simple
    unsigned long long heightMsb : 1;     // Simple
    unsigned long long heightMsbReserved : 3;     // Simple
    unsigned long long multiSampleCount : 4;     // Enum(TYPEDEF_SAMPLE_MODES)
    unsigned long long minLodClamp : 12;     // Simple
    unsigned long long reserved7a : 4;     // Simple
    // End fields of TextureHeaderDword7_Version2
    // NestedStruct(TextureHeaderDword7_Version2)
};

struct StructSphType02Version03Flat {

    // Start fields of StructSphCommonWord0
    unsigned long long SphType : 5;     // Enum(TYPEDEF_SPH_TYPE)
    unsigned long long Version : 5;     // Simple
    unsigned long long ShaderType : 4;     // Enum(TYPEDEF_SHADER_TYPE)
    unsigned long long MrtEnable : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long KillsPixels : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long DoesGlobalStore : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long SassVersion : 4;     // Simple
    unsigned long long ReservedCommonA_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonA_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long DoesLoadOrStore : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long DoesFp64 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long StreamOutMask : 4;     // Simple
    // End fields of StructSphCommonWord0
    // NestedStruct(StructSphCommonWord0)

    // Start fields of StructSphCommonWord1
    unsigned long long ShaderLocalMemoryLowSize : 24;     // Simple
    unsigned long long PerPatchAttributeCount : 8;     // Simple
    // End fields of StructSphCommonWord1
    // NestedStruct(StructSphCommonWord1)

    // Start fields of StructSphCommonWord2
    unsigned long long ShaderLocalMemoryHighSize : 24;     // Simple
    unsigned long long ThreadsPerInputPrimitive : 8;     // Simple
    // End fields of StructSphCommonWord2
    // NestedStruct(StructSphCommonWord2)

    // Start fields of StructSphCommonWord3
    unsigned long long ShaderLocalMemoryCrsSize : 24;     // Simple
    unsigned long long OutputTopology : 4;     // Enum(TYPEDEF_SPH_OUTPUT_TOPOLOGY)
    unsigned long long ReservedCommonB : 4;     // Simple
    // End fields of StructSphCommonWord3
    // NestedStruct(StructSphCommonWord3)

    // Start fields of StructSphCommonWord4
    unsigned long long MaxOutputVertexCount : 12;     // Simple
    unsigned long long StoreReqStart : 8;     // Simple
    unsigned long long ReservedCommonC_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonC_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonC_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ReservedCommonC_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long StoreReqEnd : 8;     // Simple
    // End fields of StructSphCommonWord4
    // NestedStruct(StructSphCommonWord4)

    // Start fields of StructSphImapSystemValuesA
    unsigned long long ImapSystemValuesReserved28 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved29 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved30 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved31 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodLeft : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodRight : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodBottom : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationLodTop : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationInteriorU : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationInteriorV : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved00 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved01 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved02 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved03 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved04 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved05 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved06 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved07 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved08 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved09 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved10 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved11 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved12 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved13 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapSystemValuesA
    // NestedStruct(StructSphImapSystemValuesA)

    // Start fields of StructSphImapSystemValuesB
    unsigned long long ImapPrimitiveId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapRtArrayIndex : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapViewportIndex : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPointSize : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionX : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionY : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionZ : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPositionW : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapSystemValuesB
    // NestedStruct(StructSphImapSystemValuesB)

    // Start fields of StructSphImapPixelVector_0
    unsigned long long ImapX_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_0
        // NestedStruct(StructSphImapPixelVector_0)

    // Start fields of StructSphImapPixelVector_1
    unsigned long long ImapX_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_1
        // NestedStruct(StructSphImapPixelVector_1)

    // Start fields of StructSphImapPixelVector_2
    unsigned long long ImapX_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_2
        // NestedStruct(StructSphImapPixelVector_2)

    // Start fields of StructSphImapPixelVector_3
    unsigned long long ImapX_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_3
        // NestedStruct(StructSphImapPixelVector_3)

    // Start fields of StructSphImapPixelVector_4
    unsigned long long ImapX_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_4
        // NestedStruct(StructSphImapPixelVector_4)

    // Start fields of StructSphImapPixelVector_5
    unsigned long long ImapX_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_5
        // NestedStruct(StructSphImapPixelVector_5)

    // Start fields of StructSphImapPixelVector_6
    unsigned long long ImapX_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_6
        // NestedStruct(StructSphImapPixelVector_6)

    // Start fields of StructSphImapPixelVector_7
    unsigned long long ImapX_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_7
        // NestedStruct(StructSphImapPixelVector_7)

    // Start fields of StructSphImapPixelVector_8
    unsigned long long ImapX_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_8
        // NestedStruct(StructSphImapPixelVector_8)

    // Start fields of StructSphImapPixelVector_9
    unsigned long long ImapX_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_9
        // NestedStruct(StructSphImapPixelVector_9)

    // Start fields of StructSphImapPixelVector_10
    unsigned long long ImapX_10 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_10 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_10 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_10 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_10
        // NestedStruct(StructSphImapPixelVector_10)

    // Start fields of StructSphImapPixelVector_11
    unsigned long long ImapX_11 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_11 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_11 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_11 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_11
        // NestedStruct(StructSphImapPixelVector_11)

    // Start fields of StructSphImapPixelVector_12
    unsigned long long ImapX_12 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_12 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_12 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_12 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_12
        // NestedStruct(StructSphImapPixelVector_12)

    // Start fields of StructSphImapPixelVector_13
    unsigned long long ImapX_13 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_13 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_13 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_13 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_13
        // NestedStruct(StructSphImapPixelVector_13)

    // Start fields of StructSphImapPixelVector_14
    unsigned long long ImapX_14 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_14 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_14 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_14 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_14
        // NestedStruct(StructSphImapPixelVector_14)

    // Start fields of StructSphImapPixelVector_15
    unsigned long long ImapX_15 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_15 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_15 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_15 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_15
        // NestedStruct(StructSphImapPixelVector_15)

    // Start fields of StructSphImapPixelVector_16
    unsigned long long ImapX_16 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_16 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_16 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_16 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_16
        // NestedStruct(StructSphImapPixelVector_16)

    // Start fields of StructSphImapPixelVector_17
    unsigned long long ImapX_17 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_17 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_17 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_17 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_17
        // NestedStruct(StructSphImapPixelVector_17)

    // Start fields of StructSphImapPixelVector_18
    unsigned long long ImapX_18 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_18 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_18 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_18 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_18
        // NestedStruct(StructSphImapPixelVector_18)

    // Start fields of StructSphImapPixelVector_19
    unsigned long long ImapX_19 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_19 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_19 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_19 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_19
        // NestedStruct(StructSphImapPixelVector_19)

    // Start fields of StructSphImapPixelVector_20
    unsigned long long ImapX_20 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_20 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_20 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_20 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_20
        // NestedStruct(StructSphImapPixelVector_20)

    // Start fields of StructSphImapPixelVector_21
    unsigned long long ImapX_21 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_21 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_21 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_21 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_21
        // NestedStruct(StructSphImapPixelVector_21)

    // Start fields of StructSphImapPixelVector_22
    unsigned long long ImapX_22 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_22 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_22 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_22 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_22
        // NestedStruct(StructSphImapPixelVector_22)

    // Start fields of StructSphImapPixelVector_23
    unsigned long long ImapX_23 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_23 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_23 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_23 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_23
        // NestedStruct(StructSphImapPixelVector_23)

    // Start fields of StructSphImapPixelVector_24
    unsigned long long ImapX_24 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_24 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_24 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_24 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_24
        // NestedStruct(StructSphImapPixelVector_24)

    // Start fields of StructSphImapPixelVector_25
    unsigned long long ImapX_25 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_25 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_25 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_25 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_25
        // NestedStruct(StructSphImapPixelVector_25)

    // Start fields of StructSphImapPixelVector_26
    unsigned long long ImapX_26 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_26 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_26 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_26 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_26
        // NestedStruct(StructSphImapPixelVector_26)

    // Start fields of StructSphImapPixelVector_27
    unsigned long long ImapX_27 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_27 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_27 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_27 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_27
        // NestedStruct(StructSphImapPixelVector_27)

    // Start fields of StructSphImapPixelVector_28
    unsigned long long ImapX_28 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_28 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_28 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_28 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_28
        // NestedStruct(StructSphImapPixelVector_28)

    // Start fields of StructSphImapPixelVector_29
    unsigned long long ImapX_29 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_29 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_29 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_29 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_29
        // NestedStruct(StructSphImapPixelVector_29)

    // Start fields of StructSphImapPixelVector_30
    unsigned long long ImapX_30 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_30 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_30 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_30 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_30
        // NestedStruct(StructSphImapPixelVector_30)

    // Start fields of StructSphImapPixelVector_31
    unsigned long long ImapX_31 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapY_31 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapZ_31 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapW_31 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelVector_31
        // NestedStruct(StructSphImapPixelVector_31)

    // Start fields of StructSphPixelImapColor
    unsigned long long ImapColorDiffuseRed : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapColorDiffuseGreen : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapColorDiffuseBlue : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapColorDiffuseAlpha : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapColorSpelwlarRed : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapColorSpelwlarGreen : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapColorSpelwlarBlue : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapColorSpelwlarAlpha : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphPixelImapColor
    // NestedStruct(StructSphPixelImapColor)

    // Start fields of StructSphImapSystemValuesC
    unsigned long long ImapClipDistance0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapClipDistance7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPointSpriteS : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapPointSpriteT : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapFogCoordinate : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapSystemValuesReserved17 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationEvaluationPointU : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapTessellationEvaluationPointV : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapInstanceId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long ImapVertexId : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphImapSystemValuesC
    // NestedStruct(StructSphImapSystemValuesC)

    // Start fields of StructSphImapPixelTexture_0
    unsigned long long ImapS_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_0 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_0
        // NestedStruct(StructSphImapPixelTexture_0)

    // Start fields of StructSphImapPixelTexture_1
    unsigned long long ImapS_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_1 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_1
        // NestedStruct(StructSphImapPixelTexture_1)

    // Start fields of StructSphImapPixelTexture_2
    unsigned long long ImapS_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_2 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_2
        // NestedStruct(StructSphImapPixelTexture_2)

    // Start fields of StructSphImapPixelTexture_3
    unsigned long long ImapS_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_3 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_3
        // NestedStruct(StructSphImapPixelTexture_3)

    // Start fields of StructSphImapPixelTexture_4
    unsigned long long ImapS_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_4 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_4
        // NestedStruct(StructSphImapPixelTexture_4)

    // Start fields of StructSphImapPixelTexture_5
    unsigned long long ImapS_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_5 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_5
        // NestedStruct(StructSphImapPixelTexture_5)

    // Start fields of StructSphImapPixelTexture_6
    unsigned long long ImapS_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_6 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_6
        // NestedStruct(StructSphImapPixelTexture_6)

    // Start fields of StructSphImapPixelTexture_7
    unsigned long long ImapS_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_7 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_7
        // NestedStruct(StructSphImapPixelTexture_7)

    // Start fields of StructSphImapPixelTexture_8
    unsigned long long ImapS_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_8 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_8
        // NestedStruct(StructSphImapPixelTexture_8)

    // Start fields of StructSphImapPixelTexture_9
    unsigned long long ImapS_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapT_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapR_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapQ_9 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphImapPixelTexture_9
        // NestedStruct(StructSphImapPixelTexture_9)

    // Start fields of StructSphPixelImapReserved
    unsigned long long ImapSystemValuesReserved18 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapSystemValuesReserved19 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapSystemValuesReserved20 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapSystemValuesReserved21 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapSystemValuesReserved22 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapSystemValuesReserved23 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapSystemValuesReserved24 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    unsigned long long ImapSystemValuesReserved25 : 2;     // Enum(TYPEDEF_SPH_PIXEL_IMAP)
    // End fields of StructSphPixelImapReserved
    // NestedStruct(StructSphPixelImapReserved)

    // Start fields of StructSphTarget_0
    unsigned long long OmapRed_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_0 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_0
        // NestedStruct(StructSphTarget_0)

    // Start fields of StructSphTarget_1
    unsigned long long OmapRed_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_1 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_1
        // NestedStruct(StructSphTarget_1)

    // Start fields of StructSphTarget_2
    unsigned long long OmapRed_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_2 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_2
        // NestedStruct(StructSphTarget_2)

    // Start fields of StructSphTarget_3
    unsigned long long OmapRed_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_3 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_3
        // NestedStruct(StructSphTarget_3)

    // Start fields of StructSphTarget_4
    unsigned long long OmapRed_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_4 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_4
        // NestedStruct(StructSphTarget_4)

    // Start fields of StructSphTarget_5
    unsigned long long OmapRed_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_5 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_5
        // NestedStruct(StructSphTarget_5)

    // Start fields of StructSphTarget_6
    unsigned long long OmapRed_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_6 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_6
        // NestedStruct(StructSphTarget_6)

    // Start fields of StructSphTarget_7
    unsigned long long OmapRed_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapGreen_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapBlue_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapAlpha_7 : 1;     // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    // End fields of StructSphTarget_7
        // NestedStruct(StructSphTarget_7)

    unsigned long long OmapSampleMask : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long OmapDepth : 1;         // Enum(TYPEDEF_1BIT_FALSE_TRUE)
    unsigned long long Reserved_0 : 1;         // Simple
    unsigned long long Reserved_1 : 1;         // Simple
    unsigned long long Reserved_2 : 1;         // Simple
    unsigned long long Reserved_3 : 1;         // Simple
    unsigned long long Reserved_4 : 1;         // Simple
    unsigned long long Reserved_5 : 1;         // Simple
    unsigned long long Reserved_6 : 1;         // Simple
    unsigned long long Reserved_7 : 1;         // Simple
    unsigned long long Reserved_8 : 1;         // Simple
    unsigned long long Reserved_9 : 1;         // Simple
    unsigned long long Reserved_10 : 1;         // Simple
    unsigned long long Reserved_11 : 1;         // Simple
    unsigned long long Reserved_12 : 1;         // Simple
    unsigned long long Reserved_13 : 1;         // Simple
    unsigned long long Reserved_14 : 1;         // Simple
    unsigned long long Reserved_15 : 1;         // Simple
    unsigned long long Reserved_16 : 1;         // Simple
    unsigned long long Reserved_17 : 1;         // Simple
    unsigned long long Reserved_18 : 1;         // Simple
    unsigned long long Reserved_19 : 1;         // Simple
    unsigned long long Reserved_20 : 1;         // Simple
    unsigned long long Reserved_21 : 1;         // Simple
    unsigned long long Reserved_22 : 1;         // Simple
    unsigned long long Reserved_23 : 1;         // Simple
    unsigned long long Reserved_24 : 1;         // Simple
    unsigned long long Reserved_25 : 1;         // Simple
    unsigned long long Reserved_26 : 1;         // Simple
    unsigned long long Reserved_27 : 1;         // Simple
    unsigned long long Reserved_28 : 1;         // Simple
    unsigned long long Reserved_29 : 1;         // Simple
};

struct TextureSamplerStateFlat {

    // Start fields of TextureSamplerDword0
    unsigned long long addressU : 3;     // Enum(AddressMode)
    unsigned long long addressV : 3;     // Enum(AddressMode)
    unsigned long long addressP : 3;     // Enum(AddressMode)
    unsigned long long depthCompare : 1;     // Simple
    unsigned long long depthCompareFunc : 3;     // Enum(DepthCompareFunc)
    unsigned long long sRGBColwersion : 1;     // Simple
    unsigned long long fontFilterWidth : 3;     // Enum(FontFilterSize)
    unsigned long long fontFilterHeight : 3;     // Enum(FontFilterSize)
    unsigned long long maxAnisotropy : 3;     // Enum(MaxAnisotropy)
    unsigned long long padding_dword0 : 9;     // Simple
    // End fields of TextureSamplerDword0
    // NestedStruct(TextureSamplerDword0)

    // Start fields of TextureSamplerDword1
    unsigned long long magFilter : 3;     // Enum(MagFilter)
    unsigned long long padding_dword1_0 : 1;     // Simple
    unsigned long long minFilter : 2;     // Enum(MinFilter)
    unsigned long long mipFilter : 2;     // Enum(MipFilter)
    unsigned long long reserved : 2;     // Simple
    unsigned long long padding_dword1_1 : 2;     // Simple
    unsigned long long mipLodBias : 13;     // Simple
    unsigned long long padding_dword1_3 : 1;     // Simple
    unsigned long long trilinOpt : 5;     // Simple
    unsigned long long padding_dword1_2 : 1;     // Simple
    // End fields of TextureSamplerDword1
    // NestedStruct(TextureSamplerDword1)

    // Start fields of TextureSamplerDword2
    unsigned long long minLodClamp : 12;     // Simple
    unsigned long long maxLodClamp : 12;     // Simple
    unsigned long long sRGBBorderColorR : 8;     // Simple
    // End fields of TextureSamplerDword2
    // NestedStruct(TextureSamplerDword2)

    // Start fields of TextureSamplerDword3
    unsigned long long reserved12 : 12;     // Simple
    unsigned long long sRGBBorderColorG : 8;     // Simple
    unsigned long long sRGBBorderColorB : 8;     // Simple
    unsigned long long padding_dword3 : 4;     // Simple
    // End fields of TextureSamplerDword3
    // NestedStruct(TextureSamplerDword3)

    unsigned long long borderColorR : 32;         // Simple
    unsigned long long borderColorG : 32;         // Simple
    unsigned long long borderColorB : 32;         // Simple
    unsigned long long borderColorA : 32;         // Simple
};

struct TextureHeaderStateFlat {

    // Start fields of TextureHeaderDword0
    unsigned long long componentSizes : 6;     // Enum(ComponentSizes)
    unsigned long long rDataType : 3;     // Enum(NumericalDataType)
    unsigned long long gDataType : 3;     // Enum(NumericalDataType)
    unsigned long long bDataType : 3;     // Enum(NumericalDataType)
    unsigned long long aDataType : 3;     // Enum(NumericalDataType)
    unsigned long long xSource : 3;     // Enum(XYZWSource)
    unsigned long long ySource : 3;     // Enum(XYZWSource)
    unsigned long long zSource : 3;     // Enum(XYZWSource)
    unsigned long long wSource : 3;     // Enum(XYZWSource)
    unsigned long long packComponents : 1;     // Simple
    unsigned long long reserved4 : 1;     // Simple
    // End fields of TextureHeaderDword0
    // NestedStruct(TextureHeaderDword0)

    unsigned long long offsetLower : 32;         // Simple
    // Start fields of TextureHeaderDword2
    unsigned long long offsetUpper : 8;     // Simple
    unsigned long long anisoSpreadMaxLog2LSB : 2;     // Simple
    unsigned long long sRGBColwersion : 1;     // Simple
    unsigned long long anisoSpreadMaxLog2MSB : 1;     // Simple
    unsigned long long lodAnisoQuality2 : 1;     // Simple
    unsigned long long colorKeyOp : 1;     // Simple
    unsigned long long textureType : 4;     // Enum(TextureType)
    unsigned long long memoryLayout : 1;     // Enum(MemoryLayout)
    unsigned long long gobsPerBlockWidth : 3;     // Enum(GobsPerBlockWidth)
    unsigned long long gobsPerBlockHeight : 3;     // Enum(GobsPerBlock)
    unsigned long long gobsPerBlockDepth : 3;     // Enum(GobsPerBlock)
    unsigned long long sectorPromotion : 2;     // Enum(SectorPromotion)
    unsigned long long borderSource : 1;     // Enum(BorderSource)
    unsigned long long normalizedCoords : 1;     // Simple
    // End fields of TextureHeaderDword2
    // NestedStruct(TextureHeaderDword2)

    // Start fields of TextureHeaderDword3
    unsigned long long pitch : 20;     // Simple
    unsigned long long lodAnisoQuality : 1;     // Enum(LodQuality)
    unsigned long long lodIsoQuality : 1;     // Enum(LodQuality)
    unsigned long long anisoCoarseSpreadModifier : 2;     // Enum(AnisoSpreadModifier)
    unsigned long long anisoSpreadScale : 5;     // Simple
    unsigned long long useHeaderOptControl : 1;     // Simple
    unsigned long long anisoClampAtMaxLod : 1;     // Simple
    unsigned long long anisoPow2 : 1;     // Simple
    // End fields of TextureHeaderDword3
    // NestedStruct(TextureHeaderDword3)

    // Start fields of TextureHeaderDword4
    unsigned long long width : 30;     // Simple
    unsigned long long depthTexture : 1;     // Simple
    unsigned long long useTextureHeaderVersion2 : 1;     // Simple
    // End fields of TextureHeaderDword4
    // NestedStruct(TextureHeaderDword4)

    // Start fields of TextureHeaderDword5
    unsigned long long height : 16;     // Simple
    unsigned long long depth : 12;     // Simple
    unsigned long long maxMipLevel : 4;     // Simple
    // End fields of TextureHeaderDword5
    // NestedStruct(TextureHeaderDword5)

    // Start fields of TextureHeaderDword6
    unsigned long long trilinOpt : 5;     // Simple
    unsigned long long mipLodBias : 13;     // Simple
    unsigned long long anisoRoundDown : 1;     // Simple
    unsigned long long anisoBias : 4;     // Simple
    unsigned long long anisoFineSpreadFunc : 2;     // Enum(AnisoSpreadFunc)
    unsigned long long anisoCoarseSpreadFunc : 2;     // Enum(AnisoSpreadFunc)
    unsigned long long maxAnisotropy : 3;     // Enum(MaxAnisotropy)
    unsigned long long anisoFineSpreadModifier : 2;     // Enum(AnisoSpreadModifier)
    // End fields of TextureHeaderDword6
    // NestedStruct(TextureHeaderDword6)

    unsigned long long colorKeyValue : 32;         // Simple
};

const TYPEDEF_SPH_DESCRIPTOR_SIZE AamSphImapDescriptorSizeSphType01Version02 [] = { (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1,  };
const TYPEDEF_SPH_VTG_ODMAP AamOdmapGeometryVersion02 [] = { (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3,  };
const TYPEDEF_SPH_VTG_IDMAP AamIdmapGeometryVersion02 [] = { (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const TYPEDEF_SPH_VTG_IDMAP AamVtgBmapGeneratiolwtgIdmapVersion01 [] = { (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const TYPEDEF_SPH_DESCRIPTOR_SIZE AamSphOmapDescriptorSizeSphType01Version02 [] = { (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1,  };
const TYPEDEF_SPH_VTG_ODMAP AamOdmapLwllBeforeFetchVersion02 [] = { (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3,  };
const U010 AamSphImapDescriptorLocationSphType01Version02 [] = { (U010) 160, (U010) 161, (U010) 162, (U010) 163, (U010) 164, (U010) 165, (U010) 166, (U010) 167, (U010) 168, (U010) 169, (U010) 170, (U010) 171, (U010) 172, (U010) 173, (U010) 174, (U010) 175, (U010) 176, (U010) 177, (U010) 178, (U010) 179, (U010) 180, (U010) 181, (U010) 182, (U010) 183, (U010) 184, (U010) 185, (U010) 186, (U010) 187, (U010) 188, (U010) 189, (U010) 190, (U010) 191, (U010) 192, (U010) 193, (U010) 194, (U010) 195, (U010) 196, (U010) 197, (U010) 198, (U010) 199, (U010) 200, (U010) 201, (U010) 202, (U010) 203, (U010) 204, (U010) 205, (U010) 206, (U010) 207, (U010) 208, (U010) 209, (U010) 210, (U010) 211, (U010) 212, (U010) 213, (U010) 214, (U010) 215, (U010) 216, (U010) 217, (U010) 218, (U010) 219, (U010) 220, (U010) 221, (U010) 222, (U010) 223, (U010) 224, (U010) 225, (U010) 226, (U010) 227, (U010) 228, (U010) 229, (U010) 230, (U010) 231, (U010) 232, (U010) 233, (U010) 234, (U010) 235, (U010) 236, (U010) 237, (U010) 238, (U010) 239, (U010) 240, (U010) 241, (U010) 242, (U010) 243, (U010) 244, (U010) 245, (U010) 246, (U010) 247, (U010) 248, (U010) 249, (U010) 250, (U010) 251, (U010) 252, (U010) 253, (U010) 254, (U010) 255, (U010) 256, (U010) 257, (U010) 258, (U010) 259, (U010) 260, (U010) 261, (U010) 262, (U010) 263, (U010) 264, (U010) 265, (U010) 266, (U010) 267, (U010) 268, (U010) 269, (U010) 270, (U010) 271, (U010) 272, (U010) 273, (U010) 274, (U010) 275, (U010) 276, (U010) 277, (U010) 278, (U010) 279, (U010) 280, (U010) 281, (U010) 282, (U010) 283, (U010) 284, (U010) 285, (U010) 286, (U010) 287, (U010) 288, (U010) 289, (U010) 290, (U010) 291, (U010) 292, (U010) 293, (U010) 294, (U010) 295, (U010) 296, (U010) 297, (U010) 298, (U010) 299, (U010) 300, (U010) 301, (U010) 302, (U010) 303, (U010) 304, (U010) 305, (U010) 306, (U010) 307, (U010) 308, (U010) 309, (U010) 310, (U010) 311, (U010) 312, (U010) 313, (U010) 314, (U010) 315, (U010) 316, (U010) 317, (U010) 318, (U010) 319, (U010) 320, (U010) 321, (U010) 322, (U010) 323, (U010) 324, (U010) 325, (U010) 326, (U010) 327, (U010) 328, (U010) 329, (U010) 330, (U010) 331, (U010) 332, (U010) 333, (U010) 334, (U010) 335, (U010) 336, (U010) 337, (U010) 338, (U010) 339, (U010) 340, (U010) 341, (U010) 342, (U010) 343, (U010) 344, (U010) 345, (U010) 346, (U010) 347, (U010) 348, (U010) 349, (U010) 350, (U010) 351, (U010) 352, (U010) 353, (U010) 354, (U010) 355, (U010) 356, (U010) 357, (U010) 358, (U010) 359, (U010) 360, (U010) 361, (U010) 362, (U010) 363, (U010) 364, (U010) 365, (U010) 366, (U010) 367, (U010) 368, (U010) 369, (U010) 370, (U010) 371, (U010) 372, (U010) 373, (U010) 374, (U010) 375, (U010) 376, (U010) 377, (U010) 378, (U010) 379, (U010) 380, (U010) 381, (U010) 382, (U010) 383, (U010) 384, (U010) 385, (U010) 386, (U010) 387, (U010) 388, (U010) 389, (U010) 390, (U010) 391, (U010) 392, (U010) 393, (U010) 394, (U010) 395, (U010) 396, (U010) 397, (U010) 398, (U010) 399,  };
const TYPEDEF_SPH_VTG_IDMAP AamIdmapPixelVersion03 [] = { (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const TYPEDEF_SPH_DESCRIPTOR_SIZE AamSphImapDescriptorSizeSphType02Version02 [] = { (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 2,  };
const TYPEDEF_SPH_VTG_IDMAP AamIdmapVertexVersion02 [] = { (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const TYPEDEF_SPH_VTG_IDMAP AamIdmapTessellatiolwersion02 [] = { (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const U003 TiOutputCbStorageMultiplierFracBits [] = { (U003) 5 };
const U003 GsOutputCbStorageMultiplierFracBits [] = { (U003) 5 };
const U010 AamSphOmapDescriptorLocationSphType02Version02 [] = { (U010) 576, (U010) 577, (U010) 578, (U010) 579, (U010) 580, (U010) 581, (U010) 582, (U010) 583, (U010) 584, (U010) 585, (U010) 586, (U010) 587, (U010) 588, (U010) 589, (U010) 590, (U010) 591, (U010) 592, (U010) 593, (U010) 594, (U010) 595, (U010) 596, (U010) 597, (U010) 598, (U010) 599, (U010) 600, (U010) 601, (U010) 602, (U010) 603, (U010) 604, (U010) 605, (U010) 606, (U010) 607, (U010) 608, (U010) 609, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0,  };
const TYPEDEF_SPH_INTERPOLATION_TYPE AamInterpolationTypeMapVersion02 [] = { (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 3, (TYPEDEF_SPH_INTERPOLATION_TYPE) 3, (TYPEDEF_SPH_INTERPOLATION_TYPE) 3, (TYPEDEF_SPH_INTERPOLATION_TYPE) 3, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 3, (TYPEDEF_SPH_INTERPOLATION_TYPE) 3, (TYPEDEF_SPH_INTERPOLATION_TYPE) 2, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 0, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1, (TYPEDEF_SPH_INTERPOLATION_TYPE) 1,  };
const TYPEDEF_SPH_VTG_IDMAP AamIdmapTessellationInitVersion02 [] = { (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const U010 AamSphOmapDescriptorLocationSphType01Version02 [] = { (U010) 400, (U010) 401, (U010) 402, (U010) 403, (U010) 404, (U010) 405, (U010) 406, (U010) 407, (U010) 408, (U010) 409, (U010) 410, (U010) 411, (U010) 412, (U010) 413, (U010) 414, (U010) 415, (U010) 416, (U010) 417, (U010) 418, (U010) 419, (U010) 420, (U010) 421, (U010) 422, (U010) 423, (U010) 424, (U010) 425, (U010) 426, (U010) 427, (U010) 428, (U010) 429, (U010) 430, (U010) 431, (U010) 432, (U010) 433, (U010) 434, (U010) 435, (U010) 436, (U010) 437, (U010) 438, (U010) 439, (U010) 440, (U010) 441, (U010) 442, (U010) 443, (U010) 444, (U010) 445, (U010) 446, (U010) 447, (U010) 448, (U010) 449, (U010) 450, (U010) 451, (U010) 452, (U010) 453, (U010) 454, (U010) 455, (U010) 456, (U010) 457, (U010) 458, (U010) 459, (U010) 460, (U010) 461, (U010) 462, (U010) 463, (U010) 464, (U010) 465, (U010) 466, (U010) 467, (U010) 468, (U010) 469, (U010) 470, (U010) 471, (U010) 472, (U010) 473, (U010) 474, (U010) 475, (U010) 476, (U010) 477, (U010) 478, (U010) 479, (U010) 480, (U010) 481, (U010) 482, (U010) 483, (U010) 484, (U010) 485, (U010) 486, (U010) 487, (U010) 488, (U010) 489, (U010) 490, (U010) 491, (U010) 492, (U010) 493, (U010) 494, (U010) 495, (U010) 496, (U010) 497, (U010) 498, (U010) 499, (U010) 500, (U010) 501, (U010) 502, (U010) 503, (U010) 504, (U010) 505, (U010) 506, (U010) 507, (U010) 508, (U010) 509, (U010) 510, (U010) 511, (U010) 512, (U010) 513, (U010) 514, (U010) 515, (U010) 516, (U010) 517, (U010) 518, (U010) 519, (U010) 520, (U010) 521, (U010) 522, (U010) 523, (U010) 524, (U010) 525, (U010) 526, (U010) 527, (U010) 528, (U010) 529, (U010) 530, (U010) 531, (U010) 532, (U010) 533, (U010) 534, (U010) 535, (U010) 536, (U010) 537, (U010) 538, (U010) 539, (U010) 540, (U010) 541, (U010) 542, (U010) 543, (U010) 544, (U010) 545, (U010) 546, (U010) 547, (U010) 548, (U010) 549, (U010) 550, (U010) 551, (U010) 552, (U010) 553, (U010) 554, (U010) 555, (U010) 556, (U010) 557, (U010) 558, (U010) 559, (U010) 560, (U010) 561, (U010) 562, (U010) 563, (U010) 564, (U010) 565, (U010) 566, (U010) 567, (U010) 568, (U010) 569, (U010) 570, (U010) 571, (U010) 572, (U010) 573, (U010) 574, (U010) 575, (U010) 576, (U010) 577, (U010) 578, (U010) 579, (U010) 580, (U010) 581, (U010) 582, (U010) 583, (U010) 584, (U010) 585, (U010) 586, (U010) 587, (U010) 588, (U010) 589, (U010) 590, (U010) 591, (U010) 592, (U010) 593, (U010) 594, (U010) 595, (U010) 596, (U010) 597, (U010) 598, (U010) 599, (U010) 600, (U010) 601, (U010) 602, (U010) 603, (U010) 604, (U010) 605, (U010) 606, (U010) 607, (U010) 608, (U010) 609, (U010) 610, (U010) 611, (U010) 612, (U010) 613, (U010) 614, (U010) 615, (U010) 616, (U010) 617, (U010) 618, (U010) 619, (U010) 620, (U010) 621, (U010) 622, (U010) 623, (U010) 624, (U010) 625, (U010) 626, (U010) 627, (U010) 628, (U010) 629, (U010) 630, (U010) 631, (U010) 632, (U010) 633, (U010) 634, (U010) 635, (U010) 636, (U010) 637, (U010) 638, (U010) 639,  };
const TYPEDEF_SPH_VTG_ODMAP AamOdmapTessellationInitVersion02 [] = { (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3,  };
const TYPEDEF_SPH_DESCRIPTOR_SIZE AamSphOmapDescriptorSizeSphType02Version02 [] = { (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 1, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0, (TYPEDEF_SPH_DESCRIPTOR_SIZE) 0,  };
const TYPEDEF_SPH_VTG_ODMAP AamOdmapTessellatiolwersion02 [] = { (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3,  };
const U010 AamSphImapDescriptorLocationSphType02Version02 [] = { (U010) 160, (U010) 161, (U010) 162, (U010) 163, (U010) 164, (U010) 165, (U010) 166, (U010) 167, (U010) 168, (U010) 169, (U010) 170, (U010) 171, (U010) 172, (U010) 173, (U010) 174, (U010) 175, (U010) 176, (U010) 177, (U010) 178, (U010) 179, (U010) 180, (U010) 181, (U010) 182, (U010) 183, (U010) 184, (U010) 185, (U010) 186, (U010) 187, (U010) 188, (U010) 189, (U010) 190, (U010) 191, (U010) 192, (U010) 194, (U010) 196, (U010) 198, (U010) 200, (U010) 202, (U010) 204, (U010) 206, (U010) 208, (U010) 210, (U010) 212, (U010) 214, (U010) 216, (U010) 218, (U010) 220, (U010) 222, (U010) 224, (U010) 226, (U010) 228, (U010) 230, (U010) 232, (U010) 234, (U010) 236, (U010) 238, (U010) 240, (U010) 242, (U010) 244, (U010) 246, (U010) 248, (U010) 250, (U010) 252, (U010) 254, (U010) 256, (U010) 258, (U010) 260, (U010) 262, (U010) 264, (U010) 266, (U010) 268, (U010) 270, (U010) 272, (U010) 274, (U010) 276, (U010) 278, (U010) 280, (U010) 282, (U010) 284, (U010) 286, (U010) 288, (U010) 290, (U010) 292, (U010) 294, (U010) 296, (U010) 298, (U010) 300, (U010) 302, (U010) 304, (U010) 306, (U010) 308, (U010) 310, (U010) 312, (U010) 314, (U010) 316, (U010) 318, (U010) 320, (U010) 322, (U010) 324, (U010) 326, (U010) 328, (U010) 330, (U010) 332, (U010) 334, (U010) 336, (U010) 338, (U010) 340, (U010) 342, (U010) 344, (U010) 346, (U010) 348, (U010) 350, (U010) 352, (U010) 354, (U010) 356, (U010) 358, (U010) 360, (U010) 362, (U010) 364, (U010) 366, (U010) 368, (U010) 370, (U010) 372, (U010) 374, (U010) 376, (U010) 378, (U010) 380, (U010) 382, (U010) 384, (U010) 386, (U010) 388, (U010) 390, (U010) 392, (U010) 394, (U010) 396, (U010) 398, (U010) 400, (U010) 402, (U010) 404, (U010) 406, (U010) 408, (U010) 410, (U010) 412, (U010) 414, (U010) 416, (U010) 418, (U010) 420, (U010) 422, (U010) 424, (U010) 426, (U010) 428, (U010) 430, (U010) 432, (U010) 434, (U010) 436, (U010) 438, (U010) 440, (U010) 442, (U010) 444, (U010) 446, (U010) 448, (U010) 450, (U010) 452, (U010) 454, (U010) 456, (U010) 458, (U010) 460, (U010) 462, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 0, (U010) 464, (U010) 465, (U010) 466, (U010) 467, (U010) 468, (U010) 469, (U010) 470, (U010) 471, (U010) 472, (U010) 473, (U010) 474, (U010) 475, (U010) 476, (U010) 477, (U010) 478, (U010) 479, (U010) 480, (U010) 482, (U010) 484, (U010) 486, (U010) 488, (U010) 490, (U010) 492, (U010) 494, (U010) 496, (U010) 498, (U010) 500, (U010) 502, (U010) 504, (U010) 506, (U010) 508, (U010) 510, (U010) 512, (U010) 514, (U010) 516, (U010) 518, (U010) 520, (U010) 522, (U010) 524, (U010) 526, (U010) 528, (U010) 530, (U010) 532, (U010) 534, (U010) 536, (U010) 538, (U010) 540, (U010) 542, (U010) 544, (U010) 546, (U010) 548, (U010) 550, (U010) 552, (U010) 554, (U010) 556, (U010) 558, (U010) 560, (U010) 562, (U010) 564, (U010) 566, (U010) 568, (U010) 570, (U010) 572, (U010) 574,  };
const TYPEDEF_SPH_DEFAULT_TYPE AamDefaultTypeMapVersion02 [] = { (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 2, (TYPEDEF_SPH_DEFAULT_TYPE) 2, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 1, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 10, (TYPEDEF_SPH_DEFAULT_TYPE) 8, (TYPEDEF_SPH_DEFAULT_TYPE) 8, (TYPEDEF_SPH_DEFAULT_TYPE) 8, (TYPEDEF_SPH_DEFAULT_TYPE) 1, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 9, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 1, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 1, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 2, (TYPEDEF_SPH_DEFAULT_TYPE) 2, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 11, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0, (TYPEDEF_SPH_DEFAULT_TYPE) 0,  };
const TYPEDEF_SPH_VTG_IDMAP AamIdmapVscVersion02 [] = { (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const U001 AamVtgBmapGeneratiolwtgOmapVersion01 [] = { (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1,  };
const TYPEDEF_SPH_VTG_IDMAP AamIdmapLwllBeforeFetchVersion02 [] = { (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 0, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 1, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2, (TYPEDEF_SPH_VTG_IDMAP) 2,  };
const U001 AamVtgBmapGeneratiolwtgImapVersion01 [] = { (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 1,  };
const TYPEDEF_SPH_VTG_ODMAP AamVtgBmapGeneratiolwtgOdmapVersion01 [] = { (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 1, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3,  };
const TYPEDEF_SPH_VTG_ODMAP AamOdmapVertexVersion02 [] = { (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 2, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 0, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3, (TYPEDEF_SPH_VTG_ODMAP) 3,  };
const U001 AamVtgBmapGeneratiolwtgBmapVersion01 [] = { (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 0, (U001) 1, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 0, (U001) 0, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 1, (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 0, (U001) 1, (U001) 0, (U001) 0,  };

}// namespace

#endif
